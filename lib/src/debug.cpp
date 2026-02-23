#include "quosi/bc.h"
#include "num.h"
#include <unordered_map>
#include <string>
#include <iostream>
#include <iomanip>


namespace quosi::bc {

static std::ostream& hex4(std::ostream& os) {
    os << std::hex
       << std::uppercase
       << std::setw(4)
       << std::setfill('0');
    return os;
}
#define contains_key(map, key) (map.find(key) != map.end())


void prettyprint(const byte* code, size_t size) {
    u32 PC = 0;
    u8  a1 = 0;
    u32 a2 = 0;
    u64 a3 = 0;
    u32 li = 0;
    u32 skip = 0;
    std::unordered_map<u32, std::string> jumps;
    jumps[0] = "<START>";
    jumps[UINT32_MAX] = "<EXIT>";
    jumps[UINT32_MAX-1] = "<ABORT>";
    std::cout << std::hex;
    while (PC < size) {
        switch ((bc::Instr)code[PC++]) {
        case bc::Instr::Jump:
            memcpy(&a2, code + PC, sizeof(u32));
            if (!contains_key(jumps, a2)) jumps[a2] = ".L" + std::to_string(li++);
            PC += sizeof(u32);
            break;
        case bc::Instr::Jz:
            memcpy(&a2, code + PC, sizeof(u32));
            if (!contains_key(jumps, a2)) jumps[a2] = ".L" + std::to_string(li++);
            PC += sizeof(u32);
            break;
        case bc::Instr::Jnz:
            memcpy(&a2, code + PC, sizeof(u32));
            if (!contains_key(jumps, a2)) jumps[a2] = ".L" + std::to_string(li++);
            PC += sizeof(u32);
            break;
        case bc::Instr::Switch:
            for (u32 i = 0; i < skip; i++) {
                memcpy(&a2, code + PC, sizeof(u32));
                if (!contains_key(jumps, a2)) jumps[a2] = ".L" + std::to_string(li++);
                PC += sizeof(u32);
            }
            skip = 0;
            break;
        case bc::Instr::Prop:
            PC += sizeof(u32) + sizeof(u8);
            skip++;
            break;
        default:
            break;
        }
    }
    PC = 0;
    skip = 0;
    while (PC < size) {
        if (contains_key(jumps, PC)) {
            std::cout << "    " << jumps.at(PC) << ":\n";
        }
        switch ((bc::Instr)code[PC++]) {
        case bc::Instr::Eof:
            std::cout << std::dec;
            return;

        case bc::Instr::Push:
            memcpy(&a3, code + PC, sizeof(u64));
            std::cout << "0x" << hex4 << PC-1 << "    PUSH $" << std::dec << a3 << "\n";
            PC += sizeof(u64);
            break;
        case bc::Instr::Pop:
            std::cout << "0x" << hex4 << PC-1 << "    POP\n";
            break;

        case bc::Instr::Load:
            memcpy(&a2, code + PC, sizeof(u32));
            std::cout << "0x" << hex4 << PC-1 << "    LOAD @" << std::dec << a2 << "\n";
            PC += sizeof(u32);
            break;
        case bc::Instr::Store:
            memcpy(&a2, code + PC, sizeof(u32));
            std::cout << "0x" << hex4 << PC-1 << "    STORE @" << std::dec << a2 << "\n";
            PC += sizeof(u32);
            break;

        case bc::Instr::Land:
            std::cout << "0x" << hex4 << PC-1 << "    LAND\n";
            break;
        case bc::Instr::Lor:
            std::cout << "0x" << hex4 << PC-1 << "    LOR\n";
            break;
        case bc::Instr::Lnot:
            std::cout << "0x" << hex4 << PC-1 << "    LNOT\n";
            break;
        case bc::Instr::Add:
            std::cout << "0x" << hex4 << PC-1 << "    ADD\n";
            break;
        case bc::Instr::Sub:
            std::cout << "0x" << hex4 << PC-1 << "    SUB\n";
            break;
        case bc::Instr::Mul:
            std::cout << "0x" << hex4 << PC-1 << "    MUL\n";
            break;
        case bc::Instr::Div:
            std::cout << "0x" << hex4 << PC-1 << "    DIV\n";
            break;
        case bc::Instr::Neg:
            std::cout << "0x" << hex4 << PC-1 << "    NEG\n";
            break;
        case bc::Instr::Equ:
            std::cout << "0x" << hex4 << PC-1 << "    EQU\n";
            break;
        case bc::Instr::Neq:
            std::cout << "0x" << hex4 << PC-1 << "    NEQ\n";
            break;
        case bc::Instr::IeqV:
            memcpy(&a3, code + PC, sizeof(u64));
            std::cout << "0x" << hex4 << PC-1 << "    IEQ  $" << std::dec << a3 << "\n";
            PC += sizeof(u64);
            break;
        case bc::Instr::IeqK:
            memcpy(&a2, code + PC, sizeof(u32));
            std::cout << "0x" << hex4 << PC-1 << "    IEQ  " << std::dec << a2 << "\n";
            PC += sizeof(u32);
            break;
        case bc::Instr::Leq:
            std::cout << "0x" << hex4 << PC-1 << "    LEQ\n";
            break;
        case bc::Instr::Lth:
            std::cout << "0x" << hex4 << PC-1 << "    LTH\n";
            break;
        case bc::Instr::Geq:
            std::cout << "0x" << hex4 << PC-1 << "    GEQ\n";
            break;
        case bc::Instr::Gth:
            std::cout << "0x" << hex4 << PC-1 << "    GTH\n";
            break;
        case bc::Instr::Jump:
            memcpy(&a2, code + PC, sizeof(u32));
            std::cout << "0x" << hex4 << PC-1 << "    JUMP " << jumps.at(a2) << "\n";
            PC += sizeof(u32);
            break;
        case bc::Instr::Jz:
            memcpy(&a2, code + PC, sizeof(u32));
            std::cout << "0x" << hex4 << PC-1 << "    JZ   " << jumps.at(a2) << "\n";
            PC += sizeof(u32);
            break;
        case bc::Instr::Jnz:
            memcpy(&a2, code + PC, sizeof(u32));
            std::cout << "0x" << hex4 << PC-1 << "    JNZ  " << jumps.at(a2) << "\n";
            PC += sizeof(u32);
            break;
        case bc::Instr::Switch:
            std::cout << "0x" << hex4 << PC-1 << "    SWITCH [ ";
            for (u32 i = 0; i < skip; i++) {
                memcpy(&a2, code + PC, sizeof(u32));
                std::cout << jumps.at(a2);
                if (i < skip - 1) {
                    std::cout << ", ";
                }
                PC += sizeof(u32);
            }
            std::cout << " ]\n";
            skip = 0;
            break;

        case bc::Instr::Prop:
            std::cout << "0x" << hex4 << PC-1;
            memcpy(&a2, code + PC, sizeof(u32));
            PC += sizeof(u32);
            memcpy(&a1, code + PC, sizeof(u8));
            PC += sizeof(u8);
            std::cout << "    PROP \"" << (const char*)(code + a2) << "\", " << std::dec << (int)a1 << "\n";
            skip++;
            break;

        case bc::Instr::Pick:
            std::cout << "0x" << hex4 << PC-1 << "    PICK\n";
            break;
        case bc::Instr::Line:
            std::cout << "0x" << hex4 << PC-1;
            memcpy(&a2, code + PC, sizeof(u32));
            PC += sizeof(u32);
            std::cout << "    LINE " << std::dec << a2 << ", \"";
            memcpy(&a2, code + PC, sizeof(u32));
            PC += sizeof(u32);
            std::cout << (const char*)(code + a2) << "\"\n";
            break;
        case bc::Instr::Event:
            memcpy(&a2, code + PC, sizeof(u32));
            std::cout << "0x" << hex4 << PC-1 << "    EVENT \"" << (const char*)(code + a2) << "\"\n";
            PC += sizeof(u32);
            break;
        }
    }

    std::cout << std::dec;
}

}
