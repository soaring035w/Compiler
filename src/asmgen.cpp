#include "asmgen.h"
#include <iostream>
#include <algorithm>

// 寄存器名称数组
const string REG_NAMES[32] = {
    "$zero", "$at", "$v0", "$v1", "$a0", "$a1", "$a2", "$a3",
    "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7",
    "$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7",
    "$t8", "$t9", "$k0", "$k1", "$gp", "$sp", "$fp", "$ra"
};

AsmGenerator::AsmGenerator(const vector<Quad>& codes) : quads(codes) {
    // 初始化可用寄存器池
    // 优先使用 t0-t9 (8-15, 24-25), s0-s7 (16-23)
    // 这里的顺序决定了分配优先级
    for (int i = 8; i <= 15; ++i) availRegs.push_back(i);
    for (int i = 16; i <= 23; ++i) availRegs.push_back(i);
    for (int i = 24; i <= 25; ++i) availRegs.push_back(i);
    
    currentStackSize = 0;
}

bool AsmGenerator::isNumber(string s) {
    if (s.empty()) return false;
    return isdigit(s[0]) || s[0] == '-';
}

// 在栈中为变量分配位置
int AsmGenerator::getOffset(string var) {
    if (stackOffset.find(var) == stackOffset.end()) {
        currentStackSize += 4;
        stackOffset[var] = -currentStackSize;
    }
    return stackOffset[var];
}

// 输出立即数加载: addi $reg, $zero, val
void AsmGenerator::emitImm(int reg, int val, ofstream& out) {
    out << "\taddi " << REG_NAMES[reg] << ", $zero, " << val << endl;
}

// 将寄存器内容写回内存
void AsmGenerator::spillReg(int reg) {
    string var = regContent[reg];
    if (!var.empty()) {
        // 这是一个变量，需要写回栈
        // 排除临时产生的数字常量不需要写回
        if (!isNumber(var)) {
            int offset = getOffset(var);
            // sw $reg, offset($fp)
            // 这里用了一个临时的string stream构造指令，或者直接输出
            // 注意输出流需要在外部传入，这里简化逻辑，假设调用处处理
        }
        varInReg.erase(var);
        regContent[reg] = "";
    }
}

// 实际上我们无法在内部函数轻易拿到 ofstream，
// 所以修改一下逻辑：spill操作在generate主循环里直接配合 sw 指令
// 清空所有寄存器(遇到跳转时)
void AsmGenerator::spillAll() {
    // 仅清空映射，具体的 store 指令在 generate 函数里统一处理
    // 简化的逻辑：
    // 在本编译器中，为了绝对安全，每次对变量赋值后，
    // 我们既保留在寄存器中(为了后续指令快读)，也立即 sw 回内存(Write-Through)。
    // 这样 SpillAll 只需要清空映射即可，不需要生成 sw 指令。
    for (int i = 0; i < 32; ++i) regContent[i] = "";
    varInReg.clear();
}

// 为变量找一个寄存器
// 如果已经在寄存器，返回之
// 如果不在，找个空的
// 如果没空的，踢掉一个
int AsmGenerator::getReg(string var) {
    if (varInReg.find(var) != varInReg.end()) {
        return varInReg[var];
    }
    
    // 找空闲
    for (int r : availRegs) {
        if (regContent[r].empty()) {
            regContent[r] = var;
            varInReg[var] = r;
            return r;
        }
    }
    
    // 没空闲，简单的 FIFO 替换策略：踢掉 availRegs 第一个非空的
    // 这里为了简单，随机踢掉 $t0 (即索引 8)
    // 真实情况应该分析引用计数，但这里简单点
    int victim = 8; 
    varInReg.erase(regContent[victim]);
    regContent[victim] = var;
    varInReg[var] = victim;
    return victim;
}

