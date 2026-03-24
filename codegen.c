/*
 * Velocity Compiler - Code Generator (Kashmiri Edition v2)
 * Generates x86-64 NASM assembly.
 *
 * Key design:
 *   cg->current_module  is set to the module name while compiling a
 *   module's functions.  Any plain AST_CALL whose callee exists in the
 *   same module is automatically mangled to "module__callee" so that
 *   intra-module calls (e.g. mushtarak_mutaaddid calling mushtarak_qasim)
 *   resolve correctly.
 */

#include "velocity.h"
#include <stdarg.h>

typedef struct FloatLit {
    double value;
    ValueType type;
} FloatLit;

#define LOCAL_STACK_RESERVE 1024

static bool is_float(ValueType t) { return t == TYPE_F32 || t == TYPE_F64; }
static bool is_string_like(ValueType t) { return t == TYPE_STRING || t == TYPE_OPT_STRING; }
static bool is_null_type(ValueType t) { return t == TYPE_NULL; }
static bool is_optional(ValueType t) {
    return t == TYPE_OPT_STRING || t == TYPE_OPT_INT || t == TYPE_OPT_BOOL ||
           t == TYPE_OPT_F32 || t == TYPE_OPT_F64;
}
static bool is_composite(ValueType t) { return t == TYPE_ARRAY || t == TYPE_TUPLE || t == TYPE_STRUCT; }
static bool warned_io_input = false;

static ValueType promote_float(ValueType a, ValueType b) {
    if (a == TYPE_F64 || b == TYPE_F64) return TYPE_F64;
    if (a == TYPE_F32 || b == TYPE_F32) return TYPE_F32;
    return TYPE_INT;
}

static const char *value_type_name(ValueType t) {
    switch (t) {
        case TYPE_INT: return "adad";
        case TYPE_OPT_INT: return "adad?";
        case TYPE_BOOL: return "bool";
        case TYPE_OPT_BOOL: return "bool?";
        case TYPE_F32: return "ashari32";
        case TYPE_OPT_F32: return "ashari32?";
        case TYPE_F64: return "ashari";
        case TYPE_OPT_F64: return "ashari?";
        case TYPE_STRING: return "lafz";
        case TYPE_OPT_STRING: return "lafz?";
        case TYPE_NULL: return "null";
        case TYPE_ARRAY: return "array";
        case TYPE_TUPLE: return "tuple";
        case TYPE_STRUCT: return "bina";
        default: return "unknown";
    }
}

static StructDef* struct_find(CodeGen *cg, const char *name);
static int struct_size(CodeGen *cg, StructDef *sd);
static TypeInfo* typeinfo_clone(TypeInfo *ti);
static ValueType codegen_cast(CodeGen *cg, ValueType from, ValueType to);
int codegen_find_var(CodeGen *cg, const char *name);
static bool mod_func_table_has(const char *name);
static FunctionSig* func_sig_find(CodeGen *cg, const char *name);

static int array_elem_size(CodeGen *cg, TypeInfo *ti) {
    if (!ti) return 8;
    if (ti->elem_type == TYPE_STRUCT) {
        if (!ti->elem_typeinfo)
            error("array element missing struct type info");
        StructDef *sd = struct_find(cg, ti->elem_typeinfo->struct_name);
        if (!sd)
            error("Unknown struct type: %s", ti->elem_typeinfo->struct_name);
        return struct_size(cg, sd);
    }
    return 8;
}

static int typeinfo_size(CodeGen *cg, ValueType t, TypeInfo *ti) {
    if (t == TYPE_STRUCT) {
        if (!ti) error("struct type missing info");
        if (ti->by_ref) return 8;
        StructDef *sd = struct_find(cg, ti->struct_name);
        if (!sd) error("Unknown struct type: %s", ti->struct_name);
        return struct_size(cg, sd);
    }
    if (t == TYPE_ARRAY && ti) {
        if (ti->by_ref || ti->array_len < 0)
            return 8;
        return ti->array_len * array_elem_size(cg, ti);
    }
    if (t == TYPE_TUPLE && ti) {
        if (ti->by_ref)
            return 8;
        return ti->tuple_count * 8;
    }
    return 8;
}

/* ── Init ─────────────────────────────────────────────────────────── */

void codegen_init(CodeGen *cg, FILE *output, ModuleManager *mgr) {
    cg->output          = output;
    cg->label_counter   = 0;
    cg->local_vars      = malloc(sizeof(char*) * 256);
    cg->var_offsets     = malloc(sizeof(int)   * 256);
    cg->var_count       = 0;
    cg->stack_offset    = 0;
    cg->module_mgr      = mgr;
    cg->current_module[0] = '\0';
    cg->string_literals = NULL;
    cg->string_count    = 0;
    cg->string_cap      = 0;
    cg->func_sigs       = NULL;
    cg->func_sig_count  = 0;
    cg->func_sig_cap    = 0;
    cg->var_types       = malloc(sizeof(ValueType) * 256);
    cg->var_mutable     = malloc(sizeof(bool) * 256);
    cg->var_sizes       = malloc(sizeof(int) * 256);
    cg->var_typeinfo    = malloc(sizeof(TypeInfo*) * 256);
    cg->current_return_type = TYPE_INT;
    cg->current_return_typeinfo = NULL;
    cg->has_sret        = false;
    cg->sret_offset     = 0;
    cg->needs_arr_alloc = false;
    cg->needs_lafz_parse = false;
    cg->float_literals  = NULL;
    cg->float_count     = 0;
    cg->float_cap       = 0;
    cg->loop_depth      = 0;
    cg->struct_defs     = NULL;
    cg->struct_count    = 0;
    cg->struct_cap      = 0;
}

void codegen_emit(CodeGen *cg, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(cg->output, fmt, ap);
    fprintf(cg->output, "\n");
    va_end(ap);
}

int codegen_new_label(CodeGen *cg) { return cg->label_counter++; }

static void loop_push(CodeGen *cg, int continue_lbl, int break_lbl) {
    if (cg->loop_depth >= 64)
        error("loop nesting too deep");
    cg->loop_continue_labels[cg->loop_depth] = continue_lbl;
    cg->loop_break_labels[cg->loop_depth] = break_lbl;
    cg->loop_depth++;
}

static void loop_pop(CodeGen *cg) {
    if (cg->loop_depth <= 0)
        error("internal error: loop stack underflow");
    cg->loop_depth--;
}

static int loop_continue_label(CodeGen *cg) {
    if (cg->loop_depth <= 0)
        error("continue used outside of a loop");
    return cg->loop_continue_labels[cg->loop_depth - 1];
}

static int loop_break_label(CodeGen *cg) {
    if (cg->loop_depth <= 0)
        error("break used outside of a loop");
    return cg->loop_break_labels[cg->loop_depth - 1];
}

/* ── Struct table helpers ────────────────────────────────────────── */

static StructDef* struct_find(CodeGen *cg, const char *name) {
    if (!cg || !name) return NULL;
    for (int i = 0; i < cg->struct_count; i++) {
        if (strcmp(cg->struct_defs[i].name, name) == 0)
            return &cg->struct_defs[i];
    }
    return NULL;
}

static int struct_field_index(StructDef *sd, const char *field) {
    if (!sd || !field) return -1;
    for (int i = 0; i < sd->field_count; i++) {
        if (strcmp(sd->field_names[i], field) == 0)
            return i;
    }
    return -1;
}

static int struct_size(CodeGen *cg, StructDef *sd) {
    if (!sd) return 0;
    if (sd->size >= 0) return sd->size;
    if (sd->sizing)
        error("Recursive bina definition: %s", sd->name);
    sd->sizing = true;

    int offset = 0;
    for (int i = 0; i < sd->field_count; i++) {
        int fsize = 8;
        ValueType ft = sd->field_types[i];
        if (ft == TYPE_STRUCT) {
            TypeInfo *fti = sd->field_typeinfo ? sd->field_typeinfo[i] : NULL;
            if (!fti)
                error("struct field missing type info: %s.%s",
                      sd->name, sd->field_names[i]);
            StructDef *inner = struct_find(cg, fti->struct_name);
            if (!inner)
                error("Unknown struct type '%s' in %s.%s",
                      fti->struct_name, sd->name, sd->field_names[i]);
            fsize = struct_size(cg, inner);
        } else if (ft == TYPE_ARRAY || ft == TYPE_TUPLE) {
            error("bina fields cannot be array or tuple yet");
        }
        sd->field_offsets[i] = offset;
        offset += fsize;
    }
    sd->size = offset;
    sd->sizing = false;
    return offset;
}

static void struct_add_from_ast(CodeGen *cg, ASTNode *def) {
    if (!cg || !def) return;
    if (def->type != AST_STRUCT_DEF)
        return;
    if (struct_find(cg, def->data.struct_def.name))
        error("Duplicate bina definition: %s", def->data.struct_def.name);

    if (cg->struct_count == cg->struct_cap) {
        int new_cap = cg->struct_cap == 0 ? 8 : cg->struct_cap * 2;
        StructDef *next = realloc(cg->struct_defs, sizeof(StructDef) * new_cap);
        if (!next) error("Out of memory");
        cg->struct_defs = next;
        cg->struct_cap = new_cap;
    }

    StructDef *sd = &cg->struct_defs[cg->struct_count++];
    memset(sd, 0, sizeof(*sd));
    strncpy(sd->name, def->data.struct_def.name, MAX_TOKEN_LEN - 1);
    sd->field_count = def->data.struct_def.field_count;
    sd->size = -1;
    sd->sizing = false;

    if (sd->field_count > 0) {
        sd->field_names = malloc(sizeof(char*) * sd->field_count);
        sd->field_types = malloc(sizeof(ValueType) * sd->field_count);
        sd->field_typeinfo = malloc(sizeof(TypeInfo*) * sd->field_count);
        sd->field_offsets = malloc(sizeof(int) * sd->field_count);
        if (!sd->field_names || !sd->field_types || !sd->field_typeinfo || !sd->field_offsets)
            error("Out of memory");

        for (int i = 0; i < sd->field_count; i++) {
            sd->field_names[i] = strdup(def->data.struct_def.field_names[i]);
            sd->field_types[i] = def->data.struct_def.field_types[i];
            sd->field_typeinfo[i] = typeinfo_clone(def->data.struct_def.field_typeinfo
                                                   ? def->data.struct_def.field_typeinfo[i]
                                                   : NULL);
            sd->field_offsets[i] = 0;
        }
    }
}

static void codegen_register_structs(CodeGen *cg, ASTNode *program) {
    if (!program) return;
    for (int i = 0; i < program->data.program.struct_count; i++)
        struct_add_from_ast(cg, program->data.program.structs[i]);
}

static StructDef* struct_def_from_expr(CodeGen *cg, ASTNode *expr) {
    if (!expr) return NULL;
    if (expr->type == AST_IDENTIFIER) {
        int idx = codegen_find_var(cg, expr->data.identifier);
        if (idx == -1)
            error("Undefined variable: %s", expr->data.identifier);
        if (cg->var_types[idx] != TYPE_STRUCT)
            error("Not a struct: %s", expr->data.identifier);
        TypeInfo *ti = cg->var_typeinfo[idx];
        if (!ti)
            error("struct missing type info: %s", expr->data.identifier);
        StructDef *sd = struct_find(cg, ti->struct_name);
        if (!sd)
            error("Unknown struct type: %s", ti->struct_name);
        return sd;
    }
    if (expr->type == AST_FIELD_ACCESS) {
        StructDef *parent = struct_def_from_expr(cg, expr->data.field_access.base);
        int fidx = struct_field_index(parent, expr->data.field_access.field_name);
        if (fidx < 0)
            error("Unknown field '%s' on %s",
                  expr->data.field_access.field_name, parent->name);
        if (parent->field_types[fidx] != TYPE_STRUCT)
            error("Field '%s' on %s is not a struct",
                  parent->field_names[fidx], parent->name);
        TypeInfo *fti = parent->field_typeinfo ? parent->field_typeinfo[fidx] : NULL;
        if (!fti)
            error("struct field missing type info: %s.%s",
                  parent->name, parent->field_names[fidx]);
        StructDef *child = struct_find(cg, fti->struct_name);
        if (!child)
            error("Unknown struct type: %s", fti->struct_name);
        return child;
    }
    if (expr->type == AST_ARRAY_ACCESS) {
        ASTNode *base = expr->data.array_access.array;
        if (base->type != AST_IDENTIFIER)
            error("array access requires an identifier base");
        int idx = codegen_find_var(cg, base->data.identifier);
        if (idx == -1)
            error("Undefined variable: %s", base->data.identifier);
        if (cg->var_types[idx] != TYPE_ARRAY)
            error("Not an array: %s", base->data.identifier);
        TypeInfo *ti = cg->var_typeinfo[idx];
        if (!ti || ti->elem_type != TYPE_STRUCT)
            error("array element is not a struct");
        if (!ti->elem_typeinfo)
            error("array element missing struct type info");
        StructDef *sd = struct_find(cg, ti->elem_typeinfo->struct_name);
        if (!sd)
            error("Unknown struct type: %s", ti->elem_typeinfo->struct_name);
        return sd;
    }
    error("struct field access requires a struct base");
    return NULL;
}

static StructDef* emit_struct_addr(CodeGen *cg, ASTNode *expr) {
    if (!expr) return NULL;
    if (expr->type == AST_IDENTIFIER) {
        int idx = codegen_find_var(cg, expr->data.identifier);
        if (idx == -1)
            error("Undefined variable: %s", expr->data.identifier);
        if (cg->var_types[idx] != TYPE_STRUCT)
            error("Not a struct: %s", expr->data.identifier);
        TypeInfo *ti = cg->var_typeinfo[idx];
        if (!ti)
            error("struct missing type info: %s", expr->data.identifier);
        StructDef *sd = struct_find(cg, ti->struct_name);
        if (!sd)
            error("Unknown struct type: %s", ti->struct_name);
        struct_size(cg, sd);
        if (ti->by_ref)
            codegen_emit(cg, "    mov rax, [rbp - %d]", cg->var_offsets[idx]);
        else
            codegen_emit(cg, "    lea rax, [rbp - %d]", cg->var_offsets[idx]);
        return sd;
    }
    if (expr->type == AST_FIELD_ACCESS) {
        StructDef *parent = emit_struct_addr(cg, expr->data.field_access.base);
        int fidx = struct_field_index(parent, expr->data.field_access.field_name);
        if (fidx < 0)
            error("Unknown field '%s' on %s",
                  expr->data.field_access.field_name, parent->name);
        if (parent->field_types[fidx] != TYPE_STRUCT)
            error("Field '%s' on %s is not a struct",
                  parent->field_names[fidx], parent->name);
        struct_size(cg, parent);
        int off = parent->field_offsets[fidx];
        codegen_emit(cg, "    add rax, %d", off);
        TypeInfo *fti = parent->field_typeinfo ? parent->field_typeinfo[fidx] : NULL;
        if (!fti)
            error("struct field missing type info: %s.%s",
                  parent->name, parent->field_names[fidx]);
        StructDef *child = struct_find(cg, fti->struct_name);
        if (!child)
            error("Unknown struct type: %s", fti->struct_name);
        return child;
    }
    if (expr->type == AST_ARRAY_ACCESS) {
        ASTNode *base = expr->data.array_access.array;
        if (base->type != AST_IDENTIFIER)
            error("array access requires an identifier base");
        int idx = codegen_find_var(cg, base->data.identifier);
        if (idx == -1)
            error("Undefined variable: %s", base->data.identifier);
        if (cg->var_types[idx] != TYPE_ARRAY)
            error("Not an array: %s", base->data.identifier);
        TypeInfo *ti = cg->var_typeinfo[idx];
        if (!ti || ti->elem_type != TYPE_STRUCT)
            error("array element is not a struct");
        if (!ti->elem_typeinfo)
            error("array element missing struct type info");
        StructDef *sd = struct_find(cg, ti->elem_typeinfo->struct_name);
        if (!sd)
            error("Unknown struct type: %s", ti->elem_typeinfo->struct_name);

        ValueType it = codegen_expression(cg, expr->data.array_access.index);
        if (it != TYPE_INT)
            codegen_cast(cg, it, TYPE_INT);
        codegen_emit(cg, "    mov rcx, rax");

        if (ti->by_ref || ti->array_len < 0)
            codegen_emit(cg, "    mov rbx, [rbp - %d]", cg->var_offsets[idx]);
        else
            codegen_emit(cg, "    lea rbx, [rbp - %d]", cg->var_offsets[idx]);

        int elem_size = array_elem_size(cg, ti);
        if (elem_size == 8) {
            codegen_emit(cg, "    lea rax, [rbx + rcx*8]");
        } else {
            codegen_emit(cg, "    mov rdx, %d", elem_size);
            codegen_emit(cg, "    imul rcx, rdx");
            codegen_emit(cg, "    lea rax, [rbx + rcx]");
        }
        return sd;
    }
    error("struct field access requires a struct base");
    return NULL;
}

static TypeInfo* expr_typeinfo(CodeGen *cg, ASTNode *expr) {
    if (!expr) return NULL;
    switch (expr->type) {
    case AST_IDENTIFIER: {
        int idx = codegen_find_var(cg, expr->data.identifier);
        if (idx == -1)
            error("Undefined variable: %s", expr->data.identifier);
        return cg->var_typeinfo[idx];
    }
    case AST_CALL: {
        const char *call_name = expr->data.call.func_name;
        const char *sig_name = call_name;
        char mangled[MAX_TOKEN_LEN * 2 + 4];
        if (cg->current_module[0] != '\0' &&
            mod_func_table_has(call_name)) {
            snprintf(mangled, sizeof(mangled), "%s__%s",
                     cg->current_module, call_name);
            sig_name = mangled;
        }
        FunctionSig *sig = func_sig_find(cg, sig_name);
        return sig ? sig->return_typeinfo : NULL;
    }
    case AST_ARRAY_ACCESS: {
        ASTNode *base = expr->data.array_access.array;
        if (base->type != AST_IDENTIFIER)
            return NULL;
        int idx = codegen_find_var(cg, base->data.identifier);
        if (idx == -1)
            return NULL;
        if (cg->var_types[idx] != TYPE_ARRAY)
            return NULL;
        TypeInfo *ti = cg->var_typeinfo[idx];
        if (ti && ti->elem_type == TYPE_STRUCT)
            return ti->elem_typeinfo;
        return NULL;
    }
    case AST_MODULE_CALL: {
        if (strcmp(expr->data.module_call.module_name, "system") == 0 &&
            strcmp(expr->data.module_call.func_name, "argv") == 0 &&
            expr->data.module_call.arg_count == 0) {
            static TypeInfo ti;
            static bool inited = false;
            if (!inited) {
                memset(&ti, 0, sizeof(ti));
                ti.kind = TYPE_ARRAY;
                ti.elem_type = TYPE_STRING;
                ti.array_len = -1;
                inited = true;
            }
            return &ti;
        }

        char full_name[MAX_TOKEN_LEN * 2 + 4];
        snprintf(full_name, sizeof(full_name), "%s__%s",
                 expr->data.module_call.module_name,
                 expr->data.module_call.func_name);
        FunctionSig *sig = func_sig_find(cg, full_name);
        return sig ? sig->return_typeinfo : NULL;
    }
    case AST_FIELD_ACCESS: {
        StructDef *sd = struct_def_from_expr(cg, expr->data.field_access.base);
        int fidx = struct_field_index(sd, expr->data.field_access.field_name);
        if (fidx < 0)
            error("Unknown field '%s' on %s",
                  expr->data.field_access.field_name, sd->name);
        return sd->field_typeinfo ? sd->field_typeinfo[fidx] : NULL;
    }
    case AST_STRUCT_LITERAL:
        /* handled inline by built-ins */
        return NULL;
    default:
        return NULL;
    }
}

