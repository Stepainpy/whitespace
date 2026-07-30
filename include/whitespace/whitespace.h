#ifndef WHITESPACE_H
#define WHITESPACE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ws_state_t ws_state_t;

typedef void* (ws_alloc_t)(void* ptr, size_t size, void* userdata);
typedef size_t (ws_read_t)(void* dst, size_t size, size_t count, void* userdata);
typedef size_t (ws_write_t)(const void* src, size_t size, size_t count, void* userdata);

typedef enum {
    WSE_OK = 0,
    WSE_INVAL_ARG,
} ws_error_t;

const char* ws_strerror(ws_error_t error);

ws_error_t ws_compile(ws_state_t** state,
    void* source, ws_read_t reader,
    ws_alloc_t allocator, void* alloc_ud);

ws_error_t ws_execute(ws_state_t* state,
    void*  input, ws_read_t  reader,
    void* output, ws_write_t writer);

#ifdef __cplusplus
}
#endif

#endif /* WHITESPACE_H */