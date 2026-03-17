/*
 * Velocity Compiler - Parser (Kashmiri Edition v2)
 * Builds Abstract Syntax Trees from tokens
 * Supports: anaw (import), module.function() calls
 */

#include "velocity.h"

static ValueType parse_type_token(Token tok) {
    switch (tok.type) {
        case TOK_ADAD: return TYPE_INT;
        case TOK_BOOL: return TYPE_BOOL;
        case TOK_ASHARI: return TYPE_F64;
        case TOK_ASHARI32: return TYPE_F32;
        case TOK_IDENTIFIER:
            if (strcmp(tok.value, "String") == 0 || strcmp(tok.value, "string") == 0)
                return TYPE_STRING;
            if (strcmp(tok.value, "Lafz") == 0 || strcmp(tok.value, "lafz") == 0)
                return TYPE_STRING;
            break;
        default:
            error_at(tok.line, tok.column,
                     "expected type, got '%s' ('%s')",
                     token_type_name(tok.type), tok.value);
            return TYPE_INT;
    }
    error_at(tok.line, tok.column,
             "unknown type '%s'",
             tok.value);
    return TYPE_INT;
}

static ValueType parse_type(Parser *parser, TypeInfo **out_info) {
    if (out_info) *out_info = NULL;
    Token tok = parser_current(parser);

    if (tok.type == TOK_LBRACKET) {
        parser_advance(parser);
        Token etok = parser_current(parser);
        parser_advance(parser);
        ValueType elem = parse_type_token(etok);
        TypeInfo *ti = (TypeInfo*)calloc(1, sizeof(TypeInfo));
        if (!ti) error("Out of memory");
        ti->kind = TYPE_ARRAY;
        ti->elem_type = elem;
        if (parser_match(parser, TOK_RBRACKET)) {
            parser_advance(parser);
            ti->array_len = -1; /* dynamic array */
        } else {
            parser_expect(parser, TOK_SEMICOLON);
            Token len_tok = parser_expect(parser, TOK_INTEGER);
            parser_expect(parser, TOK_RBRACKET);
            ti->array_len = atoi(len_tok.value);
            if (ti->array_len <= 0)
                error_at(len_tok.line, len_tok.column, "array length must be > 0");
        }
        if (out_info) *out_info = ti;
        return TYPE_ARRAY;
    }

    if (tok.type == TOK_LPAREN) {
        parser_advance(parser);
        ValueType *types = malloc(sizeof(ValueType) * 16);
        int count = 0;

        Token t = parser_current(parser);
        parser_advance(parser);
        types[count++] = parse_type_token(t);

        while (parser_match(parser, TOK_COMMA)) {
            parser_advance(parser);
            t = parser_current(parser);
            parser_advance(parser);
            types[count++] = parse_type_token(t);
        }
        parser_expect(parser, TOK_RPAREN);
        if (count < 2)
            error_at(tok.line, tok.column, "tuple types require at least 2 elements");

        TypeInfo *ti = (TypeInfo*)calloc(1, sizeof(TypeInfo));
        if (!ti) error("Out of memory");
        ti->kind = TYPE_TUPLE;
        ti->tuple_types = types;
        ti->tuple_count = count;
        if (out_info) *out_info = ti;
        return TYPE_TUPLE;
    }

    parser_advance(parser);
    ValueType base = parse_type_token(tok);
    if (parser_match(parser, TOK_QUESTION)) {
        parser_advance(parser);
        if (base == TYPE_STRING)
            return TYPE_OPT_STRING;
        error_at(tok.line, tok.column,
                 "optional type only supported for lafz right now");
    }
    return base;
}

