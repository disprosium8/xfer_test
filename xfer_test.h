#ifndef XFER_TEST_H
#define XFER_TEST_H

struct message {
  enum {
    MSG_READY,
    MSG_DONE,
    MSG_STOP,
    MSG_ACK
  } type;
  uint64_t size;
  uint64_t buffer_id;
#ifdef HAVE_RDMA
  struct ibv_mr mr;
#endif
};

#endif