void AsmGenerator::generate(string filename) {
    ofstream out(filename);
    
    out << ".data" << endl;
    out << ".text" << endl;

    for (const auto& q : quads) {
        // 遇到标签或跳转，必须清空寄存器状态，防止跨基本块的错误优化
        if (q.op == OP_LABEL || q.op == OP_JMP || q.op == OP_JEQ || q.op == OP_FUNC_BEGIN || q.op == OP_CALL) {
            spillAll();
        }

        switch (q.op) {
            case OP_FUNC_BEGIN: {
                out << q.result << ":" << endl;
                // Prologue: save $fp, $ra
                out << "\tsw $fp, -4($sp)" << endl;
                out << "\tsw $ra, -8($sp)" << endl;
                out << "\taddi $fp, $sp, 0" << endl;
                // 预留足够大的栈空间，简单起见直接预留 1000 字节给局部变量
                // 因为我们没有预先扫描计算 stackSize
                out << "\taddi $sp, $sp, -1000" << endl;
                stackOffset.clear();
                currentStackSize = 8; // fp, ra 已占
                break;
            }
            case OP_FUNC_END: {
                break;
            }
            case OP_ADD: 
            case OP_SUB: 
            case OP_MUL: 
            case OP_DIV: {
                // 1. 准备左操作数
                int r1 = getReg(q.arg1);
                if (isNumber(q.arg1)) {
                    emitImm(r1, stoi(q.arg1), out);
                } else {
                    // 如果寄存器里原本就是这个变量，不需要 lw (除非它被覆盖过，但我们有 map 保证)
                    // 但如果是刚分配的空寄存器，需要 lw
                    // 这里为了代码极其简单，采用：如果刚分配，就 lw。
                    // 判断方法：如果 map 刚建立映射且之前不在，我们需要 load。
                    // 简化策略：总是生成 lw，多余的 lw 不影响正确性
                    out << "\tlw " << REG_NAMES[r1] << ", " << getOffset(q.arg1) << "($fp)" << endl;
                }

                // 2. 准备右操作数
                // 注意：getReg 可能会导致 r1 被 spill (如果寄存器极度紧缺)
                // 但我们有 20+ 个寄存器，处理 simple expression 几乎不可能溢出
                int r2 = getReg(q.arg2); 
                if (isNumber(q.arg2)) {
                    emitImm(r2, stoi(q.arg2), out);
                } else {
                    out << "\tlw " << REG_NAMES[r2] << ", " << getOffset(q.arg2) << "($fp)" << endl;
                }

                // 3. 准备结果寄存器
                // 暂时从映射中移除 result，确保拿到一个新的或覆盖旧的
                if (varInReg.count(q.result)) {
                    regContent[varInReg[q.result]] = "";
                    varInReg.erase(q.result);
                }
                int r3 = getReg(q.result);

                string asmOp;
                if (q.op == OP_ADD) asmOp = "add";
                else if (q.op == OP_SUB) asmOp = "sub";
                else if (q.op == OP_MUL) asmOp = "mul";
                else if (q.op == OP_DIV) asmOp = "div";

                out << "\t" << asmOp << " " << REG_NAMES[r3] << ", " << REG_NAMES[r1] << ", " << REG_NAMES[r2] << endl;
                
                // Write-Through: 立即存回内存
                out << "\tsw " << REG_NAMES[r3] << ", " << getOffset(q.result) << "($fp)" << endl;
                break;
            }
            case OP_ASSIGN: {
                // result = arg1
                int r1 = getReg(q.arg1);
                if (isNumber(q.arg1)) {
                    emitImm(r1, stoi(q.arg1), out);
                } else {
                    out << "\tlw " << REG_NAMES[r1] << ", " << getOffset(q.arg1) << "($fp)" << endl;
                }
                // result 存在 r1 中
                varInReg[q.result] = r1;
                regContent[r1] = q.result;
                out << "\tsw " << REG_NAMES[r1] << ", " << getOffset(q.result) << "($fp)" << endl;
                break;
            }
            case OP_LABEL: {
                out << q.result << ":" << endl;
                break;
            }
            case OP_JMP: {
                out << "\tj " << q.result << endl;
                break;
            }
            case OP_JEQ: {
                // if arg1 == arg2 goto result
                int r1 = getReg(q.arg1);
                if (isNumber(q.arg1)) emitImm(r1, stoi(q.arg1), out);
                else out << "\tlw " << REG_NAMES[r1] << ", " << getOffset(q.arg1) << "($fp)" << endl;

                int r2 = getReg(q.arg2);
                if (isNumber(q.arg2)) emitImm(r2, stoi(q.arg2), out);
                else out << "\tlw " << REG_NAMES[r2] << ", " << getOffset(q.arg2) << "($fp)" << endl;

                out << "\tbeq " << REG_NAMES[r1] << ", " << REG_NAMES[r2] << ", " << q.result << endl;
                break;
            }
            case OP_RETURN: {
                if (!q.arg1.empty()) {
                    // 返回值放入 $v0 ($2)
                    int r1 = getReg(q.arg1);
                    if (isNumber(q.arg1)) emitImm(r1, stoi(q.arg1), out);
                    else out << "\tlw " << REG_NAMES[r1] << ", " << getOffset(q.arg1) << "($fp)" << endl;
                    
                    out << "\tadd $v0, " << REG_NAMES[r1] << ", $zero" << endl;
                }
                // Epilogue
                out << "\taddi $sp, $fp, 0" << endl; // 恢复SP
                out << "\tlw $ra, -8($sp)" << endl;
                out << "\tlw $fp, -4($sp)" << endl;
                out << "\tjr $ra" << endl;
                break;
            }
            default: break;
        }
    }
    out.close();
}