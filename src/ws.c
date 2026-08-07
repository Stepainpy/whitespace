#include <whitespace/whitespace.h>

#include <stdio.h>
#include <stdlib.h>

void* ws_alloc(void* ptr, size_t size, void* ud) {
    if (size) return realloc(ptr, size);
    free(ptr); (void)ud; return NULL;
}

int main(int argc, char* argv[]) {
    ws_error_t ec = WSE_NOT_IMPL;
    ws_state_t* state = NULL;
    FILE* source = NULL;

    /* Skip program name */
    --argc, ++argv;

    if (argc != 1) goto cleanup;
    source = fopen(*argv, "r");
    if (!source) goto cleanup;

    ec = ws_compile(&state,
        source, (ws_read_t)fread,
        ws_alloc, NULL);
    if (ec) goto cleanup;

    puts("before disasm");
    ec = ws_disasm(state,
        stdout, (ws_write_t)fwrite);
    if (ec) goto cleanup;
    puts("after disasm");

cleanup:
    if (state) ws_destroy(state);
    if (source) fclose(source);
    return ec;
}