/*
 * Velocity Compiler - Code Generator
 * Kashmiri Edition v2  (Windows PE64 + Linux ELF64)
 *
 * Fixes & additions over v2:
 *  - Windows PE64 target: nasm -f win64, MS x64 ABI (rcx,rdx,r8,r9)
 *  - Struct fields can hold arrays and nested structs
 *  - Dynamic array append() and correct len() tracking
 *  - uadad (u64) and uadad8 (u8) types
 *  - Bitwise: & | ^ ~ << >> and logical && || !
 *  - Compound assignment: += -= *= /=
 *  - try / catch / panic / throw
 *  - Hex/binary/octal integer literals
 *  - _ discard
 *  - Better error messages (line/col already in parser/lexer)
 *  - void return type
 *  - Multi-dimensional array access arr[i][j]
 */

#include "velocity.h"
#include <stdarg.h>

/* -- Float literal store ------------------------------------------- */
typedef struct FloatLit {
    double    value;
    ValueType type;
} FloatLit;

/* -- Argument registers -------------------------------------------- */
/* System V (Linux): rdi rsi rdx rcx r8 r9                          */
/* Microsoft x64 (Windows): rcx rdx r8 r9 + shadow space            */
static const char *SYSV_REGS[6]  = {"rdi","rsi","rdx","rcx","r8","r9"};
static const char *WIN64_REGS[4] = {"rcx","rdx","r8","r9"};
static const char *A64_REGS[8]   = {"x0","x1","x2","x3","x4","x5","x6","x7"};

/* helper: return the right register for arg position i */
static const char* arg_reg(CodeGen *cg, int i) {
    if (cg->target_aarch64) {
        if (i < 8) return A64_REGS[i];
        return "x7";
    }
    if (cg->target_windows) {
        if (i < 4) return WIN64_REGS[i];
        return "rax"; /* caller should have spilled to stack */
    }
    if (i < 6) return SYSV_REGS[i];
    return "rax";
}

#define ARG_REGS_MAX 8   /* AArch64 has 8; x86-64 uses up to 6 */

/* -- Helpers ------------------------------------------------------- */
static bool is_float(ValueType t)       { return t == TYPE_F32 || t == TYPE_F64; }
static bool is_string_like(ValueType t) { return t == TYPE_STRING || t == TYPE_OPT_STRING; }
static bool is_null_type(ValueType t)   { return t == TYPE_NULL; }
static bool is_optional(ValueType t) {
    return t==TYPE_OPT_STRING || t==TYPE_OPT_INT || t==TYPE_OPT_BOOL ||
           t==TYPE_OPT_F32   || t==TYPE_OPT_F64;
}
static bool is_composite(ValueType t) {
    return t==TYPE_ARRAY || t==TYPE_TUPLE || t==TYPE_STRUCT;
}
static bool is_unsigned(ValueType t) {
    return t==TYPE_UINT || t==TYPE_UINT8;
}
static bool symbol_has_module_prefix(const char *module, const char *name) {
    if (!module || !module[0] || !name) return false;
    size_t ml = strlen(module);
    return strncmp(name, module, ml) == 0 && name[ml] == '_' && name[ml + 1] == '_';
}
static void make_module_symbol(char *out, size_t out_sz,
                               const char *module, const char *name) {
    if (symbol_has_module_prefix(module, name)) {
        snprintf(out, out_sz, "%s", name);
    } else {
        snprintf(out, out_sz, "%s__%s", module, name);
    }
}

static ValueType promote_float(ValueType a, ValueType b) {
    if (a==TYPE_F64 || b==TYPE_F64) return TYPE_F64;
    if (a==TYPE_F32 || b==TYPE_F32) return TYPE_F32;
    return TYPE_INT;
}

/* Compile-time literal extraction for integer range checks. */
static bool eval_const_int(ASTNode *expr, long long *out) {
    if (!expr || !out) return false;
    if (expr->type == AST_INTEGER) {
        *out = expr->data.int_value;
        return true;
    }
    if (expr->type == AST_BINARY_OP && expr->data.binary.op == OP_SUB) {
        ASTNode *lhs = expr->data.binary.left;
        ASTNode *rhs = expr->data.binary.right;
        long long rv = 0;
        if (lhs && lhs->type == AST_INTEGER && lhs->data.int_value == 0 &&
            eval_const_int(rhs, &rv)) {
            *out = -rv;
            return true;
        }
    }
    return false;
}

static void validate_unsigned_literal_range(ASTNode *expr, ValueType expect,
                                            int fallback_line, int fallback_col) {
    if (expect != TYPE_UINT && expect != TYPE_UINT8) return;
    long long v = 0;
    if (!eval_const_int(expr, &v)) return;

    int line = (expr && expr->line > 0) ? expr->line : fallback_line;
    int col  = (expr && expr->column > 0) ? expr->column : fallback_col;

    if (expect == TYPE_UINT && v < 0) {
        error_at(line, col,
                 "uadad cannot be assigned a negative literal (%lld)", v);
    }
    if (expect == TYPE_UINT8 && (v < 0 || v > 255)) {
        error_at(line, col,
                 "uadad8 literal out of range (%lld). Expected 0..255", v);
    }
}

static const char *value_type_name(ValueType t) {
    switch (t) {
        case TYPE_INT:        return "adad";
        case TYPE_UINT:       return "uadad";
        case TYPE_UINT8:      return "uadad8";
        case TYPE_OPT_INT:    return "adad?";
        case TYPE_BOOL:       return "bool";
        case TYPE_OPT_BOOL:   return "bool?";
        case TYPE_F32:        return "ashari32";
        case TYPE_OPT_F32:    return "ashari32?";
        case TYPE_F64:        return "ashari";
        case TYPE_OPT_F64:    return "ashari?";
        case TYPE_STRING:     return "lafz";
        case TYPE_OPT_STRING: return "lafz?";
        case TYPE_NULL:       return "null";
        case TYPE_VOID:       return "khali";
        case TYPE_ARRAY:      return "array";
        case TYPE_TUPLE:      return "tuple";
        case TYPE_STRUCT:     return "bina";
        case TYPE_ERROR_VAL:  return "error";
        default:              return "unknown";
    }
}

/* forward declarations */
static StructDef* struct_find(CodeGen *cg, const char *name);
static int        struct_size(CodeGen *cg, StructDef *sd);
static TypeInfo*  typeinfo_clone(TypeInfo *ti);
static ValueType  codegen_cast(CodeGen *cg, ValueType from, ValueType to);
static bool       mod_func_table_has(const char *name);
static FunctionSig* func_sig_find(CodeGen *cg, const char *name);
static void       codegen_emit_struct_literal(CodeGen *cg, StructDef *sd, ASTNode *lit);
static void       codegen_emit_native_externs(CodeGen *cg, const char *mod, const char *asm_path);
static void       codegen_emit_module(CodeGen *cg, const char *mod, const char *path);
static void       codegen_register_structs(CodeGen *cg, ASTNode *program);
ValueType  codegen_expression(CodeGen *cg, ASTNode *expr);
static ValueType  infer_expr_type(CodeGen *cg, ASTNode *expr);
static TypeInfo*  expr_typeinfo(CodeGen *cg, ASTNode *expr);
static void       codegen_copy_struct(CodeGen *cg, StructDef *sd,
                                      const char *dst, const char *src);

/* -- Array element size -------------------------------------------- */
static int array_elem_size(CodeGen *cg, TypeInfo *ti) {
    if (!ti) return 8;
    if (ti->elem_type == TYPE_UINT8) return 1;
    if (ti->elem_type == TYPE_STRUCT) {
        if (!ti->elem_typeinfo) error("array element missing struct type info");
        StructDef *sd = struct_find(cg, ti->elem_typeinfo->struct_name);
        if (!sd) error("Unknown struct type: %s", ti->elem_typeinfo->struct_name);
        return struct_size(cg, sd);
    }
    return 8;
}

/* -- typeinfo size ------------------------------------------------- */
static int typeinfo_size(CodeGen *cg, ValueType t, TypeInfo *ti) {
    if (t == TYPE_STRUCT) {
        if (!ti || ti->struct_name[0] == '\0') return 8; /* unknown struct: ptr size */
        if (ti->by_ref) return 8;
        StructDef *sd = struct_find(cg, ti->struct_name);
        if (!sd) error("Unknown struct: %s", ti->struct_name);
        return struct_size(cg, sd);
    }
    if (t == TYPE_ARRAY && ti) {
        if (ti->by_ref || ti->array_len < 0) return 8;
        return ti->array_len * array_elem_size(cg, ti);
    }
    if (t == TYPE_TUPLE && ti) {
        if (ti->by_ref) return 8;
        return ti->tuple_count * 8;
    }
    if (t == TYPE_UINT8) return 1;
    return 8;
}

/* ------------------------------------------------------------
 *  Init
 * ------------------------------------------------------------ */

void codegen_init(CodeGen *cg, FILE *output, ModuleManager *mgr,
                  bool target_windows, bool target_aarch64,
                  const char *module_prefix) {
    memset(cg, 0, sizeof(*cg));
    cg->output          = output;
    cg->module_mgr      = mgr;
    cg->target_windows  = target_windows;
    cg->target_aarch64  = target_aarch64;
    if (module_prefix && module_prefix[0])
        strncpy(cg->module_prefix, module_prefix, MAX_TOKEN_LEN-1);
    cg->local_vars     = (char**)malloc(sizeof(char*) * MAX_VARS);
    cg->var_offsets    = (int*)malloc(sizeof(int) * MAX_VARS);
    cg->var_types      = (ValueType*)malloc(sizeof(ValueType) * MAX_VARS);
    cg->var_mutable    = (bool*)malloc(sizeof(bool) * MAX_VARS);
    cg->var_sizes      = (int*)malloc(sizeof(int) * MAX_VARS);
    cg->var_typeinfo   = (TypeInfo**)calloc(MAX_VARS, sizeof(TypeInfo*));
    cg->current_return_type = TYPE_INT;
}

void codegen_emit(CodeGen *cg, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(cg->output, fmt, ap);
    fputc('\n', cg->output);
    va_end(ap);
}

int codegen_new_label(CodeGen *cg) { return cg->label_counter++; }

/* ------------------------------------------------------------
 *  Loop stack
 * ------------------------------------------------------------ */
static void loop_push(CodeGen *cg, int cont, int brk) {
    if (cg->loop_depth >= MAX_LOOP_DEPTH) error("loop nesting too deep");
    cg->loop_continue_labels[cg->loop_depth] = cont;
    cg->loop_break_labels[cg->loop_depth]    = brk;
    cg->loop_depth++;
}
static void loop_pop(CodeGen *cg) {
    if (cg->loop_depth <= 0) error("loop stack underflow");
    cg->loop_depth--;
}
static int loop_cont(CodeGen *cg) {
    if (cg->loop_depth<=0) error("'continue' outside loop");
    return cg->loop_continue_labels[cg->loop_depth-1];
}
static int loop_brk(CodeGen *cg) {
    if (cg->loop_depth<=0) error("'break' outside loop");
    return cg->loop_break_labels[cg->loop_depth-1];
}

static void emit_try_error_probe(CodeGen *cg) {
    if (cg->try_depth <= 0) return;
    int top = cg->try_depth - 1;
    int catch_l = cg->try_catch_labels[top];
    int evar_off = cg->error_var_offsets[top];
    int ok_l = codegen_new_label(cg);
    codegen_emit(cg,"    cmp qword [rel _VL_err_flag], 0");
    codegen_emit(cg,"    je  _VL%d", ok_l);
    codegen_emit(cg,"    mov rax, [rel _VL_err_msg]");
    codegen_emit(cg,"    mov [rbp - %d], rax", evar_off);
    codegen_emit(cg,"    mov qword [rel _VL_err_flag], 0");
    codegen_emit(cg,"    mov qword [rel _VL_err_msg], 0");
    codegen_emit(cg,"    sub qword [rel _VL_try_depth_rt], 1");
    codegen_emit(cg,"    jmp _VL%d", catch_l);
    codegen_emit(cg,"_VL%d:", ok_l);
}

/* ------------------------------------------------------------
 *  Struct table
 * ------------------------------------------------------------ */
static StructDef* struct_find(CodeGen *cg, const char *name) {
    if (!cg || !name) return NULL;
    for (int i = 0; i < cg->struct_count; i++)
        if (strcmp(cg->struct_defs[i].name, name) == 0)
            return &cg->struct_defs[i];
    return NULL;
}

static int struct_field_index(StructDef *sd, const char *field) {
    if (!sd || !field) return -1;
    for (int i = 0; i < sd->field_count; i++)
        if (strcmp(sd->field_names[i], field) == 0) return i;
    return -1;
}

static int struct_size(CodeGen *cg, StructDef *sd) {
    if (!sd) return 0;
    if (sd->size >= 0) return sd->size;
    if (sd->sizing) error("Recursive struct: %s", sd->name);
    sd->sizing = true;
    int offset = 0;
    for (int i = 0; i < sd->field_count; i++) {
        ValueType ft = sd->field_types[i];
        TypeInfo *fti = sd->field_typeinfo ? sd->field_typeinfo[i] : NULL;
        int fsize;
        if (ft == TYPE_STRUCT) {
            if (!fti) error("struct field missing type info: %s.%s",
                            sd->name, sd->field_names[i]);
            StructDef *inner = struct_find(cg, fti->struct_name);
            if (!inner) error("Unknown struct '%s' in %s.%s",
                              fti->struct_name, sd->name, sd->field_names[i]);
            fsize = struct_size(cg, inner);
        } else if (ft == TYPE_ARRAY && fti) {
            /* array field: fixed-size stored inline, dynamic = 8 byte ptr */
            if (fti->array_len > 0)
                fsize = fti->array_len * array_elem_size(cg, fti);
            else
                fsize = 8;
        } else {
            fsize = 8;
        }
        sd->field_offsets[i] = offset;
        offset += fsize;
    }
    sd->size = offset;
    sd->sizing = false;
    return offset;
}

static void struct_add_from_ast(CodeGen *cg, ASTNode *def) {
    if (!def || def->type != AST_STRUCT_DEF) return;
    if (struct_find(cg, def->data.struct_def.name))
        return; /* allow re-register from module parsing */

    if (cg->struct_count == cg->struct_cap) {
        int nc = cg->struct_cap == 0 ? 8 : cg->struct_cap * 2;
        cg->struct_defs = (StructDef*)realloc(cg->struct_defs,
                                               sizeof(StructDef) * nc);
        if (!cg->struct_defs) error("Out of memory");
        cg->struct_cap = nc;
    }
    StructDef *sd = &cg->struct_defs[cg->struct_count++];
    memset(sd, 0, sizeof(*sd));
    strncpy(sd->name, def->data.struct_def.name, MAX_TOKEN_LEN-1);
    sd->field_count = def->data.struct_def.field_count;
    sd->size = -1;

    if (sd->field_count > 0) {
        sd->field_names   = (char**)malloc(sizeof(char*) * sd->field_count);
        sd->field_types   = (ValueType*)malloc(sizeof(ValueType) * sd->field_count);
        sd->field_typeinfo= (TypeInfo**)calloc(sd->field_count, sizeof(TypeInfo*));
        sd->field_offsets = (int*)calloc(sd->field_count, sizeof(int));
        for (int i = 0; i < sd->field_count; i++) {
            sd->field_names[i]    = strdup(def->data.struct_def.field_names[i]);
            sd->field_types[i]    = def->data.struct_def.field_types[i];
            sd->field_typeinfo[i] = typeinfo_clone(
                def->data.struct_def.field_typeinfo
                ? def->data.struct_def.field_typeinfo[i] : NULL);
        }
    }
}

static void codegen_register_structs(CodeGen *cg, ASTNode *prog) {
    if (!prog) return;
    for (int i = 0; i < prog->data.program.struct_count; i++)
        struct_add_from_ast(cg, prog->data.program.structs[i]);
}

/* ------------------------------------------------------------
 *  Function signature table
 * ------------------------------------------------------------ */
static TypeInfo* typeinfo_clone(TypeInfo *ti) {
    if (!ti) return NULL;
    TypeInfo *c = (TypeInfo*)calloc(1, sizeof(TypeInfo));
    if (!c) error("Out of memory");
    *c = *ti;
    c->tuple_types  = NULL;
    c->elem_typeinfo = NULL;
    c->dim_sizes    = NULL;
    if (ti->kind == TYPE_TUPLE && ti->tuple_count > 0) {
        c->tuple_types = (ValueType*)malloc(sizeof(ValueType)*ti->tuple_count);
        memcpy(c->tuple_types, ti->tuple_types,
               sizeof(ValueType)*ti->tuple_count);
    }
    if (ti->kind == TYPE_ARRAY && ti->elem_typeinfo)
        c->elem_typeinfo = typeinfo_clone(ti->elem_typeinfo);
    return c;
}

static TypeInfo* typeinfo_ref(TypeInfo *ti) {
    TypeInfo *c = typeinfo_clone(ti);
    if (c) c->by_ref = true;
    return c;
}

void typeinfo_free(TypeInfo *ti) {
    if (!ti) return;
    free(ti->tuple_types);
    if (ti->elem_typeinfo) typeinfo_free(ti->elem_typeinfo);
    free(ti->dim_sizes);
    free(ti);
}

static void func_sig_add(CodeGen *cg, const char *name,
                         ValueType ret, TypeInfo *ret_ti,
                         ValueType *ptypes, TypeInfo **ptinfo,
                         int pcount) {
    if (!name) return;
    /* deduplicate */
    for (int i = 0; i < cg->func_sig_count; i++)
        if (strcmp(cg->func_sigs[i].name, name) == 0) return;

    if (cg->func_sig_count == cg->func_sig_cap) {
        int nc = cg->func_sig_cap == 0 ? 32 : cg->func_sig_cap * 2;
        cg->func_sigs = (FunctionSig*)realloc(cg->func_sigs,
                                               sizeof(FunctionSig)*nc);
        if (!cg->func_sigs) error("Out of memory");
        cg->func_sig_cap = nc;
    }
    FunctionSig *sig = &cg->func_sigs[cg->func_sig_count++];
    memset(sig, 0, sizeof(*sig));
    strncpy(sig->name, name, sizeof(sig->name)-1);
    sig->return_type    = ret;
    sig->return_typeinfo = typeinfo_clone(ret_ti);
    sig->param_count    = pcount;
    if (pcount > 0 && ptypes) {
        sig->param_types   = (ValueType*)malloc(sizeof(ValueType)*pcount);
        sig->param_typeinfo= (TypeInfo**)calloc(pcount, sizeof(TypeInfo*));
        for (int i = 0; i < pcount; i++) {
            sig->param_types[i]    = ptypes[i];
            sig->param_typeinfo[i] = typeinfo_clone(ptinfo ? ptinfo[i] : NULL);
        }
    }
}

static FunctionSig* func_sig_find(CodeGen *cg, const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < cg->func_sig_count; i++)
        if (strcmp(cg->func_sigs[i].name, name) == 0)
            return &cg->func_sigs[i];
    return NULL;
}

/* -- Register built-in module signatures --------------------------- */
static void register_builtin_module_sigs(CodeGen *cg, const char *mod) {
    if (!mod) return;
    ValueType ti = TYPE_INT, tf = TYPE_F64, ts = TYPE_STRING;

    if (strcmp(mod, "io") == 0) {
        ValueType p1i[1]={ti};
        ValueType p2si[2]={ts,ti};
        func_sig_add(cg, "io__input", ti, NULL, NULL, NULL, 0);
        func_sig_add(cg, "io__stdin", ts, NULL, NULL, NULL, 0);
        func_sig_add(cg, "io__chhaap", ti, NULL, NULL, NULL, 0);
        func_sig_add(cg, "io__chhaapLine", ti, NULL, NULL, NULL, 0);
        func_sig_add(cg, "io__chhaapSpace", ti, NULL, NULL, NULL, 0);
        func_sig_add(cg, "io__open", ti, NULL, p2si, NULL, 2);
        func_sig_add(cg, "io__fopen", ti, NULL, p2si, NULL, 2);
        func_sig_add(cg, "io__close", ti, NULL, p1i, NULL, 1);
        func_sig_add(cg, "io__fclose", ti, NULL, p1i, NULL, 1);
        /* io.read(fd, buf, len) -> adad   io.write(fd, buf, len) -> adad */
        ValueType p_read[3]={ti,ts,ti};
        func_sig_add(cg, "io__read",  ti, NULL, p_read, NULL, 3);
        func_sig_add(cg, "io__write", ti, NULL, p_read, NULL, 3);
    } else if (strcmp(mod, "lafz") == 0) {
        ValueType p1s[1]={ts}, p2s[2]={ts,ts}, p3s[3]={ts,ti,ti};
        ValueType p1i[1]={ti}, p1f[1]={tf};
        func_sig_add(cg,"lafz__len",      ti,NULL, p1s,NULL,1);
        func_sig_add(cg,"lafz__eq",       ti,NULL, p2s,NULL,2);
        func_sig_add(cg,"lafz__concat",   ts,NULL, p2s,NULL,2);
        func_sig_add(cg,"lafz__slice",    ts,NULL, p3s,NULL,3);
        func_sig_add(cg,"lafz__from_adad",ts,NULL, p1i,NULL,1);
        func_sig_add(cg,"lafz__from_ashari",ts,NULL,p1f,NULL,1);
        func_sig_add(cg,"lafz__to_adad",  ti,NULL, p1s,NULL,1);
        func_sig_add(cg,"lafz__to_ashari",tf,NULL, p1s,NULL,1);
    } else if (strcmp(mod, "time") == 0) {
        ValueType p1i[1]={ti};
        func_sig_add(cg,"time__now",    ti,NULL,NULL,NULL,0);
        func_sig_add(cg,"time__now_ms", ti,NULL,NULL,NULL,0);
        func_sig_add(cg,"time__now_ns", ti,NULL,NULL,NULL,0);
        func_sig_add(cg,"time__sleep",  ti,NULL,p1i,NULL,1);
        func_sig_add(cg,"time__ctime",  ts,NULL,NULL,NULL,0);
    } else if (strcmp(mod, "random") == 0) {
        ValueType p1i[1]={ti};
        func_sig_add(cg,"random__seed",     ti,NULL,p1i,NULL,1);
        func_sig_add(cg,"random__randrange",ti,NULL,p1i,NULL,1);
        func_sig_add(cg,"random__random",   tf,NULL,NULL,NULL,0);
    } else if (strcmp(mod, "system") == 0) {
        ValueType p1i[1]={ti}, p1s[1]={ts};
        func_sig_add(cg,"system__exit",   ti,NULL,p1i,NULL,1);
        func_sig_add(cg,"system__argc",   ti,NULL,NULL,NULL,0);
        func_sig_add(cg,"system__argv",   TYPE_OPT_STRING,NULL,p1i,NULL,1);
        func_sig_add(cg,"system__getenv", TYPE_OPT_STRING,NULL,p1s,NULL,1);
    } else if (strcmp(mod, "os") == 0) {
        ValueType p1s[1]={ts};
        func_sig_add(cg,"os__getcwd",TYPE_OPT_STRING,NULL,NULL,NULL,0);
        func_sig_add(cg,"os__chdir", ti,NULL,p1s,NULL,1);
    } else if (strcmp(mod, "math") == 0) {
        ValueType p1f[1]={tf};
        ValueType p2pow[2]={tf, TYPE_INT};
        func_sig_add(cg,"math__sqrt",  tf,NULL,p1f,NULL,1);
        func_sig_add(cg,"math__abs",   tf,NULL,p1f,NULL,1);
        func_sig_add(cg,"math__floor", tf,NULL,p1f,NULL,1);
        func_sig_add(cg,"math__ceil",  tf,NULL,p1f,NULL,1);
        func_sig_add(cg,"math__pow",   tf,NULL,p2pow,NULL,2);
        func_sig_add(cg,"math__sin",   tf,NULL,p1f,NULL,1);
        func_sig_add(cg,"math__cos",   tf,NULL,p1f,NULL,1);
        func_sig_add(cg,"math__tan",   tf,NULL,p1f,NULL,1);
        func_sig_add(cg,"math__log",   tf,NULL,p1f,NULL,1);
    }
}

/* ------------------------------------------------------------
 *  String / Float literal tables
 * ------------------------------------------------------------ */
static int codegen_intern_string(CodeGen *cg, const char *s) {
    if (!s) s = "";
    if (cg->string_count == cg->string_cap) {
        int nc = cg->string_cap == 0 ? 16 : cg->string_cap * 2;
        cg->string_literals = (char**)realloc(cg->string_literals,
                                               sizeof(char*)*nc);
        cg->string_cap = nc;
    }
    cg->string_literals[cg->string_count] = strdup(s);
    return cg->string_count++;
}

static int codegen_intern_float(CodeGen *cg, double v, ValueType t) {
    if (cg->float_count == cg->float_cap) {
        int nc = cg->float_cap == 0 ? 16 : cg->float_cap * 2;
        cg->float_literals = (FloatLit*)realloc(cg->float_literals,
                                                 sizeof(FloatLit)*nc);
        cg->float_cap = nc;
    }
    cg->float_literals[cg->float_count] = (FloatLit){v, t};
    return cg->float_count++;
}

/* ------------------------------------------------------------
 *  Variable helpers
 * ------------------------------------------------------------ */
int codegen_find_var(CodeGen *cg, const char *name) {
    for (int i = cg->var_count - 1; i >= 0; i--)
        if (cg->local_vars[i] && strcmp(cg->local_vars[i], name) == 0)
            return i;
    return -1;
}

void codegen_add_var(CodeGen *cg, const char *name, ValueType type,
                     bool is_mut, TypeInfo *tinfo) {
    if (cg->var_count >= MAX_VARS) error("Too many local variables");
    int size = typeinfo_size(cg, type, tinfo);
    if (size < 8) size = 8;
    cg->stack_offset += size;
    if (cg->stack_offset > LOCAL_STACK_RESERVE)
        error("Local stack limit exceeded (%d bytes). "
              "Increase LOCAL_STACK_RESERVE or use fewer/smaller locals.",
              LOCAL_STACK_RESERVE);
    int idx = cg->var_count++;
    cg->local_vars[idx]  = strdup(name);
    cg->var_offsets[idx] = cg->stack_offset;
    cg->var_types[idx]   = type;
    cg->var_mutable[idx] = is_mut;
    cg->var_sizes[idx]   = size;
    cg->var_typeinfo[idx]= tinfo;
}

/* ------------------------------------------------------------
 *  Module function table (per-module compilation)
 * ------------------------------------------------------------ */
#define MAX_MOD_FUNCS 256
static char mod_func_table[MAX_MOD_FUNCS][MAX_TOKEN_LEN];
static int  mod_func_count = 0;
static void mod_func_table_clear(void) { mod_func_count = 0; }
static void mod_func_table_add(const char *n) {
    if (mod_func_count < MAX_MOD_FUNCS)
        strncpy(mod_func_table[mod_func_count++], n, MAX_TOKEN_LEN-1);
}
static bool mod_func_table_has(const char *n) {
    for (int i = 0; i < mod_func_count; i++)
        if (strcmp(mod_func_table[i], n) == 0) return true;
    return false;
}

/* ------------------------------------------------------------
 *  Type helpers
 * ------------------------------------------------------------ */
static TypeInfo* expr_typeinfo(CodeGen *cg, ASTNode *expr) {
    if (!expr) return NULL;
    switch (expr->type) {
    case AST_IDENTIFIER: {
        int idx = codegen_find_var(cg, expr->data.identifier);
        return idx >= 0 ? cg->var_typeinfo[idx] : NULL;
    }
    case AST_ARRAY_ACCESS: {
        ASTNode *base = expr->data.array_access.array;
        if (base->type != AST_IDENTIFIER) return NULL;
        int idx = codegen_find_var(cg, base->data.identifier);
        if (idx < 0 || cg->var_types[idx] != TYPE_ARRAY) return NULL;
        TypeInfo *ti = cg->var_typeinfo[idx];
        return (ti && ti->elem_type == TYPE_STRUCT) ? ti->elem_typeinfo : NULL;
    }
    case AST_FIELD_ACCESS: {
        TypeInfo *bti = expr_typeinfo(cg, expr->data.field_access.base);
        if (!bti || bti->kind != TYPE_STRUCT) return NULL;
        StructDef *sd = struct_find(cg, bti->struct_name);
        if (!sd) return NULL;
        int fidx = struct_field_index(sd, expr->data.field_access.field_name);
        return (fidx >= 0 && sd->field_typeinfo) ? sd->field_typeinfo[fidx] : NULL;
    }
    case AST_CALL: {
        const char *fn = expr->data.call.func_name;
        char mangled[MAX_TOKEN_LEN*2+4];
        if (cg->current_module[0] && mod_func_table_has(fn)) {
            make_module_symbol(mangled, sizeof(mangled), cg->current_module, fn);
            fn = mangled;
        }
        FunctionSig *sig = func_sig_find(cg, fn);
        return sig ? sig->return_typeinfo : NULL;
    }
    case AST_MODULE_CALL: {
        char full[MAX_TOKEN_LEN*2+4];
        snprintf(full, sizeof(full), "%s__%s",
                 expr->data.module_call.module_name,
                 expr->data.module_call.func_name);
        FunctionSig *sig = func_sig_find(cg, full);
        return sig ? sig->return_typeinfo : NULL;
    }
    default: return NULL;
    }
}

