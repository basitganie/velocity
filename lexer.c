/*
 * Velocity Compiler - Enhanced Lexer
 * With import and string literal support
 */

#include "velocity.h"
#include <stdarg.h>

static ErrorContext g_err_ctx = {NULL, NULL};

ErrorContext error_get_context(void) { return g_err_ctx; }

void error_set_context(const char *filename, const char *source) {
    g_err_ctx.filename = filename;
    g_err_ctx.source = source;
}

void error_restore_context(ErrorContext ctx) { g_err_ctx = ctx; }

static void error_print_location(int line, int column) {
    if (!g_err_ctx.source || line <= 0 || column <= 0) return;

    const char *s = g_err_ctx.source;
    const char *line_start = s;
    int cur_line = 1;

    while (*s && cur_line < line) {
        if (*s == '\n') {
            cur_line++;
            line_start = s + 1;
        }
        s++;
    }

    if (cur_line != line) return;

    const char *line_end = line_start;
    while (*line_end && *line_end != '\n' && *line_end != '\r') line_end++;

    int line_len = (int)(line_end - line_start);
    if (line_len < 0) return;

    fprintf(stderr, "%5d | %.*s\n", line, line_len, line_start);
    fprintf(stderr, "      | ");

    int caret_pos = column - 1;
    if (caret_pos < 0) caret_pos = 0;
    if (caret_pos > line_len) caret_pos = line_len;
    for (int i = 0; i < caret_pos; i++) fputc(' ', stderr);
    fputc('^', stderr);
    fputc('\n', stderr);
}

