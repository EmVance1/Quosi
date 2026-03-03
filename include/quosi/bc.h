#ifndef CQUOSI_BC_H
#define CQUOSI_BC_H
#include "quosi/quosi.h"
#include <stdint.h>


enum quosiInstr {
    QUOSI_INSTR_EOF = 0,

    QUOSI_INSTR_PUSH,
    QUOSI_INSTR_POP,
    QUOSI_INSTR_DUP,
    QUOSI_INSTR_LOAD,
    QUOSI_INSTR_STORE,

    QUOSI_INSTR_LAND,
    QUOSI_INSTR_LOR,
    QUOSI_INSTR_LNOT,
    QUOSI_INSTR_ADD,
    QUOSI_INSTR_SUB,
    QUOSI_INSTR_MUL,
    QUOSI_INSTR_DIV,
    QUOSI_INSTR_NEG,
    QUOSI_INSTR_EQU,
    QUOSI_INSTR_NEQ,
    QUOSI_INSTR_IEQV,
    QUOSI_INSTR_IEQK,
    QUOSI_INSTR_LEQ,
    QUOSI_INSTR_LTH,
    QUOSI_INSTR_GEQ,
    QUOSI_INSTR_GTH,

    QUOSI_INSTR_JUMP,
    QUOSI_INSTR_JZ,
    QUOSI_INSTR_JNZ,
    QUOSI_INSTR_SWITCH,

    QUOSI_INSTR_PROP,
    QUOSI_INSTR_PICK,
    QUOSI_INSTR_LINE,
    QUOSI_INSTR_EVENT,
};

typedef struct quosiModData {
    quosiStrView name;
    uint32_t entry;
    // vector
    uint8_t* code;
} quosiModData;

typedef struct quosiProgramData {
    // vector
    quosiModData* mods;
    // vector
    uint8_t* strs;
    // vector
    uint8_t* syms;
} quosiProgramData;

quosiProgramData quosi_compile_ast(const struct quosiAst* ast, quosiSymbolCtx ctx, quosiAllocator alloc);
void quosi_program_data_free(quosiProgramData* data, quosiAllocator alloc);


#endif
