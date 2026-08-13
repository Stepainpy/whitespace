#include "defines.h"

void ws_destroy(ws_code_t* c) {
    if (!c) return;
    c->alloc(c->instrs, 0, c->udata);
    c->alloc(c->labels, 0, c->udata);
    c->alloc(c        , 0, c->udata);
}