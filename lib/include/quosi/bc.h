#pragma once
#include "quosi/quosi.h"
#include <cstdint>

namespace quosi::ast { struct Ast; }

namespace quosi::bc {

enum class Instr : uint8_t {
    Eof = 0,

    Push,
    Pop,
    Load,
    Store,

    Land,
    Lor,
    Lnot,
    Add,
    Sub,
    Mul,
    Div,
    Neg,
    Equ,
    Neq,
    IeqV,
    IeqK,
    Leq,
    Lth,
    Geq,
    Gth,

    Jump,
    Jz,
    Jnz,
    Switch,

    Prop,
    Pick,
    Line,
    Event,
};

struct ProgramData {
    File::Header header;
    uint8_t* data;

    ~ProgramData();
};
ProgramData compile_ast(const ast::Ast& ast, SymbolContext ctx = nullptr);
void prettyprint(const uint8_t* code, size_t size);

}
