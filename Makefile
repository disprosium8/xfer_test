CC=gcc
#DEBUG = -g -O0
CFLAGS += $(XSP_FLAGS) -D_GNU_SOURCE $(DEBUG)
BASIC_FILES = get_clock.c slabs_buffer.c
BASIC_HEADERS = get_clock.h

LOADLIBS += -lrt -lm -lpthread
LDFLAGS +=
PROGS=xfer_test rdma_bw ib_bench forwarder

all: xfer_test

rdma: $(PROGS)
rdma: BASIC_FILES += xfer_rdma.c
rdma: LOADLIBS += -libverbs -lrdmacm
rdma: CFLAGS += -DHAVE_RDMA

nordma: xfer_test

${PROGS}: %: %.c ${BASIC_FILES} ${BASIC_HEADERS}
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $< ${BASIC_FILES} $(LOADLIBS) $(LDLIBS) -o $@

clean:
	$(foreach fname,${PROGS}, rm -f ${fname})
.DELETE_ON_ERROR:
.PHONY: all clean
