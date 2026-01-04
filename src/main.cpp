#include <iostream>
#include <fstream>
#include <sstream>
#include "lexer.h"
#include "myparser.h"
# include "intercode.h"
# include "asmgen.h"

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

int main(int argc, char* argv[]) {
    // 检查命令行参数
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <source_file>" << endl;
        cerr << "Example: " << argv[0] << " program.txt" << endl;
        return 1;
    }

    string filename = argv[1];
    
    // 读取源代码文件
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error: Cannot open file '" << filename << "'" << endl;
        return 1;
    }

    stringstream buffer;
    buffer << file.rdbuf();
    string code = buffer.str();
    file.close();

    if (code.empty()) {
        cerr << "Warning: File is empty" << endl;
        return 1;
    }

    cout << "Source File: " << filename << endl;
    cout << "Parsing Source Code..." << endl;

    Lexer lexer(code);
    Parser parser(lexer);
    
    ASTNode* root = parser.parse();
    
    cout << "\nGenerated AST Structure:" << endl;
    cout << "========================" << endl;
    printAST(root);
    cout << endl;

    // 生成中间代码
    InterCodeGenerator interGen;
    interGen.generate(root);
    cout << "\nGenerated Intermediate Code:" << endl;
    cout << "==============================" << endl;
    interGen.printCodes();

    // 生成汇编代码
    AsmGenerator asmGen(interGen.getCodes());
    asmGen.generate("output.asm");
    cout << "\nCompilation completed successfully!" << endl;

    return 0;
}