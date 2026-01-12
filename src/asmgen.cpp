#include "asmgen.h"
#include <iostream>
#include <algorithm>
#include <string>

// MIPS 32个寄存器的标准名称映射表
const string REG_NAMES[32] = {
    "$zero", "$at", "$v0", "$v1", "$a0", "$a1", "$a2", "$a3",
    "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7",
    "$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7",
    "$t8", "$t9", "$k0", "$k1", "$gp", "$sp", "$fp", "$ra"
};

/**
 * 构造函数：初始化汇编生成器
 * @param codes 输入的四元式列表
 */
AsmGenerator::AsmGenerator(const vector<Quad>& codes) : quads(codes) {
    // 初始化可用寄存器池（分配策略：优先使用临时寄存器 $t 和 静态寄存器 $s）
    // t0-t7 (8-15)
    for (int i = 8; i <= 15; ++i) availRegs.push_back(i);
    // s0-s7 (16-23)
    for (int i = 16; i <= 23; ++i) availRegs.push_back(i);
    // t8-t9 (24-25)
    for (int i = 24; i <= 25; ++i) availRegs.push_back(i);
    
    currentStackSize = 0; // 当前栈帧偏移初始化
    nextVictimIndex = 0;  // 寄存器置换算法（轮询法）的指针
}

/**
 * 判断操作数是否为纯数字字符串（立即数）
 */
bool AsmGenerator::isNumber(string s) {
    if (s.empty()) return false;
    return isdigit(s[0]) || (s[0] == '-' && s.size() > 1);
}

/**
 * 简单的栈分配：获取变量在当前栈帧中的偏移地址
 * 如果变量不在栈上，则为其分配 4 字节空间
 * @param var 变量名
 */
int AsmGenerator::getOffset(string var) {
    if (stackOffset.find(var) == stackOffset.end()) {
        currentStackSize += 4;
        // 栈向下增长，因此偏移量为负
        stackOffset[var] = -currentStackSize;
    }
    return stackOffset[var];
}

/**
 * 生成加载立即数的指令
 * MIPS 指令限制立即数为 16 位，若超过则需要拆分
 * @param reg 目标寄存器索引
 * @param val 立即数值
 */
void AsmGenerator::emitImm(int reg, int val, ofstream& out) {
    // 16位有符号数范围: -32768 到 32767
    if (val >= -32768 && val <= 32767) {
        // 在范围内，直接使用 addi 指令
        out << "\taddi " << REG_NAMES[reg] << ", $zero, " << val << endl;
    } else {
        // 超过16位：拆分为高16位（lui）和低16位（ori）
        int upper = (val >> 16) & 0xFFFF;
        int lower = val & 0xFFFF;
        
        out << "\tlui " << REG_NAMES[reg] << ", " << upper << endl;
        if (lower != 0) {
            out << "\tori " << REG_NAMES[reg] << ", " << REG_NAMES[reg] << ", " << lower << endl;
        }
    }
}

/**
 * 清空所有寄存器状态（Spill）
 * 通常在基本块结束或发生跳转时调用，保证数据一致性（Write-back 后清空）
 */
void AsmGenerator::spillAll() {
    for (int i = 0; i < 32; ++i) regContent[i] = "";
    varInReg.clear();
    nextVictimIndex = 0;
}

/**
 * 寄存器分配逻辑：获取一个可用的寄存器
 * 1. 如果变量已在寄存器中，直接返回。
 * 2. 如果有空闲寄存器，进行分配。
 * 3. 如果已满，使用轮询法挑选一个“受害者”寄存器腾出空间。
 */
int AsmGenerator::getReg(string var) {
    // 命中：变量已在寄存器中
    if (varInReg.find(var) != varInReg.end()) {
        return varInReg[var];
    }
    
    // 查找空闲寄存器
    for (int r : availRegs) {
        if (regContent[r].empty()) {
            regContent[r] = var;
            varInReg[var] = r;
            return r;
        }
    }
    
    // 置换：选择一个受害者
    int victim = availRegs[nextVictimIndex];
    nextVictimIndex = (nextVictimIndex + 1) % availRegs.size();
    
    string oldVar = regContent[victim];
    if (!oldVar.empty()) {
        varInReg.erase(oldVar); // 移除旧变量的映射
    }
    
    regContent[victim] = var;
    varInReg[var] = victim;
    
    return victim;
}

/**
 * 主生成函数：遍历四元式并翻译为汇编
 */