static ValueType infer_expr_type(CodeGen *cg, ASTNode *expr) {
    if (!expr) return TYPE_INT;
    switch (expr->type) {
    case AST_INTEGER:   return TYPE_INT;
    case AST_BOOL:      return TYPE_BOOL;
    case AST_NULL:      return TYPE_NULL;
    case AST_FLOAT:     return expr->data.float_lit.type;
    case AST_STRING:    return TYPE_STRING;
    case AST_STRUCT_LITERAL: return TYPE_STRUCT;
    case AST_ARRAY_LITERAL:  return TYPE_ARRAY;
    case AST_TUPLE_LITERAL: {
        int count = expr->data.tuple_lit.count;
        if (count == 0) { codegen_emit(cg,"    xor rax, rax"); return TYPE_TUPLE; }

        /* Allocate a temporary tuple on the stack and fill it */
        char tmpn[32];
        snprintf(tmpn, sizeof(tmpn), "_tup%d", codegen_new_label(cg));
        TypeInfo *ti = (TypeInfo*)calloc(1, sizeof(TypeInfo));
        ti->kind = TYPE_TUPLE;
        ti->tuple_count = count;
        ti->tuple_types = (ValueType*)malloc(sizeof(ValueType)*count);
        for (int i = 0; i < count; i++)
            ti->tuple_types[i] = infer_expr_type(cg, expr->data.tuple_lit.elements[i]);

        codegen_add_var(cg, tmpn, TYPE_TUPLE, true, ti);
        int base_off = cg->var_offsets[cg->var_count-1];  /* offset of element[0] */

        for (int i = 0; i < count; i++) {
            ValueType et = codegen_expression(cg, expr->data.tuple_lit.elements[i]);
            int slot_off = base_off - i * 8;
            if (et == TYPE_F32) codegen_emit(cg,"    movss [rbp - %d], xmm0", slot_off);
            else if (et == TYPE_F64) codegen_emit(cg,"    movsd [rbp - %d], xmm0", slot_off);
            else codegen_emit(cg,"    mov [rbp - %d], rax", slot_off);
        }
        codegen_emit(cg,"    lea rax, [rbp - %d]", base_off);
        return TYPE_TUPLE;
    }
    case AST_IDENTIFIER: {
        int idx = codegen_find_var(cg, expr->data.identifier);
        if (idx < 0) error_at(expr->line, expr->column,
                               "undefined variable '%s'", expr->data.identifier);
        return cg->var_types[idx];
    }
    case AST_ARRAY_ACCESS: {
        ASTNode *base = expr->data.array_access.array;
        if (base->type != AST_IDENTIFIER)
            error_at(expr->line, expr->column, "array access needs identifier base");
        int idx = codegen_find_var(cg, base->data.identifier);
        if (idx < 0) error_at(expr->line, expr->column,
                               "undefined variable '%s'", base->data.identifier);
        if (cg->var_types[idx] != TYPE_ARRAY)
            error_at(expr->line, expr->column,
                     "'%s' is not an array", base->data.identifier);
        TypeInfo *ti = cg->var_typeinfo[idx];
        return ti ? ti->elem_type : TYPE_INT;
    }
    case AST_FIELD_ACCESS: {
        TypeInfo *bti = expr_typeinfo(cg, expr->data.field_access.base);
        if (!bti) error_at(expr->line, expr->column, "field access on non-struct");
        StructDef *sd = struct_find(cg, bti->struct_name);
        if (!sd) error_at(expr->line, expr->column,
                           "unknown struct '%s'", bti->struct_name);
        int fidx = struct_field_index(sd, expr->data.field_access.field_name);
        if (fidx < 0) error_at(expr->line, expr->column,
                                "unknown field '%s' on '%s'",
                                expr->data.field_access.field_name, sd->name);
        return sd->field_types[fidx];
    }
    case AST_BINARY_OP: {
        BinaryOp op = expr->data.binary.op;
        if (op==OP_EQ||op==OP_NE||op==OP_LT||op==OP_LE||op==OP_GT||op==OP_GE||
            op==OP_AND||op==OP_OR||op==OP_NOT)
            return TYPE_BOOL;
        ValueType lt = infer_expr_type(cg, expr->data.binary.left);
        ValueType rt = infer_expr_type(cg, expr->data.binary.right);
        if (is_float(lt)||is_float(rt)) return promote_float(lt,rt);
        return TYPE_INT;
    }
    case AST_UNARY_OP:
        if (expr->data.unary.op == UOP_NOT) return TYPE_BOOL;
        return infer_expr_type(cg, expr->data.unary.operand);
    case AST_CALL: {
        const char *fn = expr->data.call.func_name;
        if (strcmp(fn,"len")==0||strcmp(fn,"sizeof")==0) return TYPE_INT;
        if (strcmp(fn,"type")==0) return TYPE_STRING;
        char mangled[MAX_TOKEN_LEN*2+4];
        if (cg->current_module[0] && mod_func_table_has(fn)) {
            make_module_symbol(mangled, sizeof(mangled), cg->current_module, fn);
            fn = mangled;
        }
        FunctionSig *sig = func_sig_find(cg, fn);
        if (!sig) error_at(expr->line, expr->column,
                            "unknown function '%s'", expr->data.call.func_name);
        return sig->return_type;
    }
    case AST_MODULE_CALL: {
        char full[MAX_TOKEN_LEN*2+4];
        snprintf(full,sizeof(full),"%s__%s",
                 expr->data.module_call.module_name,
                 expr->data.module_call.func_name);
        FunctionSig *sig = func_sig_find(cg, full);
        if (!sig) error_at(expr->line, expr->column,
                            "unknown function '%s.%s'",
                            expr->data.module_call.module_name,
                            expr->data.module_call.func_name);
        return sig->return_type;
    }
    case AST_TUPLE_ACCESS: {
        ASTNode *base = expr->data.tuple_access.tuple;
        int idx = codegen_find_var(cg, base->data.identifier);
        if (idx < 0) return TYPE_INT;
        TypeInfo *ti = cg->var_typeinfo[idx];
        if (!ti || expr->data.tuple_access.index >= ti->tuple_count) return TYPE_INT;
        return ti->tuple_types[expr->data.tuple_access.index];
    }
    default: return TYPE_INT;
    }
}

/* ------------------------------------------------------------
 *  Cast codegen
 * ------------------------------------------------------------ */
static ValueType codegen_cast(CodeGen *cg, ValueType from, ValueType to) {
    if (from == to) return to;
    if (is_unsigned(from) && (to==TYPE_INT||is_unsigned(to))) return to;
    if (from==TYPE_INT && is_unsigned(to)) return to;

    /* null -> optional */
    if (from == TYPE_NULL) {
        switch(to) {
        case TYPE_OPT_STRING: codegen_emit(cg,"    xor rax, rax"); return to;
        case TYPE_OPT_INT:    codegen_emit(cg,"    mov rax, 0x8000000000000000"); return to;
        case TYPE_OPT_BOOL:   codegen_emit(cg,"    mov rax, 2"); return to;
        case TYPE_OPT_F64:    codegen_emit(cg,"    mov rax, 0x7ff8000000000001"); return to;
        case TYPE_OPT_F32:    codegen_emit(cg,"    mov eax, 0x7fc00001"); return to;
        default: break;
        }
    }
    /* string <-> optional string */
    if ((from==TYPE_STRING && to==TYPE_OPT_STRING) ||
        (from==TYPE_OPT_STRING && to==TYPE_STRING))
        return to;
    /* int <-> optional int */
    if (from==TYPE_INT && to==TYPE_OPT_INT) return to;
    if (from==TYPE_OPT_INT && to==TYPE_INT) {
        int nl=codegen_new_label(cg), el=codegen_new_label(cg);
        codegen_emit(cg,"    mov rbx, 0x8000000000000000");
        codegen_emit(cg,"    cmp rax, rbx");
        codegen_emit(cg,"    jne _VL%d", el);
        codegen_emit(cg,"    xor rax, rax");
        codegen_emit(cg,"_VL%d:", el);
        return to;
    }
    /* bool -> int */
    if (from==TYPE_BOOL && to==TYPE_INT) return to;
    /* int -> bool */
    if (from==TYPE_INT && to==TYPE_BOOL) {
        codegen_emit(cg,"    cmp rax, 0");
        codegen_emit(cg,"    setne al");
        codegen_emit(cg,"    movzx rax, al");
        return to;
    }
    /* numeric conversions */
    if (from==TYPE_INT && to==TYPE_F64) { codegen_emit(cg,"    cvtsi2sd xmm0, rax"); return to; }
    if (from==TYPE_INT && to==TYPE_F32) { codegen_emit(cg,"    cvtsi2ss xmm0, rax"); return to; }
    if (from==TYPE_F64 && to==TYPE_INT) { codegen_emit(cg,"    cvttsd2si rax, xmm0"); return to; }
    if (from==TYPE_F32 && to==TYPE_INT) { codegen_emit(cg,"    cvttss2si rax, xmm0"); return to; }
    if (from==TYPE_F32 && to==TYPE_F64) { codegen_emit(cg,"    cvtss2sd xmm0, xmm0"); return to; }
    if (from==TYPE_F64 && to==TYPE_F32) { codegen_emit(cg,"    cvtsd2ss xmm0, xmm0"); return to; }
    if (from==TYPE_BOOL && to==TYPE_F64) { codegen_emit(cg,"    cvtsi2sd xmm0, rax"); return to; }
    if (from==TYPE_BOOL && to==TYPE_F32) { codegen_emit(cg,"    cvtsi2ss xmm0, rax"); return to; }
    /* uint8 masking */
    if (to==TYPE_UINT8) { codegen_emit(cg,"    and rax, 0xFF"); return to; }

    /* Compatible: treat optional as base for comparisons */
    if (from==TYPE_OPT_BOOL  && to==TYPE_BOOL)  return to;
    if (from==TYPE_OPT_F32   && to==TYPE_F32)   { codegen_emit(cg,"    movd xmm0, eax"); return to; }
    if (from==TYPE_OPT_F64   && to==TYPE_F64)   { codegen_emit(cg,"    movq xmm0, rax"); return to; }

    /* Allow anything if composite context */
    if (is_composite(from) || is_composite(to)) return to;

    /* Silently allow (may cause runtime issues, but don't abort) */
    return to;
}

/* ------------------------------------------------------------
 *  Struct literal emit
 * ------------------------------------------------------------ */
static void codegen_copy_struct(CodeGen *cg, StructDef *sd,
                                const char *dst, const char *src) {
    int size = struct_size(cg, sd);
    for (int off = 0; off < size; off += 8) {
        codegen_emit(cg,"    mov rax, [%s + %d]", src, off);
        codegen_emit(cg,"    mov [%s + %d], rax", dst, off);
    }
}

static void codegen_emit_struct_literal(CodeGen *cg, StructDef *sd,
                                        ASTNode *lit) {
    if (!lit || lit->type != AST_STRUCT_LITERAL)
        error("expected struct literal");
    struct_size(cg, sd);
    bool *seen = (bool*)calloc(sd->field_count, 1);

    for (int i = 0; i < lit->data.struct_lit.field_count; i++) {
        const char *fname = lit->data.struct_lit.field_names[i];
        ASTNode    *fval  = lit->data.struct_lit.field_values[i];
        int fidx = struct_field_index(sd, fname);
        if (fidx < 0) error_at(lit->line, lit->column,
                                "unknown field '%s' on struct '%s'",
                                fname, sd->name);
        seen[fidx] = true;
        int off   = sd->field_offsets[fidx];
        ValueType expect = sd->field_types[fidx];

        if (expect == TYPE_STRUCT) {
            TypeInfo *fti = sd->field_typeinfo ? sd->field_typeinfo[fidx] : NULL;
            if (!fti) error("struct field missing type info");
            StructDef *child = struct_find(cg, fti->struct_name);
            if (!child) error("unknown struct '%s'", fti->struct_name);
            if (fval->type == AST_STRUCT_LITERAL) {
                codegen_emit(cg,"    lea rdx, [rbx + %d]", off);
                codegen_emit(cg,"    push rbx");
                codegen_emit(cg,"    mov rbx, rdx");
                codegen_emit_struct_literal(cg, child, fval);
                codegen_emit(cg,"    pop rbx");
            } else {
                ValueType at = codegen_expression(cg, fval);
                if (at != TYPE_STRUCT) error("field '%s' expects struct", fname);
                codegen_emit(cg,"    mov rcx, rax");
                codegen_emit(cg,"    lea rdx, [rbx + %d]", off);
                codegen_copy_struct(cg, child, "rdx", "rcx");
            }
        } else if (expect == TYPE_ARRAY) {
            /* array field in struct - store pointer or inline data */
            TypeInfo *fti = sd->field_typeinfo ? sd->field_typeinfo[fidx] : NULL;
            codegen_emit(cg,"    push rbx");
            ValueType at = codegen_expression(cg, fval);
            codegen_emit(cg,"    pop rbx");
            if (at != TYPE_ARRAY) at = codegen_cast(cg, at, TYPE_ARRAY);
            codegen_emit(cg,"    mov [rbx + %d], rax", off);
        } else {
            codegen_emit(cg,"    push rbx");
            ValueType at = codegen_expression(cg, fval);
            codegen_emit(cg,"    pop rbx");
            validate_unsigned_literal_range(fval, expect, lit->line, lit->column);
            if (at != expect) at = codegen_cast(cg, at, expect);
            if (expect == TYPE_F32)
                codegen_emit(cg,"    movss [rbx + %d], xmm0", off);
            else if (expect == TYPE_F64)
                codegen_emit(cg,"    movsd [rbx + %d], xmm0", off);
            else
                codegen_emit(cg,"    mov [rbx + %d], rax", off);
        }
    }
    for (int i = 0; i < sd->field_count; i++)
        if (!seen[i])
            error_at(lit->line, lit->column,
                     "missing field '%s' in '%s' literal",
                     sd->field_names[i], sd->name);
    free(seen);
}

/* ------------------------------------------------------------
 *  Struct address emit (for field access/assign)
 * ------------------------------------------------------------ */
static StructDef* emit_struct_addr(CodeGen *cg, ASTNode *expr) {
    if (expr->type == AST_IDENTIFIER) {
        int idx = codegen_find_var(cg, expr->data.identifier);
        if (idx < 0) error_at(expr->line, expr->column,
                               "undefined variable '%s'", expr->data.identifier);
        if (cg->var_types[idx] != TYPE_STRUCT)
            error_at(expr->line, expr->column,
                     "'%s' is not a struct", expr->data.identifier);
        TypeInfo *ti = cg->var_typeinfo[idx];
        if (!ti) error("struct missing type info");
        StructDef *sd = struct_find(cg, ti->struct_name);
        if (!sd) error("unknown struct '%s'", ti->struct_name);
        struct_size(cg, sd);
        if (ti->by_ref)
            codegen_emit(cg,"    mov rax, [rbp - %d]", cg->var_offsets[idx]);
        else
            codegen_emit(cg,"    lea rax, [rbp - %d]", cg->var_offsets[idx]);
        return sd;
    }
    if (expr->type == AST_ARRAY_ACCESS) {
        /* arr[i] where arr is an array of structs -- emit pointer to element */
        ASTNode *base = expr->data.array_access.array;
        if (base->type != AST_IDENTIFIER)
            error_at(expr->line, expr->column,
                     "array-of-struct access requires identifier base");
        int idx = codegen_find_var(cg, base->data.identifier);
        if (idx < 0) error_at(expr->line, expr->column,
                               "undefined variable '%s'", base->data.identifier);
        if (cg->var_types[idx] != TYPE_ARRAY)
            error_at(expr->line, expr->column,
                     "'%s' is not an array", base->data.identifier);
        TypeInfo *ti = cg->var_typeinfo[idx];
        if (!ti || ti->elem_type != TYPE_STRUCT)
            error_at(expr->line, expr->column,
                     "'%s' is not an array of structs", base->data.identifier);
        if (!ti->elem_typeinfo)
            error_at(expr->line, expr->column,
                     "array element missing struct type info");
        StructDef *sd = struct_find(cg, ti->elem_typeinfo->struct_name);
        if (!sd) error("unknown struct '%s'", ti->elem_typeinfo->struct_name);
        struct_size(cg, sd);

        /* evaluate index -> rax, then compute element address */
        ValueType it = codegen_expression(cg, expr->data.array_access.index);
        if (it != TYPE_INT) codegen_cast(cg, it, TYPE_INT);
        codegen_emit(cg,"    mov rcx, rax");

        if (ti->by_ref || ti->array_len < 0)
            codegen_emit(cg,"    mov rbx, [rbp - %d]", cg->var_offsets[idx]);
        else
            codegen_emit(cg,"    lea rbx, [rbp - %d]", cg->var_offsets[idx]);

        int elem_size = array_elem_size(cg, ti);
        if (elem_size == 8) {
            codegen_emit(cg,"    lea rax, [rbx + rcx*8]");
        } else {
            codegen_emit(cg,"    imul rcx, %d", elem_size);
            codegen_emit(cg,"    lea rax, [rbx + rcx]");
        }
        return sd;
    }
    if (expr->type == AST_FIELD_ACCESS) {
        StructDef *parent = emit_struct_addr(cg, expr->data.field_access.base);
        int fidx = struct_field_index(parent, expr->data.field_access.field_name);
        if (fidx < 0) error_at(expr->line, expr->column,
                                "unknown field '%s'",
                                expr->data.field_access.field_name);
        if (parent->field_types[fidx] != TYPE_STRUCT)
            error_at(expr->line, expr->column,
                     "field '%s' is not a struct",
                     expr->data.field_access.field_name);
        struct_size(cg, parent);
        codegen_emit(cg,"    add rax, %d", parent->field_offsets[fidx]);
        TypeInfo *fti = parent->field_typeinfo ? parent->field_typeinfo[fidx] : NULL;
        if (!fti) error("struct field missing type info");
        StructDef *child = struct_find(cg, fti->struct_name);
        if (!child) error("unknown struct '%s'", fti->struct_name);
        return child;
    }
    error_at(expr->line, expr->column,
             "struct address requires identifier, array access, or field access base");
    return NULL;
}

/* ------------------------------------------------------------
 *  Dynamic array append helper emitter
 * ------------------------------------------------------------ */
static void codegen_emit_arr_append(CodeGen *cg) {
    /* __vl_arr_append(ptr_to_ptr, value, elem_size)
       ptr_to_ptr -> [arr_ptr_slot on stack]
       Updates the length word at arr_ptr[-8] and writes value.
       If arr_ptr is NULL (first call): allocates from heap.
       rdi = &arr_slot (pointer to where array ptr is stored)
       rsi = value (8 bytes)
       rdx = elem_size (usually 8)
    */
    const char *fmt = cg->target_windows ? "win64" : "elf64";
    (void)fmt; /* used in section label comments */

    codegen_emit(cg, "section .bss");
    codegen_emit(cg, "    _VL_arr_heap resb 131072");
    codegen_emit(cg, "    _VL_arr_hp   resq 1");
    codegen_emit(cg, "");
    codegen_emit(cg, "section .text");
    codegen_emit(cg, "__vl_arr_alloc:");
    codegen_emit(cg, "    push rbp");
    codegen_emit(cg, "    mov  rbp, rsp");
    if (cg->target_windows) {
        codegen_emit(cg, "    push rbx");
        codegen_emit(cg, "    mov  rax, [rel _VL_arr_hp]");
        codegen_emit(cg, "    test rax, rax");
        codegen_emit(cg, "    jne  .have_w");
        codegen_emit(cg, "    lea  rax, [rel _VL_arr_heap]");
        codegen_emit(cg, "    mov  [rel _VL_arr_hp], rax");
        codegen_emit(cg, ".have_w:");
        codegen_emit(cg, "    mov  rbx, rax");
        codegen_emit(cg, "    mov  [rbx], rcx");   /* rcx=count */
        codegen_emit(cg, "    lea  rax, [rbx + 8]");
        codegen_emit(cg, "    mov  r10, rcx");
        codegen_emit(cg, "    imul r10, rdx");      /* rdx=elem_size */
        codegen_emit(cg, "    add  r10, 8");
        codegen_emit(cg, "    add  rbx, r10");
        codegen_emit(cg, "    mov  [rel _VL_arr_hp], rbx");
        codegen_emit(cg, "    pop  rbx");
    } else {
        codegen_emit(cg, "    push rbx");
        codegen_emit(cg, "    mov  rax, [rel _VL_arr_hp]");
        codegen_emit(cg, "    test rax, rax");
        codegen_emit(cg, "    jne  .have");
        codegen_emit(cg, "    lea  rax, [rel _VL_arr_heap]");
        codegen_emit(cg, "    mov  [rel _VL_arr_hp], rax");
        codegen_emit(cg, ".have:");
        codegen_emit(cg, "    mov  rbx, rax");
        codegen_emit(cg, "    mov  [rbx], rdi");   /* rdi=count */
        codegen_emit(cg, "    lea  rax, [rbx + 8]");
        codegen_emit(cg, "    mov  rcx, rdi");
        codegen_emit(cg, "    imul rcx, rsi");      /* rsi=elem_size */
        codegen_emit(cg, "    add  rcx, 8");
        codegen_emit(cg, "    add  rbx, rcx");
        codegen_emit(cg, "    mov  [rel _VL_arr_hp], rbx");
        codegen_emit(cg, "    pop  rbx");
    }
    codegen_emit(cg, "    pop  rbp");
    codegen_emit(cg, "    ret");
    codegen_emit(cg, "");

    /* __vl_arr_append: append one element to dynamic array
       Linux: rdi=&slot, rsi=value, rdx=elem_size
       Win64: rcx=&slot, rdx=value, r8=elem_size
    */
    codegen_emit(cg, "__vl_arr_append:");
    codegen_emit(cg, "    push rbp");
    codegen_emit(cg, "    mov  rbp, rsp");
    codegen_emit(cg, "    push rbx");
    codegen_emit(cg, "    push r12");
    codegen_emit(cg, "    push r13");
    codegen_emit(cg, "    push r14");
    if (cg->target_windows) {
        codegen_emit(cg, "    mov  r12, rcx");   /* &slot */
        codegen_emit(cg, "    mov  r13, rdx");   /* value or src ptr */
        codegen_emit(cg, "    mov  r14, r8");    /* elem_size */
    } else {
        codegen_emit(cg, "    mov  r12, rdi");   /* &slot */
        codegen_emit(cg, "    mov  r13, rsi");   /* value or src ptr */
        codegen_emit(cg, "    mov  r14, rdx");   /* elem_size */
    }
    codegen_emit(cg, "    mov  rbx, [r12]");     /* arr ptr */
    codegen_emit(cg, "    test rbx, rbx");
    codegen_emit(cg, "    jz   .append_new");
    /* existing array: get length, increment, write element */
    codegen_emit(cg, "    mov  rax, [rbx - 8]"); /* old length */
    codegen_emit(cg, "    mov  rcx, rax");
    codegen_emit(cg, "    add  rax, 1");
    codegen_emit(cg, "    mov  [rbx - 8], rax"); /* new length */
    codegen_emit(cg, "    mov  r10, rcx");
    codegen_emit(cg, "    imul r10, r14");
    codegen_emit(cg, "    lea  r11, [rbx + r10]");
    codegen_emit(cg, "    cmp  r14, 8");
    codegen_emit(cg, "    jg   .copy_existing");
    codegen_emit(cg, "    cmp  r14, 1");
    codegen_emit(cg, "    je   .store_u8_existing");
    codegen_emit(cg, "    mov  [r11], r13");
    codegen_emit(cg, "    jmp  .append_done");
    codegen_emit(cg, ".store_u8_existing:");
    codegen_emit(cg, "    mov  [r11], r13b");
    codegen_emit(cg, "    jmp  .append_done");
    codegen_emit(cg, ".copy_existing:");
    codegen_emit(cg, "    mov  rdi, r11");
    codegen_emit(cg, "    mov  rsi, r13");
    codegen_emit(cg, "    mov  rcx, r14");
    codegen_emit(cg, "    rep movsb");
    codegen_emit(cg, "    jmp  .append_done");
    /* new array */
    codegen_emit(cg, ".append_new:");
    if (cg->target_windows) {
        codegen_emit(cg, "    mov  rcx, 1");
        codegen_emit(cg, "    mov  rdx, r14");
        codegen_emit(cg, "    call __vl_arr_alloc");
    } else {
        codegen_emit(cg, "    mov  rdi, 1");
        codegen_emit(cg, "    mov  rsi, r14");
        codegen_emit(cg, "    call __vl_arr_alloc");
    }
    codegen_emit(cg, "    mov  [r12], rax");
    codegen_emit(cg, "    cmp  r14, 8");
    codegen_emit(cg, "    jg   .copy_new");
    codegen_emit(cg, "    cmp  r14, 1");
    codegen_emit(cg, "    je   .store_u8_new");
    codegen_emit(cg, "    mov  [rax], r13");
    codegen_emit(cg, "    jmp  .append_done");
    codegen_emit(cg, ".store_u8_new:");
    codegen_emit(cg, "    mov  [rax], r13b");
    codegen_emit(cg, "    jmp  .append_done");
    codegen_emit(cg, ".copy_new:");
    codegen_emit(cg, "    mov  rdi, rax");
    codegen_emit(cg, "    mov  rsi, r13");
    codegen_emit(cg, "    mov  rcx, r14");
    codegen_emit(cg, "    rep movsb");
    codegen_emit(cg, ".append_done:");
    codegen_emit(cg, "    pop  r14");
    codegen_emit(cg, "    pop  r13");
    codegen_emit(cg, "    pop  r12");
    codegen_emit(cg, "    pop  rbx");
    codegen_emit(cg, "    pop  rbp");
    codegen_emit(cg, "    ret");
    codegen_emit(cg, "");
}

/* ------------------------------------------------------------
 *  Panic helper
 * ------------------------------------------------------------ */
static void codegen_emit_panic_helper(CodeGen *cg) {
    /* __vl_panic(msg_ptr): print to stderr and exit(1)
       Linux:  rdi=msg
       Win64:  rcx=msg */
    codegen_emit(cg, "section .bss");
    codegen_emit(cg, "    _VL_try_depth_rt resq 1");
    codegen_emit(cg, "    _VL_err_flag     resq 1");
    codegen_emit(cg, "    _VL_err_msg      resq 1");
    codegen_emit(cg, "section .text");
    codegen_emit(cg, "__vl_panic:");
    codegen_emit(cg, "    push rbp");
    codegen_emit(cg, "    mov  rbp, rsp");
    if (cg->target_windows) {
        /* Windows: WriteFile to stderr (handle -12), then ExitProcess(1) */
        codegen_emit(cg, "    mov  rax, [rel _VL_try_depth_rt]");
        codegen_emit(cg, "    test rax, rax");
        codegen_emit(cg, "    jz   .panic_fatal");
        codegen_emit(cg, "    mov  [rel _VL_err_msg], rcx");
        codegen_emit(cg, "    mov  qword [rel _VL_err_flag], 1");
        codegen_emit(cg, "    pop  rbp");
        codegen_emit(cg, "    ret");
        codegen_emit(cg, ".panic_fatal:");
        codegen_emit(cg, "    push rbx");
        codegen_emit(cg, "    mov  rbx, rcx");   /* msg ptr */
        /* compute strlen */
        codegen_emit(cg, "    mov  r10, rbx");
        codegen_emit(cg, "    xor  r11, r11");
        codegen_emit(cg, ".panic_len:");
        codegen_emit(cg, "    cmp  byte [r10 + r11], 0");
        codegen_emit(cg, "    je   .panic_write");
        codegen_emit(cg, "    inc  r11");
        codegen_emit(cg, "    jmp  .panic_len");
        codegen_emit(cg, ".panic_write:");
        /* GetStdHandle(-12) for stderr */
        codegen_emit(cg, "    sub  rsp, 40");    /* shadow space */
        codegen_emit(cg, "    mov  rcx, -12");
        codegen_emit(cg, "    extern GetStdHandle");
        codegen_emit(cg, "    call GetStdHandle");
        codegen_emit(cg, "    add  rsp, 40");
        codegen_emit(cg, "    mov  r12, rax");   /* stderr handle */
        /* WriteFile */
        codegen_emit(cg, "    sub  rsp, 40");
        codegen_emit(cg, "    mov  rcx, r12");
        codegen_emit(cg, "    mov  rdx, rbx");
        codegen_emit(cg, "    mov  r8,  r11");
        codegen_emit(cg, "    xor  r9,  r9");
        codegen_emit(cg, "    push 0");
        codegen_emit(cg, "    extern WriteFile");
        codegen_emit(cg, "    call WriteFile");
        codegen_emit(cg, "    add  rsp, 48");
        codegen_emit(cg, "    pop  rbx");
        /* ExitProcess(1) */
        codegen_emit(cg, "    sub  rsp, 40");
        codegen_emit(cg, "    mov  rcx, 1");
        codegen_emit(cg, "    extern ExitProcess");
        codegen_emit(cg, "    call ExitProcess");
        codegen_emit(cg, "    add  rsp, 40");
    } else {
        /* Linux: write(2, msg, strlen) then exit(1) */
        codegen_emit(cg, "    mov  rax, [rel _VL_try_depth_rt]");
        codegen_emit(cg, "    test rax, rax");
        codegen_emit(cg, "    jz   .panic_fatal");
        codegen_emit(cg, "    mov  [rel _VL_err_msg], rdi");
        codegen_emit(cg, "    mov  qword [rel _VL_err_flag], 1");
        codegen_emit(cg, "    pop  rbp");
        codegen_emit(cg, "    ret");
        codegen_emit(cg, ".panic_fatal:");
        codegen_emit(cg, "    mov  rbx, rdi");
        codegen_emit(cg, "    xor  rdx, rdx");
        codegen_emit(cg, ".panic_len:");
        codegen_emit(cg, "    cmp  byte [rbx + rdx], 0");
        codegen_emit(cg, "    je   .panic_write");
        codegen_emit(cg, "    inc  rdx");
        codegen_emit(cg, "    jmp  .panic_len");
        codegen_emit(cg, ".panic_write:");
        codegen_emit(cg, "    mov  rax, 1");
        codegen_emit(cg, "    mov  rdi, 2");
        codegen_emit(cg, "    mov  rsi, rbx");
        codegen_emit(cg, "    syscall");
        codegen_emit(cg, "    mov  rax, 60");
        codegen_emit(cg, "    mov  rdi, 1");
        codegen_emit(cg, "    syscall");
    }
    codegen_emit(cg, "    pop  rbp");
    codegen_emit(cg, "    ret");
    codegen_emit(cg, "");
}

/* ------------------------------------------------------------
 *  EXPRESSION CODEGEN
 * ------------------------------------------------------------ */
