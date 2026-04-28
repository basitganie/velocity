/*
 * Velocity Compiler - Enhanced Header
 * Windows (MinGW-w64) + Linux cross-platform edition
 * Kashmiri Edition v2
 *
 * New in v2:
 *   - uadad (u64), uadad8 (u8/byte)
 *   - Bitwise ops: AND (&), OR (|), XOR (^), NOT (~), SHL (<<), SHR (>>)
 *   - Logical ops: && (wa), || (ya), ! (na)
 *   - try/catch/panic error handling
 *   - Struct fields can hold arrays and nested structs (fixed)
 *   - Multi-dimensional arrays
 *   - array.append(), array.len tracking fixed
 *   - Better error messages with line/column and source highlighting
 *   - CLI: --help, --version, --verbose, --output, --stdlib
 *   - _ discard variable
 *   - Hex/binary/octal integer literals (0x, 0b, 0o)
 *   - Windows PE64 output (nasm -f win64, link via gcc/ld)
 */

#ifndef VELOCITY_H
#define VELOCITY_H

#ifdef _WIN32
#  include <windows.h>
#  include <direct.h>
#  define PATH_SEP '\\'
#  define PATH_SEP_STR "\\"
#  define getcwd _getcwd
#  define strdup _strdup
#else
#  include <unistd.h>
#  include <sys/stat.h>
#  include <libgen.h>
#  define PATH_SEP '/'
#  define PATH_SEP_STR "/"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdarg.h>

#define VL_VERSION "0.2.0"
#define VL_VERSION_MAJOR 0
#define VL_VERSION_MINOR 2
#define VL_VERSION_PATCH 0

#define MAX_TOKEN_LEN    256
#define MAX_TOKENS       16000
#define MAX_IDENTIFIERS  1024
#define MAX_CODE_LINES   20000
#define MAX_IMPORTS      128
#define MAX_PATH_LEN     1024
#define MAX_VARS         512
#define MAX_LOOP_DEPTH   128
#define LOCAL_STACK_RESERVE 8192

/* ------------------------------------------------------------
 *  Token Types
 * ------------------------------------------------------------ */