static ASTNode* parse_for_header_stmt(Parser *parser) {
    if (parser_match(parser, TOK_ATH)) {
        parser_advance(parser);
        bool is_mut = false;
        if (parser_match(parser, TOK_MUT)) {
            is_mut = true;
            parser_advance(parser);
        }
        Token name = parser_expect(parser, TOK_IDENTIFIER);
        bool has_type = false;
        ValueType vtype = TYPE_INT;
        TypeInfo *tinfo = NULL;
        if (parser_match(parser, TOK_COLON)) {
            parser_advance(parser);
            vtype = parse_type(parser, &tinfo);
            has_type = true;
        }
        parser_expect(parser, TOK_ASSIGN);
        ASTNode *val = parse_expression(parser);
        ASTNode *n = ast_node_new(AST_LET);
        strncpy(n->data.let.var_name, name.value, MAX_TOKEN_LEN - 1);
        n->data.let.value = val;
        n->data.let.has_type = has_type;
        n->data.let.var_type = vtype;
        n->data.let.is_mut = is_mut;
        n->data.let.type_info = tinfo;
        return n;
    }

    if (parser_match(parser, TOK_IDENTIFIER)) {
        int saved_pos = parser->pos;
        Token name = parser_current(parser);
        parser_advance(parser);

        if (parser_match(parser, TOK_LBRACKET)) {
            parser_advance(parser);
            ASTNode *idx = parse_expression(parser);
            parser_expect(parser, TOK_RBRACKET);
            if (parser_match(parser, TOK_ASSIGN)) {
                parser_advance(parser);
                ASTNode *val = parse_expression(parser);
                ASTNode *n = ast_node_new(AST_ARRAY_ASSIGN);
                strncpy(n->data.array_assign.var_name, name.value, MAX_TOKEN_LEN - 1);
                n->data.array_assign.index = idx;
                n->data.array_assign.value = val;
                return n;
            }
            parser->pos = saved_pos;
        }

        if (parser_match(parser, TOK_ASSIGN)) {
            parser_advance(parser);
            ASTNode *val = parse_expression(parser);
            ASTNode *n = ast_node_new(AST_ASSIGN);
            strncpy(n->data.assign.var_name, name.value, MAX_TOKEN_LEN - 1);
            n->data.assign.value = val;
            return n;
        }

        parser->pos = saved_pos;
    }

    return parse_expression(parser);
}

static ASTNode* parse_for_iterable(Parser *parser) {
    ASTNode *start = parse_expression(parser);
    if (parser_match(parser, TOK_DOTDOT) || parser_match(parser, TOK_DOTDOTEQ)) {
        bool inclusive = parser_match(parser, TOK_DOTDOTEQ);
        parser_advance(parser);
        ASTNode *end = parse_expression(parser);
        ASTNode *n = ast_node_new(AST_RANGE);
        n->data.range.start = start;
        n->data.range.end = end;
        n->data.range.inclusive = inclusive;
        n->data.range.step = NULL;
        if (parser_match(parser, TOK_BY) || parser_match(parser, TOK_STEP)) {
            parser_advance(parser);
            n->data.range.step = parse_expression(parser);
        }
        return n;
    }
    return start;
}

ASTNode* ast_node_new(ASTNodeType type) {
    ASTNode *node = (ASTNode*)calloc(1, sizeof(ASTNode));
    if (!node) error("Out of memory");
    node->type = type;
    return node;
}