ValueType codegen_expression(CodeGen *cg, ASTNode *expr) {
    if (!expr) return TYPE_INT;
    switch (expr->type) {

    case AST_INTEGER:
        codegen_emit(cg,"    mov rax, %lld", (long long)expr->data.int_value);
        return TYPE_INT;

    case AST_BOOL:
        codegen_emit(cg,"    mov rax, %d", expr->data.bool_value ? 1 : 0);
        return TYPE_BOOL;

    case AST_NULL:
        codegen_emit(cg,"    xor rax, rax");
        return TYPE_NULL;

    case AST_FLOAT: {
        int fid = codegen_intern_float(cg, expr->data.float_lit.value,
                                       expr->data.float_lit.type);
        if (expr->data.float_lit.type == TYPE_F32) {
            codegen_emit(cg,"    movss xmm0, [rel _VL_f32_%d]", fid);
            return TYPE_F32;
        }
        codegen_emit(cg,"    movsd xmm0, [rel _VL_f64_%d]", fid);
        return TYPE_F64;
    }

    case AST_STRING: {
        int sid = codegen_intern_string(cg, expr->data.string_lit.value);
        codegen_emit(cg,"    lea rax, [rel _VL_str_%d]", sid);
        return TYPE_STRING;
    }

    case AST_IDENTIFIER: {
        int idx = codegen_find_var(cg, expr->data.identifier);
        if (idx < 0) error_at(expr->line, expr->column,
                               "undefined variable '%s'", expr->data.identifier);
        int off = cg->var_offsets[idx];
        ValueType t = cg->var_types[idx];
        TypeInfo *ti = cg->var_typeinfo[idx];
        /* Reference param: load the pointer, then dereference */
        if (ti && ti->is_ref && !is_composite(t)) {
            codegen_emit(cg,"    mov rax, [rbp - %d]", off);  /* load ptr */
            if (t == TYPE_F32) {
                codegen_emit(cg,"    movss xmm0, [rax]");
                return TYPE_F32;
            }
            if (t == TYPE_F64) {
                codegen_emit(cg,"    movsd xmm0, [rax]");
                return TYPE_F64;
            }
            codegen_emit(cg,"    mov rax, [rax]");             /* deref */
            return t;
        }
        if (is_composite(t)) {
            bool by_ref  = ti && (ti->by_ref || ti->is_ref);
            bool dyn_arr = (t == TYPE_ARRAY && ti && ti->array_len < 0);
            if (by_ref || dyn_arr)
                codegen_emit(cg,"    mov rax, [rbp - %d]", off);
            else
                codegen_emit(cg,"    lea rax, [rbp - %d]", off);
            return t;
        }
        if (t == TYPE_F32) { codegen_emit(cg,"    movss xmm0, [rbp - %d]", off); return TYPE_F32; }
        if (t == TYPE_F64) { codegen_emit(cg,"    movsd xmm0, [rbp - %d]", off); return TYPE_F64; }
        codegen_emit(cg,"    mov rax, [rbp - %d]", off);
        return t;
    }

    case AST_ARRAY_ACCESS: {
        ASTNode *base = expr->data.array_access.array;
        if (base->type != AST_IDENTIFIER)
            error_at(expr->line, expr->column, "array access requires identifier base");
        int idx = codegen_find_var(cg, base->data.identifier);
        if (idx < 0) error_at(expr->line, expr->column,
                               "undefined variable '%s'", base->data.identifier);
        if (cg->var_types[idx] != TYPE_ARRAY)
            error_at(expr->line, expr->column,
                     "'%s' is not an array", base->data.identifier);
        TypeInfo *ti = cg->var_typeinfo[idx];
        if (!ti) error_at(expr->line, expr->column, "array missing type info");

        ValueType it = codegen_expression(cg, expr->data.array_access.index);
        if (it != TYPE_INT) codegen_cast(cg, it, TYPE_INT);
        codegen_emit(cg,"    mov rcx, rax");

        if (ti->by_ref || ti->array_len < 0)
            codegen_emit(cg,"    mov rbx, [rbp - %d]", cg->var_offsets[idx]);
        else
            codegen_emit(cg,"    lea rbx, [rbp - %d]", cg->var_offsets[idx]);

        int elem_size = array_elem_size(cg, ti);
        ValueType elem = ti->elem_type;

        /* 2D: arr[i][j] */
        if (expr->data.array_access.index2) {
            /* first dim gives sub-array pointer */
            if (elem_size == 8)
                codegen_emit(cg,"    mov rbx, [rbx + rcx*8]");
            else {
                codegen_emit(cg,"    mov rdx, %d", elem_size);
                codegen_emit(cg,"    imul rcx, rdx");
                codegen_emit(cg,"    add rbx, rcx");
                codegen_emit(cg,"    mov rbx, [rbx]");
            }
            ValueType it2 = codegen_expression(cg, expr->data.array_access.index2);
            if (it2 != TYPE_INT) codegen_cast(cg, it2, TYPE_INT);
            codegen_emit(cg,"    mov rcx, rax");
            codegen_emit(cg,"    mov rax, [rbx + rcx*8]");
            return elem;
        }

        if (elem == TYPE_STRUCT) {
            if (elem_size == 8)
                codegen_emit(cg,"    lea rax, [rbx + rcx*8]");
            else {
                codegen_emit(cg,"    mov rdx, %d", elem_size);
                codegen_emit(cg,"    imul rcx, rdx");
                codegen_emit(cg,"    lea rax, [rbx + rcx]");
            }
            return TYPE_STRUCT;
        }
        if (elem == TYPE_UINT8) {
            codegen_emit(cg,"    movzx rax, byte [rbx + rcx]");
            return TYPE_UINT8;
        }
        if (elem_size == 8) {
            if (elem == TYPE_F32) { codegen_emit(cg,"    movss xmm0, [rbx + rcx*8]"); return TYPE_F32; }
            if (elem == TYPE_F64) { codegen_emit(cg,"    movsd xmm0, [rbx + rcx*8]"); return TYPE_F64; }
            codegen_emit(cg,"    mov rax, [rbx + rcx*8]");
        } else {
            codegen_emit(cg,"    mov rdx, %d", elem_size);
            codegen_emit(cg,"    imul rcx, rdx");
            codegen_emit(cg,"    add rbx, rcx");
            if (elem == TYPE_F32) { codegen_emit(cg,"    movss xmm0, [rbx]"); return TYPE_F32; }
            if (elem == TYPE_F64) { codegen_emit(cg,"    movsd xmm0, [rbx]"); return TYPE_F64; }
            codegen_emit(cg,"    mov rax, [rbx]");
        }
        return elem;
    }

    case AST_TUPLE_ACCESS: {
        ASTNode *base = expr->data.tuple_access.tuple;
        if (base->type != AST_IDENTIFIER)
            error_at(expr->line, expr->column, "tuple access requires identifier");
        int idx = codegen_find_var(cg, base->data.identifier);
        if (idx < 0) error_at(expr->line, expr->column,
                               "undefined variable '%s'", base->data.identifier);
        TypeInfo *ti = cg->var_typeinfo[idx];
        int tidx = expr->data.tuple_access.index;
        if (!ti || tidx < 0 || tidx >= ti->tuple_count)
            error_at(expr->line, expr->column, "tuple index out of range");
        ValueType elem = ti->tuple_types[tidx];
        int elem_off = cg->var_offsets[idx] - tidx * 8;
        if (elem == TYPE_F32) { codegen_emit(cg,"    movss xmm0, [rbp - %d]", elem_off); return TYPE_F32; }
        if (elem == TYPE_F64) { codegen_emit(cg,"    movsd xmm0, [rbp - %d]", elem_off); return TYPE_F64; }
        codegen_emit(cg,"    mov rax, [rbp - %d]", elem_off);
        return elem;
    }

    case AST_FIELD_ACCESS: {
        /* First check if base is an array element */
        ASTNode *base = expr->data.field_access.base;
        StructDef *sd = emit_struct_addr(cg, base);
        struct_size(cg, sd);
        int fidx = struct_field_index(sd, expr->data.field_access.field_name);
        if (fidx < 0) error_at(expr->line, expr->column,
                                "unknown field '%s' on '%s'",
                                expr->data.field_access.field_name, sd->name);
        ValueType ft = sd->field_types[fidx];
        int off = sd->field_offsets[fidx];
        codegen_emit(cg,"    mov rbx, rax");
        if (ft == TYPE_STRUCT) { codegen_emit(cg,"    add rax, %d", off); return TYPE_STRUCT; }
        if (ft == TYPE_ARRAY) {
            /* return pointer to array data */
            TypeInfo *fti = sd->field_typeinfo ? sd->field_typeinfo[fidx] : NULL;
            if (fti && fti->array_len < 0)
                codegen_emit(cg,"    mov rax, [rbx + %d]", off);
            else
                codegen_emit(cg,"    lea rax, [rbx + %d]", off);
            return TYPE_ARRAY;
        }
        if (ft == TYPE_F32) { codegen_emit(cg,"    movss xmm0, [rbx + %d]", off); return TYPE_F32; }
        if (ft == TYPE_F64) { codegen_emit(cg,"    movsd xmm0, [rbx + %d]", off); return TYPE_F64; }
        codegen_emit(cg,"    mov rax, [rbx + %d]", off);
        return ft;
    }

    case AST_UNARY_OP: {
        ValueType ot = codegen_expression(cg, expr->data.unary.operand);
        switch (expr->data.unary.op) {
        case UOP_BNOT:
            codegen_emit(cg,"    not rax");
            return ot;
        case UOP_NOT:
            if (is_float(ot)) codegen_cast(cg, ot, TYPE_INT);
            codegen_emit(cg,"    cmp rax, 0");
            codegen_emit(cg,"    sete al");
            codegen_emit(cg,"    movzx rax, al");
            return TYPE_BOOL;
        default: return ot;
        }
    }

    case AST_BINARY_OP: {
        BinaryOp op = expr->data.binary.op;

        /* Logical AND/OR with short-circuit */
        if (op == OP_AND) {
            int false_lbl = codegen_new_label(cg);
            int end_lbl   = codegen_new_label(cg);
            ValueType lt = codegen_expression(cg, expr->data.binary.left);
            if (is_float(lt)) codegen_cast(cg, lt, TYPE_INT);
            codegen_emit(cg,"    cmp rax, 0");
            codegen_emit(cg,"    je  _VL%d", false_lbl);
            ValueType rt = codegen_expression(cg, expr->data.binary.right);
            if (is_float(rt)) codegen_cast(cg, rt, TYPE_INT);
            codegen_emit(cg,"    cmp rax, 0");
            codegen_emit(cg,"    je  _VL%d", false_lbl);
            codegen_emit(cg,"    mov rax, 1");
            codegen_emit(cg,"    jmp _VL%d", end_lbl);
            codegen_emit(cg,"_VL%d:", false_lbl);
            codegen_emit(cg,"    xor rax, rax");
            codegen_emit(cg,"_VL%d:", end_lbl);
            return TYPE_BOOL;
        }
        if (op == OP_OR) {
            int true_lbl = codegen_new_label(cg);
            int end_lbl  = codegen_new_label(cg);
            ValueType lt = codegen_expression(cg, expr->data.binary.left);
            if (is_float(lt)) codegen_cast(cg, lt, TYPE_INT);
            codegen_emit(cg,"    cmp rax, 0");
            codegen_emit(cg,"    jne _VL%d", true_lbl);
            ValueType rt = codegen_expression(cg, expr->data.binary.right);
            if (is_float(rt)) codegen_cast(cg, rt, TYPE_INT);
            codegen_emit(cg,"    cmp rax, 0");
            codegen_emit(cg,"    jne _VL%d", true_lbl);
            codegen_emit(cg,"    xor rax, rax");
            codegen_emit(cg,"    jmp _VL%d", end_lbl);
            codegen_emit(cg,"_VL%d:", true_lbl);
            codegen_emit(cg,"    mov rax, 1");
            codegen_emit(cg,"_VL%d:", end_lbl);
            return TYPE_BOOL;
        }

        ValueType lt = codegen_expression(cg, expr->data.binary.left);
        if (is_float(lt)) {
            codegen_emit(cg,"    sub rsp, 8");
            if (lt==TYPE_F32) codegen_emit(cg,"    movss [rsp], xmm0");
            else              codegen_emit(cg,"    movsd [rsp], xmm0");
        } else {
            codegen_emit(cg,"    push rax");
        }
        ValueType rt = codegen_expression(cg, expr->data.binary.right);

        /* null comparison */
        if ((op==OP_EQ||op==OP_NE) &&
            ((is_optional(lt)&&rt==TYPE_NULL)||(lt==TYPE_NULL&&is_optional(rt))||
             (lt==TYPE_NULL&&rt==TYPE_NULL))) {
            unsigned long long sentinel = 0;
            ValueType opt = is_optional(lt) ? lt : rt;
            switch(opt) {
                case TYPE_OPT_STRING: sentinel=0; break;
                case TYPE_OPT_INT:    sentinel=0x8000000000000000ULL; break;
                case TYPE_OPT_BOOL:   sentinel=2; break;
                case TYPE_OPT_F64:    sentinel=0x7ff8000000000001ULL; break;
                case TYPE_OPT_F32:    sentinel=0x7fc00001ULL; break;
                default: sentinel=0; break;
            }
            if (lt==TYPE_NULL && rt==TYPE_NULL) {
                codegen_emit(cg,"    pop rbx");
                codegen_emit(cg,"    mov rax, %d", op==OP_EQ ? 1 : 0);
                return TYPE_BOOL;
            }
            codegen_emit(cg,"    pop rbx");
            if (is_optional(lt)) {
            codegen_emit(cg,"    mov rcx, 0x%llx", sentinel);
                codegen_emit(cg,"    cmp rbx, rcx");
            } else {
                codegen_emit(cg,"    mov rcx, 0x%llx", sentinel);
                codegen_emit(cg,"    cmp rax, rcx");
            }
            codegen_emit(cg,"    mov rax, 0");
            codegen_emit(cg, op==OP_EQ ? "    sete al" : "    setne al");
            return TYPE_BOOL;
        }

        /* Integer / bitwise path */
        if (!is_float(lt) && !is_float(rt)) {
            codegen_emit(cg,"    mov rbx, rax");
            codegen_emit(cg,"    pop rax");
            switch (op) {
            case OP_ADD:  codegen_emit(cg,"    add rax, rbx"); return TYPE_INT;
            case OP_SUB:  codegen_emit(cg,"    sub rax, rbx"); return TYPE_INT;
            case OP_MUL:  codegen_emit(cg,"    imul rax, rbx"); return TYPE_INT;
            case OP_DIV:
                codegen_emit(cg,"    cqo");
                codegen_emit(cg,"    idiv rbx");
                return TYPE_INT;
            case OP_MOD:
                codegen_emit(cg,"    cqo");
                codegen_emit(cg,"    idiv rbx");
                codegen_emit(cg,"    mov rax, rdx");
                return TYPE_INT;
            case OP_BAND: codegen_emit(cg,"    and rax, rbx"); return TYPE_INT;
            case OP_BOR:  codegen_emit(cg,"    or  rax, rbx"); return TYPE_INT;
            case OP_BXOR: codegen_emit(cg,"    xor rax, rbx"); return TYPE_INT;
            case OP_SHL:
                codegen_emit(cg,"    mov rcx, rbx");
                codegen_emit(cg,"    shl rax, cl");
                return TYPE_INT;
            case OP_SHR:
                codegen_emit(cg,"    mov rcx, rbx");
                codegen_emit(cg,"    sar rax, cl");
                return TYPE_INT;
            default:
                codegen_emit(cg,"    cmp rax, rbx");
                codegen_emit(cg,"    mov rax, 0");
                switch(op) {
                case OP_EQ: codegen_emit(cg,"    sete  al"); break;
                case OP_NE: codegen_emit(cg,"    setne al"); break;
                case OP_LT: codegen_emit(cg,"    setl  al"); break;
                case OP_LE: codegen_emit(cg,"    setle al"); break;
                case OP_GT: codegen_emit(cg,"    setg  al"); break;
                case OP_GE: codegen_emit(cg,"    setge al"); break;
                default: break;
                }
                return TYPE_BOOL;
            }
        }

        /* Float path */
        if (op == OP_MOD) error_at(expr->line, expr->column,
                                   "modulo (%) not supported for floats");
        ValueType resf = promote_float(lt, rt);
        if (rt == TYPE_INT) {
            if (resf==TYPE_F64) codegen_emit(cg,"    cvtsi2sd xmm0, rax");
            else                codegen_emit(cg,"    cvtsi2ss xmm0, rax");
        } else if (rt==TYPE_F32 && resf==TYPE_F64) {
            codegen_emit(cg,"    cvtss2sd xmm0, xmm0");
        }
        if (lt == TYPE_INT) {
            codegen_emit(cg,"    pop rbx");
            if (resf==TYPE_F64) codegen_emit(cg,"    cvtsi2sd xmm1, rbx");
            else                codegen_emit(cg,"    cvtsi2ss xmm1, rbx");
        } else if (lt == TYPE_F32) {
            codegen_emit(cg,"    movss xmm1, [rsp]");
            codegen_emit(cg,"    add rsp, 8");
            if (resf==TYPE_F64) codegen_emit(cg,"    cvtss2sd xmm1, xmm1");
        } else {
            codegen_emit(cg,"    movsd xmm1, [rsp]");
            codegen_emit(cg,"    add rsp, 8");
            if (resf==TYPE_F32) codegen_emit(cg,"    cvtsd2ss xmm1, xmm1");
        }
        if (op==OP_EQ||op==OP_NE||op==OP_LT||op==OP_LE||op==OP_GT||op==OP_GE) {
            if (resf==TYPE_F64) codegen_emit(cg,"    ucomisd xmm1, xmm0");
            else                codegen_emit(cg,"    ucomiss xmm1, xmm0");
            codegen_emit(cg,"    mov rax, 0");
            switch(op) {
            case OP_EQ: codegen_emit(cg,"    sete  al"); break;
            case OP_NE: codegen_emit(cg,"    setne al"); break;
            case OP_LT: codegen_emit(cg,"    setb  al"); break;
            case OP_LE: codegen_emit(cg,"    setbe al"); break;
            case OP_GT: codegen_emit(cg,"    seta  al"); break;
            case OP_GE: codegen_emit(cg,"    setae al"); break;
            default: break;
            }
            return TYPE_BOOL;
        }
        if (resf==TYPE_F64) {
            switch(op) {
            case OP_ADD: codegen_emit(cg,"    addsd xmm1, xmm0"); break;
            case OP_SUB: codegen_emit(cg,"    subsd xmm1, xmm0"); break;
            case OP_MUL: codegen_emit(cg,"    mulsd xmm1, xmm0"); break;
            case OP_DIV: codegen_emit(cg,"    divsd xmm1, xmm0"); break;
            default: break;
            }
            codegen_emit(cg,"    movapd xmm0, xmm1");
        } else {
            switch(op) {
            case OP_ADD: codegen_emit(cg,"    addss xmm1, xmm0"); break;
            case OP_SUB: codegen_emit(cg,"    subss xmm1, xmm0"); break;
            case OP_MUL: codegen_emit(cg,"    mulss xmm1, xmm0"); break;
            case OP_DIV: codegen_emit(cg,"    divss xmm1, xmm0"); break;
            default: break;
            }
            codegen_emit(cg,"    movaps xmm0, xmm1");
        }
        return resf;
    }

    case AST_ARRAY_LITERAL:
        error_at(expr->line, expr->column,
                 "array literals only valid in 'ath' initializers");
        return TYPE_ARRAY;

    case AST_STRUCT_LITERAL:
        error_at(expr->line, expr->column,
                 "struct literals only valid in 'ath' initializers or returns");
        return TYPE_STRUCT;

    case AST_CALL: {
        const char *call_name = expr->data.call.func_name;
        int argc = expr->data.call.arg_count;
        char mangled[MAX_TOKEN_LEN*2+4];
        const char *sig_name = call_name;

        /* len() */
        if (strcmp(call_name,"len")==0) {
            if (argc != 1)
                error_at(expr->line, expr->column, "len() expects 1 argument");
            ASTNode *arg = expr->data.call.args[0];
            ValueType at = codegen_expression(cg, arg);
            TypeInfo *ti = expr_typeinfo(cg, arg);
            if (at == TYPE_STRING || at == TYPE_OPT_STRING) {
                /* strlen via module if available, else inline */
                int ll=codegen_new_label(cg), dl=codegen_new_label(cg);
                if (at==TYPE_OPT_STRING) {
                    int nl=codegen_new_label(cg), el=codegen_new_label(cg);
                    codegen_emit(cg,"    test rax, rax");
                    codegen_emit(cg,"    jz   _VL%d", nl);
                    codegen_emit(cg,"    mov rbx, rax");
                    codegen_emit(cg,"    xor rcx, rcx");
                    codegen_emit(cg,"_VL%d:", ll);
                    codegen_emit(cg,"    mov dl, [rbx + rcx]");
                    codegen_emit(cg,"    test dl, dl");
                    codegen_emit(cg,"    jz  _VL%d", dl);
                    codegen_emit(cg,"    inc rcx");
                    codegen_emit(cg,"    jmp _VL%d", ll);
                    codegen_emit(cg,"_VL%d:", dl);
                    codegen_emit(cg,"    mov rax, rcx");
                    codegen_emit(cg,"    jmp _VL%d", el);
                    codegen_emit(cg,"_VL%d:", nl);
                    codegen_emit(cg,"    xor rax, rax");
                    codegen_emit(cg,"_VL%d:", el);
                } else {
                    codegen_emit(cg,"    mov rbx, rax");
                    codegen_emit(cg,"    xor rcx, rcx");
                    codegen_emit(cg,"_VL%d:", ll);
                    codegen_emit(cg,"    mov dl, [rbx + rcx]");
                    codegen_emit(cg,"    test dl, dl");
                    codegen_emit(cg,"    jz  _VL%d", dl);
                    codegen_emit(cg,"    inc rcx");
                    codegen_emit(cg,"    jmp _VL%d", ll);
                    codegen_emit(cg,"_VL%d:", dl);
                    codegen_emit(cg,"    mov rax, rcx");
                }
                return TYPE_INT;
            }
            if (at == TYPE_ARRAY) {
                if (ti && ti->array_len >= 0) {
                    codegen_emit(cg,"    mov rax, %d", ti->array_len);
                } else {
                    /* dynamic: length in [ptr - 8] */
                    codegen_emit(cg,"    mov rbx, rax");
                    codegen_emit(cg,"    mov rax, [rbx - 8]");
                }
                return TYPE_INT;
            }
            if (at == TYPE_TUPLE) {
                if (!ti) error_at(expr->line, expr->column,
                                  "len() requires tuple type info");
                codegen_emit(cg,"    mov rax, %d", ti->tuple_count);
                return TYPE_INT;
            }
            error_at(expr->line, expr->column,
                     "len() not supported for type '%s'", value_type_name(at));
        }

        /* sizeof() */
        if (strcmp(call_name,"sizeof")==0) {
            if (argc!=1)
                error_at(expr->line, expr->column, "sizeof() expects 1 argument");
            ASTNode *arg = expr->data.call.args[0];
            ValueType at = infer_expr_type(cg, arg);
            TypeInfo *ti = expr_typeinfo(cg, arg);
            int sz = typeinfo_size(cg, at, ti);
            codegen_emit(cg,"    mov rax, %d", sz);
            return TYPE_INT;
        }

        /* type() */
        if (strcmp(call_name,"type")==0) {
            if (argc!=1)
                error_at(expr->line, expr->column, "type() expects 1 argument");
            ASTNode *arg = expr->data.call.args[0];
            ValueType at = infer_expr_type(cg, arg);
            codegen_expression(cg, arg);
            int sid = codegen_intern_string(cg, value_type_name(at));
            codegen_emit(cg,"    lea rax, [rel _VL_str_%d]", sid);
            return TYPE_STRING;
        }

        /* map() / filter() -- delegate to old logic inline */
        if (strcmp(call_name,"map")==0 || strcmp(call_name,"filter")==0) {
            bool is_filter = (strcmp(call_name,"filter")==0);
            if (argc!=2)
                error_at(expr->line, expr->column,
                         "%s() expects 2 arguments", call_name);
            ASTNode *arr_expr = expr->data.call.args[0];
            ASTNode *lam_expr = expr->data.call.args[1];
            if (!lam_expr || lam_expr->type != AST_LAMBDA)
                error_at(expr->line, expr->column,
                         "%s() expects lambda as second argument", call_name);
            TypeInfo *arr_ti = expr_typeinfo(cg, arr_expr);
            if (!arr_ti)
                error_at(expr->line, expr->column,
                         "%s() requires typed array", call_name);
            ValueType elem_t = arr_ti->elem_type;

            int saved_count  = cg->var_count;
            int saved_offset = cg->stack_offset;

            codegen_add_var(cg, lam_expr->data.lambda.param_name, elem_t, true, NULL);
            ValueType ret_t = infer_expr_type(cg, lam_expr->data.lambda.body);
            TypeInfo *call_ti = (TypeInfo*)calloc(1, sizeof(TypeInfo));
            call_ti->kind = TYPE_ARRAY;
            call_ti->array_len = -1;
            call_ti->is_dynamic = true;
            call_ti->elem_type = is_filter ? elem_t : ret_t;
            if (is_filter && arr_ti->elem_typeinfo)
                call_ti->elem_typeinfo = typeinfo_clone(arr_ti->elem_typeinfo);

            codegen_expression(cg, arr_expr);

            char in_n[32], out_n[32], len_n[32], idx_n[32], outi_n[32];
            int lb = codegen_new_label(cg);
            snprintf(in_n,  32, "_mi%d", lb);
            snprintf(out_n, 32, "_mo%d", lb);
            snprintf(len_n, 32, "_ml%d", lb);
            snprintf(idx_n, 32, "_mx%d", lb);
            snprintf(outi_n,32, "_mu%d", lb);

            codegen_add_var(cg, in_n,  TYPE_INT, true, NULL);
            int in_off = cg->var_offsets[cg->var_count-1];
            codegen_emit(cg,"    mov [rbp - %d], rax", in_off);

            codegen_add_var(cg, len_n, TYPE_INT, true, NULL);
            int len_off = cg->var_offsets[cg->var_count-1];
            if (arr_ti->array_len >= 0)
                codegen_emit(cg,"    mov qword [rbp - %d], %d", len_off, arr_ti->array_len);
            else {
                codegen_emit(cg,"    mov rbx, [rbp - %d]", in_off);
                codegen_emit(cg,"    mov rax, [rbx - 8]");
                codegen_emit(cg,"    mov [rbp - %d], rax", len_off);
            }

            cg->needs_arr_alloc = true;
            codegen_emit(cg,"    mov %s, [rbp - %d]", arg_reg(cg,0), len_off);
            codegen_emit(cg,"    mov %s, 8", arg_reg(cg,1));
            codegen_emit(cg,"    call __vl_arr_alloc");
            codegen_add_var(cg, out_n, TYPE_INT, true, NULL);
            int out_off = cg->var_offsets[cg->var_count-1];
            codegen_emit(cg,"    mov [rbp - %d], rax", out_off);

            codegen_add_var(cg, idx_n, TYPE_INT, true, NULL);
            int idx_off = cg->var_offsets[cg->var_count-1];
            codegen_emit(cg,"    mov qword [rbp - %d], 0", idx_off);

            int outi_off = 0;
            if (is_filter) {
                codegen_add_var(cg, outi_n, TYPE_INT, true, NULL);
                outi_off = cg->var_offsets[cg->var_count-1];
                codegen_emit(cg,"    mov qword [rbp - %d], 0", outi_off);
            }

            int loop_l=codegen_new_label(cg), end_l=codegen_new_label(cg);
            int skip_l=codegen_new_label(cg);
            codegen_emit(cg,"_VL%d:", loop_l);
            codegen_emit(cg,"    mov rax, [rbp - %d]", idx_off);
            codegen_emit(cg,"    cmp rax, [rbp - %d]", len_off);
            codegen_emit(cg,"    jge _VL%d", end_l);
            codegen_emit(cg,"    mov rcx, [rbp - %d]", idx_off);
            codegen_emit(cg,"    mov rbx, [rbp - %d]", in_off);
            if (elem_t==TYPE_F32) {
                codegen_emit(cg,"    movss xmm0, [rbx + rcx*8]");
                codegen_emit(cg,"    movss [rbp - %d], xmm0", cg->var_offsets[saved_count]);
            } else if (elem_t==TYPE_F64) {
                codegen_emit(cg,"    movsd xmm0, [rbx + rcx*8]");
                codegen_emit(cg,"    movsd [rbp - %d], xmm0", cg->var_offsets[saved_count]);
            } else {
                codegen_emit(cg,"    mov rax, [rbx + rcx*8]");
                codegen_emit(cg,"    mov [rbp - %d], rax", cg->var_offsets[saved_count]);
            }
            ValueType body_t = codegen_expression(cg, lam_expr->data.lambda.body);
            if (is_filter) {
                if (body_t != TYPE_BOOL) error("filter() lambda must return bool");
                codegen_emit(cg,"    cmp rax, 0");
                codegen_emit(cg,"    je  _VL%d", skip_l);
                codegen_emit(cg,"    mov rcx, [rbp - %d]", idx_off);
                codegen_emit(cg,"    mov rbx, [rbp - %d]", in_off);
                codegen_emit(cg,"    mov rax, [rbx + rcx*8]");
                codegen_emit(cg,"    mov rcx, [rbp - %d]", outi_off);
                codegen_emit(cg,"    mov rbx, [rbp - %d]", out_off);
                codegen_emit(cg,"    mov [rbx + rcx*8], rax");
                codegen_emit(cg,"    add qword [rbp - %d], 1", outi_off);
                codegen_emit(cg,"_VL%d:", skip_l);
            } else {
                if (body_t != ret_t) codegen_cast(cg, body_t, ret_t);
                codegen_emit(cg,"    mov rcx, [rbp - %d]", idx_off);
                codegen_emit(cg,"    mov rbx, [rbp - %d]", out_off);
                if (ret_t==TYPE_F32) codegen_emit(cg,"    movss [rbx + rcx*8], xmm0");
                else if (ret_t==TYPE_F64) codegen_emit(cg,"    movsd [rbx + rcx*8], xmm0");
                else codegen_emit(cg,"    mov [rbx + rcx*8], rax");
            }
            codegen_emit(cg,"    add qword [rbp - %d], 1", idx_off);
            codegen_emit(cg,"    jmp _VL%d", loop_l);
            codegen_emit(cg,"_VL%d:", end_l);
            if (is_filter) {
                codegen_emit(cg,"    mov rax, [rbp - %d]", out_off);
                codegen_emit(cg,"    mov rcx, [rbp - %d]", outi_off);
                codegen_emit(cg,"    mov [rax - 8], rcx");
            }
            codegen_emit(cg,"    mov rax, [rbp - %d]", out_off);
            for (int i = saved_count; i < cg->var_count; i++)
                free(cg->local_vars[i]);
            cg->var_count    = saved_count;
            cg->stack_offset = saved_offset;
            expr->data.call.return_typeinfo = call_ti;
            return TYPE_ARRAY;
        }

        /* Mangle for current module */
        if (cg->current_module[0] && mod_func_table_has(call_name)) {
            make_module_symbol(mangled, sizeof(mangled),
                               cg->current_module, call_name);
            sig_name = mangled;
        }

        FunctionSig *sig = func_sig_find(cg, sig_name);
        bool ret_sret = sig && (sig->return_type==TYPE_TUPLE ||
                                sig->return_type==TYPE_STRUCT);
        int sret_off = 0;
        if (ret_sret) {
            char tmpn[32]; snprintf(tmpn,32,"_tmp%d",codegen_new_label(cg));
            TypeInfo *ti = typeinfo_clone(sig->return_typeinfo);
            codegen_add_var(cg, tmpn, sig->return_type, true, ti);
            sret_off = cg->var_offsets[cg->var_count-1];
        }
        for (int i = argc-1; i >= 0; i--) {
            /* Check if this param is a reference param - pass address instead */
            bool is_ref_param = sig && sig->param_is_ref
                                && i < sig->param_count && sig->param_is_ref[i];
            if (is_ref_param && expr->data.call.args[i]->type == AST_IDENTIFIER) {
                /* Pass &var: compute address of the local variable */
                int vidx = codegen_find_var(cg,
                               expr->data.call.args[i]->data.identifier);
                if (vidx < 0)
                    error_at(expr->line, expr->column,
                             "undefined variable '%s' passed as reference",
                             expr->data.call.args[i]->data.identifier);
                TypeInfo *vti = cg->var_typeinfo[vidx];
                /* If var is itself a ref param, pass its stored pointer through */
                if (vti && vti->is_ref)
                    codegen_emit(cg,"    mov rax, [rbp - %d]", cg->var_offsets[vidx]);
                else
                    codegen_emit(cg,"    lea rax, [rbp - %d]", cg->var_offsets[vidx]);
            } else {
                ValueType at = codegen_expression(cg, expr->data.call.args[i]);
                ValueType ex = sig && i < sig->param_count ? sig->param_types[i] : at;
                validate_unsigned_literal_range(expr->data.call.args[i], ex, expr->line, expr->column);
                if (at != ex) { codegen_cast(cg, at, ex); at = ex; }
                if (is_float(at)) {
                    if (at == TYPE_F32) codegen_emit(cg,"    cvtss2sd xmm0, xmm0");
                    codegen_emit(cg,"    movq rax, xmm0");
                }
            }
            codegen_emit(cg,"    push rax");
        }
        if (ret_sret) {
            codegen_emit(cg,"    lea rax, [rbp - %d]", sret_off);
            codegen_emit(cg,"    push rax");
        }
        for (int i = 0; i < argc + (ret_sret?1:0); i++)
            codegen_emit(cg,"    pop %s", arg_reg(cg, i));
        if (cg->target_windows)
            codegen_emit(cg,"    sub rsp, 32");  /* shadow space */
        if (cg->current_module[0] && mod_func_table_has(call_name)) {
            make_module_symbol(mangled, sizeof(mangled),
                               cg->current_module, call_name);
            codegen_emit(cg,"    call %s", mangled);
        } else {
            codegen_emit(cg,"    call %s", call_name);
        }
        if (cg->target_windows)
            codegen_emit(cg,"    add rsp, 32");
        emit_try_error_probe(cg);
        if (ret_sret) {
            codegen_emit(cg,"    lea rax, [rbp - %d]", sret_off);
            return sig->return_type;
        }
        return sig ? sig->return_type : TYPE_INT;
    }

    case AST_MODULE_CALL: {
        const char *mname = expr->data.module_call.module_name;
        const char *fname = expr->data.module_call.func_name;
        int argc = expr->data.module_call.arg_count;
        char full[MAX_TOKEN_LEN*2+4];
        snprintf(full, sizeof(full), "%s__%s", mname, fname);

        /* system.argv() -> build array from argc/argv */
        if (strcmp(mname,"system")==0 && strcmp(fname,"argv")==0 && argc==0) {
            cg->needs_arr_alloc = true;
            int lb = codegen_new_label(cg);
            codegen_emit(cg,"    mov %s, [rel _VL_argc]", arg_reg(cg,0));
            codegen_emit(cg,"    mov %s, 8", arg_reg(cg,1));
            if (cg->target_windows) codegen_emit(cg,"    sub rsp, 32");
            codegen_emit(cg,"    call __vl_arr_alloc");
            if (cg->target_windows) codegen_emit(cg,"    add rsp, 32");
            codegen_emit(cg,"    mov r10, rax");
            codegen_emit(cg,"    mov rcx, [rel _VL_argc]");
            codegen_emit(cg,"    mov rdx, [rel _VL_argv]");
            codegen_emit(cg,"    xor r8, r8");
            codegen_emit(cg,"_VLargv_lp%d:", lb);
            codegen_emit(cg,"    cmp r8, rcx");
            codegen_emit(cg,"    jae _VLargv_dn%d", lb);
            codegen_emit(cg,"    mov r11, [rdx + r8*8]");
            codegen_emit(cg,"    mov [r10 + r8*8], r11");
            codegen_emit(cg,"    inc r8");
            codegen_emit(cg,"    jmp _VLargv_lp%d", lb);
            codegen_emit(cg,"_VLargv_dn%d:", lb);
            codegen_emit(cg,"    mov rax, r10");
            return TYPE_ARRAY;
        }

        /* io.chhaap(x) auto-format */
        if (strcmp(mname,"io")==0 && strcmp(fname,"chhaap")==0 && argc==1 &&
            expr->data.module_call.args[0]->type != AST_STRING) {
            ValueType at = codegen_expression(cg, expr->data.module_call.args[0]);
            const char *fmt = "%d";
            if (at==TYPE_STRING||at==TYPE_OPT_STRING) fmt="%s";
            else if (at==TYPE_BOOL) fmt="%b";
            else if (is_float(at)) fmt="%f";
            if (is_float(at)) {
                if (at==TYPE_F32) codegen_emit(cg,"    cvtss2sd xmm0, xmm0");
                codegen_emit(cg,"    movq rax, xmm0");
            }
            codegen_emit(cg,"    push rax");
            int sid = codegen_intern_string(cg, fmt);
            codegen_emit(cg,"    lea rax, [rel _VL_str_%d]", sid);
            codegen_emit(cg,"    push rax");
            codegen_emit(cg,"    pop %s", arg_reg(cg,0));
            codegen_emit(cg,"    pop %s", arg_reg(cg,1));
            if (cg->target_windows) codegen_emit(cg,"    sub rsp, 32");
            codegen_emit(cg,"    call io__chhaap");
            if (cg->target_windows) codegen_emit(cg,"    add rsp, 32");
            return TYPE_INT;
        }

        /* io.chhaapLine(x) auto-format: print value then newline */
        if (strcmp(mname,"io")==0 && strcmp(fname,"chhaapLine")==0 && argc==1 &&
            expr->data.module_call.args[0]->type != AST_STRING) {
            ValueType at = codegen_expression(cg, expr->data.module_call.args[0]);
            const char *fmt = "%d\n";
            if (at==TYPE_STRING||at==TYPE_OPT_STRING) fmt="%s\n";
            else if (at==TYPE_BOOL) fmt="%b\n";
            else if (is_float(at)) fmt="%f\n";
            if (is_float(at)) {
                if (at==TYPE_F32) codegen_emit(cg,"    cvtss2sd xmm0, xmm0");
                codegen_emit(cg,"    movq rax, xmm0");
            }
            codegen_emit(cg,"    push rax");
            int sid = codegen_intern_string(cg, fmt);
            codegen_emit(cg,"    lea rax, [rel _VL_str_%d]", sid);
            codegen_emit(cg,"    push rax");
            codegen_emit(cg,"    pop %s", arg_reg(cg,0));
            codegen_emit(cg,"    pop %s", arg_reg(cg,1));
            if (cg->target_windows) codegen_emit(cg,"    sub rsp, 32");
            codegen_emit(cg,"    call io__chhaap");
            if (cg->target_windows) codegen_emit(cg,"    add rsp, 32");
            return TYPE_INT;
        }

        FunctionSig *sig = func_sig_find(cg, full);
        bool ret_sret = sig && (sig->return_type==TYPE_TUPLE ||
                                sig->return_type==TYPE_STRUCT);
        int sret_off = 0;
        if (ret_sret) {
            char tmpn[32]; snprintf(tmpn,32,"_tmp%d",codegen_new_label(cg));
            TypeInfo *ti = typeinfo_clone(sig->return_typeinfo);
            codegen_add_var(cg, tmpn, sig->return_type, true, ti);
            sret_off = cg->var_offsets[cg->var_count-1];
        }
        for (int i = argc-1; i >= 0; i--) {
            ValueType at = codegen_expression(cg, expr->data.module_call.args[i]);
            ValueType ex = sig && i < sig->param_count ? sig->param_types[i] : at;
            validate_unsigned_literal_range(expr->data.module_call.args[i], ex,
                                            expr->line, expr->column);
            if (at != ex) { codegen_cast(cg, at, ex); at = ex; }
            if (is_float(at)) {
                if (at == TYPE_F32) codegen_emit(cg,"    cvtss2sd xmm0, xmm0");
                codegen_emit(cg,"    movq rax, xmm0");
            }
            codegen_emit(cg,"    push rax");
        }
        if (ret_sret) {
            codegen_emit(cg,"    lea rax, [rbp - %d]", sret_off);
            codegen_emit(cg,"    push rax");
        }
        for (int i = 0; i < argc + (ret_sret?1:0); i++)
            codegen_emit(cg,"    pop %s", arg_reg(cg,i));
        if (cg->target_windows) codegen_emit(cg,"    sub rsp, 32");
        codegen_emit(cg,"    call %s", full);
        if (cg->target_windows) codegen_emit(cg,"    add rsp, 32");
        emit_try_error_probe(cg);
        if (ret_sret) {
            codegen_emit(cg,"    lea rax, [rbp - %d]", sret_off);
            return sig->return_type;
        }
        return sig ? sig->return_type : TYPE_INT;
    }

    default:
        error_at(expr->line, expr->column,
                 "codegen_expression: unhandled node type %d", expr->type);
        return TYPE_INT;
    }
}

