/*
 * Velocity Compiler - Lexer
 * Kashmiri Edition v2 - Windows/Linux portable
 *
 * New features:
 *  - Hex (0xFF), binary (0b101), octal (0o77) literals
 *  - Bitwise: & | ^ ~ << >>
 *  - Logical: && || !
 *  - Compound assignment: += -= *= /=
 *  - try/catch/panic/throw keywords
 *  - uadad, uadad8 types
 *  - _ discard identifier
 *  - append keyword
 *  - khali (void) type
 *  - wa (&&), ya (||), na (!) Kashmiri aliases
 */

#include "velocity.h"

/* -- Error context ----------------------------------------------- */

static ErrorContext g_err_ctx = {NULL, NULL};

ErrorContext error_get_context(void)          { return g_err_ctx; }
void error_set_context(const char *f, const char *s) {
    g_err_ctx.filename = f;
    g_err_ctx.source   = s;
}
void error_restore_context(ErrorContext ctx) { g_err_ctx = ctx; }

static void print_source_line(int line, int column) {
    if (!g_err_ctx.source || line <= 0) return;
    const char *s = g_err_ctx.source;
    const char *line_start = s;
    int cur = 1;
    while (*s && cur < line) {
        if (*s == '\n') { cur++; line_start = s + 1; }
        s++;
    }
    if (cur != line) return;
    const char *line_end = line_start;
    while (*line_end && *line_end != '\n' && *line_end != '\r') line_end++;
    int len = (int)(line_end - line_start);
    if (len < 0) len = 0;
    fprintf(stderr, "\n  %5d | %.*s\n", line, len, line_start);
    fprintf(stderr,   "        | ");
    int caret = column - 1;
    if (caret < 0) caret = 0;
    if (caret > len) caret = len;
    for (int i = 0; i < caret; i++) {
        fputc(line_start[i] == '\t' ? '\t' : ' ', stderr);
    }
    fprintf(stderr, "^\n");
}

void error_at(int line, int column, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "\n\033[1;31merror\033[0m");
    if (g_err_ctx.filename)
        fprintf(stderr, " [%s:%d:%d]", g_err_ctx.filename, line, column);
    fprintf(stderr, ": ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    print_source_line(line, column);
    exit(1);
}

void error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "\n\033[1;31merror\033[0m");
    if (g_err_ctx.filename)
        fprintf(stderr, " [%s]", g_err_ctx.filename);
    fprintf(stderr, ": ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

void warn_at(int line, int column, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "\n\033[1;33mwarning\033[0m");
    if (g_err_ctx.filename)
        fprintf(stderr, " [%s:%d:%d]", g_err_ctx.filename, line, column);
    fprintf(stderr, ": ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    print_source_line(line, column);
}

/* -- File Utilities ----------------------------------------------- */

char* read_file(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) error("Cannot open file: %s", filename);
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char*)malloc(size + 2);
    if (!buf) error("Out of memory");
    size_t rd = fread(buf, 1, size, f);
    buf[rd] = '\n';
    buf[rd + 1] = '\0';
    fclose(f);
    return buf;
}

bool file_exists(const char *path) {
#ifdef _WIN32
    DWORD a = GetFileAttributesA(path);
    return (a != INVALID_FILE_ATTRIBUTES);
#else
    struct stat st;
    return stat(path, &st) == 0;
#endif
}

char* get_directory(const char *filepath) {
    if (!filepath) return strdup(".");
    char *copy = strdup(filepath);
    /* find last path separator */
    char *p = copy + strlen(copy) - 1;
    while (p > copy && *p != '/' && *p != '\\') p--;
    if (p == copy && *p != '/' && *p != '\\') {
        free(copy);
        return strdup(".");
    }
    *p = '\0';
    char *result = strdup(copy);
    free(copy);
    return result;
}

char* path_join(const char *dir, const char *file) {
    size_t dlen = strlen(dir);
    size_t flen = strlen(file);
    char *result = (char*)malloc(dlen + flen + 3);
    if (!result) error("Out of memory");
    strcpy(result, dir);
    if (dlen > 0 && dir[dlen-1] != '/' && dir[dlen-1] != '\\') {
        result[dlen] = PATH_SEP;
        strcpy(result + dlen + 1, file);
    } else {
        strcpy(result + dlen, file);
    }
    return result;
}