typedef enum {
    /* Keywords */
    TOK_KAR,        /* kar   - function      */
    TOK_CHU,        /* chu   - return         */
    TOK_ATH,        /* ath   - let/declare    */
    TOK_MUT,        /* mut   - mutable        */
    TOK_AGAR,       /* agar  - if             */
    TOK_NATE,       /* nate  - else           */
    TOK_YELI,       /* yeli  - while          */
    TOK_BAR,        /* bar   - for            */
    TOK_IN,         /* in / manz              */
    TOK_BY,         /* by    - range step     */
    TOK_STEP,       /* step  - range step     */
    TOK_BREAK,      /* break / phutraw        */
    TOK_CONTINUE,   /* continue / pakh        */
    TOK_ANAW,       /* anaw  - import         */
    TOK_BINA,       /* bina  - struct         */
    TOK_TRY,        /* koshish - try          */
    TOK_CATCH,      /* ratt   - catch        */
    TOK_PANIC,      /* panic                  */
    TOK_THROW,      /* trayith - throw         */
    TOK_APPEND,     /* append                 */

    /* Types */
    TOK_ADAD,       /* adad    - i64          */
    TOK_UADAD,      /* uadad   - u64          */
    TOK_UADAD8,     /* uadad8  - u8/byte      */
    TOK_ASHARI,     /* ashari  - f64          */
    TOK_ASHARI32,   /* ashari32 - f32         */
    TOK_BOOL,       /* bool                   */
    TOK_VOID,       /* khali   - void         */

    /* Bool literals */
    TOK_POZ,        /* poz  - true            */
    TOK_APUZ,       /* apuz - false           */
    TOK_NULL,       /* null                   */

    /* Literals */
    TOK_INTEGER,
    TOK_FLOAT,
    TOK_IDENTIFIER,
    TOK_STRING,
    TOK_HEX,        /* 0xFF  hex literal      */
    TOK_BINARY,     /* 0b101 binary literal   */
    TOK_OCTAL,      /* 0o77  octal literal    */

    /* Arithmetic */
    TOK_PLUS,       /* +  */
    TOK_MINUS,      /* -  */
    TOK_STAR,       /* *  */
    TOK_SLASH,      /* /  */
    TOK_PERCENT,    /* %  */

    /* Bitwise */
    TOK_AMP,        /* &  bitwise AND / logical AND (&&) */
    TOK_DPIPE,      /* || logical OR                     */
    TOK_CARET,      /* ^  XOR                            */
    TOK_TILDE,      /* ~  NOT                            */
    TOK_LSHIFT,     /* << SHL                            */
    TOK_RSHIFT,     /* >> SHR                            */
    TOK_DAMP,       /* && logical AND                    */
    TOK_BANG,       /* !  logical NOT                    */

    /* Comparison */
    TOK_ASSIGN,     /* =  */
    TOK_EQ,         /* == */
    TOK_NE,         /* != */
    TOK_LT,         /* <  */
    TOK_LE,         /* <= */
    TOK_GT,         /* >  */
    TOK_GE,         /* >= */

    /* Compound assignment */
    TOK_PLUS_ASSIGN,    /* += */
    TOK_MINUS_ASSIGN,   /* -= */
    TOK_STAR_ASSIGN,    /* *= */
    TOK_SLASH_ASSIGN,   /* /= */
    TOK_PERCENT_ASSIGN, /* %= */
    TOK_AMP_ASSIGN,     /* &= */
    TOK_PIPE_ASSIGN,    /* |= */
    TOK_CARET_ASSIGN,   /* ^= */
    TOK_SHL_ASSIGN,     /* <<= */
    TOK_SHR_ASSIGN,     /* >>= */

    /* Punctuation */
    TOK_LPAREN,     /* ( */
    TOK_RPAREN,     /* ) */
    TOK_LBRACE,     /* { */
    TOK_RBRACE,     /* } */
    TOK_SEMICOLON,  /* ; */
    TOK_COLON,      /* : */
    TOK_COMMA,      /* , */
    TOK_ARROW,      /* -> */
    TOK_DOT,        /* .  */
    TOK_DOTDOT,     /* .. */
    TOK_DOTDOTEQ,   /* ..= */
    TOK_QUESTION,   /* ?  */
    TOK_LBRACKET,   /* [  */
    TOK_RBRACKET,   /* ]  */
    TOK_PIPE,       /* |  */
    TOK_AT,         /* @  */

    /* Special */
    TOK_UNDERSCORE, /* _  discard */
    TOK_EOF,
    TOK_ERROR
} VelTokenType;

/* ------------------------------------------------------------
 *  Token
 * ------------------------------------------------------------ */
typedef struct {
    VelTokenType type;
    char value[MAX_TOKEN_LEN];
    int line;
    int column;
} Token;

/* ------------------------------------------------------------
 *  Value Types
 * ------------------------------------------------------------ */
typedef enum {
    TYPE_INT,          /* adad    signed 64-bit   */
    TYPE_UINT,         /* uadad   unsigned 64-bit */
    TYPE_UINT8,        /* uadad8  unsigned 8-bit  */
    TYPE_OPT_INT,
    TYPE_BOOL,
    TYPE_OPT_BOOL,
    TYPE_F32,
    TYPE_OPT_F32,
    TYPE_F64,
    TYPE_OPT_F64,
    TYPE_STRING,
    TYPE_OPT_STRING,
    TYPE_NULL,
    TYPE_VOID,
    TYPE_ARRAY,
    TYPE_TUPLE,
    TYPE_STRUCT,
    TYPE_ERROR_VAL     /* error type for try/catch */
} ValueType;