static int sizeof_type(CodeGen *cg, ValueType t, TypeInfo *ti) {
    switch (t) {
    case TYPE_ARRAY:
        if (ti && ti->array_len >= 0)
            return ti->array_len * array_elem_size(cg, ti);
        return 8;
    case TYPE_TUPLE:
        if (ti && ti->tuple_count > 0)
            return ti->tuple_count * 8;
        return 8;
    case TYPE_STRUCT:
        if (!ti)
            return 8;
        return struct_size(cg, struct_find(cg, ti->struct_name));
    default:
        return 8;
    }
}

static char* type_name_string(CodeGen *cg, ValueType t, TypeInfo *ti) {
    (void)cg;
    if (t == TYPE_ARRAY) {
        if (!ti)
            return strdup("array");
        const char *elem = value_type_name(ti->elem_type);
        char elem_buf[256];
        if (ti->elem_type == TYPE_STRUCT && ti->elem_typeinfo &&
            ti->elem_typeinfo->struct_name[0] != '\0') {
            snprintf(elem_buf, sizeof(elem_buf), "bina:%s",
                     ti->elem_typeinfo->struct_name);
            elem = elem_buf;
        }
        char buf[128];
        if (ti->array_len >= 0)
            snprintf(buf, sizeof(buf), "array[%s; %d]", elem, ti->array_len);
        else
            snprintf(buf, sizeof(buf), "array[%s]", elem);
        return strdup(buf);
    }
    if (t == TYPE_TUPLE) {
        if (!ti || ti->tuple_count <= 0)
            return strdup("tuple");
        size_t cap = 64 + (size_t)ti->tuple_count * 32;
        char *buf = malloc(cap);
        if (!buf) error("Out of memory");
        size_t pos = 0;
        pos += snprintf(buf + pos, cap - pos, "tuple(");
        for (int i = 0; i < ti->tuple_count; i++) {
            const char *nm = value_type_name(ti->tuple_types[i]);
            pos += snprintf(buf + pos, cap - pos, "%s%s",
                            (i == 0 ? "" : ", "), nm);
        }
        snprintf(buf + pos, cap - pos, ")");
        return buf;
    }
    if (t == TYPE_STRUCT) {
        if (!ti || ti->struct_name[0] == '\0')
            return strdup("bina");
        char buf[256];
        snprintf(buf, sizeof(buf), "bina:%s", ti->struct_name);
        return strdup(buf);
    }
    return strdup(value_type_name(t));
}

/* ── Variable helpers ─────────────────────────────────────────────── */

int codegen_find_var(CodeGen *cg, const char *name) {
    for (int i = cg->var_count - 1; i >= 0; i--)
        if (strcmp(cg->local_vars[i], name) == 0)
            return i;
    return -1;
}

void codegen_add_var(CodeGen *cg, const char *name, ValueType type, bool is_mut, TypeInfo *tinfo) {
    int size = typeinfo_size(cg, type, tinfo);
    if (size < 8) size = 8;
    cg->stack_offset += size;
    if (cg->stack_offset > LOCAL_STACK_RESERVE)
        error("local stack limit exceeded (%d bytes)", LOCAL_STACK_RESERVE);
    cg->local_vars[cg->var_count]  = strdup(name);
    cg->var_offsets[cg->var_count] = cg->stack_offset;
    cg->var_types[cg->var_count]   = type;
    cg->var_mutable[cg->var_count] = is_mut;
    cg->var_sizes[cg->var_count]   = size;
    cg->var_typeinfo[cg->var_count] = tinfo;
    cg->var_count++;
}

/* ── Module function table ────────────────────────────────────────── */
/*
 * When compiling a module we build a small table of every function
 * defined in that module.  Plain AST_CALL nodes whose callee is in
 * this table are rewritten to the mangled name at emit time.
 */
#define MAX_MOD_FUNCS 128
static char mod_func_table[MAX_MOD_FUNCS][MAX_TOKEN_LEN];
static int  mod_func_count = 0;

#define LOCAL_STACK_RESERVE 1024

static void mod_func_table_clear(void) { mod_func_count = 0; }

static void mod_func_table_add(const char *name) {
    if (mod_func_count < MAX_MOD_FUNCS)
        strncpy(mod_func_table[mod_func_count++], name, MAX_TOKEN_LEN - 1);
}

static bool mod_func_table_has(const char *name) {
    for (int i = 0; i < mod_func_count; i++)
        if (strcmp(mod_func_table[i], name) == 0) return true;
    return false;
}

/* ── Function signature table ─────────────────────────────────────── */
static TypeInfo* typeinfo_clone(TypeInfo *ti) {
    if (!ti) return NULL;
    TypeInfo *c = (TypeInfo*)calloc(1, sizeof(TypeInfo));
    if (!c) error("Out of memory");
    c->kind = ti->kind;
    c->elem_type = ti->elem_type;
    c->elem_typeinfo = NULL;
    c->array_len = ti->array_len;
    c->tuple_count = ti->tuple_count;
    c->by_ref = ti->by_ref;
    strncpy(c->struct_name, ti->struct_name, MAX_TOKEN_LEN - 1);
    if (ti->kind == TYPE_TUPLE && ti->tuple_count > 0) {
        c->tuple_types = malloc(sizeof(ValueType) * ti->tuple_count);
        if (!c->tuple_types) error("Out of memory");
        for (int i = 0; i < ti->tuple_count; i++)
            c->tuple_types[i] = ti->tuple_types[i];
    }
    if (ti->kind == TYPE_ARRAY && ti->elem_typeinfo) {
        c->elem_typeinfo = typeinfo_clone(ti->elem_typeinfo);
    }
    return c;
}

static TypeInfo* typeinfo_ref(TypeInfo *ti) {
    TypeInfo *c = typeinfo_clone(ti);
    if (c) c->by_ref = true;
    return c;
}

static void typeinfo_free(TypeInfo *ti) {
    if (!ti) return;
    if (ti->kind == TYPE_TUPLE)
        free(ti->tuple_types);
    if (ti->kind == TYPE_ARRAY && ti->elem_typeinfo) {
        if (ti->elem_typeinfo->kind == TYPE_TUPLE)
            free(ti->elem_typeinfo->tuple_types);
        free(ti->elem_typeinfo);
    }
    free(ti);
}

static void func_sig_add(CodeGen *cg, const char *name,
                         ValueType ret_type,
                         TypeInfo *ret_typeinfo,
                         ValueType *param_types,
                         TypeInfo **param_typeinfo,
                         int param_count) {
    if (!name) return;
    if (cg->func_sig_count == cg->func_sig_cap) {
        int new_cap = cg->func_sig_cap == 0 ? 32 : cg->func_sig_cap * 2;
        cg->func_sigs = realloc(cg->func_sigs, sizeof(FunctionSig) * new_cap);
        if (!cg->func_sigs) error("Out of memory");
        cg->func_sig_cap = new_cap;
    }

    FunctionSig *sig = &cg->func_sigs[cg->func_sig_count++];
    memset(sig, 0, sizeof(*sig));
    strncpy(sig->name, name, sizeof(sig->name) - 1);
    sig->return_type = ret_type;
    sig->return_typeinfo = typeinfo_clone(ret_typeinfo);
    sig->param_count = param_count;
    if (param_count > 0 && param_types) {
        sig->param_types = malloc(sizeof(ValueType) * param_count);
        if (!sig->param_types) error("Out of memory");
        for (int i = 0; i < param_count; i++)
            sig->param_types[i] = param_types[i];
        sig->param_typeinfo = malloc(sizeof(TypeInfo*) * param_count);
        if (!sig->param_typeinfo) error("Out of memory");
        for (int i = 0; i < param_count; i++)
            sig->param_typeinfo[i] = typeinfo_clone(param_typeinfo ? param_typeinfo[i] : NULL);
    } else {
        sig->param_types = NULL;
        sig->param_typeinfo = NULL;
    }
}

static FunctionSig* func_sig_find(CodeGen *cg, const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < cg->func_sig_count; i++) {
        if (strcmp(cg->func_sigs[i].name, name) == 0) return &cg->func_sigs[i];
    }
    return NULL;
}

static void register_builtin_module_sigs(CodeGen *cg, const char *mod) {
    if (!mod) return;
    if (strcmp(mod, "lafz") == 0) {
        ValueType t_str = TYPE_STRING;
        ValueType t_int = TYPE_INT;
        ValueType t_f64 = TYPE_F64;

        ValueType p1[1] = {t_str};
        ValueType p2[2] = {t_str, t_str};
        ValueType p3[3] = {t_str, t_int, t_int};
        ValueType p1i[1] = {t_int};
        ValueType p1f[1] = {t_f64};

        func_sig_add(cg, "lafz__len", t_int, NULL, p1, NULL, 1);
        func_sig_add(cg, "lafz__eq", t_int, NULL, p2, NULL, 2);
        func_sig_add(cg, "lafz__concat", t_str, NULL, p2, NULL, 2);
        func_sig_add(cg, "lafz__slice", t_str, NULL, p3, NULL, 3);
        func_sig_add(cg, "lafz__from_adad", t_str, NULL, p1i, NULL, 1);
        func_sig_add(cg, "lafz__from_ashari", t_str, NULL, p1f, NULL, 1);
        func_sig_add(cg, "lafz__to_adad", t_int, NULL, p1, NULL, 1);
        func_sig_add(cg, "lafz__to_ashari", t_f64, NULL, p1, NULL, 1);
    } else if (strcmp(mod, "io") == 0) {
        func_sig_add(cg, "io__input", TYPE_INT, NULL, NULL, NULL, 0);
        func_sig_add(cg, "io__stdin", TYPE_STRING, NULL, NULL, NULL, 0);
    } else if (strcmp(mod, "time") == 0) {
        func_sig_add(cg, "time__now", TYPE_INT, NULL, NULL, NULL, 0);
        func_sig_add(cg, "time__now_ms", TYPE_INT, NULL, NULL, NULL, 0);
        func_sig_add(cg, "time__now_ns", TYPE_INT, NULL, NULL, NULL, 0);
        ValueType p1[1] = {TYPE_INT};
        func_sig_add(cg, "time__sleep", TYPE_INT, NULL, p1, NULL, 1);
        func_sig_add(cg, "time__ctime", TYPE_STRING, NULL, NULL, NULL, 0);
    } else if (strcmp(mod, "random") == 0) {
        ValueType p1[1] = {TYPE_INT};
        func_sig_add(cg, "random__seed", TYPE_INT, NULL, p1, NULL, 1);
        func_sig_add(cg, "random__randrange", TYPE_INT, NULL, p1, NULL, 1);
        func_sig_add(cg, "random__random", TYPE_F64, NULL, NULL, NULL, 0);
    } else if (strcmp(mod, "system") == 0) {
        ValueType p1i[1] = {TYPE_INT};
        ValueType p1s[1] = {TYPE_STRING};
        func_sig_add(cg, "system__exit", TYPE_INT, NULL, p1i, NULL, 1);
        func_sig_add(cg, "system__argc", TYPE_INT, NULL, NULL, NULL, 0);
        func_sig_add(cg, "system__argv", TYPE_OPT_STRING, NULL, p1i, NULL, 1);
        func_sig_add(cg, "system__getenv", TYPE_OPT_STRING, NULL, p1s, NULL, 1);
    } else if (strcmp(mod, "os") == 0) {
        ValueType p1s[1] = {TYPE_STRING};
        func_sig_add(cg, "os__getcwd", TYPE_OPT_STRING, NULL, NULL, NULL, 0);
        func_sig_add(cg, "os__chdir", TYPE_INT, NULL, p1s, NULL, 1);
    }
}

/* ── String literal table ─────────────────────────────────────────── */
static int codegen_intern_string(CodeGen *cg, const char *value) {
    if (!value) value = "";
    if (cg->string_count == cg->string_cap) {
        int new_cap = cg->string_cap == 0 ? 16 : cg->string_cap * 2;
        cg->string_literals = realloc(cg->string_literals, sizeof(char*) * new_cap);
        if (!cg->string_literals) error("Out of memory");
        cg->string_cap = new_cap;
    }
    cg->string_literals[cg->string_count] = strdup(value);
    if (!cg->string_literals[cg->string_count]) error("Out of memory");
    return cg->string_count++;
}

/* ── Float literal table ──────────────────────────────────────────── */
static int codegen_intern_float(CodeGen *cg, double value, ValueType type) {
    if (cg->float_count == cg->float_cap) {
        int new_cap = cg->float_cap == 0 ? 16 : cg->float_cap * 2;
        cg->float_literals = realloc(cg->float_literals, sizeof(FloatLit) * new_cap);
        if (!cg->float_literals) error("Out of memory");
        cg->float_cap = new_cap;
    }
    cg->float_literals[cg->float_count].value = value;
    cg->float_literals[cg->float_count].type  = type;
    return cg->float_count++;
}

/* ── Argument registers (System V AMD64 ABI) ──────────────────────── */
static const char *ARG_REGS[] = {"rdi","rsi","rdx","rcx","r8","r9"};

/* ── Expression codegen ───────────────────────────────────────────── */

static ValueType codegen_cast(CodeGen *cg, ValueType from, ValueType to) {
    if (from == to) return to;

    if (from == TYPE_NULL) {
        if (to == TYPE_OPT_STRING) {
            codegen_emit(cg, "    xor rax, rax");
            return to;
        }
        if (to == TYPE_OPT_INT) {
            codegen_emit(cg, "    mov rax, 0x8000000000000000");
            return to;
        }
        if (to == TYPE_OPT_BOOL) {
            codegen_emit(cg, "    mov rax, 2");
            return to;
        }
        if (to == TYPE_OPT_F64) {
            codegen_emit(cg, "    mov rax, 0x7ff8000000000001");
            return to;
        }
        if (to == TYPE_OPT_F32) {
            codegen_emit(cg, "    mov eax, 0x7fc00001");
            return to;
        }
        if (to == TYPE_STRING) {
            error("Cannot assign null to non-optional lafz");
        }
    }

    if (from == TYPE_STRING && to == TYPE_OPT_STRING) {
        return to;
    }
    if (from == TYPE_OPT_STRING && to == TYPE_STRING) {
        return to;
    }
    if (from == TYPE_INT && to == TYPE_OPT_INT) return to;
    if (from == TYPE_BOOL && to == TYPE_OPT_BOOL) return to;
    if (from == TYPE_F32 && to == TYPE_OPT_F32) {
        codegen_emit(cg, "    movd eax, xmm0");
        return to;
    }
    if (from == TYPE_F64 && to == TYPE_OPT_F64) {
        codegen_emit(cg, "    movq rax, xmm0");
        return to;
    }

    if (from == TYPE_OPT_INT && to == TYPE_INT) {
        int null_lbl = codegen_new_label(cg);
        int end_lbl = codegen_new_label(cg);
        codegen_emit(cg, "    mov rbx, 0x8000000000000000");
        codegen_emit(cg, "    cmp rax, rbx");
        codegen_emit(cg, "    je  _VL%d", null_lbl);
        codegen_emit(cg, "    jmp _VL%d", end_lbl);
        codegen_emit(cg, "_VL%d:", null_lbl);
        codegen_emit(cg, "    xor rax, rax");
        codegen_emit(cg, "_VL%d:", end_lbl);
        return to;
    }
    if (from == TYPE_OPT_BOOL && to == TYPE_BOOL) {
        int null_lbl = codegen_new_label(cg);
        int end_lbl = codegen_new_label(cg);
        codegen_emit(cg, "    cmp rax, 2");
        codegen_emit(cg, "    je  _VL%d", null_lbl);
        codegen_emit(cg, "    jmp _VL%d", end_lbl);
        codegen_emit(cg, "_VL%d:", null_lbl);
        codegen_emit(cg, "    xor rax, rax");
        codegen_emit(cg, "_VL%d:", end_lbl);
        return to;
    }
    if (from == TYPE_OPT_F64 && to == TYPE_F64) {
        int null_lbl = codegen_new_label(cg);
        int end_lbl = codegen_new_label(cg);
        codegen_emit(cg, "    mov rbx, 0x7ff8000000000001");
        codegen_emit(cg, "    cmp rax, rbx");
        codegen_emit(cg, "    je  _VL%d", null_lbl);
        codegen_emit(cg, "    movq xmm0, rax");
        codegen_emit(cg, "    jmp _VL%d", end_lbl);
        codegen_emit(cg, "_VL%d:", null_lbl);
        codegen_emit(cg, "    xorpd xmm0, xmm0");
        codegen_emit(cg, "_VL%d:", end_lbl);
        return to;
    }
    if (from == TYPE_OPT_F32 && to == TYPE_F32) {
        int null_lbl = codegen_new_label(cg);
        int end_lbl = codegen_new_label(cg);
        codegen_emit(cg, "    cmp eax, 0x7fc00001");
        codegen_emit(cg, "    je  _VL%d", null_lbl);
        codegen_emit(cg, "    movd xmm0, eax");
        codegen_emit(cg, "    jmp _VL%d", end_lbl);
        codegen_emit(cg, "_VL%d:", null_lbl);
        codegen_emit(cg, "    xorps xmm0, xmm0");
        codegen_emit(cg, "_VL%d:", end_lbl);
        return to;
    }

    if ((from == TYPE_STRING || from == TYPE_OPT_STRING) &&
        (to == TYPE_INT || to == TYPE_F64 || to == TYPE_F32 ||
         to == TYPE_OPT_INT || to == TYPE_OPT_F64 || to == TYPE_OPT_F32)) {
        cg->needs_lafz_parse = true;
        int null_lbl = codegen_new_label(cg);
        int end_lbl = codegen_new_label(cg);
        if (from == TYPE_OPT_STRING) {
            codegen_emit(cg, "    cmp rax, 0");
            codegen_emit(cg, "    je  _VL%d", null_lbl);
        }
        if (to == TYPE_INT || to == TYPE_OPT_INT) {
            codegen_emit(cg, "    mov rdi, rax");
            codegen_emit(cg, "    call lafz__to_adad");
        } else {
            codegen_emit(cg, "    mov rdi, rax");
            codegen_emit(cg, "    call lafz__to_ashari");
            if (to == TYPE_F32 || to == TYPE_OPT_F32)
                codegen_emit(cg, "    cvtsd2ss xmm0, xmm0");
            if (to == TYPE_OPT_F64)
                codegen_emit(cg, "    movq rax, xmm0");
            if (to == TYPE_OPT_F32)
                codegen_emit(cg, "    movd eax, xmm0");
        }
        if (from == TYPE_OPT_STRING) {
            codegen_emit(cg, "    jmp _VL%d", end_lbl);
            codegen_emit(cg, "_VL%d:", null_lbl);
            if (to == TYPE_INT || to == TYPE_OPT_INT)
                codegen_emit(cg, "    xor rax, rax");
            else if (to == TYPE_F64)
                codegen_emit(cg, "    xorpd xmm0, xmm0");
            else if (to == TYPE_F32)
                codegen_emit(cg, "    xorps xmm0, xmm0");
            else if (to == TYPE_OPT_F64)
                codegen_emit(cg, "    mov rax, 0x7ff8000000000001");
            else
                codegen_emit(cg, "    mov eax, 0x7fc00001");
            codegen_emit(cg, "_VL%d:", end_lbl);
        }
        return to;
    }

    if (to == TYPE_BOOL) {
        if (from == TYPE_INT) {
            codegen_emit(cg, "    cmp rax, 0");
            codegen_emit(cg, "    setne al");
            codegen_emit(cg, "    movzx rax, al");
            return to;
        }
        if (from == TYPE_F64) {
            codegen_emit(cg, "    cvttsd2si rax, xmm0");
            codegen_emit(cg, "    cmp rax, 0");
            codegen_emit(cg, "    setne al");
            codegen_emit(cg, "    movzx rax, al");
            return to;
        }
        if (from == TYPE_F32) {
            codegen_emit(cg, "    cvttss2si rax, xmm0");
            codegen_emit(cg, "    cmp rax, 0");
            codegen_emit(cg, "    setne al");
            codegen_emit(cg, "    movzx rax, al");
            return to;
        }
        if (from == TYPE_BOOL) return to;
    }

    if (from == TYPE_BOOL && to == TYPE_INT) {
        return to;
    }

    if (from == TYPE_BOOL && to == TYPE_F64) {
        codegen_emit(cg, "    cvtsi2sd xmm0, rax");
        return to;
    }

    if (from == TYPE_BOOL && to == TYPE_F32) {
        codegen_emit(cg, "    cvtsi2ss xmm0, rax");
        return to;
    }

    if (from == TYPE_INT && to == TYPE_F64) {
        codegen_emit(cg, "    cvtsi2sd xmm0, rax");
        return to;
    }
    if (from == TYPE_INT && to == TYPE_F32) {
        codegen_emit(cg, "    cvtsi2ss xmm0, rax");
        return to;
    }
    if (from == TYPE_F64 && to == TYPE_INT) {
        codegen_emit(cg, "    cvttsd2si rax, xmm0");
        return to;
    }
    if (from == TYPE_F32 && to == TYPE_INT) {
        codegen_emit(cg, "    cvttss2si rax, xmm0");
        return to;
    }
    if (from == TYPE_F32 && to == TYPE_F64) {
        codegen_emit(cg, "    cvtss2sd xmm0, xmm0");
        return to;
    }
    if (from == TYPE_F64 && to == TYPE_F32) {
        codegen_emit(cg, "    cvtsd2ss xmm0, xmm0");
        return to;
    }

    if (is_string_like(from) || is_string_like(to) ||
        is_optional(from) || is_optional(to) ||
        from == TYPE_NULL || to == TYPE_NULL ||
        is_composite(from) || is_composite(to)) {
        error("Invalid cast from %s to %s", value_type_name(from), value_type_name(to));
    }
    return to;
}

