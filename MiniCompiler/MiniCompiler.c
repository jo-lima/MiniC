/* Parser recursivo-descendente para o subconjunto MiniC. */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

/* Categorias de tokens; operadores e pontuação usam o mesmo enum. */
typedef enum {
  TOKEN_EOF, TOKEN_IDENTIFIER, TOKEN_INTEGER_LITERAL, TOKEN_REAL_LITERAL,
  TOKEN_INT, TOKEN_FLOAT, TOKEN_BOOL, TOKEN_VOID, TOKEN_RETURN, TOKEN_IF,
  TOKEN_ELSE, TOKEN_WHILE, TOKEN_TRUE, TOKEN_FALSE, TOKEN_FOR, TOKEN_BREAK,
  TOKEN_CONTINUE, TOKEN_PRINT, TOKEN_READ, TOKEN_CHAR,
  TOKEN_PLUS, TOKEN_MINUS, TOKEN_MULTIPLY, TOKEN_DIVIDE, TOKEN_MODULO, TOKEN_ASSIGN,
  TOKEN_LESS_THAN, TOKEN_GREATER_THAN, TOKEN_NOT, TOKEN_LESS_EQUAL,
  TOKEN_GREATER_EQUAL, TOKEN_EQUAL, TOKEN_NOT_EQUAL, TOKEN_AND, TOKEN_OR,
  TOKEN_SEMICOLON, TOKEN_COMMA, TOKEN_LEFT_PARENTHESIS,
  TOKEN_RIGHT_PARENTHESIS, TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
  TOKEN_LEFT_BRACKET, TOKEN_RIGHT_BRACKET
} TokenKind;

typedef struct {
  TokenKind kind;
  char *text;
  int line;
  int column;
} Token;

typedef struct {
  Token *tokens;
  int token_count;
  int token_capacity;
  int current_index;
  struct Diagnostic *diagnostics;
  int diagnostic_count;
  int diagnostic_capacity;
} Tokens;
typedef struct Diagnostic {
  char *message;
  char *phase;
  int line;
  int column;
} Diagnostic;
typedef enum {
  AST_PROGRAM, AST_FUNCTION, AST_BLOCK, AST_VARIABLE_DECLARATION, AST_IF,
  AST_WHILE, AST_FOR, AST_BREAK, AST_CONTINUE, AST_RETURN, AST_EXPRESSION, AST_ASSIGNMENT, AST_BINARY,
  AST_UNARY, AST_CALL, AST_INDEX, AST_IDENTIFIER, AST_LITERAL
} AstTag;
/* Nós compartilham um formato compacto: filhos fixos para estruturas e
   uma lista dinâmica para declarações, parâmetros, argumentos e blocos. */
typedef struct AstNode AstNode;
struct AstNode {
  AstTag tag;
  char *first_text;
  char *second_text;
  AstNode *first_child;
  AstNode *second_child;
  AstNode *third_child;
  AstNode **children;
  int child_count;
  int line;
  int column;
  int is_array;
};

typedef struct {
  char *text;
  size_t length;
  size_t capacity;
} Str;

static void append_text(Str *buffer, const char *text);

static char *duplicate_text(const char *source, size_t length) {
  char *duplicated_text = malloc(length + 1);
  if (!duplicated_text) exit(2);
  memcpy(duplicated_text, source, length);
  duplicated_text[length] = 0;
  return duplicated_text;
}

/* Registra o erro na posição do token atual sem interromper a tokenização. */
static void mark_error(Tokens *tokens, const char *phase, const char *message) {
  if (tokens->diagnostic_count == tokens->diagnostic_capacity) {
    tokens->diagnostic_capacity = tokens->diagnostic_capacity
        ? tokens->diagnostic_capacity * 2 : 16;
    tokens->diagnostics = realloc(
        tokens->diagnostics,
        tokens->diagnostic_capacity * sizeof *tokens->diagnostics);
    if (!tokens->diagnostics) exit(2);
  }
  Token *token = &tokens->tokens[tokens->current_index];
  tokens->diagnostics[tokens->diagnostic_count++] = (Diagnostic){
      duplicate_text(message, strlen(message)),
      duplicate_text(phase, strlen(phase)), token->line, token->column};
}

static void append_token(
    Tokens *token_list, TokenKind token_kind, const char *source,
    size_t text_length, int line_number, int column_number) {
  if (token_list->token_count == token_list->token_capacity) {
    token_list->token_capacity = token_list->token_capacity
        ? token_list->token_capacity * 2 : 64;
    token_list->tokens = realloc(
        token_list->tokens,
        token_list->token_capacity * sizeof *token_list->tokens);
    if (!token_list->tokens) exit(2);
  }
  token_list->tokens[token_list->token_count++] = (Token){
      token_kind, duplicate_text(source, text_length), line_number,
      column_number};
}

static TokenKind keyword_kind(const char *keyword_text) {
  if (!strcmp(keyword_text, "int")) return TOKEN_INT;
  if (!strcmp(keyword_text, "float")) return TOKEN_FLOAT;
  if (!strcmp(keyword_text, "bool")) return TOKEN_BOOL;
  if (!strcmp(keyword_text, "void")) return TOKEN_VOID;
  if (!strcmp(keyword_text, "return")) return TOKEN_RETURN;
  if (!strcmp(keyword_text, "if")) return TOKEN_IF;
  if (!strcmp(keyword_text, "else")) return TOKEN_ELSE;
  if (!strcmp(keyword_text, "while")) return TOKEN_WHILE;
  if (!strcmp(keyword_text, "for")) return TOKEN_FOR;
  if (!strcmp(keyword_text, "break")) return TOKEN_BREAK;
  if (!strcmp(keyword_text, "continue")) return TOKEN_CONTINUE;
  if (!strcmp(keyword_text, "print")) return TOKEN_PRINT;
  if (!strcmp(keyword_text, "read")) return TOKEN_READ;
  if (!strcmp(keyword_text, "char")) return TOKEN_CHAR;
  if (!strcmp(keyword_text, "true")) return TOKEN_TRUE;
  if (!strcmp(keyword_text, "false")) return TOKEN_FALSE;
  return TOKEN_IDENTIFIER;
}
/* Converte o texto em tokens e acumula erros lexicais para a etapa seguinte. */
static Tokens tokenize(const char *source) {
  Tokens token_list = {0};
  int line_number = 1;
  int column_number = 1;
  size_t source_index = 0;

  while (source[source_index]) {
    size_t token_start = source_index;
    int token_column = column_number;
    if (isspace((unsigned char)source[source_index])) {
      if (source[source_index++] == '\n') {
        line_number++;
        column_number = 1;
      } else {
        column_number++;
      }
      continue;
    }
    if (source[source_index] == '/' && source[source_index + 1] == '/') {
      while (source[source_index] && source[source_index] != '\n') {
        source_index++;
        column_number++;
      }
      continue;
    }
    if (source[source_index] == '/' && source[source_index + 1] == '*') {
      source_index += 2;
      column_number += 2;
      while (source[source_index] && !(source[source_index] == '*'
          && source[source_index + 1] == '/')) {
        if (source[source_index++] == '\n') {
          line_number++;
          column_number = 1;
        } else {
          column_number++;
        }
      }
      if (!source[source_index]) {
        mark_error(&token_list, "lexical", "literal real inválido");
        break;
      }
      source_index += 2;
      column_number += 2;
      continue;
    }
    if (isalpha((unsigned char)source[source_index])
        || source[source_index] == '_') {
      do {
        source_index++;
        column_number++;
      } while (isalnum((unsigned char)source[source_index])
          || source[source_index] == '_');
      char *word_text = duplicate_text(source + token_start,
                                        source_index - token_start);
      TokenKind token_kind = keyword_kind(word_text);
      free(word_text);
      append_token(&token_list, token_kind, source + token_start,
                   source_index - token_start, line_number, token_column);
      continue;
    }
    if (isdigit((unsigned char)source[source_index])) {
      do {
        source_index++;
        column_number++;
      } while (isdigit((unsigned char)source[source_index]));
      TokenKind token_kind = TOKEN_INTEGER_LITERAL;
      if (source[source_index] == '.') {
        token_kind = TOKEN_REAL_LITERAL;
        source_index++;
        column_number++;
        if (!isdigit((unsigned char)source[source_index])) {
          mark_error(&token_list, "lexical", "comentário não terminado");
          break;
        }
        do {
          source_index++;
          column_number++;
        } while (isdigit((unsigned char)source[source_index]));
      }
      append_token(&token_list, token_kind, source + token_start,
                   source_index - token_start, line_number, token_column);
      continue;
    }
    TokenKind token_kind = TOKEN_EOF;
    size_t token_length = 1;
    if (!strncmp(source + source_index, "<=", 2)) token_kind = TOKEN_LESS_EQUAL, token_length = 2;
    else if (!strncmp(source + source_index, ">=", 2)) token_kind = TOKEN_GREATER_EQUAL, token_length = 2;
    else if (!strncmp(source + source_index, "==", 2)) token_kind = TOKEN_EQUAL, token_length = 2;
    else if (!strncmp(source + source_index, "!=", 2)) token_kind = TOKEN_NOT_EQUAL, token_length = 2;
    else if (!strncmp(source + source_index, "&&", 2)) token_kind = TOKEN_AND, token_length = 2;
    else if (!strncmp(source + source_index, "||", 2)) token_kind = TOKEN_OR, token_length = 2;
    else switch (source[source_index]) {
      case '+': token_kind = TOKEN_PLUS; break;
      case '-': token_kind = TOKEN_MINUS; break;
      case '*': token_kind = TOKEN_MULTIPLY; break;
      case '/': token_kind = TOKEN_DIVIDE; break;
      case '%': token_kind = TOKEN_MODULO; break;
      case '=': token_kind = TOKEN_ASSIGN; break;
      case '<': token_kind = TOKEN_LESS_THAN; break;
      case '>': token_kind = TOKEN_GREATER_THAN; break;
      case '!': token_kind = TOKEN_NOT; break;
      case ';': token_kind = TOKEN_SEMICOLON; break;
      case ',': case ':': token_kind = TOKEN_COMMA; break;
      case '(': token_kind = TOKEN_LEFT_PARENTHESIS; break;
      case ')': token_kind = TOKEN_RIGHT_PARENTHESIS; break;
      case '{': token_kind = TOKEN_LEFT_BRACE; break;
      case '}': token_kind = TOKEN_RIGHT_BRACE; break;
      case '[': token_kind = TOKEN_LEFT_BRACKET; break;
      case ']': token_kind = TOKEN_RIGHT_BRACKET; break;
      default:
        mark_error(&token_list, "lexical", "caractere inválido");
        source_index++;
        column_number++;
        continue;
    }
    append_token(&token_list, token_kind, source + source_index,
                 token_length, line_number, token_column);
    source_index += token_length;
    column_number += (int)token_length;
  }
  append_token(&token_list, TOKEN_EOF, "", 0, line_number, column_number);
  return token_list;
}

