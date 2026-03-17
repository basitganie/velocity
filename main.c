/*
 * Velocity Compiler - Main Driver (Kashmiri Edition v2)
 */

#include "velocity.h"

static void print_usage(const char *prog) {
    printf("Velocity Compiler - 0.1.1\n\n");
    printf("Usage: %s <source.vel> [-o output] [-v]\n\n", prog);
   // printf("Keywords: kar chu ath agar nate yeli anaw adad\n");
   // printf("Example:  ./velocity hello.vel -o hello && ./hello\n\n");
}

static void compile(const char *source_file,
                    const char *output_file,
                    bool        verbose) {

    printf("[velocity] compiling %s\n", source_file);

    /* ── 1. Read source ── */
    char *source = read_file(source_file);

    /* ── 2. Error context (for diagnostics) ── */
    error_set_context(source_file, source);

    /* ── 3. Module manager ── */
    ModuleManager mgr;
    module_manager_init(&mgr);

    /* add the directory where the source file lives */
    char *src_dir = get_directory(source_file);
    if (src_dir) {
        module_manager_add_search_path(&mgr, src_dir);
        free(src_dir);
    }

    /* ── 4. Lex ── */
    Lexer lexer; lexer_init(&lexer, source);
    Token tokens[MAX_TOKENS];
    int   ntokens = lexer_tokenize(&lexer, tokens, MAX_TOKENS);

    /* ── 5. Parse ── */
    Parser parser; parser_init(&parser, tokens, ntokens);
    ASTNode *ast = parse_program(&parser);

    /* ── 6. Resolve imports ── */
    for (int i = 0; i < ast->data.program.import_count; i++) {
        const char *mod = ast->data.program.imports[i]->data.import.module_name;
        module_manager_load_module(&mgr, mod, source_file);
    }

    /* print what was found */
    for (int i = 0; i < mgr.import_count; i++) {
        ImportInfo *imp = &mgr.imports[i];
        printf("[velocity] module '%s': %s\n", imp->module_name, imp->file_path);
        if (imp->asm_path[0])
            printf("[velocity]   asm: %s\n", imp->asm_path);
    }

    /* ── 7. Code generation ── */
    char asm_file[MAX_PATH_LEN];
    snprintf(asm_file, MAX_PATH_LEN, "%s.asm", output_file);

    FILE *asm_fp = fopen(asm_file, "w");
    if (!asm_fp) error("Cannot create: %s", asm_file);

    CodeGen cg; codegen_init(&cg, asm_fp, &mgr);
    codegen_program(&cg, ast);

    for (int i = 0; i < cg.string_count; i++) {
        free(cg.string_literals[i]);
    }
    free(cg.string_literals);
    free(cg.float_literals);
    for (int i = 0; i < cg.func_sig_count; i++) {
        free(cg.func_sigs[i].param_types);
        if (cg.func_sigs[i].param_typeinfo) {
            for (int j = 0; j < cg.func_sigs[i].param_count; j++) {
                if (cg.func_sigs[i].param_typeinfo[j]) {
                    if (cg.func_sigs[i].param_typeinfo[j]->kind == TYPE_TUPLE)
                        free(cg.func_sigs[i].param_typeinfo[j]->tuple_types);
                    free(cg.func_sigs[i].param_typeinfo[j]);
                }
            }
            free(cg.func_sigs[i].param_typeinfo);
        }
        if (cg.func_sigs[i].return_typeinfo) {
            if (cg.func_sigs[i].return_typeinfo->kind == TYPE_TUPLE)
                free(cg.func_sigs[i].return_typeinfo->tuple_types);
            free(cg.func_sigs[i].return_typeinfo);
        }
    }
    free(cg.func_sigs);
    free(cg.local_vars);
    free(cg.var_offsets);
    free(cg.var_types);
    free(cg.var_mutable);
    free(cg.var_sizes);
    free(cg.var_typeinfo);
    fclose(asm_fp);

    /* ── 8. Assemble main .asm ── */
    char obj_file[MAX_PATH_LEN];
    snprintf(obj_file, MAX_PATH_LEN, "%s.o", output_file);

    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "nasm -f elf64 \"%s\" -o \"%s\"", asm_file, obj_file);
    printf("[velocity] assemble: %s\n", cmd);
    if (system(cmd) != 0)
        error("NASM failed. Install: sudo apt install nasm");

    /* ── 9. Assemble each native .asm module ── */
    char extra_objs[4096] = "";
    char extra_link_flags[4096] = "";
    char tmp_objs[MAX_IMPORTS][MAX_PATH_LEN];
    int  tmp_obj_count = 0;

    for (int i = 0; i < mgr.import_count; i++) {
        ImportInfo *imp = &mgr.imports[i];

        /*
         * Determine the .asm file to assemble:
         *   - if imp->asm_path is set, use it  (sidecar or native-only)
         *   - else if is_native_asm, use file_path directly (it IS the .asm)
         *   - else no native code for this module
         */
        const char *asm_src = NULL;
        if (imp->asm_path[0] != '\0') {
            asm_src = imp->asm_path;
        } else if (imp->is_native_asm) {
            asm_src = imp->file_path;
        }

        if (!asm_src) {
            if (imp->link_flags[0]) {
                strncat(extra_link_flags, " ", sizeof(extra_link_flags) - strlen(extra_link_flags) - 1);
                strncat(extra_link_flags, imp->link_flags, sizeof(extra_link_flags) - strlen(extra_link_flags) - 1);
            }
            continue;
        }

        char sidecar_obj[MAX_PATH_LEN];
        snprintf(sidecar_obj, MAX_PATH_LEN, "%s__%s.o",
                 output_file, imp->module_name);

        snprintf(cmd, sizeof(cmd),
                 "nasm -f elf64 \"%s\" -o \"%s\"", asm_src, sidecar_obj);
        printf("[velocity] assemble module: %s\n", cmd);

        if (system(cmd) == 0) {
            strncat(extra_objs, " \"",       sizeof(extra_objs) - strlen(extra_objs) - 1);
            strncat(extra_objs, sidecar_obj, sizeof(extra_objs) - strlen(extra_objs) - 1);
            strncat(extra_objs, "\"",        sizeof(extra_objs) - strlen(extra_objs) - 1);
            strncpy(tmp_objs[tmp_obj_count++], sidecar_obj, MAX_PATH_LEN - 1);
            if (imp->link_flags[0]) {
                strncat(extra_link_flags, " ", sizeof(extra_link_flags) - strlen(extra_link_flags) - 1);
                strncat(extra_link_flags, imp->link_flags, sizeof(extra_link_flags) - strlen(extra_link_flags) - 1);
            }
        } else {
            error("Failed to assemble native module: %s", asm_src);
        }
    }

    /* ── 10. Link ── */
    snprintf(cmd, sizeof(cmd),
             "ld -dynamic-linker /lib64/ld-linux-x86-64.so.2 \"%s\"%s%s -o \"%s\"",
             obj_file, extra_objs, extra_link_flags, output_file);
    printf("[velocity] link: %s\n", cmd);
    if (system(cmd) != 0)
        error("Linking failed");

    /* ── 11. Cleanup ── */
    if (!verbose) {
        remove(asm_file);
        remove(obj_file);
        for (int i = 0; i < tmp_obj_count; i++)
            remove(tmp_objs[i]);
    }

    ast_node_free(ast);
    module_manager_free(&mgr);
    free(source);

    printf("[velocity] done: %s\n", output_file);
}

int main(int argc, char **argv) {
    module_manager_set_binary_dir(argv[0]);

    if (argc < 2) { print_usage(argv[0]); return 1; }

    const char *source_file = NULL;
    const char *output_file = "a.out";
    bool        verbose     = false;

    for (int i = 1; i < argc; i++) {
        if      (strcmp(argv[i], "-o") == 0 && i+1 < argc) output_file = argv[++i];
        else if (strcmp(argv[i], "-v") == 0)                verbose = true;
        else if (strcmp(argv[i], "-h") == 0)                { print_usage(argv[0]); return 0; }
        else if (argv[i][0] != '-')                         source_file = argv[i];
        else { fprintf(stderr, "Unknown option: %s\n", argv[i]); return 1; }
    }

    if (!source_file) { fprintf(stderr, "No source file.\n"); return 1; }
    compile(source_file, output_file, verbose);
    return 0;
}
