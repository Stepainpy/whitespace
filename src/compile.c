#include "constants.h"
#include "parse.h"

#include <string.h>

#define WSM_THROW(err_code) \
    do { ec = err_code; goto error; } while (0)

ws_error_t ws_compile(
    ws_code_t** cptr, void* src, ws_rdfn_t rdr,
    ws_alloc_t alloc, void* udata
) {
    wsi_read_buffer_t rdbuf[1] = {0};
    wsi_instrs_t arr[1] = {0};
    wsl_list_t   lst[1] = {0};

    ws_code_t* code;
    ws_error_t ec;
    int loop_exit = 0;
    size_t i;

    if (!cptr || !rdr || !alloc) return WSE_INVAL_ARG;

    *cptr = NULL; code = alloc(NULL, sizeof *code, udata);
    if (!code) return WSE_NO_MEMORY;

    memset(code, 0, sizeof *code);
    code->alloc = arr->fn = lst->fn = alloc;
    code->udata = arr->ud = lst->ud = udata;
    rdbuf->fn = rdr;
    rdbuf->ud = src;

    WSM_ARRAY_INIT(arr, instrs, WSC_INIT_INSTR_CAP, WSM_THROW(WSE_NO_MEMORY));
    WSM_ARRAY_INIT(lst, labels, WSC_INIT_LABEL_CAP, WSM_THROW(WSE_NO_MEMORY));

    while (!loop_exit)
        if ((ec = wsi_parse_instr(rdbuf, &loop_exit, arr, lst))) goto error;

    for (i = 0; i < arr->count; i++) {
        wsi_instr_t instr = arr->instrs[i];

        if (instr < WSI_CALL || WSI_IFNG < instr) {
            if (instr == WSI_PUSH || instr == WSI_COPY || instr == WSI_SLIDE)
                i += sizeof(ws_int_t);
            if (instr == WSI_MARK)
                i += sizeof(wsl_index_t);
        } else {
            wsl_index_t index;
            memcpy(&index, arr->instrs + i + 1, sizeof index);
            if (index >= lst->count) WSM_THROW(WSE_UNKNOWN_LABEL);
            if (lst->labels[index].place == WSL_INVAL_PLACE)
                WSM_THROW(WSE_NODEF_LABEL);
            i += sizeof(wsl_index_t);
        }
    }

    WSM_ARRAY_SHRINK(arr, instrs, WSM_THROW(WSE_NO_SHRINK));
    WSM_ARRAY_SHRINK(lst, labels, WSM_THROW(WSE_NO_SHRINK));

    code->instrs = arr->instrs; code->icnt = arr->count;
    code->labels = lst->labels; code->lcnt = lst->count;

    *cptr = code;
    return WSE_OK;
error:
    lst->fn(lst->labels, 0, lst->ud);
    arr->fn(arr->instrs, 0, arr->ud);
    alloc(code, 0, udata);
    return ec;
}