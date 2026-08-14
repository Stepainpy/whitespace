#include <whitespace/whitespace.h>
#include "defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WST_USAGE \
    "USAGE"                     "\n" \
    "    ws [OPTIONS] <script>" "\n" \

#define WST_OPTIONS \
    "OPTIONS"                                                                               "\n" \
    "    -t, --translate-lower   Translate s, t and l symbols to space, tab and line feed." "\n" \
    "    -T, --translate-upper   Translate S, T and L symbols to space, tab and line feed." "\n" \
    ""                                                                                      "\n" \
    "    --help              Display this information and quit"                             "\n" \

static void* ws_alloc(void* ptr, size_t size, void* ud) {
    if (size) return realloc(ptr, size);
    free(ptr); (void)ud; return NULL;
}

static int wsb_tl_lower = 0;
static int wsb_tl_upper = 0;

static size_t ws_read(void* buf, size_t size, size_t count, void* file) {
    size_t out = fread(buf, size, count, file), i;
    for (i = 0; i < out; i += size) {
        unsigned char* ptr = (unsigned char*)buf + i;
        /**/ if ((wsb_tl_lower && *ptr == 's') || (wsb_tl_upper && *ptr == 'S')) *ptr = WSC_S_CHAR;
        else if ((wsb_tl_lower && *ptr == 't') || (wsb_tl_upper && *ptr == 'T')) *ptr = WSC_T_CHAR;
        else if ((wsb_tl_lower && *ptr == 'l') || (wsb_tl_upper && *ptr == 'L')) *ptr = WSC_L_CHAR;
        else *ptr = '\0';
    }
    return out;
}

int main(int argc, char* argv[]) {
    ws_error_t ec = WSE_OK;
    ws_code_t* code = NULL;
    FILE* source = NULL;
    int i;

    --argc, ++argv; /* skip program name */

    for (i = 0; i < argc; i++)
        if (argv[i][0] == '-') {
            /*  */ if (argv[i][1] == '-' && strcmp(argv[i] + 2, "help") == 0) {
                fputs(WST_USAGE  , stdout);
                fputc('\n'       , stdout);
                fputs(WST_OPTIONS, stdout);
                goto cleanup;
            } else if (argv[i][1] == '-' && strcmp(argv[i] + 2, "translate-lower") == 0) {
                wsb_tl_lower = 1;
            } else if (argv[i][1] == '-' && strcmp(argv[i] + 2, "translate-upper") == 0) {
                wsb_tl_upper = 1;
            } else if (argv[i][1] == 't' && argv[i][2] == '\0') {
                wsb_tl_lower = 1;
            } else if (argv[i][1] == 'T' && argv[i][2] == '\0') {
                wsb_tl_upper = 1;
            } else {
                fprintf(stderr, "[ERROR]: unknown option '%s'\n", argv[i]);
                goto cleanup;
            }
        } else {
            if (source) {
                fprintf(stderr, "[ERROR]: script file already defined\n");
                goto cleanup;
            }
            source = fopen(argv[i], "r");
            if (!source) {
                fprintf(stderr, "[ERROR]: couldn't open script file '%s'\n", argv[i]);
                goto cleanup;
            }
        }

    if (!source) {
        fprintf(stderr, "[ERROR]: script file not defined\n");
        goto cleanup;
    }

    ec = ws_compile(&code, source,
        (wsb_tl_lower || wsb_tl_upper)
            ? ws_read : (ws_rdfn_t)fread,
        ws_alloc, NULL);
    if (ec) goto cleanup;
    fclose(source); source = NULL;

    ec = ws_execute(code,
        stdin,  (ws_rdfn_t)fread,
        stdout, (ws_wrfn_t)fwrite);
    if (ec) goto cleanup;

cleanup:
    if (code) ws_destroy(code);
    if (source) fclose(source);
    if (ec) fprintf(stderr, "[ERROR]: %s\n", ws_strerror(ec));
    return ec;
}