static ValueType infer_expr_type(CodeGen *cg, ASTNode *expr) {
    if (!expr) return TYPE_INT;
    switch (expr->type) {
    case AST_INTEGER: return TYPE_INT;
    case AST_BOOL: return TYPE_BOOL;
    case AST_NULL: return TYPE_NULL;
    case AST_FLOAT: return expr->data.float_lit.type;
    case AST_STRING: return TYPE_STRING;
    case AST_STRUCT_LITERAL: return TYPE_STRUCT;
    case AST_IDENTIFIER: {
        int idx = codegen_find_var(cg, expr->data.identifier);
        if (idx == -1)
            error("Undefined variable: %s", expr->data.identifier);
        return cg->var_types[idx];
    }
    case AST_ARRAY_ACCESS: {
        ASTNode *base = expr->data.array_access.array;
        if (base->type != AST_IDENTIFIER)
            error("array access requires an identifier base");
        int idx = codegen_find_var(cg, base->data.identifier);
        if (idx == -1)
            error("Undefined variable: %s", base->data.identifier);
        if (cg->var_types[idx] != TYPE_ARRAY)
            error("Not an array: %s", base->data.identifier);
        TypeInfo *ti = cg->var_typeinfo[idx];
        if (!ti)
            error("array missing type info");
        return ti->elem_type;
    }
    case AST_TUPLE_ACCESS: {
        ASTNode *base = expr->data.tuple_access.tuple;
        if (base->type != AST_IDENTIFIER)
            error("tuple access requires an identifier base");
        int idx = codegen_find_var(cg, base->data.identifier);
        if (idx == -1)
            error("Undefined variable: %s", base->data.identifier);
        if (cg->var_types[idx] != TYPE_TUPLE)
            error("Not a tuple: %s", base->data.identifier);
        TypeInfo *ti = cg->var_typeinfo[idx];
        if (!ti || expr->data.tuple_access.index < 0 ||
            expr->data.tuple_access.index >= ti->tuple_count)
            error("Tuple index out of range");
        return ti->tuple_types[expr->data.tuple_access.index];
    }
    case AST_FIELD_ACCESS: {
        StructDef *sd = struct_def_from_expr(cg, expr->data.field_access.base);
        int fidx = struct_field_index(sd, expr->data.field_access.field_name);
        if (fidx < 0)
            error("Unknown field '%s' on %s",
                  expr->data.field_access.field_name, sd->name);
        return sd->field_types[fidx];
    }
    case AST_BINARY_OP: {
        ValueType lt = infer_expr_type(cg, expr->data.binary.left);
        ValueType rt = infer_expr_type(cg, expr->data.binary.right);

        if (expr->data.binary.op == OP_EQ || expr->data.binary.op == OP_NE) {
            if ((is_optional(lt) && rt == TYPE_NULL) ||
                (lt == TYPE_NULL && is_optional(rt)) ||
                (lt == TYPE_NULL && rt == TYPE_NULL)) {
                return TYPE_BOOL;
            }
        }

        if (is_string_like(lt) || is_string_like(rt) ||
            is_null_type(lt) || is_null_type(rt) ||
            is_optional(lt) || is_optional(rt) ||
            is_composite(lt) || is_composite(rt)) {
            ValueType bad = (is_string_like(lt) || is_null_type(lt) || is_optional(lt) || is_composite(lt)) ? lt : rt;
            error("Invalid operand type for binary op: %s", value_type_name(bad));
        }

        if (expr->data.binary.op == OP_EQ ||
            expr->data.binary.op == OP_NE ||
            expr->data.binary.op == OP_LT ||
            expr->data.binary.op == OP_LE ||
            expr->data.binary.op == OP_GT ||
            expr->data.binary.op == OP_GE) {
            return TYPE_BOOL;
        }
        if (is_float(lt) || is_float(rt))
            return promote_float(lt, rt);
        return TYPE_INT;
    }
    case AST_CALL: {
        const char *call_name = expr->data.call.func_name;
        const char *sig_name = call_name;
        char mangled[MAX_TOKEN_LEN * 2 + 4];

        if (strcmp(call_name, "len") == 0)
            return TYPE_INT;
        if (strcmp(call_name, "sizeof") == 0)
            return TYPE_INT;
        if (strcmp(call_name, "type") == 0)
            return TYPE_STRING;

        if (cg->current_module[0] != '\0' &&
            mod_func_table_has(call_name)) {
            snprintf(mangled, sizeof(mangled), "%s__%s",
                     cg->current_module, call_name);
            sig_name = mangled;
        }
        FunctionSig *sig = func_sig_find(cg, sig_name);
        if (!sig)
            error("unknown function: %s", call_name);
        return sig->return_type;
    }
    case AST_MODULE_CALL: {
        if (strcmp(expr->data.module_call.module_name, "system") == 0 &&
            strcmp(expr->data.module_call.func_name, "argv") == 0 &&
            expr->data.module_call.arg_count == 0) {
            return TYPE_ARRAY;
        }

        if (strcmp(expr->data.module_call.module_name, "random") == 0 &&
            strcmp(expr->data.module_call.func_name, "choice") == 0) {
            if (expr->data.module_call.arg_count != 1)
                error("random.choice() expects 1 argument");
            ASTNode *arg = expr->data.module_call.args[0];
            ValueType at = infer_expr_type(cg, arg);
            TypeInfo *ti = expr_typeinfo(cg, arg);
            ValueType elem = TYPE_INT;

            if (at == TYPE_ARRAY) {
                if (!ti && arg->type == AST_CALL &&
                    (strcmp(arg->data.call.func_name, "map") == 0 ||
                     strcmp(arg->data.call.func_name, "filter") == 0)) {
                    ASTNode *arr_expr = arg->data.call.args[0];
                    ASTNode *lam_expr = arg->data.call.args[1];
                    if (infer_expr_type(cg, arr_expr) != TYPE_ARRAY)
                        error("random.choice() requires an array");
                    TypeInfo *arr_ti = expr_typeinfo(cg, arr_expr);
                    if (!arr_ti)
                        error("random.choice() requires array type info");
                    elem = (strcmp(arg->data.call.func_name, "filter") == 0)
                             ? arr_ti->elem_type
                             : infer_expr_type(cg, lam_expr->data.lambda.body);
                } else {
                    if (!ti)
                        error("random.choice() requires array type info");
                    elem = ti->elem_type;
                }
                if (elem == TYPE_STRUCT)
                    error("random.choice() does not support bina elements yet");
                return elem;
            }

            if (at == TYPE_TUPLE) {
                if (!ti || ti->tuple_count <= 0)
                    error("random.choice() requires tuple type info");
                elem = ti->tuple_types[0];
                for (int i = 1; i < ti->tuple_count; i++) {
                    if (ti->tuple_types[i] != elem)
                        error("random.choice() requires uniform tuple element types");
                }
                if (elem == TYPE_STRUCT)
                    error("random.choice() does not support bina elements yet");
                return elem;
            }

            error("random.choice() requires an array or tuple");
        }

        char full_name[MAX_TOKEN_LEN * 2 + 4];
        snprintf(full_name, sizeof(full_name), "%s__%s",
                 expr->data.module_call.module_name,
                 expr->data.module_call.func_name);
        FunctionSig *sig = func_sig_find(cg, full_name);
        if (!sig)
            error("unknown module function: %s.%s",
                  expr->data.module_call.module_name,
                  expr->data.module_call.func_name);
        return sig->return_type;
    }
    case AST_ARRAY_LITERAL:
    case AST_TUPLE_LITERAL:
    case AST_RANGE:
    case AST_LAMBDA:
        error("cannot infer type from this expression here");
        return TYPE_INT;
    default:
        error("unsupported expression in type inference");
        return TYPE_INT;
    }
}

static ValueType infer_array_lit_elem_type(CodeGen *cg, ASTNode *lit) {
    if (!lit || lit->type != AST_ARRAY_LITERAL || lit->data.array_lit.count <= 0)
        error("cannot infer type from empty array literal");
    ValueType inferred = TYPE_INT;
    bool set = false;

    for (int i = 0; i < lit->data.array_lit.count; i++) {
        ValueType t = infer_expr_type(cg, lit->data.array_lit.elements[i]);
        if (t == TYPE_NULL)
            error("null is not allowed in inferred array literals");
        if (is_composite(t))
            error("composite types not supported in inferred array literals");
        if (is_optional(t))
            error("optional types not supported in inferred array literals");

        if (!set) {
            inferred = t;
            set = true;
            continue;
        }

        if (inferred == t)
            continue;

        if (is_float(inferred) || is_float(t)) {
            if (inferred == TYPE_F64 || t == TYPE_F64)
                inferred = TYPE_F64;
            else
                inferred = TYPE_F32;
            if (t == TYPE_INT || inferred == TYPE_INT)
                continue;
            if (is_float(t))
                continue;
        }

        if (inferred == TYPE_INT && t == TYPE_INT)
            continue;

        error("inferred array literal has mixed element types");
    }
    return inferred;
}

static void codegen_copy_struct(CodeGen *cg, StructDef *sd,
                                const char *dst_reg, const char *src_reg) {
    int size = struct_size(cg, sd);
    for (int off = 0; off < size; off += 8) {
        codegen_emit(cg, "    mov rax, [%s + %d]", src_reg, off);
        codegen_emit(cg, "    mov [%s + %d], rax", dst_reg, off);
    }
}

static void codegen_emit_struct_literal(CodeGen *cg, StructDef *sd, ASTNode *lit) {
    if (!lit || lit->type != AST_STRUCT_LITERAL)
        error("expected struct literal");
    if (strcmp(lit->data.struct_lit.struct_name, sd->name) != 0)
        error("struct literal type mismatch: expected %s, got %s",
              sd->name, lit->data.struct_lit.struct_name);
    struct_size(cg, sd);

    bool *seen = calloc(sd->field_count, sizeof(bool));
    if (!seen) error("Out of memory");

    for (int i = 0; i < lit->data.struct_lit.field_count; i++) {
        const char *fname = lit->data.struct_lit.field_names[i];
        ASTNode *fval = lit->data.struct_lit.field_values[i];
        int fidx = struct_field_index(sd, fname);
        if (fidx < 0)
            error("Unknown field '%s' on %s", fname, sd->name);
        if (seen[fidx])
            error("Duplicate field '%s' in %s literal", fname, sd->name);
        seen[fidx] = true;

        ValueType expect = sd->field_types[fidx];
        int off = sd->field_offsets[fidx];

        if (expect == TYPE_STRUCT) {
            TypeInfo *fti = sd->field_typeinfo ? sd->field_typeinfo[fidx] : NULL;
            if (!fti)
                error("struct field missing type info: %s.%s",
                      sd->name, sd->field_names[fidx]);
            StructDef *child = struct_find(cg, fti->struct_name);
            if (!child)
                error("Unknown struct type: %s", fti->struct_name);

            if (fval->type == AST_STRUCT_LITERAL) {
                codegen_emit(cg, "    lea rdx, [rbx + %d]", off);
                codegen_emit(cg, "    push rbx");
                codegen_emit(cg, "    mov rbx, rdx");
                codegen_emit_struct_literal(cg, child, fval);
                codegen_emit(cg, "    pop rbx");
            } else {
                ValueType at = codegen_expression(cg, fval);
                if (at != TYPE_STRUCT)
                    error("struct field '%s' expects %s", fname, child->name);
                codegen_emit(cg, "    mov rcx, rax");
                codegen_emit(cg, "    lea rdx, [rbx + %d]", off);
                codegen_copy_struct(cg, child, "rdx", "rcx");
            }
            continue;
        }

        codegen_emit(cg, "    push rbx");
        ValueType at = codegen_expression(cg, fval);
        codegen_emit(cg, "    pop rbx");
        if (at != expect)
            at = codegen_cast(cg, at, expect);

        if (expect == TYPE_F32)
            codegen_emit(cg, "    movss [rbx + %d], xmm0", off);
        else if (expect == TYPE_F64)
            codegen_emit(cg, "    movsd [rbx + %d], xmm0", off);
        else
            codegen_emit(cg, "    mov [rbx + %d], rax", off);
    }

    for (int i = 0; i < sd->field_count; i++) {
        if (!seen[i])
            error("Missing field '%s' in %s literal",
                  sd->field_names[i], sd->name);
    }
    free(seen);
}

