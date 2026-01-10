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
    string regContent[32];
    map<string, int> varInReg;

    // 寄存器池
    vector<int> availRegs;

    // 替换策略索引
    int nextVictimIndex; 

    // 辅助函数
    bool isNumber(string s);
    int getOffset(string var); // 获取相对于 SP 的偏移
    
    // 寄存器分配
    int getReg(string var);      
    void spillAll();             
    
    // 输出指令辅助
    void emitImm(int reg, int val, ofstream& out);

public:
    AsmGenerator(const vector<Quad>& codes);
    void generate(string filename);
};

#endif