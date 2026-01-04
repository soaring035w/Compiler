#include "intercode.h"
#include <string>

using namespace std;

InterCodeGenerator::InterCodeGenerator() {}

string InterCodeGenerator::newTemp() {
    return "t" + to_string(tempCount++);
}

string InterCodeGenerator::newLabel() {
    return "L" + to_string(labelCount++);
}

void InterCodeGenerator::emit(QuadOp op, string arg1, string arg2, string result) {
    codes.emplace_back(op, arg1, arg2, result);
}

const vector<Quad>& InterCodeGenerator::getCodes() const {
    return codes;
}

string InterCodeGenerator::genExpr(ExprNode* node) {
    if (!node) return "";
    
    if (node->nodeType == NODE_NUMBER) {
        return to_string(((NumberNode*)node)->value);
    } 
    else if (node->nodeType == NODE_IDENTIFIER) {
        return ((IdNode*)node)->name;
    } 
    else if (node->nodeType == NODE_BINARY_EXPR) {
        BinaryExpr* bin = (BinaryExpr*)node;
        string t1 = genExpr(bin->left);
        string t2 = genExpr(bin->right);
        string res = newTemp();
        
        QuadOp op = OP_ADD;
        if (bin->op == "+") op = OP_ADD;
        else if (bin->op == "-") op = OP_SUB;
        else if (bin->op == "*") op = OP_MUL;
        else if (bin->op == "/") op = OP_DIV;
        
        emit(op, t1, t2, res);
        return res;
    }
    return "";
}

void InterCodeGenerator::genNode(ASTNode* node) {
    if (!node) return;

    switch (node->nodeType) {
        case NODE_PROGRAM: {
            ProgramNode* prog = (ProgramNode*)node;
            for (auto el : prog->elements) genNode(el);
            break;
        }
        case NODE_FUNC_DEF: {
            FuncDef* func = (FuncDef*)node;
            emit(OP_FUNC_BEGIN, "", "", func->funcName);
            genNode(func->body);
            emit(OP_FUNC_END, "", "", func->funcName);
            break;
        }
        case NODE_BLOCK: {
            BlockStmt* block = (BlockStmt*)node;
            for (auto stmt : block->stmts) genNode(stmt);
            break;
        }
        case NODE_VAR_DECL: {
            VarDeclStmt* decl = (VarDeclStmt*)node;
            if (decl->initVal) {
                string val = genExpr(decl->initVal);
                emit(OP_ASSIGN, val, "", decl->name);
            }
            break;
        }
        case NODE_ASSIGN_STMT: {
            AssignStmt* assign = (AssignStmt*)node;
            string val = genExpr(assign->value);
            emit(OP_ASSIGN, val, "", assign->varName);
            break;
        }
        case NODE_RETURN_STMT: {
            ReturnStmt* ret = (ReturnStmt*)node;
            string val = genExpr(ret->retVal);
            emit(OP_RETURN, val, "", "");
            break;
        }
        case NODE_IF_STMT: {
            IfStmt* stmt = (IfStmt*)node;
            string cond = genExpr(stmt->cond);
            string lblElse = newLabel();
            string lblEnd = newLabel();
            
            // if cond == 0 goto else
            emit(OP_JEQ, cond, "0", lblElse);
            genNode(stmt->thenBlock);
            emit(OP_JMP, "", "", lblEnd);
            
            emit(OP_LABEL, "", "", lblElse);
            if (stmt->elseBlock) genNode(stmt->elseBlock);
            emit(OP_LABEL, "", "", lblEnd);
            break;
        }
        case NODE_WHILE_STMT: {
            WhileStmt* stmt = (WhileStmt*)node;
            string lblStart = newLabel();
            string lblEnd = newLabel();
            
            emit(OP_LABEL, "", "", lblStart);
            string cond = genExpr(stmt->cond);
            emit(OP_JEQ, cond, "0", lblEnd);
            
            genNode(stmt->body);
            emit(OP_JMP, "", "", lblStart);
            emit(OP_LABEL, "", "", lblEnd);
            break;
        }
        default: break;
    }
}

void InterCodeGenerator::generate(ASTNode* root) {
    codes.clear();
    tempCount = 0;
    labelCount = 0;
    genNode(root);
}

void InterCodeGenerator::printCodes() {
    // 调试代码
    for (auto& q : codes) {
        cout << q.op << " " << q.arg1 << " " << q.arg2 << " " << q.result << endl;
    }
}