void ast_node_free(ASTNode *node) {
    if (!node) return;
    switch (node->type) {
        case AST_BINARY_OP:
            ast_node_free(node->data.binary.left);
            ast_node_free(node->data.binary.right);
            break;
        case AST_CALL:
            for (int i = 0; i < node->data.call.arg_count; i++)
                ast_node_free(node->data.call.args[i]);
            free(node->data.call.args);
            break;
        case AST_ARRAY_LITERAL:
            for (int i = 0; i < node->data.array_lit.count; i++)
                ast_node_free(node->data.array_lit.elements[i]);
            free(node->data.array_lit.elements);
            break;
        case AST_ARRAY_ACCESS:
            ast_node_free(node->data.array_access.array);
            ast_node_free(node->data.array_access.index);
            break;
        case AST_STRING:
            free(node->data.string_lit.value);
            break;
        case AST_TUPLE_LITERAL:
            for (int i = 0; i < node->data.tuple_lit.count; i++)
                ast_node_free(node->data.tuple_lit.elements[i]);
            free(node->data.tuple_lit.elements);
            break;
        case AST_TUPLE_ACCESS:
            ast_node_free(node->data.tuple_access.tuple);
            break;
        case AST_MODULE_CALL:
            for (int i = 0; i < node->data.module_call.arg_count; i++)
                ast_node_free(node->data.module_call.args[i]);
            free(node->data.module_call.args);
            break;
        case AST_LET:
            ast_node_free(node->data.let.value);
            if (node->data.let.type_info) {
                if (node->data.let.type_info->kind == TYPE_TUPLE)
                    free(node->data.let.type_info->tuple_types);
                free(node->data.let.type_info);
            }
            break;
        case AST_ASSIGN: ast_node_free(node->data.assign.value); break;
        case AST_ARRAY_ASSIGN:
            ast_node_free(node->data.array_assign.index);
            ast_node_free(node->data.array_assign.value);
            break;
        case AST_RETURN: ast_node_free(node->data.ret.value); break;
        case AST_IF:
            ast_node_free(node->data.if_stmt.condition);
            for (int i = 0; i < node->data.if_stmt.then_count; i++)
                ast_node_free(node->data.if_stmt.then_stmts[i]);
            free(node->data.if_stmt.then_stmts);
            for (int i = 0; i < node->data.if_stmt.else_count; i++)
                ast_node_free(node->data.if_stmt.else_stmts[i]);
            if (node->data.if_stmt.else_stmts)
                free(node->data.if_stmt.else_stmts);
            break;
        case AST_WHILE:
            ast_node_free(node->data.while_stmt.condition);
            for (int i = 0; i < node->data.while_stmt.body_count; i++)
                ast_node_free(node->data.while_stmt.body[i]);
            free(node->data.while_stmt.body);
            break;
        case AST_FOR:
            if (node->data.for_stmt.init) ast_node_free(node->data.for_stmt.init);
            if (node->data.for_stmt.condition) ast_node_free(node->data.for_stmt.condition);
            if (node->data.for_stmt.post) ast_node_free(node->data.for_stmt.post);
            for (int i = 0; i < node->data.for_stmt.body_count; i++)
                ast_node_free(node->data.for_stmt.body[i]);
            free(node->data.for_stmt.body);
            break;
        case AST_FOR_IN:
            if (node->data.for_in_stmt.iterable)
                ast_node_free(node->data.for_in_stmt.iterable);
            for (int i = 0; i < node->data.for_in_stmt.body_count; i++)
                ast_node_free(node->data.for_in_stmt.body[i]);
            free(node->data.for_in_stmt.body);
            break;
        case AST_RANGE:
            ast_node_free(node->data.range.start);
            ast_node_free(node->data.range.end);
            if (node->data.range.step)
                ast_node_free(node->data.range.step);
            break;
        case AST_FUNCTION:
            for (int i = 0; i < node->data.function.param_count; i++)
                free(node->data.function.param_names[i]);
            free(node->data.function.param_names);
            free(node->data.function.param_types);
            if (node->data.function.param_typeinfo) {
                for (int i = 0; i < node->data.function.param_count; i++) {
                    if (node->data.function.param_typeinfo[i]) {
                        if (node->data.function.param_typeinfo[i]->kind == TYPE_TUPLE)
                            free(node->data.function.param_typeinfo[i]->tuple_types);
                        free(node->data.function.param_typeinfo[i]);
                    }
                }
                free(node->data.function.param_typeinfo);
            }
            if (node->data.function.return_typeinfo) {
                if (node->data.function.return_typeinfo->kind == TYPE_TUPLE)
                    free(node->data.function.return_typeinfo->tuple_types);
                free(node->data.function.return_typeinfo);
            }
            for (int i = 0; i < node->data.function.body_count; i++)
                ast_node_free(node->data.function.body[i]);
            free(node->data.function.body);
            break;
        case AST_PROGRAM:
            for (int i = 0; i < node->data.program.import_count; i++)
                ast_node_free(node->data.program.imports[i]);
            free(node->data.program.imports);
            for (int i = 0; i < node->data.program.function_count; i++)
                ast_node_free(node->data.program.functions[i]);
            free(node->data.program.functions);
            break;
        default: break;
    }
    free(node);
}

void parser_init(Parser *parser, Token *tokens, int token_count) {
    parser->tokens = tokens;
    parser->token_count = token_count;
    parser->pos = 0;
}

Token parser_current(Parser *parser) {
    if (parser->pos >= parser->token_count) {
        Token eof; eof.type = TOK_EOF; eof.value[0] = '\0';
        return eof;
    }
    return parser->tokens[parser->pos];
}

static Token parser_peek(Parser *parser, int offset) {
    int pos = parser->pos + offset;
    if (pos >= parser->token_count || pos < 0) {
        Token eof; eof.type = TOK_EOF; eof.value[0] = '\0';
        return eof;
    }
    return parser->tokens[pos];
}

void parser_advance(Parser *parser) {
    if (parser->pos < parser->token_count) parser->pos++;
}

bool parser_match(Parser *parser, TokenType type) {
    return parser_current(parser).type == type;
}

Token parser_expect(Parser *parser, TokenType type) {
    Token current = parser_current(parser);
    if (current.type != type) {
        error_at(current.line, current.column,
                 "expected '%s', got '%s' ('%s')",
                 token_type_name(type),
                 token_type_name(current.type),
                 current.value);
    }
    parser_advance(parser);
    return current;
}

/* ── Expressions ── */