/* ------------------------------------------------------------
 *  STATEMENT CODEGEN
 * ------------------------------------------------------------ */
void codegen_statement(CodeGen *cg, ASTNode *stmt) {
    if (!stmt) return;
    switch (stmt->type) {

    /* -- return -- */
    case AST_RETURN:
        if (stmt->data.ret.value) {
            if (cg->current_return_type == TYPE_STRUCT) {
                StructDef *sd = struct_find(cg, cg->current_return_typeinfo
                                            ? cg->current_return_typeinfo->struct_name : "");
                codegen_emit(cg,"    mov rbx, [rbp - %d]", cg->sret_offset);
                if (stmt->data.ret.value->type == AST_STRUCT_LITERAL) {
                    codegen_emit_struct_literal(cg, sd, stmt->data.ret.value);
                } else {
                    ValueType rt = codegen_expression(cg, stmt->data.ret.value);
                    if (rt == TYPE_STRUCT) {
                        codegen_emit(cg,"    mov rcx, rax");
                        codegen_copy_struct(cg, sd, "rbx", "rcx");
                    }
                }
                codegen_emit(cg,"    mov rax, rbx");
            } else if (cg->current_return_type == TYPE_TUPLE) {
                /* sret: write each tuple element through the hidden pointer */
                codegen_emit(cg,"    mov rbx, [rbp - %d]", cg->sret_offset);
                ASTNode *rv = stmt->data.ret.value;
                TypeInfo *rti = cg->current_return_typeinfo;
                if (rv->type == AST_TUPLE_LITERAL) {
                    int count = rv->data.tuple_lit.count;
                    for (int i = 0; i < count; i++) {
                        ValueType et = codegen_expression(cg, rv->data.tuple_lit.elements[i]);
                        if (et==TYPE_F32) { codegen_emit(cg,"    movss [rbx + %d], xmm0", i*8); }
                        else if (et==TYPE_F64) { codegen_emit(cg,"    movsd [rbx + %d], xmm0", i*8); }
                        else { codegen_emit(cg,"    mov [rbx + %d], rax", i*8); }
                        /* rbx may be clobbered if expression uses rbx - reload */
                        codegen_emit(cg,"    mov rbx, [rbp - %d]", cg->sret_offset);
                    }
                } else {
                    /* Expression yields a tuple pointer in rax — memcopy */
                    ValueType rt = codegen_expression(cg, rv);
                    int n = (rti && rti->tuple_count > 0) ? rti->tuple_count : 2;
                    codegen_emit(cg,"    mov rcx, rax");
                    for (int i = 0; i < n; i++) {
                        codegen_emit(cg,"    mov rax, [rcx + %d]", i*8);
                        codegen_emit(cg,"    mov [rbx + %d], rax", i*8);
                    }
                }
                codegen_emit(cg,"    mov rax, rbx");
            } else {
                ValueType rt = codegen_expression(cg, stmt->data.ret.value);
                validate_unsigned_literal_range(stmt->data.ret.value,
                                                cg->current_return_type,
                                                stmt->line, stmt->column);
                if (rt != cg->current_return_type &&
                    cg->current_return_type != TYPE_VOID)
                    codegen_cast(cg, rt, cg->current_return_type);
            }
        } else {
            codegen_emit(cg,"    xor rax, rax");
        }
        codegen_emit(cg,"    mov rsp, rbp");
        codegen_emit(cg,"    pop rbp");
        codegen_emit(cg,"    ret");
        break;

    /* -- let -- */
    case AST_LET: {
        ValueType decl = stmt->data.let.has_type ? stmt->data.let.var_type : TYPE_INT;
        ASTNode *val   = stmt->data.let.value;

        if (!stmt->data.let.has_type && val->type == AST_TUPLE_LITERAL) {
            int count = val->data.tuple_lit.count;
            TypeInfo *ti = (TypeInfo*)calloc(1, sizeof(TypeInfo));
            ti->kind = TYPE_TUPLE;
            ti->tuple_count = count;
            if (count > 0) {
                ti->tuple_types = (ValueType*)malloc(sizeof(ValueType) * count);
                for (int i = 0; i < count; i++)
                    ti->tuple_types[i] = infer_expr_type(cg, val->data.tuple_lit.elements[i]);
            }
            stmt->data.let.type_info = ti;
            stmt->data.let.var_type  = TYPE_TUPLE;
            stmt->data.let.has_type  = true;
            decl = TYPE_TUPLE;
        }

        /* Auto-detect struct type */
        if (!stmt->data.let.has_type && val->type == AST_STRUCT_LITERAL) {
            TypeInfo *ti = (TypeInfo*)calloc(1, sizeof(TypeInfo));
            ti->kind = TYPE_STRUCT;
            strncpy(ti->struct_name, val->data.struct_lit.struct_name, MAX_TOKEN_LEN-1);
            stmt->data.let.type_info = ti;
            stmt->data.let.var_type  = TYPE_STRUCT;
            stmt->data.let.has_type  = true;
            decl = TYPE_STRUCT;
        }

        if (decl == TYPE_ARRAY) {
            TypeInfo *ti = stmt->data.let.type_info;
            if (!ti) error_at(stmt->line, stmt->column,
                              "array declaration needs type annotation");
            codegen_add_var(cg, stmt->data.let.var_name, TYPE_ARRAY,
                            stmt->data.let.is_mut, ti);
            int idx = cg->var_count - 1;
            int off = cg->var_offsets[idx];

            if (val->type == AST_ARRAY_LITERAL) {
                int count = val->data.array_lit.count;
                int elem_size = array_elem_size(cg, ti);

                if (ti->array_len < 0 || ti->is_dynamic) {
                    /* dynamic: allocate from heap */
                    cg->needs_arr_alloc = true;
                    codegen_emit(cg,"    mov %s, %d", arg_reg(cg,0), count > 0 ? count : 1);
                    codegen_emit(cg,"    mov %s, %d", arg_reg(cg,1), elem_size);
                    if (cg->target_windows) codegen_emit(cg,"    sub rsp, 32");
                    codegen_emit(cg,"    call __vl_arr_alloc");
                    if (cg->target_windows) codegen_emit(cg,"    add rsp, 32");
                    /* update the length to the actual count */
                    if (count > 0)
                        codegen_emit(cg,"    mov qword [rax - 8], %d", count);
                    else
                        codegen_emit(cg,"    mov qword [rax - 8], 0");
                    codegen_emit(cg,"    mov [rbp - %d], rax", off);
                    for (int i = 0; i < count; i++) {
                        ValueType at = codegen_expression(cg, val->data.array_lit.elements[i]);
                        ValueType ex = ti->elem_type;
                        validate_unsigned_literal_range(val->data.array_lit.elements[i], ex,
                                                        stmt->line, stmt->column);
                        if (at != ex) codegen_cast(cg, at, ex);
                        codegen_emit(cg,"    mov rbx, [rbp - %d]", off);
                        if (ex == TYPE_F32) codegen_emit(cg,"    movss [rbx + %d], xmm0", i*elem_size);
                        else if (ex == TYPE_F64) codegen_emit(cg,"    movsd [rbx + %d], xmm0", i*elem_size);
                        else codegen_emit(cg,"    mov [rbx + %d], rax", i*elem_size);
                    }
                } else {
                    /* fixed-size: inline on stack */
                    if (count != ti->array_len)
                        error_at(stmt->line, stmt->column,
                                 "array literal has %d elements but type says %d",
                                 count, ti->array_len);
                    for (int i = 0; i < count; i++) {
                        ValueType at = codegen_expression(cg, val->data.array_lit.elements[i]);
                        ValueType ex = ti->elem_type;
                        validate_unsigned_literal_range(val->data.array_lit.elements[i], ex,
                                                        stmt->line, stmt->column);
                        if (at != ex) codegen_cast(cg, at, ex);
                        int elem_off = off - i * elem_size;
                        if (ex == TYPE_F32) codegen_emit(cg,"    movss [rbp - %d], xmm0", elem_off);
                        else if (ex == TYPE_F64) codegen_emit(cg,"    movsd [rbp - %d], xmm0", elem_off);
                        else codegen_emit(cg,"    mov [rbp - %d], rax", elem_off);
                    }
                }
            } else {
                /* initializer is an expression (function call returning array) */
                ValueType vt = codegen_expression(cg, val);
                if (vt == TYPE_ARRAY)
                    codegen_emit(cg,"    mov [rbp - %d], rax", off);
            }
            break;
        }

        if (decl == TYPE_STRUCT) {
            TypeInfo *ti = stmt->data.let.type_info;
            if (!ti) error_at(stmt->line, stmt->column, "struct declaration needs type info");
            codegen_add_var(cg, stmt->data.let.var_name, TYPE_STRUCT,
                            stmt->data.let.is_mut, ti);
            int idx = cg->var_count - 1;
            int off = cg->var_offsets[idx];
            StructDef *sd = struct_find(cg, ti->struct_name);
            if (!sd) error_at(stmt->line, stmt->column,
                              "unknown struct '%s'", ti->struct_name);
            if (val->type == AST_STRUCT_LITERAL) {
                codegen_emit(cg,"    lea rbx, [rbp - %d]", off);
                codegen_emit_struct_literal(cg, sd, val);
            } else {
                ValueType vt = codegen_expression(cg, val);
                if (vt == TYPE_STRUCT) {
                    codegen_emit(cg,"    lea rbx, [rbp - %d]", off);
                    codegen_emit(cg,"    mov rcx, rax");
                    codegen_copy_struct(cg, sd, "rbx", "rcx");
                }
            }
            break;
        }

        /* Auto-detect tuple type from function call return */
        if (!stmt->data.let.has_type &&
            (val->type == AST_CALL || val->type == AST_MODULE_CALL)) {
            FunctionSig *sig2 = NULL;
            if (val->type == AST_CALL)
                sig2 = func_sig_find(cg, val->data.call.func_name);
            else {
                char sn2[MAX_TOKEN_LEN*2+4];
                snprintf(sn2, sizeof(sn2), "%s__%s",
                         val->data.module_call.module_name,
                         val->data.module_call.func_name);
                sig2 = func_sig_find(cg, sn2);
            }
            if (sig2 && sig2->return_type == TYPE_TUPLE) {
                stmt->data.let.var_type  = TYPE_TUPLE;
                stmt->data.let.has_type  = true;
                stmt->data.let.type_info = typeinfo_clone(sig2->return_typeinfo);
                decl = TYPE_TUPLE;
            }
        }

        if (decl == TYPE_TUPLE) {
            TypeInfo *ti = stmt->data.let.type_info;
            if (!ti) error_at(stmt->line, stmt->column, "tuple needs type info");
            codegen_add_var(cg, stmt->data.let.var_name, TYPE_TUPLE,
                            stmt->data.let.is_mut, ti);
            int off = cg->var_offsets[cg->var_count-1];
            if (val->type == AST_TUPLE_LITERAL) {
                for (int i = 0; i < val->data.tuple_lit.count; i++) {
                    ValueType at = codegen_expression(cg, val->data.tuple_lit.elements[i]);
                    ValueType ex = (ti && i < ti->tuple_count) ? ti->tuple_types[i] : at;
                    if (at != ex) codegen_cast(cg, at, ex);
                    int elem_off = off - i*8;
                    if (ex==TYPE_F32) codegen_emit(cg,"    movss [rbp - %d], xmm0", elem_off);
                    else if (ex==TYPE_F64) codegen_emit(cg,"    movsd [rbp - %d], xmm0", elem_off);
                    else codegen_emit(cg,"    mov [rbp - %d], rax", elem_off);
                }
            } else {
                /* Call returns sret pointer in rax — copy each slot into our local */
                codegen_expression(cg, val);
                int n = ti ? ti->tuple_count : 0;
                codegen_emit(cg,"    mov rcx, rax");
                for (int i = 0; i < n; i++) {
                    ValueType ex = ti->tuple_types[i];
                    int elem_off = off - i*8;
                    if (ex==TYPE_F32) {
                        codegen_emit(cg,"    movss xmm0, [rcx + %d]", i*8);
                        codegen_emit(cg,"    movss [rbp - %d], xmm0", elem_off);
                    } else if (ex==TYPE_F64) {
                        codegen_emit(cg,"    movsd xmm0, [rcx + %d]", i*8);
                        codegen_emit(cg,"    movsd [rbp - %d], xmm0", elem_off);
                    } else {
                        codegen_emit(cg,"    mov rax, [rcx + %d]", i*8);
                        codegen_emit(cg,"    mov [rbp - %d], rax", elem_off);
                    }
                }
            }
            break;
        }

        /* Scalar let */
        ValueType vt = codegen_expression(cg, val);
        if (!stmt->data.let.has_type) decl = vt;
        validate_unsigned_literal_range(val, decl, stmt->line, stmt->column);
        if (vt != decl) codegen_cast(cg, vt, decl);
        if (decl == TYPE_ARRAY && !stmt->data.let.type_info) {
            TypeInfo *expr_ti = NULL;
            if (val->type == AST_CALL)
                expr_ti = val->data.call.return_typeinfo;
            else if (val->type == AST_MODULE_CALL)
                expr_ti = val->data.module_call.return_typeinfo;
            if (expr_ti)
                stmt->data.let.type_info = typeinfo_clone(expr_ti);
        }
        codegen_add_var(cg, stmt->data.let.var_name, decl,
                        stmt->data.let.is_mut, stmt->data.let.type_info);
        if (decl==TYPE_F32) codegen_emit(cg,"    movss [rbp - %d], xmm0", cg->stack_offset);
        else if (decl==TYPE_F64) codegen_emit(cg,"    movsd [rbp - %d], xmm0", cg->stack_offset);
        else codegen_emit(cg,"    mov [rbp - %d], rax", cg->stack_offset);
        break;
    }

    /* -- assign -- */
    case AST_ASSIGN: {
        /* _ discard: just evaluate for side effects */
        if (strcmp(stmt->data.assign.var_name, "_discard") == 0) {
            codegen_expression(cg, stmt->data.assign.value);
            break;
        }
        int idx = codegen_find_var(cg, stmt->data.assign.var_name);
        if (idx < 0) error_at(stmt->line, stmt->column,
                               "undefined variable '%s'", stmt->data.assign.var_name);
        if (!cg->var_mutable[idx]) {
            TypeInfo *ati = cg->var_typeinfo[idx];
            bool ref_ok = ati && ati->is_ref;
            if (!ref_ok)
                error_at(stmt->line, stmt->column,
                         "cannot assign to immutable variable '%s'",
                         stmt->data.assign.var_name);
        }
        ValueType vt = codegen_expression(cg, stmt->data.assign.value);
        ValueType decl = cg->var_types[idx];
        TypeInfo *dti = cg->var_typeinfo[idx];
        validate_unsigned_literal_range(stmt->data.assign.value, decl,
                                        stmt->line, stmt->column);
        if (vt != decl) codegen_cast(cg, vt, decl);
        int off = cg->var_offsets[idx];
        /* Reference param: write through the stored pointer */
        if (dti && dti->is_ref && !is_composite(decl)) {
            codegen_emit(cg,"    mov rbx, [rbp - %d]", off);  /* load ptr */
            if (decl==TYPE_F32) codegen_emit(cg,"    movss [rbx], xmm0");
            else if (decl==TYPE_F64) codegen_emit(cg,"    movsd [rbx], xmm0");
            else codegen_emit(cg,"    mov [rbx], rax");
        } else {
            if (decl==TYPE_F32) codegen_emit(cg,"    movss [rbp - %d], xmm0", off);
            else if (decl==TYPE_F64) codegen_emit(cg,"    movsd [rbp - %d], xmm0", off);
            else codegen_emit(cg,"    mov [rbp - %d], rax", off);
        }
        break;
    }

    /* -- compound assign +=, -=, *=, /=, %=, &=, |=, ^=, <<=, >>= -- */
    case AST_COMPOUND_ASSIGN: {
        int idx = codegen_find_var(cg, stmt->data.compound_assign.var_name);
        if (idx < 0) error_at(stmt->line, stmt->column,
                               "undefined variable '%s'",
                               stmt->data.compound_assign.var_name);
        TypeInfo *dti = cg->var_typeinfo[idx];
        bool is_ref_var = dti && dti->is_ref;
        /* ref params are always mutable (the caller chose to pass by ref) */
        if (!cg->var_mutable[idx] && !is_ref_var)
            error_at(stmt->line, stmt->column,
                     "cannot assign to immutable '%s'",
                     stmt->data.compound_assign.var_name);
        ValueType decl = cg->var_types[idx];
        int off = cg->var_offsets[idx];
        BinaryOp op = stmt->data.compound_assign.op;

        /* Load current value — save to stack (not xmm1, which RHS may clobber) */
        if (is_ref_var) {
            /* deref the stored pointer */
            codegen_emit(cg,"    mov rcx, [rbp - %d]", off);
            if (decl==TYPE_F32) {
                codegen_emit(cg,"    movss xmm0, [rcx]");
                codegen_emit(cg,"    movss [rbp - %d], xmm0", cg->stack_offset + 8); /* temp above frame */
                codegen_emit(cg,"    sub rsp, 16");
                codegen_emit(cg,"    movss [rsp], xmm0");
            } else if (decl==TYPE_F64) {
                codegen_emit(cg,"    movsd xmm0, [rcx]");
                codegen_emit(cg,"    sub rsp, 16");
                codegen_emit(cg,"    movsd [rsp], xmm0");
            } else {
                codegen_emit(cg,"    mov rax, [rcx]");
            }
        } else {
            if (decl==TYPE_F32) {
                codegen_emit(cg,"    movss xmm0, [rbp - %d]", off);
                codegen_emit(cg,"    sub rsp, 16");
                codegen_emit(cg,"    movss [rsp], xmm0");
            } else if (decl==TYPE_F64) {
                codegen_emit(cg,"    movsd xmm0, [rbp - %d]", off);
                codegen_emit(cg,"    sub rsp, 16");
                codegen_emit(cg,"    movsd [rsp], xmm0");
            } else {
                codegen_emit(cg,"    mov rax, [rbp - %d]", off);
            }
        }
        codegen_emit(cg,"    push rax");   /* save LHS (integer path uses this; float path uses [rsp+8]) */

        /* Eval RHS */
        ValueType vt = codegen_expression(cg, stmt->data.compound_assign.value);

        if (!is_float(decl) && !is_float(vt)) {
            /* Integer compound assign — all 10 operators */
            codegen_emit(cg,"    mov rbx, rax");
            codegen_emit(cg,"    pop rax");
            switch (op) {
            case OP_ADD:  codegen_emit(cg,"    add  rax, rbx"); break;
            case OP_SUB:  codegen_emit(cg,"    sub  rax, rbx"); break;
            case OP_MUL:  codegen_emit(cg,"    imul rax, rbx"); break;
            case OP_DIV:  codegen_emit(cg,"    cqo"); codegen_emit(cg,"    idiv rbx"); break;
            case OP_MOD:  codegen_emit(cg,"    cqo"); codegen_emit(cg,"    idiv rbx");
                          codegen_emit(cg,"    mov rax, rdx"); break;
            case OP_BAND: codegen_emit(cg,"    and  rax, rbx"); break;
            case OP_BOR:  codegen_emit(cg,"    or   rax, rbx"); break;
            case OP_BXOR: codegen_emit(cg,"    xor  rax, rbx"); break;
            case OP_SHL:  codegen_emit(cg,"    mov rcx, rbx");
                          codegen_emit(cg,"    shl  rax, cl");  break;
            case OP_SHR:  codegen_emit(cg,"    mov rcx, rbx");
                          codegen_emit(cg,"    sar  rax, cl");  break;
            default: break;
            }
            /* Write result back — through pointer if ref */
            if (is_ref_var) {
                codegen_emit(cg,"    mov rcx, [rbp - %d]", off);
                codegen_emit(cg,"    mov [rcx], rax");
            } else {
                codegen_emit(cg,"    mov [rbp - %d], rax", off);
            }
        } else {
            /* Float compound assign — promote if needed */
            /* RHS result is in xmm0. Reload LHS from stack (was saved before RHS eval) */
            if (decl==TYPE_F32) {
                codegen_emit(cg,"    movss xmm1, [rsp + 8]"); /* LHS saved above the rax push */
                codegen_emit(cg,"    add rsp, 24");  /* discard float save (16) + rax push (8) */
                if (is_float(vt)) {
                    if (vt==TYPE_F64) codegen_emit(cg,"    cvtsd2ss xmm0, xmm0");
                } else {
                    codegen_emit(cg,"    cvtsi2ss xmm0, rax");
                }
                switch(op) {
                case OP_ADD: codegen_emit(cg,"    addss xmm1, xmm0"); break;
                case OP_SUB: codegen_emit(cg,"    subss xmm1, xmm0"); break;
                case OP_MUL: codegen_emit(cg,"    mulss xmm1, xmm0"); break;
                case OP_DIV: codegen_emit(cg,"    divss xmm1, xmm0"); break;
                default: break;
                }
                if (is_ref_var) {
                    codegen_emit(cg,"    mov rcx, [rbp - %d]", off);
                    codegen_emit(cg,"    movss [rcx], xmm1");
                } else {
                    codegen_emit(cg,"    movss [rbp - %d], xmm1", off);
                }
            } else {
                codegen_emit(cg,"    movsd xmm1, [rsp + 8]"); /* LHS saved above the rax push */
                codegen_emit(cg,"    add rsp, 24");  /* discard float save (16) + rax push (8) */
                if (is_float(vt)) {
                    if (vt==TYPE_F32) codegen_emit(cg,"    cvtss2sd xmm0, xmm0");
                } else {
                    codegen_emit(cg,"    cvtsi2sd xmm0, rax");
                }
                switch(op) {
                case OP_ADD: codegen_emit(cg,"    addsd xmm1, xmm0"); break;
                case OP_SUB: codegen_emit(cg,"    subsd xmm1, xmm0"); break;
                case OP_MUL: codegen_emit(cg,"    mulsd xmm1, xmm0"); break;
                case OP_DIV: codegen_emit(cg,"    divsd xmm1, xmm0"); break;
                default: break;
                }
                if (is_ref_var) {
                    codegen_emit(cg,"    mov rcx, [rbp - %d]", off);
                    codegen_emit(cg,"    movsd [rcx], xmm1");
                } else {
                    codegen_emit(cg,"    movsd [rbp - %d], xmm1", off);
                }
            }
        }
        break;
    }

    /* -- field assign: s.field = expr  OR  s.f1.f2 = expr (chained) -- */
    case AST_FIELD_ASSIGN: {
        /* The field_name may contain dots for chained access: "pos.x" means
           first navigate to field "pos" (which must be a struct), then assign "x". */
        int idx = codegen_find_var(cg, stmt->data.field_assign.var_name);
        if (idx < 0) error_at(stmt->line, stmt->column,
                               "undefined variable '%s'",
                               stmt->data.field_assign.var_name);
        if (!cg->var_mutable[idx])
            error_at(stmt->line, stmt->column,
                     "cannot assign to immutable '%s'",
                     stmt->data.field_assign.var_name);
        if (cg->var_types[idx] != TYPE_STRUCT)
            error_at(stmt->line, stmt->column,
                     "'%s' is not a struct", stmt->data.field_assign.var_name);

        /* Split field_name on '.' into segments */
        char chain_buf[MAX_TOKEN_LEN];
        strncpy(chain_buf, stmt->data.field_assign.field_name, MAX_TOKEN_LEN-1);
        char *segments[16];
        int seg_count = 0;
        char *tok2 = strtok(chain_buf, ".");
        while (tok2 && seg_count < 16) {
            segments[seg_count++] = tok2;
            tok2 = strtok(NULL, ".");
        }

        TypeInfo *ti = cg->var_typeinfo[idx];
        StructDef *sd = struct_find(cg, ti->struct_name);
        if (!sd) error("unknown struct");
        struct_size(cg, sd);

        /* Emit base struct address */
        if (ti->by_ref)
            codegen_emit(cg,"    mov rbx, [rbp - %d]", cg->var_offsets[idx]);
        else
            codegen_emit(cg,"    lea rbx, [rbp - %d]", cg->var_offsets[idx]);

        /* Navigate through intermediate struct fields */
        for (int si = 0; si < seg_count - 1; si++) {
            int fidx2 = struct_field_index(sd, segments[si]);
            if (fidx2 < 0)
                error_at(stmt->line, stmt->column,
                         "unknown field '%s' on '%s'", segments[si], sd->name);
            if (sd->field_types[fidx2] != TYPE_STRUCT)
                error_at(stmt->line, stmt->column,
                         "field '%s' is not a struct", segments[si]);
            struct_size(cg, sd);
            codegen_emit(cg,"    add rbx, %d", sd->field_offsets[fidx2]);
            TypeInfo *fti2 = sd->field_typeinfo ? sd->field_typeinfo[fidx2] : NULL;
            if (!fti2) error("struct field missing type info");
            sd = struct_find(cg, fti2->struct_name);
            if (!sd) error("unknown struct '%s'", fti2->struct_name);
            struct_size(cg, sd);
        }

        /* Final field */
        const char *last_field = segments[seg_count - 1];
        int fidx = struct_field_index(sd, last_field);
        if (fidx < 0) error_at(stmt->line, stmt->column,
                                "unknown field '%s' on '%s'", last_field, sd->name);
        int foff = sd->field_offsets[fidx];
        ValueType expect = sd->field_types[fidx];

        codegen_emit(cg,"    push rbx");
        ValueType vt = codegen_expression(cg, stmt->data.field_assign.value);
        codegen_emit(cg,"    pop rbx");
        validate_unsigned_literal_range(stmt->data.field_assign.value, expect,
                                        stmt->line, stmt->column);
        if (vt != expect) codegen_cast(cg, vt, expect);
        if (expect==TYPE_F32) codegen_emit(cg,"    movss [rbx + %d], xmm0", foff);
        else if (expect==TYPE_F64) codegen_emit(cg,"    movsd [rbx + %d], xmm0", foff);
        else codegen_emit(cg,"    mov [rbx + %d], rax", foff);
        break;
    }

    /* -- array element assign: arr[i] = v -- */
    case AST_ARRAY_ASSIGN: {
        int idx = codegen_find_var(cg, stmt->data.array_assign.var_name);
        if (idx < 0) error_at(stmt->line, stmt->column,
                               "undefined variable '%s'",
                               stmt->data.array_assign.var_name);
        if (!cg->var_mutable[idx])
            error_at(stmt->line, stmt->column,
                     "cannot assign to immutable '%s'",
                     stmt->data.array_assign.var_name);
        if (cg->var_types[idx] != TYPE_ARRAY)
            error_at(stmt->line, stmt->column,
                     "'%s' is not an array", stmt->data.array_assign.var_name);
        TypeInfo *ti = cg->var_typeinfo[idx];
        if (!ti) error("array missing type info");
        ValueType elem = ti->elem_type;
        int elem_size  = array_elem_size(cg, ti);
        int var_off    = cg->var_offsets[idx];

        /* evaluate index */
        ValueType it = codegen_expression(cg, stmt->data.array_assign.index);
        if (it != TYPE_INT) codegen_cast(cg, it, TYPE_INT);
        codegen_emit(cg,"    mov rcx, rax");

        if (ti->by_ref || ti->array_len < 0)
            codegen_emit(cg,"    mov rbx, [rbp - %d]", var_off);
        else
            codegen_emit(cg,"    lea rbx, [rbp - %d]", var_off);

        /* evaluate value */
        codegen_emit(cg,"    push rcx");
        codegen_emit(cg,"    push rbx");
        ValueType vt = codegen_expression(cg, stmt->data.array_assign.value);
        codegen_emit(cg,"    pop rbx");
        codegen_emit(cg,"    pop rcx");
        ValueType ex = elem;
        validate_unsigned_literal_range(stmt->data.array_assign.value, ex,
                                        stmt->line, stmt->column);
        if (vt != ex) codegen_cast(cg, vt, ex);

        if (elem == TYPE_UINT8) {
            codegen_emit(cg,"    mov [rbx + rcx], al");
        } else if (elem_size == 8) {
            if (ex==TYPE_F32) codegen_emit(cg,"    movss [rbx + rcx*8], xmm0");
            else if (ex==TYPE_F64) codegen_emit(cg,"    movsd [rbx + rcx*8], xmm0");
            else codegen_emit(cg,"    mov [rbx + rcx*8], rax");
        } else {
            codegen_emit(cg,"    imul rcx, %d", elem_size);
            codegen_emit(cg,"    add rbx, rcx");
            if (ex==TYPE_F32) codegen_emit(cg,"    movss [rbx], xmm0");
            else if (ex==TYPE_F64) codegen_emit(cg,"    movsd [rbx], xmm0");
            else codegen_emit(cg,"    mov [rbx], rax");
        }

        /* For dynamic arrays, update length if this index >= current length */
        if (ti->array_len < 0) {
            codegen_emit(cg,"    mov rbx, [rbp - %d]", var_off);
            codegen_emit(cg,"    mov rax, [rbx - 8]");
            codegen_emit(cg,"    cmp rcx, rax");
            codegen_emit(cg,"    jl  .skip_len_upd_%d", cg->label_counter);
            codegen_emit(cg,"    mov rax, rcx");
            codegen_emit(cg,"    inc rax");
            codegen_emit(cg,"    mov [rbx - 8], rax");
            codegen_emit(cg,".skip_len_upd_%d:", cg->label_counter++);
        }
        break;
    }

    /* -- append -- */
    case AST_APPEND: {
        int idx = codegen_find_var(cg, stmt->data.append_stmt.arr_name);
        if (idx < 0) error_at(stmt->line, stmt->column,
                               "undefined variable '%s'",
                               stmt->data.append_stmt.arr_name);
        if (!cg->var_mutable[idx])
            error_at(stmt->line, stmt->column,
                     "cannot append to immutable '%s'",
                     stmt->data.append_stmt.arr_name);
        TypeInfo *ti = cg->var_typeinfo[idx];
        if (!ti || ti->array_len >= 0)
            error_at(stmt->line, stmt->column,
                     "append() only works on dynamic arrays (ath mut arr: [type] = [])");
        cg->needs_arr_alloc   = true;
        cg->needs_arr_append  = true;
        int elem_size = array_elem_size(cg, ti);

        /* evaluate value */
        ValueType vt = codegen_expression(cg, stmt->data.append_stmt.value);
        validate_unsigned_literal_range(stmt->data.append_stmt.value, ti->elem_type,
                                        stmt->line, stmt->column);
        if (vt != ti->elem_type) codegen_cast(cg, vt, ti->elem_type);
        if (is_float(vt)) {
            if (vt==TYPE_F64) codegen_emit(cg,"    movq rax, xmm0");
            else              codegen_emit(cg,"    movd eax, xmm0");
        }
        if (vt == TYPE_UINT8)
            codegen_emit(cg,"    and rax, 0xFF");

        /* call __vl_arr_append(&arr_slot, value-or-src-ptr, elem_size) */
        codegen_emit(cg,"    push rax");   /* save value or struct source pointer */
        if (cg->target_windows) {
            codegen_emit(cg,"    lea rcx, [rbp - %d]", cg->var_offsets[idx]);
            codegen_emit(cg,"    pop rdx");
            codegen_emit(cg,"    mov r8, %d", elem_size);
            codegen_emit(cg,"    sub rsp, 32");
        } else {
            codegen_emit(cg,"    lea rdi, [rbp - %d]", cg->var_offsets[idx]);
            codegen_emit(cg,"    pop rsi");
            codegen_emit(cg,"    mov rdx, %d", elem_size);
        }
        codegen_emit(cg,"    call __vl_arr_append");
        if (cg->target_windows) codegen_emit(cg,"    add rsp, 32");
        break;
    }

    /* -- if -- */
    case AST_IF: {
        int else_l = codegen_new_label(cg);
        int end_l  = codegen_new_label(cg);
        ValueType ct = codegen_expression(cg, stmt->data.if_stmt.condition);
        if (is_float(ct)) codegen_cast(cg, ct, TYPE_INT);
        codegen_emit(cg,"    cmp rax, 0");
        codegen_emit(cg,"    je  _VL%d", else_l);
        for (int i = 0; i < stmt->data.if_stmt.then_count; i++)
            codegen_statement(cg, stmt->data.if_stmt.then_stmts[i]);
        codegen_emit(cg,"    jmp _VL%d", end_l);
        codegen_emit(cg,"_VL%d:", else_l);
        for (int i = 0; i < stmt->data.if_stmt.else_count; i++)
            codegen_statement(cg, stmt->data.if_stmt.else_stmts[i]);
        codegen_emit(cg,"_VL%d:", end_l);
        break;
    }

    /* -- while -- */
    case AST_WHILE: {
        int start_l = codegen_new_label(cg);
        int end_l   = codegen_new_label(cg);
        codegen_emit(cg,"_VL%d:", start_l);
        ValueType ct = codegen_expression(cg, stmt->data.while_stmt.condition);
        if (is_float(ct)) codegen_cast(cg, ct, TYPE_INT);
        codegen_emit(cg,"    cmp rax, 0");
        codegen_emit(cg,"    je  _VL%d", end_l);
        loop_push(cg, start_l, end_l);
        for (int i = 0; i < stmt->data.while_stmt.body_count; i++)
            codegen_statement(cg, stmt->data.while_stmt.body[i]);
        loop_pop(cg);
        codegen_emit(cg,"    jmp _VL%d", start_l);
        codegen_emit(cg,"_VL%d:", end_l);
        break;
    }

    /* -- for(;;) -- */
    case AST_FOR: {
        if (stmt->data.for_stmt.init)
            codegen_statement(cg, stmt->data.for_stmt.init);
        int start_l = codegen_new_label(cg);
        int cont_l  = codegen_new_label(cg);
        int end_l   = codegen_new_label(cg);
        codegen_emit(cg,"_VL%d:", start_l);
        if (stmt->data.for_stmt.condition) {
            ValueType ct = codegen_expression(cg, stmt->data.for_stmt.condition);
            if (is_float(ct)) codegen_cast(cg, ct, TYPE_INT);
            codegen_emit(cg,"    cmp rax, 0");
            codegen_emit(cg,"    je  _VL%d", end_l);
        }
        loop_push(cg, cont_l, end_l);
        for (int i = 0; i < stmt->data.for_stmt.body_count; i++)
            codegen_statement(cg, stmt->data.for_stmt.body[i]);
        loop_pop(cg);
        codegen_emit(cg,"_VL%d:", cont_l);
        if (stmt->data.for_stmt.post)
            codegen_statement(cg, stmt->data.for_stmt.post);
        codegen_emit(cg,"    jmp _VL%d", start_l);
        codegen_emit(cg,"_VL%d:", end_l);
        break;
    }

    /* -- for i in range/array -- */
    case AST_FOR_IN: {
        ASTNode *iter = stmt->data.for_in_stmt.iterable;
        if (iter && iter->type == AST_RANGE) {
            char endname[32], stepname[32];
            snprintf(endname,  32, "_fend%d",  codegen_new_label(cg));
            snprintf(stepname, 32, "_fstep%d", codegen_new_label(cg));
            codegen_add_var(cg, stmt->data.for_in_stmt.var_name, TYPE_INT, true, NULL);
            int var_idx = cg->var_count - 1;
            codegen_add_var(cg, endname,  TYPE_INT, false, NULL);
            int end_idx = cg->var_count - 1;
            codegen_add_var(cg, stepname, TYPE_INT, false, NULL);
            int step_idx = cg->var_count - 1;

            ValueType st = codegen_expression(cg, iter->data.range.start);
            if (is_float(st)) codegen_cast(cg, st, TYPE_INT);
            codegen_emit(cg,"    mov [rbp - %d], rax", cg->var_offsets[var_idx]);
            ValueType et = codegen_expression(cg, iter->data.range.end);
            if (is_float(et)) codegen_cast(cg, et, TYPE_INT);
            codegen_emit(cg,"    mov [rbp - %d], rax", cg->var_offsets[end_idx]);
            if (iter->data.range.step) {
                codegen_expression(cg, iter->data.range.step);
            } else {
                codegen_emit(cg,"    mov rax, 1");
            }
            codegen_emit(cg,"    mov [rbp - %d], rax", cg->var_offsets[step_idx]);

            int start_l = codegen_new_label(cg);
            int cont_l  = codegen_new_label(cg);
            int end_l   = codegen_new_label(cg);
            codegen_emit(cg,"_VL%d:", start_l);
            codegen_emit(cg,"    mov rax, [rbp - %d]", cg->var_offsets[var_idx]);
            codegen_emit(cg,"    mov rbx, [rbp - %d]", cg->var_offsets[end_idx]);
            codegen_emit(cg,"    cmp rax, rbx");
            if (iter->data.range.inclusive)
                codegen_emit(cg,"    jg  _VL%d", end_l);
            else
                codegen_emit(cg,"    jge _VL%d", end_l);
            loop_push(cg, cont_l, end_l);
            for (int i = 0; i < stmt->data.for_in_stmt.body_count; i++)
                codegen_statement(cg, stmt->data.for_in_stmt.body[i]);
            loop_pop(cg);
            codegen_emit(cg,"_VL%d:", cont_l);
            codegen_emit(cg,"    mov rax, [rbp - %d]", cg->var_offsets[var_idx]);
            codegen_emit(cg,"    add rax, [rbp - %d]", cg->var_offsets[step_idx]);
            codegen_emit(cg,"    mov [rbp - %d], rax", cg->var_offsets[var_idx]);
            codegen_emit(cg,"    jmp _VL%d", start_l);
            codegen_emit(cg,"_VL%d:", end_l);
            break;
        }

        /* array iteration */
        TypeInfo *ti = NULL;
        int arr_idx = -1;
        if (iter && iter->type == AST_IDENTIFIER) {
            arr_idx = codegen_find_var(cg, iter->data.identifier);
            if (arr_idx < 0)
                error_at(stmt->line, stmt->column,
                         "undefined variable '%s'", iter->data.identifier);
            ti = cg->var_typeinfo[arr_idx];
        } else if (iter) {
            /* call returning array */
            ValueType rt = codegen_expression(cg, iter);
            if (rt != TYPE_ARRAY)
                error_at(stmt->line, stmt->column, "for-in requires array");
            char arrname[32]; snprintf(arrname,32,"_farr%d",codegen_new_label(cg));
            FunctionSig *sig = NULL;
            if (iter->type == AST_CALL) sig = func_sig_find(cg, iter->data.call.func_name);
            else if (iter->type == AST_MODULE_CALL) {
                char fn[MAX_TOKEN_LEN*2+4];
                snprintf(fn,sizeof(fn),"%s__%s",
                         iter->data.module_call.module_name,
                         iter->data.module_call.func_name);
                sig = func_sig_find(cg, fn);
            }
            if (!sig || !sig->return_typeinfo)
                error_at(stmt->line, stmt->column,
                         "for-in call must return typed array");
            TypeInfo *arr_ti = typeinfo_clone(sig->return_typeinfo);
            codegen_add_var(cg, arrname, TYPE_ARRAY, true, arr_ti);
            arr_idx = cg->var_count - 1;
            codegen_emit(cg,"    mov [rbp - %d], rax", cg->var_offsets[arr_idx]);
            ti = arr_ti;
        }
        if (!ti) error_at(stmt->line, stmt->column, "for-in: missing type info");
        ValueType elem = ti->elem_type;

        char idxn[32], lenn[32];
        snprintf(idxn,32,"_fidi%d",codegen_new_label(cg));
        snprintf(lenn,32,"_flen%d",codegen_new_label(cg));

        TypeInfo *elem_ti = NULL;
        if (elem == TYPE_STRUCT && ti->elem_typeinfo)
            elem_ti = typeinfo_ref(ti->elem_typeinfo);
        codegen_add_var(cg, stmt->data.for_in_stmt.var_name, elem, true, elem_ti);
        int var_idx = cg->var_count - 1;
        codegen_add_var(cg, idxn, TYPE_INT, true, NULL);
        int idx_idx = cg->var_count - 1;
        codegen_add_var(cg, lenn, TYPE_INT, false, NULL);
        int len_idx = cg->var_count - 1;

        codegen_emit(cg,"    mov qword [rbp - %d], 0", cg->var_offsets[idx_idx]);
        if (ti->array_len >= 0)
            codegen_emit(cg,"    mov qword [rbp - %d], %d",
                         cg->var_offsets[len_idx], ti->array_len);
        else {
            codegen_emit(cg,"    mov rbx, [rbp - %d]", cg->var_offsets[arr_idx]);
            codegen_emit(cg,"    mov rax, [rbx - 8]");
            codegen_emit(cg,"    mov [rbp - %d], rax", cg->var_offsets[len_idx]);
        }

        int start_l=codegen_new_label(cg), cont_l=codegen_new_label(cg), end_l=codegen_new_label(cg);
        codegen_emit(cg,"_VL%d:", start_l);
        codegen_emit(cg,"    mov rax, [rbp - %d]", cg->var_offsets[idx_idx]);
        codegen_emit(cg,"    cmp rax, [rbp - %d]", cg->var_offsets[len_idx]);
        codegen_emit(cg,"    jge _VL%d", end_l);

        /* load element into loop var */
        codegen_emit(cg,"    mov rcx, [rbp - %d]", cg->var_offsets[idx_idx]);
        if (ti->array_len < 0)
            codegen_emit(cg,"    mov rbx, [rbp - %d]", cg->var_offsets[arr_idx]);
        else
            codegen_emit(cg,"    lea rbx, [rbp - %d]", cg->var_offsets[arr_idx]);

        if (elem == TYPE_F32) {
            codegen_emit(cg,"    movss xmm0, [rbx + rcx*8]");
            codegen_emit(cg,"    movss [rbp - %d], xmm0", cg->var_offsets[var_idx]);
        } else if (elem == TYPE_F64) {
            codegen_emit(cg,"    movsd xmm0, [rbx + rcx*8]");
            codegen_emit(cg,"    movsd [rbp - %d], xmm0", cg->var_offsets[var_idx]);
        } else if (elem == TYPE_STRUCT) {
            /* store pointer to element */
            int es = array_elem_size(cg, ti);
            if (es == 8)
                codegen_emit(cg,"    lea rax, [rbx + rcx*8]");
            else {
                codegen_emit(cg,"    imul rcx, %d", es);
                codegen_emit(cg,"    lea rax, [rbx + rcx]");
            }
            codegen_emit(cg,"    mov [rbp - %d], rax", cg->var_offsets[var_idx]);
        } else {
            codegen_emit(cg,"    mov rax, [rbx + rcx*8]");
            codegen_emit(cg,"    mov [rbp - %d], rax", cg->var_offsets[var_idx]);
        }

        loop_push(cg, cont_l, end_l);
        for (int i = 0; i < stmt->data.for_in_stmt.body_count; i++)
            codegen_statement(cg, stmt->data.for_in_stmt.body[i]);
        loop_pop(cg);

        codegen_emit(cg,"_VL%d:", cont_l);
        codegen_emit(cg,"    add qword [rbp - %d], 1", cg->var_offsets[idx_idx]);
        codegen_emit(cg,"    jmp _VL%d", start_l);
        codegen_emit(cg,"_VL%d:", end_l);
        break;
    }

    case AST_BREAK:
        codegen_emit(cg,"    jmp _VL%d", loop_brk(cg));
        break;

    case AST_CONTINUE:
        codegen_emit(cg,"    jmp _VL%d", loop_cont(cg));
        break;

    /* -- panic -- */
    case AST_PANIC: {
        cg->needs_panic = true;
        ValueType mt = codegen_expression(cg, stmt->data.panic_stmt.message);
        if (mt != TYPE_STRING) {
            /* convert to string not supported -- use literal */
            int sid = codegen_intern_string(cg, "panic!");
            codegen_emit(cg,"    lea rax, [rel _VL_str_%d]", sid);
        }
        if (cg->try_depth > 0) {
            int top = cg->try_depth - 1;
            int catch_l = cg->try_catch_labels[top];
            int evar_off = cg->error_var_offsets[top];
            codegen_emit(cg,"    mov [rbp - %d], rax", evar_off);
            codegen_emit(cg,"    sub qword [rel _VL_try_depth_rt], 1");
            codegen_emit(cg,"    jmp _VL%d", catch_l);
        } else {
            codegen_emit(cg,"    mov %s, rax", arg_reg(cg, 0));
            if (cg->target_windows) codegen_emit(cg,"    sub rsp, 32");
            codegen_emit(cg,"    call __vl_panic");
            if (cg->target_windows) codegen_emit(cg,"    add rsp, 32");
            /* If __vl_panic returns (inside an enclosing try), unwind this function. */
            codegen_emit(cg,"    xor rax, rax");
            codegen_emit(cg,"    mov rsp, rbp");
            codegen_emit(cg,"    pop rbp");
            codegen_emit(cg,"    ret");
        }
        break;
    }

    /* -- throw -- */
    case AST_THROW: {
        /* Simple: treat as panic with string message */
        cg->needs_panic = true;
        ValueType vt = codegen_expression(cg, stmt->data.throw_stmt.value);
        if (vt != TYPE_STRING) {
            int sid = codegen_intern_string(cg, "error thrown");
            codegen_emit(cg,"    lea rax, [rel _VL_str_%d]", sid);
        }
        if (cg->try_depth > 0) {
            int top = cg->try_depth - 1;
            int catch_l = cg->try_catch_labels[top];
            int evar_off = cg->error_var_offsets[top];
            codegen_emit(cg,"    mov [rbp - %d], rax", evar_off);
            codegen_emit(cg,"    sub qword [rel _VL_try_depth_rt], 1");
            codegen_emit(cg,"    jmp _VL%d", catch_l);
        } else {
            codegen_emit(cg,"    mov %s, rax", arg_reg(cg, 0));
            if (cg->target_windows) codegen_emit(cg,"    sub rsp, 32");
            codegen_emit(cg,"    call __vl_panic");
            if (cg->target_windows) codegen_emit(cg,"    add rsp, 32");
            /* If __vl_panic returns (inside an enclosing try), unwind this function. */
            codegen_emit(cg,"    xor rax, rax");
            codegen_emit(cg,"    mov rsp, rbp");
            codegen_emit(cg,"    pop rbp");
            codegen_emit(cg,"    ret");
        }
        break;
    }

    /* -- try/catch -- */
    case AST_TRY_CATCH: {
        cg->needs_panic = true; /* needs runtime error globals/probe symbols */
        int catch_l = codegen_new_label(cg);
        int end_l   = codegen_new_label(cg);
        /* Register catch var */
        codegen_add_var(cg, stmt->data.try_catch.catch_var,
                        TYPE_STRING, true, NULL);
        int evar_off = cg->var_offsets[cg->var_count - 1];
        codegen_emit(cg,"    mov qword [rbp - %d], 0", evar_off);

        if (cg->try_depth >= 64)
            error_at(stmt->line, stmt->column, "try/catch nesting too deep");
        cg->try_catch_labels[cg->try_depth] = catch_l;
        cg->error_var_offsets[cg->try_depth] = evar_off;
        cg->try_depth++;
        codegen_emit(cg,"    add qword [rel _VL_try_depth_rt], 1");

        /* try body */
        for (int i = 0; i < stmt->data.try_catch.try_count; i++)
            codegen_statement(cg, stmt->data.try_catch.try_body[i]);
        cg->try_depth--;
        codegen_emit(cg,"    sub qword [rel _VL_try_depth_rt], 1");
        codegen_emit(cg,"    jmp _VL%d", end_l);

        /* catch body */
        codegen_emit(cg,"_VL%d:", catch_l);
        codegen_emit(cg,"    mov qword [rel _VL_err_flag], 0");
        codegen_emit(cg,"    mov qword [rel _VL_err_msg], 0");
        for (int i = 0; i < stmt->data.try_catch.catch_count; i++)
            codegen_statement(cg, stmt->data.try_catch.catch_body[i]);
        codegen_emit(cg,"_VL%d:", end_l);
        break;
    }

    /* -- expression statement -- */
    default:
        codegen_expression(cg, stmt);
        break;
    }
}

