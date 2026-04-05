#include "zeus.h"



static int temp_off(int id) { return (id + 1) * 8; }

static void emit_inst(FILE *f, IRInst *i) {
    switch (i->kind) {
    case I_LoadStr: {
        fprintf(f, "  mov rax, const_s%d\n", pool_str(i->l.v.str));
        fprintf(f, "  mov [rbp-%d], rax\n", temp_off(i->dest.v.tmp_id));
        break;
    }
    case I_LoadVar: {
        int is_str_glb = 0;
        for (size_t j = 0; j < ir.glb_count; ++j) {
            if (strcmp(i->l.v.name, ir.glbs[j].name) == 0) {
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
        fprintf(f, "  mov rdi, [rbp-%d]\n", temp_off(i->dest.v.tmp_id));
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

static char *dir;
static char *asm_path;
static char *obj_path;
static char *exe_path;

static void cleanup() {
    free(dir);
    free(asm_path);
    free(obj_path);
    free(exe_path);
}

static int run_cmd(const char *cmd, char *const argv[]) {
    pid_t pid = fork();
    if (pid < 0) return 1;
    if (pid == 0) {
        execvp(cmd, argv);
        _exit(1);
    }
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) return 0;
    return 1;
}

int codegen() {
    // Find the right most slash
    const char *slash = strrchr(source_path, '/');
    if (slash) {
        asprintf(&dir, "%.*s", (int)(slash - source_path), source_path);
    } else asprintf(&dir, ".");

    asprintf(&asm_path, "%s/out.asm", dir);
    asprintf(&obj_path, "%s/out.o", dir);
    asprintf(&exe_path, "%s/out", dir);

    FILE *f = fopen(asm_path, "w");
    if (!f) {
        fprintf(stderr, "Failed to open %s\n", asm_path);
        cleanup();
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

    char *nasm[] = { "nasm", "-felf64", "-o", obj_path, asm_path, NULL };
    if (run_cmd("nasm", nasm) != 0) {
        cleanup();
        return 1;
    }

    char *gcc[] = { "gcc", "-no-pie", "-o", exe_path, obj_path, NULL };
    if (run_cmd("gcc", gcc) != 0) {
        cleanup();
        return 1;
    }

    cleanup();
    return 0;
}

