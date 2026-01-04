#ifndef INTERCODE_H
#define INTERCODE_H

#include "ast.h"
#include <string>
#include <vector>
#include <iostream>

using namespace std;

// 操作符枚举
enum QuadOp {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, // 算术
    OP_ASSIGN,                      // 赋值 result = arg1
    OP_LABEL,                       // 标签 result:
    OP_JMP,                         // 跳转 goto result
    OP_JEQ, OP_JNE, OP_JGT, OP_JLT, // 条件跳转
    OP_PARAM,                       // 参数传递
    OP_CALL,                        // 函数调用
    OP_RETURN,                      // 返回
    OP_FUNC_BEGIN,                  // 函数头
    OP_FUNC_END                     // 函数尾
};

// 四元式结构
struct Quad {
    QuadOp op;
    string arg1;
    string arg2;
    string result;

    Quad(QuadOp o, string a1, string a2, string res) 
        : op(o), arg1(a1), arg2(a2), result(res) {}
};

class InterCodeGenerator {
private:
    vector<Quad> codes;
    int tempCount = 0;
    int labelCount = 0;

    string newTemp();
    string newLabel();
    void emit(QuadOp op, string arg1, string arg2, string result);

    void genNode(ASTNode* node);
    string genExpr(ExprNode* node);

public:
    InterCodeGenerator();
    void generate(ASTNode* root);
    const vector<Quad>& getCodes() const;
    void printCodes(); // 调试用
};

#endif