/* -- Lexer core --------------------------------------------------- */

void lexer_init(Lexer *lexer, const char *source) {
    lexer->source = source;
    lexer->pos    = 0;
    lexer->line   = 1;
    lexer->column = 1;
    lexer->current_char = source[0];
}

static void lexer_advance(Lexer *lx) {
    if (lx->current_char == '\n') {
        lx->line++;
        lx->column = 1;
    } else {
        lx->column++;
    }
    lx->pos++;
    lx->current_char = lx->source[lx->pos]; /* '\0' at end is fine */
}

static void lexer_skip_ws(Lexer *lx) {
    while (lx->current_char != '\0') {
        if (isspace((unsigned char)lx->current_char)) {
            lexer_advance(lx);
        } else if (lx->current_char == '#') {
            /* line comment */
            while (lx->current_char != '\0' && lx->current_char != '\n')
                lexer_advance(lx);
        } else if (lx->current_char == '/' &&
                   lx->source[lx->pos + 1] == '/') {
            /* C++ style line comment */
            while (lx->current_char != '\0' && lx->current_char != '\n')
                lexer_advance(lx);
        } else if (lx->current_char == '/' &&
                   lx->source[lx->pos + 1] == '*') {
            /* Block comment */
            lexer_advance(lx); lexer_advance(lx);
            while (lx->current_char != '\0') {
                if (lx->current_char == '*' &&
                    lx->source[lx->pos + 1] == '/') {
                    lexer_advance(lx); lexer_advance(lx);
                    break;
                }
                lexer_advance(lx);
            }
        } else {
            break;
        }
    }
}

static Token read_number(Lexer *lx) {
    Token tok;
    tok.line   = lx->line;
    tok.column = lx->column;
    int i = 0;
    bool is_float = false;

    /* Check for 0x/0b/0o prefixes */
    if (lx->current_char == '0' &&
        (lx->source[lx->pos + 1] == 'x' || lx->source[lx->pos + 1] == 'X')) {
        lexer_advance(lx); lexer_advance(lx);
        while (isxdigit((unsigned char)lx->current_char) && i < MAX_TOKEN_LEN - 1)
            tok.value[i++] = lx->current_char, lexer_advance(lx);
        tok.value[i] = '\0';
        tok.type = TOK_HEX;
        return tok;
    }
    if (lx->current_char == '0' &&
        (lx->source[lx->pos + 1] == 'b' || lx->source[lx->pos + 1] == 'B')) {
        lexer_advance(lx); lexer_advance(lx);
        while ((lx->current_char == '0' || lx->current_char == '1') &&
               i < MAX_TOKEN_LEN - 1)
            tok.value[i++] = lx->current_char, lexer_advance(lx);
        tok.value[i] = '\0';
        tok.type = TOK_BINARY;
        return tok;
    }
    if (lx->current_char == '0' &&
        (lx->source[lx->pos + 1] == 'o' || lx->source[lx->pos + 1] == 'O')) {
        lexer_advance(lx); lexer_advance(lx);
        while (lx->current_char >= '0' && lx->current_char <= '7' &&
               i < MAX_TOKEN_LEN - 1)
            tok.value[i++] = lx->current_char, lexer_advance(lx);
        tok.value[i] = '\0';
        tok.type = TOK_OCTAL;
        return tok;
    }

    /* Decimal integer or float */
    while (isdigit((unsigned char)lx->current_char) && i < MAX_TOKEN_LEN - 1)
        tok.value[i++] = lx->current_char, lexer_advance(lx);

    if (lx->current_char == '.' && i < MAX_TOKEN_LEN - 1) {
        /* peek: is it a range '..'? */
        if (lx->source[lx->pos + 1] == '.') {
            tok.value[i] = '\0';
            tok.type = TOK_INTEGER;
            return tok;
        }
        is_float = true;
        tok.value[i++] = '.';
        lexer_advance(lx);
        while (isdigit((unsigned char)lx->current_char) && i < MAX_TOKEN_LEN - 1)
            tok.value[i++] = lx->current_char, lexer_advance(lx);
    }
    if ((lx->current_char == 'e' || lx->current_char == 'E') &&
        i < MAX_TOKEN_LEN - 1) {
        is_float = true;
        tok.value[i++] = lx->current_char; lexer_advance(lx);
        if ((lx->current_char == '+' || lx->current_char == '-') &&
            i < MAX_TOKEN_LEN - 1)
            tok.value[i++] = lx->current_char, lexer_advance(lx);
        while (isdigit((unsigned char)lx->current_char) && i < MAX_TOKEN_LEN - 1)
            tok.value[i++] = lx->current_char, lexer_advance(lx);
    }
    tok.value[i] = '\0';
    tok.type = is_float ? TOK_FLOAT : TOK_INTEGER;
    return tok;
}

