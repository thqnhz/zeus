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
    enum { E_Lit, E_Str, E_V, E_Bin, E_Un, E_Call, } kind;
    union {
        float lit;
        char *var;
        char *str;
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
    struct { char *name; Expr *init; } *glbs;
    size_t glb_count;
    size_t glb_cap;
} Prog;

typedef enum OpKind {
    O_Const,
    O_Tmp,
    O_Var,
    O_Str,
} OpKind;

typedef struct IROp {
    OpKind kind;
    union {
        float constant;
        int tmp_id;
        char *name;
        char *str;
    } v;
} IROp;

typedef enum IRKind {
    I_LoadConst, I_LoadStr, I_LoadVar, I_StoreVar,
    I_Add, I_Sub, I_Mul, I_Div,
    I_Neg, I_Call, I_Print, I_PrintStr, I_Ret, I_Exit,
} IRKind;

typedef struct IRInstruction {
    IRKind kind;
    IROp dest;
    IROp l;
    IROp r;
    char *fn_name;
    IROp call_arg;
} IRInst;

typedef struct IRFn {
    char *name;
    char *param;
    IRInst *inst;
    size_t count;
    size_t cap;
    int next_tmp;
} IRFn;

typedef struct IRProg {
    IRFn *fns;
    size_t count;
    size_t cap;
    struct { char *name; float val; } *glbs;
    size_t glb_count;
    size_t glb_cap;
} IRProg;

typedef struct ConstPool {
    float *floats;
    size_t f_count;
    size_t f_cap;
    char **strings;
    size_t s_count;
    size_t s_cap;
} ConstPool;

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
IRProg ir = {0};
IRFn *curr_fn = NULL;
static ConstPool pool = {0};

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
        case '"': {
            int start = cursor;
            int start_col = col;
            advance();
            while (src[cursor] != '"' && src[cursor] != '\0') advance();
            advance();
            int len = cursor - start - 2;
            char buf[len + 1];
            memcpy(buf, src + start + 1, len);
            buf[len] = '\0';
            add_token_(TT_Str, strdup(buf), line, start_col);
            continue;
        }
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
    if (t.type == TT_Str) {
        advance_token();
        Expr *e = new_expr();
        e->kind = E_Str;
        e->v.str = strdup(t.lexeme);
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
                prog.glbs = realloc(prog.glbs, prog.glb_cap * sizeof(*prog.glbs));
            }
            prog.glbs[prog.glb_count].name = strdup(name.lexeme);
            prog.glbs[prog.glb_count].init = init;
            prog.glb_count++;
        } else {
            error_at_current("expected 'f' or 'v' at top level");
            sync_();
        }
    }
}

static IROp op_const(float v) {
    return (IROp){ O_Const, .v.constant = v };
}

static IROp op_str(const char *s) {
    return (IROp){ O_Str, .v.str = strdup(s) };
}

static IROp op_tmp(int id) {
    return (IROp){ O_Tmp, .v.tmp_id = id };
}

static IROp op_var(const char *name) {
    return (IROp){ O_Var, .v.name = strdup(name) };
}

static void emit(IRKind kind, IROp dst, IROp lhs, IROp rhs) {
    if (curr_fn->count >= curr_fn->cap) {
        curr_fn->cap = curr_fn->cap ? curr_fn->cap * 2 : 8;
        curr_fn->inst = realloc(curr_fn->inst, curr_fn->cap * sizeof(IRInst));
    }
    curr_fn->inst[curr_fn->count++] = (IRInst){
        .kind = kind,
        .dest = dst,
        .l = lhs,
        .r = rhs
    };
}

static int new_tmp() { return curr_fn->next_tmp++; }

