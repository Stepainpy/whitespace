#ifndef WHITESPACE_H
#define WHITESPACE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Types */

typedef struct ws_code_t ws_code_t;

/* Callback signatures */

typedef void* (*ws_alloc_t)(void* ptr, size_t size, void* userdata);
typedef size_t (*ws_rdfn_t)(      void* dst, size_t size, size_t count, void* userdata);
typedef size_t (*ws_wrfn_t)(const void* src, size_t size, size_t count, void* userdata);

/* Error handling */

typedef enum {
    WSE_OK = 0,

    WSE_INVAL_ARG,
    WSE_NO_MEMORY,
    WSE_NO_SHRINK,
    WSE_INVAL_INSTR,
    WSE_INCOMPL_INSTR,

    WSE_INVAL_SIGN,
    WSE_INT_OVERFLOW,
    WSE_INCOMPL_INT,

    WSE_INVAL_LABEL,
    WSE_TOO_LONG_LABEL,
    WSE_INCOMPL_LABEL,
    WSE_REDEF_LABEL,
    WSE_NODEF_LABEL,
    WSE_UNKNOWN_LABEL,

    WSE_FAIL_WRITE,

    WSE_NOT_IMPL
} ws_error_t;

const char* ws_strerror(ws_error_t error);

/* Compilation and interpretation */

ws_error_t ws_compile(ws_code_t** code,
    void* source, ws_rdfn_t reader,
    ws_alloc_t allocator, void* alloc_ud);

ws_error_t ws_execute(ws_code_t* code,
    void*  input, ws_rdfn_t reader,
    void* output, ws_wrfn_t writer);

/* Other functions */

ws_error_t ws_disasm(ws_code_t* code,
    void* output, ws_wrfn_t writer);

void ws_destroy(ws_code_t* code);

#ifdef __cplusplus
}
#endif

#endif /* WHITESPACE_H */