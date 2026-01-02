#include <iostream>
#include "lexer.h"
#include "myparser.h"

// 打印工具：支持所有节点类型
void printAST(ASTNode* node, int level = 0) {
    if (!node) return;
    string indent(level * 2, ' ');

    switch (node->nodeType) {
        case NODE_PROGRAM: {
            cout << indent << "Program" << endl;
            ProgramNode* prog = (ProgramNode*)node;
            for (auto el : prog->elements) printAST(el, level + 1);
            break;
        }
        case NODE_FUNC_DEF: {
            FuncDef* func = (FuncDef*)node;
            cout << indent << "Function: " << func->returnType << " " << func->funcName << "()" << endl;
            printAST(func->body, level + 1);
            break;
        }
        case NODE_BLOCK: {
            cout << indent << "Block { ... }" << endl;
            BlockStmt* block = (BlockStmt*)node;
            for (auto stmt : block->stmts) printAST(stmt, level + 1);
            break;
        }
        case NODE_IF_STMT: {
            cout << indent << "If Statement" << endl;
            IfStmt* s = (IfStmt*)node;
            cout << indent << "  Cond:" << endl;
            printAST(s->cond, level + 2);
            cout << indent << "  Then:" << endl;
            printAST(s->thenBlock, level + 2);
            if (s->elseBlock) {
                cout << indent << "  Else:" << endl;
                printAST(s->elseBlock, level + 2);
            }
            break;
        }
        case NODE_WHILE_STMT: {
            cout << indent << "While Statement" << endl;
            WhileStmt* s = (WhileStmt*)node;
            cout << indent << "  Cond:" << endl;
            printAST(s->cond, level + 2);
            cout << indent << "  Body:" << endl;
            printAST(s->body, level + 2);
            break;
        }
        case NODE_RETURN_STMT: {
            cout << indent << "Return" << endl;
            printAST(((ReturnStmt*)node)->retVal, level + 1);
            break;
        }
        case NODE_VAR_DECL: {
            VarDeclStmt* s = (VarDeclStmt*)node;
            cout << indent << "VarDecl: " << s->type << " " << s->name << endl;
            if (s->initVal) {
                cout << indent << "  = " << endl;
                printAST(s->initVal, level + 2);
            }
            break;
        }
        case NODE_ASSIGN_STMT: {
            AssignStmt* s = (AssignStmt*)node;
            cout << indent << "Assign: " << s->varName << " =" << endl;
            printAST(s->value, level + 1);
            break;
        }
        case NODE_BINARY_EXPR: {
            BinaryExpr* s = (BinaryExpr*)node;
            cout << indent << "Op: " << s->op << endl;
            printAST(s->left, level + 1);
            printAST(s->right, level + 1);
            break;
        }
        case NODE_NUMBER: {
            cout << indent << ((NumberNode*)node)->value << endl;
            break;
        }
        case NODE_IDENTIFIER: {
            cout << indent << "Id: " << ((IdNode*)node)->name << endl;
            break;
        }
    }
}

int main() {
    // === 终极测试用例 ===
    // 包含函数、If、While、代码块、复杂运算
    string code = R"(
        int main() {
            int a = 10;
            int b = 5;
            if (a + b) {
                a = a + 1;
            } else {
                b = 0;
            }
            while (b) {
                b = b - 1;
            }
            return a;
        }
    )";
    
    cout << "=== Final Compiler Frontend Test ===" << endl;
    cout << "Parsing Source Code..." << endl;

    Lexer lexer(code);
    Parser parser(lexer);
    
    ASTNode* root = parser.parse();
    
    cout << "\nGenerated AST Structure:" << endl;
    cout << "========================" << endl;
    printAST(root);

    return 0;
}