ASTNode* parse_primary(Parser *parser) {
    Token cur = parser_current(parser);

    /* Integer literal */
    if (cur.type == TOK_INTEGER) {
        ASTNode *n = ast_node_new(AST_INTEGER);
        n->data.int_value = atoi(cur.value);
        parser_advance(parser);
        return n;
    }

    /* Bool literal */
    if (cur.type == TOK_POZ || cur.type == TOK_APUZ) {
        ASTNode *n = ast_node_new(AST_BOOL);
        n->data.bool_value = (cur.type == TOK_POZ);
        parser_advance(parser);
        return n;
    }

    /* Null literal */
    if (cur.type == TOK_NULL) {
        ASTNode *n = ast_node_new(AST_NULL);
        parser_advance(parser);
        return n;
    }

    /* Float literal */
    if (cur.type == TOK_FLOAT) {
        ASTNode *n = ast_node_new(AST_FLOAT);
        n->data.float_lit.value = strtod(cur.value, NULL);
        n->data.float_lit.type = TYPE_F64;
        parser_advance(parser);
        return n;
    }

    /* String literal */
    if (cur.type == TOK_STRING) {
        ASTNode *n = ast_node_new(AST_STRING);
        n->data.string_lit.value = strdup(cur.value);
        n->data.string_lit.length = (int)strlen(cur.value);
        parser_advance(parser);
        return n;
    }

    /* Array literal */
    if (cur.type == TOK_LBRACKET) {
        parser_advance(parser);
        ASTNode *n = ast_node_new(AST_ARRAY_LITERAL);
        n->data.array_lit.elements = malloc(sizeof(ASTNode*) * 64);
        n->data.array_lit.count = 0;

        if (!parser_match(parser, TOK_RBRACKET)) {
            n->data.array_lit.elements[n->data.array_lit.count++] = parse_expression(parser);
            while (parser_match(parser, TOK_COMMA)) {
                parser_advance(parser);
                n->data.array_lit.elements[n->data.array_lit.count++] = parse_expression(parser);
            }
        }
        parser_expect(parser, TOK_RBRACKET);
        return n;
    }

    /* Tuple literal or parenthesised expression */
    if (cur.type == TOK_LPAREN) {
        parser_advance(parser);
        ASTNode *first = parse_expression(parser);
        if (parser_match(parser, TOK_COMMA)) {
            ASTNode *n = ast_node_new(AST_TUPLE_LITERAL);
            n->data.tuple_lit.elements = malloc(sizeof(ASTNode*) * 32);
            n->data.tuple_lit.count = 0;
            n->data.tuple_lit.elements[n->data.tuple_lit.count++] = first;
            while (parser_match(parser, TOK_COMMA)) {
                parser_advance(parser);
                n->data.tuple_lit.elements[n->data.tuple_lit.count++] = parse_expression(parser);
            }
            parser_expect(parser, TOK_RPAREN);
            if (n->data.tuple_lit.count < 2)
                error_at(cur.line, cur.column, "tuple literals require at least 2 elements");
            return n;
        }
        parser_expect(parser, TOK_RPAREN);
        return first;
    }

    /* Identifier, function call, or module.function() */
    if (cur.type == TOK_IDENTIFIER) {
        char name[MAX_TOKEN_LEN];
        strncpy(name, cur.value, MAX_TOKEN_LEN - 1);
        parser_advance(parser);
        ASTNode *node = NULL;

        /* module.function(...) */
        if (parser_match(parser, TOK_DOT)) {
            Token next = parser_peek(parser, 1);
            if (next.type == TOK_IDENTIFIER) {
                parser_advance(parser); /* consume '.' */
                Token func_tok = parser_expect(parser, TOK_IDENTIFIER);
                parser_expect(parser, TOK_LPAREN);

                ASTNode *n = ast_node_new(AST_MODULE_CALL);
                strncpy(n->data.module_call.module_name, name, MAX_TOKEN_LEN - 1);
                strncpy(n->data.module_call.func_name, func_tok.value, MAX_TOKEN_LEN - 1);
                n->data.module_call.args = malloc(sizeof(ASTNode*) * 16);
                n->data.module_call.arg_count = 0;

                if (!parser_match(parser, TOK_RPAREN)) {
                    n->data.module_call.args[n->data.module_call.arg_count++] = parse_expression(parser);
                    while (parser_match(parser, TOK_COMMA)) {
                        parser_advance(parser);
                        n->data.module_call.args[n->data.module_call.arg_count++] = parse_expression(parser);
                    }
                }
                parser_expect(parser, TOK_RPAREN);
                node = n;
            }
        }

        /* plain function call */
        if (!node && parser_match(parser, TOK_LPAREN)) {
            parser_advance(parser);
            ASTNode *n = ast_node_new(AST_CALL);
            strncpy(n->data.call.func_name, name, MAX_TOKEN_LEN - 1);
            n->data.call.args = malloc(sizeof(ASTNode*) * 16);
            n->data.call.arg_count = 0;

            if (!parser_match(parser, TOK_RPAREN)) {
                n->data.call.args[n->data.call.arg_count++] = parse_expression(parser);
                while (parser_match(parser, TOK_COMMA)) {
                    parser_advance(parser);
                    n->data.call.args[n->data.call.arg_count++] = parse_expression(parser);
                }
            }
            parser_expect(parser, TOK_RPAREN);
            node = n;
        }

        if (!node) {
            node = ast_node_new(AST_IDENTIFIER);
            strncpy(node->data.identifier, name, MAX_TOKEN_LEN - 1);
        }

        /* postfix: array access and tuple access */
        while (1) {
            if (parser_match(parser, TOK_LBRACKET)) {
                parser_advance(parser);
                ASTNode *idx = parse_expression(parser);
                parser_expect(parser, TOK_RBRACKET);
                ASTNode *acc = ast_node_new(AST_ARRAY_ACCESS);
                acc->data.array_access.array = node;
                acc->data.array_access.index = idx;
                node = acc;
                continue;
            }
            if (parser_match(parser, TOK_DOT)) {
                Token peek = parser_peek(parser, 1);
                if (peek.type == TOK_INTEGER) {
                    parser_advance(parser);
                    Token idx_tok = parser_expect(parser, TOK_INTEGER);
                    ASTNode *acc = ast_node_new(AST_TUPLE_ACCESS);
                    acc->data.tuple_access.tuple = node;
                    acc->data.tuple_access.index = atoi(idx_tok.value);
                    node = acc;
                    continue;
                }
            }
            break;
        }

        return node;
    }

    error_at(cur.line, cur.column,
             "unexpected token '%s' ('%s')",
             token_type_name(cur.type),
             cur.value);
    return NULL;
}

