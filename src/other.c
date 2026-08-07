#include "defines.h"

void ws_destroy(ws_state_t* s) {
    if (!s) return;
    s->alloc(s->instrs, 0, s->udata);
    s->alloc(s        , 0, s->udata);
}