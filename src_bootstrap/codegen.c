#include "zeus.h"



static void get_dir(const char *path, char *dir, size_t size) {
    // Find the right most slash
    const char *slash = strrchr(path, '/');
    if (slash) {
        size_t len = slash - path;
        snprintf(dir, size, "%.*s", (int)len, path);
    } else snprintf(dir, size, ".");
}

static int temp_off(int id) { return (id + 1) * 8; }

static void emit_inst(FILE *f, IRInst *i) {
    switch (i->kind) {
    case I_LoadStr: {
        fprintf(f, "  mov rdi, const_s%d\n", pool_str(i->l.v.str));
        break;
    }
    case I_LoadVar: {
        int is_str_glb = 0;
        for (size_t j = 0; j < prog.glb_count; ++j) {
            if (strcmp(i->l.v.name, prog.glbs[j].name) == 0) {
                is_str_glb = ir.glbs[j].str_idx != -1;
                break;
            }
        }
        if (is_str_glb) {
            fprintf(f, "  mov rax, [rel %s]\n", i->l.v.name);
            fprintf(f, "  mov [rbp-%d], rax\n", temp_off(i->dest.v.tmp_id));
        } else {
            fprintf(f, "  movsd xmm0, [rel %s]\n", i->l.v.name);
            fprintf(f, "  movsd [rbp-%d], xmm0\n", temp_off(i->dest.v.tmp_id));
        }
        break;
    }
    case I_PrintStr: {
        fprintf(f, "  mov rdi, [rbp-%d]\n", temp_off(i->l.v.tmp_id));
        fprintf(f, "  xor eax, eax\n");
        fprintf(f, "  call puts\n");
        break;
    }
    case I_Ret: {
        if (i->dest.kind == O_Const) fprintf(f, "  mov eax, %d\n", (int)i->dest.v.constant);
        else fprintf(f, "  mov eax, 0\n");
        fprintf(f, "  leave\n");
        fprintf(f, "  ret\n");
        break;
    }
    }
}

static void emit_fn(FILE *f, IRFn *fn) {
    fprintf(f, "%s:\n", fn->name);
    fprintf(f, "  push rbp\n");
    fprintf(f, "  mov rbp, rsp\n");

    int stack = fn->next_tmp * 8;
    if (stack % 16 != 0) stack += 8;
    if (stack > 0) fprintf(f, "  sub rsp, %d\n", stack);

    for (size_t i = 0; i < fn->count; ++i) emit_inst(f, &fn->inst[i]);

    fprintf(f, "\n");
}

int codegen() {
    char dir[512];
    get_dir(source_path, dir, sizeof(dir));
    char asm_path[512];
    char obj_path[512];
    char exe_path[512];
    snprintf(asm_path, sizeof(asm_path), "%s/out.asm", dir);
    snprintf(obj_path, sizeof(obj_path), "%s/out.o", dir);
    snprintf(exe_path, sizeof(exe_path), "%s/out", dir);

    FILE *f = fopen(asm_path, "w");
    if (!f) {
        fprintf(stderr, "Failed to open %s\n", asm_path);
        return 1;
    }

    fprintf(f, "SECTION .data\n");
    for (size_t i = 0; i < ir.glb_count; ++i) {
        if (ir.glbs[i].str_idx != -1)
            fprintf(f, "%s: dq const_s%d\n", ir.glbs[i].name, ir.glbs[i].str_idx);
        else
            fprintf(f, "%s: dq %f\n", ir.glbs[i].name, ir.glbs[i].val);
    }
    fprintf(f, "\n");

    fprintf(f, "SECTION .rodata\n");
    for (size_t i = 0; i < pool.f_count; ++i)
        fprintf(f, "  const_f%zu: dq %f\n",
            i, pool.floats[i]);
    for (size_t i = 0; i < pool.s_count; ++i)
        fprintf(f, "  const_s%zu: db \"%s\", 0\n",
            i, pool.strings[i]);
    fprintf(f, "  fmt_float: db \"%%g\", 10, 0\n\n");

    fprintf(f, "SECTION .text\n");
    fprintf(f, "  EXTERN puts\n");
    fprintf(f, "  global main\n\n");

    for (size_t i = 0; i < ir.count; ++i) emit_fn(f, &ir.fns[i]);

    fclose(f);

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "nasm -felf64 -o %s %s", obj_path, asm_path);
    if (system(cmd) != 0) return 1;
    snprintf(cmd, sizeof(cmd), "gcc -no-pie -o %s %s", exe_path, obj_path);
    if (system(cmd) != 0) return 1;
    return 0;
}