/* ------------------------------------------------------------
 *  TypeInfo  (for composite/complex types)
 * ------------------------------------------------------------ */
typedef struct TypeInfo {
    ValueType kind;               /* TYPE_ARRAY, TYPE_TUPLE, TYPE_STRUCT    */
    ValueType elem_type;          /* for arrays: element type               */
    struct TypeInfo *elem_typeinfo; /* for arrays of structs / nested arrays */
    int array_len;                /* >=0 = fixed, -1 = dynamic              */
    bool is_dynamic;              /* true = append-capable dynamic array    */
    ValueType *tuple_types;
    int tuple_count;
    char struct_name[MAX_TOKEN_LEN];
    bool by_ref;
    bool is_ref;                  /* true = reference param (&T)            */
    char generic_name[MAX_TOKEN_LEN]; /* if non-empty: this slot is a type var  */
    int ndim;                     /* number of dimensions (default 1)       */
    int *dim_sizes;               /* sizes per dimension (for fixed nD)     */
} TypeInfo;

void typeinfo_free(TypeInfo *ti);

/* ------------------------------------------------------------
 *  StructDef
 * ------------------------------------------------------------ */
typedef struct StructDef {
    char name[MAX_TOKEN_LEN];
    char **field_names;
    ValueType *field_types;
    TypeInfo **field_typeinfo;
    int *field_offsets;
    int field_count;
    int size;
    bool sizing;
} StructDef;

/* ------------------------------------------------------------
 *  FunctionSig
 * ------------------------------------------------------------ */
typedef struct FunctionSig {
    char name[MAX_TOKEN_LEN * 2 + 4];
    ValueType return_type;
    TypeInfo *return_typeinfo;
    ValueType *param_types;
    TypeInfo **param_typeinfo;
    bool *param_is_ref;          /* true = &-reference parameter            */
    int param_count;
    bool can_error;   /* true if function can return an error */
    /* generics support */
    char generic_params[8][MAX_TOKEN_LEN]; /* type param names e.g. "T" "U" */
    int  generic_count;
    bool is_generic;
} FunctionSig;

typedef struct FloatLit FloatLit;

/* ------------------------------------------------------------
 *  Import Info
 * ------------------------------------------------------------ */
typedef struct {
    char module_name[MAX_TOKEN_LEN];
    char file_path[MAX_PATH_LEN];
    char asm_path[MAX_PATH_LEN];
    char link_flags[512];
    bool is_standard_lib;
    bool is_native_asm;
} ImportInfo;

/* ------------------------------------------------------------
 *  AST Node Types
 * ------------------------------------------------------------ */
typedef enum {
    AST_PROGRAM,
    AST_STRUCT_DEF,
    AST_FUNCTION,
    AST_PARAM,
    AST_RETURN,
    AST_LET,
    AST_ASSIGN,
    AST_COMPOUND_ASSIGN,   /* +=, -=, *=, /= */
    AST_FIELD_ASSIGN,
    AST_ARRAY_ASSIGN,
    AST_IF,
    AST_WHILE,
    AST_FOR,
    AST_FOR_IN,
    AST_BREAK,
    AST_CONTINUE,
    AST_BINARY_OP,
    AST_UNARY_OP,
    AST_CALL,
    AST_FLOAT,
    AST_INTEGER,
    AST_BOOL,
    AST_NULL,
    AST_STRING,
    AST_ARRAY_LITERAL,
    AST_ARRAY_ACCESS,
    AST_TUPLE_LITERAL,
    AST_TUPLE_ACCESS,
    AST_STRUCT_LITERAL,
    AST_FIELD_ACCESS,
    AST_RANGE,
    AST_LAMBDA,
    AST_IDENTIFIER,
    AST_IMPORT,
    AST_MODULE_CALL,
    AST_TRY_CATCH,         /* try { } catch e { } */
    AST_PANIC,             /* panic("msg") */
    AST_THROW,             /* trayith expr */
    AST_APPEND,            /* arr.append(val) */
    AST_DISCARD            /* _ = expr */
} ASTNodeType;

