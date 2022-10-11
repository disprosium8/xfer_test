#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <unistd.h>
#include <malloc.h>
#include <sys/time.h>

#include "slabs_buffer.h"

#define READ_BUF_WAIT_TIME 30

int __psd_slabs_buf_index(psdSLAB *slab, int side) {
  switch (side) {
  case PSB_READ:
    return slab->r_index;
    break;
  case PSB_WRITE:
    return slab->w_index;
    break;
  case PSB_CURR:
    return slab->s_index;
    break;
  default:
    break;
  }
  return -1;
}

psdSLAB *psd_slabs_buf_create(size_t size, int partitions,
                              int read_zero) {
  psdSLAB *buf;
  int page_size;
  int i;

  page_size = sysconf(_SC_PAGESIZE);

  if (partitions <= 0)
    partitions = 1;

  buf = malloc(sizeof(psdSLAB));
  if (!buf)
    goto error_exit;

  memset(buf, 0, sizeof(psdSLAB));

  buf->p_size = floor(size / partitions);
  buf->size = buf->p_size * partitions;
  buf->p_count = partitions - 1;
  buf->total_count_bytes = 0;
  if (read_zero)
    buf->r_index = 0;
  else
    buf->r_index = buf->p_count;
  buf->s_index = 0;
  buf->w_index = 0;

  if (buf->p_size < page_size) {
    goto buf_exit;
  }

  buf->entries = (bufEntry**)malloc(partitions * sizeof(bufEntry*));
  if (!buf->entries)
    goto buf_exit;

  for (i=0; i <= buf->p_count; i++) {
    bufEntry *entry = malloc(sizeof(bufEntry));
    entry->base = NULL;

    //entry->base = malloc(buf->p_size * sizeof(char));
    entry->base = memalign(page_size, buf->p_size * sizeof(char));
    if (!entry->base) {
      printf("could not allocate aligned memory\n");
      goto entry_exit;
    }

    entry->size = buf->p_size;
    entry->ptr = entry->base;
    entry->empty = TRUE;
    entry->priv = NULL;
    entry->write_amount = 0;
    entry->read_amount = 0;
    entry->status = PSB_NO_ERR;

    buf->entries[i] = entry;
  }

  if (pthread_mutex_init(&(buf->buf_lock), NULL) < 0)
    goto entry_exit;

  if (pthread_cond_init(&(buf->read_cond), NULL) < 0)
    goto entry_exit;

  if (pthread_cond_init(&(buf->write_cond), NULL) < 0)
    goto entry_exit;


  return buf;

entry_exit:
  for (i=0; i<= buf->p_count; i++) {
    if (buf->entries[i]) {
      free(buf->entries[i]->base);
      free(buf->entries[i]);
    }
    else
      break;
  }

buf_exit:
  free(buf);
error_exit:
  return NULL;
}

void psd_slabs_buf_free(psdSLAB *slab) {
  int i;

  if (slab) {
    for (i=0; i<=slab->p_count; i++) {
      free(slab->entries[i]->base);
      free(slab->entries[i]);
    }
  }
}

void psd_slabs_buf_reset(psdSLAB *slab) {

}

void psd_slabs_buf_wait_curr(psdSLAB *slab, int side) {
  pthread_mutex_lock(&(slab->buf_lock));
  {
    if (side == PSB_READ) {
      if (slab->entries[slab->s_index]->status != PSB_SEND_READY) {
        pthread_cond_wait(&(slab->write_cond), &(slab->buf_lock));
      }
    }
    else if (side == PSB_WRITE) {
      if (slab->entries[slab->s_index]->status != PSB_RECV_READY) {
        pthread_cond_wait(&(slab->read_cond), &(slab->buf_lock));
      }
    }
  }
  pthread_mutex_unlock(&(slab->buf_lock));
}

void psd_slabs_buf_set_read_index(psdSLAB *slab, int ind) {
  slab->r_index = ind;
}

void psd_slabs_buf_set_write_index(psdSLAB *slab, int ind) {
  slab->w_index = ind;
}

void psd_slabs_buf_set_status(psdSLAB *slab, int status) {
  slab->status |= status;
}

void psd_slabs_buf_unset_pstatus(psdSLAB *slab, int status, int side) {
  int ind = __psd_slabs_buf_index(slab, side);
  slab->entries[ind]->status ^= status;
}

void psd_slabs_buf_set_pstatus(psdSLAB *slab, int status, int side) {
  int ind = __psd_slabs_buf_index(slab, side);
  slab->entries[ind]->status |= status;
}

int psd_slabs_buf_get_status(psdSLAB *slab) {
  return slab->status;
}

int psd_slabs_buf_get_pstatus(psdSLAB *slab, int side) {
  int ind = __psd_slabs_buf_index(slab, side);
  return slab->entries[ind]->status;
}

uint64_t psd_slabs_buf_get_size(psdSLAB *slab) {
  return slab->size;
}

uint64_t psd_slabs_buf_get_psize(psdSLAB *slab) {
  return slab->p_size;
}

uint64_t psd_slabs_buf_get_pcount(psdSLAB *slab) {
  return slab->p_count + 1;
}

void psd_slabs_buf_set_priv_data_ind(psdSLAB *slab, void *data, int ind) {
  slab->entries[ind]->priv = data;
}

