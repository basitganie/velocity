/*
 * Velocity Compiler V2 - Main Driver
 */

#include "velocity.h"
#include <sys/stat.h>
#include <unistd.h>

#ifdef _WIN32
#  define HOST_WINDOWS 1
#else
#  define HOST_WINDOWS 0
#endif

/* Auto-detect AArch64 host so velc defaults to the right target */
#if defined(__aarch64__) || defined(_M_ARM64)
#  define HOST_AARCH64 1
#else
#  define HOST_AARCH64 0
#endif

#define VERSION_STRING "velocity " VL_VERSION " (Kashmiri Edition v2)"

static void print_help(const char *prog) {
    printf(
        "%s\n\n"
        "Usage:\n"
        "  %s <source.vel> [options]\n\n"
        "Options:\n"
        "  -o, --output <file>    Output binary name\n"
        "  -v, --verbose          Verbose (keep .asm/.o, print commands)\n"
        "  -V, --version          Print version and exit\n"
        "  -h, --help             Print this help and exit\n"
        "  --windows              Target Windows PE64 (MS x64 ABI)\n"
        "  --linux                Target Linux ELF64 (System V AMD64)\n"
        "  --target <triple>      Cross-compile target. Supported:\n"
        "                           aarch64-linux-android  (Android AArch64)\n"
        "  --stdlib <path>        Extra stdlib search path\n"
        "  --emit-asm             Keep generated .asm/.s file\n"
        "  --no-link              Stop after assemble (.o only)\n\n"
        "  AArch64-Android env vars:\n"
        "    VELOCITY_NDK_AS      Path to aarch64-linux-android-as\n"
        "    VELOCITY_NDK_CLANG   Path to aarch64-linux-android21-clang\n\n"

        "Author: Basit Ahmad Ganie <basitahmed1412@gmail.com>\n",
        VERSION_STRING, prog, prog
    );
}

static char* make_path(const char *base, const char *ext) {
    size_t n = strlen(base) + strlen(ext) + 2;
    char *p = (char*)malloc(n);
    snprintf(p, n, "%s%s", base, ext);
    return p;
}

static char* stem(const char *path) {
    const char *base = path;
    for (const char *p = path; *p; p++)
        if (*p=='/' || *p=='\\') base = p + 1;
    char *result = strdup(base);
    char *dot = strrchr(result, '.');
    if (dot) *dot = '\0';
    return result;
}

static int run_cmd(const char *cmd, bool verbose) {
    if (verbose) { printf("  [cmd] %s\n", cmd); fflush(stdout); }
    return system(cmd);
}

/* Run cmd and capture stderr into buf (at most bufsz-1 chars). Returns exit code. */
static int run_cmd_capture(const char *cmd, char *errbuf, int bufsz) {
    char full[8192];
    char tmp[256] = "/tmp/vl_err_XXXXXX";
    int fd = mkstemp(tmp);
    if (fd < 0) return system(cmd);
    close(fd);
    snprintf(full, sizeof(full), "%s 2>\"%s\"", cmd, tmp);
    int rc = system(full);
    if (errbuf && bufsz > 0) {
        FILE *ef = fopen(tmp, "r");
        if (ef) {
            int n = fread(errbuf, 1, bufsz-1, ef);
            errbuf[n > 0 ? n : 0] = '\0';
            /* strip trailing newline */
            int l = strlen(errbuf);
            while (l > 0 && (errbuf[l-1]=='\n'||errbuf[l-1]=='\r')) errbuf[--l]='\0';
            fclose(ef);
        } else errbuf[0] = '\0';
    }
    remove(tmp);
    return rc;
}

/* Check that the output path is not an existing directory */
static void check_output_not_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        /* Get just the final component for the /tmp suggestion */
        const char *base = strrchr(path, '/');
        base = base ? base + 1 : path;
        fprintf(stderr,
            "\n\033[1;31merror\033[0m: output path '%s' is a directory.\n"
            "  Use a different name, e.g.:  -o %s_out\n"
            "  Or write to /tmp:            -o /tmp/%s\n",
            path, path, base);
        exit(1);
    }
}