static IROp lower_expr(Expr *e) {
    switch (e->kind) {
    case E_Lit: {
        int t = new_tmp();
        emit(I_LoadConst, op_tmp(t), op_const(e->v.lit), (IROp){0});
        return op_tmp(t);
    }
    case E_Str: {
        int t = new_tmp();
        emit(I_LoadStr, op_tmp(t), op_str(e->v.str), (IROp){0});
        return op_tmp(t);
    }
    case E_V: {
        int t = new_tmp();
        emit(I_LoadVar, op_tmp(t), op_var(e->v.var), (IROp){0});
        return op_tmp(t);
    }
    case E_Bin: {
        IROp l = lower_expr(e->v.bin.lhs);
        IROp r = lower_expr(e->v.bin.rhs);
        int t = new_tmp();
        IRKind op;
        switch (e->v.bin.op.type) {
        case TT_Plus: op = I_Add; break;
        case TT_Minus: op = I_Sub; break;
        case TT_Star: op = I_Mul; break;
        case TT_Slash: op = I_Div; break;
        default: op = I_Add; break;
        }
        emit(op, op_tmp(t), l, r);
        return op_tmp(t);
    }
    case E_Un: {
        IROp r = lower_expr(e->v.un.rhs);
        int t = new_tmp();
        emit(I_Neg, op_tmp(t), r, (IROp){0});
        return op_tmp(t);
    }
    case E_Call: {
        IROp arg = e->v.call.args ? lower_expr(e->v.call.args) : (IROp){0};
        int t = new_tmp();
        IRInst inst = {
            .kind = I_Call,
            .dest = op_tmp(t),
            .fn_name = strdup(e->v.call.name),
            .call_arg = arg,
        };
        if (curr_fn->count >= curr_fn->cap) {
            curr_fn->cap = curr_fn->cap ? curr_fn->cap * 2 : 8;
            curr_fn->inst = realloc(curr_fn->inst, curr_fn->cap * sizeof(IRInst));
        }
        curr_fn->inst[curr_fn->count++] = inst;
        return op_tmp(t);
    }
    }
    return (IROp){0};
}

static void lower_stmt(Stmt *s) {
    switch (s->kind) {
    case S_V: {
        IROp val = lower_expr(s->v.var.init);
        emit(I_StoreVar, (IROp){0}, val, op_var(s->v.var.name));
        break;
    }
    case S_Ret: {
        IROp val = lower_expr(s->v.ret);
        emit(I_Ret, val, (IROp){0}, (IROp){0});
        break;
    }
    case S_Print: {
        if (s->v.print->kind == E_Str) {
            IROp val = lower_expr(s->v.print);
            emit(I_PrintStr, val, (IROp){0}, (IROp){0});
        } else {
            IROp val = lower_expr(s->v.print);
            emit(I_Print, val, (IROp){0}, (IROp){0});
        }
        break;
    }
    case S_Exit: {
        IROp val = lower_expr(s->v.exit);
        emit(I_Exit, val, (IROp){0}, (IROp){0});
        break;
    }
    case S_E: {
        lower_expr(s->v.expr);
        break;
    }
    }
}

static void lower_fn(Fn *f) {
    if (ir.count >= ir.cap) {
        ir.cap = ir.cap ? ir.cap * 2 : 8;
        ir.fns = realloc(ir.fns, ir.cap * sizeof(IRFn));
    }
    IRFn *fn = &ir.fns[ir.count++];
    fn->name = strdup(f->name);
    fn->param = f->params ? strdup(f->params) : NULL;
    fn->inst = NULL;
    fn->count = 0;
    fn->cap = 0;
    fn->next_tmp = 0;
    curr_fn = fn;
    for (size_t i = 0; i < f->body_count; ++i) {
        lower_stmt(f->body[i]);
    }
}

static void lower_prog() {
    for (size_t i = 0; i < prog.glb_count; ++i) {
        if (ir.glb_count >= ir.glb_cap) {
            ir.glb_cap = ir.glb_cap ? ir.glb_cap * 2 : 8;
            ir.glbs = realloc(ir.glbs, ir.glb_cap * sizeof(*ir.glbs));
        }
        ir.glbs[ir.glb_count].name = strdup(prog.glbs[i].name);
        Expr *e = prog.glbs[i].init;
        if (e && e->kind == E_Lit) {
            ir.glbs[ir.glb_count].val = e->v.lit;
        }
        ir.glb_count++;
    }
    for (size_t i = 0; i < prog.fn_count; ++i) {
        lower_fn(prog.fns[i]);
    }
}

