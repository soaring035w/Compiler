#include "asmgen.h"
#include <iostream>
#include <algorithm>
#include <string>

// 寄存器名称数组
const string REG_NAMES[32] = {
    "$zero", "$at", "$v0", "$v1", "$a0", "$a1", "$a2", "$a3",
    "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7",
    "$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7",
    "$t8", "$t9", "$k0", "$k1", "$gp", "$sp", "$fp", "$ra"
};

AsmGenerator::AsmGenerator(const vector<Quad>& codes) : quads(codes) {
    // 初始化可用寄存器池
    // 优先使用 t0-t9, s0-s7
    for (int i = 8; i <= 15; ++i) availRegs.push_back(i);
    for (int i = 16; i <= 23; ++i) availRegs.push_back(i);
    for (int i = 24; i <= 25; ++i) availRegs.push_back(i);
    
    currentStackSize = 0;
    nextVictimIndex = 0; 
}

bool AsmGenerator::isNumber(string s) {
    if (s.empty()) return false;
    return isdigit(s[0]) || (s[0] == '-' && s.size() > 1);
}

int AsmGenerator::getOffset(string var) {
    if (stackOffset.find(var) == stackOffset.end()) {
        currentStackSize += 4;
        // 简单的栈分配：每个变量占用 4 字节，向下增长
        stackOffset[var] = -currentStackSize;
    }
    return stackOffset[var];
}

// 实现 emitImm 替代 li 指令
// 如果数字在 16 位范围内，用 addi；否则用 lui + ori
void AsmGenerator::emitImm(int reg, int val, ofstream& out) {
    // 16位有符号数范围: -32768 到 32767
    if (val >= -32768 && val <= 32767) {
        out << "\taddi " << REG_NAMES[reg] << ", $zero, " << val << endl;
    } else {
        // 拆分高16位和低16位
        int upper = (val >> 16) & 0xFFFF;
        int lower = val & 0xFFFF;
        
        out << "\tlui " << REG_NAMES[reg] << ", " << upper << endl;
        if (lower != 0) {
            out << "\tori " << REG_NAMES[reg] << ", " << REG_NAMES[reg] << ", " << lower << endl;
        }
    }
}

void AsmGenerator::spillAll() {
    for (int i = 0; i < 32; ++i) regContent[i] = "";
    varInReg.clear();
    nextVictimIndex = 0; 
}

int AsmGenerator::getReg(string var) {
    if (varInReg.find(var) != varInReg.end()) {
        return varInReg[var];
    }
    
    for (int r : availRegs) {
        if (regContent[r].empty()) {
            regContent[r] = var;
            varInReg[var] = r;
            return r;
        }
    }
    
    int victim = availRegs[nextVictimIndex];
    nextVictimIndex = (nextVictimIndex + 1) % availRegs.size();
    
    string oldVar = regContent[victim];
    if (!oldVar.empty()) {
        varInReg.erase(oldVar);
    }
    
    regContent[victim] = var;
    varInReg[var] = victim;
    
    return victim;
}