ValueType codegen_expression(CodeGen *cg, ASTNode *expr) {
    if (!expr) return TYPE_INT;

    switch (expr->type) {

    case AST_INTEGER:
        codegen_emit(cg, "    mov rax, %d", expr->data.int_value);
        return TYPE_INT;

    case AST_BOOL:
        codegen_emit(cg, "    mov rax, %d", expr->data.bool_value ? 1 : 0);
        return TYPE_BOOL;

    case AST_NULL:
        codegen_emit(cg, "    xor rax, rax");
        return TYPE_NULL;

    case AST_ARRAY_LITERAL:
        error("array literals are only supported in let initializers");
        return TYPE_ARRAY;

    case AST_TUPLE_LITERAL:
        error("tuple literals are only supported in let initializers");
        return TYPE_TUPLE;

    case AST_STRUCT_LITERAL:
        error("struct literals are only supported in let initializers or returns");
        return TYPE_STRUCT;

    case AST_LAMBDA:
        error("lambda can only be used as an argument to map()/filter()");
        return TYPE_INT;

    case AST_RANGE:
        error("range expressions are only supported in for-in loops");
        return TYPE_INT;

    case AST_FLOAT: {
        int fid = codegen_intern_float(cg,
                                       expr->data.float_lit.value,
                                       expr->data.float_lit.type);
        if (expr->data.float_lit.type == TYPE_F32) {
            codegen_emit(cg, "    movss xmm0, [rel _VL_f32_%d]", fid);
            return TYPE_F32;
        }
        codegen_emit(cg, "    movsd xmm0, [rel _VL_f64_%d]", fid);
        return TYPE_F64;
    }

    case AST_STRING: {
        int sid = codegen_intern_string(cg, expr->data.string_lit.value);
        codegen_emit(cg, "    lea rax, [rel _VL_str_%d]", sid);
        return TYPE_STRING;
    }

    case AST_ARRAY_ACCESS: {
        ASTNode *base = expr->data.array_access.array;
        if (base->type != AST_IDENTIFIER)
            error("array access requires an identifier base");
        int idx = codegen_find_var(cg, base->data.identifier);
        if (idx == -1)
            error("Undefined variable: %s", base->data.identifier);
        if (cg->var_types[idx] != TYPE_ARRAY)
            error("Not an array: %s", base->data.identifier);

        ValueType it = codegen_expression(cg, expr->data.array_access.index);
        if (it != TYPE_INT)
            codegen_cast(cg, it, TYPE_INT);

        TypeInfo *ti = cg->var_typeinfo[idx];
        if (!ti)
            error("array missing type info");
        ValueType elem = ti ? ti->elem_type : TYPE_INT;
        int elem_size = array_elem_size(cg, ti);

        codegen_emit(cg, "    mov rcx, rax");
        if (ti->by_ref || ti->array_len < 0)
            codegen_emit(cg, "    mov rbx, [rbp - %d]", cg->var_offsets[idx]);
        else
            codegen_emit(cg, "    lea rbx, [rbp - %d]", cg->var_offsets[idx]);
        if (elem == TYPE_STRUCT) {
            if (elem_size == 8)
                codegen_emit(cg, "    lea rax, [rbx + rcx*8]");
            else {
                codegen_emit(cg, "    mov rdx, %d", elem_size);
                codegen_emit(cg, "    imul rcx, rdx");
                codegen_emit(cg, "    lea rax, [rbx + rcx]");
            }
            return TYPE_STRUCT;
        }
        if (elem_size != 8) {
            codegen_emit(cg, "    mov rdx, %d", elem_size);
            codegen_emit(cg, "    imul rcx, rdx");
            codegen_emit(cg, "    add rbx, rcx");
            if (elem == TYPE_F32) {
                codegen_emit(cg, "    movss xmm0, [rbx]");
                return TYPE_F32;
            }
            if (elem == TYPE_F64) {
                codegen_emit(cg, "    movsd xmm0, [rbx]");
                return TYPE_F64;
            }
            codegen_emit(cg, "    mov rax, [rbx]");
            return elem;
        }
        if (elem == TYPE_F32) {
            codegen_emit(cg, "    movss xmm0, [rbx + rcx*8]");
            return TYPE_F32;
        }
        if (elem == TYPE_F64) {
            codegen_emit(cg, "    movsd xmm0, [rbx + rcx*8]");
            return TYPE_F64;
        }
        codegen_emit(cg, "    mov rax, [rbx + rcx*8]");
        return elem;
    }

    case AST_TUPLE_ACCESS: {
        ASTNode *base = expr->data.tuple_access.tuple;
        if (base->type != AST_IDENTIFIER)
            error("tuple access requires an identifier base");
        int idx = codegen_find_var(cg, base->data.identifier);
        if (idx == -1)
            error("Undefined variable: %s", base->data.identifier);
        if (cg->var_types[idx] != TYPE_TUPLE)
            error("Not a tuple: %s", base->data.identifier);

        TypeInfo *ti = cg->var_typeinfo[idx];
        if (!ti || expr->data.tuple_access.index < 0 ||
            expr->data.tuple_access.index >= ti->tuple_count)
            error("Tuple index out of range");

        ValueType elem = ti->tuple_types[expr->data.tuple_access.index];
        if (ti->by_ref) {
            codegen_emit(cg, "    mov rbx, [rbp - %d]", cg->var_offsets[idx]);
            if (elem == TYPE_F32) {
                codegen_emit(cg, "    movss xmm0, [rbx + %d]", expr->data.tuple_access.index * 8);
                return TYPE_F32;
            }
            if (elem == TYPE_F64) {
                codegen_emit(cg, "    movsd xmm0, [rbx + %d]", expr->data.tuple_access.index * 8);
                return TYPE_F64;
            }
            codegen_emit(cg, "    mov rax, [rbx + %d]", expr->data.tuple_access.index * 8);
            return elem;
        }
        int elem_off = cg->var_offsets[idx] - (expr->data.tuple_access.index * 8);
        if (elem == TYPE_F32) {
            codegen_emit(cg, "    movss xmm0, [rbp - %d]", elem_off);
            return TYPE_F32;
        }
        if (elem == TYPE_F64) {
            codegen_emit(cg, "    movsd xmm0, [rbp - %d]", elem_off);
            return TYPE_F64;
        }
        codegen_emit(cg, "    mov rax, [rbp - %d]", elem_off);
        return elem;
    }

    case AST_FIELD_ACCESS: {
        StructDef *sd = emit_struct_addr(cg, expr->data.field_access.base);
        struct_size(cg, sd);
        int fidx = struct_field_index(sd, expr->data.field_access.field_name);
        if (fidx < 0)
            error("Unknown field '%s' on %s",
                  expr->data.field_access.field_name, sd->name);

        ValueType ft = sd->field_types[fidx];
        int off = sd->field_offsets[fidx];
        if (ft == TYPE_STRUCT) {
            codegen_emit(cg, "    add rax, %d", off);
            return TYPE_STRUCT;
        }
        codegen_emit(cg, "    mov rbx, rax");
        if (ft == TYPE_F32) {
            codegen_emit(cg, "    movss xmm0, [rbx + %d]", off);
            return TYPE_F32;
        }
        if (ft == TYPE_F64) {
            codegen_emit(cg, "    movsd xmm0, [rbx + %d]", off);
            return TYPE_F64;
        }
        codegen_emit(cg, "    mov rax, [rbx + %d]", off);
        return ft;
    }

    case AST_IDENTIFIER: {
        int idx = codegen_find_var(cg, expr->data.identifier);
        if (idx == -1)
            error("Undefined variable: %s", expr->data.identifier);
        int off = cg->var_offsets[idx];
        ValueType t = cg->var_types[idx];
        if (t == TYPE_ARRAY || t == TYPE_TUPLE || t == TYPE_STRUCT) {
            TypeInfo *ti = cg->var_typeinfo[idx];
            if (t == TYPE_STRUCT && !ti)
                error("struct missing type info: %s", expr->data.identifier);
            bool by_ref = ti && ti->by_ref;
            bool dyn_arr = (t == TYPE_ARRAY && ti && ti->array_len < 0);
            if (by_ref || dyn_arr)
                codegen_emit(cg, "    mov rax, [rbp - %d]", off);
            else
                codegen_emit(cg, "    lea rax, [rbp - %d]", off);
            return t;
        }
        if (t == TYPE_F32) {
            codegen_emit(cg, "    movss xmm0, [rbp - %d]", off);
            return TYPE_F32;
        }
        if (t == TYPE_F64) {
            codegen_emit(cg, "    movsd xmm0, [rbp - %d]", off);
            return TYPE_F64;
        }
        codegen_emit(cg, "    mov rax, [rbp - %d]", off);
        return t;
    }

    case AST_BINARY_OP: {
        ValueType lt = codegen_expression(cg, expr->data.binary.left);
        if (is_float(lt)) {
            codegen_emit(cg, "    sub rsp, 8");
            if (lt == TYPE_F32)
                codegen_emit(cg, "    movss [rsp], xmm0");
            else
                codegen_emit(cg, "    movsd [rsp], xmm0");
        } else {
            codegen_emit(cg, "    push rax");
        }

        ValueType rt = codegen_expression(cg, expr->data.binary.right);
        bool is_null_cmp =
            (expr->data.binary.op == OP_EQ || expr->data.binary.op == OP_NE) &&
            ((is_optional(lt) && rt == TYPE_NULL) ||
             (lt == TYPE_NULL && is_optional(rt)) ||
             (lt == TYPE_NULL && rt == TYPE_NULL));

        if (is_null_cmp) {
            unsigned long long sentinel = 0;
            bool use_eax = false;
            ValueType opt_t = is_optional(lt) ? lt : rt;
            switch (opt_t) {
                case TYPE_OPT_STRING: sentinel = 0; break;
                case TYPE_OPT_INT: sentinel = 0x8000000000000000ULL; break;
                case TYPE_OPT_BOOL: sentinel = 2; break;
                case TYPE_OPT_F64: sentinel = 0x7ff8000000000001ULL; break;
                case TYPE_OPT_F32: sentinel = 0x7fc00001ULL; use_eax = true; break;
                default: sentinel = 0; break;
            }

            if (lt == TYPE_NULL && rt == TYPE_NULL) {
                codegen_emit(cg, "    pop rbx");
                codegen_emit(cg, "    mov rax, 1");
                if (expr->data.binary.op == OP_NE)
                    codegen_emit(cg, "    mov rax, 0");
                return TYPE_BOOL;
            }

            if (is_optional(lt)) {
                codegen_emit(cg, "    pop rbx");
                if (use_eax) {
                    codegen_emit(cg, "    cmp ebx, 0x%08x", (unsigned int)sentinel);
                } else {
                    codegen_emit(cg, "    mov rcx, 0x%llx", sentinel);
                    codegen_emit(cg, "    cmp rbx, rcx");
                }
            } else {
                codegen_emit(cg, "    pop rbx");
                if (use_eax) {
                    codegen_emit(cg, "    cmp eax, 0x%08x", (unsigned int)sentinel);
                } else {
                    codegen_emit(cg, "    mov rcx, 0x%llx", sentinel);
                    codegen_emit(cg, "    cmp rax, rcx");
                }
            }
            codegen_emit(cg, "    mov rax, 0");
            if (expr->data.binary.op == OP_EQ)
                codegen_emit(cg, "    sete  al");
            else
                codegen_emit(cg, "    setne al");
            return TYPE_BOOL;
        }

        if (is_string_like(lt) || is_string_like(rt) ||
            is_null_type(lt) || is_null_type(rt) ||
            is_optional(lt) || is_optional(rt) ||
            is_composite(lt) || is_composite(rt)) {
            ValueType bad = (is_string_like(lt) || is_null_type(lt) || is_optional(lt) || is_composite(lt)) ? lt : rt;
            error("Invalid operand type for binary op: %s", value_type_name(bad));
        }

        if (!is_float(lt) && !is_float(rt)) {
            codegen_emit(cg, "    mov rbx, rax");
            codegen_emit(cg, "    pop rax");
            switch (expr->data.binary.op) {
            case OP_ADD: codegen_emit(cg, "    add rax, rbx");  break;
            case OP_SUB: codegen_emit(cg, "    sub rax, rbx");  break;
            case OP_MUL: codegen_emit(cg, "    imul rax, rbx"); break;
            case OP_DIV:
                codegen_emit(cg, "    cqo");
                codegen_emit(cg, "    idiv rbx");
                break;
            case OP_MOD:
                codegen_emit(cg, "    cqo");
                codegen_emit(cg, "    idiv rbx");
                codegen_emit(cg, "    mov rax, rdx");
                break;
            default:
                codegen_emit(cg, "    cmp rax, rbx");
                codegen_emit(cg, "    mov rax, 0");
                switch (expr->data.binary.op) {
                case OP_EQ: codegen_emit(cg, "    sete  al"); break;
                case OP_NE: codegen_emit(cg, "    setne al"); break;
                case OP_LT: codegen_emit(cg, "    setl  al"); break;
                case OP_LE: codegen_emit(cg, "    setle al"); break;
                case OP_GT: codegen_emit(cg, "    setg  al"); break;
                case OP_GE: codegen_emit(cg, "    setge al"); break;
                default: break;
                }
                return TYPE_BOOL;
            }
            return TYPE_INT;
        }

        if (expr->data.binary.op == OP_MOD) {
            error("Modulo is not supported for floating-point values");
        }

        ValueType resf = promote_float(lt, rt);

        /* convert right to resf in xmm0 */
        if (rt == TYPE_INT) {
            if (resf == TYPE_F64)
                codegen_emit(cg, "    cvtsi2sd xmm0, rax");
            else
                codegen_emit(cg, "    cvtsi2ss xmm0, rax");
        } else if (rt == TYPE_F32 && resf == TYPE_F64) {
            codegen_emit(cg, "    cvtss2sd xmm0, xmm0");
        } else if (rt == TYPE_F64 && resf == TYPE_F32) {
            codegen_emit(cg, "    cvtsd2ss xmm0, xmm0");
        }

        /* load left into xmm1 with conversion */
        if (lt == TYPE_INT) {
            codegen_emit(cg, "    pop rbx");
            if (resf == TYPE_F64)
                codegen_emit(cg, "    cvtsi2sd xmm1, rbx");
            else
                codegen_emit(cg, "    cvtsi2ss xmm1, rbx");
        } else if (lt == TYPE_F32) {
            codegen_emit(cg, "    movss xmm1, [rsp]");
            codegen_emit(cg, "    add rsp, 8");
            if (resf == TYPE_F64)
                codegen_emit(cg, "    cvtss2sd xmm1, xmm1");
        } else {
            codegen_emit(cg, "    movsd xmm1, [rsp]");
            codegen_emit(cg, "    add rsp, 8");
            if (resf == TYPE_F32)
                codegen_emit(cg, "    cvtsd2ss xmm1, xmm1");
        }

        if (expr->data.binary.op == OP_EQ ||
            expr->data.binary.op == OP_NE ||
            expr->data.binary.op == OP_LT ||
            expr->data.binary.op == OP_LE ||
            expr->data.binary.op == OP_GT ||
            expr->data.binary.op == OP_GE) {
            if (resf == TYPE_F64)
                codegen_emit(cg, "    ucomisd xmm1, xmm0");
            else
                codegen_emit(cg, "    ucomiss xmm1, xmm0");
            codegen_emit(cg, "    mov rax, 0");
            switch (expr->data.binary.op) {
            case OP_EQ: codegen_emit(cg, "    sete  al"); break;
            case OP_NE: codegen_emit(cg, "    setne al"); break;
            case OP_LT: codegen_emit(cg, "    setb  al"); break;
            case OP_LE: codegen_emit(cg, "    setbe al"); break;
            case OP_GT: codegen_emit(cg, "    seta  al"); break;
            case OP_GE: codegen_emit(cg, "    setae al"); break;
            default: break;
            }
            return TYPE_BOOL;
        }

        if (resf == TYPE_F64) {
            switch (expr->data.binary.op) {
            case OP_ADD: codegen_emit(cg, "    addsd xmm1, xmm0"); break;
            case OP_SUB: codegen_emit(cg, "    subsd xmm1, xmm0"); break;
            case OP_MUL: codegen_emit(cg, "    mulsd xmm1, xmm0"); break;
            case OP_DIV: codegen_emit(cg, "    divsd xmm1, xmm0"); break;
            default: break;
            }
            codegen_emit(cg, "    movapd xmm0, xmm1");
        } else {
            switch (expr->data.binary.op) {
            case OP_ADD: codegen_emit(cg, "    addss xmm1, xmm0"); break;
            case OP_SUB: codegen_emit(cg, "    subss xmm1, xmm0"); break;
            case OP_MUL: codegen_emit(cg, "    mulss xmm1, xmm0"); break;
            case OP_DIV: codegen_emit(cg, "    divss xmm1, xmm0"); break;
            default: break;
            }
            codegen_emit(cg, "    movaps xmm0, xmm1");
        }
        return resf;
    }

    case AST_CALL: {
        int argc = expr->data.call.arg_count;
        const char *call_name = expr->data.call.func_name;
        char mangled[MAX_TOKEN_LEN * 2 + 4];
        const char *sig_name = call_name;

        if (strcmp(call_name, "map") == 0 || strcmp(call_name, "filter") == 0) {
            bool is_filter = (strcmp(call_name, "filter") == 0);
            if (argc != 2)
                error("%s() expects 2 arguments", call_name);

            ASTNode *arr_expr = expr->data.call.args[0];
            ASTNode *lam_expr = expr->data.call.args[1];
            if (!lam_expr || lam_expr->type != AST_LAMBDA)
                error("%s() expects a lambda as the second argument", call_name);

            ValueType at = infer_expr_type(cg, arr_expr);
            if (at != TYPE_ARRAY)
                error("%s() requires an array as first argument", call_name);
            TypeInfo *arr_ti = expr_typeinfo(cg, arr_expr);
            if (!arr_ti)
                error("%s() requires array type info", call_name);

            ValueType elem_t = arr_ti->elem_type;
            if (elem_t == TYPE_STRUCT)
                error("%s() does not support arrays of bina yet", call_name);
            ASTNode *lambda = lam_expr;
            if (lambda->data.lambda.has_type &&
                lambda->data.lambda.param_type != elem_t) {
                error("lambda param type does not match array element type");
            }

            int saved_count = cg->var_count;
            int saved_offset = cg->stack_offset;

            codegen_add_var(cg, lambda->data.lambda.param_name, elem_t, true, NULL);

            ValueType ret_t = infer_expr_type(cg, lambda->data.lambda.body);
            if (is_filter && ret_t != TYPE_BOOL)
                error("filter() lambda must return bool");
            if (!is_filter && is_composite(ret_t))
                error("map() lambda cannot return composite types");
            if (!is_filter && ret_t == TYPE_NULL)
                error("map() lambda cannot return null");
            if (!is_filter && ret_t == TYPE_OPT_STRING)
                error("map() lambda cannot return optional lafz");

            /* evaluate array expression to pointer in rax */
            ValueType rt = codegen_expression(cg, arr_expr);
            (void)rt;

            char in_name[32], out_name[32], len_name[32], idx_name[32], out_idx_name[32];
            snprintf(in_name, sizeof(in_name), "_map_in%d", codegen_new_label(cg));
            snprintf(out_name, sizeof(out_name), "_map_out%d", codegen_new_label(cg));
            snprintf(len_name, sizeof(len_name), "_map_len%d", codegen_new_label(cg));
            snprintf(idx_name, sizeof(idx_name), "_map_idx%d", codegen_new_label(cg));
            snprintf(out_idx_name, sizeof(out_idx_name), "_map_outi%d", codegen_new_label(cg));

            codegen_add_var(cg, in_name, TYPE_INT, true, NULL);
            int in_off = cg->var_offsets[cg->var_count - 1];
            codegen_emit(cg, "    mov [rbp - %d], rax", in_off);

            /* length */
            codegen_add_var(cg, len_name, TYPE_INT, true, NULL);
            int len_off = cg->var_offsets[cg->var_count - 1];
            if (arr_ti->array_len >= 0) {
                codegen_emit(cg, "    mov rax, %d", arr_ti->array_len);
            } else {
                codegen_emit(cg, "    mov rbx, [rbp - %d]", in_off);
                codegen_emit(cg, "    mov rax, [rbx - 8]");
            }
            codegen_emit(cg, "    mov [rbp - %d], rax", len_off);

            /* allocate output array */
            cg->needs_arr_alloc = true;
            codegen_emit(cg, "    mov rdi, [rbp - %d]", len_off);
            codegen_emit(cg, "    mov rsi, 8");
            codegen_emit(cg, "    call __vl_arr_alloc");
            codegen_add_var(cg, out_name, TYPE_INT, true, NULL);
            int out_off = cg->var_offsets[cg->var_count - 1];
            codegen_emit(cg, "    mov [rbp - %d], rax", out_off);

            /* idx and out_idx */
            codegen_add_var(cg, idx_name, TYPE_INT, true, NULL);
            int idx_off = cg->var_offsets[cg->var_count - 1];
            codegen_emit(cg, "    mov qword [rbp - %d], 0", idx_off);

            int out_idx_off = 0;
            if (is_filter) {
                codegen_add_var(cg, out_idx_name, TYPE_INT, true, NULL);
                out_idx_off = cg->var_offsets[cg->var_count - 1];
                codegen_emit(cg, "    mov qword [rbp - %d], 0", out_idx_off);
            }

            int loop_lbl = codegen_new_label(cg);
            int end_lbl = codegen_new_label(cg);
            int skip_lbl = codegen_new_label(cg);

            codegen_emit(cg, "_VL%d:", loop_lbl);
            codegen_emit(cg, "    mov rax, [rbp - %d]", idx_off);
            codegen_emit(cg, "    cmp rax, [rbp - %d]", len_off);
            codegen_emit(cg, "    jge _VL%d", end_lbl);

            /* load element */
            codegen_emit(cg, "    mov rcx, [rbp - %d]", idx_off);
            codegen_emit(cg, "    mov rbx, [rbp - %d]", in_off);
            if (elem_t == TYPE_F32) {
                codegen_emit(cg, "    movss xmm0, [rbx + rcx*8]");
                codegen_emit(cg, "    movss [rbp - %d], xmm0",
                             cg->var_offsets[saved_count]);
            } else if (elem_t == TYPE_F64) {
                codegen_emit(cg, "    movsd xmm0, [rbx + rcx*8]");
                codegen_emit(cg, "    movsd [rbp - %d], xmm0",
                             cg->var_offsets[saved_count]);
            } else {
                codegen_emit(cg, "    mov rax, [rbx + rcx*8]");
                codegen_emit(cg, "    mov [rbp - %d], rax",
                             cg->var_offsets[saved_count]);
            }

            /* evaluate lambda body */
            ValueType body_t = codegen_expression(cg, lambda->data.lambda.body);

            if (is_filter) {
                if (body_t != TYPE_BOOL)
                    error("filter() lambda must return bool");
                codegen_emit(cg, "    cmp rax, 0");
                codegen_emit(cg, "    je  _VL%d", skip_lbl);
                codegen_emit(cg, "    mov rcx, [rbp - %d]", idx_off);
                codegen_emit(cg, "    mov rbx, [rbp - %d]", in_off);
                if (elem_t == TYPE_F32) {
                    codegen_emit(cg, "    movss xmm0, [rbx + rcx*8]");
                    codegen_emit(cg, "    mov rcx, [rbp - %d]", out_idx_off);
                    codegen_emit(cg, "    mov rbx, [rbp - %d]", out_off);
                    codegen_emit(cg, "    movss [rbx + rcx*8], xmm0");
                } else if (elem_t == TYPE_F64) {
                    codegen_emit(cg, "    movsd xmm0, [rbx + rcx*8]");
                    codegen_emit(cg, "    mov rcx, [rbp - %d]", out_idx_off);
                    codegen_emit(cg, "    mov rbx, [rbp - %d]", out_off);
                    codegen_emit(cg, "    movsd [rbx + rcx*8], xmm0");
                } else {
                    codegen_emit(cg, "    mov rax, [rbx + rcx*8]");
                    codegen_emit(cg, "    mov rcx, [rbp - %d]", out_idx_off);
                    codegen_emit(cg, "    mov rbx, [rbp - %d]", out_off);
                    codegen_emit(cg, "    mov [rbx + rcx*8], rax");
                }
                codegen_emit(cg, "    mov rax, [rbp - %d]", out_idx_off);
                codegen_emit(cg, "    add rax, 1");
                codegen_emit(cg, "    mov [rbp - %d], rax", out_idx_off);
                codegen_emit(cg, "_VL%d:", skip_lbl);
            } else {
                if (body_t != ret_t)
                    body_t = codegen_cast(cg, body_t, ret_t);
                codegen_emit(cg, "    mov rcx, [rbp - %d]", idx_off);
                codegen_emit(cg, "    mov rbx, [rbp - %d]", out_off);
                if (ret_t == TYPE_F32)
                    codegen_emit(cg, "    movss [rbx + rcx*8], xmm0");
                else if (ret_t == TYPE_F64)
                    codegen_emit(cg, "    movsd [rbx + rcx*8], xmm0");
                else
                    codegen_emit(cg, "    mov [rbx + rcx*8], rax");
            }

            /* idx++ */
            codegen_emit(cg, "    mov rax, [rbp - %d]", idx_off);
            codegen_emit(cg, "    add rax, 1");
            codegen_emit(cg, "    mov [rbp - %d], rax", idx_off);
            codegen_emit(cg, "    jmp _VL%d", loop_lbl);
            codegen_emit(cg, "_VL%d:", end_lbl);

            if (is_filter) {
                codegen_emit(cg, "    mov rax, [rbp - %d]", out_off);
                codegen_emit(cg, "    mov rcx, [rbp - %d]", out_idx_off);
                codegen_emit(cg, "    mov [rax - 8], rcx");
            }

            codegen_emit(cg, "    mov rax, [rbp - %d]", out_off);

            for (int i = saved_count; i < cg->var_count; i++)
                free(cg->local_vars[i]);
            cg->var_count = saved_count;
            cg->stack_offset = saved_offset;

            return TYPE_ARRAY;
        }

        if (strcmp(call_name, "len") == 0) {
            if (argc != 1)
                error("len() expects 1 argument");
            ASTNode *arg = expr->data.call.args[0];
            ValueType at = codegen_expression(cg, arg);
            TypeInfo *ti = expr_typeinfo(cg, arg);

            if (at == TYPE_STRING || at == TYPE_OPT_STRING) {
                int loop_lbl = codegen_new_label(cg);
                int done_lbl = codegen_new_label(cg);
                int null_lbl = codegen_new_label(cg);
                int end_lbl  = codegen_new_label(cg);

                if (at == TYPE_OPT_STRING) {
                    codegen_emit(cg, "    cmp rax, 0");
                    codegen_emit(cg, "    je  _VL%d", null_lbl);
                }
                codegen_emit(cg, "    mov rbx, rax");
                codegen_emit(cg, "    xor rcx, rcx");
                codegen_emit(cg, "_VL%d:", loop_lbl);
                codegen_emit(cg, "    mov dl, [rbx + rcx]");
                codegen_emit(cg, "    cmp dl, 0");
                codegen_emit(cg, "    je  _VL%d", done_lbl);
                codegen_emit(cg, "    inc rcx");
                codegen_emit(cg, "    jmp _VL%d", loop_lbl);
                codegen_emit(cg, "_VL%d:", done_lbl);
                codegen_emit(cg, "    mov rax, rcx");
                if (at == TYPE_OPT_STRING) {
                    codegen_emit(cg, "    jmp _VL%d", end_lbl);
                    codegen_emit(cg, "_VL%d:", null_lbl);
                    codegen_emit(cg, "    xor rax, rax");
                    codegen_emit(cg, "_VL%d:", end_lbl);
                }
                return TYPE_INT;
            }

            if (at == TYPE_ARRAY) {
                if (ti && ti->array_len >= 0) {
                    codegen_emit(cg, "    mov rax, %d", ti->array_len);
                } else {
                    codegen_emit(cg, "    mov rbx, rax");
                    codegen_emit(cg, "    mov rax, [rbx - 8]");
                }
                return TYPE_INT;
            }

            if (at == TYPE_TUPLE) {
                if (!ti || ti->tuple_count <= 0)
                    error("len() requires tuple type info");
                codegen_emit(cg, "    mov rax, %d", ti->tuple_count);
                return TYPE_INT;
            }

            if (at == TYPE_STRUCT) {
                if (!ti)
                    error("len() requires struct type info");
                StructDef *sd = struct_find(cg, ti->struct_name);
                if (!sd)
                    error("Unknown struct type: %s", ti->struct_name);
                codegen_emit(cg, "    mov rax, %d", sd->field_count);
                return TYPE_INT;
            }

            error("len() not supported for type: %s", value_type_name(at));
        }

        if (strcmp(call_name, "sizeof") == 0) {
            if (argc != 1)
                error("sizeof() expects 1 argument");
            ASTNode *arg = expr->data.call.args[0];
            ValueType at = infer_expr_type(cg, arg);
            TypeInfo *ti = expr_typeinfo(cg, arg);
            TypeInfo tmp;
            if (arg->type == AST_STRUCT_LITERAL) {
                memset(&tmp, 0, sizeof(tmp));
                tmp.kind = TYPE_STRUCT;
                strncpy(tmp.struct_name,
                        arg->data.struct_lit.struct_name,
                        MAX_TOKEN_LEN - 1);
                ti = &tmp;
            }
            int sz = sizeof_type(cg, at, ti);
            codegen_emit(cg, "    mov rax, %d", sz);
            return TYPE_INT;
        }

        if (strcmp(call_name, "type") == 0) {
            if (argc != 1)
                error("type() expects 1 argument");
            ASTNode *arg = expr->data.call.args[0];
            ValueType at = infer_expr_type(cg, arg);
            TypeInfo *ti = expr_typeinfo(cg, arg);
            TypeInfo tmp;
            if (arg->type == AST_STRUCT_LITERAL) {
                memset(&tmp, 0, sizeof(tmp));
                tmp.kind = TYPE_STRUCT;
                strncpy(tmp.struct_name,
                        arg->data.struct_lit.struct_name,
                        MAX_TOKEN_LEN - 1);
                ti = &tmp;
            }
            /* evaluate for side effects */
            codegen_expression(cg, arg);
            char *tname = type_name_string(cg, at, ti);
            int sid = codegen_intern_string(cg, tname);
            free(tname);
            codegen_emit(cg, "    lea rax, [rel _VL_str_%d]", sid);
            return TYPE_STRING;
        }

        if (cg->current_module[0] != '\0' &&
            mod_func_table_has(expr->data.call.func_name)) {
            snprintf(mangled, sizeof(mangled), "%s__%s",
                     cg->current_module, expr->data.call.func_name);
            sig_name = mangled;
        }

        FunctionSig *sig = func_sig_find(cg, sig_name);
        bool ret_tuple = sig && sig->return_type == TYPE_TUPLE;
        bool ret_struct = sig && sig->return_type == TYPE_STRUCT;
        bool ret_sret = ret_tuple || ret_struct;
        if (ret_tuple && (!sig->return_typeinfo || sig->return_typeinfo->tuple_count <= 0))
            error("tuple return type missing info");
        if (ret_struct && !sig->return_typeinfo)
            error("struct return type missing info");
        if (ret_sret && argc > 5)
            error("More than 5 arguments not supported for sret calls");
        if (!ret_sret && argc > 6)
            error("More than 6 arguments not supported");

        int sret_off = 0;
        if (ret_sret) {
            char tmpname[32];
            snprintf(tmpname, sizeof(tmpname), "_tmp%d", codegen_new_label(cg));
            TypeInfo *ti = typeinfo_clone(sig->return_typeinfo);
            codegen_add_var(cg, tmpname, sig->return_type, true, ti);
            sret_off = cg->var_offsets[cg->var_count - 1];
        }

        /* push args right-to-left, pop into registers */
        for (int i = argc - 1; i >= 0; i--) {
            ValueType at = codegen_expression(cg, expr->data.call.args[i]);
            ValueType expect = sig && i < sig->param_count ? sig->param_types[i] : at;
            if (expect == TYPE_STRING) expect = at;
            if (at != expect) {
                codegen_cast(cg, at, expect);
                at = expect;
            }
            if (is_float(at)) {
                if (at == TYPE_F64)
                    codegen_emit(cg, "    movq rax, xmm0");
                else
                    codegen_emit(cg, "    movd eax, xmm0");
                codegen_emit(cg, "    push rax");
            } else {
                codegen_emit(cg, "    push rax");
            }
        }
        if (ret_sret) {
            codegen_emit(cg, "    lea rax, [rbp - %d]", sret_off);
            codegen_emit(cg, "    push rax");
        }
        for (int i = 0; i < argc + (ret_sret ? 1 : 0); i++)
            codegen_emit(cg, "    pop %s", ARG_REGS[i]);

        if (cg->current_module[0] != '\0' &&
            mod_func_table_has(expr->data.call.func_name)) {
            codegen_emit(cg, "    call %s__%s",
                         cg->current_module,
                         expr->data.call.func_name);
        } else {
            codegen_emit(cg, "    call %s", expr->data.call.func_name);
        }
        if (ret_sret) {
            codegen_emit(cg, "    lea rax, [rbp - %d]", sret_off);
            return sig->return_type;
        }
        FunctionSig *retsig = func_sig_find(cg, sig_name);
        return retsig ? retsig->return_type : TYPE_INT;
    }

    case AST_MODULE_CALL: {
        /* module.function() — always use mangled name */
        int argc = expr->data.module_call.arg_count;
        if (!warned_io_input &&
            strcmp(expr->data.module_call.module_name, "io") == 0 &&
            strcmp(expr->data.module_call.func_name, "input") == 0) {
            fprintf(stderr, "Warning: io.input() is deprecated; use io.stdin() instead.\n");
            warned_io_input = true;
        }

        if (strcmp(expr->data.module_call.module_name, "system") == 0 &&
            strcmp(expr->data.module_call.func_name, "argv") == 0 &&
            argc == 0) {
            cg->needs_arr_alloc = true;
            int lbl = codegen_new_label(cg);
            codegen_emit(cg, "    mov rdi, [rel _VL_argc]");
            codegen_emit(cg, "    mov rsi, 8");
            codegen_emit(cg, "    call __vl_arr_alloc");
            codegen_emit(cg, "    mov r10, rax");
            codegen_emit(cg, "    mov rcx, [rel _VL_argc]");
            codegen_emit(cg, "    mov rdx, [rel _VL_argv]");
            codegen_emit(cg, "    xor r8, r8");
            codegen_emit(cg, "_VLargv_loop_%d:", lbl);
            codegen_emit(cg, "    cmp r8, rcx");
            codegen_emit(cg, "    jae _VLargv_done_%d", lbl);
            codegen_emit(cg, "    mov r11, [rdx + r8*8]");
            codegen_emit(cg, "    mov [r10 + r8*8], r11");
            codegen_emit(cg, "    inc r8");
            codegen_emit(cg, "    jmp _VLargv_loop_%d", lbl);
            codegen_emit(cg, "_VLargv_done_%d:", lbl);
            codegen_emit(cg, "    mov rax, r10");
            return TYPE_ARRAY;
        }

        if (strcmp(expr->data.module_call.module_name, "random") == 0 &&
            strcmp(expr->data.module_call.func_name, "choice") == 0) {
            if (argc != 1)
                error("random.choice() expects 1 argument");

            ASTNode *arg = expr->data.module_call.args[0];
            ValueType at = infer_expr_type(cg, arg);
            TypeInfo *ti = expr_typeinfo(cg, arg);
            ValueType elem = TYPE_INT;
            int arr_len = -1;
            bool is_tuple = false;
            int tuple_count = 0;

            if (at == TYPE_ARRAY) {
                if (!ti && arg->type == AST_CALL &&
                    (strcmp(arg->data.call.func_name, "map") == 0 ||
                     strcmp(arg->data.call.func_name, "filter") == 0)) {
                    ASTNode *arr_expr = arg->data.call.args[0];
                    ASTNode *lam_expr = arg->data.call.args[1];
                    if (infer_expr_type(cg, arr_expr) != TYPE_ARRAY)
                        error("random.choice() requires an array");
                    TypeInfo *arr_ti = expr_typeinfo(cg, arr_expr);
                    if (!arr_ti)
                        error("random.choice() requires array type info");
                    elem = (strcmp(arg->data.call.func_name, "filter") == 0)
                             ? arr_ti->elem_type
                             : infer_expr_type(cg, lam_expr->data.lambda.body);
                    arr_len = -1;
                } else {
                    if (!ti)
                        error("random.choice() requires array type info");
                    elem = ti->elem_type;
                    arr_len = ti->array_len;
                }
                if (arr_len == 0)
                    error("random.choice() cannot choose from an empty array");
                if (elem == TYPE_STRUCT)
                    error("random.choice() does not support bina elements yet");
            } else if (at == TYPE_TUPLE) {
                if (!ti || ti->tuple_count <= 0)
                    error("random.choice() requires tuple type info");
                elem = ti->tuple_types[0];
                for (int i = 1; i < ti->tuple_count; i++) {
                    if (ti->tuple_types[i] != elem)
                        error("random.choice() requires uniform tuple element types");
                }
                if (elem == TYPE_STRUCT)
                    error("random.choice() does not support bina elements yet");
                is_tuple = true;
                tuple_count = ti->tuple_count;
            } else {
                error("random.choice() requires an array or tuple");
            }

            codegen_expression(cg, arg);
            codegen_emit(cg, "    push rax");
            if (is_tuple) {
                codegen_emit(cg, "    mov rdi, %d", tuple_count);
            } else if (arr_len >= 0) {
                codegen_emit(cg, "    mov rdi, %d", arr_len);
            } else {
                codegen_emit(cg, "    mov rbx, [rsp]");
                codegen_emit(cg, "    mov rdi, [rbx - 8]");
            }
            codegen_emit(cg, "    call random__randrange");

            if (is_tuple) {
                codegen_emit(cg, "    pop rbx");
                int end_lbl = codegen_new_label(cg);
                for (int i = 0; i < tuple_count; i++) {
                    int next_lbl = codegen_new_label(cg);
                    codegen_emit(cg, "    cmp rax, %d", i);
                    codegen_emit(cg, "    jne _VL%d", next_lbl);
                    if (elem == TYPE_F32)
                        codegen_emit(cg, "    movss xmm0, [rbx + %d]", i * 8);
                    else if (elem == TYPE_F64)
                        codegen_emit(cg, "    movsd xmm0, [rbx + %d]", i * 8);
                    else
                        codegen_emit(cg, "    mov rax, [rbx + %d]", i * 8);
                    codegen_emit(cg, "    jmp _VL%d", end_lbl);
                    codegen_emit(cg, "_VL%d:", next_lbl);
                }
                codegen_emit(cg, "_VL%d:", end_lbl);
                return elem;
            }

            codegen_emit(cg, "    mov rcx, rax");
            codegen_emit(cg, "    pop rbx");
            if (elem == TYPE_F32) {
                codegen_emit(cg, "    movss xmm0, [rbx + rcx*8]");
                return TYPE_F32;
            }
            if (elem == TYPE_F64) {
                codegen_emit(cg, "    movsd xmm0, [rbx + rcx*8]");
                return TYPE_F64;
            }
            codegen_emit(cg, "    mov rax, [rbx + rcx*8]");
            return elem;
        }

        /* Back-compat: io.chhaap(x) => io.chhaap("%d", x) */
        if (strcmp(expr->data.module_call.module_name, "io") == 0 &&
            strcmp(expr->data.module_call.func_name, "chhaap") == 0 &&
            argc == 1 &&
            expr->data.module_call.args[0]->type != AST_STRING) {
            if (2 > 6) error("More than 6 arguments not supported");

            /* push args right-to-left: value, then format */
            ValueType at = codegen_expression(cg, expr->data.module_call.args[0]);
            const char *fmt = "%d";
            if (at == TYPE_STRING || at == TYPE_OPT_STRING) fmt = "%s";
            else if (at == TYPE_BOOL) fmt = "%b";
            else if (is_float(at)) fmt = "%f";

            if (is_float(at)) {
                if (at == TYPE_F32)
                    codegen_emit(cg, "    cvtss2sd xmm0, xmm0");
                codegen_emit(cg, "    movq rax, xmm0");
            }
            codegen_emit(cg, "    push rax");
            int sid = codegen_intern_string(cg, fmt);
            codegen_emit(cg, "    lea rax, [rel _VL_str_%d]", sid);
            codegen_emit(cg, "    push rax");
            codegen_emit(cg, "    pop %s", ARG_REGS[0]);
            codegen_emit(cg, "    pop %s", ARG_REGS[1]);

            codegen_emit(cg, "    call %s__%s",
                         expr->data.module_call.module_name,
                         expr->data.module_call.func_name);
            return TYPE_INT;
        }

        char full_name[MAX_TOKEN_LEN * 2 + 4];
        snprintf(full_name, sizeof(full_name), "%s__%s",
                 expr->data.module_call.module_name,
                 expr->data.module_call.func_name);

        bool is_io_printf =
            strcmp(expr->data.module_call.module_name, "io") == 0 &&
            strcmp(expr->data.module_call.func_name, "chhaap") == 0;

        FunctionSig *sig = func_sig_find(cg, full_name);
        bool ret_tuple = sig && sig->return_type == TYPE_TUPLE;
        bool ret_struct = sig && sig->return_type == TYPE_STRUCT;
        bool ret_sret = ret_tuple || ret_struct;
        if (ret_tuple && (!sig->return_typeinfo || sig->return_typeinfo->tuple_count <= 0))
            error("tuple return type missing info");
        if (ret_struct && !sig->return_typeinfo)
            error("struct return type missing info");
        if (ret_sret && argc > 5)
            error("More than 5 arguments not supported for sret calls");
        if (!ret_sret && argc > 6)
            error("More than 6 arguments not supported");

        int sret_off = 0;
        if (ret_sret) {
            char tmpname[32];
            snprintf(tmpname, sizeof(tmpname), "_tmp%d", codegen_new_label(cg));
            TypeInfo *ti = typeinfo_clone(sig->return_typeinfo);
            codegen_add_var(cg, tmpname, sig->return_type, true, ti);
            sret_off = cg->var_offsets[cg->var_count - 1];
        }

        for (int i = argc - 1; i >= 0; i--) {
            ValueType at = codegen_expression(cg, expr->data.module_call.args[i]);
            ValueType expect = sig && i < sig->param_count ? sig->param_types[i] : at;

            if (is_io_printf) {
                if (is_float(at)) {
                    if (at == TYPE_F32)
                        codegen_emit(cg, "    cvtss2sd xmm0, xmm0");
                    at = TYPE_F64;
                }
                expect = at;
            }

            if (at != expect) {
                codegen_cast(cg, at, expect);
                at = expect;
            }

            if (is_float(at)) {
                if (at == TYPE_F64)
                    codegen_emit(cg, "    movq rax, xmm0");
                else
                    codegen_emit(cg, "    movd eax, xmm0");
                codegen_emit(cg, "    push rax");
            } else {
                codegen_emit(cg, "    push rax");
            }
        }
        if (ret_sret) {
            codegen_emit(cg, "    lea rax, [rbp - %d]", sret_off);
            codegen_emit(cg, "    push rax");
        }
        for (int i = 0; i < argc + (ret_sret ? 1 : 0); i++)
            codegen_emit(cg, "    pop %s", ARG_REGS[i]);

        codegen_emit(cg, "    call %s", full_name);
        if (ret_sret) {
            codegen_emit(cg, "    lea rax, [rbp - %d]", sret_off);
            return sig->return_type;
        }
        return sig ? sig->return_type : TYPE_INT;
    }

    default:
        error("codegen_expression: unknown node type %d", expr->type);
    }
    return TYPE_INT;
}

