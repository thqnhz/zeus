#ifndef ZEUS_H
#define ZEUS_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define ANSI(c) ansi[c]

//
// lexer.c
//

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

extern Tokens tokens;

int tokenize();


//
// parser.c
//

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

extern Prog prog;

int parse();


//
// ir.c
//

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

extern IRProg ir;
extern IRFn *curr_fn;
extern ConstPool pool;

void lower_prog();
void dedup();
int pool_float(float v);
int pool_str(const char *s);


//
// codegen.c
//

extern const char *source_path;

void codegen();

//
// zeus.c
//

extern char *src;

void error(Token t, const char *fmt, ...);

#endif // ZEUS_H_