/* ------------------------------------------------------------
 *  Binary / Unary Operations
 * ------------------------------------------------------------ */
typedef enum {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
    OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE,
    /* Bitwise */
    OP_BAND, OP_BOR, OP_BXOR, OP_BNOT, OP_SHL, OP_SHR,
    /* Logical */
    OP_AND, OP_OR, OP_NOT
} BinaryOp;

typedef enum {
    UOP_NEG, UOP_NOT, UOP_BNOT
} UnaryOp;

/* ------------------------------------------------------------
 *  AST Node
 * ------------------------------------------------------------ */
typedef struct ASTNode {
    ASTNodeType type;
    int line;
    int column;
    union {
        long long int_value;

        bool bool_value;

        struct {
            double value;
            ValueType type;
        } float_lit;

        struct {
            char *value;
            int length;
        } string_lit;

        struct {
            struct ASTNode **elements;
            int count;
            int capacity;
        } array_lit;

        struct {
            struct ASTNode *array;
            struct ASTNode *index;
            struct ASTNode *index2;  /* for 2D: arr[i][j] */
        } array_access;

        struct {
            struct ASTNode **elements;
            int count;
        } tuple_lit;

        struct {
            struct ASTNode *tuple;
            int index;
        } tuple_access;

        struct {
            char struct_name[MAX_TOKEN_LEN];
            char **field_names;
            struct ASTNode **field_values;
            int field_count;
        } struct_lit;

        struct {
            struct ASTNode *base;
            char field_name[MAX_TOKEN_LEN];
        } field_access;

        struct {
            struct ASTNode *start;
            struct ASTNode *end;
            bool inclusive;
            struct ASTNode *step;
        } range;

        struct {
            char param_name[MAX_TOKEN_LEN];
            bool has_type;
            ValueType param_type;
            TypeInfo *param_typeinfo;
            struct ASTNode *body;
        } lambda;

        char identifier[MAX_TOKEN_LEN];

        struct {
            char module_name[MAX_TOKEN_LEN];
        } import;

        struct {
            BinaryOp op;
            struct ASTNode *left;
            struct ASTNode *right;
        } binary;

        struct {
            UnaryOp op;
            struct ASTNode *operand;
        } unary;

        struct {
            char func_name[MAX_TOKEN_LEN];
            struct ASTNode **args;
            int arg_count;
            TypeInfo *return_typeinfo;
        } call;

        struct {
            char module_name[MAX_TOKEN_LEN];
            char func_name[MAX_TOKEN_LEN];
            struct ASTNode **args;
            int arg_count;
            TypeInfo *return_typeinfo;
        } module_call;

        struct {
            char var_name[MAX_TOKEN_LEN];
            struct ASTNode *value;
            ValueType var_type;
            bool has_type;
            bool is_mut;
            TypeInfo *type_info;
        } let;

        struct {
            char var_name[MAX_TOKEN_LEN];
            struct ASTNode *value;
        } assign;

        struct {
            char var_name[MAX_TOKEN_LEN];
            struct ASTNode *value;
            BinaryOp op;  /* which compound op */
        } compound_assign;

        struct {
            char var_name[MAX_TOKEN_LEN];   /* simple: s.field = val */
            char field_name[MAX_TOKEN_LEN];
            struct ASTNode *value;
        } field_assign;

        struct {
            char var_name[MAX_TOKEN_LEN];
            struct ASTNode *index;
            struct ASTNode *index2;  /* for 2D */
            struct ASTNode *value;
        } array_assign;

        struct {
            struct ASTNode *value;
        } ret;

        struct {
            struct ASTNode *condition;
            struct ASTNode **then_stmts;
            int then_count;
            struct ASTNode **else_if_conds;
            struct ASTNode ***else_if_stmts;
            int *else_if_counts;
            int else_if_count;
            struct ASTNode **else_stmts;
            int else_count;
        } if_stmt;

        struct {
            struct ASTNode *condition;
            struct ASTNode **body;
            int body_count;
        } while_stmt;

        struct {
            struct ASTNode *init;
            struct ASTNode *condition;
            struct ASTNode *post;
            struct ASTNode **body;
            int body_count;
        } for_stmt;

        struct {
            char var_name[MAX_TOKEN_LEN];
            bool is_mut;
            bool has_type;
            ValueType var_type;
            struct ASTNode *iterable;
            struct ASTNode **body;
            int body_count;
        } for_in_stmt;

        struct {
            char name[MAX_TOKEN_LEN];
            char **param_names;
            ValueType *param_types;
            TypeInfo **param_typeinfo;
            bool *param_is_ref;           /* &-reference flags per param     */
            int param_count;
            ValueType return_type;
            TypeInfo *return_typeinfo;
            struct ASTNode **body;
            int body_count;
            bool is_exported;
            /* generics */
            char generic_params[8][MAX_TOKEN_LEN];
            int  generic_count;
        } function;

        struct {
            char name[MAX_TOKEN_LEN];
            char **field_names;
            ValueType *field_types;
            TypeInfo **field_typeinfo;
            int field_count;
            char generic_params[8][MAX_TOKEN_LEN];
            int  generic_count;
        } struct_def;

        struct {
            struct ASTNode **imports;
            int import_count;
            struct ASTNode **structs;
            int struct_count;
            struct ASTNode **functions;
            int function_count;
        } program;

        /* try { body } catch(e) { handler } */
        struct {
            struct ASTNode **try_body;
            int try_count;
            char catch_var[MAX_TOKEN_LEN];
            struct ASTNode **catch_body;
            int catch_count;
        } try_catch;

        /* panic("message") */
        struct {
            struct ASTNode *message;
        } panic_stmt;

        /* throw expr */
        struct {
            struct ASTNode *value;
        } throw_stmt;

        /* arr.append(val) */
        struct {
            char arr_name[MAX_TOKEN_LEN];
            struct ASTNode *value;
        } append_stmt;

    } data;
} ASTNode;

