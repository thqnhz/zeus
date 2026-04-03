#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>


#define ANSI(c) ansi[c]

typedef enum TT {
    TT_Ill,

    // Separators
    TT_Dot,
    TT_Comma,
    TT_Semi,
    TT_LP,
    TT_RP,
    TT_LBrc,
    TT_RBrc,
    TT_LBrk,
    TT_RBrk,

    // Literals
    TT_Id,
    TT_Num,
    TT_Str,

    // Comparisons
    TT_Eq,
    TT_Ne,
    TT_Lt,
    TT_Le,
    TT_Gt,
    TT_Ge,

    // Arithmetic
    TT_Plus,
    TT_Minus,
    TT_Star,
    TT_Slash,
    TT_Perc,

    // Logicals
    TT_And,
    TT_Or,
    TT_Not,
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
    size_t pos;
} Tokens;

typedef struct Expr {
    enum { E_Lit, E_V, E_Bin, E_Un, E_Call, } kind;
    union {
        float lit;
        char *var;
        struct { struct Expr *lhs, *rhs; Token op; } bin;
        struct { Token op; struct Expr *rhs; } un;
        // TODO: multiple args passing
        struct { char *name; struct Expr *args; } call;
    } v;
} Expr;

typedef struct Stmt {
    enum { S_E, S_Ret, S_Print, S_Exit, S_V, } kind;
    union {
        Expr *expr;
        Expr *ret;
        Expr *print;
        Expr *exit;
        struct { char *name; Expr *init; } var;
    } v;
} Stmt;

typedef struct Fn {
    char *name;
    char *params;
    Stmt **body;
    size_t body_count;
    size_t body_cap;
} Fn;

typedef struct Prog {
    Fn **fns;
    size_t fn_count;
    size_t fn_cap;
    struct { char *name; Expr *init; } *glb;
    size_t glb_count;
    size_t glb_cap;
} Prog;

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

const char *source_path;

Tokens tokens = {0};
Prog prog = {0};

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

static void error(Token t, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("%s:%u:%u: %s%serror:%s ",
            source_path, t.line, t.col,
            ANSI(CRed), ANSI(FBold), ANSI(CReset));
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
    has_error = true;
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
        case '<':
            add_token(TT_Lt, "<");
            break;
        case '>':
            add_token(TT_Gt, ">");
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
                add_token_(TT_Id, strdup(buf), line, start_col);
                continue;
            } else {
                add_token(TT_Ill, strdup(&c));
                has_error = true;
            }
        }
        advance();
    }
}

static Token peek() {
    if (tokens.pos < tokens.size) return tokens.tokens[tokens.pos];
    return (Token){ TT_Ill, "", .v.nil = NULL, 0, 0 };
}

static Token advance_token() { return tokens.tokens[tokens.pos++]; }

static bool match_kw(const char *kw) {
    if (peek().type == TT_Id && strcmp(peek().lexeme, kw) == 0) {
        advance_token();
        return true;
    }
    return false;
}

static void sync_() {
    while (tokens.pos < tokens.size) {
        TT t = peek().type;
        if (t == TT_Semi || t == TT_RBrc) {
            advance_token();
            break;
        }
        advance_token();
    }
}

static Token expect(TT type, const char *msg) {
    Token t = peek();
    if (t.type != type) {
        error(t, "%s, got '%s'", msg, t.lexeme);
        sync_();
    }
    return advance_token();
}

static void error_at_current(const char *msg) { error(peek(), msg); }

static Expr *new_expr() { return calloc(1, sizeof(Expr)); }

static Stmt *new_stmt() { return calloc(1, sizeof(Stmt)); }

static Expr *parse_expr();

static Expr *parse_primary() {
    Token t = peek();
    if (t.type == TT_Num) {
        Token t = advance_token();
        Expr *e = new_expr();
        e->kind = E_Lit;
        e->v.lit = atof(t.lexeme);
        return e;
    }
    if (t.type == TT_LP) {
        advance_token();
        Expr *e = parse_expr();
        expect(TT_RP, "expected ')' after expression");
        return e;
    }
    error_at_current("expected expression");
    return NULL;
}

static Expr *parse_call() {
    if (peek().type == TT_Id) {
        Token name = advance_token();
        if (peek().type == TT_LP) {
            Expr *arg = NULL;
            if (peek().type != TT_RP) {
                arg = parse_expr();
            }
            expect(TT_RP, "expected ')' after function call");
            Expr *call = new_expr();
            call->kind = E_Call;
            call->v.call.name = strdup(name.lexeme);
            call->v.call.args = arg;
            return call;
        }
        Expr *e = new_expr();
        e->kind = E_V;
        e->v.var = strdup(name.lexeme);
        return e;
    }
    return parse_primary();
}

static Expr *parse_un() {
    if (peek().type == TT_Minus
        || peek().type == TT_Not
    ) {
        Token op = advance_token();
        Expr *e = parse_un();
        Expr *un = new_expr();
        un->kind = E_Un;
        un->v.un.op = op;
        un->v.un.rhs = e;
        return un;
    }
    return parse_call();
}

