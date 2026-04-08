#include "zeus.h"


static int cursor = 0;
static int line = 1;
static int col = 1;

static void add_token_(TT type, const char *lexeme, int line, int col) {
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

static void add_token(TT type, const char *lexeme) {
    add_token_(type, lexeme, line, col);
}

void advance() {
    cursor++;
    col++;
}

static int is_number(char c) {
    return c >= '0' && c <= '9';
}

static int is_alpha(char c) {
    return (c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z')
        || c == '_';
}

static int e = 0;
int tokenize() {
    for (;;) {
        char c = src[cursor];
        if (c == '\0') break;
        switch (c) {
        case '\n':
            line++;
            col = 1;
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
        case ',':
            add_token(TT_Comma, ",");
            break;
        case '=':
            add_token(TT_Eq, "=");
            break;
        case '!':
            add_token(TT_Not, "!");
            break;
        case '{':
            add_token(TT_LBrc, "{");
            break;
        case '}':
            add_token(TT_RBrc, "}");
            break;
        case '+':
            add_token(TT_Plus, "+");
            break;
        case '-':
            add_token(TT_Minus, "-");
            break;
        case '*':
            add_token(TT_Star, "*");
            break;
        case '/':
            add_token(TT_Slash, "/");
            break;
        case '<':
            add_token(TT_Lt, "<");
            break;
        case '>':
            add_token(TT_Gt, ">");
            break;
        case '"': {
            int start = cursor;
            int start_col = col;
            advance();
            while (src[cursor] != '"' && src[cursor] != '\0') advance();
            advance();
            int len = cursor - start - 2;
            add_token_(TT_Str, strndup(src + start + 1, len), line, start_col);
            continue;
        }
        default:
            if (is_number(c)) {
                int start = cursor;
                int start_col = col;
                while (is_number(src[cursor])) advance();
                int len = cursor - start;
                add_token_(TT_Num, strndup(src + start, len), line, start_col);
                continue;
            } else if (is_alpha(c)) {
                int start = cursor;
                int start_col = col;
                while (is_alpha(src[cursor])) advance();
                int len = cursor - start;
                add_token_(TT_Id, strndup(src + start, len), line, start_col);
                continue;
            } else {
                char s[2] = { c, '\0' };
                add_token(TT_Ill, strdup(s));
                e = 1;
            }
        }
        advance();
    }
    free(src);
    return e;
}