static Token read_string(Lexer *lx) {
    Token tok;
    tok.type   = TOK_STRING;
    tok.line   = lx->line;
    tok.column = lx->column;
    lexer_advance(lx); /* skip opening " */
    int i = 0;
    while (lx->current_char != '"' && lx->current_char != '\0' &&
           i < MAX_TOKEN_LEN - 1) {
        if (lx->current_char == '\\') {
            lexer_advance(lx);
            switch (lx->current_char) {
                case 'n':  tok.value[i++] = '\n'; break;
                case 't':  tok.value[i++] = '\t'; break;
                case 'r':  tok.value[i++] = '\r'; break;
                case '\\': tok.value[i++] = '\\'; break;
                case '"':  tok.value[i++] = '"';  break;
                case '0':  tok.value[i++] = '\0'; break;
                default:   tok.value[i++] = lx->current_char; break;
            }
        } else {
            tok.value[i++] = lx->current_char;
        }
        lexer_advance(lx);
    }
    if (lx->current_char != '"')
        error_at(tok.line, tok.column, "unterminated string literal");
    lexer_advance(lx);
    tok.value[i] = '\0';
    return tok;
}

static Token read_identifier(Lexer *lx) {
    Token tok;
    tok.line   = lx->line;
    tok.column = lx->column;
    int i = 0;
    while ((isalnum((unsigned char)lx->current_char) ||
            lx->current_char == '_') && i < MAX_TOKEN_LEN - 1) {
        tok.value[i++] = lx->current_char;
        lexer_advance(lx);
    }
    tok.value[i] = '\0';

    /* Keyword table */
    struct { const char *kw; VelTokenType tt; } kwtab[] = {
        {"kar",       TOK_KAR},
        {"chu",       TOK_CHU},
        {"wapas",     TOK_CHU},    /* wapas = return (Kashmiri: "go back") */
        {"ath",       TOK_ATH},
        {"mut",       TOK_MUT},
        {"agar",      TOK_AGAR},
        {"nate",      TOK_NATE},
        {"yeli",      TOK_YELI},
        {"bar",       TOK_BAR},
        {"dohraw",    TOK_BAR},
        {"in",        TOK_IN},
        {"manz",      TOK_IN},
        {"by",        TOK_BY},
        {"jadh",      TOK_STEP},    /* jadh = step in range (was 'step', freed as ident) */
        {"break",     TOK_BREAK},
        {"phutraw",   TOK_BREAK},
        {"continue",  TOK_CONTINUE},
        {"pakh",      TOK_CONTINUE},
        {"anaw",      TOK_ANAW},
        {"bina",      TOK_BINA},
        /* try/catch/panic/throw */
        {"koshish",   TOK_TRY},
        {"try",       TOK_TRY},
        {"ratt",     TOK_CATCH},
        {"catch",     TOK_CATCH},
        {"panic",     TOK_PANIC},
        {"trayith",    TOK_THROW},
        {"throw",     TOK_THROW},
        {"append",    TOK_APPEND},
        /* types */
        {"adad",      TOK_ADAD},
        {"Adad",      TOK_ADAD},
        {"uadad",     TOK_UADAD},
        {"uadad8",    TOK_UADAD8},
        {"ashari",    TOK_ASHARI},
        {"Ashari",    TOK_ASHARI},
        {"ashari32",  TOK_ASHARI32},
        {"Ashari32",  TOK_ASHARI32},
        {"bool",      TOK_BOOL},
        {"khali",     TOK_VOID},
        {"void",      TOK_VOID},
        /* bool literals */
        {"poz",       TOK_POZ},
        {"apuz",      TOK_APUZ},
        {"null",      TOK_NULL},
        /* logical aliases */
        {"wa",        TOK_DAMP},   /* wa = && */
        {"ya",        TOK_DPIPE},  /* ya = || */
        {"na",        TOK_BANG},   /* na = !  */
        /* string type aliases */
        {"lafz",      TOK_IDENTIFIER},
        {"lafz",      TOK_IDENTIFIER},
        {NULL, 0}
    };

    for (int k = 0; kwtab[k].kw; k++) {
        if (strcmp(tok.value, kwtab[k].kw) == 0) {
            tok.type = kwtab[k].tt;
            return tok;
        }
    }
    /* lafz -> treat as TOK_IDENTIFIER but keep the name */
    tok.type = TOK_IDENTIFIER;
    return tok;
}