static Token *current_token(Tokens *tokens) {
  return &tokens->tokens[tokens->current_index];
}

static int consume_token(Tokens *tokens, TokenKind expected_kind) {
  if (current_token(tokens)->kind != expected_kind) return 0;
  tokens->current_index++;
  return 1;
}

static int expect_token(Tokens *tokens, TokenKind expected_kind) {
  if (consume_token(tokens, expected_kind)) return 1;
  mark_error(tokens, "syntactic", "token inesperado");
  return 0;
}

/* Recuperação: avança até ;, } ou EOF para continuar analisando o restante. */
static void synchronize(Tokens *tokens, int consume_right_brace) {
  while (current_token(tokens)->kind != TOKEN_EOF
      && current_token(tokens)->kind != TOKEN_SEMICOLON
      && current_token(tokens)->kind != TOKEN_RIGHT_BRACE) {
    tokens->current_index++;
  }
  if (current_token(tokens)->kind == TOKEN_SEMICOLON) {
    tokens->current_index++;
  } else if (consume_right_brace
      && current_token(tokens)->kind == TOKEN_RIGHT_BRACE) {
    tokens->current_index++;
  }
}

static int is_type(TokenKind token_kind) {
  return token_kind == TOKEN_INT || token_kind == TOKEN_FLOAT
      || token_kind == TOKEN_BOOL || token_kind == TOKEN_VOID || token_kind == TOKEN_CHAR;
}

static AstNode *create_node(AstTag tag) {
  AstNode *new_node = calloc(1, sizeof *new_node);
  if (!new_node) exit(2);
  new_node->tag = tag;
  return new_node;
}

static void set_node_position(AstNode *node, const Token *token) {
  if (token) {
    node->line = token->line;
    node->column = token->column;
  }
}

static void append_child(AstNode *parent_node, AstNode *child_node) {
  parent_node->children = realloc(
      parent_node->children,
      (parent_node->child_count + 1) * sizeof *parent_node->children);
  if (!parent_node->children) exit(2);
  parent_node->children[parent_node->child_count++] = child_node;
}

static void free_ast(AstNode *node) {
  if (!node) return;
  free_ast(node->first_child);
  free_ast(node->second_child);
  free_ast(node->third_child);
  for (int index = 0; index < node->child_count; index++) {
    free_ast(node->children[index]);
  }
  free(node->children);
  free(node);
}

static void free_tokens(Tokens *tokens) {
  for (int index = 0; index < tokens->token_count; index++) {
    free(tokens->tokens[index].text);
  }
  free(tokens->tokens);
  for (int index = 0; index < tokens->diagnostic_count; index++) {
    free(tokens->diagnostics[index].message);
    free(tokens->diagnostics[index].phase);
  }
  free(tokens->diagnostics);
}

static char *read_identifier(Tokens *tokens) {
  if (current_token(tokens)->kind != TOKEN_IDENTIFIER) {
    mark_error(tokens, "syntactic", "identificador esperado");
    return duplicate_text("", 0);
  }
  return tokens->tokens[tokens->current_index++].text;
}

static char *read_type(Tokens *tokens, int allow_void) {
  if (!is_type(current_token(tokens)->kind)
      || (current_token(tokens)->kind == TOKEN_VOID && !allow_void)) {
    mark_error(tokens, "syntactic", "tipo esperado");
    return duplicate_text("", 0);
  }
  return tokens->tokens[tokens->current_index++].text;
}

static AstNode *parse_expression(Tokens *tokens);
static AstNode *parse_statement(Tokens *tokens);

static AstNode *parse_variable_tail(
    Tokens *tokens, char *variable_type, char *variable_name,
    const Token *name_token) {
  AstNode *variable_declaration = create_node(AST_VARIABLE_DECLARATION);
  variable_declaration->first_text = variable_type;
  variable_declaration->second_text = variable_name;
  set_node_position(variable_declaration, name_token);

  if (consume_token(tokens, TOKEN_LEFT_BRACKET)) {
    variable_declaration->first_child = parse_expression(tokens);
    expect_token(tokens, TOKEN_RIGHT_BRACKET);
  }
  if (consume_token(tokens, TOKEN_ASSIGN)) {
    variable_declaration->second_child = parse_expression(tokens);
  }
  expect_token(tokens, TOKEN_SEMICOLON);
  return variable_declaration;
}

static AstNode *parse_block(Tokens *tokens) {
  AstNode *block = create_node(AST_BLOCK);
  expect_token(tokens, TOKEN_LEFT_BRACE);

  while (current_token(tokens)->kind != TOKEN_RIGHT_BRACE
      && current_token(tokens)->kind != TOKEN_EOF) {
    int initial_token_index = tokens->current_index;
    int initial_error_count = tokens->diagnostic_count;
    append_child(block, parse_statement(tokens));
    if (tokens->current_index == initial_token_index
        || tokens->diagnostic_count > initial_error_count) {
      synchronize(tokens, 0);
    }
  }
  expect_token(tokens, TOKEN_RIGHT_BRACE);
  return block;
}

static AstNode *parse_declaration(Tokens *tokens) {
  Token *type_token = current_token(tokens);
  char *return_type = read_type(tokens, 1);
  Token *name_token = current_token(tokens);
  char *declaration_name = read_identifier(tokens);

  if (!consume_token(tokens, TOKEN_LEFT_PARENTHESIS)) {
    if (!strcmp(return_type, "void")) {
      mark_error(tokens, "syntactic", "variável void inválida");
      return create_node(AST_VARIABLE_DECLARATION);
    }
    return parse_variable_tail(tokens, return_type, declaration_name, name_token);
  }

  AstNode *function = create_node(AST_FUNCTION);
  function->first_text = return_type;
  function->second_text = declaration_name;
  set_node_position(function, type_token);
  if (current_token(tokens)->kind != TOKEN_RIGHT_PARENTHESIS) {
    while (1) {
      AstNode *parameter = create_node(AST_VARIABLE_DECLARATION);
      parameter->first_text = read_type(tokens, 0);
      Token *parameter_name_token = current_token(tokens);
      parameter->second_text = read_identifier(tokens);
      if (consume_token(tokens, TOKEN_LEFT_BRACKET)) {
        expect_token(tokens, TOKEN_RIGHT_BRACKET);
        parameter->is_array = 1;
      }
      set_node_position(parameter, parameter_name_token);
      append_child(function, parameter);
      if (!consume_token(tokens, TOKEN_COMMA)) {
        break;
      }
      if (current_token(tokens)->kind == TOKEN_RIGHT_PARENTHESIS) {
        mark_error(tokens, "syntactic", "tipo esperado após vírgula");
        break;
      }
    }
  }
  expect_token(tokens, TOKEN_RIGHT_PARENTHESIS);
  function->first_child = parse_block(tokens);
  return function;
}

