#include <stdio.h>
#include "defines.h"

int main(int argc, char* argv[]) {
    --argc; ++argv; /* skip prog name */
    /*  */ if (argc == 0) {
        int ch; while ((ch = fgetc(stdin)) != EOF)
            switch (ch) {
                case 'S': fputc(WSC_S_CHAR, stdout); break;
                case 'T': fputc(WSC_T_CHAR, stdout); break;
                case 'L': fputc(WSC_L_CHAR, stdout); break;

                case ';':
                    do ch = fgetc(stdin); while (ch != '\n' && ch != EOF);
                default:
                    break;
            }
        return 0;
    } else if (argc == 2) {
        FILE* src = NULL;
        FILE* dst = NULL;
        int ch;

        if (!(src = fopen(argv[0], "r"))) goto cleanup;
        if (!(dst = fopen(argv[1], "w"))) goto cleanup;

        while ((ch = fgetc(src)) != EOF)
            switch (ch) {
                case 'S': fputc(WSC_S_CHAR, dst); break;
                case 'T': fputc(WSC_T_CHAR, dst); break;
                case 'L': fputc(WSC_L_CHAR, dst); break;

                case ';':
                    do ch = fgetc(src); while (ch != '\n' && ch != EOF);
                default:
                    break;
            }

    cleanup:
        if (dst) fclose(dst);
        if (src) fclose(src);
        return !src || !dst;
    } else
        return 1;
}