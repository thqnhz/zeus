#include "zeus.h"


const char *source_path;

Tokens tokens = {0};
Prog prog = {0};
IRProg ir = {0};
IRFn *curr_fn = NULL;
ConstPool pool = {0};

char *src;
static int readfile() {
    FILE *f = fopen(source_path, "rb");
    if (!f) return 1;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    src = malloc(size + 1);
    fread(src, 1, size, f);
    src[size] = '\0';
    fclose(f);
    return 0;
}

void error(Token t, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("%s:%u:%u: %s%serror:%s ",
            source_path, t.line, t.col,
            CRed, FBold, CReset);
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}


int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <path/to/file.zeus>\n", argv[0]);
        return 67;
    }
    source_path = argv[1];
    if (readfile() != 0) {
        printf("How would one expect to open a file that doesn't exist?\n\n");
        printf("Usage: %s <path/to/file.zeus>\n", argv[0]);
        return 69;
    }

    if (tokenize() != 0) {
        for (size_t i = 0; i < tokens.size; ++i) {
            Token *t = &tokens.tokens[i];
            if (t->type == TT_Ill) error(*t, "Unexpected token '%s'", t->lexeme);
        }
        printf("\nTokenizing failed. See errors above. You suck.\n");
        return 1;
    }

    tokens.pos = 0;
    if (parse() != 0) {
        printf("\nParsing failed. See errors above.\n");
        return 2;
    }

    lower_prog();
    dedup();
    // print_ir();
    if (codegen() != 0) {
        printf("\nCode gen failed");
        return 3;
    }
    return 0;
}