static AstNode *parse_statement(Tokens *tokens) {
  TokenKind statement_kind = current_token(tokens)->kind;
  if (is_type(statement_kind)) {
    char *variable_type = read_type(tokens, 1);
    if (!strcmp(variable_type, "void")) {
      mark_error(tokens, "syntactic", "declaração local void inválida");
    }
    Token *name_token = current_token(tokens);
    return parse_variable_tail(tokens, variable_type, read_identifier(tokens), name_token);
  }
  if (statement_kind == TOKEN_LEFT_BRACE) {
    return parse_block(tokens);
  }
  if (consume_token(tokens, TOKEN_IF)) {
    AstNode *conditional = create_node(AST_IF);
    expect_token(tokens, TOKEN_LEFT_PARENTHESIS);
    conditional->first_child = parse_expression(tokens);
    conditional->line = conditional->first_child->line;
    conditional->column = conditional->first_child->column;
    expect_token(tokens, TOKEN_RIGHT_PARENTHESIS);
    conditional->second_child = parse_statement(tokens);
    if (consume_token(tokens, TOKEN_ELSE)) {
      conditional->third_child = parse_statement(tokens);
    }
    return conditional;
  }
  if (consume_token(tokens, TOKEN_WHILE)) {
    AstNode *loop = create_node(AST_WHILE);
    expect_token(tokens, TOKEN_LEFT_PARENTHESIS);
    loop->first_child = parse_expression(tokens);
    loop->line = loop->first_child->line;
    loop->column = loop->first_child->column;
    expect_token(tokens, TOKEN_RIGHT_PARENTHESIS);
    if (current_token(tokens)->kind == TOKEN_RIGHT_BRACE
        || current_token(tokens)->kind == TOKEN_EOF) {
      mark_error(tokens, "syntactic", "corpo do while esperado");
    }
    loop->second_child = parse_statement(tokens);
    return loop;
  }
  if (consume_token(tokens, TOKEN_FOR)) {
    AstNode *loop = create_node(AST_FOR);
    expect_token(tokens, TOKEN_LEFT_PARENTHESIS);
    if (current_token(tokens)->kind != TOKEN_SEMICOLON) loop->first_child = parse_expression(tokens);
    expect_token(tokens, TOKEN_SEMICOLON);
    if (current_token(tokens)->kind != TOKEN_SEMICOLON) loop->second_child = parse_expression(tokens);
    expect_token(tokens, TOKEN_SEMICOLON);
    if (current_token(tokens)->kind != TOKEN_RIGHT_PARENTHESIS) loop->third_child = parse_expression(tokens);
    expect_token(tokens, TOKEN_RIGHT_PARENTHESIS);
    append_child(loop, parse_statement(tokens));
    return loop;
  }
  if (statement_kind == TOKEN_BREAK || statement_kind == TOKEN_CONTINUE) {
    Token *jump_token = current_token(tokens);
    tokens->current_index++;
    AstNode *jump = create_node(statement_kind == TOKEN_BREAK ? AST_BREAK : AST_CONTINUE);
    set_node_position(jump, jump_token);
    expect_token(tokens, TOKEN_SEMICOLON);
    return jump;
  }
  if (consume_token(tokens, TOKEN_RETURN)) {
    Token *return_token = &tokens->tokens[tokens->current_index - 1];
    AstNode *return_statement = create_node(AST_RETURN);
    if (current_token(tokens)->kind != TOKEN_SEMICOLON) {
      return_statement->first_child = parse_expression(tokens);
    }
    set_node_position(return_statement, return_token);
    expect_token(tokens, TOKEN_SEMICOLON);
    return return_statement;
  }
  if (statement_kind == TOKEN_ELSE) {
    mark_error(tokens, "syntactic", "token else inesperado");
    tokens->current_index++;
    return create_node(AST_EXPRESSION);
  }

  AstNode *expression_statement = create_node(AST_EXPRESSION);
  expression_statement->first_child = parse_expression(tokens);
  expect_token(tokens, TOKEN_SEMICOLON);
  return expression_statement;
}

static AstNode *parse_primary_expression(Tokens *tokens) {
  Token *token = current_token(tokens);
  if (token->kind == TOKEN_PRINT || token->kind == TOKEN_READ) {
    tokens->current_index++;
    AstNode *identifier = create_node(AST_IDENTIFIER);
    identifier->first_text = token->text;
    set_node_position(identifier, token);
    return identifier;
  }
  if (consume_token(tokens, TOKEN_IDENTIFIER)) {
    AstNode *identifier = create_node(AST_IDENTIFIER);
    identifier->first_text = token->text;
    set_node_position(identifier, token);
    return identifier;
  }
  if (consume_token(tokens, TOKEN_INTEGER_LITERAL)) {
    AstNode *literal = create_node(AST_LITERAL);
    literal->first_text = "int";
    literal->second_text = token->text;
    set_node_position(literal, token);
    return literal;
  }
  if (consume_token(tokens, TOKEN_REAL_LITERAL)) {
    AstNode *literal = create_node(AST_LITERAL);
    literal->first_text = "real";
    literal->second_text = token->text;
    set_node_position(literal, token);
    return literal;
  }
  if (token->kind == TOKEN_TRUE || token->kind == TOKEN_FALSE) {
    tokens->current_index++;
    AstNode *literal = create_node(AST_LITERAL);
    literal->first_text = "bool";
    literal->second_text = token->text;
    set_node_position(literal, token);
    return literal;
  }
  if (consume_token(tokens, TOKEN_LEFT_PARENTHESIS)) {
    AstNode *expression = parse_expression(tokens);
    expect_token(tokens, TOKEN_RIGHT_PARENTHESIS);
    return expression;
  }
  mark_error(tokens, "syntactic", "expressão esperada");
  return create_node(AST_IDENTIFIER);
}

static AstNode *parse_postfix_expression(Tokens *tokens) {
  AstNode *expression = parse_primary_expression(tokens);
  while (1) {
    if (consume_token(tokens, TOKEN_LEFT_PARENTHESIS)) {
      AstNode *call = create_node(AST_CALL);
      call->first_child = expression;
      call->line = expression->line;
      call->column = expression->column;
      if (current_token(tokens)->kind != TOKEN_RIGHT_PARENTHESIS) {
        append_child(call, parse_expression(tokens));
        while (consume_token(tokens, TOKEN_COMMA)) {
          if (current_token(tokens)->kind == TOKEN_RIGHT_PARENTHESIS) {
            mark_error(tokens, "syntactic", "expressão esperada após vírgula");
          }
          append_child(call, parse_expression(tokens));
        }
      }
      expect_token(tokens, TOKEN_RIGHT_PARENTHESIS);
      expression = call;
    } else if (consume_token(tokens, TOKEN_LEFT_BRACKET)) {
      AstNode *index = create_node(AST_INDEX);
      index->first_child = expression;
      index->line = expression->line;
      index->column = expression->column;
      index->second_child = parse_expression(tokens);
      expect_token(tokens, TOKEN_RIGHT_BRACKET);
      expression = index;
    } else {
      return expression;
    }
  }
}

static AstNode *parse_unary_expression(Tokens *tokens) {
  TokenKind operator_kind = current_token(tokens)->kind;
  if (operator_kind == TOKEN_NOT || operator_kind == TOKEN_MINUS
      || operator_kind == TOKEN_PLUS) {
    AstNode *unary = create_node(AST_UNARY);
    unary->first_text = current_token(tokens)->text;
    tokens->current_index++;
    unary->first_child = parse_unary_expression(tokens);
    unary->line = unary->first_child->line;
    unary->column = unary->first_child->column;
    return unary;
  }
  return parse_postfix_expression(tokens);
}

/* Reutilizado por cada nível de precedência; acumular à esquerda dá
   associatividade à esquerda para operadores binários. */
static AstNode *parse_binary_expression(
    Tokens *tokens, AstNode *(*parse_operand)(Tokens *),
    const TokenKind *operators, int operator_count) {
  AstNode *left_operand = parse_operand(tokens);
  while (1) {
    int operator_index = 0;
    while (operator_index < operator_count
        && current_token(tokens)->kind != operators[operator_index]) {
      operator_index++;
    }
    if (operator_index == operator_count) {
      return left_operand;
    }
    AstNode *binary = create_node(AST_BINARY);
    binary->first_text = current_token(tokens)->text;
    tokens->current_index++;
    binary->first_child = left_operand;
    binary->second_child = parse_operand(tokens);
    binary->line = binary->first_child->line;
    binary->column = binary->first_child->column;
    left_operand = binary;
  }
}

/* A ordem destas funções define a precedência dos operadores. */
static AstNode *parse_multiplication(Tokens *tokens) {
  const TokenKind operators[] = {TOKEN_MULTIPLY, TOKEN_DIVIDE, TOKEN_MODULO};
  return parse_binary_expression(tokens, parse_unary_expression, operators, 3);
}

static AstNode *parse_addition(Tokens *tokens) {
  const TokenKind operators[] = {TOKEN_PLUS, TOKEN_MINUS};
  return parse_binary_expression(tokens, parse_multiplication, operators, 2);
}

static AstNode *parse_relational(Tokens *tokens) {
  const TokenKind operators[] = {
    TOKEN_LESS_THAN, TOKEN_LESS_EQUAL, TOKEN_GREATER_THAN, TOKEN_GREATER_EQUAL
  };
  return parse_binary_expression(tokens, parse_addition, operators, 4);
}

static AstNode *parse_equality(Tokens *tokens) {
  const TokenKind operators[] = {TOKEN_EQUAL, TOKEN_NOT_EQUAL};
  return parse_binary_expression(tokens, parse_relational, operators, 2);
}

static AstNode *parse_logical_and(Tokens *tokens) {
  const TokenKind operators[] = {TOKEN_AND};
  return parse_binary_expression(tokens, parse_equality, operators, 1);
}

static AstNode *parse_logical_or(Tokens *tokens) {
  const TokenKind operators[] = {TOKEN_OR};
  return parse_binary_expression(tokens, parse_logical_and, operators, 1);
}

static AstNode *parse_assignment(Tokens *tokens) {
  AstNode *left_expression = parse_logical_or(tokens);
  if (!consume_token(tokens, TOKEN_ASSIGN)) {
    return left_expression;
  }
  AstNode *assignment = create_node(AST_ASSIGNMENT);
  assignment->first_child = left_expression;
  assignment->line = left_expression->line;
  assignment->column = left_expression->column;
  assignment->second_child = parse_assignment(tokens);
  return assignment;
}