ASTNode* parse_unary(Parser *parser) {
    if (parser_match(parser, TOK_MINUS)) {
        parser_advance(parser);
        ASTNode *operand = parse_unary(parser);
        ASTNode *zero = ast_node_new(AST_INTEGER);
        zero->data.int_value = 0;
        ASTNode *n = ast_node_new(AST_BINARY_OP);
        n->data.binary.op = OP_SUB;
        n->data.binary.left  = zero;
        n->data.binary.right = operand;
        return n;
    }
    return parse_primary(parser);
}

ASTNode* parse_multiplicative(Parser *parser) {
    ASTNode *left = parse_unary(parser);
    while (parser_match(parser, TOK_STAR) ||
           parser_match(parser, TOK_SLASH) ||
           parser_match(parser, TOK_PERCENT)) {
        Token op = parser_current(parser); parser_advance(parser);
        ASTNode *right = parse_unary(parser);
        ASTNode *n = ast_node_new(AST_BINARY_OP);
        n->data.binary.op    = (op.type == TOK_STAR) ? OP_MUL :
                               (op.type == TOK_SLASH) ? OP_DIV : OP_MOD;
        n->data.binary.left  = left;
        n->data.binary.right = right;
        left = n;
    }
    return left;
}

ASTNode* parse_additive(Parser *parser) {
    ASTNode *left = parse_multiplicative(parser);
    while (parser_match(parser, TOK_PLUS) || parser_match(parser, TOK_MINUS)) {
        Token op = parser_current(parser); parser_advance(parser);
        ASTNode *right = parse_multiplicative(parser);
        ASTNode *n = ast_node_new(AST_BINARY_OP);
        n->data.binary.op    = (op.type == TOK_PLUS) ? OP_ADD : OP_SUB;
        n->data.binary.left  = left;
        n->data.binary.right = right;
        left = n;
    }
    return left;
}

ASTNode* parse_comparison(Parser *parser) {
    ASTNode *left = parse_additive(parser);
    while (parser_match(parser, TOK_LT)  || parser_match(parser, TOK_LE) ||
           parser_match(parser, TOK_GT)  || parser_match(parser, TOK_GE) ||
           parser_match(parser, TOK_EQ)  || parser_match(parser, TOK_NE)) {
        Token op = parser_current(parser); parser_advance(parser);
        ASTNode *right = parse_additive(parser);
        ASTNode *n = ast_node_new(AST_BINARY_OP);
        switch (op.type) {
            case TOK_LT: n->data.binary.op = OP_LT; break;
            case TOK_LE: n->data.binary.op = OP_LE; break;
            case TOK_GT: n->data.binary.op = OP_GT; break;
            case TOK_GE: n->data.binary.op = OP_GE; break;
            case TOK_EQ: n->data.binary.op = OP_EQ; break;
            case TOK_NE: n->data.binary.op = OP_NE; break;
            default: break;
        }
        n->data.binary.left  = left;
        n->data.binary.right = right;
        left = n;
    }
    return left;
}

