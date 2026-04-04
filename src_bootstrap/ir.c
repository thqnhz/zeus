#include "zeus.h"




static inline IROp op_const(float v) {
    return (IROp){ O_Const, .v.constant = v };
}

static inline IROp op_str(const char *s) {
    return (IROp){ O_Str, .v.str = strdup(s) };
}

static inline IROp op_tmp(int id) {
    return (IROp){ O_Tmp, .v.tmp_id = id };
}

static inline IROp op_var(const char *name) {
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

static inline int new_tmp() {
    return curr_fn->next_tmp++;
}

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
    if (strcmp(fn->name, "main") == 0) {
        int has_ret = fn->count > 0 && fn->inst[fn->count - 1].kind == I_Ret;
        if (!has_ret) emit(I_Ret, op_const(0.0f), (IROp){0}, (IROp){0});
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

int pool_float(float v) {
    for (size_t i = 0; i < pool.f_count; ++i) if (pool.floats[i] == v) return i;
    if (pool.f_count >= pool.f_cap) {
        pool.f_cap = pool.f_cap ? pool.f_cap * 2 : 8;
        pool.floats = realloc(pool.floats, pool.f_cap * sizeof(float));
    }
    pool.floats[pool.f_count++] = v;
    return pool.f_count - 1;
}

int pool_str(const char *s) {
    for (size_t i = 0; i < pool.s_count; ++i) if (strcmp(pool.strings[i], s) == 0) return i;
    if (pool.s_count >= pool.s_cap) {
        pool.s_cap = pool.s_cap ? pool.s_cap * 2 : 8;
        pool.strings = realloc(pool.strings, pool.s_cap * sizeof(char *));
    }
    pool.strings[pool.s_count++] = strdup(s);
    return pool.s_count - 1;
}

void dedup() {
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

void lower_prog() {
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