/* ------------------------------------------------------------
 *  Lexer
 * ------------------------------------------------------------ */
typedef struct {
    const char *source;
    int pos;
    int line;
    int column;
    char current_char;
} Lexer;

/* ------------------------------------------------------------
 *  Parser
 * ------------------------------------------------------------ */
typedef struct {
    Token *tokens;
    int token_count;
    int pos;
} Parser;

/* ------------------------------------------------------------
 *  Module Manager
 * ------------------------------------------------------------ */
typedef struct {
    ImportInfo imports[MAX_IMPORTS];
    int import_count;
    char **search_paths;
    int search_path_count;
    char stdlib_path[MAX_PATH_LEN];
} ModuleManager;

/* ------------------------------------------------------------
 *  Code Generator
 * ------------------------------------------------------------ */
typedef struct {
    FILE *output;
    int label_counter;
    char **local_vars;
    int *var_offsets;
    int var_count;
    int stack_offset;
    ModuleManager *module_mgr;
    char current_module[MAX_TOKEN_LEN];
    char **string_literals;
    int string_count;
    int string_cap;
    struct FunctionSig *func_sigs;
    int func_sig_count;
    int func_sig_cap;
    ValueType *var_types;
    bool *var_mutable;
    int *var_sizes;
    TypeInfo **var_typeinfo;
    ValueType current_return_type;
    TypeInfo *current_return_typeinfo;
    bool has_sret;
    int sret_offset;
    bool needs_arr_alloc;
    bool needs_arr_append;
    bool needs_lafz_parse;
    bool needs_panic;
    struct FloatLit *float_literals;
    int float_count;
    int float_cap;
    int loop_continue_labels[MAX_LOOP_DEPTH];
    int loop_break_labels[MAX_LOOP_DEPTH];
    int loop_depth;
    StructDef *struct_defs;
    int struct_count;
    int struct_cap;
    bool target_windows;   /* true = emit win64 ABI, false = Linux SysV */
    bool target_aarch64;   /* true = emit AArch64 GNU as, Android Bionic */
    char module_prefix[MAX_TOKEN_LEN]; /* if set: prefix all funcs, skip _start */
    int try_depth;         /* for try/catch nesting */
    int try_catch_labels[64]; /* catch label stack */
    int error_var_offsets[64]; /* error var offset stack */
} CodeGen;