void error_at(int line, int column, const char *format, ...) {
    va_list args;
    va_start(args, format);
    if (g_err_ctx.filename)
        fprintf(stderr, "%s:%d:%d: error: ", g_err_ctx.filename, line, column);
    else
        fprintf(stderr, "Error: ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
    error_print_location(line, column);
    exit(1);
}

void error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    if (g_err_ctx.filename)
        fprintf(stderr, "%s: error: ", g_err_ctx.filename);
    else
        fprintf(stderr, "Error: ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(1);
}

void lexer_init(Lexer *lexer, const char *source) {
    lexer->source = source;
    lexer->pos = 0;
    lexer->line = 1;
    lexer->column = 1;
    lexer->current_char = source[0];
}

void lexer_advance(Lexer *lexer) {
    if (lexer->current_char == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    
    lexer->pos++;
    if (lexer->pos >= strlen(lexer->source)) {
        lexer->current_char = '\0';
    } else {
        lexer->current_char = lexer->source[lexer->pos];
    }
}

void lexer_skip_whitespace(Lexer *lexer) {
    while (lexer->current_char != '\0') {
        if (isspace(lexer->current_char)) {
            lexer_advance(lexer);
        } else if (lexer->current_char == '#') {
            // Skip comment until end of line
            while (lexer->current_char != '\0' && lexer->current_char != '\n') {
                lexer_advance(lexer);
            }
        } else {
            break;
        }
    }
}

Token lexer_read_number(Lexer *lexer) {
    Token token;
    token.line = lexer->line;
    token.column = lexer->column;
    
    int i = 0;
    bool is_float = false;
    while (isdigit(lexer->current_char) && i < MAX_TOKEN_LEN - 1) {
        token.value[i++] = lexer->current_char;
        lexer_advance(lexer);
    }

    if (lexer->current_char == '.' && i < MAX_TOKEN_LEN - 1) {
        if (lexer->pos + 1 < strlen(lexer->source) &&
            lexer->source[lexer->pos + 1] == '.') {
            token.value[i] = '\0';
            token.type = TOK_INTEGER;
            return token;
        }
        is_float = true;
        token.value[i++] = lexer->current_char;
        lexer_advance(lexer);
        while (isdigit(lexer->current_char) && i < MAX_TOKEN_LEN - 1) {
            token.value[i++] = lexer->current_char;
            lexer_advance(lexer);
        }
    }

    if ((lexer->current_char == 'e' || lexer->current_char == 'E') &&
        i < MAX_TOKEN_LEN - 1) {
        is_float = true;
        token.value[i++] = lexer->current_char;
        lexer_advance(lexer);
        if ((lexer->current_char == '+' || lexer->current_char == '-') &&
            i < MAX_TOKEN_LEN - 1) {
            token.value[i++] = lexer->current_char;
            lexer_advance(lexer);
        }
        if (!isdigit(lexer->current_char)) {
            error_at(token.line, token.column, "invalid float exponent");
        }
        while (isdigit(lexer->current_char) && i < MAX_TOKEN_LEN - 1) {
            token.value[i++] = lexer->current_char;
            lexer_advance(lexer);
        }
    }
    token.value[i] = '\0';
    token.type = is_float ? TOK_FLOAT : TOK_INTEGER;

    return token;
}

Token lexer_read_string(Lexer *lexer) {
    Token token;
    token.type = TOK_STRING;
    token.line = lexer->line;
    token.column = lexer->column;
    
    lexer_advance(lexer);  // Skip opening "
    
    int i = 0;
    while (lexer->current_char != '"' && lexer->current_char != '\0' && i < MAX_TOKEN_LEN - 1) {
        // Handle escape sequences
        if (lexer->current_char == '\\') {
            lexer_advance(lexer);
            switch (lexer->current_char) {
                case 'n': token.value[i++] = '\n'; break;
                case 't': token.value[i++] = '\t'; break;
                case 'r': token.value[i++] = '\r'; break;
                case '\\': token.value[i++] = '\\'; break;
                case '"': token.value[i++] = '"'; break;
                default: token.value[i++] = lexer->current_char; break;
            }
            lexer_advance(lexer);
        } else {
            token.value[i++] = lexer->current_char;
            lexer_advance(lexer);
        }
    }
    
    if (lexer->current_char != '"') {
        error_at(token.line, token.column, "unterminated string literal");
    }
    
    lexer_advance(lexer);  // Skip closing "
    token.value[i] = '\0';
    
    return token;
}

Token lexer_read_identifier(Lexer *lexer) {
    Token token;
    token.line = lexer->line;
    token.column = lexer->column;
    
    int i = 0;
    while ((isalnum(lexer->current_char) || lexer->current_char == '_') && 
           i < MAX_TOKEN_LEN - 1) {
        token.value[i++] = lexer->current_char;
        lexer_advance(lexer);
    }
    token.value[i] = '\0';
    
    // Check for Kashmiri keywords
    if (strcmp(token.value, "kar") == 0) {
        token.type = TOK_KAR;
    } else if (strcmp(token.value, "chu") == 0) {
        token.type = TOK_CHU;
    } else if (strcmp(token.value, "ath") == 0) {
        token.type = TOK_ATH;
    } else if (strcmp(token.value, "mut") == 0) {
        token.type = TOK_MUT;
    } else if (strcmp(token.value, "agar") == 0) {
        token.type = TOK_AGAR;
    } else if (strcmp(token.value, "nate") == 0) {
        token.type = TOK_NATE;
    } else if (strcmp(token.value, "yeli") == 0) {
        token.type = TOK_YELI;
    } else if (strcmp(token.value, "bar") == 0) {
        token.type = TOK_BAR;
    } else if (strcmp(token.value, "dohraw") == 0) {
        token.type = TOK_BAR;
    } else if (strcmp(token.value, "in") == 0 || strcmp(token.value, "manz") == 0) {
        token.type = TOK_IN;
    } else if (strcmp(token.value, "by") == 0) {
        token.type = TOK_BY;
    } else if (strcmp(token.value, "step") == 0) {
        token.type = TOK_STEP;
    } else if (strcmp(token.value, "break") == 0 || strcmp(token.value, "phutraw") == 0) {
        token.type = TOK_BREAK;
    } else if (strcmp(token.value, "continue") == 0 || strcmp(token.value, "pakh") == 0) {
        token.type = TOK_CONTINUE;
    } else if (strcmp(token.value, "anaw") == 0) {  // NEW!
        token.type = TOK_ANAW;
    } else if (strcmp(token.value, "bina") == 0) {
        token.type = TOK_BINA;
    } else if (strcmp(token.value, "adad") == 0 || strcmp(token.value, "Adad") == 0) {
        token.type = TOK_ADAD;
    } else if (strcmp(token.value, "Ashari") == 0 || strcmp(token.value, "ashari") == 0) {
        token.type = TOK_ASHARI;
    } else if (strcmp(token.value, "Ashari32") == 0 || strcmp(token.value, "ashari32") == 0) {
        token.type = TOK_ASHARI32;
    } else if (strcmp(token.value, "bool") == 0) {
        token.type = TOK_BOOL;
    } else if (strcmp(token.value, "poz") == 0) {
        token.type = TOK_POZ;
    } else if (strcmp(token.value, "apuz") == 0) {
        token.type = TOK_APUZ;
    } else if (strcmp(token.value, "null") == 0) {
        token.type = TOK_NULL;
    } else {
        token.type = TOK_IDENTIFIER;
    }
    
    return token;
}

Token lexer_next_token(Lexer *lexer) {
    Token token;
    
    lexer_skip_whitespace(lexer);
    
    token.line = lexer->line;
    token.column = lexer->column;
    
    if (lexer->current_char == '\0') {
        token.type = TOK_EOF;
        token.value[0] = '\0';
        return token;
    }
    
    // String literals
    if (lexer->current_char == '"') {
        return lexer_read_string(lexer);
    }
    
    // Numbers
    if (isdigit(lexer->current_char)) {
        return lexer_read_number(lexer);
    }
    
    // Identifiers and keywords
    if (isalpha(lexer->current_char) || lexer->current_char == '_') {
        return lexer_read_identifier(lexer);
    }
    
    // Two-character operators
    if (lexer->current_char == '-' && 
        lexer->pos + 1 < strlen(lexer->source) && 
        lexer->source[lexer->pos + 1] == '>') {
        token.type = TOK_ARROW;
        strcpy(token.value, "->");
        lexer_advance(lexer);
        lexer_advance(lexer);
        return token;
    }

    if (lexer->current_char == '.' &&
        lexer->pos + 1 < strlen(lexer->source) &&
        lexer->source[lexer->pos + 1] == '.') {
        token.type = TOK_DOTDOT;
        strcpy(token.value, "..");
        lexer_advance(lexer);
        lexer_advance(lexer);
        if (lexer->current_char == '=') {
            token.type = TOK_DOTDOTEQ;
            strcpy(token.value, "..=");
            lexer_advance(lexer);
        }
        return token;
    }
    
    if (lexer->current_char == '=' && 
        lexer->pos + 1 < strlen(lexer->source) && 
        lexer->source[lexer->pos + 1] == '=') {
        token.type = TOK_EQ;
        strcpy(token.value, "==");
        lexer_advance(lexer);
        lexer_advance(lexer);
        return token;
    }
    
    if (lexer->current_char == '!' && 
        lexer->pos + 1 < strlen(lexer->source) && 
        lexer->source[lexer->pos + 1] == '=') {
        token.type = TOK_NE;
        strcpy(token.value, "!=");
        lexer_advance(lexer);
        lexer_advance(lexer);
        return token;
    }
    
    if (lexer->current_char == '<' && 
        lexer->pos + 1 < strlen(lexer->source) && 
        lexer->source[lexer->pos + 1] == '=') {
        token.type = TOK_LE;
        strcpy(token.value, "<=");
        lexer_advance(lexer);
        lexer_advance(lexer);
        return token;
    }
    
    if (lexer->current_char == '>' && 
        lexer->pos + 1 < strlen(lexer->source) && 
        lexer->source[lexer->pos + 1] == '=') {
        token.type = TOK_GE;
        strcpy(token.value, ">=");
        lexer_advance(lexer);
        lexer_advance(lexer);
        return token;
    }
    
    // Single-character tokens
    char c = lexer->current_char;
    token.value[0] = c;
    token.value[1] = '\0';
    
    switch (c) {
        case '+': token.type = TOK_PLUS; break;
        case '-': token.type = TOK_MINUS; break;
        case '*': token.type = TOK_STAR; break;
        case '/': token.type = TOK_SLASH; break;
        case '%': token.type = TOK_PERCENT; break;
        case '=': token.type = TOK_ASSIGN; break;
        case '<': token.type = TOK_LT; break;
        case '>': token.type = TOK_GT; break;
        case '(': token.type = TOK_LPAREN; break;
        case ')': token.type = TOK_RPAREN; break;
        case '{': token.type = TOK_LBRACE; break;
        case '}': token.type = TOK_RBRACE; break;
        case ';': token.type = TOK_SEMICOLON; break;
        case ':': token.type = TOK_COLON; break;
        case ',': token.type = TOK_COMMA; break;
        case '.': token.type = TOK_DOT; break;  // NEW!
        case '?': token.type = TOK_QUESTION; break;
        case '[': token.type = TOK_LBRACKET; break;
        case ']': token.type = TOK_RBRACKET; break;
        case '|': token.type = TOK_PIPE; break;
        default:
            token.type = TOK_ERROR;
            sprintf(token.value, "Unexpected character: '%c'", c);
            return token;
    }
    
    lexer_advance(lexer);
    return token;
}

int lexer_tokenize(Lexer *lexer, Token *tokens, int max_tokens) {
    int count = 0;
    
    while (count < max_tokens) {
        Token token = lexer_next_token(lexer);
        
        if (token.type == TOK_ERROR) {
            error_at(token.line, token.column, "%s", token.value);
        }
        
        tokens[count++] = token;
        
        if (token.type == TOK_EOF) {
            break;
        }
    }
    
    return count;
}

const char* token_type_name(TokenType type) {
    switch (type) {
        case TOK_KAR: return "KAR";
        case TOK_CHU: return "CHU";
        case TOK_ATH: return "ATH";
        case TOK_MUT: return "MUT";
        case TOK_AGAR: return "AGAR";
        case TOK_NATE: return "NATE";
        case TOK_YELI: return "YELI";
        case TOK_BAR: return "BAR";
        case TOK_IN: return "IN";
        case TOK_BY: return "BY";
        case TOK_STEP: return "STEP";
        case TOK_BREAK: return "BREAK";
        case TOK_CONTINUE: return "CONTINUE";
        case TOK_ANAW: return "ANAW";
        case TOK_BINA: return "BINA";
        case TOK_ADAD: return "ADAD";
        case TOK_ASHARI: return "ASHARI";
        case TOK_ASHARI32: return "ASHARI32";
        case TOK_BOOL: return "BOOL";
        case TOK_POZ: return "POZ";
        case TOK_APUZ: return "APUZ";
        case TOK_NULL: return "NULL";
        case TOK_INTEGER: return "INTEGER";
        case TOK_FLOAT: return "FLOAT";
        case TOK_STRING: return "STRING";
        case TOK_IDENTIFIER: return "IDENTIFIER";
        case TOK_PLUS: return "PLUS";
        case TOK_MINUS: return "MINUS";
        case TOK_STAR: return "STAR";
        case TOK_SLASH: return "SLASH";
        case TOK_PERCENT: return "PERCENT";
        case TOK_ASSIGN: return "ASSIGN";
        case TOK_EQ: return "EQ";
        case TOK_NE: return "NE";
        case TOK_LT: return "LT";
        case TOK_LE: return "LE";
        case TOK_GT: return "GT";
        case TOK_GE: return "GE";
        case TOK_LPAREN: return "LPAREN";
        case TOK_RPAREN: return "RPAREN";
        case TOK_LBRACE: return "LBRACE";
        case TOK_RBRACE: return "RBRACE";
        case TOK_SEMICOLON: return "SEMICOLON";
        case TOK_COLON: return "COLON";
        case TOK_COMMA: return "COMMA";
        case TOK_ARROW: return "ARROW";
        case TOK_DOT: return "DOT";
        case TOK_DOTDOT: return "DOTDOT";
        case TOK_DOTDOTEQ: return "DOTDOTEQ";
        case TOK_QUESTION: return "QUESTION";
        case TOK_LBRACKET: return "LBRACKET";
        case TOK_RBRACKET: return "RBRACKET";
        case TOK_PIPE: return "PIPE";
        case TOK_EOF: return "EOF";
        case TOK_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

char* read_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        error("Could not open file: %s", filename);
    }
    
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char *content = malloc(size + 1);
    if (!content) {
        error("Out of memory");
    }
    
    fread(content, 1, size, file);
    content[size] = '\0';
    
    fclose(file);
    return content;
}
