/*
 * Velocity Compiler - Enhanced Header with Module System
 * Kashmiri Edition with Import Support
 */

#ifndef VELOCITY_H
#define VELOCITY_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <libgen.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_TOKEN_LEN 256
#define MAX_TOKENS 10000
#define MAX_IDENTIFIERS 1000
#define MAX_CODE_LINES 10000
#define MAX_IMPORTS 100
#define MAX_PATH_LEN 1024

/* Token Types */
typedef enum {
    // Keywords (Kashmiri)
    TOK_KAR,        // kar (function)
    TOK_CHU,        // chu (return)
    TOK_ATH,        // ath (let)
    TOK_MUT,        // mut (mutable)
    TOK_AGAR,       // agar (if)
    TOK_NATE,       // nate (else)
    TOK_YELI,       // yeli (while)
    TOK_BAR,        // bar (for)
    TOK_IN,         // in/manz (iterator)
    TOK_BY,         // by (range step)
    TOK_STEP,       // step (range step)
    TOK_BREAK,      // break
    TOK_CONTINUE,   // continue
    TOK_ANAW,       // anaw (import) - NEW!
    TOK_BINA,       // bina (struct)
    
    // Types
    TOK_ADAD,       // adad (i32/number)
    TOK_ASHARI,     // ashari (f64)
    TOK_ASHARI32,   // ashari32 (f32)
    TOK_BOOL,       // bool

    // Bool literals
    TOK_POZ,        // poz (true)
    TOK_APUZ,       // apuz (false)
    TOK_NULL,       // null
    
    // Literals
    TOK_INTEGER,
    TOK_FLOAT,
    TOK_IDENTIFIER,
    TOK_STRING,     // String literal - NEW!
    
    // Operators
    TOK_PLUS,       // +
    TOK_MINUS,      // -
    TOK_STAR,       // *
    TOK_SLASH,      // /
    TOK_PERCENT,    // %
    TOK_ASSIGN,     // =
    TOK_EQ,         // ==
    TOK_NE,         // !=
    TOK_LT,         // <
    TOK_LE,         // <=
    TOK_GT,         // >
    TOK_GE,         // >=
    
    // Punctuation
    TOK_LPAREN,     // (
    TOK_RPAREN,     // )
    TOK_LBRACE,     // {
    TOK_RBRACE,     // }
    TOK_SEMICOLON,  // ;
    TOK_COLON,      // :
    TOK_COMMA,      // ,
    TOK_ARROW,      // ->
    TOK_DOT,        // . - NEW! For module.function
    TOK_DOTDOT,     // ..  range (exclusive)
    TOK_DOTDOTEQ,   // ..= range (inclusive)
    TOK_QUESTION,   // ? - optional types
    TOK_LBRACKET,   // [
    TOK_RBRACKET,   // ]
    TOK_PIPE,       // |
    
    // Special
    TOK_EOF,
    TOK_ERROR
} TokenType;

/* Token Structure */
typedef struct {
    TokenType type;
    char value[MAX_TOKEN_LEN];
    int line;
    int column;
} Token;

/* Import Information */
typedef struct {
    char module_name[MAX_TOKEN_LEN];
    char file_path[MAX_PATH_LEN];   /* .vel path, or .asm path if no .vel */
    char asm_path[MAX_PATH_LEN];    /* .asm sidecar path, or "" if none   */
    char link_flags[256];           /* linker flags requested by module */
    bool is_standard_lib;
    bool is_native_asm;             /* true = no .vel, only .asm          */
} ImportInfo;

/* AST Node Types */
typedef enum {
    TYPE_INT,
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
    TYPE_ARRAY,
    TYPE_TUPLE,
    TYPE_STRUCT
} ValueType;

typedef struct TypeInfo {
    ValueType kind; /* TYPE_ARRAY, TYPE_TUPLE, or TYPE_STRUCT */
    ValueType elem_type;
    int array_len;
    ValueType *tuple_types;
    int tuple_count;
    char struct_name[MAX_TOKEN_LEN];
    bool by_ref;
} TypeInfo;

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

typedef struct FunctionSig {
    char name[MAX_TOKEN_LEN * 2 + 4];
    ValueType return_type;
    TypeInfo *return_typeinfo;
    ValueType *param_types;
    TypeInfo **param_typeinfo;
    int param_count;
} FunctionSig;

typedef struct FloatLit FloatLit;