void AsmGenerator::generate(string filename) {
    ofstream out(filename);
    
    out << ".data" << endl; 
    out << ".text" << endl;

    // 假设 RAM 大小允许栈顶在 0x400 (1024)
    // 这一步确保在任何函数调用前 $sp 是有效的
    bool spInitialized = false;

    for (const auto& q : quads) {
        // 基本块边界清理寄存器
        if (q.op == OP_LABEL || q.op == OP_JMP || q.op == OP_JEQ || q.op == OP_FUNC_BEGIN || q.op == OP_CALL) {
            spillAll();
        }

        switch (q.op) {
            case OP_FUNC_BEGIN: {
                out << q.result << ":" << endl;
                
                // 初始化栈指针
                // 每次函数开始都重置栈
                if (!spInitialized) {
                    out << "\taddi $sp, $zero, 1024" << endl;
                    spInitialized = true;
                }
                
                stackOffset.clear();
                currentStackSize = 0;
                break;
            }
            case OP_FUNC_END: {
                break;
            }
            case OP_ADD: 
            case OP_SUB: 
            case OP_MUL: 
            case OP_DIV: {
                // 左操作数
                int r1 = getReg(q.arg1);
                if (isNumber(q.arg1)) {
                    emitImm(r1, stoi(q.arg1), out);
                } else {
                    out << "\tlw " << REG_NAMES[r1] << ", " << getOffset(q.arg1) << "($sp)" << endl;
                }

                // 右操作数
                int r2 = getReg(q.arg2); 
                if (isNumber(q.arg2)) {
                    emitImm(r2, stoi(q.arg2), out);
                } else {
                    out << "\tlw " << REG_NAMES[r2] << ", " << getOffset(q.arg2) << "($sp)" << endl;
                }

                // 结果
                if (varInReg.count(q.result)) {
                    regContent[varInReg[q.result]] = "";
                    varInReg.erase(q.result);
                }
                int r3 = getReg(q.result);

                if (q.op == OP_ADD) {
                    out << "\tadd " << REG_NAMES[r3] << ", " << REG_NAMES[r1] << ", " << REG_NAMES[r2] << endl;
                }
                else if (q.op == OP_SUB) {
                    out << "\tsub " << REG_NAMES[r3] << ", " << REG_NAMES[r1] << ", " << REG_NAMES[r2] << endl;
                }
                else if (q.op == OP_MUL) {
                    // 使用mult(结果在 HI/LO)然后mflo
                    out << "\tmult " << REG_NAMES[r1] << ", " << REG_NAMES[r2] << endl;
                    out << "\tmflo " << REG_NAMES[r3] << endl;
                }
                else if (q.op == OP_DIV) {
                    // 使用div(结果在 HI/LO)然后mflo
                    out << "\tdiv " << REG_NAMES[r1] << ", " << REG_NAMES[r2] << endl;
                    out << "\tmflo " << REG_NAMES[r3] << endl;
                }
                
                // Write-Through 到栈
                out << "\tsw " << REG_NAMES[r3] << ", " << getOffset(q.result) << "($sp)" << endl;
                break;
            }
            case OP_ASSIGN: {
                int r1 = getReg(q.arg1);
                if (isNumber(q.arg1)) {
                    emitImm(r1, stoi(q.arg1), out);
                } else {
                    out << "\tlw " << REG_NAMES[r1] << ", " << getOffset(q.arg1) << "($sp)" << endl;
                }
                varInReg[q.result] = r1;
                regContent[r1] = q.result;
                out << "\tsw " << REG_NAMES[r1] << ", " << getOffset(q.result) << "($sp)" << endl;
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
                int r1 = getReg(q.arg1);
                if (isNumber(q.arg1)) emitImm(r1, stoi(q.arg1), out);
                else out << "\tlw " << REG_NAMES[r1] << ", " << getOffset(q.arg1) << "($sp)" << endl;

                int r2 = getReg(q.arg2);
                if (isNumber(q.arg2)) emitImm(r2, stoi(q.arg2), out);
                else out << "\tlw " << REG_NAMES[r2] << ", " << getOffset(q.arg2) << "($sp)" << endl;

                out << "\tbeq " << REG_NAMES[r1] << ", " << REG_NAMES[r2] << ", " << q.result << endl;
                break;
            }
            case OP_RETURN: {
                if (!q.arg1.empty()) {
                    int r1 = getReg(q.arg1);
                    if (isNumber(q.arg1)) emitImm(r1, stoi(q.arg1), out);
                    else out << "\tlw " << REG_NAMES[r1] << ", " << getOffset(q.arg1) << "($sp)" << endl;
                    out << "\tadd $v0, " << REG_NAMES[r1] << ", $zero" << endl;
                }
                string endLabel = "Program_End";
                out << endLabel << ":" << endl;
                out << "\tj " << endLabel << endl; 
                break;
            }
            default: break;
        }
    }
    out.close();
}