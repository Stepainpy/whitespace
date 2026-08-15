#include <stdio.h>

static void process(FILE* src, FILE* dst) {
    int ch; while ((ch = fgetc(src)) != EOF)
        switch (ch) {
            case 'S': fputc('\x20', dst); break;
            case 'T': fputc('\x09', dst); break;
            case 'L': fputc('\x0A', dst); break;

            case ';':
                do ch = fgetc(src); while (ch != '\n' && ch != EOF);
            default:
                break;
        }
}

int main(int argc, char* argv[]) {
    --argc; ++argv; /* skip prog name */
    /*  */ if (argc == 0) {
        process(stdin, stdout);
        return 0;
    } else if (argc == 2) {
        FILE* src = NULL;
        FILE* dst = NULL;

        if (!(src = fopen(argv[0], "r"))) goto cleanup;
        if (!(dst = fopen(argv[1], "w"))) goto cleanup;

        process(src, dst);

    cleanup:
        if (dst) fclose(dst);
        if (src) fclose(src);
        return !src || !dst;
    } else
        return 1;
}