static void print_ir() {
    for (size_t i = 0; i < ir.count; i++) {
        IRFn *f = &ir.fns[i];
        printf("FUNC %s(%s)\n", f->name, f->param ? f->param : "");
        for (size_t j = 0; j < f->count; j++) {
            IRInst *inst = &f->inst[j];
            switch (inst->kind) {
            case I_LoadConst:
                printf("  t%d = LOAD_CONST %.1f\n", inst->dest.v.tmp_id, inst->l.v.constant);
                break;
            case I_LoadVar:
                printf("  t%d = LOAD_VAR %s\n", inst->dest.v.tmp_id, inst->l.v.name);
                break;
            case I_LoadStr:
                printf("  t%d = LOAD_STR %s\n", inst->dest.v.tmp_id, inst->l.v.str);
                break;
            case I_StoreVar:
                printf("  STORE_VAR %s, t%d\n", inst->r.v.name, inst->l.v.tmp_id);
                break;
            case I_Add:
                printf("  t%d = t%d + t%d\n", inst->dest.v.tmp_id, inst->l.v.tmp_id, inst->r.v.tmp_id);
                break;
            case I_Sub:
                printf("  t%d = t%d - t%d\n", inst->dest.v.tmp_id, inst->l.v.tmp_id, inst->r.v.tmp_id);
                break;
            case I_Mul:
                printf("  t%d = t%d * t%d\n", inst->dest.v.tmp_id, inst->l.v.tmp_id, inst->r.v.tmp_id);
                break;
            case I_Div:
                printf("  t%d = t%d / t%d\n", inst->dest.v.tmp_id, inst->l.v.tmp_id, inst->r.v.tmp_id);
                break;
            case I_Neg:
                printf("  t%d = -t%d\n", inst->dest.v.tmp_id, inst->l.v.tmp_id);
                break;
            case I_Call:
                printf("  t%d = CALL %s(t%d)\n", inst->dest.v.tmp_id, inst->fn_name, inst->call_arg.v.tmp_id);
                break;
            case I_Print:
                printf("  PRINT t%d\n", inst->dest.v.tmp_id);
                break;
            case I_PrintStr:
                printf("  PRINTSTR t%d\n", inst->dest.v.tmp_id);
                break;
            case I_Ret:
                printf("  RET t%d\n", inst->dest.v.tmp_id);
                break;
            case I_Exit:
                printf("  EXIT t%d\n", inst->dest.v.tmp_id);
                break;
            }
        }
        printf("END\n\n");
    }
}

static int pool_float(float v) {
    for (size_t i = 0; i < pool.f_count; ++i) if (pool.floats[i] == v) return i;
    if (pool.f_count >= pool.f_cap) {
        pool.f_cap = pool.f_cap ? pool.f_cap * 2 : 8;
        pool.floats = realloc(pool.floats, pool.f_cap * sizeof(float));
    }
    pool.floats[pool.f_count++] = v;
    return pool.f_count - 1;
}

static int pool_str(const char *s) {
    for (size_t i = 0; i < pool.s_count; ++i) if (strcmp(pool.strings[i], s) == 0) return i;
    if (pool.s_count >= pool.s_cap) {
        pool.s_cap = pool.s_cap ? pool.s_cap * 2 : 8;
        pool.strings = realloc(pool.strings, pool.s_cap * sizeof(char *));
    }
    pool.strings[pool.s_count++] = strdup(s);
    return pool.s_count - 1;
}

static void dedup() {
    for (size_t i = 0; i < ir.count; ++i) {
        IRFn *f = &ir.fns[i];
        for (size_t j = 0; j < f->count; ++j) {
            IRInst *inst = &f->inst[j];
            if (inst->kind == I_LoadConst) {
                pool_float(inst->l.v.constant);
            } else if (inst->kind == I_LoadStr) {
                pool_str(inst->l.v.str);
            }
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

    lower_prog();
    dedup();
    print_ir();
    return 0;
}