static Token lexer_next_token(Lexer *lx) {
    Token tok;
    lexer_skip_ws(lx);
    tok.line   = lx->line;
    tok.column = lx->column;

    if (lx->current_char == '\0') {
        tok.type = TOK_EOF;
        tok.value[0] = '\0';
        return tok;
    }

    if (lx->current_char == '"') return read_string(lx);
    if (isdigit((unsigned char)lx->current_char)) return read_number(lx);
    if (isalpha((unsigned char)lx->current_char) ||
        lx->current_char == '_') {
        /* _ alone is discard */
        if (lx->current_char == '_' &&
            !isalnum((unsigned char)lx->source[lx->pos + 1]) &&
            lx->source[lx->pos + 1] != '_') {
            lexer_advance(lx);
            tok.type = TOK_UNDERSCORE;
            strcpy(tok.value, "_");
            return tok;
        }
        return read_identifier(lx);
    }

    /* Multi-char operators - order matters */
    char c  = lx->current_char;
    char c2 = lx->source[lx->pos + 1];

    /* -= */
    if (c == '-' && c2 == '=') {
        lexer_advance(lx); lexer_advance(lx);
        tok.type = TOK_MINUS_ASSIGN; strcpy(tok.value, "-="); return tok;
    }
    /* -> */
    if (c == '-' && c2 == '>') {
        lexer_advance(lx); lexer_advance(lx);
        tok.type = TOK_ARROW; strcpy(tok.value, "->"); return tok;
    }
    /* += */
    if (c == '+' && c2 == '=') {
        lexer_advance(lx); lexer_advance(lx);
        tok.type = TOK_PLUS_ASSIGN; strcpy(tok.value, "+="); return tok;
    }
    /* *= */
    if (c == '*' && c2 == '=') {
        lexer_advance(lx); lexer_advance(lx);
        tok.type = TOK_STAR_ASSIGN; strcpy(tok.value, "*="); return tok;
    }
    /* /= */
    if (c == '/' && c2 == '=') {
        lexer_advance(lx); lexer_advance(lx);
        tok.type = TOK_SLASH_ASSIGN; strcpy(tok.value, "/="); return tok;
    }
    /* %= */
    if (c == '%' && c2 == '=') {
        lexer_advance(lx); lexer_advance(lx);
        tok.type = TOK_PERCENT_ASSIGN; strcpy(tok.value, "%="); return tok;
    }
    /* &= */
    if (c == '&' && c2 == '=') {
        lexer_advance(lx); lexer_advance(lx);
        tok.type = TOK_AMP_ASSIGN; strcpy(tok.value, "&="); return tok;
    }
    /* |= */
    if (c == '|' && c2 == '=') {
        lexer_advance(lx); lexer_advance(lx);
        tok.type = TOK_PIPE_ASSIGN; strcpy(tok.value, "|="); return tok;
    }
    /* ^= */
    if (c == '^' && c2 == '=') {
        lexer_advance(lx); lexer_advance(lx);
        tok.type = TOK_CARET_ASSIGN; strcpy(tok.value, "^="); return tok;
    }
    /* <<= */
    if (c == '<' && c2 == '<' && lx->source[lx->pos + 2] == '=') {
        lexer_advance(lx); lexer_advance(lx); lexer_advance(lx);
        tok.type = TOK_SHL_ASSIGN; strcpy(tok.value, "<<="); return tok;
    }
    /* >>= */
    if (c == '>' && c2 == '>' && lx->source[lx->pos + 2] == '=') {
        lexer_advance(lx); lexer_advance(lx); lexer_advance(lx);
        tok.type = TOK_SHR_ASSIGN; strcpy(tok.value, ">>="); return tok;
    }
    /* ..= */
    if (c == '.' && c2 == '.') {
        lexer_advance(lx); lexer_advance(lx);
        if (lx->current_char == '=') {
            lexer_advance(lx);
            tok.type = TOK_DOTDOTEQ; strcpy(tok.value, "..="); return tok;
        }
        tok.type = TOK_DOTDOT; strcpy(tok.value, ".."); return tok;
    }
    /* == */
    if (c == '=' && c2 == '=') {
        lexer_advance(lx); lexer_advance(lx);
        tok.type = TOK_EQ; strcpy(tok.value, "=="); return tok;
    }
    /* != */
    if (c == '!' && c2 == '=') {
        lexer_advance(lx); lexer_advance(lx);
        tok.type = TOK_NE; strcpy(tok.value, "!="); return tok;
    }
    /* <= */
    if (c == '<' && c2 == '=') {
        lexer_advance(lx); lexer_advance(lx);
        tok.type = TOK_LE; strcpy(tok.value, "<="); return tok;
    }
    /* >= */
    if (c == '>' && c2 == '=') {
        lexer_advance(lx); lexer_advance(lx);
        tok.type = TOK_GE; strcpy(tok.value, ">="); return tok;
    }
    /* << */
    if (c == '<' && c2 == '<') {
        lexer_advance(lx); lexer_advance(lx);
        tok.type = TOK_LSHIFT; strcpy(tok.value, "<<"); return tok;
    }
    /* >> */
    if (c == '>' && c2 == '>') {
        lexer_advance(lx); lexer_advance(lx);
        tok.type = TOK_RSHIFT; strcpy(tok.value, ">>"); return tok;
    }
    /* && */
    if (c == '&' && c2 == '&') {
        lexer_advance(lx); lexer_advance(lx);
        tok.type = TOK_DAMP; strcpy(tok.value, "&&"); return tok;
    }
    /* || */
    if (c == '|' && c2 == '|') {
        lexer_advance(lx); lexer_advance(lx);
        tok.type = TOK_DPIPE; strcpy(tok.value, "||"); return tok;
    }

    /* Single-char */
    tok.value[0] = c;
    tok.value[1] = '\0';
    switch (c) {
        case '+': tok.type = TOK_PLUS;      break;
        case '-': tok.type = TOK_MINUS;     break;
        case '*': tok.type = TOK_STAR;      break;
        case '/': tok.type = TOK_SLASH;     break;
        case '%': tok.type = TOK_PERCENT;   break;
        case '=': tok.type = TOK_ASSIGN;    break;
        case '<': tok.type = TOK_LT;        break;
        case '>': tok.type = TOK_GT;        break;
        case '&': tok.type = TOK_AMP;       break;
        case '|': tok.type = TOK_PIPE;      break;
        case '^': tok.type = TOK_CARET;     break;
        case '~': tok.type = TOK_TILDE;     break;
        case '!': tok.type = TOK_BANG;      break;
        case '(': tok.type = TOK_LPAREN;    break;
        case ')': tok.type = TOK_RPAREN;    break;
        case '{': tok.type = TOK_LBRACE;    break;
        case '}': tok.type = TOK_RBRACE;    break;
        case '[': tok.type = TOK_LBRACKET;  break;
        case ']': tok.type = TOK_RBRACKET;  break;
        case ';': tok.type = TOK_SEMICOLON; break;
        case ':': tok.type = TOK_COLON;     break;
        case ',': tok.type = TOK_COMMA;     break;
        case '.': tok.type = TOK_DOT;       break;
        case '?': tok.type = TOK_QUESTION;  break;
        case '@': tok.type = TOK_AT;        break;
        default:
            tok.type = TOK_ERROR;
            snprintf(tok.value, MAX_TOKEN_LEN,
                     "unexpected character '%c' (0x%02x)", c, (unsigned char)c);
            return tok;
    }
    lexer_advance(lx);
    return tok;
}

