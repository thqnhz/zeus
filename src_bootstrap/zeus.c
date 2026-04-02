#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>


#define ANSI(c) ansi[c]

typedef enum TT {
    TT_Ill,

    // Keywords
    TT_Exit,

    // Separators
    TT_Semi,
    TT_LP,
    TT_RP,

    // Literals
    TT_Id,
    TT_Num,
    TT_Str,
} TT;

typedef struct Token {
    TT type;
    const char *lexeme;
    union {
        float num;
        char *content;
        void *nil;
    } v;
    int line;
    int col;
} Token;

typedef struct Tokens {
    Token *tokens;
    size_t size;
    size_t cap;
} Tokens;

typedef struct Kws {
    const char *name;
    TT kw;
} Kws;

typedef enum Ansi {
    CReset,
    CRed,
    CGreen,
    CYellow,
    CBlue,

    FBold,
    FUnderline,
} Ansi;

static const char *ansi[] = {
    [CReset]     = "\x1b[0m",
    [CRed]       = "\x1b[31m",
    [CGreen]     = "\x1b[32m",
    [CYellow]    = "\x1b[33m",
    [CBlue]      = "\x1b[34m",

    [FBold]      = "\x1b[1m",
    [FUnderline] = "\x1b[4m",
};

Tokens tokens = {0};

static Kws kws[] = {
    { "exit", TT_Exit },
};

char *src;
int cursor = 0;
int line = 1;
int col = 1;
bool has_error = false;

static void readfile(const char *path) {
    FILE *f = fopen(path, "rb");
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    src = malloc(size + 1);
    fread(src, 1, size, f);
    src[size] = '\0';
    fclose(f);
}

void advance() {
    cursor++;
    col++;
}

static inline bool is_number(char c) {
    return c >= '0' && c <= '9';
}

static inline bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z')
        || c == '_';
}

void add_token_(TT type, const char *lexeme, int line, int col) {
    if (tokens.size >= tokens.cap) {
        tokens.cap = tokens.cap ? tokens.cap * 2 : 8;
        tokens.tokens = realloc(tokens.tokens, tokens.cap * sizeof(Token));
    }
    tokens.tokens[tokens.size++] = (Token){
        .type = type,
        .lexeme = lexeme,
        .col = col,
        .line = line,
    };
}

void add_token(TT type, const char *lexeme) {
    add_token_(type, lexeme, line, col);
}

static void tokenize() {
    for (;;) {
        char c = src[cursor];
        if (c == '\0') break;
        switch (c) {
        case '\n':
            line++;
            col = 0;
        case '\t':
        case '\r':
        case ' ':
            break;
        case '(':
            add_token(TT_LP, "(");
            break;
        case ')':
            add_token(TT_RP, ")");
            break;
        case ';':
            add_token(TT_Semi, ";");
            break;
        default:
            if (is_number(c)) {
                int start = cursor;
                int start_col = col;
                while (is_number(src[cursor])) advance();
                int len = cursor - start;
                char buf[len + 1];
                memcpy(buf, src + start, len);
                buf[len] = '\0';
                add_token_(TT_Num, strdup(buf), line, start_col);
                continue;
            } else if (is_alpha(c)) {
                int start = cursor;
                int start_col = col;
                while (is_alpha(src[cursor])) advance();
                int len = cursor - start;
                char buf[len + 1];
                memcpy(buf, src + start, len);
                buf[len] = '\0';
                bool is_kw = false;
                for (size_t i = 0; i < sizeof(kws) / sizeof(kws[0]); ++i) {
                    if (strcmp(buf, kws[i].name) == 0) {
                        add_token_(kws[i].kw, strdup(buf), line, start_col);
                        is_kw = true;
                        break;
                    }
                }
                if (!is_kw) add_token_(TT_Id, strdup(buf), line, start_col);
                continue;
            } else {
                add_token(TT_Ill, strdup(&c));
                has_error = true;
            }
        }
        advance();
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <path/to/file.zeus>\n", argv[0]);
        return 67;
    }
    if (access(argv[1], F_OK) != 0) {
        printf("How would one expect to open a file that doesn't exist?\n\n");
        printf("Usage: %s <path/to/file.zeus>\n", argv[0]);
        return 69;
    }
    readfile(argv[1]);
    tokenize();

    for (size_t i = 0; i < tokens.size; ++i) {
        Token *t = &tokens.tokens[i];
        if (t->type == TT_Ill)
            printf(
                "%s:%u:%u: "
                "%s%serror:%s "
                "Unexpected token \"%s\"\n",
                argv[1], t->line, t->col,
                ANSI(CRed), ANSI(FBold), ANSI(CReset),
                t->lexeme
            );
    }

    if (has_error) {
        printf("\nParsing failed. See errors above. You suck.\n");
        return 1;
    }
    return 0;
}

