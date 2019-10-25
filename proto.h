#ifndef PROTO_H
#define PROTO_H

#include "proto_stub.h"

struct member {
    int a;
    int b;
    int c;
};

int module_init();

int module_set(int handler, struct member *param IO_WO, int *flag);

int module_get(int handler, struct member *param IO_RO, int *flag IO_RO);

int module_update(int handler, struct member *param IO_RW, int flag);

int module_close(int handler);

#endif /* PROTO_H */