/* ── Statement codegen ────────────────────────────────────────────── */

void codegen_statement(CodeGen *cg, ASTNode *stmt) {
    if (!stmt) return;

    switch (stmt->type) {

    case AST_RETURN:
        if (cg->current_return_type == TYPE_TUPLE) {
            if (!stmt->data.ret.value)
                error("tuple return requires a value");
            if (!cg->current_return_typeinfo)
                error("tuple return type missing info");
            codegen_emit(cg, "    mov rbx, [rbp - %d]", cg->sret_offset);
            if (stmt->data.ret.value->type == AST_TUPLE_LITERAL) {
                ASTNode *lit = stmt->data.ret.value;
                if (lit->data.tuple_lit.count != cg->current_return_typeinfo->tuple_count)
                    error("tuple literal length does not match return type");
                for (int i = 0; i < lit->data.tuple_lit.count; i++) {
                    ValueType at = codegen_expression(cg, lit->data.tuple_lit.elements[i]);
                    ValueType expect = cg->current_return_typeinfo->tuple_types[i];
                    if (at != expect)
                        at = codegen_cast(cg, at, expect);
                    codegen_emit(cg, "    mov rbx, [rbp - %d]", cg->sret_offset);
                    if (expect == TYPE_F32)
                        codegen_emit(cg, "    movss [rbx + %d], xmm0", i * 8);
                    else if (expect == TYPE_F64)
                        codegen_emit(cg, "    movsd [rbx + %d], xmm0", i * 8);
                    else
                        codegen_emit(cg, "    mov [rbx + %d], rax", i * 8);
                }
            } else {
                ValueType rt = codegen_expression(cg, stmt->data.ret.value);
                if (rt != TYPE_TUPLE)
                    error("return type mismatch: expected tuple");
                codegen_emit(cg, "    mov rcx, rax");
                for (int i = 0; i < cg->current_return_typeinfo->tuple_count; i++) {
                    codegen_emit(cg, "    mov rax, [rcx + %d]", i * 8);
                    codegen_emit(cg, "    mov [rbx + %d], rax", i * 8);
                }
            }
            codegen_emit(cg, "    mov rax, rbx");
        } else if (cg->current_return_type == TYPE_STRUCT) {
            if (!stmt->data.ret.value)
                error("struct return requires a value");
            if (!cg->current_return_typeinfo)
                error("struct return type missing info");
            StructDef *sd = struct_find(cg, cg->current_return_typeinfo->struct_name);
            if (!sd)
                error("Unknown struct type: %s", cg->current_return_typeinfo->struct_name);
            codegen_emit(cg, "    mov rbx, [rbp - %d]", cg->sret_offset);
            ASTNode *val = stmt->data.ret.value;
            if (val->type == AST_STRUCT_LITERAL) {
                codegen_emit_struct_literal(cg, sd, val);
            } else {
                ValueType rt = codegen_expression(cg, val);
                if (rt != TYPE_STRUCT)
                    error("return type mismatch: expected struct");
                codegen_emit(cg, "    mov rcx, rax");
                codegen_copy_struct(cg, sd, "rbx", "rcx");
            }
            codegen_emit(cg, "    mov rax, rbx");
        } else if (stmt->data.ret.value) {
            ValueType rt = codegen_expression(cg, stmt->data.ret.value);
            if (rt != cg->current_return_type)
                codegen_cast(cg, rt, cg->current_return_type);
        } else {
            if (cg->current_return_type == TYPE_F32)
                codegen_emit(cg, "    xorps xmm0, xmm0");
            else if (cg->current_return_type == TYPE_F64)
                codegen_emit(cg, "    xorpd xmm0, xmm0");
            else
                codegen_emit(cg, "    mov rax, 0");
        }
        codegen_emit(cg, "    mov rsp, rbp");
        codegen_emit(cg, "    pop rbp");
        codegen_emit(cg, "    ret");
        break;

    case AST_LET:
    {
        ValueType decl = stmt->data.let.has_type ? stmt->data.let.var_type : TYPE_INT;

        if (!stmt->data.let.has_type &&
            stmt->data.let.value->type == AST_STRUCT_LITERAL) {
            TypeInfo *ti = (TypeInfo*)calloc(1, sizeof(TypeInfo));
            if (!ti) error("Out of memory");
            ti->kind = TYPE_STRUCT;
            strncpy(ti->struct_name,
                    stmt->data.let.value->data.struct_lit.struct_name,
                    MAX_TOKEN_LEN - 1);
            stmt->data.let.type_info = ti;
            stmt->data.let.var_type = TYPE_STRUCT;
            stmt->data.let.has_type = true;
            decl = TYPE_STRUCT;
        } else if (!stmt->data.let.has_type) {
            ASTNode *val = stmt->data.let.value;
            if (val->type == AST_IDENTIFIER ||
                val->type == AST_CALL ||
                val->type == AST_MODULE_CALL ||
                val->type == AST_FIELD_ACCESS ||
                val->type == AST_ARRAY_ACCESS) {
                ValueType vt = infer_expr_type(cg, val);
                if (vt == TYPE_STRUCT || vt == TYPE_ARRAY || vt == TYPE_TUPLE) {
                    TypeInfo *vti = expr_typeinfo(cg, val);
                    if (!vti)
                        error("initializer missing type info");
                    TypeInfo *ti = typeinfo_clone(vti);
                    if (ti) ti->by_ref = false;
                    stmt->data.let.type_info = ti;
                    stmt->data.let.var_type = vt;
                    stmt->data.let.has_type = true;
                    decl = vt;
                }
            }
        }

        if (stmt->data.let.has_type && is_composite(decl)) {
            if (decl == TYPE_ARRAY && stmt->data.let.value->type != AST_ARRAY_LITERAL) {
                if (!stmt->data.let.type_info ||
                    stmt->data.let.type_info->array_len >= 0)
                    error("array declarations require an array literal initializer");
            }
            if (!stmt->data.let.type_info)
                error("missing type info for composite type");

            codegen_add_var(cg, stmt->data.let.var_name, decl,
                            stmt->data.let.is_mut, stmt->data.let.type_info);
            int idx = cg->var_count - 1;
            int off = cg->var_offsets[idx];

            if (decl == TYPE_ARRAY) {
                TypeInfo *ti = stmt->data.let.type_info;
                ASTNode *lit = stmt->data.let.value;
                int elem_size = array_elem_size(cg, ti);
                StructDef *esd = NULL;
                if (ti->elem_type == TYPE_STRUCT) {
                    if (!ti->elem_typeinfo)
                        error("array element missing struct type info");
                    esd = struct_find(cg, ti->elem_typeinfo->struct_name);
                    if (!esd)
                        error("Unknown struct type: %s", ti->elem_typeinfo->struct_name);
                }
                if (lit->type != AST_ARRAY_LITERAL) {
                    if (ti->array_len >= 0)
                        error("fixed-size arrays require a literal initializer");
                    ValueType vt = codegen_expression(cg, lit);
                    if (vt != TYPE_ARRAY)
                        error("array initializer must be an array expression");
                    codegen_emit(cg, "    mov [rbp - %d], rax", off);
                } else if (ti->array_len < 0) {
                    cg->needs_arr_alloc = true;
                    codegen_emit(cg, "    mov rdi, %d", lit->data.array_lit.count);
                    codegen_emit(cg, "    mov rsi, %d", elem_size);
                    codegen_emit(cg, "    call __vl_arr_alloc");
                    codegen_emit(cg, "    mov [rbp - %d], rax", off);
                    for (int i = 0; i < lit->data.array_lit.count; i++) {
                        int off_bytes = i * elem_size;
                        if (ti->elem_type == TYPE_STRUCT) {
                            if (lit->data.array_lit.elements[i]->type == AST_STRUCT_LITERAL) {
                                codegen_emit(cg, "    mov rbx, [rbp - %d]", off);
                                codegen_emit(cg, "    lea rbx, [rbx + %d]", off_bytes);
                                codegen_emit_struct_literal(cg, esd, lit->data.array_lit.elements[i]);
                            } else {
                                ValueType at = codegen_expression(cg, lit->data.array_lit.elements[i]);
                                if (at != TYPE_STRUCT)
                                    error("array element expects %s", esd->name);
                                codegen_emit(cg, "    mov rbx, [rbp - %d]", off);
                                codegen_emit(cg, "    lea rdx, [rbx + %d]", off_bytes);
                                codegen_copy_struct(cg, esd, "rdx", "rax");
                            }
                        } else {
                            ValueType at = codegen_expression(cg, lit->data.array_lit.elements[i]);
                            ValueType expect = ti->elem_type;
                            if (at != expect)
                                at = codegen_cast(cg, at, expect);
                            codegen_emit(cg, "    mov rbx, [rbp - %d]", off);
                            if (expect == TYPE_F32)
                                codegen_emit(cg, "    movss [rbx + %d], xmm0", off_bytes);
                            else if (expect == TYPE_F64)
                                codegen_emit(cg, "    movsd [rbx + %d], xmm0", off_bytes);
                            else
                                codegen_emit(cg, "    mov [rbx + %d], rax", off_bytes);
                        }
                    }
                } else {
                    if (lit->data.array_lit.count != ti->array_len)
                        error("array literal length does not match type");
                    for (int i = 0; i < lit->data.array_lit.count; i++) {
                        int elem_off = off - (i * elem_size);
                        if (ti->elem_type == TYPE_STRUCT) {
                            if (lit->data.array_lit.elements[i]->type == AST_STRUCT_LITERAL) {
                                codegen_emit(cg, "    lea rbx, [rbp - %d]", elem_off);
                                codegen_emit_struct_literal(cg, esd, lit->data.array_lit.elements[i]);
                            } else {
                                ValueType at = codegen_expression(cg, lit->data.array_lit.elements[i]);
                                if (at != TYPE_STRUCT)
                                    error("array element expects %s", esd->name);
                                codegen_emit(cg, "    lea rdx, [rbp - %d]", elem_off);
                                codegen_copy_struct(cg, esd, "rdx", "rax");
                            }
                        } else {
                            ValueType at = codegen_expression(cg, lit->data.array_lit.elements[i]);
                            ValueType expect = ti->elem_type;
                            if (at != expect)
                                at = codegen_cast(cg, at, expect);
                            if (expect == TYPE_F32)
                                codegen_emit(cg, "    movss [rbp - %d], xmm0", elem_off);
                            else if (expect == TYPE_F64)
                                codegen_emit(cg, "    movsd [rbp - %d], xmm0", elem_off);
                            else
                                codegen_emit(cg, "    mov [rbp - %d], rax", elem_off);
                        }
                    }
                }
                break;
            }

            if (decl == TYPE_TUPLE) {
                TypeInfo *ti = stmt->data.let.type_info;
                ASTNode *val = stmt->data.let.value;
                if (val->type == AST_TUPLE_LITERAL) {
                    if (val->data.tuple_lit.count != ti->tuple_count)
                        error("tuple literal length does not match type");
                    for (int i = 0; i < val->data.tuple_lit.count; i++) {
                        ValueType at = codegen_expression(cg, val->data.tuple_lit.elements[i]);
                        ValueType expect = ti->tuple_types[i];
                        if (at != expect)
                            at = codegen_cast(cg, at, expect);
                        int elem_off = off - (i * 8);
                        if (expect == TYPE_F32)
                            codegen_emit(cg, "    movss [rbp - %d], xmm0", elem_off);
                        else if (expect == TYPE_F64)
                            codegen_emit(cg, "    movsd [rbp - %d], xmm0", elem_off);
                        else
                            codegen_emit(cg, "    mov [rbp - %d], rax", elem_off);
                    }
                } else {
                    ValueType at = codegen_expression(cg, val);
                    if (at != TYPE_TUPLE)
                        error("tuple initializer must be tuple literal or tuple expression");
                    codegen_emit(cg, "    mov rcx, rax");
                    for (int i = 0; i < ti->tuple_count; i++) {
                        int elem_off = off - (i * 8);
                        codegen_emit(cg, "    mov rax, [rcx + %d]", i * 8);
                        codegen_emit(cg, "    mov [rbp - %d], rax", elem_off);
                    }
                }
                break;
            }

            if (decl == TYPE_STRUCT) {
                TypeInfo *ti = stmt->data.let.type_info;
                ASTNode *val = stmt->data.let.value;
                if (!ti)
                    error("missing struct type info");
                StructDef *sd = struct_find(cg, ti->struct_name);
                if (!sd)
                    error("Unknown struct type: %s", ti->struct_name);
                int off = cg->var_offsets[idx];
                if (val->type == AST_STRUCT_LITERAL) {
                    codegen_emit(cg, "    lea rbx, [rbp - %d]", off);
                    codegen_emit_struct_literal(cg, sd, val);
                } else {
                    ValueType vt = codegen_expression(cg, val);
                    if (vt != TYPE_STRUCT)
                        error("struct declarations require a struct initializer");
                    TypeInfo *vti = expr_typeinfo(cg, val);
                    if (!vti || strcmp(vti->struct_name, ti->struct_name) != 0)
                        error("struct type mismatch: expected %s", ti->struct_name);
                    codegen_emit(cg, "    lea rbx, [rbp - %d]", off);
                    codegen_emit(cg, "    mov rcx, rax");
                    codegen_copy_struct(cg, sd, "rbx", "rcx");
                }
                break;
            }
        }

        if (!stmt->data.let.has_type &&
            (stmt->data.let.value->type == AST_ARRAY_LITERAL ||
             stmt->data.let.value->type == AST_TUPLE_LITERAL)) {
            error("array/tuple literals require explicit type annotations");
        }

        ValueType vt = codegen_expression(cg, stmt->data.let.value);
        decl = stmt->data.let.has_type ? stmt->data.let.var_type : vt;
        if (vt != decl)
            vt = codegen_cast(cg, vt, decl);
        codegen_add_var(cg, stmt->data.let.var_name, decl, stmt->data.let.is_mut,
                        stmt->data.let.type_info);
        if (decl == TYPE_F32)
            codegen_emit(cg, "    movss [rbp - %d], xmm0", cg->stack_offset);
        else if (decl == TYPE_F64)
            codegen_emit(cg, "    movsd [rbp - %d], xmm0", cg->stack_offset);
        else
            codegen_emit(cg, "    mov [rbp - %d], rax", cg->stack_offset);
        break;
    }

    case AST_ASSIGN: {
        ValueType vt = codegen_expression(cg, stmt->data.assign.value);
        int idx = codegen_find_var(cg, stmt->data.assign.var_name);
        if (idx == -1)
            error("Undefined variable: %s", stmt->data.assign.var_name);
        if (!cg->var_mutable[idx])
            error("Cannot assign to immutable variable: %s", stmt->data.assign.var_name);
        int off = cg->var_offsets[idx];
        ValueType decl = cg->var_types[idx];
        if (is_composite(decl) && decl != TYPE_STRUCT)
            error("Assignment to composite types is not supported yet");
        if (decl == TYPE_STRUCT) {
            TypeInfo *ti = cg->var_typeinfo[idx];
            if (!ti)
                error("struct missing type info: %s", stmt->data.assign.var_name);
            StructDef *sd = struct_find(cg, ti->struct_name);
            if (!sd)
                error("Unknown struct type: %s", ti->struct_name);
            if (ti->by_ref)
                codegen_emit(cg, "    mov rbx, [rbp - %d]", off);
            else
                codegen_emit(cg, "    lea rbx, [rbp - %d]", off);
            ASTNode *val = stmt->data.assign.value;
            if (val->type == AST_STRUCT_LITERAL) {
                codegen_emit_struct_literal(cg, sd, val);
            } else {
                if (vt != TYPE_STRUCT)
                    error("struct assignment requires struct value");
                TypeInfo *vti = expr_typeinfo(cg, val);
                if (!vti || strcmp(vti->struct_name, ti->struct_name) != 0)
                    error("struct type mismatch: expected %s", ti->struct_name);
                codegen_emit(cg, "    mov rcx, rax");
                codegen_copy_struct(cg, sd, "rbx", "rcx");
            }
            break;
        }
        if (vt != decl)
            vt = codegen_cast(cg, vt, decl);
        if (decl == TYPE_F32)
            codegen_emit(cg, "    movss [rbp - %d], xmm0", off);
        else if (decl == TYPE_F64)
            codegen_emit(cg, "    movsd [rbp - %d], xmm0", off);
        else
            codegen_emit(cg, "    mov [rbp - %d], rax", off);
        break;
    }

    case AST_FIELD_ASSIGN: {
        int idx = codegen_find_var(cg, stmt->data.field_assign.var_name);
        if (idx == -1)
            error("Undefined variable: %s", stmt->data.field_assign.var_name);
        if (!cg->var_mutable[idx])
            error("Cannot assign to immutable variable: %s", stmt->data.field_assign.var_name);
        if (cg->var_types[idx] != TYPE_STRUCT)
            error("Not a struct: %s", stmt->data.field_assign.var_name);
        TypeInfo *ti = cg->var_typeinfo[idx];
        if (!ti)
            error("struct missing type info: %s", stmt->data.field_assign.var_name);
        StructDef *sd = struct_find(cg, ti->struct_name);
        if (!sd)
            error("Unknown struct type: %s", ti->struct_name);
        struct_size(cg, sd);

        int fidx = struct_field_index(sd, stmt->data.field_assign.field_name);
        if (fidx < 0)
            error("Unknown field '%s' on %s",
                  stmt->data.field_assign.field_name, sd->name);

        int off = sd->field_offsets[fidx];
        ValueType expect = sd->field_types[fidx];

        if (ti->by_ref)
            codegen_emit(cg, "    mov rbx, [rbp - %d]", cg->var_offsets[idx]);
        else
            codegen_emit(cg, "    lea rbx, [rbp - %d]", cg->var_offsets[idx]);

        if (expect == TYPE_STRUCT) {
            TypeInfo *fti = sd->field_typeinfo ? sd->field_typeinfo[fidx] : NULL;
            if (!fti)
                error("struct field missing type info: %s.%s",
                      sd->name, sd->field_names[fidx]);
            StructDef *child = struct_find(cg, fti->struct_name);
            if (!child)
                error("Unknown struct type: %s", fti->struct_name);

            ASTNode *val = stmt->data.field_assign.value;
            if (val->type == AST_STRUCT_LITERAL) {
                codegen_emit(cg, "    lea rdx, [rbx + %d]", off);
                codegen_emit(cg, "    push rbx");
                codegen_emit(cg, "    mov rbx, rdx");
                codegen_emit_struct_literal(cg, child, val);
                codegen_emit(cg, "    pop rbx");
            } else {
                codegen_emit(cg, "    push rbx");
                ValueType vt = codegen_expression(cg, val);
                codegen_emit(cg, "    pop rbx");
                if (vt != TYPE_STRUCT)
                    error("struct field '%s' expects %s",
                          stmt->data.field_assign.field_name, child->name);
                codegen_emit(cg, "    mov rcx, rax");
                codegen_emit(cg, "    lea rdx, [rbx + %d]", off);
                codegen_copy_struct(cg, child, "rdx", "rcx");
            }
            break;
        }

        codegen_emit(cg, "    push rbx");
        ValueType vt = codegen_expression(cg, stmt->data.field_assign.value);
        codegen_emit(cg, "    pop rbx");
        if (vt != expect)
            vt = codegen_cast(cg, vt, expect);

        if (expect == TYPE_F32)
            codegen_emit(cg, "    movss [rbx + %d], xmm0", off);
        else if (expect == TYPE_F64)
            codegen_emit(cg, "    movsd [rbx + %d], xmm0", off);
        else
            codegen_emit(cg, "    mov [rbx + %d], rax", off);
        break;
    }

    case AST_ARRAY_ASSIGN: {
        int idx = codegen_find_var(cg, stmt->data.array_assign.var_name);
        if (idx == -1)
            error("Undefined variable: %s", stmt->data.array_assign.var_name);
        if (!cg->var_mutable[idx])
            error("Cannot assign to immutable variable: %s", stmt->data.array_assign.var_name);
        if (cg->var_types[idx] != TYPE_ARRAY)
            error("Not an array: %s", stmt->data.array_assign.var_name);
        TypeInfo *ti = cg->var_typeinfo[idx];
        if (!ti)
            error("array missing type info");
        ValueType elem = ti->elem_type;
        int elem_size = array_elem_size(cg, ti);

        ValueType it = codegen_expression(cg, stmt->data.array_assign.index);
        if (it != TYPE_INT)
            codegen_cast(cg, it, TYPE_INT);
        codegen_emit(cg, "    mov rcx, rax");

        if (ti->by_ref || ti->array_len < 0)
            codegen_emit(cg, "    mov rbx, [rbp - %d]", cg->var_offsets[idx]);
        else
            codegen_emit(cg, "    lea rbx, [rbp - %d]", cg->var_offsets[idx]);

        if (elem == TYPE_STRUCT) {
            StructDef *sd = struct_find(cg, ti->elem_typeinfo->struct_name);
            if (!sd)
                error("Unknown struct type: %s", ti->elem_typeinfo->struct_name);
            if (elem_size == 8) {
                codegen_emit(cg, "    lea rdx, [rbx + rcx*8]");
            } else {
                codegen_emit(cg, "    mov rdx, %d", elem_size);
                codegen_emit(cg, "    imul rcx, rdx");
                codegen_emit(cg, "    lea rdx, [rbx + rcx]");
            }
            codegen_emit(cg, "    push rdx");
            if (stmt->data.array_assign.value->type == AST_STRUCT_LITERAL) {
                codegen_emit(cg, "    pop rbx");
                codegen_emit_struct_literal(cg, sd, stmt->data.array_assign.value);
            } else {
                ValueType vt = codegen_expression(cg, stmt->data.array_assign.value);
                if (vt != TYPE_STRUCT)
                    error("array assignment expects %s", sd->name);
                codegen_emit(cg, "    pop rdx");
                codegen_copy_struct(cg, sd, "rdx", "rax");
            }
            break;
        }

        ValueType vt = codegen_expression(cg, stmt->data.array_assign.value);
        ValueType expect = ti->elem_type;
        if (vt != expect)
            vt = codegen_cast(cg, vt, expect);

        if (elem_size != 8) {
            codegen_emit(cg, "    mov rdx, %d", elem_size);
            codegen_emit(cg, "    imul rcx, rdx");
            codegen_emit(cg, "    add rbx, rcx");
            if (expect == TYPE_F32)
                codegen_emit(cg, "    movss [rbx], xmm0");
            else if (expect == TYPE_F64)
                codegen_emit(cg, "    movsd [rbx], xmm0");
            else
                codegen_emit(cg, "    mov [rbx], rax");
        } else {
            if (expect == TYPE_F32)
                codegen_emit(cg, "    movss [rbx + rcx*8], xmm0");
            else if (expect == TYPE_F64)
                codegen_emit(cg, "    movsd [rbx + rcx*8], xmm0");
            else
                codegen_emit(cg, "    mov [rbx + rcx*8], rax");
        }
        break;
    }

    case AST_IF: {
        int else_lbl = codegen_new_label(cg);
        int end_lbl  = codegen_new_label(cg);

        ValueType ct = codegen_expression(cg, stmt->data.if_stmt.condition);
        if (is_composite(ct) || is_optional(ct))
            error("composite types cannot be used as conditions");
        if (is_float(ct))
            codegen_cast(cg, ct, TYPE_INT);
        codegen_emit(cg, "    cmp rax, 0");
        codegen_emit(cg, "    je  _VL%d", else_lbl);

        for (int i = 0; i < stmt->data.if_stmt.then_count; i++)
            codegen_statement(cg, stmt->data.if_stmt.then_stmts[i]);
        codegen_emit(cg, "    jmp _VL%d", end_lbl);

        codegen_emit(cg, "_VL%d:", else_lbl);
        if (stmt->data.if_stmt.else_stmts)
            for (int i = 0; i < stmt->data.if_stmt.else_count; i++)
                codegen_statement(cg, stmt->data.if_stmt.else_stmts[i]);

        codegen_emit(cg, "_VL%d:", end_lbl);
        break;
    }

    case AST_WHILE: {
        int start_lbl = codegen_new_label(cg);
        int end_lbl   = codegen_new_label(cg);

        codegen_emit(cg, "_VL%d:", start_lbl);
        ValueType ct = codegen_expression(cg, stmt->data.while_stmt.condition);
        if (is_composite(ct) || is_optional(ct))
            error("composite types cannot be used as conditions");
        if (is_float(ct))
            codegen_cast(cg, ct, TYPE_INT);
        codegen_emit(cg, "    cmp rax, 0");
        codegen_emit(cg, "    je  _VL%d", end_lbl);

        loop_push(cg, start_lbl, end_lbl);
        for (int i = 0; i < stmt->data.while_stmt.body_count; i++)
            codegen_statement(cg, stmt->data.while_stmt.body[i]);
        loop_pop(cg);

        codegen_emit(cg, "    jmp _VL%d", start_lbl);
        codegen_emit(cg, "_VL%d:", end_lbl);
        break;
    }

    case AST_FOR: {
        if (stmt->data.for_stmt.init)
            codegen_statement(cg, stmt->data.for_stmt.init);

        int start_lbl = codegen_new_label(cg);
        int cont_lbl  = codegen_new_label(cg);
        int end_lbl   = codegen_new_label(cg);

        codegen_emit(cg, "_VL%d:", start_lbl);
        if (stmt->data.for_stmt.condition) {
            ValueType ct = codegen_expression(cg, stmt->data.for_stmt.condition);
            if (is_composite(ct) || is_optional(ct))
                error("composite types cannot be used as conditions");
            if (is_float(ct))
                codegen_cast(cg, ct, TYPE_INT);
            codegen_emit(cg, "    cmp rax, 0");
            codegen_emit(cg, "    je  _VL%d", end_lbl);
        }

        loop_push(cg, cont_lbl, end_lbl);
        for (int i = 0; i < stmt->data.for_stmt.body_count; i++)
            codegen_statement(cg, stmt->data.for_stmt.body[i]);
        loop_pop(cg);

        codegen_emit(cg, "_VL%d:", cont_lbl);
        if (stmt->data.for_stmt.post)
            codegen_statement(cg, stmt->data.for_stmt.post);

        codegen_emit(cg, "    jmp _VL%d", start_lbl);
        codegen_emit(cg, "_VL%d:", end_lbl);
        break;
    }

    case AST_FOR_IN: {
        ASTNode *iter = stmt->data.for_in_stmt.iterable;
        bool has_type = stmt->data.for_in_stmt.has_type;
        ValueType user_type = stmt->data.for_in_stmt.var_type;

        if (iter && iter->type == AST_RANGE) {
            if (has_type && user_type != TYPE_INT)
                error("range for-in requires 'adad' loop variable");

            char endname[32];
            snprintf(endname, sizeof(endname), "_bar_end%d", codegen_new_label(cg));
            codegen_add_var(cg, stmt->data.for_in_stmt.var_name, TYPE_INT,
                            stmt->data.for_in_stmt.is_mut, NULL);
            int var_idx = cg->var_count - 1;

            codegen_add_var(cg, endname, TYPE_INT, false, NULL);
            int end_idx = cg->var_count - 1;

            char stepname[32];
            snprintf(stepname, sizeof(stepname), "_bar_step%d", codegen_new_label(cg));
            codegen_add_var(cg, stepname, TYPE_INT, false, NULL);
            int step_idx = cg->var_count - 1;

            ValueType st = codegen_expression(cg, iter->data.range.start);
            if (is_composite(st) || is_string_like(st) || is_optional(st) || st == TYPE_NULL)
                error("range start must be numeric");
            if (is_float(st))
                codegen_cast(cg, st, TYPE_INT);
            codegen_emit(cg, "    mov [rbp - %d], rax", cg->var_offsets[var_idx]);

            ValueType et = codegen_expression(cg, iter->data.range.end);
            if (is_composite(et) || is_string_like(et) || is_optional(et) || et == TYPE_NULL)
                error("range end must be numeric");
            if (is_float(et))
                codegen_cast(cg, et, TYPE_INT);
            codegen_emit(cg, "    mov [rbp - %d], rax", cg->var_offsets[end_idx]);

            if (iter->data.range.step) {
                ValueType stp = codegen_expression(cg, iter->data.range.step);
                if (is_composite(stp) || is_string_like(stp) || is_optional(stp) || stp == TYPE_NULL)
                    error("range step must be numeric");
                if (is_float(stp))
                    codegen_cast(cg, stp, TYPE_INT);
                codegen_emit(cg, "    mov [rbp - %d], rax", cg->var_offsets[step_idx]);
            } else {
                codegen_emit(cg, "    mov rax, 1");
                codegen_emit(cg, "    mov [rbp - %d], rax", cg->var_offsets[step_idx]);
            }

            int start_lbl = codegen_new_label(cg);
            int cont_lbl  = codegen_new_label(cg);
            int end_lbl   = codegen_new_label(cg);

            codegen_emit(cg, "    mov rax, [rbp - %d]", cg->var_offsets[step_idx]);
            codegen_emit(cg, "    cmp rax, 0");
            codegen_emit(cg, "    jle _VL%d", end_lbl);

            codegen_emit(cg, "_VL%d:", start_lbl);
            codegen_emit(cg, "    mov rax, [rbp - %d]", cg->var_offsets[var_idx]);
            codegen_emit(cg, "    mov rbx, [rbp - %d]", cg->var_offsets[end_idx]);
            codegen_emit(cg, "    cmp rax, rbx");
            if (iter->data.range.inclusive)
                codegen_emit(cg, "    jg  _VL%d", end_lbl);
            else
                codegen_emit(cg, "    jge _VL%d", end_lbl);

            loop_push(cg, cont_lbl, end_lbl);
            for (int i = 0; i < stmt->data.for_in_stmt.body_count; i++)
                codegen_statement(cg, stmt->data.for_in_stmt.body[i]);
            loop_pop(cg);

            codegen_emit(cg, "_VL%d:", cont_lbl);
            codegen_emit(cg, "    mov rax, [rbp - %d]", cg->var_offsets[var_idx]);
            codegen_emit(cg, "    mov rbx, [rbp - %d]", cg->var_offsets[step_idx]);
            codegen_emit(cg, "    add rax, rbx");
            codegen_emit(cg, "    mov [rbp - %d], rax", cg->var_offsets[var_idx]);
            codegen_emit(cg, "    jmp _VL%d", start_lbl);
            codegen_emit(cg, "_VL%d:", end_lbl);
            break;
        }

        int arr_idx = -1;
        TypeInfo *ti = NULL;
        ValueType elem = TYPE_INT;

        if (!iter)
            error("for-in requires an iterable");

        if (iter->type == AST_IDENTIFIER) {
            arr_idx = codegen_find_var(cg, iter->data.identifier);
            if (arr_idx == -1)
                error("Undefined variable: %s", iter->data.identifier);
            if (cg->var_types[arr_idx] != TYPE_ARRAY)
                error("for-in requires an array");
            ti = cg->var_typeinfo[arr_idx];
            if (!ti)
                error("array missing type info");
        } else if (iter->type == AST_CALL || iter->type == AST_MODULE_CALL) {
            const char *sig_name = NULL;
            FunctionSig *sig = NULL;
            char mangled[MAX_TOKEN_LEN * 2 + 4];
            if (iter->type == AST_CALL) {
                const char *call_name = iter->data.call.func_name;
                sig_name = call_name;
                if (cg->current_module[0] != '\0' &&
                    mod_func_table_has(call_name)) {
                    snprintf(mangled, sizeof(mangled), "%s__%s",
                             cg->current_module, call_name);
                    sig_name = mangled;
                }
                sig = func_sig_find(cg, sig_name);
            } else {
                snprintf(mangled, sizeof(mangled), "%s__%s",
                         iter->data.module_call.module_name,
                         iter->data.module_call.func_name);
                sig_name = mangled;
                sig = func_sig_find(cg, sig_name);
            }
            if (!sig || sig->return_type != TYPE_ARRAY || !sig->return_typeinfo)
                error("for-in call must return an array with type info");

            ValueType rt = codegen_expression(cg, iter);
            if (rt != TYPE_ARRAY)
                error("for-in call did not return an array");

            char arrname[32];
            snprintf(arrname, sizeof(arrname), "_bar_arr%d", codegen_new_label(cg));
            TypeInfo *arr_ti = typeinfo_clone(sig->return_typeinfo);
            codegen_add_var(cg, arrname, TYPE_ARRAY, true, arr_ti);
            arr_idx = cg->var_count - 1;
            codegen_emit(cg, "    mov [rbp - %d], rax", cg->var_offsets[arr_idx]);
            ti = arr_ti;
        } else if (iter->type == AST_ARRAY_LITERAL) {
            ValueType inferred = user_type;
            if (!has_type) {
                inferred = infer_array_lit_elem_type(cg, iter);
            }
            if (is_composite(inferred) || inferred == TYPE_NULL)
                error("for-in array literal requires primitive loop variable type");

            char arrname[32];
            snprintf(arrname, sizeof(arrname), "_bar_arr%d", codegen_new_label(cg));
            TypeInfo *arr_ti = (TypeInfo*)calloc(1, sizeof(TypeInfo));
            if (!arr_ti) error("Out of memory");
            arr_ti->kind = TYPE_ARRAY;
            arr_ti->elem_type = inferred;
            arr_ti->array_len = iter->data.array_lit.count;
            codegen_add_var(cg, arrname, TYPE_ARRAY, true, arr_ti);
            arr_idx = cg->var_count - 1;
            int off = cg->var_offsets[arr_idx];

            for (int i = 0; i < iter->data.array_lit.count; i++) {
                ValueType at = codegen_expression(cg, iter->data.array_lit.elements[i]);
                ValueType expect = inferred;
                if (at != expect)
                    at = codegen_cast(cg, at, expect);
                int elem_off = off - (i * 8);
                if (expect == TYPE_F32)
                    codegen_emit(cg, "    movss [rbp - %d], xmm0", elem_off);
                else if (expect == TYPE_F64)
                    codegen_emit(cg, "    movsd [rbp - %d], xmm0", elem_off);
                else
                    codegen_emit(cg, "    mov [rbp - %d], rax", elem_off);
            }
            ti = arr_ti;
        } else {
            error("for-in requires an array identifier, array literal, or array-returning call");
        }

        elem = ti->elem_type;
        if (elem == TYPE_ARRAY || elem == TYPE_TUPLE)
            error("for-in does not support composite element types yet");

        if (has_type && user_type != elem)
            error("for-in variable type does not match array element type");

        char idxname[32];
        char lenname[32];
        snprintf(idxname, sizeof(idxname), "_bar_idx%d", codegen_new_label(cg));
        snprintf(lenname, sizeof(lenname), "_bar_len%d", codegen_new_label(cg));

        TypeInfo *elem_ti = NULL;
        if (elem == TYPE_STRUCT) {
            if (!ti->elem_typeinfo)
                error("array element missing struct type info");
            elem_ti = typeinfo_ref(ti->elem_typeinfo);
        }
        codegen_add_var(cg, stmt->data.for_in_stmt.var_name, elem,
                        stmt->data.for_in_stmt.is_mut, elem_ti);
        int var_idx = cg->var_count - 1;

        codegen_add_var(cg, idxname, TYPE_INT, true, NULL);
        int idx_idx = cg->var_count - 1;

        codegen_add_var(cg, lenname, TYPE_INT, false, NULL);
        int len_idx = cg->var_count - 1;

        codegen_emit(cg, "    mov rax, 0");
        codegen_emit(cg, "    mov [rbp - %d], rax", cg->var_offsets[idx_idx]);

        if (ti->array_len < 0) {
            codegen_emit(cg, "    mov rbx, [rbp - %d]", cg->var_offsets[arr_idx]);
            codegen_emit(cg, "    mov rax, [rbx - 8]");
        } else {
            codegen_emit(cg, "    mov rax, %d", ti->array_len);
        }
        codegen_emit(cg, "    mov [rbp - %d], rax", cg->var_offsets[len_idx]);

        int start_lbl = codegen_new_label(cg);
        int cont_lbl  = codegen_new_label(cg);
        int end_lbl   = codegen_new_label(cg);

        codegen_emit(cg, "_VL%d:", start_lbl);
        codegen_emit(cg, "    mov rax, [rbp - %d]", cg->var_offsets[idx_idx]);
        codegen_emit(cg, "    mov rbx, [rbp - %d]", cg->var_offsets[len_idx]);
        codegen_emit(cg, "    cmp rax, rbx");
        codegen_emit(cg, "    jge _VL%d", end_lbl);

        codegen_emit(cg, "    mov rcx, rax");
        if (ti->array_len < 0)
            codegen_emit(cg, "    mov rbx, [rbp - %d]", cg->var_offsets[arr_idx]);
        else
            codegen_emit(cg, "    lea rbx, [rbp - %d]", cg->var_offsets[arr_idx]);

        int elem_size = array_elem_size(cg, ti);
        if (elem == TYPE_STRUCT) {
            if (elem_size == 8) {
                codegen_emit(cg, "    lea rax, [rbx + rcx*8]");
            } else {
                codegen_emit(cg, "    mov rdx, %d", elem_size);
                codegen_emit(cg, "    imul rcx, rdx");
                codegen_emit(cg, "    lea rax, [rbx + rcx]");
            }
            codegen_emit(cg, "    mov [rbp - %d], rax", cg->var_offsets[var_idx]);
        } else if (elem == TYPE_F32) {
            codegen_emit(cg, "    movss xmm0, [rbx + rcx*8]");
            codegen_emit(cg, "    movss [rbp - %d], xmm0", cg->var_offsets[var_idx]);
        } else if (elem == TYPE_F64) {
            codegen_emit(cg, "    movsd xmm0, [rbx + rcx*8]");
            codegen_emit(cg, "    movsd [rbp - %d], xmm0", cg->var_offsets[var_idx]);
        } else {
            codegen_emit(cg, "    mov rax, [rbx + rcx*8]");
            codegen_emit(cg, "    mov [rbp - %d], rax", cg->var_offsets[var_idx]);
        }

        loop_push(cg, cont_lbl, end_lbl);
        for (int i = 0; i < stmt->data.for_in_stmt.body_count; i++)
            codegen_statement(cg, stmt->data.for_in_stmt.body[i]);
        loop_pop(cg);

        codegen_emit(cg, "_VL%d:", cont_lbl);
        codegen_emit(cg, "    mov rax, [rbp - %d]", cg->var_offsets[idx_idx]);
        codegen_emit(cg, "    add rax, 1");
        codegen_emit(cg, "    mov [rbp - %d], rax", cg->var_offsets[idx_idx]);
        codegen_emit(cg, "    jmp _VL%d", start_lbl);
        codegen_emit(cg, "_VL%d:", end_lbl);
        break;
    }

    case AST_BREAK: {
        int lbl = loop_break_label(cg);
        codegen_emit(cg, "    jmp _VL%d", lbl);
        break;
    }

    case AST_CONTINUE: {
        int lbl = loop_continue_label(cg);
        codegen_emit(cg, "    jmp _VL%d", lbl);
        break;
    }

    default:
        codegen_expression(cg, stmt);
        break;
    }
}

