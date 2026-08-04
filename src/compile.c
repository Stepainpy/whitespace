#include "defines.h"

#include <string.h>

#define WSC_READ_BUFFER_SIZE 256

typedef enum {
    WSA_EOF = 0,
    WSA_SPACE,
    WSA_TAB,
    WSA_LF
} wsa_char_t;

typedef struct {
    char window[WSC_READ_BUFFER_SIZE];
    size_t index, count;
    ws_read_t func; void* ud;
} wsi_read_buffer_t;

static wsi_rb_get(wsi_read_buffer_t* rdbuf) {
retry:
    if (rdbuf->index < rdbuf->count) {
        switch (rdbuf->window[rdbuf->index++]) {
            case WSC_S_CHAR: return WSA_SPACE;
            case WSC_T_CHAR: return WSA_TAB;
            case WSC_L_CHAR: return WSA_LF;
            default: goto retry;
        }
    } else {
        rdbuf->index = 0;
        rdbuf->count = rdbuf->func(
            rdbuf->window, 1, WSC_READ_BUFFER_SIZE, rdbuf->ud);
        if (rdbuf->count) goto retry;
        return WSA_EOF;
    }
}

#define WSM_THROW(err_code) \
    do { rc = err_code; goto error; } while (0)

ws_error_t ws_compile(
    ws_state_t** sptr,
    void* src, ws_read_t rdr,
    ws_alloc_t alloc, void* udata
) {
    wsi_read_buffer_t rb[1] = {0};
    ws_state_t* state;
    ws_error_t ec;

    if (!sptr || !rdr || !alloc) return WSE_INVAL_ARG;
    *sptr = NULL;
    rb->func = rdr;
    rb->ud   = src;

    state = alloc(NULL, sizeof *state, udata);
    if (!state) return WSE_NO_MEMORY;
    memset(state, 0, sizeof *state);
    state->alloc = alloc;
    state->udata = udata;

    while (1) {
        switch (wsi_rb_get(rb)) {
            case WSA_SPACE:
                /* Stack manipulation */
                break;

            case WSA_TAB:
                switch (wsi_rb_get(rb)) {
                    case WSA_SPACE:
                        /* Arithmetic */
                        break;

                    case WSA_TAB:
                        /* Heap access */
                        break;

                    case WSA_LF:
                        /* I/O */
                        break;

                    case WSA_EOF:
                        WSM_THROW(WSE_INVAL_INSTR);
                } break;

            case WSA_LF:
                /* Flow control */
                break;

            case WSA_EOF:
                goto loop_exit;
        }
    }
loop_exit:

error:
    alloc(state, 0, udata);
    return ec;
}