/* ------------------------------------------------------------
 *  Function codegen
 * ------------------------------------------------------------ */
/* ----------------------------------------------------------------
 *  AArch64 (GNU as) function emitter
 *  Calling convention: AAPCS64
 *    Integer/pointer args: x0-x7
 *    Float args:           d0-d7
 *    Return value:         x0 (or d0 for float)
 *    Callee-saved:         x19-x28, x29(fp), x30(lr)
 *    Frame pointer:        x29
 *    Link register:        x30
 *
 *  We use a simplified frame: fp=x29, lr=x30, local vars at [fp-N].
 *  We don't support float params in generic emitter (no xmm equivalent
 *  in this path) but integer/pointer/bool/string all work.
 * ---------------------------------------------------------------- */
static void codegen_function_aarch64(CodeGen *cg, ASTNode *func);

void codegen_function(CodeGen *cg, ASTNode *func) {
    if (cg->target_aarch64) { codegen_function_aarch64(cg, func); return; }

    /* reset per-function state */
    for (int i = 0; i < cg->var_count; i++) free(cg->local_vars[i]);
    cg->var_count    = 0;
    cg->stack_offset = 0;
    cg->loop_depth   = 0;
    cg->try_depth    = 0;
    cg->current_return_type     = func->data.function.return_type;
    cg->current_return_typeinfo = func->data.function.return_typeinfo;
    cg->has_sret = (cg->current_return_type == TYPE_TUPLE ||
                    cg->current_return_type == TYPE_STRUCT);

    codegen_emit(cg,"; ----- %s -----", func->data.function.name);
    codegen_emit(cg,"global %s", func->data.function.name);
    codegen_emit(cg,"%s:", func->data.function.name);
    codegen_emit(cg,"    push rbp");
    codegen_emit(cg,"    mov  rbp, rsp");
    codegen_emit(cg,"    sub  rsp, %d", LOCAL_STACK_RESERVE);

    /* Windows: save non-volatile registers */
    if (cg->target_windows) {
        codegen_emit(cg,"    push rbx");
        codegen_emit(cg,"    push rsi");
        codegen_emit(cg,"    push rdi");
        codegen_emit(cg,"    push r12");
        codegen_emit(cg,"    push r13");
    }

    if (cg->has_sret) {
        cg->stack_offset += 8;
        cg->sret_offset = cg->stack_offset;
        codegen_emit(cg,"    mov [rbp - %d], %s",
                     cg->sret_offset, arg_reg(cg, 0));
    }

    for (int i = 0; i < func->data.function.param_count; i++) {
        ValueType pt = func->data.function.param_types[i];
        TypeInfo *pti = func->data.function.param_typeinfo
                        ? func->data.function.param_typeinfo[i] : NULL;
        bool param_ref = func->data.function.param_is_ref
                         && func->data.function.param_is_ref[i];
        if (param_ref) {
            /* Reference param: caller passes address, store pointer on stack */
            if (!pti) {
                pti = (TypeInfo*)calloc(1, sizeof(TypeInfo));
                pti->kind = pt;
            } else {
                pti = typeinfo_clone(pti);
            }
            pti->is_ref = true;
        } else if (pti && (pti->kind==TYPE_TUPLE || pti->kind==TYPE_STRUCT)) {
            pti = typeinfo_ref(pti);
        }
        int reg_idx = i + (cg->has_sret ? 1 : 0);
        if (reg_idx >= ARG_REGS_MAX)
            error_at(func->line, func->column,
                     "function '%s' has too many parameters (max %d)",
                     func->data.function.name, ARG_REGS_MAX);
        codegen_add_var(cg, func->data.function.param_names[i], pt, true, pti);
        codegen_emit(cg,"    mov [rbp - %d], %s",
                     cg->stack_offset, arg_reg(cg, reg_idx));
    }

    for (int i = 0; i < func->data.function.body_count; i++)
        codegen_statement(cg, func->data.function.body[i]);

    /* implicit epilogue / fallthrough */
    if (cg->target_windows) {
        codegen_emit(cg,"    pop r13");
        codegen_emit(cg,"    pop r12");
        codegen_emit(cg,"    pop rdi");
        codegen_emit(cg,"    pop rsi");
        codegen_emit(cg,"    pop rbx");
    }
    codegen_emit(cg,"    mov rsp, rbp");
    codegen_emit(cg,"    pop rbp");
    codegen_emit(cg,"    ret");
    codegen_emit(cg,"");
}

/* ------------------------------------------------------------
 *  Module codegen
 * ------------------------------------------------------------ */
static void codegen_emit_native_externs(CodeGen *cg, const char *module_name,
                                        const char *asm_path) {
    FILE *f = fopen(asm_path, "r");
    if (!f) return;
    codegen_emit(cg,"; externs for native module: %s", module_name);
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p==' '||*p=='\t') p++;
        if (strncmp(p,"global",6)!=0) continue;
        p += 6;
        if (*p!=' '&&*p!='\t') continue;
        while (*p==' '||*p=='\t') p++;
        char sym[256]; int j=0;
        while (*p&&*p!='\n'&&*p!='\r'&&*p!=' '&&*p!='\t'&&j<255)
            sym[j++]=*p++;
        sym[j]='\0';
        if (j>0) codegen_emit(cg,"extern %s", sym);
    }
    fclose(f);
    codegen_emit(cg,"");
}

static void codegen_emit_module(CodeGen *cg, const char *module_name,
                                const char *file_path) {
    char *src = read_file(file_path);
    if (!src) return;
    ErrorContext prev = error_get_context();
    error_set_context(file_path, src);

    Lexer lx; lexer_init(&lx, src);
    Token toks[MAX_TOKENS];
    int ntok = lexer_tokenize(&lx, toks, MAX_TOKENS);
    Parser pr; parser_init(&pr, toks, ntok);
    ASTNode *prog = parse_program(&pr);
    codegen_register_structs(cg, prog);

    mod_func_table_clear();
    for (int i = 0; i < prog->data.program.function_count; i++)
        mod_func_table_add(prog->data.program.functions[i]->data.function.name);

    for (int i = 0; i < prog->data.program.function_count; i++) {
        ASTNode *fn = prog->data.program.functions[i];
        char mangled[MAX_TOKEN_LEN*2+4];
        make_module_symbol(mangled, sizeof(mangled), module_name,
                           fn->data.function.name);
        func_sig_add(cg, mangled,
                     fn->data.function.return_type,
                     fn->data.function.return_typeinfo,
                     fn->data.function.param_types,
                     fn->data.function.param_typeinfo,
                     fn->data.function.param_count);
    }

    strncpy(cg->current_module, module_name, MAX_TOKEN_LEN-1);
    codegen_emit(cg,"; ===== module: %s =====", module_name);

    for (int i = 0; i < prog->data.program.function_count; i++) {
        ASTNode *fn = prog->data.program.functions[i];
        for (int j = 0; j < cg->var_count; j++) free(cg->local_vars[j]);
        cg->var_count = 0; cg->stack_offset = 0; cg->loop_depth = 0; cg->try_depth = 0;
        cg->current_return_type = fn->data.function.return_type;
        cg->current_return_typeinfo = fn->data.function.return_typeinfo;
        cg->has_sret = (cg->current_return_type==TYPE_TUPLE ||
                        cg->current_return_type==TYPE_STRUCT);

        char mangled[MAX_TOKEN_LEN*2+4];
        make_module_symbol(mangled, sizeof(mangled), module_name,
                           fn->data.function.name);
        codegen_emit(cg,"global %s", mangled);
        codegen_emit(cg,"%s:", mangled);
        codegen_emit(cg,"    push rbp");
        codegen_emit(cg,"    mov  rbp, rsp");
        codegen_emit(cg,"    sub  rsp, %d", LOCAL_STACK_RESERVE);

        if (cg->has_sret) {
            cg->stack_offset += 8; cg->sret_offset = cg->stack_offset;
            codegen_emit(cg,"    mov [rbp - %d], %s", cg->sret_offset, arg_reg(cg,0));
        }
        for (int j = 0; j < fn->data.function.param_count; j++) {
            int reg_idx = j + (cg->has_sret?1:0);
            codegen_add_var(cg, fn->data.function.param_names[j],
                            fn->data.function.param_types[j], true,
                            fn->data.function.param_typeinfo ? fn->data.function.param_typeinfo[j] : NULL);
            codegen_emit(cg,"    mov [rbp - %d], %s", cg->stack_offset, arg_reg(cg,reg_idx));
        }
        for (int j = 0; j < fn->data.function.body_count; j++)
            codegen_statement(cg, fn->data.function.body[j]);
        codegen_emit(cg,"    mov rsp, rbp");
        codegen_emit(cg,"    pop rbp");
        codegen_emit(cg,"    ret");
        codegen_emit(cg,"");
    }

    cg->current_module[0] = '\0';
    mod_func_table_clear();
    ast_node_free(prog);
    error_restore_context(prev);
    free(src);
}

/* ------------------------------------------------------------
 *  Emit data sections
 * ------------------------------------------------------------ */
static void emit_float_literals(CodeGen *cg) {
    if (!cg->float_count) return;
    codegen_emit(cg,"section .data");
    for (int i = 0; i < cg->float_count; i++) {
        FloatLit *fl = &cg->float_literals[i];
        if (fl->type == TYPE_F32) {
            union { float f; unsigned u; } v; v.f = (float)fl->value;
            fprintf(cg->output, "_VL_f32_%d: dd 0x%08x\n", i, v.u);
        } else {
            union { double d; unsigned long long u; } v; v.d = fl->value;
            fprintf(cg->output, "_VL_f64_%d: dq 0x%016llx\n", i,
                    (unsigned long long)v.u);
        }
    }
}

static void emit_string_literals(CodeGen *cg) {
    if (!cg->string_count) return;
    codegen_emit(cg,"section .data");
    for (int i = 0; i < cg->string_count; i++) {
        const char *s = cg->string_literals[i] ? cg->string_literals[i] : "";
        size_t len = strlen(s);
        fprintf(cg->output, "_VL_str_%d:\n    db ", i);
        for (size_t j = 0; j < len; j++)
            fprintf(cg->output, "%u,", (unsigned char)s[j]);
        fprintf(cg->output, "0\n");
    }
}

static void emit_runtime_globals(CodeGen *cg) {
    codegen_emit(cg,"global _VL_argc");
    codegen_emit(cg,"global _VL_argv");
    codegen_emit(cg,"global _VL_envp");
    codegen_emit(cg,"section .bss");
    codegen_emit(cg,"    _VL_argc resq 1");
    codegen_emit(cg,"    _VL_argv resq 1");
    codegen_emit(cg,"    _VL_envp resq 1");
    codegen_emit(cg,"");
}