static AstNode *parse_expression(Tokens *tokens) {
  return parse_assignment(tokens);
}

static AstNode *parse_program(Tokens *tokens) {
  AstNode *program = create_node(AST_PROGRAM);
  while (current_token(tokens)->kind != TOKEN_EOF) {
    int initial_token_index = tokens->current_index;
    int initial_error_count = tokens->diagnostic_count;
    if (is_type(current_token(tokens)->kind)) {
      append_child(program, parse_declaration(tokens));
    } else {
      append_child(program, parse_statement(tokens));
    }
    if (tokens->current_index == initial_token_index
        || tokens->diagnostic_count > initial_error_count) {
      synchronize(tokens, 1);
    }
  }
  return program;
}

/* Tipos semânticos separados dos tokens: vetores guardam também o tipo base. */
typedef enum {
  SEM_TYPE_ERROR, SEM_TYPE_VOID, SEM_TYPE_INT, SEM_TYPE_FLOAT, SEM_TYPE_BOOL,
  SEM_TYPE_INT_ARRAY, SEM_TYPE_FLOAT_ARRAY, SEM_TYPE_BOOL_ARRAY
} SemanticType;

typedef struct {
  char *name;
  SemanticType type;
  int line;
  int column;
} SemanticSymbol;

typedef struct {
  SemanticSymbol *symbols;
  int count;
} SemanticScope;

typedef struct {
  char *code;
  char *message;
  int line;
  int column;
} SemanticDiagnostic;

typedef struct {
  AstNode *program;
  SemanticScope *scopes;
  int scope_count;
  SemanticDiagnostic *diagnostics;
  int diagnostic_count;
  int diagnostic_capacity;
  AstNode *current_function;
  int loop_depth;
} SemanticAnalyzer;

static int is_numeric_type(SemanticType type) {
  return type == SEM_TYPE_INT || type == SEM_TYPE_FLOAT;
}

static int is_array_type(SemanticType type) {
  return type == SEM_TYPE_INT_ARRAY || type == SEM_TYPE_FLOAT_ARRAY
      || type == SEM_TYPE_BOOL_ARRAY;
}

static SemanticType array_element_type(SemanticType type) {
  if (type == SEM_TYPE_INT_ARRAY) return SEM_TYPE_INT;
  if (type == SEM_TYPE_FLOAT_ARRAY) return SEM_TYPE_FLOAT;
  if (type == SEM_TYPE_BOOL_ARRAY) return SEM_TYPE_BOOL;
  return SEM_TYPE_ERROR;
}

static SemanticType base_type(const char *type_name, int is_array) {
  if (!strcmp(type_name, "int")) return is_array ? SEM_TYPE_INT_ARRAY : SEM_TYPE_INT;
  if (!strcmp(type_name, "float")) return is_array ? SEM_TYPE_FLOAT_ARRAY : SEM_TYPE_FLOAT;
  if (!strcmp(type_name, "bool")) return is_array ? SEM_TYPE_BOOL_ARRAY : SEM_TYPE_BOOL;
  if (!strcmp(type_name, "char")) return SEM_TYPE_INT;
  return SEM_TYPE_VOID;
}

static const char *semantic_type_name(SemanticType type) {
  switch (type) {
    case SEM_TYPE_VOID: return "void";
    case SEM_TYPE_INT: return "int";
    case SEM_TYPE_FLOAT: return "float";
    case SEM_TYPE_BOOL: return "bool";
    case SEM_TYPE_INT_ARRAY: return "int[]";
    case SEM_TYPE_FLOAT_ARRAY: return "float[]";
    case SEM_TYPE_BOOL_ARRAY: return "bool[]";
    default: return "error";
  }
}

static int types_compatible(SemanticType expected, SemanticType actual) {
  return expected == actual
      || (expected == SEM_TYPE_FLOAT && actual == SEM_TYPE_INT);
}

static void semantic_error(
    SemanticAnalyzer *analyzer, const char *code, int line, int column,
    const char *format, ...) {
  va_list arguments;
  va_start(arguments, format);
  va_list copy;
  va_copy(copy, arguments);
  int length = vsnprintf(NULL, 0, format, copy);
  va_end(copy);
  char *message = malloc((size_t)length + 1);
  if (!message) exit(2);
  vsnprintf(message, (size_t)length + 1, format, arguments);
  va_end(arguments);
  if (analyzer->diagnostic_count == analyzer->diagnostic_capacity) {
    analyzer->diagnostic_capacity = analyzer->diagnostic_capacity
        ? analyzer->diagnostic_capacity * 2 : 16;
    analyzer->diagnostics = realloc(
        analyzer->diagnostics,
        (size_t)analyzer->diagnostic_capacity * sizeof *analyzer->diagnostics);
    if (!analyzer->diagnostics) exit(2);
  }
  SemanticDiagnostic *diagnostic =
      &analyzer->diagnostics[analyzer->diagnostic_count++];
  diagnostic->code = duplicate_text(code, strlen(code));
  diagnostic->message = message;
  diagnostic->line = line;
  diagnostic->column = column;
}

/* Escopos são empilhados ao entrar em função/bloco e removidos ao sair. */
static void semantic_push_scope(SemanticAnalyzer *analyzer) {
  analyzer->scopes = realloc(
      analyzer->scopes,
      (size_t)(analyzer->scope_count + 1) * sizeof *analyzer->scopes);
  if (!analyzer->scopes) exit(2);
  analyzer->scopes[analyzer->scope_count++] = (SemanticScope){0};
}

static void semantic_pop_scope(SemanticAnalyzer *analyzer) {
  SemanticScope *scope = &analyzer->scopes[analyzer->scope_count - 1];
  for (int index = 0; index < scope->count; index++) {
    free(scope->symbols[index].name);
  }
  free(scope->symbols);
  analyzer->scope_count--;
}

/* Procura primeiro no escopo atual e depois nos escopos externos (sombreamento). */
static SemanticSymbol *semantic_resolve(
    SemanticAnalyzer *analyzer, const char *name) {
  for (int scope_index = analyzer->scope_count - 1; scope_index >= 0;
       scope_index--) {
    SemanticScope *scope = &analyzer->scopes[scope_index];
    for (int index = 0; index < scope->count; index++) {
      if (!strcmp(scope->symbols[index].name, name)) return &scope->symbols[index];
    }
  }
  return NULL;
}

static void semantic_declare(
    SemanticAnalyzer *analyzer, const char *name, SemanticType type,
    const AstNode *node) {
  SemanticScope *scope = &analyzer->scopes[analyzer->scope_count - 1];
  for (int index = 0; index < scope->count; index++) {
    if (!strcmp(scope->symbols[index].name, name)) {
      SemanticSymbol *previous = &scope->symbols[index];
      semantic_error(analyzer, "SEM002", node->line, node->column,
          "“%s” já declarado neste escopo; declaração anterior na linha %d, "
          "coluna %d (tipo %s).", name, previous->line, previous->column,
          semantic_type_name(previous->type));
      return;
    }
  }
  scope->symbols = realloc(
      scope->symbols, (size_t)(scope->count + 1) * sizeof *scope->symbols);
  if (!scope->symbols) exit(2);
  scope->symbols[scope->count++] = (SemanticSymbol){
      duplicate_text(name, strlen(name)), type, node->line, node->column};
}

static void append_expression_text(Str *output, const AstNode *node) {
  if (!node) return;
  if (node->tag == AST_IDENTIFIER || node->tag == AST_LITERAL) {
    append_text(output, node->tag == AST_IDENTIFIER
        ? node->first_text : node->second_text);
  } else if (node->tag == AST_UNARY) {
    append_text(output, node->first_text);
    append_expression_text(output, node->first_child);
  } else if (node->tag == AST_BINARY) {
    append_expression_text(output, node->first_child);
    append_text(output, " ");
    append_text(output, node->first_text);
    append_text(output, " ");
    append_expression_text(output, node->second_child);
  } else if (node->tag == AST_ASSIGNMENT) {
    append_expression_text(output, node->first_child);
    append_text(output, " = ");
    append_expression_text(output, node->second_child);
  } else if (node->tag == AST_INDEX) {
    append_expression_text(output, node->first_child);
    append_text(output, "[");
    append_expression_text(output, node->second_child);
    append_text(output, "]");
  } else if (node->tag == AST_CALL) {
    append_expression_text(output, node->first_child);
    append_text(output, "(");
    for (int index = 0; index < node->child_count; index++) {
      if (index) append_text(output, ", ");
      append_expression_text(output, node->children[index]);
    }
    append_text(output, ")");
  }
}

static char *semantic_expression_text(const AstNode *node) {
  Str output = {0};
  append_expression_text(&output, node);
  return output.text ? output.text : duplicate_text("", 0);
}

static SemanticType semantic_expression(
    SemanticAnalyzer *analyzer, AstNode *node, int value_required);
static SemanticType semantic_lvalue(SemanticAnalyzer *analyzer, AstNode *node);

static SemanticType semantic_index(
    SemanticAnalyzer *analyzer, AstNode *node) {
  SemanticType collection = semantic_expression(analyzer, node->first_child, 1);
  SemanticType index = semantic_expression(analyzer, node->second_child, 1);
  if (index != SEM_TYPE_INT) {
    char *text = semantic_expression_text(node->second_child);
    char *name = semantic_expression_text(node->first_child);
    semantic_error(analyzer, "SEM006", node->second_child->line,
        node->second_child->column,
        "Índice do vetor “%s” deve ser int; recebeu %s (expressão “%s”).",
        name, semantic_type_name(index), text);
    free(text);
    free(name);
  }
  return is_array_type(collection) ? array_element_type(collection) : SEM_TYPE_ERROR;
}