/* AST Node Types */
typedef enum {
    AST_PROGRAM,
    AST_STRUCT_DEF,
    AST_FUNCTION,
    AST_PARAM,
    AST_RETURN,
    AST_LET,
    AST_ASSIGN,
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
    AST_IMPORT,         // NEW!
    AST_MODULE_CALL     // NEW! For module.function()
} ASTNodeType;

/* Binary Operations */
typedef enum {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
    OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE
} BinaryOp;

/* AST Node */
typedef struct ASTNode {
    ASTNodeType type;
    union {
        // Integer literal
        int int_value;

        // Bool literal
        bool bool_value;

        // Float literal
        struct {
            double value;
            ValueType type; /* TYPE_F32 or TYPE_F64 */
        } float_lit;

        // String literal
        struct {
            char *value;
            int length;
        } string_lit;

        // Array literal
        struct {
            struct ASTNode **elements;
            int count;
        } array_lit;

        // Array access
        struct {
            struct ASTNode *array;
            struct ASTNode *index;
        } array_access;

        // Tuple literal
        struct {
            struct ASTNode **elements;
            int count;
        } tuple_lit;

        // Tuple access
        struct {
            struct ASTNode *tuple;
            int index;
        } tuple_access;

        // Struct literal
        struct {
            char struct_name[MAX_TOKEN_LEN];
            char **field_names;
            struct ASTNode **field_values;
            int field_count;
        } struct_lit;

        // Struct field access
        struct {
            struct ASTNode *base;
            char field_name[MAX_TOKEN_LEN];
        } field_access;

        // Range expression (start..end or start..=end)
        struct {
            struct ASTNode *start;
            struct ASTNode *end;
            bool inclusive;
            struct ASTNode *step;
        } range;

        // Lambda expression
        struct {
            char param_name[MAX_TOKEN_LEN];
            bool has_type;
            ValueType param_type;
            TypeInfo *param_typeinfo;
            struct ASTNode *body;
        } lambda;
        
        // Identifier
        char identifier[MAX_TOKEN_LEN];
        
        // Import statement
        struct {
            char module_name[MAX_TOKEN_LEN];
        } import;
        
        // Binary operation
        struct {
            BinaryOp op;
            struct ASTNode *left;
            struct ASTNode *right;
        } binary;
        
        // Function call
        struct {
            char func_name[MAX_TOKEN_LEN];
            struct ASTNode **args;
            int arg_count;
        } call;
        
        // Module function call (module.function)
        struct {
            char module_name[MAX_TOKEN_LEN];
            char func_name[MAX_TOKEN_LEN];
            struct ASTNode **args;
            int arg_count;
        } module_call;
        
        // Let statement
        struct {
            char var_name[MAX_TOKEN_LEN];
            struct ASTNode *value;
            ValueType var_type;
            bool has_type;
            bool is_mut;
            TypeInfo *type_info;
        } let;
        
        // Assignment
        struct {
            char var_name[MAX_TOKEN_LEN];
            struct ASTNode *value;
        } assign;

        // Struct field assignment
        struct {
            char var_name[MAX_TOKEN_LEN];
            char field_name[MAX_TOKEN_LEN];
            struct ASTNode *value;
        } field_assign;

        // Array element assignment
        struct {
            char var_name[MAX_TOKEN_LEN];
            struct ASTNode *index;
            struct ASTNode *value;
        } array_assign;
        
        // Return statement
        struct {
            struct ASTNode *value;
        } ret;
        
        // If statement
        struct {
            struct ASTNode *condition;
            struct ASTNode **then_stmts;
            int then_count;
            struct ASTNode **else_stmts;
            int else_count;
        } if_stmt;
        
        // While loop
        struct {
            struct ASTNode *condition;
            struct ASTNode **body;
            int body_count;
        } while_stmt;

        // For loop (C-style)
        struct {
            struct ASTNode *init;
            struct ASTNode *condition;
            struct ASTNode *post;
            struct ASTNode **body;
            int body_count;
        } for_stmt;

        // For-in loop
        struct {
            char var_name[MAX_TOKEN_LEN];
            bool is_mut;
            bool has_type;
            ValueType var_type;
            struct ASTNode *iterable;
            struct ASTNode **body;
            int body_count;
        } for_in_stmt;
        
        // Function
        struct {
            char name[MAX_TOKEN_LEN];
            char **param_names;
            ValueType *param_types;
            TypeInfo **param_typeinfo;
            int param_count;
            ValueType return_type;
            TypeInfo *return_typeinfo;
            struct ASTNode **body;
            int body_count;
            bool is_exported;  // NEW! For library functions
        } function;

        // Struct definition
        struct {
            char name[MAX_TOKEN_LEN];
            char **field_names;
            ValueType *field_types;
            TypeInfo **field_typeinfo;
            int field_count;
        } struct_def;
        
        // Program
        struct {
            struct ASTNode **imports;
            int import_count;
            struct ASTNode **structs;
            int struct_count;
            struct ASTNode **functions;
            int function_count;
        } program;
    } data;
} ASTNode;