ASTNode* parse_expression(Parser *parser) {
    return parse_comparison(parser);
}

/* ── Statements ── */

ASTNode* parse_statement(Parser *parser) {

    /* chu — return */
    if (parser_match(parser, TOK_CHU)) {
        parser_advance(parser);
        ASTNode *n = ast_node_new(AST_RETURN);
        n->data.ret.value = parser_match(parser, TOK_SEMICOLON)
                            ? NULL : parse_expression(parser);
        parser_expect(parser, TOK_SEMICOLON);
        return n;
    }

    /* ath — let */
    if (parser_match(parser, TOK_ATH)) {
        parser_advance(parser);
        bool is_mut = false;
        if (parser_match(parser, TOK_MUT)) {
            is_mut = true;
            parser_advance(parser);
        }
        Token name = parser_expect(parser, TOK_IDENTIFIER);
        bool has_type = false;
        ValueType vtype = TYPE_INT;
        TypeInfo *tinfo = NULL;
        if (parser_match(parser, TOK_COLON)) {
            parser_advance(parser);
            vtype = parse_type(parser, &tinfo);
            has_type = true;
        }
        parser_expect(parser, TOK_ASSIGN);
        ASTNode *val = parse_expression(parser);
        parser_expect(parser, TOK_SEMICOLON);
        ASTNode *n = ast_node_new(AST_LET);
        strncpy(n->data.let.var_name, name.value, MAX_TOKEN_LEN - 1);
        n->data.let.value = val;
        n->data.let.has_type = has_type;
        n->data.let.var_type = vtype;
        n->data.let.is_mut = is_mut;
        n->data.let.type_info = tinfo;
        return n;
    }

    /* agar — if */
    if (parser_match(parser, TOK_AGAR)) {
        parser_advance(parser);
        ASTNode *cond = parse_expression(parser);
        parser_expect(parser, TOK_LBRACE);

        ASTNode **then_s = malloc(sizeof(ASTNode*) * 256);
        int then_c = 0;
        while (!parser_match(parser, TOK_RBRACE))
            then_s[then_c++] = parse_statement(parser);
        parser_advance(parser); /* } */

        ASTNode **else_s = NULL;
        int else_c = 0;
        if (parser_match(parser, TOK_NATE)) {
            parser_advance(parser);
            parser_expect(parser, TOK_LBRACE);
            else_s = malloc(sizeof(ASTNode*) * 256);
            while (!parser_match(parser, TOK_RBRACE))
                else_s[else_c++] = parse_statement(parser);
            parser_advance(parser); /* } */
        }

        ASTNode *n = ast_node_new(AST_IF);
        n->data.if_stmt.condition   = cond;
        n->data.if_stmt.then_stmts  = then_s;
        n->data.if_stmt.then_count  = then_c;
        n->data.if_stmt.else_stmts  = else_s;
        n->data.if_stmt.else_count  = else_c;
        return n;
    }

    /* yeli — while */
    if (parser_match(parser, TOK_YELI)) {
        parser_advance(parser);
        ASTNode *cond = parse_expression(parser);
        parser_expect(parser, TOK_LBRACE);

        ASTNode **body = malloc(sizeof(ASTNode*) * 256);
        int body_c = 0;
        while (!parser_match(parser, TOK_RBRACE))
            body[body_c++] = parse_statement(parser);
        parser_advance(parser); /* } */

        ASTNode *n = ast_node_new(AST_WHILE);
        n->data.while_stmt.condition  = cond;
        n->data.while_stmt.body       = body;
        n->data.while_stmt.body_count = body_c;
        return n;
    }

    /* bar — for */
    if (parser_match(parser, TOK_BAR)) {
        parser_advance(parser);

        if (parser_match(parser, TOK_LPAREN)) {
            parser_advance(parser);
            ASTNode *init = NULL;
            ASTNode *cond = NULL;
            ASTNode *post = NULL;

            if (!parser_match(parser, TOK_SEMICOLON))
                init = parse_for_header_stmt(parser);
            parser_expect(parser, TOK_SEMICOLON);

            if (!parser_match(parser, TOK_SEMICOLON))
                cond = parse_expression(parser);
            parser_expect(parser, TOK_SEMICOLON);

            if (!parser_match(parser, TOK_RPAREN))
                post = parse_for_header_stmt(parser);
            parser_expect(parser, TOK_RPAREN);

            parser_expect(parser, TOK_LBRACE);
            ASTNode **body = malloc(sizeof(ASTNode*) * 256);
            int body_c = 0;
            while (!parser_match(parser, TOK_RBRACE))
                body[body_c++] = parse_statement(parser);
            parser_advance(parser);

            ASTNode *n = ast_node_new(AST_FOR);
            n->data.for_stmt.init = init;
            n->data.for_stmt.condition = cond;
            n->data.for_stmt.post = post;
            n->data.for_stmt.body = body;
            n->data.for_stmt.body_count = body_c;
            return n;
        }

        bool is_mut = false;
        if (parser_match(parser, TOK_MUT)) {
            is_mut = true;
            parser_advance(parser);
        }
        Token name = parser_expect(parser, TOK_IDENTIFIER);
        bool has_type = false;
        ValueType vtype = TYPE_INT;
        TypeInfo *tinfo = NULL;
        if (parser_match(parser, TOK_COLON)) {
            parser_advance(parser);
            vtype = parse_type(parser, &tinfo);
            has_type = true;
            if (tinfo && (tinfo->kind == TYPE_ARRAY || tinfo->kind == TYPE_TUPLE)) {
                if (tinfo->kind == TYPE_TUPLE)
                    free(tinfo->tuple_types);
                free(tinfo);
                error_at(name.line, name.column, "for-in variable type must be primitive for now");
            }
            if (tinfo) free(tinfo);
        }
        parser_expect(parser, TOK_IN);
        ASTNode *iter = parse_for_iterable(parser);
        parser_expect(parser, TOK_LBRACE);

        ASTNode **body = malloc(sizeof(ASTNode*) * 256);
        int body_c = 0;
        while (!parser_match(parser, TOK_RBRACE))
            body[body_c++] = parse_statement(parser);
        parser_advance(parser);

        ASTNode *n = ast_node_new(AST_FOR_IN);
        strncpy(n->data.for_in_stmt.var_name, name.value, MAX_TOKEN_LEN - 1);
        n->data.for_in_stmt.is_mut = is_mut;
        n->data.for_in_stmt.has_type = has_type;
        n->data.for_in_stmt.var_type = vtype;
        n->data.for_in_stmt.iterable = iter;
        n->data.for_in_stmt.body = body;
        n->data.for_in_stmt.body_count = body_c;
        return n;
    }

    /* break */
    if (parser_match(parser, TOK_BREAK)) {
        parser_advance(parser);
        parser_expect(parser, TOK_SEMICOLON);
        return ast_node_new(AST_BREAK);
    }

    /* continue */
    if (parser_match(parser, TOK_CONTINUE)) {
        parser_advance(parser);
        parser_expect(parser, TOK_SEMICOLON);
        return ast_node_new(AST_CONTINUE);
    }

    /* identifier — assignment or expression-statement */
    if (parser_match(parser, TOK_IDENTIFIER)) {
        int saved_pos = parser->pos;
        Token name = parser_current(parser);
        parser_advance(parser);

        /* array element assignment: name[expr] = expr; */
        if (parser_match(parser, TOK_LBRACKET)) {
            parser_advance(parser);
            ASTNode *idx = parse_expression(parser);
            parser_expect(parser, TOK_RBRACKET);
            if (parser_match(parser, TOK_ASSIGN)) {
                parser_advance(parser);
                ASTNode *val = parse_expression(parser);
                parser_expect(parser, TOK_SEMICOLON);
                ASTNode *n = ast_node_new(AST_ARRAY_ASSIGN);
                strncpy(n->data.array_assign.var_name, name.value, MAX_TOKEN_LEN - 1);
                n->data.array_assign.index = idx;
                n->data.array_assign.value = val;
                return n;
            }
            /* not an assignment; backtrack */
            parser->pos = saved_pos;
        }

        /* plain assignment: name = expr; */
        if (parser_match(parser, TOK_ASSIGN)) {
            parser_advance(parser);
            ASTNode *val = parse_expression(parser);
            parser_expect(parser, TOK_SEMICOLON);
            ASTNode *n = ast_node_new(AST_ASSIGN);
            strncpy(n->data.assign.var_name, name.value, MAX_TOKEN_LEN - 1);
            n->data.assign.value = val;
            return n;
        }

        /* Not an assignment — backtrack and fall through to expr-stmt */
        parser->pos = saved_pos;
    }

    /* expression statement */
    ASTNode *expr = parse_expression(parser);
    parser_expect(parser, TOK_SEMICOLON);
    return expr;
}