static void compile(CompilerOptions *opts) {
    bool verbose    = opts->verbose;
    bool target_win = opts->target_windows;
    bool target_a64 = opts->target_aarch64;
    const char *src = opts->input_file;
    const char *out = opts->output_file;

    if (verbose) printf("[velocity] compiling %s\n", src);

    char *source = read_file(src);
    error_set_context(src, source);

    ModuleManager mgr;
    module_manager_init(&mgr);
    if (opts->stdlib_path)
        strncpy(mgr.stdlib_path, opts->stdlib_path, MAX_PATH_LEN-1);

    char *src_dir = get_directory(src);
    if (src_dir) { module_manager_add_search_path(&mgr, src_dir); free(src_dir); }

    Lexer lexer; lexer_init(&lexer, source);
    Token *tokens = (Token*)malloc(sizeof(Token) * MAX_TOKENS);
    int ntokens = lexer_tokenize(&lexer, tokens, MAX_TOKENS);

    Parser parser; parser_init(&parser, tokens, ntokens);
    ASTNode *ast = parse_program(&parser);

    for (int i = 0; i < ast->data.program.import_count; i++) {
        const char *mod = ast->data.program.imports[i]->data.import.module_name;
        module_manager_load_module(&mgr, mod, src);
    }

    char *asm_file = make_path(out, ".asm");
    FILE *asm_fp = fopen(asm_file, "w");
    if (!asm_fp) error("Cannot create: %s", asm_file);

    CodeGen cg;
    codegen_init(&cg, asm_fp, &mgr, target_win, opts->target_aarch64,
                 opts->module_compile_name);

    /* Windows: rename user main -> main_vel to avoid collision with CRT main */
    if (target_win) {
        for (int i = 0; i < ast->data.program.function_count; i++) {
            ASTNode *fn = ast->data.program.functions[i];
            if (strcmp(fn->data.function.name, "main") == 0) {
                strncpy(fn->data.function.name, "main_vel", MAX_TOKEN_LEN-1);
                break;
            }
        }
    }

    codegen_program(&cg, ast);
    fclose(asm_fp);

    if (verbose) printf("[velocity] assembly written: %s\n", asm_file);

    char obj_file[MAX_PATH_LEN];
    snprintf(obj_file, MAX_PATH_LEN, "%s.o", out);

    char cmd[4096];

    if (target_a64) {
        /* AArch64-Android: use GNU as via NDK clang or cross-as */
        const char *as_path = getenv("VELOCITY_NDK_AS");
        if (!as_path) as_path = "aarch64-linux-android-as";

        /* The generated file is .s (GNU as syntax), not .asm */
        char *s_file = make_path(out, ".s");
        /* Rename: codegen wrote to asm_file (out+".asm"), copy/rename to .s */
        rename(asm_file, s_file);

        snprintf(cmd, sizeof(cmd),
                 "%s \"%s\" -o \"%s\"", as_path, s_file, obj_file);
        if (verbose) printf("[velocity] assemble (aarch64): %s\n", cmd);
        char aserr[1024] = "";
        if (run_cmd_capture(cmd, aserr, sizeof(aserr)) != 0) {
            /* Try clang as a fallback */
            const char *clang = getenv("VELOCITY_NDK_CLANG");
            if (!clang) clang = "aarch64-linux-android21-clang";
            bool is_ndk = (strstr(clang,"clang") != NULL);
            if (is_ndk)
                snprintf(cmd, sizeof(cmd),
                         "%s --target=aarch64-linux-android21 -c \"%s\" -o \"%s\"",
                         clang, s_file, obj_file);
            else
                snprintf(cmd, sizeof(cmd),
                         "%s -c \"%s\" -o \"%s\"",
                         clang, s_file, obj_file);
            char aserr2[1024] = "";
            if (run_cmd_capture(cmd, aserr2, sizeof(aserr2)) != 0) {
                fprintf(stderr, "\n\033[1;31merror\033[0m: AArch64 assembly failed.\n");
                if (aserr[0])  fprintf(stderr, "  %s\n", aserr);
                if (aserr2[0]) fprintf(stderr, "  %s\n", aserr2);
                if (!getenv("VELOCITY_NDK_AS"))
                    fprintf(stderr,
                        "  Tip: ensure 'as' is installed (apt install binutils)\n");
                exit(1);
            }
        }

        /* Assemble stdlib modules */
        char extra_objs[8192] = "";
        char *tmp_objs[MAX_IMPORTS];
        int   tmp_count = 0;
        for (int i = 0; i < mgr.import_count; i++) {
            ImportInfo *imp = &mgr.imports[i];
            const char *asm_src = NULL;
            /* Look for _aarch64.s sidecar first, then fall back to .asm */
            char a64_asm[MAX_PATH_LEN];
            snprintf(a64_asm, sizeof(a64_asm), "%s", imp->file_path);
            /* Replace .asm → _aarch64.s if present */
            char *dot = strrchr(a64_asm, '.');
            if (dot) strcpy(dot, "_aarch64.s");
            if (file_exists(a64_asm)) asm_src = a64_asm;
            else if (imp->asm_path[0] && file_exists(imp->asm_path))
                asm_src = imp->asm_path;

            /* If no .asm found, check if this is a .vel module — compile it with AArch64 backend */
            if (!asm_src) {
                /* Check for .vel source */
                char vel_src[MAX_PATH_LEN];
                strncpy(vel_src, imp->file_path, MAX_PATH_LEN-1);
                dot = strrchr(vel_src, '.');
                /* imp->file_path might already be .vel or might have no extension */
                bool is_vel_src = (dot && strcmp(dot, ".vel")==0);
                if (!is_vel_src) {
                    /* Try appending .vel */
                    snprintf(vel_src, MAX_PATH_LEN, "%s.vel", imp->file_path);
                    is_vel_src = file_exists(vel_src);
                }
                if (is_vel_src && file_exists(vel_src)) {
                    /* Compile the .vel module with AArch64 target */
                    char mod_obj[MAX_PATH_LEN];
                    snprintf(mod_obj, MAX_PATH_LEN, "%s__%s.o", out, imp->module_name);
                    /* velc --no-link writes to out.o, so pass base name without .o */
                    char mod_base[MAX_PATH_LEN];
                    snprintf(mod_base, MAX_PATH_LEN, "%s__%s", out, imp->module_name);
                    /* find velc: check ./velc first, then system velc */
                    const char *velc_cmd = access("./velc", X_OK) == 0 ? "./velc" : "velc";
                    const char *stdlib_arg = opts->stdlib_path ? opts->stdlib_path : "stdlib";
                    snprintf(cmd, sizeof(cmd),
                             "%s \"%s\" -o \"%s\" --stdlib \"%s\""
                             " --target aarch64-linux-android --no-link"
                             " --module \"%s\"",
                             velc_cmd, vel_src, mod_base, stdlib_arg,
                             imp->module_name);
                    if (verbose) printf("[velocity] compile vel module: %s\n", vel_src);
                    if (run_cmd(cmd, false) == 0) {
                        strncat(extra_objs, " \"", sizeof(extra_objs)-strlen(extra_objs)-1);
                        strncat(extra_objs, mod_obj, sizeof(extra_objs)-strlen(extra_objs)-1);
                        strncat(extra_objs, "\"", sizeof(extra_objs)-strlen(extra_objs)-1);
                        tmp_objs[tmp_count++] = strdup(mod_obj);
                    }
                    continue;
                }
            }
            if (!asm_src) continue;

            char sidecar_obj[MAX_PATH_LEN];
            snprintf(sidecar_obj, MAX_PATH_LEN, "%s__%s.o", out, imp->module_name);
            const char *as_p = getenv("VELOCITY_NDK_AS");
            if (!as_p) as_p = "aarch64-linux-android-as";
            snprintf(cmd, sizeof(cmd), "%s \"%s\" -o \"%s\"",
                     as_p, asm_src, sidecar_obj);
            if (verbose) printf("[velocity] assemble module: %s\n", cmd);
            if (run_cmd(cmd, false) == 0) {
                strncat(extra_objs, " \"", sizeof(extra_objs)-strlen(extra_objs)-1);
                strncat(extra_objs, sidecar_obj, sizeof(extra_objs)-strlen(extra_objs)-1);
                strncat(extra_objs, "\"", sizeof(extra_objs)-strlen(extra_objs)-1);
                tmp_objs[tmp_count++] = strdup(sidecar_obj);
            }
        }

        if (!opts->no_link) {
            const char *ld = getenv("VELOCITY_NDK_CLANG");
            if (!ld) ld = "aarch64-linux-android21-clang";
            bool is_ndk_ld = (strstr(ld,"clang") != NULL);
            if (is_ndk_ld)
                snprintf(cmd, sizeof(cmd),
                         "%s --target=aarch64-linux-android21 -static"
                         " \"%s\"%s -o \"%s\" -lc",
                         ld, obj_file, extra_objs, out);
            else {
                /* GNU cross-gcc: use bare ld directly — no libc needed
                   (io stdlib uses raw Linux syscalls only) */
                const char *bare_ld = "aarch64-linux-gnu-ld";
                const char *ld_env = getenv("VELOCITY_NDK_LD");
                if (ld_env) bare_ld = ld_env;
                snprintf(cmd, sizeof(cmd),
                         "%s -static \"%s\"%s -o \"%s\"",
                         bare_ld, obj_file, extra_objs, out);
            }
            if (verbose) printf("[velocity] link (aarch64): %s\n", cmd);
            char lderr[1024] = "";
            if (run_cmd_capture(cmd, lderr, sizeof(lderr)) != 0) {
                fprintf(stderr, "\n\033[1;31merror\033[0m: AArch64 linking failed.\n");
                if (lderr[0]) fprintf(stderr, "  %s\n", lderr);
                /* Only show NDK hint if env vars aren't set */
                if (!getenv("VELOCITY_NDK_CLANG"))
                    fprintf(stderr,
                        "  Tip: set VELOCITY_NDK_CLANG to your linker, e.g.:\n"
                        "    export VELOCITY_NDK_CLANG=gcc\n"
                        "  Or use the Android NDK clang:\n"
                        "    export VELOCITY_NDK_CLANG="
                        "$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/"
                        "aarch64-linux-android21-clang\n");
                exit(1);
            }
        }

        if (!verbose && !opts->keep_asm) {
            if (!opts->no_link) remove(obj_file);
            remove(s_file);
            for (int i = 0; i < tmp_count; i++) { remove(tmp_objs[i]); free(tmp_objs[i]); }
        }
        free(s_file);
        printf("[velocity] [OK]  %s\n", opts->no_link ? obj_file : out);
        ast_node_free(ast);
        module_manager_free(&mgr);
        free(tokens);
        free(source);
        free(asm_file);
        return;
    }

    /* x86-64: NASM path (existing) */
    const char *nasm_fmt = target_win ? "win64" : "elf64";
    snprintf(cmd, sizeof(cmd), "nasm -f %s \"%s\" -o \"%s\"",
             nasm_fmt, asm_file, obj_file);
    if (verbose) printf("[velocity] assemble: %s\n", cmd);
    if (run_cmd(cmd, false) != 0) {
        fprintf(stderr,
            "\n\033[1;31merror\033[0m: NASM failed.\n"
            "  Install: sudo apt install nasm  OR  choco install nasm\n");
        exit(1);
    }

    char extra_objs[8192]  = "";
    char extra_flags[2048] = "";
    char *tmp_objs[MAX_IMPORTS];
    int   tmp_count = 0;

    for (int i = 0; i < mgr.import_count; i++) {
        ImportInfo *imp = &mgr.imports[i];
        const char *asm_src = NULL;
        if (imp->asm_path[0])        asm_src = imp->asm_path;
        else if (imp->is_native_asm) asm_src = imp->file_path;
        if (!asm_src) {
            if (imp->link_flags[0]) {
                strncat(extra_flags, " ", sizeof(extra_flags)-strlen(extra_flags)-1);
                strncat(extra_flags, imp->link_flags, sizeof(extra_flags)-strlen(extra_flags)-1);
            }
            continue;
        }
        char sidecar_obj[MAX_PATH_LEN];
        snprintf(sidecar_obj, MAX_PATH_LEN, "%s__%s.o", out, imp->module_name);
        snprintf(cmd, sizeof(cmd), "nasm -f %s \"%s\" -o \"%s\"",
                 nasm_fmt, asm_src, sidecar_obj);
        if (verbose) printf("[velocity] assemble module: %s\n", cmd);
        if (run_cmd(cmd, false) == 0) {
            strncat(extra_objs, " \"", sizeof(extra_objs)-strlen(extra_objs)-1);
            strncat(extra_objs, sidecar_obj, sizeof(extra_objs)-strlen(extra_objs)-1);
            strncat(extra_objs, "\"", sizeof(extra_objs)-strlen(extra_objs)-1);
            tmp_objs[tmp_count++] = strdup(sidecar_obj);
        }
        if (imp->link_flags[0]) {
            strncat(extra_flags, " ", sizeof(extra_flags)-strlen(extra_flags)-1);
            strncat(extra_flags, imp->link_flags, sizeof(extra_flags)-strlen(extra_flags)-1);
        }
    }

    if (!opts->no_link) {
        if (target_win) {
            snprintf(cmd, sizeof(cmd),
                     "gcc -o \"%s\" \"%s\"%s%s -lmsvcrt -lkernel32",
                     out, obj_file, extra_objs, extra_flags);
        } else {
            snprintf(cmd, sizeof(cmd),
                     "ld -dynamic-linker /lib64/ld-linux-x86-64.so.2"
                     " \"%s\"%s%s -o \"%s\"",
                     obj_file, extra_objs, extra_flags, out);
        }
        if (verbose) printf("[velocity] link: %s\n", cmd);
        if (run_cmd(cmd, false) != 0) {
            fprintf(stderr,
                "\n\033[1;31merror\033[0m: Linking failed.\n"
                "  Windows target: install MinGW-w64, ensure gcc is in PATH.\n"
                "  Linux target:   ensure ld (binutils) is installed.\n");
            exit(1);
        }
    }

    if (!verbose && !opts->keep_asm) {
        if (!opts->no_link) remove(obj_file);
        remove(asm_file);
        for (int i = 0; i < tmp_count; i++) { remove(tmp_objs[i]); free(tmp_objs[i]); }
    }

    printf("[velocity] [OK]  %s\n",
           opts->no_link ? obj_file : out);

    ast_node_free(ast);
    module_manager_free(&mgr);
    free(tokens);
    free(source);
    free(asm_file);
}

