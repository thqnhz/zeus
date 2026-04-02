#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>


typedef enum TTKw {
    TT_Exit,
} TTKw;

typedef enum TTSep {
    TT_Semi,
    TT_LP,
    TT_RP,
} TTSep;

typedef enum TTLiteral {
    TT_Num,
} TTLiteral;

typedef struct TokenKw {
    TTKw type;
} TokenKw;

typedef struct TokenSep {
    TTSep type;
} TokenSep;

typedef struct TokenLiteral {
    TTLiteral type;
    float value;
} TokenLiteral;

char *src;
int cursor = 0;

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

static inline bool is_number(char c) {
    return c >= '0' && c <= '9';
}

static inline bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z')
        || c == '_';
}

static void tokenize() {
    for (;;) {
        char c = src[cursor];
        if (c == '\0') break;
        switch (c) {
        case '\n':
        case '\t':
        case '\r':
        case ' ':
            break;
        case '(':
            printf("Open Par: (\n");
            break;
        case ')':
            printf("Close Par: )\n");
            break;
        case ';':
            printf("Semicolon: ;\n");
            break;
        default:
            if (is_number(c)) {
                int start = cursor;
                while (is_number(src[cursor])) cursor++;
                printf("Number: %.*s\n", cursor - start, src + start);
            } else if (is_alpha(c)) {
                int start = cursor;
                while (is_alpha(src[cursor])) cursor++;
                printf("Identifier: %.*s\n", cursor - start, src + start);
            } else {
                printf("Unknown token: %c\n", c);
            }
            continue;
        }
        cursor++;
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
    return 0;
}

