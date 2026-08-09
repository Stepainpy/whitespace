#include <whitespace/whitespace.h>

#include <stdio.h>
#include <stdlib.h>

void* ws_alloc(void* ptr, size_t size, void* ud) {
    if (size) return realloc(ptr, size);
    free(ptr); (void)ud; return NULL;
}

int main(int argc, char* argv[]) {
    ws_error_t ec = WSE_NOT_IMPL;
    ws_code_t* code = NULL;
    FILE* source = NULL;

    /* Skip program name */
    --argc, ++argv;

    if (argc != 1) goto cleanup;
    source = fopen(*argv, "r");
    if (!source) goto cleanup;

    ec = ws_compile(&code,
        source, (ws_rdfn_t)fread, ws_alloc, NULL);
    if (ec) goto cleanup;

    puts("==== BEGIN ====");
    ec = ws_disasm(code, stdout, (ws_wrfn_t)fwrite);
    if (ec) goto cleanup;
    puts("===== END =====");

cleanup:
    if (code) ws_destroy(code);
    if (source) fclose(source);
    return ec;
}