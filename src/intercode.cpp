#include "intercode.h"
#include <string>

InterCodeGenerator::InterCodeGenerator() {
    tempCount = 0;
    labelCount = 0;
}

/**
 * 生成一个新的临时变量名，如 t0, t1, t2...
 * 用于存储表达式计算的中间结果
 */
string InterCodeGenerator::newTemp() {
    return "t" + to_string(tempCount++);
}

/**
 * 生成一个新的逻辑标签名，如 L0, L1, L2...
 * 用于控制流跳转（if, while）
 */
string InterCodeGenerator::newLabel() {
    return "L" + to_string(labelCount++);
}

/**
 * 将一条四元式指令添加到指令序列中
 * @param op     操作符 (ADD, SUB, JMP, etc.)
 * @param arg1   操作数1
 * @param arg2   操作数2
 * @param result 结果存放地
 */
void InterCodeGenerator::emit(QuadOp op, string arg1, string arg2, string result) {
    codes.emplace_back(op, arg1, arg2, result);
}

/**
 * 获取生成的四元式列表
 */
const vector<Quad>& InterCodeGenerator::getCodes() const {
    return codes;
}

/**
 * 生成表达式的中间代码
 * 处理算术运算，并返回存储该结果的变量名（或数字字符串）
 * 例如：a + b * c 会生成：
 * MUL b, c, t0
 * ADD a, t0, t1
 * 并返回 "t1"
 */
string InterCodeGenerator::genExpr(ExprNode* node) {
    if (!node) return "";
    
    // 情况1：数字节点，直接返回数值字符串
    if (node->nodeType == NODE_NUMBER) {
        return to_string(((NumberNode*)node)->value);
    } 
    // 情况2：标识符节点，返回变量名
    else if (node->nodeType == NODE_IDENTIFIER) {
        return ((IdNode*)node)->name;
    } 
    // 情况3：二元表达式（+ - * /）
    else if (node->nodeType == NODE_BINARY_EXPR) {
        BinaryExpr* bin = (BinaryExpr*)node;
        
        // 递归计算左右子树
        string t1 = genExpr(bin->left);
        string t2 = genExpr(bin->right);
        
        // 分配一个临时变量来存储运算结果
        string res = newTemp();
        
        // 映射操作符
        QuadOp op = OP_ADD;
        if (bin->op == "+") op = OP_ADD;
        else if (bin->op == "-") op = OP_SUB;
        else if (bin->op == "*") op = OP_MUL;
        else if (bin->op == "/") op = OP_DIV;
        
        // 生成四元式：res = t1 op t2
        emit(op, t1, t2, res);
        return res;
    }
    return "";
}

/**
 * 生成语句及控制结构的中间代码
 */
void InterCodeGenerator::genNode(ASTNode* node) {
    if (!node) return;

    switch (node->nodeType) {
        // 根节点：遍历所有顶层元素（函数或声明）
        case NODE_PROGRAM: {
            ProgramNode* prog = (ProgramNode*)node;
            for (auto el : prog->elements) genNode(el);
            break;
        }

        // 函数定义：标记函数开始和结束
        case NODE_FUNC_DEF: {
            FuncDef* func = (FuncDef*)node;
            emit(OP_FUNC_BEGIN, "", "", func->funcName);
            genNode(func->body); // 递归生成函数体代码
            emit(OP_FUNC_END, "", "", func->funcName);
            break;
        }

        // 语句块：遍历块内所有语句
        case NODE_BLOCK: {
            BlockStmt* block = (BlockStmt*)node;
            for (auto stmt : block->stmts) genNode(stmt);
            break;
        }

        // 变量声明：如果有初始化值，生成赋值指令
        case NODE_VAR_DECL: {
            VarDeclStmt* decl = (VarDeclStmt*)node;
            if (decl->initVal) {
                string val = genExpr(decl->initVal);
                emit(OP_ASSIGN, val, "", decl->name);
            }
            break;
        }

        // 赋值语句：x = expr
        case NODE_ASSIGN_STMT: {
            AssignStmt* assign = (AssignStmt*)node;
            string val = genExpr(assign->value);
            emit(OP_ASSIGN, val, "", assign->varName);
            break;
        }

        // 返回语句：return expr
        case NODE_RETURN_STMT: {
            ReturnStmt* ret = (ReturnStmt*)node;
            string val = genExpr(ret->retVal);
            emit(OP_RETURN, val, "", "");
            break;
        }

        // IF 语句：控制流转换逻辑
        case NODE_IF_STMT: {
            IfStmt* stmt = (IfStmt*)node;
            string cond = genExpr(stmt->cond); // 计算条件表达式
            
            string lblElse = newLabel(); // else 分支入口
            string lblEnd = newLabel();  // 整个 if 结构的出口
            
            // 核心逻辑：如果条件为 0 (false)，跳转到 else 标签
            emit(OP_JEQ, cond, "0", lblElse);
            
            // 生成 then 分支代码
            genNode(stmt->thenBlock);
            // 执行完 then 后，必须强制跳转到 end，跳过 else 分支
            emit(OP_JMP, "", "", lblEnd);
            
            // else 分支开始
            emit(OP_LABEL, "", "", lblElse);
            if (stmt->elseBlock) genNode(stmt->elseBlock);
            
            // if 结构结束点
            emit(OP_LABEL, "", "", lblEnd);
            break;
        }

        // WHILE 语句：循环控制
        case NODE_WHILE_STMT: {
            WhileStmt* stmt = (WhileStmt*)node;
            string lblStart = newLabel(); // 循环检查点
            string lblEnd = newLabel();   // 循环出口
            
            // 在头部放置标签，以便每次循环结束后跳回这里
            emit(OP_LABEL, "", "", lblStart);
            
            // 检查循环条件
            string cond = genExpr(stmt->cond);
            // 如果条件不成立 (==0)，直接跳出循环
            emit(OP_JEQ, cond, "0", lblEnd);
            
            // 生成循环体内部代码
            genNode(stmt->body);
            
            // 循环体结束后，无条件跳转回头部再次检查条件
            emit(OP_JMP, "", "", lblStart);
            
            // 整个循环结束的出口
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
    for (auto& q : codes) {
        // 输出格式：操作码 操作数1 操作数2 结果
        cout << q.op << " " << q.arg1 << " " << q.arg2 << " " << q.result << endl;
    }
}