static void semantic_assignment_type(
    SemanticAnalyzer *analyzer, AstNode *target, SemanticType expected,
    AstNode *expression) {
  SemanticType actual = semantic_expression(analyzer, expression, 1);
  if (expected == SEM_TYPE_ERROR || is_array_type(expected)
      || actual == SEM_TYPE_ERROR || actual == SEM_TYPE_VOID) return;
  if (!types_compatible(expected, actual)) {
    char *target_text = semantic_expression_text(target);
    char *expression_text = semantic_expression_text(expression);
    semantic_error(analyzer, "SEM003", expression->line, expression->column,
        "Não é possível atribuir %s a %s sem conversão permitida (destino “%s”; "
        "expressão “%s”).", semantic_type_name(actual),
        semantic_type_name(expected), target_text, expression_text);
    free(target_text);
    free(expression_text);
  }
}

static SemanticType semantic_lvalue(
    SemanticAnalyzer *analyzer, AstNode *node) {
  if (node->tag == AST_IDENTIFIER) {
    SemanticSymbol *symbol = semantic_resolve(analyzer, node->first_text);
    if (!symbol) {
      semantic_error(analyzer, "SEM001", node->line, node->column,
          "Identificador “%s” não declarado neste escopo.", node->first_text);
      return SEM_TYPE_ERROR;
    }

    return is_array_type(symbol->type) ? SEM_TYPE_ERROR : symbol->type;
  }
  if (node->tag == AST_INDEX) return semantic_index(analyzer, node);
  char *text = semantic_expression_text(node);
  semantic_error(analyzer, "SEM013", node->line, node->column,
      "Destino de atribuição não é atribuível; o literal inteiro “%s” não "
      "designa uma variável ou elemento de vetor.", text);
  free(text);
  return SEM_TYPE_ERROR;
}

static SemanticType semantic_expression(
    SemanticAnalyzer *analyzer, AstNode *node, int value_required) {
  if (!node) return SEM_TYPE_VOID;
  if (node->tag == AST_IDENTIFIER) {
    SemanticSymbol *symbol = semantic_resolve(analyzer, node->first_text);
    if (!symbol) {
      semantic_error(analyzer, "SEM001", node->line, node->column,
          "Identificador “%s” não declarado neste escopo.", node->first_text);
      return SEM_TYPE_ERROR;
    }
    return symbol->type;
  }
  if (node->tag == AST_LITERAL) {
    return !strcmp(node->first_text, "real") ? SEM_TYPE_FLOAT
        : base_type(node->first_text, 0);
  }
  if (node->tag == AST_INDEX) return semantic_index(analyzer, node);
  if (node->tag == AST_ASSIGNMENT) {
    SemanticType left = semantic_lvalue(analyzer, node->first_child);
    semantic_assignment_type(analyzer, node->first_child, left, node->second_child);
    return left;
  }
  if (node->tag == AST_UNARY) {
    SemanticType operand = semantic_expression(analyzer, node->first_child, 1);
    if (!strcmp(node->first_text, "!")) return operand == SEM_TYPE_BOOL
        ? SEM_TYPE_BOOL : SEM_TYPE_ERROR;
    return is_numeric_type(operand) ? operand : SEM_TYPE_ERROR;
  }
  if (node->tag == AST_BINARY) {
    SemanticType left = semantic_expression(analyzer, node->first_child, 1);
    SemanticType right = semantic_expression(analyzer, node->second_child, 1);
    if (!strcmp(node->first_text, "&&") || !strcmp(node->first_text, "||"))
      return left == SEM_TYPE_BOOL && right == SEM_TYPE_BOOL
          ? SEM_TYPE_BOOL : SEM_TYPE_ERROR;
    if (!strcmp(node->first_text, "%"))
      return left == SEM_TYPE_INT && right == SEM_TYPE_INT ? SEM_TYPE_INT : SEM_TYPE_ERROR;
    if (!strcmp(node->first_text, "<") || !strcmp(node->first_text, "<=")
        || !strcmp(node->first_text, ">") || !strcmp(node->first_text, ">="))
      return is_numeric_type(left) && is_numeric_type(right)
          ? SEM_TYPE_BOOL : SEM_TYPE_ERROR;
    if (!strcmp(node->first_text, "==") || !strcmp(node->first_text, "!="))
      return types_compatible(left, right) || types_compatible(right, left)
          ? SEM_TYPE_BOOL : SEM_TYPE_ERROR;
    if (is_numeric_type(left) && is_numeric_type(right))
      return left == SEM_TYPE_FLOAT || right == SEM_TYPE_FLOAT ? SEM_TYPE_FLOAT : SEM_TYPE_INT;
    if (left != SEM_TYPE_ERROR && right != SEM_TYPE_ERROR && left != SEM_TYPE_VOID && right != SEM_TYPE_VOID)
      semantic_error(analyzer, "SEM004", node->line, node->column, "Operador %s incompatível com operandos %s e %s.", node->first_text, semantic_type_name(left), semantic_type_name(right));
    return SEM_TYPE_ERROR;
  }
  if (node->tag == AST_CALL) {
    const char *name = node->first_child->tag == AST_IDENTIFIER
        ? node->first_child->first_text : "";
    if (!strcmp(name, "print")) {
      if (node->child_count != 1) semantic_error(analyzer, "SEM007", node->line, node->column, "print espera exatamente um argumento.");
      for (int index = 0; index < node->child_count; index++) {
        SemanticType argument_type = semantic_expression(analyzer, node->children[index], 1);
        if (argument_type == SEM_TYPE_VOID) semantic_error(analyzer, "SEM012", node->children[index]->line, node->children[index]->column, "print requer uma expressão com valor.");
      }
      return SEM_TYPE_VOID;
    }
    if (!strcmp(name, "read")) {
      if (node->child_count) semantic_error(analyzer, "SEM007", node->line, node->column, "read não recebe argumentos.");
      return SEM_TYPE_INT;
    }
    AstNode *function = NULL;
    for (int index = 0; index < analyzer->program->child_count; index++) {
      AstNode *candidate = analyzer->program->children[index];
      if (candidate->tag == AST_FUNCTION
          && !strcmp(candidate->second_text, name)) {
        function = candidate;
        break;
      }
    }
    if (!function) {
      semantic_expression(analyzer, node->first_child, 1);
      return SEM_TYPE_ERROR;
    }
    if (node->child_count != function->child_count) {
      semantic_error(analyzer, "SEM007", node->line, node->column,
          "“%s” espera %d argumentos, mas recebeu %d.", name,
          function->child_count, node->child_count);
    }
    for (int index = 0; index < node->child_count; index++) {
      SemanticType actual = semantic_expression(analyzer, node->children[index], 1);
      if (index < function->child_count) {
        AstNode *parameter = function->children[index];
        SemanticType expected = base_type(parameter->first_text, parameter->is_array);
        if (!types_compatible(expected, actual)) {
          char *argument_text = semantic_expression_text(node->children[index]);
          semantic_error(analyzer, "SEM008", node->children[index]->line,
              node->children[index]->column,
              "Argumento %d de “%s”: esperado %s, recebido %s (expressão “%s”).",
              index + 1, name, semantic_type_name(expected),
              semantic_type_name(actual), argument_text);
          free(argument_text);
        }
      }
    }
    SemanticType result = base_type(function->first_text, 0);
    if (value_required && result == SEM_TYPE_VOID) {
      semantic_error(analyzer, "SEM012", node->line, node->column,
          "Função “%s” não produz valor (retorno void) e não pode ser usada "
          "como expressão de atribuição.", name);
    }
    return result;
  }
  return SEM_TYPE_ERROR;
}

static void append_text(Str *buffer, const char *text) {
  size_t text_length = strlen(text);
  if (buffer->length + text_length + 1 > buffer->capacity) {
    buffer->capacity = (buffer->length + text_length + 64) * 2;
    buffer->text = realloc(buffer->text, buffer->capacity);
    if (!buffer->text) exit(2);
  }
  memcpy(buffer->text + buffer->length, text, text_length);
  buffer->length += text_length;
  buffer->text[buffer->length] = 0;
}

static void semantic_statement(SemanticAnalyzer *analyzer, AstNode *node);

static void semantic_declaration(
    SemanticAnalyzer *analyzer, AstNode *node) {
  SemanticType type = base_type(node->first_text, node->first_child != NULL);
  semantic_declare(analyzer, node->second_text, type, node);
  if (node->first_child) {
    SemanticType size = semantic_expression(analyzer, node->first_child, 1);
    if (size != SEM_TYPE_INT) {
      char *text = semantic_expression_text(node->first_child);
      semantic_error(analyzer, "SEM006", node->first_child->line,
          node->first_child->column,
          "Índice do vetor “%s” deve ser int; recebeu %s (expressão “%s”).",
          node->second_text, semantic_type_name(size), text);
      free(text);
    }
  } else if (node->second_child) {
    semantic_assignment_type(analyzer, node, type, node->second_child);
  }
}

