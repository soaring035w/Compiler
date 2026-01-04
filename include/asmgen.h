#ifndef ASMGEN_H
#define ASMGEN_H

#include "intercode.h"
#include <vector>
#include <string>
#include <map>
#include <set>
#include <fstream>

using namespace std;

class AsmGenerator {
private:
    const vector<Quad>& quads;
    
    // 变量到栈偏移的映射
    map<string, int> stackOffset;
    int currentStackSize;

    // 寄存器描述符: 记录哪个变量在哪个寄存器
    // reg -> varName
    string regContent[32]; 
    // varName -> regIndex (如果不在寄存器则不存在)
    map<string, int> varInReg;

    // 寄存器池：主要使用 $t0-$t9 (8-15, 24-25) 和 $s0-$s7 (16-23)
    // 简单起见，我们将动态分配 $t0-$t9, $s0-$s7
    vector<int> availRegs;

    // 辅助函数
    bool isNumber(string s);
    int getOffset(string var); // 获取栈中位置，如果没有则分配
    
    // 寄存器分配核心逻辑
    int allocateReg(string var); // 为变量分配一个寄存器
    int getReg(string var);      // 获取存放该变量的寄存器(如果没有则加载)
    void spillReg(int regIdx);   // 强制释放某个寄存器(写回内存)
    void spillAll();             // 释放所有寄存器(在跳转/Label处调用)
    
    // 输出指令辅助
    // 因为不能用li，我们需要用 addi reg, $zero, val
    void emitImm(int reg, int val, ofstream& out);

public:
    AsmGenerator(const vector<Quad>& codes);
    void generate(string filename);
};

#endif