/* ------------------------------------------------------------
 *  Error Context
 * ------------------------------------------------------------ */
typedef struct {
    const char *filename;
    const char *source;
} ErrorContext;

/* ------------------------------------------------------------
 *  Compiler Options
 * ------------------------------------------------------------ */
typedef struct {
    const char *input_file;
    const char *output_file;
    bool verbose;
    bool target_windows;
    bool target_aarch64;   /* --target aarch64-linux-android */
    bool keep_asm;
    const char *stdlib_path;
    bool emit_ast;
    bool no_link;
    const char *module_compile_name;  /* --module <name>: compile as a module, prefix funcs */
} CompilerOptions;

/* ------------------------------------------------------------
 *  Function Declarations
 * ------------------------------------------------------------ */

/* Error / diagnostics */
ErrorContext error_get_context(void);
void error_set_context(const char *filename, const char *source);
void error_restore_context(ErrorContext ctx);
void error_at(int line, int column, const char *format, ...);
void error(const char *format, ...);
void warn_at(int line, int column, const char *format, ...);

/* Utilities */
ASTNode* ast_node_new(ASTNodeType type);
void ast_node_free(ASTNode *node);
const char* token_type_name(VelTokenType type);
char* read_file(const char *filename);
char* get_directory(const char *filepath);
char* path_join(const char *dir, const char *file);
bool file_exists(const char *path);

/* Module Manager */
void module_manager_set_binary_dir(const char *argv0);
void module_manager_init(ModuleManager *mgr);
void module_manager_add_search_path(ModuleManager *mgr, const char *path);
char* module_manager_find_module(ModuleManager *mgr, const char *module_name);
char* module_manager_find_asm(ModuleManager *mgr, const char *module_name);
bool module_manager_load_module(ModuleManager *mgr, const char *module_name, const char *current_file);
void module_manager_free(ModuleManager *mgr);

/* Lexer */
void lexer_init(Lexer *lexer, const char *source);
int lexer_tokenize(Lexer *lexer, Token *tokens, int max_tokens);
const char* token_type_name(VelTokenType type);

/* Parser */
void parser_init(Parser *parser, Token *tokens, int token_count);
Token parser_current(Parser *parser);
void parser_advance(Parser *parser);
bool parser_match(Parser *parser, VelTokenType type);
Token parser_expect(Parser *parser, VelTokenType type);
ASTNode* parse_program(Parser *parser);

/* Code Generator */
void codegen_init(CodeGen *cg, FILE *output, ModuleManager *mgr, bool target_windows, bool target_aarch64, const char *module_prefix);
void codegen_emit(CodeGen *cg, const char *format, ...);
int codegen_new_label(CodeGen *cg);
void codegen_function(CodeGen *cg, ASTNode *func);
void codegen_statement(CodeGen *cg, ASTNode *stmt);
ValueType codegen_expression(CodeGen *cg, ASTNode *expr);
void codegen_program(CodeGen *cg, ASTNode *program);
int codegen_find_var(CodeGen *cg, const char *name);
void codegen_add_var(CodeGen *cg, const char *name, ValueType type, bool is_mut, TypeInfo *tinfo);

#endif /* VELOCITY_H */