/* ── Function codegen (user functions) ───────────────────────────── */

void codegen_function(CodeGen *cg, ASTNode *func) {
    for (int i = 0; i < cg->var_count; i++) free(cg->local_vars[i]);
    cg->var_count    = 0;
    cg->stack_offset = 0;
    cg->loop_depth   = 0;
    cg->current_return_type = func->data.function.return_type;
    cg->current_return_typeinfo = func->data.function.return_typeinfo;
    cg->has_sret = (cg->current_return_type == TYPE_TUPLE ||
                    cg->current_return_type == TYPE_STRUCT);
    cg->sret_offset = 0;

    codegen_emit(cg, "; ----- %s -----", func->data.function.name);
    codegen_emit(cg, "global %s", func->data.function.name);
    codegen_emit(cg, "%s:", func->data.function.name);
    codegen_emit(cg, "    push rbp");
    codegen_emit(cg, "    mov  rbp, rsp");

    /* reserve stack space for params + estimate for locals (we'll fix this) */
    int param_space = func->data.function.param_count * 8;
    codegen_emit(cg, "    sub  rsp, %d", param_space + LOCAL_STACK_RESERVE); /* local stack */

    if (cg->has_sret) {
        cg->stack_offset += 8;
        cg->sret_offset = cg->stack_offset;
        codegen_emit(cg, "    mov [rbp - %d], rdi", cg->sret_offset);
    }

    for (int i = 0; i < func->data.function.param_count; i++) {
        ValueType pt = func->data.function.param_types[i];
        TypeInfo *pti = func->data.function.param_typeinfo
                        ? func->data.function.param_typeinfo[i]
                        : NULL;
        if (pti && (pti->kind == TYPE_TUPLE || pti->kind == TYPE_STRUCT))
            pti = typeinfo_ref(pti);
        int reg_idx = i + (cg->has_sret ? 1 : 0);
        if (reg_idx >= 6)
            error("More than 6 arguments not supported");
        codegen_add_var(cg, func->data.function.param_names[i], pt, true, pti);
        codegen_emit(cg, "    mov [rbp - %d], %s",
                     cg->stack_offset, ARG_REGS[reg_idx]);
    }

    for (int i = 0; i < func->data.function.body_count; i++)
        codegen_statement(cg, func->data.function.body[i]);

    /* Check if the last statement was a return — if not, warn and
     * emit implicit return of the last expression value (already in rax)
     * or 0 if the body was empty.
     */
    bool has_explicit_return = false;
    if (func->data.function.body_count > 0) {
        ASTNode *last = func->data.function.body[func->data.function.body_count - 1];
        if (last->type == AST_RETURN) has_explicit_return = true;
    }

    if (!has_explicit_return) {
        fprintf(stderr,
            "Warning: function '%s' has no 'chu' (return) statement.\n"
            "  The last computed value will be returned.\n"
            "  Add 'chu <value>;' to silence this warning.\n",
            func->data.function.name);
        /* rax already holds the last expression value — just epilogue */
    }

    /* implicit epilogue (also serves as safety fallback after explicit returns) */
    codegen_emit(cg, "    mov rsp, rbp");
    codegen_emit(cg, "    pop rbp");
    codegen_emit(cg, "    ret");
    codegen_emit(cg, "");
}

