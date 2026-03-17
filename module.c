/*
 * Velocity Compiler - Module Manager
 *
 * Search order:
 *   1. Directory of the source file being compiled
 *   2. Current working directory
 *   3. <binary dir>          (where ./velocity lives)
 *   4. <binary dir>/stdlib
 *   5. $VELOCITY_STDLIB      (env var, if set)
 *   6. /usr/local/lib/velocity (system install)
 */

#include "velocity.h"

static char binary_dir[MAX_PATH_LEN] = "";

void module_manager_set_binary_dir(const char *argv0) {
#ifdef __linux__
    char resolved[MAX_PATH_LEN];
    ssize_t len = readlink("/proc/self/exe", resolved, MAX_PATH_LEN - 1);
    if (len > 0) {
        resolved[len] = '\0';
        char *copy = strdup(resolved);
        strncpy(binary_dir, dirname(copy), MAX_PATH_LEN - 1);
        free(copy);
        return;
    }
#endif
    if (argv0) {
        char *copy = strdup(argv0);
        strncpy(binary_dir, dirname(copy), MAX_PATH_LEN - 1);
        free(copy);
    }
}

void module_manager_init(ModuleManager *mgr) {
    mgr->import_count      = 0;
    mgr->search_path_count = 0;
    mgr->search_paths      = malloc(sizeof(char*) * 64);
    mgr->stdlib_path[0]    = '\0';
    /* paths are added dynamically in module_manager_load_module
     * and module_manager_add_source_dir — see below */
}

void module_manager_add_search_path(ModuleManager *mgr, const char *path) {
    if (!path || !path[0] || mgr->search_path_count >= 63) return;
    /* no duplicates */
    for (int i = 0; i < mgr->search_path_count; i++)
        if (strcmp(mgr->search_paths[i], path) == 0) return;
    mgr->search_paths[mgr->search_path_count++] = strdup(path);
}

/* Build the full search list. Called lazily before each lookup. */
static void build_search_paths(ModuleManager *mgr, const char *source_dir) {
    /* 1. source file dir (highest priority — where your .vel lives) */
    if (source_dir) module_manager_add_search_path(mgr, source_dir);

    /* 2. cwd */
    char cwd[MAX_PATH_LEN];
    if (getcwd(cwd, MAX_PATH_LEN)) {
        module_manager_add_search_path(mgr, cwd);

        /* 2b. cwd/stdlib */
        char cwdstd[MAX_PATH_LEN];
        snprintf(cwdstd, MAX_PATH_LEN, "%s/stdlib", cwd);
        module_manager_add_search_path(mgr, cwdstd);
    }

    /* 3. binary dir + binary/stdlib */
    if (binary_dir[0]) {
        module_manager_add_search_path(mgr, binary_dir);
        char binstd[MAX_PATH_LEN];
        snprintf(binstd, MAX_PATH_LEN, "%s/stdlib", binary_dir);
        module_manager_add_search_path(mgr, binstd);
    }

    /* 4. $VELOCITY_STDLIB */
    char *env = getenv("VELOCITY_STDLIB");
    if (env) module_manager_add_search_path(mgr, env);

    /* 5. system install */
    module_manager_add_search_path(mgr, "/usr/local/lib/velocity");
}

char* module_manager_find_module(ModuleManager *mgr, const char *name) {
    char path[MAX_PATH_LEN];
    struct stat st;
    for (int i = 0; i < mgr->search_path_count; i++) {
        snprintf(path, MAX_PATH_LEN, "%s/%s.vel", mgr->search_paths[i], name);
        if (stat(path, &st) == 0) return strdup(path);

        snprintf(path, MAX_PATH_LEN, "%s/%s.asm", mgr->search_paths[i], name);
        if (stat(path, &st) == 0) return strdup(path);
    }
    return NULL;
}

char* module_manager_find_asm(ModuleManager *mgr, const char *name) {
    char path[MAX_PATH_LEN];
    struct stat st;
    for (int i = 0; i < mgr->search_path_count; i++) {
        snprintf(path, MAX_PATH_LEN, "%s/%s.asm", mgr->search_paths[i], name);
        if (stat(path, &st) == 0) return strdup(path);
    }
    return NULL;
}

bool module_manager_load_module(ModuleManager *mgr,
                                const char    *module_name,
                                const char    *current_file) {
    /* already loaded? */
    for (int i = 0; i < mgr->import_count; i++)
        if (strcmp(mgr->imports[i].module_name, module_name) == 0)
            return true;

    /* build/extend search paths using the source file's directory */
    char *src_dir = NULL;
    if (current_file) {
        char *tmp = strdup(current_file);
        char *d   = dirname(tmp);
        src_dir   = strdup(d);
        free(tmp);
    }
    build_search_paths(mgr, src_dir);
    if (src_dir) free(src_dir);

    /* find primary file */
    char *primary = module_manager_find_module(mgr, module_name);
    if (!primary) {
        fprintf(stderr, "Error: Module not found: '%s'\n", module_name);
        fprintf(stderr, "  Searched:\n");
        for (int i = 0; i < mgr->search_path_count; i++)
            fprintf(stderr, "    %s\n", mgr->search_paths[i]);
        fprintf(stderr,
            "\n  Make sure %s.vel or %s.asm is in the same folder as your "
            "source file,\n  or in a 'stdlib' subfolder next to the velocity "
            "binary.\n", module_name, module_name);
        exit(1);
    }

    char *asm_path = module_manager_find_asm(mgr, module_name);

    ImportInfo *imp = &mgr->imports[mgr->import_count++];
    strncpy(imp->module_name, module_name, MAX_TOKEN_LEN - 1);
    strncpy(imp->file_path,   primary,     MAX_PATH_LEN  - 1);
    imp->link_flags[0] = '\0';

    const char *ext    = strrchr(primary, '.');
    imp->is_native_asm = (ext && strcmp(ext, ".asm") == 0);
    imp->is_standard_lib = false;

    if (asm_path) {
        strncpy(imp->asm_path, asm_path, MAX_PATH_LEN - 1);
        free(asm_path);
    } else {
        imp->asm_path[0] = '\0';
    }

    if (strcmp(module_name, "sdl_native") == 0) {
        strncpy(imp->link_flags, "-L/usr/lib/x86_64-linux-gnu -lSDL2", sizeof(imp->link_flags) - 1);
    }

    free(primary);
    return true;
}

void module_manager_free(ModuleManager *mgr) {
    for (int i = 0; i < mgr->search_path_count; i++)
        free(mgr->search_paths[i]);
    free(mgr->search_paths);
}

char* get_directory(const char *filepath) {
    if (!filepath) return NULL;
    char *copy   = strdup(filepath);
    char *d      = dirname(copy);
    char *result = strdup(d);
    free(copy);
    return result;
}