static Expr *parse_mul() {
    Expr *e = parse_un();
    while (peek().type == TT_Star
        || peek().type == TT_Slash
        || peek().type == TT_Perc
    ) {
        Token op = advance_token();
        Expr *rhs = parse_un();
        Expr *bin = new_expr();
        bin->kind = E_Bin;
        bin->v.bin.lhs = e;
        bin->v.bin.op = op;
        bin->v.bin.rhs = rhs;
        e = bin;
    }
    return e;
}

static Expr *parse_add() {
    Expr *e = parse_mul();
    while (peek().type == TT_Plus
        || peek().type ==TT_Minus
    ) {
        Token op = advance_token();
        Expr *rhs = parse_mul();
        Expr *bin = new_expr();
        bin->kind = E_Bin;
        bin->v.bin.lhs = e;
        bin->v.bin.op = op;
        bin->v.bin.rhs = rhs;
        e = bin;
    }
    return e;
}

static Expr *parse_expr() {
    return parse_add();
}

static Stmt *parse_stmt() {
    Stmt *s = new_stmt();
    if (match_kw("v")) {
        Token name = expect(TT_Id, "expected variable name");
        expect(TT_Eq, " expected '=' after variable name");
        Expr *init = parse_expr();
        expect(TT_Semi, " expected ';' after variable declaration");
        s->kind = S_V;
        s->v.var.name = strdup(name.lexeme);
        s->v.var.init = init;
    } else if (match_kw("ret")) {
        Expr *e = parse_expr();
        expect(TT_Semi, " expected ';' after return");
        s->kind = S_Ret;
        s->v.ret = e;
    } else if (match_kw("print")) {
        Expr *e = parse_expr();
        expect(TT_Semi, " expected ';' after print");
        s->kind = S_Print;
        s->v.print = e;
    } else if (match_kw("exit")) {
        Expr *e = parse_expr();
        expect(TT_Semi, " expected ';' after exit");
        s->kind = S_Exit;
        s->v.exit = e;
    } else {
        Expr *e = parse_expr();
        expect(TT_Semi, " expected ';' after expression");
        s->kind = S_E;
        s->v.expr = e;
    }
    return s;
}

static Fn *parse_fn() {
    Token name = expect(TT_Id, " expected function name");
    expect(TT_LP, " expected '(' after function name");
    char *param = NULL;
    if (peek().type == TT_Id) {
        param = strdup(advance_token().lexeme);
    }
    expect(TT_RP, " expected ')' after function name");
    expect(TT_LBrc, " expected '{' before function body");
    Fn *fn = malloc(sizeof(Fn));
    fn->name = strdup(name.lexeme);
    fn->params = param;
    fn->body = NULL;
    fn->body_count = 0;
    fn->body_cap = 0;

    while (tokens.pos < tokens.size && peek().type != TT_RBrc) {
        Stmt *s = parse_stmt();
        if (fn->body_count >= fn->body_cap) {
            fn->body_cap = fn->body_cap ? fn->body_cap * 2 : 8;
            fn->body = realloc(fn->body, fn->body_cap * sizeof(Stmt *));
        }
        fn->body[fn->body_count++] = s;
    }
    expect(TT_RBrc, " expected '}' after function body");
    return fn;
}

static void parse() {
    while (tokens.pos < tokens.size) {
        if (match_kw("f")) {
            Fn *f = parse_fn();
            if (prog.fn_count >= prog.fn_cap) {
                prog.fn_cap = prog.fn_cap ? prog.fn_cap * 2 : 8;
                prog.fns = realloc(prog.fns, prog.fn_cap * sizeof(Fn *));
            }
            prog.fns[prog.fn_count++] = f;
        } else if (match_kw("v")) {
            Token name = expect(TT_Id, "expected variable name");
            expect(TT_Eq, " expected '=' after variable name");
            Expr *init = parse_expr();
            expect(TT_Semi, " expected ';' after variable declaration");
            if (prog.glb_count >= prog.glb_cap) {
                prog.glb_cap = prog.glb_cap ? prog.glb_cap * 2 : 8;
                prog.glb = realloc(prog.glb, prog.glb_cap * sizeof(*prog.glb));
            }
            prog.glb[prog.glb_count].name = strdup(name.lexeme);
            prog.glb[prog.glb_count].init = init;
            prog.glb_count++;
        } else {
            error_at_current("expected 'f' or 'v' at top level");
            sync_();
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <path/to/file.zeus>\n", argv[0]);
        return 67;
    }
    source_path = argv[1];
    if (access(source_path, F_OK) != 0) {
        printf("How would one expect to open a file that doesn't exist?\n\n");
        printf("Usage: %s <path/to/file.zeus>\n", argv[0]);
        return 69;
    }
    readfile(source_path);
    tokenize();

    if (has_error) {
        for (size_t i = 0; i < tokens.size; ++i) {
            Token *t = &tokens.tokens[i];
            if (t->type == TT_Ill) error(*t, "Unexpected token '%s'", t->lexeme);
        }
        printf("\nTokenizing failed. See errors above. You suck.\n");
        return 1;
    }
    tokens.pos = 0;
    parse();

    if (has_error) {
        printf("\nParsing failed. See errors above.\n");
        return 2;
    }
    return 0;
}