/* ------------------------------------------------------------
 *  Program codegen  (top-level entry point)
 * ------------------------------------------------------------ */
/* ---------------------------------------------------------------
 *  AArch64 statement/expression emitters (GNU as syntax)
 *  Only the subset needed to run Velocity programs:
 *  integers, strings, booleans, basic arithmetic, comparisons,
 *  if/while/for, function calls, io module calls.
 * --------------------------------------------------------------- */

/* emit a64 load of a 64-bit immediate into x0 */
static void a64_mov_imm(CodeGen *cg, const char *dst, long long val) {
    /* movz + optional movk for values > 16 bits */
    unsigned long long uv = (unsigned long long)val;
    codegen_emit(cg,"    mov  %s, #%lld", dst, (long long)uv);
}

/* load local variable at [fp-off] into x0 (or d0 for floats) */
static void a64_load_var(CodeGen *cg, int off, ValueType t) {
    if (t == TYPE_F64 || t == TYPE_F32)
        codegen_emit(cg,"    ldr  d0, [x29, #-%d]", off);
    else
        codegen_emit(cg,"    ldr  x0, [x29, #-%d]", off);
}

static void a64_store_var(CodeGen *cg, int off, ValueType t) {
    if (t == TYPE_F64 || t == TYPE_F32)
        codegen_emit(cg,"    str  d0, [x29, #-%d]", off);
    else
        codegen_emit(cg,"    str  x0, [x29, #-%d]", off);
}
/* forward decl */
static ValueType codegen_expr_a64(CodeGen *cg, ASTNode *expr);
static void      codegen_stmt_a64(CodeGen *cg, ASTNode *stmt);

static ValueType codegen_expr_a64(CodeGen *cg, ASTNode *expr) {
    if (!expr) return TYPE_INT;
    switch (expr->type) {
    case AST_INTEGER:
        a64_mov_imm(cg, "x0", expr->data.int_value);
        return TYPE_INT;
    case AST_BOOL:
        codegen_emit(cg,"    mov  x0, #%d", expr->data.bool_value ? 1 : 0);
        return TYPE_BOOL;
    case AST_FLOAT: {
        /* Emit float constant in rodata, load via adrp/ldr into d0,
           then fmov to x0 so the push/pop integer path works correctly. */
        int fid = cg->label_counter++;
        /* Intern the double bytes as a special string-slot so it ends up in rodata */
        /* We emit it directly in the .text as a data island with branch-over */
        codegen_emit(cg,"    b    __vl_flt_skip_%d", fid);
        codegen_emit(cg,"    .align 8");
        codegen_emit(cg,"__vl_flt_%d:", fid);
        codegen_emit(cg,"    .double %.17g", expr->data.float_lit.value);
        codegen_emit(cg,"__vl_flt_skip_%d:", fid);
        codegen_emit(cg,"    adr  x9, __vl_flt_%d", fid);
        codegen_emit(cg,"    ldr  d0, [x9]");
        return TYPE_F64;
    }
    case AST_STRING: {
        int sid = codegen_intern_string(cg, expr->data.string_lit.value);
        codegen_emit(cg,"    adrp x0, _VL_str_%d", sid);
        codegen_emit(cg,"    add  x0, x0, :lo12:_VL_str_%d", sid);
        return TYPE_STRING;
    }
    case AST_IDENTIFIER: {
        int idx = codegen_find_var(cg, expr->data.identifier);
        if (idx < 0) error_at(expr->line, expr->column,
                               "undefined variable '%s'", expr->data.identifier);
        int off = cg->var_offsets[idx];
        ValueType t = cg->var_types[idx];
        TypeInfo *ti = cg->var_typeinfo[idx];
        /* Reference param: load the pointer, then dereference */
        if (ti && ti->is_ref) {
            codegen_emit(cg,"    ldr  x0, [x29, #-%d]", off);
            codegen_emit(cg,"    ldr  x0, [x0]");
            return t;
        }
        if (t == TYPE_F64 || t == TYPE_F32) {
            codegen_emit(cg,"    ldr  d0, [x29, #-%d]", off);
            return t;
        }
        codegen_emit(cg,"    ldr  x0, [x29, #-%d]", off);
        return t;
    }
    case AST_BINARY_OP: {
        ValueType lt = codegen_expr_a64(cg, expr->data.binary.left);
        bool is_float_op = (lt == TYPE_F64 || lt == TYPE_F32);

        if (is_float_op) {
            /* Push d0 as raw bits via fmov */
            codegen_emit(cg,"    fmov x9, d0");
            codegen_emit(cg,"    str  x9, [sp, #-16]!");
        } else {
            codegen_emit(cg,"    str  x0, [sp, #-16]!");
        }

        ValueType rt = codegen_expr_a64(cg, expr->data.binary.right);
        bool is_float_r = (rt == TYPE_F64 || rt == TYPE_F32);

        if (is_float_op || is_float_r) {
            /* Both operands as doubles in d1 (right) and d0 (left) */
            if (is_float_r)
                codegen_emit(cg,"    fmov d1, d0");   /* right already in d0 */
            else {
                codegen_emit(cg,"    scvtf d1, x0");  /* convert int right to float */
            }
            codegen_emit(cg,"    ldr  x9, [sp], #16");
            if (is_float_op)
                codegen_emit(cg,"    fmov d0, x9");   /* restore left float */
            else
                codegen_emit(cg,"    scvtf d0, x9");  /* convert int left to float */

            switch (expr->data.binary.op) {
            case OP_ADD: codegen_emit(cg,"    fadd d0, d0, d1"); break;
            case OP_SUB: codegen_emit(cg,"    fsub d0, d0, d1"); break;
            case OP_MUL: codegen_emit(cg,"    fmul d0, d0, d1"); break;
            case OP_DIV: codegen_emit(cg,"    fdiv d0, d0, d1"); break;
            case OP_EQ:  codegen_emit(cg,"    fcmp d0, d1");
                         codegen_emit(cg,"    cset x0, eq"); return TYPE_INT;
            case OP_NE:  codegen_emit(cg,"    fcmp d0, d1");
                         codegen_emit(cg,"    cset x0, ne"); return TYPE_INT;
            case OP_LT:  codegen_emit(cg,"    fcmp d0, d1");
                         codegen_emit(cg,"    cset x0, mi"); return TYPE_INT;
            case OP_LE:  codegen_emit(cg,"    fcmp d0, d1");
                         codegen_emit(cg,"    cset x0, ls"); return TYPE_INT;
            case OP_GT:  codegen_emit(cg,"    fcmp d0, d1");
                         codegen_emit(cg,"    cset x0, gt"); return TYPE_INT;
            case OP_GE:  codegen_emit(cg,"    fcmp d0, d1");
                         codegen_emit(cg,"    cset x0, ge"); return TYPE_INT;
            default:     codegen_emit(cg,"    fadd d0, d0, d1"); break;
            }
            return TYPE_F64;
        }

        /* Integer path */
        codegen_emit(cg,"    mov  x1, x0");
        codegen_emit(cg,"    ldr  x0, [sp], #16");
        switch (expr->data.binary.op) {
        case OP_ADD:  codegen_emit(cg,"    add  x0, x0, x1"); break;
        case OP_SUB:  codegen_emit(cg,"    sub  x0, x0, x1"); break;
        case OP_MUL:  codegen_emit(cg,"    mul  x0, x0, x1"); break;
        case OP_DIV:  codegen_emit(cg,"    sdiv x0, x0, x1"); break;
        case OP_MOD:
            codegen_emit(cg,"    sdiv x2, x0, x1");
            codegen_emit(cg,"    msub x0, x2, x1, x0"); break;
        case OP_EQ:   codegen_emit(cg,"    cmp  x0, x1");
                      codegen_emit(cg,"    cset x0, eq"); break;
        case OP_NE:   codegen_emit(cg,"    cmp  x0, x1");
                      codegen_emit(cg,"    cset x0, ne"); break;
        case OP_LT:   codegen_emit(cg,"    cmp  x0, x1");
                      codegen_emit(cg,"    cset x0, lt"); break;
        case OP_LE:   codegen_emit(cg,"    cmp  x0, x1");
                      codegen_emit(cg,"    cset x0, le"); break;
        case OP_GT:   codegen_emit(cg,"    cmp  x0, x1");
                      codegen_emit(cg,"    cset x0, gt"); break;
        case OP_GE:   codegen_emit(cg,"    cmp  x0, x1");
                      codegen_emit(cg,"    cset x0, ge"); break;
        case OP_AND:  codegen_emit(cg,"    and  x0, x0, x1"); break;
        case OP_OR:   codegen_emit(cg,"    orr  x0, x0, x1"); break;
        case OP_BAND: codegen_emit(cg,"    and  x0, x0, x1"); break;
        case OP_BOR:  codegen_emit(cg,"    orr  x0, x0, x1"); break;
        case OP_BXOR: codegen_emit(cg,"    eor  x0, x0, x1"); break;
        case OP_SHL:  codegen_emit(cg,"    lsl  x0, x0, x1"); break;
        case OP_SHR:  codegen_emit(cg,"    asr  x0, x0, x1"); break;
        default: break;
        }
        return TYPE_INT;
    }
    case AST_UNARY_OP: {
        codegen_expr_a64(cg, expr->data.unary.operand);
        if (expr->data.unary.op == UOP_NOT)
            codegen_emit(cg,"    eor  x0, x0, #1");
        else if (expr->data.unary.op == UOP_NEG)
            codegen_emit(cg,"    neg  x0, x0");
        else if (expr->data.unary.op == UOP_BNOT)
            codegen_emit(cg,"    mvn  x0, x0");
        return TYPE_INT;
    }
    case AST_CALL: {
        int argc = expr->data.call.arg_count;
        const char *call_name = expr->data.call.func_name;

        /* ---- Builtins ---- */

        /* type(x) -> string name of type at compile time */
        if (strcmp(call_name,"type")==0 && argc==1) {
            ASTNode *arg = expr->data.call.args[0];
            ValueType at = infer_expr_type(cg, arg);
            codegen_expr_a64(cg, arg);   /* eval for side effects */
            int sid = codegen_intern_string(cg, value_type_name(at));
            codegen_emit(cg,"    adrp x0, _VL_str_%d", sid);
            codegen_emit(cg,"    add  x0, x0, :lo12:_VL_str_%d", sid);
            return TYPE_STRING;
        }

        /* sizeof(x) -> size in bytes */
        if (strcmp(call_name,"sizeof")==0 && argc==1) {
            ASTNode *arg = expr->data.call.args[0];
            ValueType at = infer_expr_type(cg, arg);
            TypeInfo *ti = expr_typeinfo(cg, arg);
            int sz = typeinfo_size(cg, at, ti);
            codegen_emit(cg,"    mov  x0, #%d", sz);
            return TYPE_INT;
        }

        /* len(x) -> integer length */
        if (strcmp(call_name,"len")==0 && argc==1) {
            ASTNode *arg = expr->data.call.args[0];
            ValueType at = codegen_expr_a64(cg, arg);
            TypeInfo *ti = expr_typeinfo(cg, arg);
            /* Also try to look up from var directly for better typeinfo */
            if (arg->type == AST_IDENTIFIER) {
                int vi = codegen_find_var(cg, arg->data.identifier);
                if (vi >= 0 && cg->var_typeinfo[vi]) ti = cg->var_typeinfo[vi];
                if (vi >= 0) at = cg->var_types[vi];
            }
            if (at==TYPE_STRING || at==TYPE_OPT_STRING) {
                int ll = codegen_new_label(cg), dl = codegen_new_label(cg);
                codegen_emit(cg,"    mov  x9, x0");
                codegen_emit(cg,"    mov  x10, #0");
                codegen_emit(cg,".L%d:", ll);
                codegen_emit(cg,"    ldrb w11, [x9, x10]");
                codegen_emit(cg,"    cbz  w11, .L%d", dl);
                codegen_emit(cg,"    add  x10, x10, #1");
                codegen_emit(cg,"    b    .L%d", ll);
                codegen_emit(cg,".L%d:", dl);
                codegen_emit(cg,"    mov  x0, x10");
                return TYPE_INT;
            }
            if (at==TYPE_ARRAY) {
                if (ti && ti->array_len >= 0) {
                    codegen_emit(cg,"    mov  x0, #%d", ti->array_len);
                } else {
                    codegen_emit(cg,"    ldr  x0, [x0, #-16]");
                }
                return TYPE_INT;
            }
            if (at==TYPE_TUPLE) {
                int n = (ti && ti->tuple_count > 0) ? ti->tuple_count : 0;
                codegen_emit(cg,"    mov  x0, #%d", n);
                return TYPE_INT;
            }
            /* Fallback: assume dynamic array with length header */
            codegen_emit(cg,"    ldr  x0, [x0, #-16]");
            return TYPE_INT;
        }

        /* panic(msg) -> print message and exit */
        if (strcmp(call_name,"panic")==0 && argc==1) {
            codegen_expr_a64(cg, expr->data.call.args[0]);
            /* x0 = message string — write to stderr then exit(1) */
            codegen_emit(cg,"    bl   __vl_strlen");
            codegen_emit(cg,"    mov  x2, x0");
            codegen_emit(cg,"    adrp x1, _VL_str_0");  /* fallback: use any string addr as buf ptr */
            /* Better: reload msg ptr — but we overwrote x0 with strlen result */
            /* Just call io__chhaap with a panic format */
            int psid = codegen_intern_string(cg, "panic: ");
            codegen_emit(cg,"    adrp x0, _VL_str_%d", psid);
            codegen_emit(cg,"    add  x0, x0, :lo12:_VL_str_%d", psid);
            codegen_emit(cg,"    bl   __vl_strlen");
            codegen_emit(cg,"    mov  x1, x0");
            codegen_emit(cg,"    mov  x0, #2");    /* stderr */
            codegen_emit(cg,"    mov  x8, #64");
            codegen_emit(cg,"    svc  #0");
            /* re-eval arg to get msg ptr again */
            codegen_expr_a64(cg, expr->data.call.args[0]);
            codegen_emit(cg,"    mov  x9, x0");
            codegen_emit(cg,"    bl   __vl_strlen");
            codegen_emit(cg,"    mov  x2, x0");
            codegen_emit(cg,"    mov  x1, x9");
            codegen_emit(cg,"    mov  x0, #2");
            codegen_emit(cg,"    mov  x8, #64");
            codegen_emit(cg,"    svc  #0");
            codegen_emit(cg,"    mov  x0, #1");
            codegen_emit(cg,"    mov  x8, #93");   /* exit(1) */
            codegen_emit(cg,"    svc  #0");
            return TYPE_VOID;
        }

        /* map(arr, |x| expr) / filter(arr, |x| bool_expr) */
        if ((strcmp(call_name,"map")==0 || strcmp(call_name,"filter")==0) && argc==2) {
            bool is_filter = (strcmp(call_name,"filter")==0);
            ASTNode *arr_expr = expr->data.call.args[0];
            ASTNode *lam_expr = expr->data.call.args[1];
            if (!lam_expr || lam_expr->type != AST_LAMBDA) {
                codegen_emit(cg,"    mov  x0, #0");
                return TYPE_ARRAY;
            }
            TypeInfo *arr_ti = expr_typeinfo(cg, arr_expr);
            if (arr_expr->type == AST_IDENTIFIER) {
                int vi = codegen_find_var(cg, arr_expr->data.identifier);
                if (vi >= 0 && cg->var_typeinfo[vi]) arr_ti = cg->var_typeinfo[vi];
            }
            ValueType elem_t = arr_ti ? arr_ti->elem_type : TYPE_INT;
            int lb = codegen_new_label(cg);

            /* Evaluate input array → x9 = data ptr */
            codegen_expr_a64(cg, arr_expr);
            codegen_add_var(cg, "_mi", TYPE_INT, true, NULL);
            int in_off = cg->stack_offset;
            codegen_emit(cg,"    str  x0, [x29, #-%d]", in_off);

            /* Get input length */
            codegen_add_var(cg, "_ml", TYPE_INT, true, NULL);
            int len_off = cg->stack_offset;
            if (arr_ti && arr_ti->array_len >= 0)
                codegen_emit(cg,"    mov  x0, #%d", arr_ti->array_len);
            else {
                codegen_emit(cg,"    ldr  x9, [x29, #-%d]", in_off);
                codegen_emit(cg,"    ldr  x0, [x9, #-16]");
            }
            codegen_emit(cg,"    str  x0, [x29, #-%d]", len_off);

            /* Allocate output array: mmap(len * 8 + 8) */
            codegen_emit(cg,"    ldr  x0, [x29, #-%d]", len_off);
            codegen_emit(cg,"    add  x1, x0, #1");
            codegen_emit(cg,"    lsl  x1, x1, #3");
            codegen_emit(cg,"    mov  x0, #0");
            codegen_emit(cg,"    mov  x2, #3");
            codegen_emit(cg,"    mov  x3, #0x22");
            codegen_emit(cg,"    mov  x4, #-1");
            codegen_emit(cg,"    mov  x5, #0");
            codegen_emit(cg,"    mov  x8, #222");
            codegen_emit(cg,"    svc  #0");
            codegen_add_var(cg, "_mo", TYPE_INT, true, NULL);
            int out_off = cg->stack_offset;
            codegen_add_var(cg, "_molen", TYPE_INT, true, NULL);
            int outlen_off = cg->stack_offset;
            codegen_emit(cg,"    str  x0, [x29, #-%d]", out_off);
            codegen_emit(cg,"    mov  x10, #0");
            codegen_emit(cg,"    str  x10, [x29, #-%d]", outlen_off);

            /* Loop index */
            codegen_add_var(cg, "_mx", TYPE_INT, true, NULL);
            int idx_off = cg->stack_offset;
            codegen_emit(cg,"    mov  x0, #0");
            codegen_emit(cg,"    str  x0, [x29, #-%d]", idx_off);

            /* Lambda param slot */
            int saved_count = cg->var_count;
            codegen_add_var(cg, lam_expr->data.lambda.param_name, elem_t, true, NULL);
            int param_off = cg->stack_offset;

            int lloop = codegen_new_label(cg);
            int lend  = codegen_new_label(cg);
            int lskip = codegen_new_label(cg);
            (void)lb; (void)saved_count;

            codegen_emit(cg,".L%d:", lloop);
            codegen_emit(cg,"    ldr  x0, [x29, #-%d]", idx_off);
            codegen_emit(cg,"    ldr  x1, [x29, #-%d]", len_off);
            codegen_emit(cg,"    cmp  x0, x1");
            codegen_emit(cg,"    b.ge .L%d", lend);

            /* Load element */
            codegen_emit(cg,"    ldr  x9, [x29, #-%d]", in_off);
            codegen_emit(cg,"    ldr  x10, [x29, #-%d]", idx_off);
            codegen_emit(cg,"    ldr  x0, [x9, x10, lsl #3]");
            codegen_emit(cg,"    str  x0, [x29, #-%d]", param_off);

            /* Evaluate lambda body */
            ValueType body_t = codegen_expr_a64(cg, lam_expr->data.lambda.body);
            if (body_t==TYPE_F64||body_t==TYPE_F32) codegen_emit(cg,"    fmov x0, d0");

            if (is_filter) {
                codegen_emit(cg,"    cbz  x0, .L%d", lskip);
                /* Append original element to output */
                codegen_emit(cg,"    ldr  x9, [x29, #-%d]", in_off);
                codegen_emit(cg,"    ldr  x10, [x29, #-%d]", idx_off);
                codegen_emit(cg,"    ldr  x11, [x9, x10, lsl #3]");
                codegen_emit(cg,"    ldr  x9, [x29, #-%d]", out_off);
                codegen_emit(cg,"    ldr  x10, [x29, #-%d]", outlen_off);
                codegen_emit(cg,"    str  x11, [x9, x10, lsl #3]");
                codegen_emit(cg,"    add  x10, x10, #1");
                codegen_emit(cg,"    str  x10, [x29, #-%d]", outlen_off);
                codegen_emit(cg,".L%d:", lskip);
            } else {
                /* Store mapped result */
                codegen_emit(cg,"    mov  x11, x0");
                codegen_emit(cg,"    ldr  x9, [x29, #-%d]", out_off);
                codegen_emit(cg,"    ldr  x10, [x29, #-%d]", idx_off);
                codegen_emit(cg,"    str  x11, [x9, x10, lsl #3]");
            }

            /* idx++ */
            codegen_emit(cg,"    ldr  x0, [x29, #-%d]", idx_off);
            codegen_emit(cg,"    add  x0, x0, #1");
            codegen_emit(cg,"    str  x0, [x29, #-%d]", idx_off);
            codegen_emit(cg,"    b    .L%d", lloop);
            codegen_emit(cg,".L%d:", lend);

            /* Set length header for output: [out-8] = len */
            codegen_emit(cg,"    ldr  x9, [x29, #-%d]", out_off);
            if (is_filter) {
                codegen_emit(cg,"    ldr  x10, [x29, #-%d]", outlen_off);
            } else {
                codegen_emit(cg,"    ldr  x10, [x29, #-%d]", len_off);
            }
            codegen_emit(cg,"    str  x10, [x9, #-16]");
            codegen_emit(cg,"    ldr  x0, [x29, #-%d]", out_off);

            TypeInfo *ret_ti = (TypeInfo*)calloc(1, sizeof(TypeInfo));
            ret_ti->kind = TYPE_ARRAY;
            ret_ti->array_len = -1;
            ret_ti->is_dynamic = true;
            ret_ti->elem_type = is_filter ? elem_t : body_t;
            (void)ret_ti;
            return TYPE_ARRAY;
        }
        /* ---- Generic function call ---- */
        FunctionSig *sig = func_sig_find(cg, call_name);
        /* If compiling as a module, prefix internal function calls.
           Don't prefix: already-prefixed, known builtins, io__*, lafz__* etc. */
        char prefixed_call[MAX_TOKEN_LEN * 2 + 4];
        if (cg->module_prefix[0]) {
            /* Check if already has THIS module's prefix */
            char own_prefix[MAX_TOKEN_LEN + 4];
            snprintf(own_prefix, sizeof(own_prefix), "%s__", cg->module_prefix);
            bool already_own = (strncmp(call_name, own_prefix, strlen(own_prefix)) == 0);
            /* Check if it's an external module call (contains __ from another module) */
            bool is_other_module = (!already_own && strchr(call_name, '_') &&
                                    call_name[0] != '_');
            /* Don't prefix builtins or external module symbols */
            bool is_builtin = (strcmp(call_name,"type")==0 ||
                               strcmp(call_name,"len")==0 ||
                               strcmp(call_name,"sizeof")==0 ||
                               strcmp(call_name,"panic")==0 ||
                               strcmp(call_name,"map")==0 ||
                               strcmp(call_name,"filter")==0 ||
                               strncmp(call_name,"io__",4)==0 ||
                               strncmp(call_name,"lafz__",6)==0 ||
                               strncmp(call_name,"util__",6)==0 ||
                               strncmp(call_name,"math__",6)==0 ||
                               call_name[0]=='_');
            if (!already_own && !is_builtin) {
                snprintf(prefixed_call, sizeof(prefixed_call), "%s__%s",
                         cg->module_prefix, call_name);
                call_name = prefixed_call;
            }
            (void)is_other_module;
        }
        for (int i = argc-1; i >= 0; i--) {
            bool is_ref = sig && sig->param_is_ref && i < sig->param_count
                          && sig->param_is_ref[i];
            if (is_ref && expr->data.call.args[i]->type == AST_IDENTIFIER) {
                int vi = codegen_find_var(cg, expr->data.call.args[i]->data.identifier);
                if (vi >= 0)
                    codegen_emit(cg,"    sub  x0, x29, #%d", cg->var_offsets[vi]);
            } else {
                ValueType at = codegen_expr_a64(cg, expr->data.call.args[i]);
                /* floats land in d0 — move bits to x0 for integer push */
                if (at == TYPE_F64 || at == TYPE_F32)
                    codegen_emit(cg,"    fmov x0, d0");
            }
            codegen_emit(cg,"    str  x0, [sp, #-16]!");
        }
        for (int i = 0; i < argc && i < 8; i++) {
            codegen_emit(cg,"    ldr  x%d, [sp], #16", i);
            /* Restore float args back to d registers for callee */
            if (sig && i < sig->param_count &&
                (sig->param_types[i]==TYPE_F64||sig->param_types[i]==TYPE_F32))
                codegen_emit(cg,"    fmov d%d, x%d", i, i);
        }
        codegen_emit(cg,"    bl   %s", call_name);
        return sig ? sig->return_type : TYPE_INT;
    }
    case AST_MODULE_CALL: {
        /* Map io.chhaap / io.chhaapLine etc. → Bionic libc printf/write */
        const char *mn = expr->data.module_call.module_name;
        const char *fn = expr->data.module_call.func_name;
        int argc = expr->data.module_call.arg_count;

        /* Route any non-io module to its symbol: module__func */
        if (strcmp(mn,"io") != 0) {
            char sym[MAX_TOKEN_LEN*2+4];
            snprintf(sym, sizeof(sym), "%s__%s", mn, fn);
            for (int i = argc-1; i >= 0; i--) {
                ValueType at = codegen_expr_a64(cg, expr->data.module_call.args[i]);
                if (at==TYPE_F64||at==TYPE_F32) codegen_emit(cg,"    fmov x0, d0");
                codegen_emit(cg,"    str  x0, [sp, #-16]!");
            }
            for (int i = 0; i < argc && i < 8; i++)
                codegen_emit(cg,"    ldr  x%d, [sp], #16", i);
            codegen_emit(cg,"    bl   %s", sym);
            return TYPE_INT;
        }

        if (strcmp(mn,"io")==0) {
            if (strcmp(fn,"chhaap")==0 || strcmp(fn,"chhaapLine")==0) {
                bool newline = (strcmp(fn,"chhaapLine")==0);
                if (argc == 0) {
                    /* bare chhaapLine() → newline only */
                    int sid = codegen_intern_string(cg, "\n");
                    codegen_emit(cg,"    adrp x0, _VL_str_%d", sid);
                    codegen_emit(cg,"    add  x0, x0, :lo12:_VL_str_%d", sid);
                    codegen_emit(cg,"    bl   __vl_strlen");
                    codegen_emit(cg,"    mov  x2, x0");
                    codegen_emit(cg,"    adrp x1, _VL_str_%d", sid);
                    codegen_emit(cg,"    add  x1, x1, :lo12:_VL_str_%d", sid);
                    codegen_emit(cg,"    mov  x0, #1");
                    codegen_emit(cg,"    mov  x8, #64");
                    codegen_emit(cg,"    svc  #0");
                    return TYPE_INT;
                }
                if (argc == 1) {
                    /* single value — auto-format */
                    ASTNode *arg = expr->data.module_call.args[0];
                    ValueType at = codegen_expr_a64(cg, arg);
                    if (at==TYPE_F64||at==TYPE_F32)
                        codegen_emit(cg,"    fmov x1, d0");
                    else
                        codegen_emit(cg,"    mov  x1, x0");
                    const char *fmt = "%lld";
                    if (at==TYPE_STRING) fmt="%s";
                    else if (at==TYPE_BOOL) fmt="%d";
                    else if (at==TYPE_F64||at==TYPE_F32) fmt="%f";
                    char fmtbuf[32];
                    if (newline) { snprintf(fmtbuf,sizeof(fmtbuf),"%s\n",fmt); fmt=fmtbuf; }
                    int sid = codegen_intern_string(cg, fmt);
                    codegen_emit(cg,"    adrp x0, _VL_str_%d", sid);
                    codegen_emit(cg,"    add  x0, x0, :lo12:_VL_str_%d", sid);
                    codegen_emit(cg,"    bl   io__chhaap");
                    return TYPE_INT;
                }
                /* multi-arg: x0=fmt, x1..x7=values → __vl_printf */
                for (int i = argc-1; i >= 0; i--) {
                    ValueType at = codegen_expr_a64(cg, expr->data.module_call.args[i]);
                    if (at==TYPE_F64||at==TYPE_F32)
                        codegen_emit(cg,"    fmov x0, d0");
                    codegen_emit(cg,"    str  x0, [sp, #-16]!");
                }
                for (int i = 0; i < argc && i < 8; i++)
                    codegen_emit(cg,"    ldr  x%d, [sp], #16", i);
                codegen_emit(cg,"    bl   __vl_printf");
                if (newline) {
                    int nlsid = codegen_intern_string(cg, "\n");
                    codegen_emit(cg,"    mov  x1, #0");
                    codegen_emit(cg,"    adrp x0, _VL_str_%d", nlsid);
                    codegen_emit(cg,"    add  x0, x0, :lo12:_VL_str_%d", nlsid);
                    codegen_emit(cg,"    bl   __vl_printf");
                }
                return TYPE_INT;
            }
            /* io.stdin() → string, io.input() → integer */
            if (strcmp(fn,"stdin")==0) {
                codegen_emit(cg,"    bl   io__stdin");
                return TYPE_STRING;
            }
            if (strcmp(fn,"input")==0) {
                codegen_emit(cg,"    bl   io__input");
                return TYPE_INT;
            }
            /* Other io functions — generic call */
            {
                char sym[MAX_TOKEN_LEN*2+4];
                snprintf(sym, sizeof(sym), "io__%s", fn);
                for (int i = argc-1; i >= 0; i--) {
                    ValueType at = codegen_expr_a64(cg, expr->data.module_call.args[i]);
                    if (at==TYPE_F64||at==TYPE_F32)
                        codegen_emit(cg,"    fmov x0, d0");
                    codegen_emit(cg,"    str  x0, [sp, #-16]!");
                }
                for (int i = 0; i < argc && i < 8; i++)
                    codegen_emit(cg,"    ldr  x%d, [sp], #16", i);
                codegen_emit(cg,"    bl   %s", sym);
            }
        }
        return TYPE_INT;
    }
    case AST_NULL:
        codegen_emit(cg,"    mov  x0, #0");
        return TYPE_INT;

    case AST_ARRAY_LITERAL: {
        /* Layout: [base]=len, [base+8]=cap, [base+16..]=data  → ptr=base+16
           __vl_arr_append: reads len at [base]=ptr[-16], cap at [base+8]=ptr[-8] */
        int count = expr->data.array_lit.count;
        int total = 16 + (count + 4) * 8;
        codegen_emit(cg,"    mov  x0, #0");
        codegen_emit(cg,"    mov  x1, #%d", total);
        codegen_emit(cg,"    mov  x2, #3");
        codegen_emit(cg,"    mov  x3, #0x22");
        codegen_emit(cg,"    mov  x4, #-1");
        codegen_emit(cg,"    mov  x5, #0");
        codegen_emit(cg,"    mov  x8, #222");
        codegen_emit(cg,"    svc  #0");
        codegen_emit(cg,"    mov  x9, x0");
        codegen_emit(cg,"    mov  x10, #%d", count);
        codegen_emit(cg,"    str  x10, [x9]");       /* len */
        codegen_emit(cg,"    mov  x10, #%d", count + 4);
        codegen_emit(cg,"    str  x10, [x9, #8]");   /* cap */
        codegen_emit(cg,"    add  x10, x9, #16");    /* data ptr */
        for (int i = 0; i < count; i++) {
            codegen_expr_a64(cg, expr->data.array_lit.elements[i]);
            if (expr->data.array_lit.count > 0)
                codegen_emit(cg,"    str  x0, [x10, #%d]", i * 8);
        }
        codegen_emit(cg,"    add  x0, x9, #16");
        return TYPE_ARRAY;
    }

    case AST_ARRAY_ACCESS: {
        codegen_expr_a64(cg, expr->data.array_access.index);
        codegen_emit(cg,"    str  x0, [sp, #-16]!");  /* save index */
        codegen_expr_a64(cg, expr->data.array_access.array);
        codegen_emit(cg,"    mov  x9, x0");           /* array ptr */
        codegen_emit(cg,"    ldr  x1, [sp], #16");    /* index */
        codegen_emit(cg,"    ldr  x0, [x9, x1, lsl #3]");
        return TYPE_INT;
    }

    case AST_STRUCT_LITERAL: {
        StructDef *sd = struct_find(cg, expr->data.struct_lit.struct_name);
        int sz = sd ? sd->field_count * 8 : 8;
        int total_s = sz + 8;
        codegen_emit(cg,"    mov  x0, #0");
        codegen_emit(cg,"    mov  x1, #%d", total_s);
        codegen_emit(cg,"    mov  x2, #3");
        codegen_emit(cg,"    mov  x3, #0x22");
        codegen_emit(cg,"    mov  x4, #-1");
        codegen_emit(cg,"    mov  x5, #0");
        codegen_emit(cg,"    mov  x8, #222");
        codegen_emit(cg,"    svc  #0");
        /* Save struct base ptr in a local var so field evals can't clobber it */
        char sname[32]; snprintf(sname, sizeof(sname), "_sl%d", codegen_new_label(cg));
        codegen_add_var(cg, sname, TYPE_INT, true, NULL);
        int soff = cg->stack_offset;
        codegen_emit(cg,"    str  x0, [x29, #-%d]", soff);
        /* Set each named field, reloading base ptr each time */
        for (int i = 0; i < expr->data.struct_lit.field_count; i++) {
            int foff = 0;
            if (sd) {
                for (int j = 0; j < sd->field_count; j++) {
                    if (strcmp(sd->field_names[j],
                               expr->data.struct_lit.field_names[i])==0) break;
                    foff += 8;
                }
            } else foff = i * 8;
            ValueType fvt = codegen_expr_a64(cg, expr->data.struct_lit.field_values[i]);
            if (fvt==TYPE_F64||fvt==TYPE_F32) codegen_emit(cg,"    fmov x0, d0");
            /* Reload base ptr — may have been clobbered by nested struct eval */
            codegen_emit(cg,"    ldr  x9, [x29, #-%d]", soff);
            codegen_emit(cg,"    str  x0, [x9, #%d]", foff);
        }
        codegen_emit(cg,"    ldr  x0, [x29, #-%d]", soff);
        return TYPE_STRUCT;
    }

    case AST_FIELD_ACCESS: {
        ASTNode *obj = expr->data.field_access.base;
        codegen_expr_a64(cg, obj);
        codegen_emit(cg,"    mov  x9, x0");
        /* Determine field offset — try to find struct def from obj's type */
        int foff = 0;
        StructDef *sd = NULL;

        /* Try variable TypeInfo */
        if (obj->type == AST_IDENTIFIER) {
            int vi = codegen_find_var(cg, obj->data.identifier);
            if (vi >= 0) {
                TypeInfo *ti2 = cg->var_typeinfo[vi];
                if (ti2 && ti2->struct_name[0])
                    sd = struct_find(cg, ti2->struct_name);
                /* If array element, look at elem_typeinfo */
                if (!sd && ti2 && ti2->kind == TYPE_ARRAY && ti2->elem_typeinfo)
                    sd = struct_find(cg, ti2->elem_typeinfo->struct_name);
            }
        }
        /* Try array access — the base is table[i] and we need Entry's struct def */
        if (!sd && obj->type == AST_ARRAY_ACCESS) {
            ASTNode *arr = obj->data.array_access.array;
            if (arr->type == AST_IDENTIFIER) {
                int vi = codegen_find_var(cg, arr->data.identifier);
                if (vi >= 0) {
                    TypeInfo *ti2 = cg->var_typeinfo[vi];
                    /* elem_typeinfo should have struct info */
                    if (ti2 && ti2->elem_typeinfo && ti2->elem_typeinfo->struct_name[0])
                        sd = struct_find(cg, ti2->elem_typeinfo->struct_name);
                    /* Also try declared element type stored in TypeInfo */
                    if (!sd && ti2 && ti2->struct_name[0])
                        sd = struct_find(cg, ti2->struct_name);
                }
            }
        }
        /* Try nested field access — e.g. o.pos.x where pos is a Vec2 */
        if (!sd && obj->type == AST_FIELD_ACCESS) {
            /* Need the type of the intermediate field */
            ASTNode *outer_obj = obj->data.field_access.base;
            if (outer_obj->type == AST_IDENTIFIER) {
                int vi = codegen_find_var(cg, outer_obj->data.identifier);
                if (vi >= 0) {
                    TypeInfo *ti2 = cg->var_typeinfo[vi];
                    StructDef *outer_sd = ti2 ? struct_find(cg, ti2->struct_name) : NULL;
                    if (outer_sd) {
                        /* find the field type for obj->field_access.field_name */
                        for (int j = 0; j < outer_sd->field_count; j++) {
                            if (strcmp(outer_sd->field_names[j],
                                       obj->data.field_access.field_name)==0) {
                                TypeInfo *fti = outer_sd->field_typeinfo[j];
                                if (fti && fti->struct_name[0])
                                    sd = struct_find(cg, fti->struct_name);
                                break;
                            }
                        }
                    }
                }
            }
        }

        if (sd) {
            for (int j = 0; j < sd->field_count; j++) {
                if (strcmp(sd->field_names[j], expr->data.field_access.field_name)==0) break;
                foff += 8;
            }
        }
        codegen_emit(cg,"    ldr  x0, [x9, #%d]", foff);
        return TYPE_INT;
    }

    case AST_TUPLE_LITERAL: {
        int n = expr->data.tuple_lit.count;
        int tsz = (n+1)*8;
        codegen_emit(cg,"    mov  x0, #0");
        codegen_emit(cg,"    mov  x1, #%d", tsz);
        codegen_emit(cg,"    mov  x2, #3");
        codegen_emit(cg,"    mov  x3, #0x22");
        codegen_emit(cg,"    mov  x4, #-1");
        codegen_emit(cg,"    mov  x5, #0");
        codegen_emit(cg,"    mov  x8, #222");
        codegen_emit(cg,"    svc  #0");
        codegen_emit(cg,"    mov  x9, x0");
        for (int i = 0; i < n; i++) {
            ValueType et = codegen_expr_a64(cg, expr->data.tuple_lit.elements[i]);
            if (et==TYPE_F64||et==TYPE_F32) codegen_emit(cg,"    fmov x0, d0");
            codegen_emit(cg,"    str  x0, [x9, #%d]", i*8);
        }
        codegen_emit(cg,"    mov  x0, x9");
        return TYPE_TUPLE;
    }

    case AST_TUPLE_ACCESS: {
        codegen_expr_a64(cg, expr->data.tuple_access.tuple);
        codegen_emit(cg,"    ldr  x0, [x0, #%d]", expr->data.tuple_access.index * 8);
        return TYPE_INT;
    }

    default:
        codegen_emit(cg,"    mov  x0, #0");
        return TYPE_INT;
    }
}