void *psd_slabs_buf_get_priv_data_ind(psdSLAB *slab, int ind) {
  return slab->entries[ind]->priv;
}

void *psd_slabs_buf_get_priv_data(psdSLAB *slab, int side) {
  int ind = __psd_slabs_buf_index(slab, side);
  return slab->entries[ind]->priv;
}

void psd_slabs_buf_read_swap(psdSLAB *slab, int total) {
  pthread_mutex_lock(&(slab->buf_lock));
  {
    //get the entry ready for writing
    slab->entries[slab->r_index]->empty = TRUE;
    slab->entries[slab->r_index]->ptr = slab->entries[slab->r_index]->base;
    slab->entries[slab->r_index]->write_amount = 0;
    slab->entries[slab->r_index]->status = PSB_RECV_READY;

    // signal that we finished reading from this buf entry
    pthread_cond_signal(&(slab->read_cond));

    // now get the next entry to read
    slab->r_index++;
    if (slab->r_index > slab->p_count)
      slab->r_index = 0;

    // wait if the next entry has no data
    if (slab->entries[slab->r_index]->empty == TRUE) {
      int rc;
      struct timeval tp;
      struct timespec read_wait_time;

      do {
        gettimeofday(&tp, NULL);
        read_wait_time.tv_sec = tp.tv_sec;
        read_wait_time.tv_nsec = tp.tv_usec * 1000;
        read_wait_time.tv_sec += READ_BUF_WAIT_TIME;

        rc = pthread_cond_timedwait(&(slab->write_cond), &(slab->buf_lock), &read_wait_time);
      }
      while ((rc == ETIMEDOUT) && (slab->entries[slab->r_index]->write_amount == 0));
    }
  }
  pthread_mutex_unlock(&(slab->buf_lock));
}

void psd_slabs_buf_curr_swap(psdSLAB *slab) {
  pthread_mutex_lock(&(slab->buf_lock));
  {
    slab->entries[slab->s_index]->status = PSB_NO_ERR;
    slab->s_index++;
    if (slab->s_index > slab->p_count)
      slab->s_index = 0;
  }
  pthread_mutex_unlock(&(slab->buf_lock));
}

void psd_slabs_buf_write_swap(psdSLAB *slab, int total) {
  pthread_mutex_lock(&(slab->buf_lock));
  {
    //get the entry ready for reading
    slab->entries[slab->w_index]->empty = FALSE;
    slab->entries[slab->w_index]->ptr = slab->entries[slab->w_index]->base;
    slab->entries[slab->w_index]->read_amount = 0;
    slab->entries[slab->w_index]->status = PSB_SEND_READY;

    // signal that we finished writing this buf entry
    pthread_cond_signal(&(slab->write_cond));

    // now get the next entry to write
    slab->w_index++;
    if (slab->w_index > slab->p_count)
      slab->w_index = 0;

    // wait if the next buf is not ready
    if (slab->entries[slab->w_index]->empty == FALSE) {
      pthread_cond_wait(&(slab->read_cond), &(slab->buf_lock));
      // reset status
      slab->entries[slab->w_index]->status = PSB_NO_ERR;
    }
  }
  pthread_mutex_unlock(&(slab->buf_lock));
}

void *psd_slabs_buf_addr(psdSLAB *slab, int side) {
  int ind = __psd_slabs_buf_index(slab, side);
  return slab->entries[ind]->ptr;
}

void *psd_slabs_buf_addr_ind(psdSLAB *slab, int ind) {
  return slab->entries[ind]->ptr;
}

void psd_slabs_buf_advance_curr(psdSLAB *slab, uint64_t bytes, int side) {
  slab->entries[slab->s_index]->ptr = (char*)(slab->entries[slab->s_index]->ptr) + bytes;

  if (side == PSB_WRITE)
    slab->entries[slab->s_index]->write_amount += bytes;
  else if (side == PSB_READ)
    slab->entries[slab->s_index]->read_amount += bytes;
}

void psd_slabs_buf_advance(psdSLAB *slab, uint64_t bytes, int side) {
  int ind = __psd_slabs_buf_index(slab, side);
  slab->entries[ind]->ptr = (char*)(slab->entries[ind]->ptr) + bytes;

  if (side == PSB_WRITE)
    slab->entries[ind]->write_amount += bytes;
  else if (side == PSB_READ)
    slab->entries[ind]->read_amount += bytes;
}

uint64_t psd_slabs_buf_count_bytes(psdSLAB *slab, int side) {
  int ind = __psd_slabs_buf_index(slab, side);

  if (side == PSB_WRITE)
    return slab->entries[ind]->write_amount;
  else if ((side == PSB_READ) || (side == PSB_CURR))
    return slab->entries[ind]->read_amount;
}

uint64_t psd_slabs_buf_count_bytes_free(psdSLAB *slab, int side) {
  int ind = __psd_slabs_buf_index(slab, side);

  if (side == PSB_WRITE)
    return slab->entries[ind]->size - slab->entries[ind]->write_amount;
  else if ((side == PSB_READ) || (side == PSB_CURR)) {
    return slab->entries[ind]->write_amount - slab->entries[ind]->read_amount;
  }

  return 0;
}
