#ifndef PROTO_H
#define PROTO_H

#include "proto_stub.h"

struct member {
    int a;
    int b;
    int c;
};

int test_rpc_init();

int test_rpc_set(int handler, struct member *param IO_WO);

int test_rpc_get(int handler, struct member *param IO_RO);

int test_rpc_update(int handler, struct member *param IO_RW);

int test_rpc_read(int handler, char *data IO_RO, int len);

int test_rpc_write(int handler, char *data IO_WO, int len);

int test_rpc_close(int handler);

#endif /* PROTO_H */