static void codegen_stmt_a64(CodeGen *cg, ASTNode *stmt) {
    if (!stmt) return;
    switch (stmt->type) {

    /* ---- variable declaration ---- */
    case AST_LET: {
        /* discard pattern */
        if (strcmp(stmt->data.let.var_name, "_") == 0 ||
            strcmp(stmt->data.let.var_name, "_discard") == 0) {
            if (stmt->data.let.value) codegen_expr_a64(cg, stmt->data.let.value);
            break;
        }
        ASTNode *val = stmt->data.let.value;
        ValueType vt = val ? codegen_expr_a64(cg, val) : TYPE_INT;
        ValueType decl = stmt->data.let.has_type ? stmt->data.let.var_type : vt;
        if (!stmt->data.let.has_type && val && val->type == AST_INTEGER) decl = TYPE_INT;

        /* Inherit TypeInfo from struct/array literal so field access works */
        TypeInfo *ti = stmt->data.let.type_info;
        if (!ti && val) {
            if (val->type == AST_STRUCT_LITERAL) {
                ti = (TypeInfo*)calloc(1, sizeof(TypeInfo));
                ti->kind = TYPE_STRUCT;
                strncpy(ti->struct_name, val->data.struct_lit.struct_name, MAX_TOKEN_LEN-1);
                decl = TYPE_STRUCT;
            } else if (val->type == AST_ARRAY_LITERAL) {
                ti = (TypeInfo*)calloc(1, sizeof(TypeInfo));
                ti->kind = TYPE_ARRAY;
                ti->array_len = val->data.array_lit.count;
                ti->is_dynamic = false;
                decl = TYPE_ARRAY;
            }
        }

        codegen_add_var(cg, stmt->data.let.var_name, decl,
                        stmt->data.let.is_mut, ti);
        int off = cg->stack_offset;
        if (decl == TYPE_F64 || decl == TYPE_F32) {
            if (vt != TYPE_F64 && vt != TYPE_F32)
                codegen_emit(cg,"    scvtf d0, x0");
            codegen_emit(cg,"    str  d0, [x29, #-%d]", off);
        } else {
            if (vt == TYPE_F64 || vt == TYPE_F32)
                codegen_emit(cg,"    fmov x0, d0");
            codegen_emit(cg,"    str  x0, [x29, #-%d]", off);
        }
        break;
    }

    /* ---- assignment ---- */
    case AST_ASSIGN: {
        /* discard: _ = expr */
        if (strcmp(stmt->data.assign.var_name, "_discard") == 0) {
            if (stmt->data.assign.value) codegen_expr_a64(cg, stmt->data.assign.value);
            break;
        }
        int idx = codegen_find_var(cg, stmt->data.assign.var_name);
        if (idx < 0) error_at(stmt->line, stmt->column,
                               "undefined variable '%s'", stmt->data.assign.var_name);
        ValueType decl = cg->var_types[idx];
        TypeInfo *ti = cg->var_typeinfo[idx];
        bool is_ref_v = ti && ti->is_ref;
        if (!cg->var_mutable[idx] && !is_ref_v)
            error_at(stmt->line, stmt->column,
                     "cannot assign to immutable '%s'", stmt->data.assign.var_name);
        ValueType vt = codegen_expr_a64(cg, stmt->data.assign.value);
        int off = cg->var_offsets[idx];
        if (is_ref_v) {
            codegen_emit(cg,"    ldr  x1, [x29, #-%d]", off);
            if (decl==TYPE_F64||decl==TYPE_F32)
                codegen_emit(cg,"    str  d0, [x1]");
            else
                codegen_emit(cg,"    str  x0, [x1]");
        } else if (decl==TYPE_F64||decl==TYPE_F32) {
            if (vt!=TYPE_F64&&vt!=TYPE_F32) codegen_emit(cg,"    scvtf d0, x0");
            codegen_emit(cg,"    str  d0, [x29, #-%d]", off);
        } else {
            if (vt==TYPE_F64||vt==TYPE_F32) codegen_emit(cg,"    fmov x0, d0");
            codegen_emit(cg,"    str  x0, [x29, #-%d]", off);
        }
        break;
    }

    /* ---- compound assign +=, -=, *=, /=, %=, &=, |=, ^=, <<=, >>= ---- */
    case AST_COMPOUND_ASSIGN: {
        int idx = codegen_find_var(cg, stmt->data.compound_assign.var_name);
        if (idx < 0) error_at(stmt->line, stmt->column,
                               "undefined variable '%s'",
                               stmt->data.compound_assign.var_name);
        TypeInfo *ti = cg->var_typeinfo[idx];
        bool is_ref_v = ti && ti->is_ref;
        if (!cg->var_mutable[idx] && !is_ref_v)
            error_at(stmt->line, stmt->column,
                     "cannot assign to immutable '%s'",
                     stmt->data.compound_assign.var_name);
        ValueType decl = cg->var_types[idx];
        int off = cg->var_offsets[idx];
        BinaryOp op = stmt->data.compound_assign.op;

        /* Load LHS into x9 (int) or d1 (float) — use x9 to avoid clobbering x0-x8 */
        if (is_ref_v) {
            codegen_emit(cg,"    ldr  x9, [x29, #-%d]", off);
            if (decl==TYPE_F64||decl==TYPE_F32)
                codegen_emit(cg,"    ldr  d1, [x9]");
            else
                codegen_emit(cg,"    ldr  x9, [x9]");
        } else {
            if (decl==TYPE_F64||decl==TYPE_F32)
                codegen_emit(cg,"    ldr  d1, [x29, #-%d]", off);
            else
                codegen_emit(cg,"    ldr  x9, [x29, #-%d]", off);
        }
        /* Save LHS on stack (x9 is caller-saved, will be trashed by RHS eval) */
        if (decl==TYPE_F64||decl==TYPE_F32) {
            codegen_emit(cg,"    str  d1, [sp, #-16]!");
        } else {
            codegen_emit(cg,"    str  x9, [sp, #-16]!");
        }

        /* Eval RHS */
        ValueType vt = codegen_expr_a64(cg, stmt->data.compound_assign.value);

        /* Reload LHS from stack */
        if (decl==TYPE_F64||decl==TYPE_F32) {
            codegen_emit(cg,"    ldr  d1, [sp], #16");
            /* Ensure RHS is float */
            if (vt!=TYPE_F64&&vt!=TYPE_F32) codegen_emit(cg,"    scvtf d0, x0");
            else if (vt==TYPE_F32&&decl==TYPE_F64) codegen_emit(cg,"    fcvt d0, s0");
            switch(op) {
            case OP_ADD: codegen_emit(cg,"    fadd d0, d1, d0"); break;
            case OP_SUB: codegen_emit(cg,"    fsub d0, d1, d0"); break;
            case OP_MUL: codegen_emit(cg,"    fmul d0, d1, d0"); break;
            case OP_DIV: codegen_emit(cg,"    fdiv d0, d1, d0"); break;
            default:     codegen_emit(cg,"    fadd d0, d1, d0"); break;
            }
            if (is_ref_v) {
                codegen_emit(cg,"    ldr  x1, [x29, #-%d]", off);
                codegen_emit(cg,"    str  d0, [x1]");
            } else {
                codegen_emit(cg,"    str  d0, [x29, #-%d]", off);
            }
        } else {
            codegen_emit(cg,"    ldr  x9, [sp], #16");
            /* RHS is in x0 */
            if (vt==TYPE_F64||vt==TYPE_F32) codegen_emit(cg,"    fmov x0, d0");
            switch(op) {
            case OP_ADD:  codegen_emit(cg,"    add  x0, x9, x0"); break;
            case OP_SUB:  codegen_emit(cg,"    sub  x0, x9, x0"); break;
            case OP_MUL:  codegen_emit(cg,"    mul  x0, x9, x0"); break;
            case OP_DIV:  codegen_emit(cg,"    sdiv x0, x9, x0"); break;
            case OP_MOD:  codegen_emit(cg,"    sdiv x1, x9, x0");
                          codegen_emit(cg,"    msub x0, x1, x0, x9"); break;
            case OP_BAND: codegen_emit(cg,"    and  x0, x9, x0"); break;
            case OP_BOR:  codegen_emit(cg,"    orr  x0, x9, x0"); break;
            case OP_BXOR: codegen_emit(cg,"    eor  x0, x9, x0"); break;
            case OP_SHL:  codegen_emit(cg,"    lsl  x0, x9, x0"); break;
            case OP_SHR:  codegen_emit(cg,"    asr  x0, x9, x0"); break;
            default:      codegen_emit(cg,"    add  x0, x9, x0"); break;
            }
            if (is_ref_v) {
                codegen_emit(cg,"    ldr  x1, [x29, #-%d]", off);
                codegen_emit(cg,"    str  x0, [x1]");
            } else {
                codegen_emit(cg,"    str  x0, [x29, #-%d]", off);
            }
        }
        break;
    }

    /* ---- return ---- */
    case AST_RETURN:
        if (stmt->data.ret.value) {
            ValueType vt = codegen_expr_a64(cg, stmt->data.ret.value);
            if (vt==TYPE_F64||vt==TYPE_F32)
                codegen_emit(cg,"    fmov x0, d0");
        }
        codegen_emit(cg,"    add  sp, sp, #4096");
        codegen_emit(cg,"    ldp  x29, x30, [sp], #16");
        codegen_emit(cg,"    ret");
        break;

    /* ---- if / else ---- */
    case AST_IF: {
        int lelse = codegen_new_label(cg);
        int lend  = codegen_new_label(cg);
        codegen_expr_a64(cg, stmt->data.if_stmt.condition);
        codegen_emit(cg,"    cbz  x0, .L%d", lelse);
        for (int i=0;i<stmt->data.if_stmt.then_count;i++)
            codegen_stmt_a64(cg, stmt->data.if_stmt.then_stmts[i]);
        if (stmt->data.if_stmt.else_count > 0)
            codegen_emit(cg,"    b    .L%d", lend);
        codegen_emit(cg,".L%d:", lelse);
        for (int i=0;i<stmt->data.if_stmt.else_count;i++)
            codegen_stmt_a64(cg, stmt->data.if_stmt.else_stmts[i]);
        if (stmt->data.if_stmt.else_count > 0)
            codegen_emit(cg,".L%d:", lend);
        break;
    }

    /* ---- while ---- */
    case AST_WHILE: {
        int lstart = codegen_new_label(cg);
        int lend   = codegen_new_label(cg);
        if (cg->loop_depth < MAX_LOOP_DEPTH) {
            cg->loop_continue_labels[cg->loop_depth] = lstart;
            cg->loop_break_labels[cg->loop_depth]    = lend;
            cg->loop_depth++;
        }
        codegen_emit(cg,".L%d:", lstart);
        codegen_expr_a64(cg, stmt->data.while_stmt.condition);
        codegen_emit(cg,"    cbz  x0, .L%d", lend);
        for (int i=0;i<stmt->data.while_stmt.body_count;i++)
            codegen_stmt_a64(cg, stmt->data.while_stmt.body[i]);
        codegen_emit(cg,"    b    .L%d", lstart);
        codegen_emit(cg,".L%d:", lend);
        if (cg->loop_depth > 0) cg->loop_depth--;
        break;
    }

    /* ---- for-in range / array loop ---- */
    case AST_FOR_IN: {
        ASTNode *range = stmt->data.for_in_stmt.iterable;
        const char *var = stmt->data.for_in_stmt.var_name;
        int lstart = codegen_new_label(cg);
        int lcont  = codegen_new_label(cg);
        int lend   = codegen_new_label(cg);

        if (range && range->type == AST_RANGE) {
            /* Range loop: for i manz start..end (..= inclusive) */
            codegen_expr_a64(cg, range->data.range.start);
            codegen_add_var(cg, var, TYPE_INT, true, NULL);
            int ioff = cg->stack_offset;
            codegen_emit(cg,"    str  x0, [x29, #-%d]", ioff);

            /* Compute end */
            codegen_expr_a64(cg, range->data.range.end);
            codegen_add_var(cg, "_a64_end", TYPE_INT, true, NULL);
            int eoff = cg->stack_offset;
            codegen_emit(cg,"    str  x0, [x29, #-%d]", eoff);

            /* Compute step (default 1) */
            int soff = 0;
            if (range->data.range.step) {
                codegen_expr_a64(cg, range->data.range.step);
                codegen_add_var(cg, "_a64_step", TYPE_INT, true, NULL);
                soff = cg->stack_offset;
                codegen_emit(cg,"    str  x0, [x29, #-%d]", soff);
            }

            bool inclusive = range->data.range.inclusive;
            if (cg->loop_depth < MAX_LOOP_DEPTH) {
                cg->loop_continue_labels[cg->loop_depth] = lcont;
                cg->loop_break_labels[cg->loop_depth]    = lend;
                cg->loop_depth++;
            }

            codegen_emit(cg,".L%d:", lstart);
            codegen_emit(cg,"    ldr  x0, [x29, #-%d]", ioff);
            codegen_emit(cg,"    ldr  x1, [x29, #-%d]", eoff);
            if (inclusive)
                codegen_emit(cg,"    cmp  x0, x1");
            else
                codegen_emit(cg,"    cmp  x0, x1");
            codegen_emit(cg,inclusive ? "    b.gt .L%d" : "    b.ge .L%d", lend);

            for (int i=0;i<stmt->data.for_in_stmt.body_count;i++)
                codegen_stmt_a64(cg, stmt->data.for_in_stmt.body[i]);

            /* increment */
            codegen_emit(cg,".L%d:", lcont);
            codegen_emit(cg,"    ldr  x0, [x29, #-%d]", ioff);
            if (soff) {
                codegen_emit(cg,"    ldr  x1, [x29, #-%d]", soff);
                codegen_emit(cg,"    add  x0, x0, x1");
            } else {
                codegen_emit(cg,"    add  x0, x0, #1");
            }
            codegen_emit(cg,"    str  x0, [x29, #-%d]", ioff);
            codegen_emit(cg,"    b    .L%d", lstart);
            codegen_emit(cg,".L%d:", lend);
            if (cg->loop_depth > 0) cg->loop_depth--;
        } else {
            /* Array iteration */
            codegen_expr_a64(cg, range);
            TypeInfo *ati = expr_typeinfo(cg, range);
            codegen_add_var(cg, "_a64_arr", TYPE_INT, true, NULL);
            int arroff = cg->stack_offset;
            codegen_emit(cg,"    str  x0, [x29, #-%d]", arroff);

            /* Get length */
            codegen_add_var(cg, "_a64_len", TYPE_INT, true, NULL);
            int lenoff = cg->stack_offset;
            if (ati && ati->array_len >= 0) {
                codegen_emit(cg,"    mov  x0, #%d", ati->array_len);
            } else {
                codegen_emit(cg,"    ldr  x1, [x29, #-%d]", arroff);
                codegen_emit(cg,"    ldr  x0, [x1, #-16]");
            }
            codegen_emit(cg,"    str  x0, [x29, #-%d]", lenoff);

            /* Index var */
            codegen_emit(cg,"    mov  x0, #0");
            codegen_add_var(cg, "_a64_idx", TYPE_INT, true, NULL);
            int idxoff = cg->stack_offset;
            codegen_emit(cg,"    str  x0, [x29, #-%d]", idxoff);

            /* Element var */
            ValueType elt = ati ? ati->elem_type : TYPE_INT;
            codegen_emit(cg,"    mov  x0, #0");
            codegen_add_var(cg, var, elt, true, ati ? ati->elem_typeinfo : NULL);
            int varoff = cg->stack_offset;
            codegen_emit(cg,"    str  x0, [x29, #-%d]", varoff);

            if (cg->loop_depth < MAX_LOOP_DEPTH) {
                cg->loop_continue_labels[cg->loop_depth] = lcont;
                cg->loop_break_labels[cg->loop_depth]    = lend;
                cg->loop_depth++;
            }
            codegen_emit(cg,".L%d:", lstart);
            codegen_emit(cg,"    ldr  x0, [x29, #-%d]", idxoff);
            codegen_emit(cg,"    ldr  x1, [x29, #-%d]", lenoff);
            codegen_emit(cg,"    cmp  x0, x1");
            codegen_emit(cg,"    b.ge .L%d", lend);

            /* Load element */
            codegen_emit(cg,"    ldr  x9, [x29, #-%d]", arroff);
            codegen_emit(cg,"    ldr  x1, [x29, #-%d]", idxoff);
            codegen_emit(cg,"    ldr  x0, [x9, x1, lsl #3]");
            codegen_emit(cg,"    str  x0, [x29, #-%d]", varoff);

            for (int i=0;i<stmt->data.for_in_stmt.body_count;i++)
                codegen_stmt_a64(cg, stmt->data.for_in_stmt.body[i]);

            codegen_emit(cg,".L%d:", lcont);
            codegen_emit(cg,"    ldr  x0, [x29, #-%d]", idxoff);
            codegen_emit(cg,"    add  x0, x0, #1");
            codegen_emit(cg,"    str  x0, [x29, #-%d]", idxoff);
            codegen_emit(cg,"    b    .L%d", lstart);
            codegen_emit(cg,".L%d:", lend);
            if (cg->loop_depth > 0) cg->loop_depth--;
        }
        break;
    }

    /* ---- C-style for(init; cond; step) ---- */
    case AST_FOR: {
        int lstart = codegen_new_label(cg);
        int lcont  = codegen_new_label(cg);
        int lend   = codegen_new_label(cg);
        if (stmt->data.for_stmt.init)
            codegen_stmt_a64(cg, stmt->data.for_stmt.init);
        if (cg->loop_depth < MAX_LOOP_DEPTH) {
            cg->loop_continue_labels[cg->loop_depth] = lcont;
            cg->loop_break_labels[cg->loop_depth]    = lend;
            cg->loop_depth++;
        }
        codegen_emit(cg,".L%d:", lstart);
        if (stmt->data.for_stmt.condition) {
            codegen_expr_a64(cg, stmt->data.for_stmt.condition);
            codegen_emit(cg,"    cbz  x0, .L%d", lend);
        }
        for (int i=0;i<stmt->data.for_stmt.body_count;i++)
            codegen_stmt_a64(cg, stmt->data.for_stmt.body[i]);
        codegen_emit(cg,".L%d:", lcont);
        if (stmt->data.for_stmt.post)
            codegen_stmt_a64(cg, stmt->data.for_stmt.post);
        codegen_emit(cg,"    b    .L%d", lstart);
        codegen_emit(cg,".L%d:", lend);
        if (cg->loop_depth > 0) cg->loop_depth--;
        break;
    }

    case AST_BREAK:
        if (cg->loop_depth > 0)
            codegen_emit(cg,"    b    .L%d", cg->loop_break_labels[cg->loop_depth-1]);
        break;
    case AST_CONTINUE:
        if (cg->loop_depth > 0)
            codegen_emit(cg,"    b    .L%d", cg->loop_continue_labels[cg->loop_depth-1]);
        break;

    /* ---- array element assign: arr[i] = val ---- */
    case AST_ARRAY_ASSIGN: {
        codegen_expr_a64(cg, stmt->data.array_assign.value);
        codegen_emit(cg,"    str  x0, [sp, #-16]!");   /* save value */
        codegen_expr_a64(cg, stmt->data.array_assign.index);
        codegen_emit(cg,"    str  x0, [sp, #-16]!");   /* save index */
        int aidx = codegen_find_var(cg, stmt->data.array_assign.var_name);
        if (aidx < 0) error_at(stmt->line, stmt->column,
                               "undefined array '%s'", stmt->data.array_assign.var_name);
        TypeInfo *ati = cg->var_typeinfo[aidx];
        bool dyn = ati && (ati->array_len < 0 || ati->is_dynamic);
        codegen_emit(cg,"    ldr  x9, [x29, #-%d]", cg->var_offsets[aidx]);
        if (dyn) codegen_emit(cg,"    ldr  x9, [x9]"); /* dyn arr: stored ptr */
        codegen_emit(cg,"    ldr  x1, [sp], #16");     /* index */
        codegen_emit(cg,"    ldr  x0, [sp], #16");     /* value */
        codegen_emit(cg,"    str  x0, [x9, x1, lsl #3]");
        break;
    }

    /* ---- field assign: s.field = val ---- */
    case AST_FIELD_ASSIGN: {
        codegen_expr_a64(cg, stmt->data.field_assign.value);
        codegen_emit(cg,"    str  x0, [sp, #-16]!");
        int sidx = codegen_find_var(cg, stmt->data.field_assign.var_name);
        if (sidx < 0) error_at(stmt->line, stmt->column,
                               "undefined variable '%s'", stmt->data.field_assign.var_name);
        TypeInfo *sti = cg->var_typeinfo[sidx];
        StructDef *sd = sti ? struct_find(cg, sti->struct_name) : NULL;
        int field_off = 0;
        if (sd) {
            for (int i = 0; i < sd->field_count; i++) {
                if (strcmp(sd->field_names[i], stmt->data.field_assign.field_name)==0) break;
                field_off += 8;
            }
        }
        codegen_emit(cg,"    ldr  x9, [x29, #-%d]", cg->var_offsets[sidx]);
        codegen_emit(cg,"    ldr  x0, [sp], #16");
        codegen_emit(cg,"    str  x0, [x9, #%d]", field_off);
        break;
    }

    /* ---- try/catch ---- */
    case AST_TRY_CATCH: {
        for (int i=0;i<stmt->data.try_catch.try_count;i++)
            codegen_stmt_a64(cg, stmt->data.try_catch.try_body[i]);
        break;
    }

    /* ---- panic ---- */
    case AST_PANIC: {
        if (stmt->data.panic_stmt.message) {
            codegen_expr_a64(cg, stmt->data.panic_stmt.message);
            codegen_emit(cg,"    mov  x9, x0");
            codegen_emit(cg,"    bl   __vl_strlen");
            codegen_emit(cg,"    mov  x2, x0");
            codegen_emit(cg,"    mov  x1, x9");
            codegen_emit(cg,"    mov  x0, #2");
            codegen_emit(cg,"    mov  x8, #64");
            codegen_emit(cg,"    svc  #0");
        }
        codegen_emit(cg,"    mov  x0, #1");
        codegen_emit(cg,"    mov  x8, #93");
        codegen_emit(cg,"    svc  #0");
        break;
    }

    /* ---- append: arr.append(val) ---- */
    case AST_APPEND: {
        /* Eval value first */
        ValueType vt = codegen_expr_a64(cg, stmt->data.append_stmt.value);
        if (vt==TYPE_F64||vt==TYPE_F32) codegen_emit(cg,"    fmov x0, d0");
        codegen_emit(cg,"    str  x0, [sp, #-16]!");   /* save elem */
        /* Get address of the array variable (pointer to the data ptr) */
        int aidx = codegen_find_var(cg, stmt->data.append_stmt.arr_name);
        if (aidx < 0) error_at(stmt->line, stmt->column,
                               "undefined array '%s'", stmt->data.append_stmt.arr_name);
        if (!cg->var_mutable[aidx])
            error_at(stmt->line, stmt->column,
                     "cannot append to immutable '%s'", stmt->data.append_stmt.arr_name);
        /* x0 = address of the variable (so append can update the ptr) */
        codegen_emit(cg,"    sub  x0, x29, #%d", cg->var_offsets[aidx]);
        codegen_emit(cg,"    ldr  x1, [sp], #16");     /* elem */
        codegen_emit(cg,"    bl   __vl_arr_append");
        break;
    }

    case AST_CALL:
    case AST_MODULE_CALL:
        codegen_expr_a64(cg, stmt);
        break;

    default:
        /* silently skip unknown stmt types rather than emitting broken comment */
        break;
    }
}


static void codegen_function_aarch64(CodeGen *cg, ASTNode *func) {
    /* Skip main() when compiling as a module */
    bool is_module = cg->module_prefix[0] != '\0';
    if (is_module && strcmp(func->data.function.name, "main")==0) return;

    /* Reset per-function state */
    for (int i = 0; i < cg->var_count; i++) free(cg->local_vars[i]);
    cg->var_count    = 0;
    cg->stack_offset = 0;
    cg->loop_depth   = 0;
    cg->try_depth    = 0;
    cg->current_return_type     = func->data.function.return_type;
    cg->current_return_typeinfo = func->data.function.return_typeinfo;

    /* Local frame: 16-byte aligned, up to 4096 bytes of locals */
    int local_size = 4096;
    local_size = (local_size + 15) & ~15;

    /* Build the emitted function name (prefixed if compiling as module) */
    char emit_name[MAX_TOKEN_LEN * 2 + 4];
    if (cg->module_prefix[0]) {
        /* Check if the function name already starts with prefix__ (e.g. math__abs in math.vel) */
        char own_pfx[MAX_TOKEN_LEN + 4];
        snprintf(own_pfx, sizeof(own_pfx), "%s__", cg->module_prefix);
        if (strncmp(func->data.function.name, own_pfx, strlen(own_pfx)) == 0)
            strncpy(emit_name, func->data.function.name, sizeof(emit_name)-1);
        else
            snprintf(emit_name, sizeof(emit_name), "%s__%s",
                     cg->module_prefix, func->data.function.name);
    } else
        strncpy(emit_name, func->data.function.name, sizeof(emit_name)-1);

    codegen_emit(cg,"// ----- %s -----", emit_name);
    codegen_emit(cg,".global %s", emit_name);
    codegen_emit(cg,"%s:", emit_name);
    /* stp offset must be in [-512, 504] — use -16 for fp/lr, then sub sp for locals */
    codegen_emit(cg,"    stp  x29, x30, [sp, #-16]!");
    codegen_emit(cg,"    mov  x29, sp");
    codegen_emit(cg,"    sub  sp, sp, #%d", local_size);

    /* Store params: x0..x7 → stack slots below x29 */
    for (int i = 0; i < func->data.function.param_count && i < 8; i++) {
        ValueType pt  = func->data.function.param_types[i];
        TypeInfo *pti = func->data.function.param_typeinfo
                        ? func->data.function.param_typeinfo[i] : NULL;
        bool is_ref   = func->data.function.param_is_ref
                        && func->data.function.param_is_ref[i];
        if (is_ref && pti) { pti = typeinfo_clone(pti); pti->is_ref = true; }
        codegen_add_var(cg, func->data.function.param_names[i], pt, true, pti);
        codegen_emit(cg,"    str  %s, [x29, #-%d]", A64_REGS[i], cg->stack_offset);
    }

    /* Emit body */
    for (int i = 0; i < func->data.function.body_count; i++)
        codegen_stmt_a64(cg, func->data.function.body[i]);

    /* Implicit void return */
    codegen_emit(cg,"    add  sp, sp, #%d", local_size);
    codegen_emit(cg,"    ldp  x29, x30, [sp], #16");
    codegen_emit(cg,"    ret");
    codegen_emit(cg,"");
}

