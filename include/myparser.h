#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"

class Parser {
private:
    Lexer& lexer;
    Token currentToken;
    void eat(TokenType type);

public:
    Parser(Lexer& lex);
    
    ASTNode* parse();               // 程序入口
    
    // 顶层结构
    ASTNode* parseGlobal();         // 解析全局变量或函数
    FuncDef* parseFuncDef();        // 解析函数
    
    // 语句
    StmtNode* parseStatement();     // 语句分发器
    BlockStmt* parseBlock();        // { ... }
    StmtNode* parseVarDecl();       // int a = 1;
    StmtNode* parseAssign();        // a = 1;
    StmtNode* parseIf();            // if
    StmtNode* parseWhile();         // while
    StmtNode* parseReturn();        // return

    // 表达式
    ExprNode* parseExpression();
    ExprNode* parseTerm();
    ExprNode* parseFactor();
};

#endif