int lexer_tokenize(Lexer *lx, Token *tokens, int max) {
    int count = 0;
    while (count < max) {
        Token tok = lexer_next_token(lx);
        if (tok.type == TOK_ERROR)
            error_at(tok.line, tok.column, "%s", tok.value);
        tokens[count++] = tok;
        if (tok.type == TOK_EOF) break;
    }
    return count;
}

/* -- token_type_name ----------------------------------------------- */
const char* token_type_name(VelTokenType t) {
    switch (t) {
        case TOK_KAR:          return "kar";
        case TOK_CHU:          return "chu";
        case TOK_ATH:          return "ath";
        case TOK_MUT:          return "mut";
        case TOK_AGAR:         return "agar";
        case TOK_NATE:         return "nate";
        case TOK_YELI:         return "yeli";
        case TOK_BAR:          return "bar";
        case TOK_IN:           return "in";
        case TOK_BY:           return "by";
        case TOK_STEP:         return "step";
        case TOK_BREAK:        return "break";
        case TOK_CONTINUE:     return "continue";
        case TOK_ANAW:         return "anaw";
        case TOK_BINA:         return "bina";
        case TOK_TRY:          return "koshish";
        case TOK_CATCH:        return "ratt";
        case TOK_PANIC:        return "panic";
        case TOK_THROW:        return "trayith";
        case TOK_APPEND:       return "append";
        case TOK_ADAD:         return "adad";
        case TOK_UADAD:        return "uadad";
        case TOK_UADAD8:       return "uadad8";
        case TOK_ASHARI:       return "ashari";
        case TOK_ASHARI32:     return "ashari32";
        case TOK_BOOL:         return "bool";
        case TOK_VOID:         return "khali";
        case TOK_POZ:          return "poz";
        case TOK_APUZ:         return "apuz";
        case TOK_NULL:         return "null";
        case TOK_INTEGER:      return "integer";
        case TOK_FLOAT:        return "float";
        case TOK_STRING:       return "string";
        case TOK_HEX:          return "hex_integer";
        case TOK_BINARY:       return "binary_integer";
        case TOK_OCTAL:        return "octal_integer";
        case TOK_IDENTIFIER:   return "identifier";
        case TOK_PLUS:         return "+";
        case TOK_MINUS:        return "-";
        case TOK_STAR:         return "*";
        case TOK_SLASH:        return "/";
        case TOK_PERCENT:      return "%";
        case TOK_AMP:          return "&";
        case TOK_PIPE:         return "|";
        case TOK_DPIPE:        return "||";
        case TOK_CARET:        return "^";
        case TOK_TILDE:        return "~";
        case TOK_LSHIFT:       return "<<";
        case TOK_RSHIFT:       return ">>";
        case TOK_DAMP:         return "&&";
        case TOK_BANG:         return "!";
        case TOK_ASSIGN:       return "=";
        case TOK_EQ:           return "==";
        case TOK_NE:           return "!=";
        case TOK_LT:           return "<";
        case TOK_LE:           return "<=";
        case TOK_GT:           return ">";
        case TOK_GE:           return ">=";
        case TOK_PLUS_ASSIGN:    return "+=";
        case TOK_MINUS_ASSIGN:   return "-=";
        case TOK_STAR_ASSIGN:    return "*=";
        case TOK_SLASH_ASSIGN:   return "/=";
        case TOK_PERCENT_ASSIGN: return "%=";
        case TOK_AMP_ASSIGN:     return "&=";
        case TOK_PIPE_ASSIGN:    return "|=";
        case TOK_CARET_ASSIGN:   return "^=";
        case TOK_SHL_ASSIGN:     return "<<=";
        case TOK_SHR_ASSIGN:     return ">>=";
        case TOK_LPAREN:       return "(";
        case TOK_RPAREN:       return ")";
        case TOK_LBRACE:       return "{";
        case TOK_RBRACE:       return "}";
        case TOK_LBRACKET:     return "[";
        case TOK_RBRACKET:     return "]";
        case TOK_SEMICOLON:    return ";";
        case TOK_COLON:        return ":";
        case TOK_COMMA:        return ",";
        case TOK_ARROW:        return "->";
        case TOK_DOT:          return ".";
        case TOK_DOTDOT:       return "..";
        case TOK_DOTDOTEQ:     return "..=";
        case TOK_QUESTION:     return "?";
        case TOK_AT:           return "@";
        case TOK_UNDERSCORE:   return "_";
        case TOK_EOF:          return "EOF";
        case TOK_ERROR:        return "ERROR";
        default:               return "UNKNOWN";
    }
}