void AsmGenerator::generate(string filename) {
    ofstream out(filename);
    
    // 输出汇编文件头
    out << ".data" << endl; 
    out << ".text" << endl;

    bool spInitialized = false; // 标记栈指针是否已初始化

    for (const auto& q : quads) {
        // 基本块边界处理
        // 在标签、跳转、函数调用前清空寄存器，将变量写回内存（保证跳转后状态正确）
        if (q.op == OP_LABEL || q.op == OP_JMP || q.op == OP_JEQ || q.op == OP_FUNC_BEGIN || q.op == OP_CALL) {
            spillAll();
        }

        switch (q.op) {
            case OP_FUNC_BEGIN: {
                out << q.result << ":" << endl; // 函数名标签
                
                // 运行时环境初始化：设置栈指针起始地址（假设 1024）
                if (!spInitialized) {
                    out << "\taddi $sp, $zero, 1024" << endl;
                    spInitialized = true;
                }
                
                // 函数开始时重置当前函数的栈偏移映射
                stackOffset.clear();
                currentStackSize = 0;
                break;
            }

            case OP_ADD: 
            case OP_SUB: 
            case OP_MUL: 
            case OP_DIV: {
                // 1. 处理左操作数 arg1
                int r1 = getReg(q.arg1);
                if (isNumber(q.arg1)) {
                    emitImm(r1, stoi(q.arg1), out);
                } else {
                    out << "\tlw " << REG_NAMES[r1] << ", " << getOffset(q.arg1) << "($sp)" << endl;
                }

                // 2. 处理右操作数 arg2
                int r2 = getReg(q.arg2); 
                if (isNumber(q.arg2)) {
                    emitImm(r2, stoi(q.arg2), out);
                } else {
                    out << "\tlw " << REG_NAMES[r2] << ", " << getOffset(q.arg2) << "($sp)" << endl;
                }

                // 3. 准备结果寄存器 r3
                if (varInReg.count(q.result)) {
                    regContent[varInReg[q.result]] = "";
                    varInReg.erase(q.result);
                }
                int r3 = getReg(q.result);

                // 4. 根据操作符生成对应 MIPS 指令
                if (q.op == OP_ADD) {
                    out << "\tadd " << REG_NAMES[r3] << ", " << REG_NAMES[r1] << ", " << REG_NAMES[r2] << endl;
                }
                else if (q.op == OP_SUB) {
                    out << "\tsub " << REG_NAMES[r3] << ", " << REG_NAMES[r1] << ", " << REG_NAMES[r2] << endl;
                }
                else if (q.op == OP_MUL) {
                    // MIPS 乘法结果存放在 HI/LO 寄存器，mflo 取出
                    out << "\tmult " << REG_NAMES[r1] << ", " << REG_NAMES[r2] << endl;
                    out << "\tmflo " << REG_NAMES[r3] << endl;
                }
                else if (q.op == OP_DIV) {
                    // MIPS 除法，mflo 获取商
                    out << "\tdiv " << REG_NAMES[r1] << ", " << REG_NAMES[r2] << endl;
                    out << "\tmflo " << REG_NAMES[r3] << endl;
                }
                
                // 5. 写穿（Write-Through）策略：结果立即存回栈，防止溢出丢失
                out << "\tsw " << REG_NAMES[r3] << ", " << getOffset(q.result) << "($sp)" << endl;
                break;
            }

            case OP_ASSIGN: { // 赋值语句：result = arg1
                int r1 = getReg(q.arg1);
                if (isNumber(q.arg1)) {
                    emitImm(r1, stoi(q.arg1), out);
                } else {
                    out << "\tlw " << REG_NAMES[r1] << ", " << getOffset(q.arg1) << "($sp)" << endl;
                }
                // 更新寄存器描述符，并将结果存回栈
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

            case OP_JEQ: { // 条件跳转：if (arg1 == arg2) goto result
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
                // 如果有返回值，将其放入 $v0
                if (!q.arg1.empty()) {
                    int r1 = getReg(q.arg1);
                    if (isNumber(q.arg1)) emitImm(r1, stoi(q.arg1), out);
                    else out << "\tlw " << REG_NAMES[r1] << ", " << getOffset(q.arg1) << "($sp)" << endl;
                    out << "\tadd $v0, " << REG_NAMES[r1] << ", $zero" << endl;
                }
                // 简化的程序终止逻辑：死循环
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