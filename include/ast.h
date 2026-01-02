#ifndef AST_H
#define AST_H

#include <string>
#include <vector>
#include <iostream>

using namespace std;

enum NodeType {
    NODE_PROGRAM, NODE_VAR_DECL, NODE_FUNC_DEF, NODE_BLOCK,
    NODE_IF_STMT, NODE_WHILE_STMT, NODE_RETURN_STMT, NODE_ASSIGN_STMT,
    NODE_BINARY_EXPR, NODE_NUMBER, NODE_IDENTIFIER
};

class ASTNode {
public:
    NodeType nodeType;
    virtual ~ASTNode() {}
};

class ExprNode : public ASTNode {};
class StmtNode : public ASTNode {};

// --- 表达式 ---
class NumberNode : public ExprNode {
public:
    int value;
    NumberNode(int val) : value(val) { nodeType = NODE_NUMBER; }
};

class IdNode : public ExprNode {
public:
    string name;
    IdNode(string n) : name(n) { nodeType = NODE_IDENTIFIER; }
};

class BinaryExpr : public ExprNode {
public:
    string op;
    ExprNode* left;
    ExprNode* right;
    BinaryExpr(string o, ExprNode* l, ExprNode* r) 
        : op(o), left(l), right(r) { nodeType = NODE_BINARY_EXPR; }
};

// --- 语句 ---
class VarDeclStmt : public StmtNode {
public:
    string type;
    string name;
    ExprNode* initVal;
    VarDeclStmt(string t, string n, ExprNode* init = nullptr) 
        : type(t), name(n), initVal(init) { nodeType = NODE_VAR_DECL; }
};

class AssignStmt : public StmtNode {
public:
    string varName;
    ExprNode* value;
    AssignStmt(string name, ExprNode* val) 
        : varName(name), value(val) { nodeType = NODE_ASSIGN_STMT; }
};

class ReturnStmt : public StmtNode {
public:
    ExprNode* retVal;
    ReturnStmt(ExprNode* val) : retVal(val) { nodeType = NODE_RETURN_STMT; }
};

class BlockStmt : public StmtNode {
public:
    vector<StmtNode*> stmts;
    BlockStmt() { nodeType = NODE_BLOCK; }
};

class IfStmt : public StmtNode {
public:
    ExprNode* cond;
    StmtNode* thenBlock;
    StmtNode* elseBlock;
    IfStmt(ExprNode* c, StmtNode* t, StmtNode* e = nullptr) 
        : cond(c), thenBlock(t), elseBlock(e) { nodeType = NODE_IF_STMT; }
};

class WhileStmt : public StmtNode {
public:
    ExprNode* cond;
    StmtNode* body;
    WhileStmt(ExprNode* c, StmtNode* b) 
        : cond(c), body(b) { nodeType = NODE_WHILE_STMT; }
};

// --- 顶层结构 ---
class FuncDef : public ASTNode {
public:
    string returnType;
    string funcName;
    vector<string> args; 
    BlockStmt* body;
    FuncDef(string rt, string fn, BlockStmt* b) 
        : returnType(rt), funcName(fn), body(b) { nodeType = NODE_FUNC_DEF; }
};

class ProgramNode : public ASTNode {
public:
    vector<ASTNode*> elements; // 可以是全局变量或函数
    ProgramNode() { nodeType = NODE_PROGRAM; }
};

#endif