/* ── Import ── */

ASTNode* parse_import(Parser *parser) {
    parser_expect(parser, TOK_ANAW);
    Token mod = parser_expect(parser, TOK_IDENTIFIER);
    parser_expect(parser, TOK_SEMICOLON);

    ASTNode *n = ast_node_new(AST_IMPORT);
    strncpy(n->data.import.module_name, mod.value, MAX_TOKEN_LEN - 1);
    return n;
}

/* ── Function ── */

ASTNode* parse_function(Parser *parser) {
    parser_expect(parser, TOK_KAR);
    Token name = parser_expect(parser, TOK_IDENTIFIER);
    parser_expect(parser, TOK_LPAREN);

    char **param_names = malloc(sizeof(char*) * 16);
    ValueType *param_types = malloc(sizeof(ValueType) * 16);
    TypeInfo **param_typeinfo = malloc(sizeof(TypeInfo*) * 16);
    int param_count = 0;

    if (!parser_match(parser, TOK_RPAREN)) {
        Token pname = parser_expect(parser, TOK_IDENTIFIER);
        param_names[param_count] = strdup(pname.value);
        param_count++;
        parser_expect(parser, TOK_COLON);
        TypeInfo *ptinfo = NULL;
        param_types[param_count - 1] = parse_type(parser, &ptinfo);
        param_typeinfo[param_count - 1] = ptinfo;
        if (param_types[param_count - 1] == TYPE_ARRAY &&
            ptinfo && ptinfo->array_len > 0) {
            error_at(pname.line, pname.column,
                     "fixed-size arrays are not supported in function parameters yet");
        }

        while (parser_match(parser, TOK_COMMA)) {
            parser_advance(parser);
            pname = parser_expect(parser, TOK_IDENTIFIER);
            param_names[param_count] = strdup(pname.value);
            param_count++;
            parser_expect(parser, TOK_COLON);
            ptinfo = NULL;
            param_types[param_count - 1] = parse_type(parser, &ptinfo);
            param_typeinfo[param_count - 1] = ptinfo;
            if (param_types[param_count - 1] == TYPE_ARRAY &&
                ptinfo && ptinfo->array_len > 0) {
                error_at(pname.line, pname.column,
                         "fixed-size arrays are not supported in function parameters yet");
            }
        }
    }
    parser_expect(parser, TOK_RPAREN);
    parser_expect(parser, TOK_ARROW);
    TypeInfo *rtinfo = NULL;
    ValueType ret_type = parse_type(parser, &rtinfo);
    if (ret_type == TYPE_ARRAY && rtinfo && rtinfo->array_len > 0)
        error_at(name.line, name.column,
                 "fixed-size arrays are not supported as return types yet");
    parser_expect(parser, TOK_LBRACE);

    ASTNode **body = malloc(sizeof(ASTNode*) * 256);
    int body_count = 0;
    while (!parser_match(parser, TOK_RBRACE))
        body[body_count++] = parse_statement(parser);
    parser_advance(parser); /* } */

    ASTNode *n = ast_node_new(AST_FUNCTION);
    strncpy(n->data.function.name, name.value, MAX_TOKEN_LEN - 1);
    n->data.function.param_names  = param_names;
    n->data.function.param_types  = param_types;
    n->data.function.param_typeinfo = param_typeinfo;
    n->data.function.param_count  = param_count;
    n->data.function.return_type  = ret_type;
    n->data.function.return_typeinfo = rtinfo;
    n->data.function.body         = body;
    n->data.function.body_count   = body_count;
    n->data.function.is_exported  = true;
    return n;
}

/* ── Program ── */

ASTNode* parse_program(Parser *parser) {
    ASTNode *prog = ast_node_new(AST_PROGRAM);
    prog->data.program.imports         = malloc(sizeof(ASTNode*) * MAX_IMPORTS);
    prog->data.program.import_count    = 0;
    prog->data.program.functions       = malloc(sizeof(ASTNode*) * 256);
    prog->data.program.function_count  = 0;

    while (!parser_match(parser, TOK_EOF)) {
        if (parser_match(parser, TOK_ANAW))
            prog->data.program.imports[prog->data.program.import_count++] =
                parse_import(parser);
        else
            prog->data.program.functions[prog->data.program.function_count++] =
                parse_function(parser);
    }
    return prog;
}
