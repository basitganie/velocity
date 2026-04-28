/*
 * Velocity Compiler - Module Manager
 * Kashmiri Edition v2  - Cross-platform (Windows + Linux)
 *
 * Search order:
 *   1. Directory of the source file being compiled
 *   2. Current working directory
 *   3. <binary dir>
 *   4. <binary dir>/stdlib
 *   5. --stdlib / VELOCITY_STDLIB env var
 *   6. /usr/local/lib/velocity  (Linux only)
 *   7. C:/velocity/stdlib       (Windows only)
 */

#include "velocity.h"

#ifdef _WIN32
# include <windows.h>
#else
# include <sys/stat.h>
# include <libgen.h>
#endif

static char binary_dir[MAX_PATH_LEN] = "";

void module_manager_set_binary_dir(const char *argv0) {
#ifdef __linux__
    char resolved[MAX_PATH_LEN];
    ssize_t len = readlink("/proc/self/exe", resolved, MAX_PATH_LEN - 1);
    if (len > 0) {
        resolved[len] = '\0';
        char *copy = strdup(resolved);
        char *d = dirname(copy);
        strncpy(binary_dir, d, MAX_PATH_LEN - 1);
        free(copy);
        return;
    }
#endif
#ifdef _WIN32
    char path[MAX_PATH_LEN];
    DWORD sz = GetModuleFileNameA(NULL, path, MAX_PATH_LEN);
    if (sz > 0) {
        char *last = strrchr(path, '\\');
        if (!last) last = strrchr(path, '/');
        if (last) *last = '\0';
        strncpy(binary_dir, path, MAX_PATH_LEN - 1);
        return;
    }
#endif
    if (argv0) {
        char *copy = strdup(argv0);
        /* strip basename */
        char *p = copy + strlen(copy) - 1;
        while (p > copy && *p != '/' && *p != '\\') p--;
        if (p > copy) *p = '\0';
        strncpy(binary_dir, copy, MAX_PATH_LEN - 1);
        free(copy);
    }
}

void module_manager_init(ModuleManager *mgr) {
    mgr->import_count      = 0;
    mgr->search_path_count = 0;
    mgr->search_paths      = (char**)malloc(sizeof(char*) * 128);
    mgr->stdlib_path[0]    = '\0';
}

void module_manager_add_search_path(ModuleManager *mgr, const char *path) {
    if (!path || !path[0] || mgr->search_path_count >= 127) return;
    for (int i = 0; i < mgr->search_path_count; i++)
        if (strcmp(mgr->search_paths[i], path) == 0) return;
    mgr->search_paths[mgr->search_path_count++] = strdup(path);
}

static void build_search_paths(ModuleManager *mgr, const char *src_dir,
                                const char *extra_stdlib) {
    if (src_dir) module_manager_add_search_path(mgr, src_dir);

    char cwd[MAX_PATH_LEN];
    if (getcwd(cwd, MAX_PATH_LEN)) {
        module_manager_add_search_path(mgr, cwd);
        char buf[MAX_PATH_LEN];
        snprintf(buf, MAX_PATH_LEN, "%s%cstdlib", cwd, PATH_SEP);
        module_manager_add_search_path(mgr, buf);
    }
    if (binary_dir[0]) {
        module_manager_add_search_path(mgr, binary_dir);
        char buf[MAX_PATH_LEN];
        snprintf(buf, MAX_PATH_LEN, "%s%cstdlib", binary_dir, PATH_SEP);
        module_manager_add_search_path(mgr, buf);
    }
    if (extra_stdlib && extra_stdlib[0])
        module_manager_add_search_path(mgr, extra_stdlib);

    char *env = getenv("VELOCITY_STDLIB");
    if (env) module_manager_add_search_path(mgr, env);

    module_manager_add_search_path(mgr, "/usr/local/lib/velocity");
#ifdef _WIN32
    module_manager_add_search_path(mgr, "C:\\velocity\\stdlib");
#endif
}

char* module_manager_find_module(ModuleManager *mgr, const char *name) {
    char path[MAX_PATH_LEN];
    for (int i = 0; i < mgr->search_path_count; i++) {
        snprintf(path, MAX_PATH_LEN, "%s%c%s.vel",
                 mgr->search_paths[i], PATH_SEP, name);
        if (file_exists(path)) return strdup(path);
        snprintf(path, MAX_PATH_LEN, "%s%c%s.asm",
                 mgr->search_paths[i], PATH_SEP, name);
        if (file_exists(path)) return strdup(path);
    }
    return NULL;
}

char* module_manager_find_asm(ModuleManager *mgr, const char *name) {
    char path[MAX_PATH_LEN];
    for (int i = 0; i < mgr->search_path_count; i++) {
        snprintf(path, MAX_PATH_LEN, "%s%c%s.asm",
                 mgr->search_paths[i], PATH_SEP, name);
        if (file_exists(path)) return strdup(path);
    }
    return NULL;
}

bool module_manager_load_module(ModuleManager *mgr,
                                const char *module_name,
                                const char *current_file) {
    for (int i = 0; i < mgr->import_count; i++)
        if (strcmp(mgr->imports[i].module_name, module_name) == 0)
            return true;

    char *src_dir = NULL;
    if (current_file) src_dir = get_directory(current_file);
    build_search_paths(mgr, src_dir,
                       mgr->stdlib_path[0] ? mgr->stdlib_path : NULL);
    if (src_dir) free(src_dir);

    char *primary = module_manager_find_module(mgr, module_name);
    if (!primary) {
        fprintf(stderr,
            "\n\033[1;31merror\033[0m: module '%s' not found.\n"
            "  Searched:\n", module_name);
        for (int i = 0; i < mgr->search_path_count; i++)
            fprintf(stderr, "    %s\n", mgr->search_paths[i]);
        fprintf(stderr,
            "\n  Tip: make sure %s.vel or %s.asm is in the same folder\n"
            "  as your source file, or in a 'stdlib' subfolder next to\n"
            "  the velocity binary.  You can also set VELOCITY_STDLIB.\n",
            module_name, module_name);
        exit(1);
    }

    char *asm_path = module_manager_find_asm(mgr, module_name);
    ImportInfo *imp = &mgr->imports[mgr->import_count++];
    memset(imp, 0, sizeof(*imp));
    strncpy(imp->module_name, module_name,   MAX_TOKEN_LEN - 1);
    strncpy(imp->file_path,   primary,       MAX_PATH_LEN  - 1);

    const char *ext = strrchr(primary, '.');
    imp->is_native_asm  = (ext && strcmp(ext, ".asm") == 0);
    imp->is_standard_lib = false;

    if (asm_path) {
        strncpy(imp->asm_path, asm_path, MAX_PATH_LEN - 1);
        free(asm_path);
    }

    if (strcmp(module_name, "sdl_native") == 0) {
#ifdef _WIN32
        strncpy(imp->link_flags, "-lSDL2", sizeof(imp->link_flags)-1);
#else
        strncpy(imp->link_flags,
                "-L/usr/lib/x86_64-linux-gnu -lSDL2",
                sizeof(imp->link_flags)-1);
#endif
    }

    free(primary);
    return true;
}

void module_manager_free(ModuleManager *mgr) {
    for (int i = 0; i < mgr->search_path_count; i++)
        free(mgr->search_paths[i]);
    free(mgr->search_paths);
}
