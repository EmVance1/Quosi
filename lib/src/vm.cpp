#include "quosi/vm.h"
#include "quosi/bc.h"
#include "num.h"
#include <cstring>


namespace quosi {

void VirtualMachine::enq_iprop(const IProp& prop) {
    text[TH++] = prop;
}

VirtualMachine::IProp VirtualMachine::deq_iprop() {
    return text[TT++];
}

VirtualMachine::Prop VirtualMachine::deq_text() {
    const auto p = deq_iprop();
    return Prop{ (const char*)code + p.s, p.i };
}


void VirtualMachine::reset(const byte* _code) {
    code = _code;
    PC = V_START;
    SP = 0;
    TH = 0;
    TT = 0;
    A  = 0;
    B  = 0;
}


VirtualMachine::UpCall VirtualMachine::step(Context ctx) {
    switch ((bc::Instr)code[PC++]) {
    case bc::Instr::Eof:
        return UpCall::Exit;

    case bc::Instr::Push:
        memcpy(expr + SP++, code + PC, sizeof(u64));
        PC += sizeof(u64);
        break;
    case bc::Instr::Pop:
        --SP;
        break;

    case bc::Instr::Load: {
        u32 k;
        memcpy(&k, code + PC, sizeof(u32));
        PC += sizeof(u32);
        expr[SP++] = *ctx(k, false);
        break; }
    case bc::Instr::Store: {
        u32 k;
        memcpy(&k, code + PC, sizeof(u32));
        PC += sizeof(u32);
        *ctx(k, false) = expr[SP--];
        break; }

    case bc::Instr::Land: {
        const u64 rhs = expr[--SP];
        const u64 lhs = expr[--SP];
        expr[SP++] = (u64)(lhs && rhs);
        break; }
    case bc::Instr::Lor: {
        const u64 rhs = expr[--SP];
        const u64 lhs = expr[--SP];
        expr[SP++] = (u64)(lhs || rhs);
        break; }
    case bc::Instr::Lnot: {
        const u64 lhs = expr[--SP];
        expr[SP++] = (u64)(!lhs);
        break; }
    case bc::Instr::Add: {
        const u64 rhs = expr[--SP];
        const u64 lhs = expr[--SP];
        expr[SP++] = (u64)(lhs + rhs);
        break; }
    case bc::Instr::Sub: {
        const u64 rhs = expr[--SP];
        const u64 lhs = expr[--SP];
        expr[SP++] = (u64)(lhs - rhs);
        break; }
    case bc::Instr::Mul: {
        const u64 rhs = expr[--SP];
        const u64 lhs = expr[--SP];
        expr[SP++] = (u64)(lhs * rhs);
        break; }
    case bc::Instr::Div: {
        const u64 rhs = expr[--SP];
        const u64 lhs = expr[--SP];
        expr[SP++] = (u64)(lhs / rhs);
        break; }
    case bc::Instr::Neg: {
        const i64 lhs = (i64)expr[--SP];
        expr[SP++] = (u64)(-lhs);
        break; }
    case bc::Instr::Equ: {
        const u64 rhs = expr[--SP];
        const u64 lhs = expr[--SP];
        expr[SP++] = (u64)(lhs == rhs);
        break; }
    case bc::Instr::Neq: {
        const u64 rhs = expr[--SP];
        const u64 lhs = expr[--SP];
        expr[SP++] = (u64)(lhs != rhs);
        break; }
    case bc::Instr::IeqV: {
        const u64 lhs = expr[SP-1];
        u64 rhs;
        memcpy(&rhs, code + PC, sizeof(u64));
        expr[SP++] = (u64)(lhs == rhs);
        PC += sizeof(u64);
        break; }
    case bc::Instr::IeqK: {
        const u64 lhs = expr[SP-1];
        u32 rhs;
        memcpy(&rhs, code + PC, sizeof(u32));
        expr[SP++] = (u64)(lhs == *ctx(rhs, false));
        PC += sizeof(u32);
        break; }
    case bc::Instr::Leq: {
        const u64 rhs = expr[--SP];
        const u64 lhs = expr[--SP];
        expr[SP++] = (u64)(lhs <= rhs);
        break; }
    case bc::Instr::Lth: {
        const u64 rhs = expr[--SP];
        const u64 lhs = expr[--SP];
        expr[SP++] = (u64)(lhs < rhs);
        break; }
    case bc::Instr::Geq: {
        const u64 rhs = expr[--SP];
        const u64 lhs = expr[--SP];
        expr[SP++] = (u64)(lhs >= rhs);
        break; }
    case bc::Instr::Gth: {
        const u64 rhs = expr[--SP];
        const u64 lhs = expr[--SP];
        expr[SP++] = (u64)(lhs > rhs);
        break; }
    case bc::Instr::Jump:
        memcpy(&PC, code + PC, sizeof(u32));
        if (PC == V_END)   return UpCall::Exit;
        if (PC == V_ABORT) return UpCall::Abort;
        break;
    case bc::Instr::Jz:
        if (expr[--SP] == 0) {
            memcpy(&PC, code + PC, sizeof(u32));
            if (PC == V_END)   return UpCall::Exit;
            if (PC == V_ABORT) return UpCall::Abort;
        } else {
            PC += sizeof(u32);
        }
        break;
    case bc::Instr::Jnz:
        if (expr[--SP] != 0) {
            memcpy(&PC, code + PC, sizeof(u32));
            if (PC == V_END)   return UpCall::Exit;
            if (PC == V_ABORT) return UpCall::Abort;
        } else {
            PC += sizeof(u32);
        }
        break;
    case bc::Instr::Switch: {
        const u32 off = (u32)expr[--SP] * sizeof(u32);
        memcpy(&PC, code + PC + off, sizeof(u32));
        if (PC == V_END)   return UpCall::Exit;
        if (PC == V_ABORT) return UpCall::Abort;
        break; }

    case bc::Instr::Prop: {
        IProp p;
        memcpy(&p, code + PC, sizeof(u32) + sizeof(u8));
        PC += sizeof(u32) + sizeof(u8);
        enq_iprop(p);
        break; }
    /*
    case bc::Instr::CProp:
        if (expr[--SP] != 0) {
            IProp p;
            memcpy(&p, code + PC, sizeof(u32) + sizeof(u8));
            PC += sizeof(u32) + sizeof(u8);
            enq_iprop(p);
        }
        break;
    */

    case bc::Instr::Pick:
        B = TH;
        return UpCall::Pick;
    case bc::Instr::Line:
        memcpy(&A, code + PC, sizeof(u32));
        PC += sizeof(u32);
        memcpy(&B, code + PC, sizeof(u32));
        PC += sizeof(u32);
        return UpCall::Line;
    case bc::Instr::Event: {
        memcpy(&B, code + PC, sizeof(u32));
        PC += sizeof(u32);
        return UpCall::Event;
        break; }
    }
    return UpCall::NONE;
}

VirtualMachine::UpCall VirtualMachine::exec(Context ctx) {
    TH = 0;
    TT = 0;
    UpCall s = step(ctx);
    while (s == UpCall::NONE) s = step(ctx);
    return s;
}


}