int main(int argc, char *argv[]) {
    module_manager_set_binary_dir(argv[0]);

    if (argc < 2) {
        fprintf(stderr, "%s\nRun '%s --help' for usage.\n",
                VERSION_STRING, argv[0]);
        return 1;
    }

    CompilerOptions opts;
    memset(&opts, 0, sizeof(opts));
    opts.target_windows = (bool)HOST_WINDOWS;
    opts.target_aarch64 = (bool)HOST_AARCH64;
    /* On AArch64, default assembler/linker to system tools if not set */
#if HOST_AARCH64
    if (!getenv("VELOCITY_NDK_AS"))    setenv("VELOCITY_NDK_AS",    "as",  0);
    if (!getenv("VELOCITY_NDK_CLANG")) setenv("VELOCITY_NDK_CLANG", "gcc", 0);
    if (!getenv("VELOCITY_NDK_LD"))    setenv("VELOCITY_NDK_LD",    "ld",  0);
#endif

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (!strcmp(arg,"-h")||!strcmp(arg,"--help"))    { print_help(argv[0]); return 0; }
        if (!strcmp(arg,"-V")||!strcmp(arg,"--version")) { printf("%s\n",VERSION_STRING); return 0; }
        if (!strcmp(arg,"-v")||!strcmp(arg,"--verbose")) { opts.verbose=true; continue; }
        if (!strcmp(arg,"--windows"))  { opts.target_windows=true; opts.target_aarch64=false; continue; }
        if (!strcmp(arg,"--linux"))    { opts.target_windows=false; opts.target_aarch64=false; continue; }
        if (!strcmp(arg,"--target") && i+1<argc) {
            const char *triple = argv[++i];
            if (strstr(triple,"aarch64") && strstr(triple,"android")) {
                opts.target_aarch64=true; opts.target_windows=false;
            } else if (strstr(triple,"aarch64")) {
                opts.target_aarch64=true; opts.target_windows=false;
            } else if (strstr(triple,"windows")||strstr(triple,"win64")) {
                opts.target_windows=true; opts.target_aarch64=false;
            } else {
                fprintf(stderr,"error: unknown target '%s'\n", triple);
                return 1;
            }
            continue;
        }
        if (!strcmp(arg,"--emit-asm")) { opts.keep_asm=true;  continue; }
        if (!strcmp(arg,"--no-link"))  { opts.no_link=true;   continue; }
        if (!strcmp(arg,"--module") && i+1<argc) {
            opts.module_compile_name = argv[++i]; continue;
        }
        if ((!strcmp(arg,"-o")||!strcmp(arg,"--output")) && i+1<argc) {
            opts.output_file = argv[++i]; continue;
        }
        if (!strcmp(arg,"--stdlib") && i+1<argc) {
            opts.stdlib_path = argv[++i]; continue;
        }
        if (arg[0]=='-') {
            fprintf(stderr,"error: unknown option '%s'\n"
                           "Run '%s --help' for usage.\n", arg, argv[0]);
            return 1;
        }
        if (!opts.input_file) { opts.input_file = arg; continue; }
        fprintf(stderr,"error: unexpected argument '%s'\n", arg);
        return 1;
    }

    if (!opts.input_file) {
        fprintf(stderr,"error: no input file.\nRun '%s --help' for usage.\n",argv[0]);
        return 1;
    }

    char *default_out = NULL;
    if (!opts.output_file) {
        char *s = stem(opts.input_file);
        default_out = opts.target_windows ? make_path(s,".exe") : strdup(s);
        free(s);
        opts.output_file = default_out;
    }

    check_output_not_dir(opts.output_file);
    compile(&opts);
    if (default_out) free(default_out);
    return 0;
}