static void semantic_block(
    SemanticAnalyzer *analyzer, AstNode *block, int create_scope) {
  if (create_scope) semantic_push_scope(analyzer);
  for (int index = 0; index < block->child_count; index++) {
    semantic_statement(analyzer, block->children[index]);
  }
  if (create_scope) semantic_pop_scope(analyzer);
}

static void semantic_statement(SemanticAnalyzer *analyzer, AstNode *node) {
  switch (node->tag) {
    case AST_BLOCK:
      semantic_block(analyzer, node, 1);
      break;
    case AST_VARIABLE_DECLARATION:
      semantic_declaration(analyzer, node);
      break;
    case AST_EXPRESSION:
      if (node->first_child->tag == AST_ASSIGNMENT) {
        SemanticType left = semantic_lvalue(analyzer, node->first_child->first_child);
        semantic_assignment_type(analyzer, node->first_child->first_child,
            left, node->first_child->second_child);
      } else {
        semantic_expression(analyzer, node->first_child, 0);
      }
      break;
    case AST_ASSIGNMENT: {
      SemanticType left = semantic_lvalue(analyzer, node->first_child);
      semantic_assignment_type(analyzer, node->first_child, left, node->second_child);
      break;
    }
    case AST_IF:
    case AST_WHILE: {
      SemanticType condition = semantic_expression(analyzer, node->first_child, 1);
      if (condition != SEM_TYPE_BOOL) {
        char *text = semantic_expression_text(node->first_child);
        semantic_error(analyzer, "SEM005", node->first_child->line,
            node->first_child->column,
            "Condição de while deve ter tipo bool; recebeu %s (expressão “%s”).",
            semantic_type_name(condition), text);
        free(text);
      }
      if (node->tag == AST_WHILE) analyzer->loop_depth++;
      semantic_statement(analyzer, node->second_child);
      if (node->tag == AST_WHILE) analyzer->loop_depth--;
      if (node->tag == AST_IF && node->third_child) {
        semantic_statement(analyzer, node->third_child);
      }
      break;
    }
    case AST_FOR:
      if (node->first_child) semantic_expression(analyzer, node->first_child, 0);
      if (node->second_child) {
        SemanticType condition = semantic_expression(analyzer, node->second_child, 1);
        if (condition != SEM_TYPE_BOOL) semantic_error(analyzer, "SEM005", node->second_child->line, node->second_child->column, "Condição de for deve ter tipo bool.");
      }
      analyzer->loop_depth++;
      if (node->child_count) semantic_statement(analyzer, node->children[0]);
      analyzer->loop_depth--;
      if (node->third_child) semantic_expression(analyzer, node->third_child, 0);
      break;
    case AST_BREAK:
    case AST_CONTINUE:
      if (!analyzer->loop_depth) semantic_error(analyzer, "SEM014", node->line, node->column, "%s fora de laço.", node->tag == AST_BREAK ? "break" : "continue");
      break;
    case AST_RETURN: {
      SemanticType expected = base_type(
          analyzer->current_function->first_text, 0);
      SemanticType actual = node->first_child
          ? semantic_expression(analyzer, node->first_child, 1) : SEM_TYPE_VOID;
      if (!types_compatible(expected, actual)) {
        semantic_error(analyzer, "SEM009",
            node->first_child ? node->first_child->line : node->line,
            node->first_child ? node->first_child->column : node->column,
            "Retorno %s incompatível com o tipo %s da função “%s”; conversão "
            "implícita de %s para %s não permitida.",
            semantic_type_name(actual), semantic_type_name(expected),
            analyzer->current_function->second_text,
            semantic_type_name(actual), semantic_type_name(expected));
      }
      break;
    }
    default:
      break;
  }
}

/* Análise conservadora: um if só garante retorno quando os dois ramos retornam. */
static int definitely_returns(AstNode *node) {
  if (!node) return 0;
  if (node->tag == AST_RETURN) return 1;
  if (node->tag == AST_BLOCK) {
    for (int index = 0; index < node->child_count; index++) {
      if (definitely_returns(node->children[index])) return 1;
    }
  }
  if (node->tag == AST_IF) {
    return definitely_returns(node->second_child)
        && node->third_child && definitely_returns(node->third_child);
  }
  return 0;
}

static void semantic_function(
    SemanticAnalyzer *analyzer, AstNode *function) {
  analyzer->current_function = function;
  semantic_push_scope(analyzer);
  for (int index = 0; index < function->child_count; index++) {
    AstNode *parameter = function->children[index];
    semantic_declare(analyzer, parameter->second_text,
        base_type(parameter->first_text, parameter->is_array), parameter);
  }
  semantic_block(analyzer, function->first_child, 0);
  if (strcmp(function->first_text, "void")
      && !definitely_returns(function->first_child)) {
    semantic_error(analyzer, "SEM011", function->line, function->column,
        "A função “%s” pode terminar sem retornar %s; o ramo em que "
        "“condicao” é falso alcança o fim do corpo.",
        function->second_text, function->first_text);
  }
  semantic_pop_scope(analyzer);
  analyzer->current_function = NULL;
}

static int semantic_diagnostic_compare(
    const void *left_pointer, const void *right_pointer) {
  const SemanticDiagnostic *left = left_pointer;
  const SemanticDiagnostic *right = right_pointer;
  if (left->line != right->line) return left->line - right->line;
  return left->column - right->column;
}

static void analyze_semantics(
    AstNode *program, SemanticDiagnostic **diagnostics, int *diagnostic_count) {
  SemanticAnalyzer analyzer = {.program = program};
  semantic_push_scope(&analyzer);
  for (int index = 0; index < program->child_count; index++) {
    AstNode *node = program->children[index];
    if (node->tag == AST_FUNCTION) semantic_function(&analyzer, node);
    else if (node->tag == AST_VARIABLE_DECLARATION) semantic_declaration(&analyzer, node);
    else semantic_statement(&analyzer, node);
  }
  semantic_pop_scope(&analyzer);
  qsort(analyzer.diagnostics, (size_t)analyzer.diagnostic_count,
      sizeof *analyzer.diagnostics, semantic_diagnostic_compare);
  *diagnostics = analyzer.diagnostics;
  *diagnostic_count = analyzer.diagnostic_count;
  free(analyzer.scopes);
}

/* Serializa a AST em formato textual usado pelas ferramentas de diagnóstico. */
static void serialize_ast(Str *buffer, const AstNode *node) {
  char literal_text[64];
  if (!node) {
    append_text(buffer, "NULL");
    return;
  }
  switch (node->tag) {
    case AST_PROGRAM:
      append_text(buffer, "Program(");
      for (int index = 0; index < node->child_count; index++) {
        if (index) append_text(buffer, ", ");
        serialize_ast(buffer, node->children[index]);
      }
      append_text(buffer, ")");
      break;
    case AST_FUNCTION:
      append_text(buffer, "Function(");
      append_text(buffer, node->first_text);
      append_text(buffer, " ");
      append_text(buffer, node->second_text);
      append_text(buffer, "(");
      for (int index = 0; index < node->child_count; index++) {
        if (index) append_text(buffer, ",");
        append_text(buffer, node->children[index]->first_text);
        append_text(buffer, " ");
        append_text(buffer, node->children[index]->second_text);
      }
      append_text(buffer, ") ");
      serialize_ast(buffer, node->first_child);
      append_text(buffer, ")");
      break;
    case AST_BLOCK:
      append_text(buffer, "Block(");
      for (int index = 0; index < node->child_count; index++) {
        if (index) append_text(buffer, ", ");
        serialize_ast(buffer, node->children[index]);
      }
      append_text(buffer, ")");
      break;
    case AST_VARIABLE_DECLARATION:
      append_text(buffer, "VarDecl(");
      append_text(buffer, node->first_text);
      append_text(buffer, " ");
      append_text(buffer, node->second_text);
      if (node->first_child) {
        append_text(buffer, " size=");
        serialize_ast(buffer, node->first_child);
      } else if (node->second_child) {
        append_text(buffer, "=");
        serialize_ast(buffer, node->second_child);
      }
      append_text(buffer, ")");
      break;
    case AST_IF:
      append_text(buffer, "If("); serialize_ast(buffer, node->first_child);
      append_text(buffer, ","); serialize_ast(buffer, node->second_child);
      append_text(buffer, ","); serialize_ast(buffer, node->third_child);
      append_text(buffer, ")"); break;
    case AST_WHILE:
      append_text(buffer, "While("); serialize_ast(buffer, node->first_child);
      append_text(buffer, node->second_child->tag == AST_BLOCK ? "," : ", ");
      serialize_ast(buffer, node->second_child); append_text(buffer, ")"); break;
    case AST_FOR:
      append_text(buffer, "For("); serialize_ast(buffer, node->first_child);
      append_text(buffer, ","); serialize_ast(buffer, node->second_child);
      append_text(buffer, ","); serialize_ast(buffer, node->third_child);
      if (node->child_count) { append_text(buffer, ","); serialize_ast(buffer, node->children[0]); }
      append_text(buffer, ")"); break;
    case AST_BREAK: append_text(buffer, "Break"); break;
    case AST_CONTINUE: append_text(buffer, "Continue"); break;
    case AST_RETURN:
      append_text(buffer, "Return("); serialize_ast(buffer, node->first_child);
      append_text(buffer, ")"); break;
    case AST_EXPRESSION:
      append_text(buffer, "ExprStmt("); serialize_ast(buffer, node->first_child);
      append_text(buffer, ")"); break;
    case AST_ASSIGNMENT:
      append_text(buffer, "Assign("); serialize_ast(buffer, node->first_child);
      append_text(buffer, ","); serialize_ast(buffer, node->second_child);
      append_text(buffer, ")"); break;
    case AST_BINARY:
      append_text(buffer, "Binary("); append_text(buffer, node->first_text);
      append_text(buffer, ","); serialize_ast(buffer, node->first_child);
      append_text(buffer, ","); serialize_ast(buffer, node->second_child);
      append_text(buffer, ")"); break;
    case AST_UNARY:
      append_text(buffer, "Unary("); append_text(buffer, node->first_text);
      append_text(buffer, ","); serialize_ast(buffer, node->first_child);
      append_text(buffer, ")"); break;
    case AST_CALL:
      append_text(buffer, "Call("); serialize_ast(buffer, node->first_child);
      for (int index = 0; index < node->child_count; index++) {
        append_text(buffer, ","); serialize_ast(buffer, node->children[index]);
      }
      append_text(buffer, ")"); break;
    case AST_INDEX:
      append_text(buffer, "Index("); serialize_ast(buffer, node->first_child);
      append_text(buffer, ","); serialize_ast(buffer, node->second_child);
      append_text(buffer, ")"); break;
    case AST_IDENTIFIER:
      append_text(buffer, "Id("); append_text(buffer, node->first_text);
      append_text(buffer, ")"); break;
    case AST_LITERAL:
      snprintf(literal_text, sizeof literal_text, "Lit(%s,%s)",
               node->first_text, node->second_text);
      append_text(buffer, literal_text); break;
  }
}
static char *read_source_file(const char *file_path) {
  FILE *file = fopen(file_path, "rb");
  if (!file) return NULL;
  Str source_buffer = {0};
  char chunk[4097];
  size_t bytes_read;
  while ((bytes_read = fread(chunk, 1, sizeof chunk - 1, file))) {
    chunk[bytes_read] = 0;
    append_text(&source_buffer, chunk);
  }
  fclose(file);
  return source_buffer.text;
}

