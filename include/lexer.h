#ifndef LEXER_H
#define LEXER_H

#include <string>
#include <iostream>

using namespace std;

enum TokenType {
    TOK_INT, TOK_VOID, TOK_RETURN, TOK_IF, TOK_ELSE, TOK_WHILE,
    TOK_ID, TOK_NUM,
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_ASSIGN,
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE, TOK_SEMI,
    TOK_EOF, TOK_ERROR
};

struct Token {
    TokenType type;
    string value;
};

class Lexer {
private:
    string src;
    int pos;
public:
    Lexer(string source);
    Token nextToken();
};

#endif