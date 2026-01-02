// 文件名: lexer.cpp (修正版)
#include "lexer.h"
#include <cctype>

Lexer::Lexer(string source) : src(source), pos(0) {}

Token Lexer::nextToken() {
    while (pos < src.length()) {
        char current = src[pos];

        // 1. 跳过空白
        if (isspace(current)) { pos++; continue; }

        // 2. 识别单词 (关键字或变量名)
        if (isalpha(current)) {
            string word;
            while (pos < src.length() && (isalnum(src[pos]) || src[pos] == '_')) {
                word += src[pos++];
            }
            
            // --- 之前漏掉的关键字补在这个位置 ---
            if (word == "int") return {TOK_INT, "int"};
            if (word == "void") return {TOK_VOID, "void"};
            if (word == "return") return {TOK_RETURN, "return"};
            if (word == "if") return {TOK_IF, "if"};         // <--- 新增
            if (word == "else") return {TOK_ELSE, "else"};   // <--- 新增
            if (word == "while") return {TOK_WHILE, "while"}; // <--- 新增
            
            return {TOK_ID, word};
        }

        // 3. 识别数字
        if (isdigit(current)) {
            string numStr;
            while (pos < src.length() && isdigit(src[pos])) {
                numStr += src[pos++];
            }
            return {TOK_NUM, numStr};
        }

        // 4. 识别符号
        pos++;
        switch (current) {
            case '+': return {TOK_PLUS, "+"};
            case '-': return {TOK_MINUS, "-"};
            case '*': return {TOK_STAR, "*"};
            case '/': return {TOK_SLASH, "/"};
            case '=': return {TOK_ASSIGN, "="};
            case ';': return {TOK_SEMI, ";"};
            case '(': return {TOK_LPAREN, "("};
            case ')': return {TOK_RPAREN, ")"};
            case '{': return {TOK_LBRACE, "{"};
            case '}': return {TOK_RBRACE, "}"};
            default: return {TOK_ERROR, string(1, current)};
        }
    }
    return {TOK_EOF, ""};
}