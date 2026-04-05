#include "zeus.h"


static Token peek() {
    if (tokens.pos < tokens.size) return tokens.tokens[tokens.pos];
    return (Token){ TT_Ill, "", 0, 0 };
}

static Token advance_token() {
    return tokens.tokens[tokens.pos++];
}

static int match_kw(const char *kw) {
    if (peek().type == TT_Id && strcmp(peek().lexeme, kw) == 0) {
        advance_token();
        return 1;
    }
    return 0;
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

static void error_at_current(const char *msg) { error(peek(), msg); }

static Token expect(TT type, const char *msg) {
    Token t = peek();
    if (t.type != type) {
        error(t, "%s, got '%s'", msg, t.lexeme);
        sync_();
    }
    return advance_token();
}

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

static int e = 0;
int parse() {
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
            e = 1;
        }
    }
    return e;
}