/* ── Module codegen ───────────────────────────────────────────────── */
/*
 * Parse the .vel file for the module, build a table of all its
 * function names, then emit each function under the mangled label
 * "module__funcname".  While doing so, set cg->current_module so
 * that intra-module plain calls are also mangled automatically.
 */
static void codegen_emit_native_externs(CodeGen *cg, const char *module_name,
                                        const char *asm_path);

static void codegen_emit_module(CodeGen *cg, const char *module_name,
                                const char *file_path) {
    char *src = read_file(file_path);
    if (!src) return;

    ErrorContext prev_ctx = error_get_context();
    error_set_context(file_path, src);

    Lexer  lex; lexer_init(&lex, src);
    Token  toks[MAX_TOKENS];
    int    ntok = lexer_tokenize(&lex, toks, MAX_TOKENS);

    Parser par; parser_init(&par, toks, ntok);
    ASTNode *prog = parse_program(&par);

    codegen_register_structs(cg, prog);

    for (int i = 0; i < prog->data.program.import_count; i++) {
        const char *submod = prog->data.program.imports[i]->data.import.module_name;
        if (!cg->module_mgr) continue;
        module_manager_load_module(cg->module_mgr, submod, file_path);
        for (int j = 0; j < cg->module_mgr->import_count; j++) {
            ImportInfo *imp = &cg->module_mgr->imports[j];
            if (strcmp(imp->module_name, submod) != 0) continue;
            const char *dep_path = imp->is_native_asm ? imp->file_path : imp->asm_path;
            if (dep_path[0])
                codegen_emit_native_externs(cg, submod, dep_path);
            break;
        }
    }

    /* ── Step 1: build function table for this module ── */
    mod_func_table_clear();
    for (int i = 0; i < prog->data.program.function_count; i++)
        mod_func_table_add(prog->data.program.functions[i]->data.function.name);

    /* register module function signatures */
    for (int i = 0; i < prog->data.program.function_count; i++) {
        ASTNode *fn = prog->data.program.functions[i];
        char mangled[MAX_TOKEN_LEN * 2 + 4];
        snprintf(mangled, sizeof(mangled), "%s__%s",
                 module_name, fn->data.function.name);
        func_sig_add(cg, mangled,
                     fn->data.function.return_type,
                     fn->data.function.return_typeinfo,
                     fn->data.function.param_types,
                     fn->data.function.param_typeinfo,
                     fn->data.function.param_count);
    }

    /* ── Step 2: set current_module so calls get mangled ── */
    strncpy(cg->current_module, module_name, MAX_TOKEN_LEN - 1);

    codegen_emit(cg, "; ===== module: %s =====", module_name);

    for (int i = 0; i < prog->data.program.function_count; i++) {
        ASTNode *fn = prog->data.program.functions[i];

        /* reset per-function local state */
        for (int j = 0; j < cg->var_count; j++) free(cg->local_vars[j]);
        cg->var_count    = 0;
        cg->stack_offset = 0;
        cg->loop_depth   = 0;
        cg->current_return_type = fn->data.function.return_type;
        cg->current_return_typeinfo = fn->data.function.return_typeinfo;
        cg->has_sret = (cg->current_return_type == TYPE_TUPLE ||
                        cg->current_return_type == TYPE_STRUCT);
        cg->sret_offset = 0;

        /* mangled label */
        char mangled[MAX_TOKEN_LEN * 2 + 4];
        snprintf(mangled, sizeof(mangled), "%s__%s",
                 module_name, fn->data.function.name);

        codegen_emit(cg, "; --- %s ---", mangled);
        codegen_emit(cg, "global %s", mangled);
        codegen_emit(cg, "%s:", mangled);
        codegen_emit(cg, "    push rbp");
        codegen_emit(cg, "    mov  rbp, rsp");
        
        /* allocate stack space */
        int param_space = fn->data.function.param_count * 8;
        codegen_emit(cg, "    sub  rsp, %d", param_space + LOCAL_STACK_RESERVE);

        if (cg->has_sret) {
            cg->stack_offset += 8;
            cg->sret_offset = cg->stack_offset;
            codegen_emit(cg, "    mov [rbp - %d], rdi", cg->sret_offset);
        }

        for (int j = 0; j < fn->data.function.param_count; j++) {
            ValueType pt = fn->data.function.param_types[j];
            TypeInfo *pti = fn->data.function.param_typeinfo
                            ? fn->data.function.param_typeinfo[j]
                            : NULL;
            if (pti && (pti->kind == TYPE_TUPLE || pti->kind == TYPE_STRUCT))
                pti = typeinfo_ref(pti);
            int reg_idx = j + (cg->has_sret ? 1 : 0);
            if (reg_idx >= 6)
                error("More than 6 arguments not supported");
            codegen_add_var(cg, fn->data.function.param_names[j], pt, true, pti);
            codegen_emit(cg, "    mov [rbp - %d], %s",
                         cg->stack_offset, ARG_REGS[reg_idx]);
        }

        /* body — intra-module AST_CALL nodes are mangled via current_module */
        for (int j = 0; j < fn->data.function.body_count; j++)
            codegen_statement(cg, fn->data.function.body[j]);

        /* implicit epilogue — rax holds last expression value */
        codegen_emit(cg, "    mov rsp, rbp");
        codegen_emit(cg, "    pop rbp");
        codegen_emit(cg, "    ret");
        codegen_emit(cg, "");
    }

    /* ── Step 3: clear module context ── */
    cg->current_module[0] = '\0';
    mod_func_table_clear();

    ast_node_free(prog);
    error_restore_context(prev_ctx);
    free(src);
}

