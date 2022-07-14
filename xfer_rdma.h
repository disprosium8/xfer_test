#ifndef RDMA_LIB_H
#define RDMA_LIB_H

#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>
#include "xfer_test.h"

struct xfer_context {
  struct ibv_context *context;
  struct ibv_pd      *pd;
  struct ibv_mr      *recv_mr;
  struct ibv_mr      *send_mr;
  struct message     *recv_msg;
  struct message     *send_msg;
  struct ibv_cq      *cq;
  struct ibv_qp      *qp;
  struct rdma_cm_id  *cm_id;
  struct ibv_comp_channel *ch;
  int                 tx_depth;
  struct ibv_sge      list;
  struct ibv_send_wr  wr;
};

// might need this again in the future
// for non CMA
struct xfer_dest {
  int lid;
  int qpn;
  int psn;
  unsigned rkey;
  unsigned long long vaddr;
  unsigned size;
};

struct xfer_data {
  int                             port;
  int                             ib_port;
  int                             tx_depth;
  int                             use_cma;
  int                             sockfd;
  char                            *servername;
  struct ibv_device               *ib_dev;
  struct rdma_event_channel       *cm_channel;
  struct rdma_cm_id               *cm_id;
  void                            *local_priv;
  void                            *remote_priv;
  uint64_t                        local_priv_size;
  uint64_t                        remote_priv_size;
};

typedef struct xfer_rdma_buf_handle_t {
  int                             opcode;
  int                             got_done;
  void                            *buf;
  uint64_t                        local_size;
  uint64_t                        remote_size;
  uint64_t                        id;
  struct ibv_mr                   *local_mr;
  struct ibv_mr                   *remote_mr;
  struct xfer_context             *ctx;
} XFER_RDMA_buf_handle;

typedef struct xfer_rdma_poll_info_t {
  int                            opcode;
  int                            status;
  uint64_t                       id;
} XFER_RDMA_poll_info;


int xfer_rdma_init(struct xfer_data *data);
int xfer_rdma_finalize(struct xfer_data *data);
struct xfer_context *xfer_rdma_init_ctx(void *, struct xfer_data *);
void xfer_rdma_destroy_ctx(struct xfer_context *ctx);
struct xfer_rdma_buf_handle_t *xfer_rdma_alloc_handle();

int xfer_rdma_post_os_put(struct xfer_rdma_buf_handle_t **handles, int hcount);
int xfer_rdma_post_os_get(struct xfer_rdma_buf_handle_t **handles, int hcount);
int xfer_rdma_wait_os(struct xfer_rdma_buf_handle_t *handle);
int xfer_rdma_wait_os_event(struct xfer_context *ctx, struct xfer_rdma_poll_info_t *info);

int xfer_rdma_wait_buffer(struct xfer_rdma_buf_handle_t *handle);
int xfer_rdma_post_buffer(struct xfer_rdma_buf_handle_t *handle);

int xfer_rdma_send_stop(struct xfer_rdma_buf_handle_t *handle);
int xfer_rdma_send_done(struct xfer_rdma_buf_handle_t *handle);
int xfer_rdma_wait_done(struct xfer_rdma_buf_handle_t *handle);

struct xfer_context *xfer_rdma_client_connect(struct xfer_data *data);
struct xfer_context *xfer_rdma_server_connect(struct xfer_data *data);

int xfer_rdma_register_buffer(struct xfer_context *ctx, struct xfer_rdma_buf_handle_t *handle);
int xfer_rdma_unregister_buffer(struct xfer_rdma_buf_handle_t *handle);

#endif
