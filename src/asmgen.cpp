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

// 在栈中为变量分配位置
// 这里分配的是相对于 $sp (初始化后的高地址) 的负偏移
int AsmGenerator::getOffset(string var) {
    if (stackOffset.find(var) == stackOffset.end()) {
        currentStackSize += 4;
        // 简单的栈分配：每个变量占用 4 字节，向下增长
        stackOffset[var] = -currentStackSize;
    }
    return stackOffset[var];
}

void AsmGenerator::emitImm(int reg, int val, ofstream& out) {
    out << "\taddi " << REG_NAMES[reg] << ", $zero, " << val << endl;
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
    
    // 标准头部
    out << ".data" << endl;
    out << ".text" << endl;

    for (const auto& q : quads) {
        // 基本块边界清理寄存器
        if (q.op == OP_LABEL || q.op == OP_JMP || q.op == OP_JEQ || q.op == OP_FUNC_BEGIN || q.op == OP_CALL) {
            spillAll();
        }

        switch (q.op) {
            case OP_FUNC_BEGIN: {
                out << q.result << ":" << endl;
                
                // 假设内存大小为 1024 (0x400)，将 $sp 设置到栈底
                out << "\taddi $sp, $zero, 1024" << endl;
                
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
                // 1. 左操作数
                int r1 = getReg(q.arg1);
                if (isNumber(q.arg1)) {
                    emitImm(r1, stoi(q.arg1), out);
                } else {
                    // 所有 lw/sw 使用 $sp 作为基址
                    out << "\tlw " << REG_NAMES[r1] << ", " << getOffset(q.arg1) << "($sp)" << endl;
                }

                // 2. 右操作数
                int r2 = getReg(q.arg2); 
                if (isNumber(q.arg2)) {
                    emitImm(r2, stoi(q.arg2), out);
                } else {
                    out << "\tlw " << REG_NAMES[r2] << ", " << getOffset(q.arg2) << "($sp)" << endl;
                }

                // 3. 结果
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
                
                // Write-Through 到栈 ($sp)
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
                // 如果有返回值 (可选)
                if (!q.arg1.empty()) {
                    int r1 = getReg(q.arg1);
                    if (isNumber(q.arg1)) emitImm(r1, stoi(q.arg1), out);
                    else out << "\tlw " << REG_NAMES[r1] << ", " << getOffset(q.arg1) << "($sp)" << endl;
                    out << "\tadd $v0, " << REG_NAMES[r1] << ", $zero" << endl;
                }

                // 生成一个死循环，代表程序结束。
                string endLabel = "END_LOOP_" + to_string(rand()); 
                out << "Program_End:" << endl;
                out << "\tj Program_End" << endl; 
                break;
            }
            default: break;
        }
    }
    out.close();
}