/* Lexer State */
typedef struct {
    const char *source;
    int pos;
    int line;
    int column;
    char current_char;
} Lexer;

/* Parser State */
typedef struct {
    Token *tokens;
    int token_count;
    int pos;
} Parser;

/* Module Manager */
typedef struct {
    ImportInfo imports[MAX_IMPORTS];
    int import_count;
    char **search_paths;
    int search_path_count;
    char stdlib_path[MAX_PATH_LEN];
} ModuleManager;

/* Code Generator State */
typedef struct {
    FILE *output;
    int label_counter;
    char **local_vars;
    int *var_offsets;
    int var_count;
    int stack_offset;
    ModuleManager *module_mgr;
    char current_module[MAX_TOKEN_LEN]; /* module being compiled, "" for user code */
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
    bool needs_lafz_parse;
    struct FloatLit *float_literals;
    int float_count;
    int float_cap;
    int loop_continue_labels[64];
    int loop_break_labels[64];
    int loop_depth;
    StructDef *struct_defs;
    int struct_count;
    int struct_cap;
} CodeGen;

/* Function Declarations */

// Module Manager
void module_manager_set_binary_dir(const char *argv0);
void module_manager_init(ModuleManager *mgr);
void module_manager_add_search_path(ModuleManager *mgr, const char *path);
char* module_manager_find_module(ModuleManager *mgr, const char *module_name);
char* module_manager_find_asm(ModuleManager *mgr, const char *module_name);
bool module_manager_load_module(ModuleManager *mgr, const char *module_name, const char *current_file);
void module_manager_free(ModuleManager *mgr);

// Lexer
void lexer_init(Lexer *lexer, const char *source);
void lexer_advance(Lexer *lexer);
void lexer_skip_whitespace(Lexer *lexer);
Token lexer_read_number(Lexer *lexer);
Token lexer_read_identifier(Lexer *lexer);
Token lexer_read_string(Lexer *lexer);  // NEW!
Token lexer_next_token(Lexer *lexer);
int lexer_tokenize(Lexer *lexer, Token *tokens, int max_tokens);

// Parser
void parser_init(Parser *parser, Token *tokens, int token_count);
Token parser_current(Parser *parser);
void parser_advance(Parser *parser);
bool parser_match(Parser *parser, TokenType type);
Token parser_expect(Parser *parser, TokenType type);
ASTNode* parse_program(Parser *parser);
ASTNode* parse_import(Parser *parser);  // NEW!
ASTNode* parse_struct(Parser *parser);  // NEW!
ASTNode* parse_function(Parser *parser);
ASTNode* parse_statement(Parser *parser);
ASTNode* parse_expression(Parser *parser);

// Code Generator
void codegen_init(CodeGen *cg, FILE *output, ModuleManager *mgr);
void codegen_emit(CodeGen *cg, const char *format, ...);
int codegen_new_label(CodeGen *cg);
void codegen_function(CodeGen *cg, ASTNode *func);
void codegen_statement(CodeGen *cg, ASTNode *stmt);
ValueType codegen_expression(CodeGen *cg, ASTNode *expr);
void codegen_program(CodeGen *cg, ASTNode *program);

// Utilities
ASTNode* ast_node_new(ASTNodeType type);
void ast_node_free(ASTNode *node);
const char* token_type_name(TokenType type);
void error(const char *format, ...);
typedef struct {
    const char *filename;
    const char *source;
} ErrorContext;
ErrorContext error_get_context(void);
void error_set_context(const char *filename, const char *source);
void error_restore_context(ErrorContext ctx);
void error_at(int line, int column, const char *format, ...);
char* read_file(const char *filename);
char* get_directory(const char *filepath);

#endif /* VELOCITY_H */
