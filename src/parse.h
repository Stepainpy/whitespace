#ifndef WS_PARSER_H
#define WS_PARSER_H

#include <whitespace/whitespace.h>
#include "code.h"
#include "array.h"
#include "label.h"

#define WSC_READ_BUFFER_SIZE 256

typedef WSM_ARRAY_STRUCT(wsi_instr_t, instrs) wsi_instrs_t;

typedef struct {
    char window[WSC_READ_BUFFER_SIZE];
    size_t index, count;
    ws_rdfn_t fn; void* ud;
} wsi_read_buffer_t;

ws_error_t wsi_parse_instr(
    wsi_read_buffer_t* rdbuf, int* loop_exit,
    wsi_instrs_t* array, wsl_list_t* list);

#endif /* WS_PARSER_H */