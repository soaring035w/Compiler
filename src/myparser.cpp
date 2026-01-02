#include "myparser.h"
#include <cstdlib>

Parser::Parser(Lexer& lex) : lexer(lex) {
    currentToken = lexer.nextToken();
}

void Parser::eat(TokenType type) {
    if (currentToken.type == type) {
        currentToken = lexer.nextToken();
    } else {
        cerr << "Syntax Error: Expected token type " << type 
             << " but got " << currentToken.type << " value: " << currentToken.value << endl;
        exit(1);
    }
}

// Factor -> NUM | ID | ( Expr )
ExprNode* Parser::parseFactor() {
    Token token = currentToken;
    if (token.type == TOK_NUM) {
        eat(TOK_NUM);
        return new NumberNode(stoi(token.value));
    } else if (token.type == TOK_ID) {
        eat(TOK_ID);
        return new IdNode(token.value);
    } else if (token.type == TOK_LPAREN) {
        eat(TOK_LPAREN);
        ExprNode* node = parseExpression();
        eat(TOK_RPAREN);
        return node;
    }
    cerr << "Error: Unexpected token in Factor: " << token.value << endl;
    exit(1);
}

// Term -> * /
ExprNode* Parser::parseTerm() {
    ExprNode* left = parseFactor();
    while (currentToken.type == TOK_STAR || currentToken.type == TOK_SLASH) {
        Token op = currentToken;
        eat(op.type);
        ExprNode* right = parseFactor();
        left = new BinaryExpr(op.value, left, right);
    }
    return left;
}

// Expr -> + -
ExprNode* Parser::parseExpression() {
    ExprNode* left = parseTerm();
    while (currentToken.type == TOK_PLUS || currentToken.type == TOK_MINUS) {
        Token op = currentToken;
        eat(op.type);
        ExprNode* right = parseTerm();
        left = new BinaryExpr(op.value, left, right);
    }
    return left;
}

// Block -> { stmt... }
BlockStmt* Parser::parseBlock() {
    eat(TOK_LBRACE);
    BlockStmt* block = new BlockStmt();
    while (currentToken.type != TOK_RBRACE && currentToken.type != TOK_EOF) {
        block->stmts.push_back(parseStatement());
    }
    eat(TOK_RBRACE);
    return block;
}

// If -> if (expr) stmt [else stmt]
StmtNode* Parser::parseIf() {
    eat(TOK_IF);
    eat(TOK_LPAREN);
    ExprNode* cond = parseExpression();
    eat(TOK_RPAREN);
    
    StmtNode* thenStmt = parseStatement();
    StmtNode* elseStmt = nullptr;
    
    if (currentToken.type == TOK_ELSE) {
        eat(TOK_ELSE);
        elseStmt = parseStatement();
    }
    return new IfStmt(cond, thenStmt, elseStmt);
}

// While -> while (expr) stmt
StmtNode* Parser::parseWhile() {
    eat(TOK_WHILE);
    eat(TOK_LPAREN);
    ExprNode* cond = parseExpression();
    eat(TOK_RPAREN);
    StmtNode* body = parseStatement();
    return new WhileStmt(cond, body);
}

// Return -> return expr;
StmtNode* Parser::parseReturn() {
    eat(TOK_RETURN);
    ExprNode* val = parseExpression();
    eat(TOK_SEMI);
    return new ReturnStmt(val);
}

// VarDecl -> int id [= expr];
StmtNode* Parser::parseVarDecl() {
    eat(TOK_INT);
    string name = currentToken.value;
    eat(TOK_ID);
    ExprNode* init = nullptr;
    if (currentToken.type == TOK_ASSIGN) {
        eat(TOK_ASSIGN);
        init = parseExpression();
    }
    eat(TOK_SEMI);
    return new VarDeclStmt("int", name, init);
}

// Assign -> id = expr;
StmtNode* Parser::parseAssign() {
    string name = currentToken.value;
    eat(TOK_ID);
    eat(TOK_ASSIGN);
    ExprNode* val = parseExpression();
    eat(TOK_SEMI);
    return new AssignStmt(name, val);
}

// 语句分发
StmtNode* Parser::parseStatement() {
    if (currentToken.type == TOK_IF) return parseIf();
    if (currentToken.type == TOK_WHILE) return parseWhile();
    if (currentToken.type == TOK_RETURN) return parseReturn();
    if (currentToken.type == TOK_LBRACE) return parseBlock();
    if (currentToken.type == TOK_INT) return parseVarDecl();
    if (currentToken.type == TOK_ID) return parseAssign();
    
    cerr << "Unknown statement: " << currentToken.value << endl;
    exit(1);
}

// Func -> int id () block
FuncDef* Parser::parseFuncDef() {
    // 简单假设函数都是 int 返回类型
    string retType = "int";
    if (currentToken.type == TOK_INT) eat(TOK_INT);
    else if (currentToken.type == TOK_VOID) { eat(TOK_VOID); retType = "void"; }
    
    string name = currentToken.value;
    eat(TOK_ID);
    
    eat(TOK_LPAREN);
    // 这里偷懒省略了参数解析，直接假设无参或跳过参数
    // 真正的编译器需要在这里解析参数列表
    while (currentToken.type != TOK_RPAREN) { 
        currentToken = lexer.nextToken(); // 跳过参数
    }
    eat(TOK_RPAREN);
    
    BlockStmt* body = parseBlock();
    return new FuncDef(retType, name, body);
}

ASTNode* Parser::parse() {
    ProgramNode* root = new ProgramNode();
    while (currentToken.type != TOK_EOF) {
        // 简单处理：如果是 int 开头，预读下一个看看是函数还是变量
        // 这是一个简化的预测分析，真实情况要复杂点
        // 这里直接假设全是函数定义，方便完工
        root->elements.push_back(parseFuncDef());
    }
    return root;
}