/* ── Scan a native .asm file and emit extern for every global symbol ── */
static void codegen_emit_native_externs(CodeGen *cg, const char *module_name,
                                        const char *asm_path) {
    FILE *f = fopen(asm_path, "r");
    if (!f) return;

    codegen_emit(cg, "; externs for native module: %s", module_name);

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
    /* look for lines like: "global io__chhaap" */
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;  /* skip leading whitespace */
        if (strncmp(p, "global", 6) != 0) continue;
        p += 6;
        if (*p != ' ' && *p != '\t') continue;
        while (*p == ' ' || *p == '\t') p++;  /* skip space after 'global' */

        /* trim trailing whitespace/newline */
        char sym[256];
        int j = 0;
        while (*p && *p != '\n' && *p != '\r' && *p != ' ' && *p != '\t' && j < 255)
            sym[j++] = *p++;
        sym[j] = '\0';

        if (j == 0) continue;

        codegen_emit(cg, "extern %s", sym);
    }
    fclose(f);
    codegen_emit(cg, "");
}

/* ── Emit float literals ─────────────────────────────────────────── */
static void codegen_emit_float_literals(CodeGen *cg) {
    if (cg->float_count == 0) return;

    codegen_emit(cg, "");
    codegen_emit(cg, "section .data");

    for (int i = 0; i < cg->float_count; i++) {
        FloatLit *fl = &cg->float_literals[i];
        if (fl->type == TYPE_F32) {
            union { float f; unsigned int u; } v;
            v.f = (float)fl->value;
            fprintf(cg->output, "_VL_f32_%d: dd 0x%08x\n", i, v.u);
        } else {
            union { double d; unsigned long long u; } v;
            v.d = fl->value;
            fprintf(cg->output, "_VL_f64_%d: dq 0x%016llx\n", i,
                    (unsigned long long)v.u);
        }
    }
}

/* ── Emit string literals ─────────────────────────────────────────── */
static void codegen_emit_string_literals(CodeGen *cg) {
    if (cg->string_count == 0) return;

    codegen_emit(cg, "");
    codegen_emit(cg, "section .data");

    for (int i = 0; i < cg->string_count; i++) {
        const char *s = cg->string_literals[i] ? cg->string_literals[i] : "";
        size_t len = strlen(s);

        fprintf(cg->output, "_VL_str_%d:\n    db ", i);
        int col = 0;
        for (size_t j = 0; j < len; j++) {
            unsigned int byte = (unsigned char)s[j];
            if (col >= 16) {
                fprintf(cg->output, "\n    db ");
                col = 0;
            }
            fprintf(cg->output, "%u, ", byte);
            col++;
        }
        fprintf(cg->output, "0\n");
    }
}

static void codegen_emit_array_alloc(CodeGen *cg) {
    codegen_emit(cg, "section .bss");
    codegen_emit(cg, "    _VL_arr_heap resb 65536");
    codegen_emit(cg, "    _VL_arr_hp   resq 1");
    codegen_emit(cg, "");
    codegen_emit(cg, "section .text");
    codegen_emit(cg, "__vl_arr_alloc:");
    codegen_emit(cg, "    push rbp");
    codegen_emit(cg, "    mov  rbp, rsp");
    codegen_emit(cg, "    push rbx");
    codegen_emit(cg, "    mov  rax, [rel _VL_arr_hp]");
    codegen_emit(cg, "    test rax, rax");
    codegen_emit(cg, "    jne  .have");
    codegen_emit(cg, "    lea  rax, [rel _VL_arr_heap]");
    codegen_emit(cg, "    mov  [rel _VL_arr_hp], rax");
    codegen_emit(cg, ".have:");
    codegen_emit(cg, "    mov  rbx, rax");
    codegen_emit(cg, "    mov  [rbx], rdi"); /* store length */
    codegen_emit(cg, "    lea  rdx, [rbx + 8]");
    codegen_emit(cg, "    mov  rcx, rdi");
    codegen_emit(cg, "    imul rcx, rsi");
    codegen_emit(cg, "    add  rcx, 8");
    codegen_emit(cg, "    add  rax, rcx");
    codegen_emit(cg, "    mov  [rel _VL_arr_hp], rax");
    codegen_emit(cg, "    mov  rax, rdx");
    codegen_emit(cg, "    pop  rbx");
    codegen_emit(cg, "    pop  rbp");
    codegen_emit(cg, "    ret");
}

static void codegen_emit_runtime_globals(CodeGen *cg) {
    codegen_emit(cg, "global _VL_argc");
    codegen_emit(cg, "global _VL_argv");
    codegen_emit(cg, "global _VL_envp");
    codegen_emit(cg, "section .bss");
    codegen_emit(cg, "    _VL_argc resq 1");
    codegen_emit(cg, "    _VL_argv resq 1");
    codegen_emit(cg, "    _VL_envp resq 1");
    codegen_emit(cg, "");
}

/* ── Program codegen ──────────────────────────────────────────────── */

void codegen_program(CodeGen *cg, ASTNode *program) {
    codegen_emit(cg, "; Velocity Compiler - Kashmiri Edition v2");
    codegen_emit(cg, "; x86-64 Linux (NASM syntax)");
    codegen_emit(cg, "");
    codegen_emit(cg, "section .text");
    codegen_emit(cg, "");

    /* ── Register struct definitions ── */
    codegen_register_structs(cg, program);

    /* ── Register user function signatures ── */
    for (int i = 0; i < program->data.program.function_count; i++) {
        ASTNode *fn = program->data.program.functions[i];
        func_sig_add(cg, fn->data.function.name,
                     fn->data.function.return_type,
                     fn->data.function.return_typeinfo,
                     fn->data.function.param_types,
                     fn->data.function.param_typeinfo,
                     fn->data.function.param_count);
    }

    /* ── Pass 1: handle all imports ── */
    for (int i = 0; i < program->data.program.import_count; i++) {
        const char *mod = program->data.program.imports[i]->data.import.module_name;
        register_builtin_module_sigs(cg, mod);
        if (!cg->module_mgr) continue;

        char *path = module_manager_find_module(cg->module_mgr, mod);
        if (!path) {
            fprintf(stderr, "Warning: module '%s' not found.\n", mod);
            continue;
        }

        const char *ext = strrchr(path, '.');

        if (ext && strcmp(ext, ".asm") == 0) {
            /*
             * Native-only module (.asm, no .vel):
             * Scan the .asm file for 'global' declarations and emit
             * matching 'extern' lines so NASM knows the symbols are
             * defined in another object file linked in later.
             */
            codegen_emit_native_externs(cg, mod, path);
        } else {
            /*
             * Velocity module (.vel):
             * Also check for a native .asm sidecar and emit externs for it.
             * Then inline the .vel functions under mangled names.
             */
            char *asm_path = module_manager_find_asm(cg->module_mgr, mod);
            if (asm_path) {
                codegen_emit_native_externs(cg, mod, asm_path);
                free(asm_path);
            }
            codegen_emit_module(cg, mod, path);
        }

        free(path);
    }

    /* ── Pass 2: emit user functions ── */
    for (int i = 0; i < program->data.program.function_count; i++)
        codegen_function(cg, program->data.program.functions[i]);

    /* ── Implicit: ensure lafz module is linked if string parsing was used ── */
    if (cg->needs_lafz_parse && cg->module_mgr) {
        register_builtin_module_sigs(cg, "lafz");
        module_manager_load_module(cg->module_mgr, "lafz", NULL);
        char *path = module_manager_find_module(cg->module_mgr, "lafz");
        if (path) {
            const char *ext = strrchr(path, '.');
            if (ext && strcmp(ext, ".asm") == 0) {
                codegen_emit_native_externs(cg, "lafz", path);
            } else {
                char *asm_path = module_manager_find_asm(cg->module_mgr, "lafz");
                if (asm_path) {
                    codegen_emit_native_externs(cg, "lafz", asm_path);
                    free(asm_path);
                }
                codegen_emit_module(cg, "lafz", path);
            }
            free(path);
        } else {
            fprintf(stderr, "Warning: module 'lafz' not found (required for string casts).\n");
        }
    }

    /* ── Entry point ── */
    codegen_emit(cg, "; ----- entry point -----");
    codegen_emit(cg, "global _start");
    codegen_emit(cg, "_start:");
    codegen_emit(cg, "    mov rbx, rsp");
    codegen_emit(cg, "    mov rax, [rbx]");
    codegen_emit(cg, "    mov [rel _VL_argc], rax");
    codegen_emit(cg, "    lea rcx, [rbx + 8]");
    codegen_emit(cg, "    mov [rel _VL_argv], rcx");
    codegen_emit(cg, "    mov rdx, rax");
    codegen_emit(cg, "    add rdx, 1");
    codegen_emit(cg, "    shl rdx, 3");
    codegen_emit(cg, "    add rcx, rdx");
    codegen_emit(cg, "    mov [rel _VL_envp], rcx");
    codegen_emit(cg, "    call main");
    codegen_emit(cg, "    mov  rdi, rax");
    codegen_emit(cg, "    mov  rax, 60");
    codegen_emit(cg, "    syscall");

    codegen_emit_runtime_globals(cg);
    codegen_emit_float_literals(cg);
    codegen_emit_string_literals(cg);
    if (cg->needs_arr_alloc)
        codegen_emit_array_alloc(cg);
}