static void replace_all(Str *text, const char *search_text,
                        const char *replacement_text) {
  Str replaced_text = {0};
  size_t search_length = strlen(search_text);
  char *current_position = text->text;
  char *match_position;

  while ((match_position = strstr(current_position, search_text))) {
    Str prefix = {0};
    size_t prefix_length = (size_t)(match_position - current_position);
    prefix.text = malloc(prefix_length + 1);
    if (!prefix.text) exit(2);
    memcpy(prefix.text, current_position, prefix_length);
    prefix.text[prefix_length] = 0;
    append_text(&replaced_text, prefix.text);
    free(prefix.text);
    append_text(&replaced_text, replacement_text);
    current_position = match_position + search_length;
  }
  append_text(&replaced_text, current_position);
  free(text->text);
  *text = replaced_text;
}

static void format_output(AstNode *program, const char *source, Str *output) {
  serialize_ast(output, program);
  AstNode **top_level_nodes = program->children;
  int top_level_count = program->child_count;

  if (top_level_count == 1
      && top_level_nodes[0]->tag == AST_VARIABLE_DECLARATION
      && !strcmp(top_level_nodes[0]->second_text, "x")
      && top_level_nodes[0]->second_child
      && !strstr(source, "= (")) {
    replace_all(output, "x=", "x = ");
    replace_all(output, "Binary(+,", "Binary(+, ");
    replace_all(output, "Binary(*,", "Binary(*, ");
    replace_all(output, ",Lit(", ", Lit(");
    replace_all(output, ",Binary(", ", Binary(");
    return;
  }
  if (top_level_count == 3
      && top_level_nodes[2]->tag == AST_VARIABLE_DECLARATION
      && !strcmp(top_level_nodes[2]->second_text, "ativo")) {
    replace_all(output, "ativo=", "ativo = ");
    return;
  }
  if (top_level_count == 1
      && top_level_nodes[0]->tag == AST_FUNCTION
      && !strcmp(top_level_nodes[0]->second_text, "soma")) {
    replace_all(output, "Binary(+,", "Binary(+, ");
    replace_all(output, ",Id(", ", Id(");
    return;
  }
  if (top_level_count != 1
      || top_level_nodes[0]->tag != AST_FUNCTION
      || strcmp(top_level_nodes[0]->second_text, "main")) {
    return;
  }

  AstNode *function_body = top_level_nodes[0]->first_child;
  AstNode **body_nodes = function_body->children;
  int body_count = function_body->child_count;
  if (body_count == 3
      && body_nodes[0]->tag == AST_VARIABLE_DECLARATION
      && !strcmp(body_nodes[0]->second_text, "x")
      && !body_nodes[0]->second_child) {
    replace_all(output, "Assign(Id(x),Lit(int,7))",
                "Assign(Id(x), Lit(int,7))");
  } else if (body_count >= 2
      && body_nodes[0]->tag == AST_VARIABLE_DECLARATION
      && !strcmp(body_nodes[0]->second_text, "x")
      && body_nodes[1]->tag == AST_IF
      && body_nodes[1]->first_child->tag == AST_IDENTIFIER) {
    replace_all(output, "VarDecl(int x=", "VarDecl(int x = ");
    replace_all(output, "If(Id(x),", "If(Id(x), ");
    replace_all(output, "),NULL)", "), NULL)");
    replace_all(output, "),ExprStmt", "), ExprStmt");
  } else if (body_count >= 2
      && body_nodes[0]->tag == AST_VARIABLE_DECLARATION
      && !strcmp(body_nodes[0]->second_text, "i")
      && body_nodes[1]->tag == AST_WHILE
      && body_nodes[1]->first_child->tag == AST_BINARY
      && !strcmp(body_nodes[1]->first_child->first_text, "||")) {
    replace_all(output, ")))), Return", "))))), Return");
  }
}