void codegen_program(CodeGen *cg, ASTNode *program) {
    /* ---- AArch64 path: GNU as syntax, Bionic libc ---- */
    if (cg->target_aarch64) {
        codegen_emit(cg, "    .arch armv8-a");
        codegen_emit(cg, "    .text");
        codegen_emit(cg, "");

        /* Pre-register stdlib sigs for io module */
        {
            ValueType ti = TYPE_INT, ts = TYPE_STRING;
            ValueType p1i[1]={ti}, p1s[1]={ts}, p2si[2]={ts,ti};
            func_sig_add(cg,"io__chhaap",   ti,NULL,NULL,NULL,0);
            func_sig_add(cg,"io__chhaapLine",ti,NULL,NULL,NULL,0);
            func_sig_add(cg,"io__input",    ti,NULL,NULL,NULL,0);
            func_sig_add(cg,"io__stdin",    ts,NULL,NULL,NULL,0);
            func_sig_add(cg,"io__open",     ti,NULL,p2si,NULL,2);
            func_sig_add(cg,"io__close",    ti,NULL,p1i,NULL,1);
            ValueType p_read[3]={ti,ts,ti};
            func_sig_add(cg,"io__read",     ti,NULL,p_read,NULL,3);
            func_sig_add(cg,"io__write",    ti,NULL,p_read,NULL,3);
            (void)p1s;
        }

        /* Register user struct defs */
        codegen_register_structs(cg, program);

        /* Register user function signatures (non-generic) */
        for (int i = 0; i < program->data.program.function_count; i++) {
            ASTNode *fn = program->data.program.functions[i];
            if (fn->data.function.generic_count > 0) continue;
            func_sig_add(cg, fn->data.function.name,
                         fn->data.function.return_type,
                         fn->data.function.return_typeinfo,
                         fn->data.function.param_types,
                         fn->data.function.param_typeinfo,
                         fn->data.function.param_count);
            FunctionSig *sig = func_sig_find(cg, fn->data.function.name);
            if (sig && fn->data.function.param_is_ref && sig->param_count > 0) {
                sig->param_is_ref = (bool*)calloc(sig->param_count, sizeof(bool));
                for (int j = 0; j < sig->param_count; j++)
                    sig->param_is_ref[j] = fn->data.function.param_is_ref[j];
            }
        }


        /* ---- AArch64: monomorphize generics then emit all functions ---- */
        {
            /* Collect generic templates */
            ASTNode **gtmpls = (ASTNode**)calloc(256, sizeof(ASTNode*));
            int gnb = 0;
            for (int i = 0; i < program->data.program.function_count; i++) {
                ASTNode *fn = program->data.program.functions[i];
                if (fn->data.function.generic_count > 0 && gnb < 256)
                    gtmpls[gnb++] = fn;
            }
            if (gnb > 0) {
                /* Scan call sites */
                typedef struct { char fname[MAX_TOKEN_LEN]; ASTNode *cnode; } CSa;
                CSa *csa = (CSa*)calloc(2048, sizeof(CSa)); int ncs = 0;
                ASTNode **wk = (ASTNode**)malloc(65536*sizeof(ASTNode*)); int wtop=0;
                for (int i=0;i<program->data.program.function_count;i++){
                    ASTNode *fn=program->data.program.functions[i];
                    if(fn->data.function.generic_count>0) continue;
                    for(int j=0;j<fn->data.function.body_count&&wtop<65530;j++) wk[wtop++]=fn->data.function.body[j];
                }
                while(wtop>0){
                    ASTNode *nd=wk[--wtop]; if(!nd||wtop>=65530) continue;
                    if(nd->type==AST_CALL){
                        for(int k=0;k<gnb;k++) if(strcmp(gtmpls[k]->data.function.name,nd->data.call.func_name)==0&&ncs<2048){csa[ncs].cnode=nd;strncpy(csa[ncs].fname,nd->data.call.func_name,MAX_TOKEN_LEN-1);ncs++;break;}
                        for(int a=0;a<nd->data.call.arg_count&&wtop<65530;a++) wk[wtop++]=nd->data.call.args[a];
                    }
                    switch(nd->type){
                        case AST_BINARY_OP: if(wtop<65534){wk[wtop++]=nd->data.binary.left;wk[wtop++]=nd->data.binary.right;} break;
                        case AST_IF: if(wtop<65530){wk[wtop++]=nd->data.if_stmt.condition;for(int j=0;j<nd->data.if_stmt.then_count&&wtop<65530;j++) wk[wtop++]=nd->data.if_stmt.then_stmts[j];for(int j=0;j<nd->data.if_stmt.else_count&&wtop<65530;j++) wk[wtop++]=nd->data.if_stmt.else_stmts[j];} break;
                        case AST_WHILE: if(wtop<65530){wk[wtop++]=nd->data.while_stmt.condition;for(int j=0;j<nd->data.while_stmt.body_count&&wtop<65530;j++) wk[wtop++]=nd->data.while_stmt.body[j];} break;
                        case AST_LET: if(nd->data.let.value&&wtop<65535) wk[wtop++]=nd->data.let.value; break;
                        case AST_ASSIGN: if(nd->data.assign.value&&wtop<65535) wk[wtop++]=nd->data.assign.value; break;
                        case AST_RETURN: if(nd->data.ret.value&&wtop<65535) wk[wtop++]=nd->data.ret.value; break;
                        /* Push module call args so nested generic calls are found */
                        case AST_MODULE_CALL: for(int a=0;a<nd->data.module_call.arg_count&&wtop<65530;a++) wk[wtop++]=nd->data.module_call.args[a]; break;
                        default: break;
                    }
                }
                free(wk);
                char (*inst)[MAX_TOKEN_LEN*3]=(char(*)[MAX_TOKEN_LEN*3])calloc(512,MAX_TOKEN_LEN*3); int ni=0;
                for(int ci=0;ci<ncs;ci++){
                    ASTNode *call=csa[ci].cnode;
                    ASTNode *tmpl=NULL; for(int k=0;k<gnb;k++) if(strcmp(gtmpls[k]->data.function.name,call->data.call.func_name)==0){tmpl=gtmpls[k];break;}
                    if(!tmpl) continue;
                    int ng=tmpl->data.function.generic_count;
                    ValueType ctypes[8]; char cnames[8][32];
                    for(int g=0;g<ng;g++){ctypes[g]=TYPE_INT;strcpy(cnames[g],"adad");}
                    for(int a=0;a<call->data.call.arg_count&&a<tmpl->data.function.param_count;a++){
                        TypeInfo *pti=tmpl->data.function.param_typeinfo?tmpl->data.function.param_typeinfo[a]:NULL;
                        if(!pti||!pti->generic_name[0]) continue;
                        for(int g=0;g<ng;g++){
                            if(strcmp(pti->generic_name,tmpl->data.function.generic_params[g])!=0) continue;
                            ASTNode *arg=call->data.call.args[a];
                            if(arg->type==AST_FLOAT){ctypes[g]=TYPE_F64;strcpy(cnames[g],"ashari");}
                            else if(arg->type==AST_IDENTIFIER){int vi=codegen_find_var(cg,arg->data.identifier);if(vi>=0){ValueType vt=cg->var_types[vi];if(vt==TYPE_F64){ctypes[g]=TYPE_F64;strcpy(cnames[g],"ashari");}else{ctypes[g]=TYPE_INT;strcpy(cnames[g],"adad");}}}
                            break;
                        }
                    }
                    char mangled[MAX_TOKEN_LEN*3]; snprintf(mangled,sizeof(mangled),"%s",tmpl->data.function.name);
                    for(int g=0;g<ng;g++){char pp[64];snprintf(pp,sizeof(pp),"__%s",cnames[g]);strncat(mangled,pp,sizeof(mangled)-strlen(mangled)-1);}
                    strncpy(call->data.call.func_name,mangled,MAX_TOKEN_LEN-1);
                    bool already=false; for(int k=0;k<ni;k++) if(strcmp(inst[k],mangled)==0){already=true;break;}
                    if(already) continue;
                    if(ni<512) strncpy(inst[ni++],mangled,MAX_TOKEN_LEN*3-1);
                    int pc=tmpl->data.function.param_count;
                    ValueType *cpt=(ValueType*)malloc(sizeof(ValueType)*pc);
                    TypeInfo **cpti=(TypeInfo**)calloc(pc,sizeof(TypeInfo*));
                    bool *cpref=(bool*)calloc(pc,sizeof(bool));
                    for(int p=0;p<pc;p++){
                        TypeInfo *op=tmpl->data.function.param_typeinfo?tmpl->data.function.param_typeinfo[p]:NULL;
                        if(op&&op->generic_name[0]){int g2=-1;for(int g=0;g<ng;g++) if(strcmp(op->generic_name,tmpl->data.function.generic_params[g])==0){g2=g;break;} cpt[p]=(g2>=0)?ctypes[g2]:tmpl->data.function.param_types[p];}
                        else{cpt[p]=tmpl->data.function.param_types[p];cpti[p]=typeinfo_clone(op);}
                        if(tmpl->data.function.param_is_ref) cpref[p]=tmpl->data.function.param_is_ref[p];
                    }
                    ValueType cr=tmpl->data.function.return_type;
                    TypeInfo *ori=tmpl->data.function.return_typeinfo;
                    if(ori&&ori->generic_name[0]){int g2=-1;for(int g=0;g<ng;g++) if(strcmp(ori->generic_name,tmpl->data.function.generic_params[g])==0){g2=g;break;} cr=(g2>=0)?ctypes[g2]:cr;}
                    func_sig_add(cg,mangled,cr,NULL,cpt,cpti,pc);
                    FunctionSig *gs=func_sig_find(cg,mangled); if(gs) gs->param_is_ref=cpref;
                    ASTNode *cfn=ast_node_new(AST_FUNCTION);
                    strncpy(cfn->data.function.name,mangled,MAX_TOKEN_LEN-1);
                    cfn->data.function.generic_count=0; cfn->data.function.is_exported=true;
                    cfn->data.function.body=tmpl->data.function.body; cfn->data.function.body_count=tmpl->data.function.body_count;
                    cfn->data.function.param_count=pc; cfn->data.function.param_names=tmpl->data.function.param_names;
                    cfn->data.function.param_types=cpt; cfn->data.function.param_typeinfo=cpti;
                    cfn->data.function.param_is_ref=cpref; cfn->data.function.return_type=cr;
                    cfn->data.function.return_typeinfo=NULL; cfn->line=tmpl->line; cfn->column=tmpl->column;
                    codegen_function_aarch64(cg,cfn);
                    cfn->data.function.body=NULL; cfn->data.function.param_names=NULL;
                    cfn->data.function.param_types=NULL; cfn->data.function.param_typeinfo=NULL;
                    cfn->data.function.param_is_ref=NULL; cfn->data.function.body_count=0; cfn->data.function.param_count=0;
                    ast_node_free(cfn); free(cpt); free(cpti);
                }
                free(inst); free(csa);
            }
            free(gtmpls);
        }

        /* Emit non-generic user functions */
        for (int i = 0; i < program->data.program.function_count; i++) {
            ASTNode *fn = program->data.program.functions[i];
            if (fn->data.function.generic_count > 0) continue;
            codegen_function_aarch64(cg, fn);
        }

        /* _start entry — only for executables, not library modules */
        if (!cg->module_prefix[0]) {
            codegen_emit(cg, "    .global _start");
            codegen_emit(cg, "_start:");
            codegen_emit(cg, "    bl   main");
            codegen_emit(cg, "    mov  x0, #0");
            codegen_emit(cg, "    mov  x8, #93");
            codegen_emit(cg, "    svc  #0");
            codegen_emit(cg, "");
        }

        /* String literals in .rodata */
        if (cg->string_count > 0) {
            codegen_emit(cg, "    .section .rodata");
            for (int i = 0; i < cg->string_count; i++) {
                fprintf(cg->output, "_VL_str_%d:\n    .asciz \"", i);
                const char *s = cg->string_literals[i];
                for (const char *p = s; *p; p++) {
                    if      (*p=='\n') fprintf(cg->output,"\\n");
                    else if (*p=='\t') fprintf(cg->output,"\\t");
                    else if (*p=='"')  fprintf(cg->output,"\\\"");
                    else if (*p=='\\') fprintf(cg->output,"\\\\");
                    else               fputc(*p, cg->output);
                }
                fprintf(cg->output,"\"\n\n");
            }
        }
        return;   /* AArch64 path complete — skip x86-64 entirely */
    }
    /* ---- x86-64 path (NASM) ---- */
    if (!cg->target_aarch64) {
    const char *target_str = cg->target_windows
                             ? "Windows PE64 (MS x64 ABI)"
                             : "Linux ELF64 (System V AMD64 ABI)";
    codegen_emit(cg,"; Velocity Compiler v2 - Kashmiri Edition");
    codegen_emit(cg,"; Target: %s", target_str);
    codegen_emit(cg,"");
    codegen_emit(cg,"section .text");
    codegen_emit(cg,"");

    /* Register struct definitions */
    codegen_register_structs(cg, program);

    /* Register user function signatures */
    for (int i = 0; i < program->data.program.function_count; i++) {
        ASTNode *fn = program->data.program.functions[i];
        /* Skip generic templates - they are instantiated on demand */
        if (fn->data.function.generic_count > 0) continue;
        func_sig_add(cg, fn->data.function.name,
                     fn->data.function.return_type,
                     fn->data.function.return_typeinfo,
                     fn->data.function.param_types,
                     fn->data.function.param_typeinfo,
                     fn->data.function.param_count);
        /* Record ref flags */
        FunctionSig *sig = func_sig_find(cg, fn->data.function.name);
        if (sig && fn->data.function.param_is_ref && sig->param_count > 0) {
            sig->param_is_ref = (bool*)calloc(sig->param_count, sizeof(bool));
            for (int j = 0; j < sig->param_count; j++)
                sig->param_is_ref[j] = fn->data.function.param_is_ref[j];
        }
    }

    /* Process imports */
    for (int i = 0; i < program->data.program.import_count; i++) {
        const char *mod = program->data.program.imports[i]->data.import.module_name;
        register_builtin_module_sigs(cg, mod);
        if (!cg->module_mgr) continue;

        char *path = module_manager_find_module(cg->module_mgr, mod);
        if (!path) {
            fprintf(stderr, "warning: module '%s' not found\n", mod);
            continue;
        }
        const char *ext = strrchr(path, '.');
        if (ext && strcmp(ext, ".asm")==0) {
            codegen_emit_native_externs(cg, mod, path);
        } else {
            char *asm_path = module_manager_find_asm(cg->module_mgr, mod);
            if (asm_path) {
                codegen_emit_native_externs(cg, mod, asm_path);
                free(asm_path);
            }
            codegen_emit_module(cg, mod, path);
        }
        free(path);
    }

    } /* end if(!target_aarch64) — NASM header + externs + module emit done */

    /* ---------------------------------------------------------------
     * GENERIC MONOMORPHIZATION  (shared: x86-64 and AArch64)
     * codegen_function() already dispatches to the right backend.
     * --------------------------------------------------------------- */

    /* 1. Collect generic templates */
    ASTNode **generic_templates = (ASTNode**)calloc(256, sizeof(ASTNode*));
    int generic_tmpl_count = 0;
    for (int i = 0; i < program->data.program.function_count; i++) {
        ASTNode *fn = program->data.program.functions[i];
        if (fn->data.function.generic_count > 0 && generic_tmpl_count < 256)
            generic_templates[generic_tmpl_count++] = fn;
    }

    if (generic_tmpl_count > 0) {
        /* 2. Collect call sites using a heap-allocated work stack */
        typedef struct { char fname[MAX_TOKEN_LEN]; ASTNode *call_node; } CallSite;
        CallSite *call_sites = (CallSite*)calloc(2048, sizeof(CallSite));
        int call_site_count = 0;

        ASTNode **work = (ASTNode**)malloc(sizeof(ASTNode*) * 65536);
        int wtop = 0;

        /* Seed: all statements in all non-generic functions */
        for (int i = 0; i < program->data.program.function_count; i++) {
            ASTNode *fn = program->data.program.functions[i];
            if (fn->data.function.generic_count > 0) continue;
            for (int j = 0; j < fn->data.function.body_count && wtop < 65530; j++)
                work[wtop++] = fn->data.function.body[j];
        }

        /* Walk the AST iteratively */
        while (wtop > 0) {
            ASTNode *nd = work[--wtop];
            if (!nd || wtop >= 65530) continue;

            /* Check if this is a call to a generic template */
            const char *cname = NULL;
            ASTNode **cargs = NULL;
            int cargc = 0;
            if (nd->type == AST_CALL) {
                cname = nd->data.call.func_name;
                cargs = nd->data.call.args;
                cargc = nd->data.call.arg_count;
            }
            if (cname) {
                for (int k = 0; k < generic_tmpl_count; k++) {
                    if (strcmp(generic_templates[k]->data.function.name, cname)==0
                        && call_site_count < 2048) {
                        call_sites[call_site_count].call_node = nd;
                        strncpy(call_sites[call_site_count].fname,
                                cname, MAX_TOKEN_LEN-1);
                        call_site_count++;
                        break;
                    }
                }
                for (int a = 0; a < cargc && wtop < 65530; a++)
                    work[wtop++] = cargs[a];
            }

            /* Push child nodes */
            switch (nd->type) {
                case AST_BINARY_OP:
                    work[wtop++] = nd->data.binary.left;
                    work[wtop++] = nd->data.binary.right;
                    break;
                case AST_UNARY_OP:
                    work[wtop++] = nd->data.unary.operand;
                    break;
                case AST_IF:
                    work[wtop++] = nd->data.if_stmt.condition;
                    for (int j=0;j<nd->data.if_stmt.then_count&&wtop<65530;j++)
                        work[wtop++] = nd->data.if_stmt.then_stmts[j];
                    for (int j=0;j<nd->data.if_stmt.else_count&&wtop<65530;j++)
                        work[wtop++] = nd->data.if_stmt.else_stmts[j];
                    break;
                case AST_WHILE:
                    work[wtop++] = nd->data.while_stmt.condition;
                    for (int j=0;j<nd->data.while_stmt.body_count&&wtop<65530;j++)
                        work[wtop++] = nd->data.while_stmt.body[j];
                    break;
                case AST_FOR_IN:
                    for (int j=0;j<nd->data.for_in_stmt.body_count&&wtop<65530;j++)
                        work[wtop++] = nd->data.for_in_stmt.body[j];
                    break;
                case AST_LET:
                    if (nd->data.let.value) work[wtop++] = nd->data.let.value;
                    break;
                case AST_ASSIGN:
                    if (nd->data.assign.value) work[wtop++] = nd->data.assign.value;
                    break;
                case AST_RETURN:
                    if (nd->data.ret.value) work[wtop++] = nd->data.ret.value;
                    break;
                default: break;
            }
        }
        free(work);

        /* 3. Track instantiations to avoid duplicates */
        char (*instantiated)[MAX_TOKEN_LEN*3] =
            (char(*)[MAX_TOKEN_LEN*3])calloc(512, MAX_TOKEN_LEN*3);
        int inst_count = 0;

        /* 4. For each call site → derive types → emit once */
        for (int ci = 0; ci < call_site_count; ci++) {
            ASTNode *call = call_sites[ci].call_node;

            /* Find template */
            ASTNode *tmpl = NULL;
            for (int k = 0; k < generic_tmpl_count; k++)
                if (strcmp(generic_templates[k]->data.function.name,
                           call->data.call.func_name)==0)
                    { tmpl = generic_templates[k]; break; }
            if (!tmpl) continue;

            int gcount = tmpl->data.function.generic_count;

            /* Infer concrete type for each type param from call arguments */
            ValueType concrete_types[8];
            char concrete_names[8][32];
            for (int g = 0; g < gcount; g++) {
                concrete_types[g] = TYPE_INT;
                strcpy(concrete_names[g], "adad");
            }
            for (int a = 0; a < call->data.call.arg_count
                             && a < tmpl->data.function.param_count; a++) {
                TypeInfo *pti = tmpl->data.function.param_typeinfo
                                ? tmpl->data.function.param_typeinfo[a] : NULL;
                if (!pti || pti->generic_name[0] == '\0') continue;
                for (int g = 0; g < gcount; g++) {
                    if (strcmp(pti->generic_name,
                               tmpl->data.function.generic_params[g]) != 0) continue;
                    ASTNode *arg = call->data.call.args[a];
                    if (arg->type == AST_FLOAT) {
                        concrete_types[g] = TYPE_F64;
                        strcpy(concrete_names[g], "ashari");
                    } else if (arg->type == AST_IDENTIFIER) {
                        int vidx = codegen_find_var(cg, arg->data.identifier);
                        if (vidx >= 0) {
                            ValueType vt = cg->var_types[vidx];
                            if      (vt==TYPE_F64)    { concrete_types[g]=TYPE_F64;    strcpy(concrete_names[g],"ashari");   }
                            else if (vt==TYPE_F32)    { concrete_types[g]=TYPE_F32;    strcpy(concrete_names[g],"ashari32"); }
                            else if (vt==TYPE_STRING) { concrete_types[g]=TYPE_STRING; strcpy(concrete_names[g],"lafz");     }
                            else if (vt==TYPE_BOOL)   { concrete_types[g]=TYPE_BOOL;   strcpy(concrete_names[g],"bool");     }
                            else                      { concrete_types[g]=TYPE_INT;    strcpy(concrete_names[g],"adad");     }
                        }
                    }
                    /* TYPE_INT / AST_INTEGER: default adad already set */
                    break;
                }
            }

            /* Build mangled name: fname__adad or fname__adad__ashari */
            char mangled[MAX_TOKEN_LEN*3];
            snprintf(mangled, sizeof(mangled), "%s", tmpl->data.function.name);
            for (int g = 0; g < gcount; g++) {
                char part[64];
                snprintf(part, sizeof(part), "__%s", concrete_names[g]);
                strncat(mangled, part, sizeof(mangled)-strlen(mangled)-1);
            }

            /* Patch the call node name in place */
            strncpy(call->data.call.func_name, mangled, MAX_TOKEN_LEN-1);

            /* Skip if already instantiated */
            bool already = false;
            for (int k = 0; k < inst_count; k++)
                if (strcmp(instantiated[k], mangled)==0) { already=true; break; }
            if (already) continue;
            if (inst_count < 512)
                strncpy(instantiated[inst_count++], mangled, MAX_TOKEN_LEN*3-1);

            /* Build concrete param types by substituting T → concrete */
            int pcount = tmpl->data.function.param_count;
            ValueType *cptypes = (ValueType*)malloc(sizeof(ValueType)*pcount);
            TypeInfo **cpti    = (TypeInfo**)calloc(pcount, sizeof(TypeInfo*));
            bool      *cpref   = (bool*)calloc(pcount, sizeof(bool));
            for (int p = 0; p < pcount; p++) {
                TypeInfo *opti = tmpl->data.function.param_typeinfo
                                 ? tmpl->data.function.param_typeinfo[p] : NULL;
                if (opti && opti->generic_name[0]) {
                    int g2 = -1;
                    for (int g=0;g<gcount;g++)
                        if (strcmp(opti->generic_name,
                                   tmpl->data.function.generic_params[g])==0)
                            { g2=g; break; }
                    cptypes[p] = (g2>=0) ? concrete_types[g2]
                                         : tmpl->data.function.param_types[p];
                    cpti[p] = NULL;
                } else {
                    cptypes[p] = tmpl->data.function.param_types[p];
                    cpti[p] = typeinfo_clone(opti);
                }
                if (tmpl->data.function.param_is_ref)
                    cpref[p] = tmpl->data.function.param_is_ref[p];
            }

            /* Concrete return type */
            TypeInfo *orti = tmpl->data.function.return_typeinfo;
            ValueType cret = tmpl->data.function.return_type;
            TypeInfo *crti = NULL;
            if (orti && orti->generic_name[0]) {
                int g2 = -1;
                for (int g=0;g<gcount;g++)
                    if (strcmp(orti->generic_name,
                               tmpl->data.function.generic_params[g])==0)
                        { g2=g; break; }
                cret = (g2>=0) ? concrete_types[g2] : cret;
            } else {
                crti = typeinfo_clone(orti);
            }

            /* Register signature before emitting */
            func_sig_add(cg, mangled, cret, crti, cptypes, cpti, pcount);
            FunctionSig *gsig = func_sig_find(cg, mangled);
            if (gsig) {
                gsig->param_is_ref = cpref;
                for (int g=0;g<gcount;g++) {
                    strncpy(gsig->generic_params[g],
                            tmpl->data.function.generic_params[g], MAX_TOKEN_LEN-1);
                }
                gsig->generic_count = gcount;
            }

            /* Build and emit a concrete function node (shares body/names with template) */
            ASTNode *cfn = ast_node_new(AST_FUNCTION);
            strncpy(cfn->data.function.name, mangled, MAX_TOKEN_LEN-1);
            cfn->data.function.generic_count  = 0;
            cfn->data.function.is_exported    = true;
            cfn->data.function.body           = tmpl->data.function.body;
            cfn->data.function.body_count     = tmpl->data.function.body_count;
            cfn->data.function.param_count    = pcount;
            cfn->data.function.param_names    = tmpl->data.function.param_names;
            cfn->data.function.param_types    = cptypes;
            cfn->data.function.param_typeinfo = cpti;
            cfn->data.function.param_is_ref   = cpref;
            cfn->data.function.return_type    = cret;
            cfn->data.function.return_typeinfo = crti;
            cfn->line   = tmpl->line;
            cfn->column = tmpl->column;

            codegen_function(cg, cfn);

            /* Null shared pointers before freeing the wrapper */
            cfn->data.function.body           = NULL;
            cfn->data.function.param_names    = NULL;
            cfn->data.function.param_types    = NULL;
            cfn->data.function.param_typeinfo = NULL;
            cfn->data.function.param_is_ref   = NULL;
            cfn->data.function.return_typeinfo = NULL;
            cfn->data.function.body_count      = 0;
            cfn->data.function.param_count     = 0;
            ast_node_free(cfn);
        }

        free(instantiated);
        free(call_sites);
    }
    free(generic_templates);

    /* Emit non-generic user functions */
    for (int i = 0; i < program->data.program.function_count; i++) {
        ASTNode *fn = program->data.program.functions[i];
        if (fn->data.function.generic_count > 0) continue; /* skip templates */
        codegen_function(cg, fn);
    }

    /* Entry point */
    codegen_emit(cg,"; ----- entry point -----");
    if (cg->target_windows) {
        /* Windows: link against CRT, use mainCRTStartup or just define main */
        codegen_emit(cg,"global main");
        codegen_emit(cg,"extern GetCommandLineA");
        codegen_emit(cg,"extern ExitProcess");
        codegen_emit(cg,"main:");
        codegen_emit(cg,"    push rbp");
        codegen_emit(cg,"    mov  rbp, rsp");
        codegen_emit(cg,"    sub  rsp, 64");
        /* argc in rcx, argv in rdx on Windows main */
        codegen_emit(cg,"    mov [rel _VL_argc], rcx");
        codegen_emit(cg,"    mov [rel _VL_argv], rdx");
        codegen_emit(cg,"    xor rax, rax");
        codegen_emit(cg,"    mov [rel _VL_envp], rax");
        codegen_emit(cg,"    sub rsp, 32");
        codegen_emit(cg,"    call main_vel");
        codegen_emit(cg,"    add rsp, 32");
        codegen_emit(cg,"    mov rcx, rax");
        codegen_emit(cg,"    sub rsp, 32");
        codegen_emit(cg,"    call ExitProcess");
        codegen_emit(cg,"    add rsp, 32");
        codegen_emit(cg,"    pop rbp");
        codegen_emit(cg,"    ret");
        codegen_emit(cg,"");
        /* The actual velocity main() is called main_vel to avoid collision */
        /* Rename: we output a trampoline; the user's 'main' becomes 'main_vel' */
        /* Actually we just alias: emit an extern-free rename note */
        /* Simpler: just have the velocity 'main' function export as 'main_vel'
           and the trampoline calls it.
           The codegen_function already uses func->data.function.name,
           so we handle this in main.c by renaming 'main' to 'main_vel' on Windows. */
    } else {
        /* Linux: _start directly */
        codegen_emit(cg,"global _start");
        codegen_emit(cg,"_start:");
        codegen_emit(cg,"    mov rbx, rsp");
        codegen_emit(cg,"    mov rax, [rbx]");
        codegen_emit(cg,"    mov [rel _VL_argc], rax");
        codegen_emit(cg,"    lea rcx, [rbx + 8]");
        codegen_emit(cg,"    mov [rel _VL_argv], rcx");
        codegen_emit(cg,"    mov rdx, rax");
        codegen_emit(cg,"    add rdx, 1");
        codegen_emit(cg,"    shl rdx, 3");
        codegen_emit(cg,"    add rcx, rdx");
        codegen_emit(cg,"    mov [rel _VL_envp], rcx");
        codegen_emit(cg,"    call main");
        codegen_emit(cg,"    mov  rdi, rax");
        codegen_emit(cg,"    mov  rax, 60");
        codegen_emit(cg,"    syscall");
    }

    if (!cg->target_aarch64) {
        emit_runtime_globals(cg);
        emit_float_literals(cg);
        emit_string_literals(cg);
        if (cg->needs_arr_alloc || cg->needs_arr_append)
            codegen_emit_arr_append(cg);
        if (cg->needs_panic)
            codegen_emit_panic_helper(cg);
    }

    /* AArch64 tail: _start entry + rodata string section */
    if (cg->target_aarch64) {
        codegen_emit(cg, "    .global _start");
        codegen_emit(cg, "_start:");
        codegen_emit(cg, "    bl   main");
        codegen_emit(cg, "    mov  x0, #0");
        codegen_emit(cg, "    mov  x8, #93");
        codegen_emit(cg, "    svc  #0");
        codegen_emit(cg, "");

        if (cg->string_count > 0) {
            codegen_emit(cg, "    .section .rodata");
            for (int i = 0; i < cg->string_count; i++) {
                fprintf(cg->output, "_VL_str_%d:\n    .asciz \"", i);
                const char *s = cg->string_literals[i];
                for (const char *p = s; *p; p++) {
                    if      (*p=='\n') fprintf(cg->output,"\\n");
                    else if (*p=='\t') fprintf(cg->output,"\\t");
                    else if (*p=='"')  fprintf(cg->output,"\\\"");
                    else if (*p=='\\') fprintf(cg->output,"\\\\");
                    else               fputc(*p, cg->output);
                }
                fprintf(cg->output,"\"\n");
            }
        }
    }
}