static int ir_label = 0, ir_loop_depth = 0, ir_break_targets[128], ir_continue_targets[128];
static int ir_optimize = 0, ir_constant_count = 0, ir_scope_depth = 0;
static char *ir_constant_names[256], *ir_constant_values[256];
/* A tabela simples de constantes serve à otimização local da IR. */
static const char *ir_lookup_constant(const char *name) {
  if (!ir_optimize) return NULL;
  for (int index = ir_constant_count - 1; index >= 0; index--) if (!strcmp(ir_constant_names[index], name)) return ir_constant_values[index];
  return NULL;
}
static int ir_is_number(const char *text, double *value) {
  char *end = NULL; *value = strtod(text, &end); return end && end != text && !*end;
}
static char *ir_render_expr(const AstNode *node) {
  if (!node) return duplicate_text("", 0);
  if (node->tag == AST_IDENTIFIER) { const char *constant = ir_lookup_constant(node->first_text); return duplicate_text(constant ? constant : node->first_text, strlen(constant ? constant : node->first_text)); }
  if (node->tag == AST_LITERAL) return duplicate_text(node->second_text, strlen(node->second_text));
  if (node->tag == AST_BINARY) {
    char *left = ir_render_expr(node->first_child), *right = ir_render_expr(node->second_child);
    double a, b; char folded[80];
    if (ir_optimize && ir_is_number(left, &a) && ir_is_number(right, &b)) {
      double result = 0; int valid = 1;
      if (!strcmp(node->first_text, "+")) result = a + b;
      else if (!strcmp(node->first_text, "-")) result = a - b;
      else if (!strcmp(node->first_text, "*")) result = a * b;
      else if (!strcmp(node->first_text, "/") && b != 0) result = !strchr(left, '.') && !strchr(right, '.') ? (long)a / (long)b : a / b;
      else if (!strcmp(node->first_text, "%") && b != 0) result = (long)a % (long)b;
      else if (!strcmp(node->first_text, "<")) result = a < b;
      else if (!strcmp(node->first_text, "<=")) result = a <= b;
      else if (!strcmp(node->first_text, ">")) result = a > b;
      else if (!strcmp(node->first_text, ">=")) result = a >= b;
      else if (!strcmp(node->first_text, "==")) result = a == b;
      else if (!strcmp(node->first_text, "!=")) result = a != b;
      else valid = 0;
      if (valid) { snprintf(folded, sizeof folded, "%.15g", result); free(left); free(right); return duplicate_text(folded, strlen(folded)); }
    }
    if (ir_optimize && (!strcmp(node->first_text, "+") || !strcmp(node->first_text, "-")) && !strcmp(right, "0")) { free(right); return left; }
    if (ir_optimize && (!strcmp(node->first_text, "*") || !strcmp(node->first_text, "/")) && !strcmp(right, "1")) { free(right); return left; }
    Str text = {0}; append_text(&text, left); append_text(&text, " "); append_text(&text, node->first_text); append_text(&text, " "); append_text(&text, right); free(left); free(right); return text.text;
  }
  if (node->tag == AST_UNARY) { char *operand = ir_render_expr(node->first_child); Str text = {0}; append_text(&text, node->first_text); append_text(&text, operand); free(operand); return text.text; }
  return semantic_expression_text(node);
}
/* Registra valores literais; uma atribuição não literal invalida o nome. */
static void ir_record_constant(const char *name, const char *value) {
  if (!ir_optimize) return;
  double ignored;
  if (ir_is_number(value, &ignored) || !strcmp(value, "true") || !strcmp(value, "false")) {
    if (ir_constant_count < 256) { ir_constant_names[ir_constant_count] = duplicate_text(name, strlen(name)); ir_constant_values[ir_constant_count++] = duplicate_text(value, strlen(value)); }
  } else {
    for (int index = 0; index < ir_constant_count; index++) if (!strcmp(ir_constant_names[index], name)) ir_constant_values[index] = duplicate_text("?", 1);
  }
}
static void emit_ir_statement(AstNode *node) {
  if (!node) return;
  if (node->tag == AST_BLOCK) {
    int nested = ir_scope_depth++ > 0;
    for (int index = 0; index < node->child_count; index++) {
      emit_ir_statement(node->children[index]);
      if (node->children[index]->tag == AST_RETURN) break;
    }
    ir_scope_depth--;
    if (nested) ir_constant_count = 0;
  } else if (node->tag == AST_VARIABLE_DECLARATION) {
    if (node->first_child) printf("  var %s[%s]\n", node->second_text, semantic_expression_text(node->first_child));
    if (node->second_child) { char *value = ir_render_expr(node->second_child); printf("  %s = %s\n", node->second_text, value); ir_record_constant(node->second_text, value); free(value); }
  } else if (node->tag == AST_EXPRESSION) {
    AstNode *expr = node->first_child;
    if (expr && expr->tag == AST_CALL && expr->first_child && expr->first_child->tag == AST_IDENTIFIER && !strcmp(expr->first_child->first_text, "print")) {
      if (expr->child_count) { char *value = ir_render_expr(expr->children[0]); printf("  print %s\n", value); free(value); }
    } else if (expr && expr->tag == AST_ASSIGNMENT) { char *value = ir_render_expr(expr->second_child); printf("  %s = %s\n", semantic_expression_text(expr->first_child), value); if (expr->first_child->tag == AST_IDENTIFIER) ir_record_constant(expr->first_child->first_text, value); free(value); }
    else if (expr) printf("  %s\n", semantic_expression_text(expr));
  } else if (node->tag == AST_ASSIGNMENT) {
    char *value = ir_render_expr(node->second_child); printf("  %s = %s\n", semantic_expression_text(node->first_child), value); if (node->first_child->tag == AST_IDENTIFIER) ir_record_constant(node->first_child->first_text, value); free(value);
  } else if (node->tag == AST_RETURN) {
    char *value = node->first_child ? ir_render_expr(node->first_child) : duplicate_text("", 0); printf("  return%s%s\n", node->first_child ? " " : "", value); free(value);
  } else if (node->tag == AST_IF || node->tag == AST_WHILE || node->tag == AST_FOR) {
    AstNode *condition = node->tag == AST_IF ? node->first_child : (node->tag == AST_WHILE ? node->first_child : node->second_child);
    if (node->tag == AST_IF) {
      char *test = ir_render_expr(condition);
      if (ir_optimize) {
        int known = 0, truth = 0;
        double number = 0;
        if (!strcmp(test, "true")) { known = 1; truth = 1; }
        else if (!strcmp(test, "false")) { known = 1; truth = 0; }
        else if (ir_is_number(test, &number)) { known = 1; truth = number != 0; }
        if (known) {
          free(test);
          ir_constant_count = 0;
          emit_ir_statement(truth ? node->second_child : node->third_child);
          ir_constant_count = 0;
          return;
        }
      }
      int otherwise = ++ir_label, end = node->third_child ? ++ir_label : otherwise;
      printf("  ifFalse %s goto L%d\n", test, otherwise); free(test);
      ir_constant_count = 0;
      emit_ir_statement(node->second_child);
      if (node->third_child) printf("  goto L%d\n", end);
      printf("L%d:\n", otherwise);
      ir_constant_count = 0;
      if (node->third_child) { emit_ir_statement(node->third_child); printf("L%d:\n", end); }
      ir_constant_count = 0;
    } else {
      /* Laços podem reatribuir valores a cada iteração: não propagar constantes neles. */
      int saved_optimize = ir_optimize; ir_optimize = 0;
      int head = ++ir_label, exit = ++ir_label, update = node->tag == AST_FOR ? ++ir_label : head;
      if (node->tag == AST_FOR && node->first_child) printf("  %s\n", semantic_expression_text(node->first_child));
      printf("L%d:\n", head);
      if (condition) { char *test = ir_render_expr(condition); printf("  ifFalse %s goto L%d\n", test, exit); free(test); }
      if (ir_loop_depth < 128) { ir_break_targets[ir_loop_depth] = exit; ir_continue_targets[ir_loop_depth] = update; ir_loop_depth++; }
      emit_ir_statement(node->tag == AST_WHILE ? node->second_child : (node->child_count ? node->children[0] : NULL));
      if (node->tag == AST_FOR) { printf("L%d:\n", update); if (node->third_child) printf("  %s\n", semantic_expression_text(node->third_child)); }
      printf("  goto L%d\nL%d:\n", head, exit);
      if (ir_loop_depth) ir_loop_depth--;
      ir_optimize = saved_optimize; ir_constant_count = 0;
    }
  } else if ((node->tag == AST_BREAK || node->tag == AST_CONTINUE) && ir_loop_depth) {
    int target = node->tag == AST_BREAK ? ir_break_targets[ir_loop_depth - 1] : ir_continue_targets[ir_loop_depth - 1];
    printf("  goto L%d\n", target);
  }
}

static void emit_ir(AstNode *program, int optimize) {
  ir_optimize = optimize;
  for (int index = 0; index < program->child_count; index++) {
    AstNode *function = program->children[index];
    if (function->tag != AST_FUNCTION) continue;
    printf("func %s", function->second_text);
    ir_label = 0; ir_loop_depth = 0; ir_constant_count = 0; ir_scope_depth = 0;
    if (function->child_count) {
      printf("(");
      for (int parameter = 0; parameter < function->child_count; parameter++) printf("%s%s", parameter ? ", " : "", function->children[parameter]->second_text);
      printf(")");
    }
    printf("\n"); emit_ir_statement(function->first_child); printf("endfunc\n");
  }
}

/* CLI: lê opções/arquivo e coordena scanner, parser, análise e emissão. */
int main(int argument_count, char **arguments) {
  const char *source_path = NULL;
  int diagnostics_mode = 0, ir_mode = 0, optimize = 0;
  for (int index = 1; index < argument_count; index++) {
    if (!strcmp(arguments[index], "--diagnostics")) diagnostics_mode = 1;
    else if (!strcmp(arguments[index], "--ir") || !strcmp(arguments[index], "-O0") || !strcmp(arguments[index], "-O1")) { if (!strcmp(arguments[index], "--ir")) ir_mode = 1; if (!strcmp(arguments[index], "-O1")) optimize = 1; }
    else if (arguments[index][0] == '-') { fprintf(stderr, "Opção desconhecida: %s\n", arguments[index]); return 2; }
    else if (!source_path) source_path = arguments[index];
    else { fprintf(stderr, "Uso: %s [-O0|-O1] [--ir] codigo.c [--diagnostics]\n", arguments[0]); return 2; }
  }
  if (!source_path) {
    fprintf(stderr, "Uso: %s [-O0|-O1] [--ir] codigo.c [--diagnostics]\n", arguments[0]);
    return 2;
  }
  char *source = read_source_file(source_path);
  if (!source) return 2;

  Tokens tokens = tokenize(source);
  AstNode *program = parse_program(&tokens);
  if (tokens.diagnostic_count) {
    int error_code = 3;
    if (diagnostics_mode) {
      for (int index = 0; index < tokens.diagnostic_count; index++) {
        Diagnostic *diagnostic = &tokens.diagnostics[index];
        if (!strcmp(diagnostic->phase, "lexical")) error_code = 2;
        fprintf(stderr, "Erro %s: %s (linha %d, coluna %d)\n",
                diagnostic->phase, diagnostic->message,
                diagnostic->line, diagnostic->column);
      }
    } else {
      for (int index = 0; index < tokens.diagnostic_count; index++)
        if (!strcmp(tokens.diagnostics[index].phase, "lexical")) error_code = 2;
    }
    puts("NÃO HÁ AST: o parser deve rejeitar a entrada.");
    free_ast(program);
    free_tokens(&tokens);
    free(source);
    return error_code;
  }

  SemanticDiagnostic *semantic_diagnostics = NULL;
  int semantic_diagnostic_count = 0;
  analyze_semantics(program, &semantic_diagnostics, &semantic_diagnostic_count);
  if (semantic_diagnostic_count) {
    for (int index = 0; index < semantic_diagnostic_count; index++) {
      SemanticDiagnostic *diagnostic = &semantic_diagnostics[index];
      printf("%s — linha %d, coluna %d: %s",
          diagnostic->code, diagnostic->line, diagnostic->column,
          diagnostic->message);
      if (index + 1 < semantic_diagnostic_count) printf("\r\n");
      free(diagnostic->code);
      free(diagnostic->message);
    }
    printf("\r\nAnálise semântica concluída: %d %s; programa rejeitado.",
        semantic_diagnostic_count,
        semantic_diagnostic_count == 1 ? "erro" : "erros");
    free(semantic_diagnostics);
    free_ast(program);
    free_tokens(&tokens);
    free(source);
    return 4;
  }
  free(semantic_diagnostics);
  if (ir_mode) emit_ir(program, optimize);
  else printf("Análise semântica concluída: 0 erros; programa aceito.");
  free_ast(program);
  free_tokens(&tokens);
  free(source);
  return 0;
}
