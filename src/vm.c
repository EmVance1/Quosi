#include "quosi/quosi.h"
#include "quosi/vm.h"
#include "quosi/bc.h"
#include <string.h>
#include <stdbool.h>


typedef struct _InternalProp {
    uint32_t pos;
    uint8_t idx;
} _InternalProp;
static void _vm_enq_iprop(quosiVm* self, _InternalProp p) {
    self->text[self->TH++] = (quosiProposition){ (const char*)self->strs + p.pos, p.idx };
}
static int _vm_step(quosiVm* self, quosiVmCtx ctx);


void quosi_vm_init(quosiVm* self, const quosiFile* file, const char* module) {
    self->base = (const uint8_t*)file;
    quosiFileModTableEntry entry = quosi_file_module(file, module);
    self->code = entry.code;
    self->strs = quosi_file_strs(file);
    self->PC = entry.entry;
    self->SP = 0;
    self->TH = 0;
    self->TT = 0;
    self->A  = 0;
    self->B  = 0;
}

const char* quosi_vm_line(const quosiVm* self) { return (const char*)self->strs + self->B; }
uint32_t    quosi_vm_id(const quosiVm* self) { return self->A; }
uint32_t    quosi_vm_nq(const quosiVm* self) { return self->B; }

void             quosi_vm_push_value(quosiVm* self, uint64_t val) { self->stack[(self->SP)++] = val; }
uint64_t         quosi_vm_pop_value(quosiVm* self)       { return self->stack[--(self->SP)]; }
uint64_t         quosi_vm_top_value(const quosiVm* self) { return self->stack[self->SP-1]; }
quosiProposition quosi_vm_dequeue_text(quosiVm* self)    { return self->text[(self->TT)++]; }

int quosi_vm_exec(quosiVm* self, quosiVmCtx ctx) {
    self->TH = 0;
    self->TT = 0;
    int s = _vm_step(self, ctx);
    while (s == QUOSI_UPCALL_NONE) s = _vm_step(self, ctx);
    return s;
}


static int _vm_step(quosiVm* self, quosiVmCtx ctx) {
    switch (self->code[self->PC++]) {
    case QUOSI_INSTR_EOF:
        return QUOSI_UPCALL_EXIT;

    case QUOSI_INSTR_PUSH:
        memcpy(self->stack + self->SP++, self->code + self->PC, sizeof(uint64_t));
        self->PC += sizeof(uint64_t);
        break;
    case QUOSI_INSTR_POP:
        --self->SP;
        break;
    case QUOSI_INSTR_DUP: {
        const uint64_t val = self->stack[self->SP-1];
        self->stack[self->SP++] = val;
        break; }

    case QUOSI_INSTR_LOAD: {
        uint32_t k;
        memcpy(&k, self->code + self->PC, sizeof(uint32_t));
        self->stack[self->SP++] = *ctx(k);
        self->PC += sizeof(uint32_t);
        break; }
    case QUOSI_INSTR_STORE: {
        uint32_t k;
        memcpy(&k, self->code + self->PC, sizeof(uint32_t));
        *ctx(k) = self->stack[--self->SP];
        self->PC += sizeof(uint32_t);
        break; }

    case QUOSI_INSTR_LAND: {
        const uint64_t rhs = self->stack[--self->SP];
        const uint64_t lhs = self->stack[--self->SP];
        self->stack[self->SP++] = (uint64_t)(lhs && rhs);
        break; }
    case QUOSI_INSTR_LOR: {
        const uint64_t rhs = self->stack[--self->SP];
        const uint64_t lhs = self->stack[--self->SP];
        self->stack[self->SP++] = (uint64_t)(lhs || rhs);
        break; }
    case QUOSI_INSTR_LNOT: {
        const uint64_t lhs = self->stack[--self->SP];
        self->stack[self->SP++] = (uint64_t)(!lhs);
        break; }
    case QUOSI_INSTR_ADD: {
        const uint64_t rhs = self->stack[--self->SP];
        const uint64_t lhs = self->stack[--self->SP];
        self->stack[self->SP++] = (uint64_t)(lhs + rhs);
        break; }
    case QUOSI_INSTR_SUB: {
        const uint64_t rhs = self->stack[--self->SP];
        const uint64_t lhs = self->stack[--self->SP];
        self->stack[self->SP++] = (uint64_t)(lhs - rhs);
        break; }
    case QUOSI_INSTR_MUL: {
        const uint64_t rhs = self->stack[--self->SP];
        const uint64_t lhs = self->stack[--self->SP];
        self->stack[self->SP++] = (uint64_t)(lhs * rhs);
        break; }
    case QUOSI_INSTR_DIV: {
        const uint64_t rhs = self->stack[--self->SP];
        const uint64_t lhs = self->stack[--self->SP];
        self->stack[self->SP++] = (uint64_t)(lhs / rhs);
        break; }
    case QUOSI_INSTR_NEG: {
        const int64_t lhs = (int64_t)self->stack[--self->SP];
        self->stack[self->SP++] = (uint64_t)(-lhs);
        break; }
    case QUOSI_INSTR_EQU: {
        const uint64_t rhs = self->stack[--self->SP];
        const uint64_t lhs = self->stack[--self->SP];
        self->stack[self->SP++] = (uint64_t)(lhs == rhs);
        break; }
    case QUOSI_INSTR_NEQ: {
        const uint64_t rhs = self->stack[--self->SP];
        const uint64_t lhs = self->stack[--self->SP];
        self->stack[self->SP++] = (uint64_t)(lhs != rhs);
        break; }
    case QUOSI_INSTR_IEQV: {
        const uint64_t lhs = self->stack[self->SP-1];
        uint64_t rhs;
        memcpy(&rhs, self->code + self->PC, sizeof(uint64_t));
        self->stack[self->SP++] = (uint64_t)(lhs == rhs);
        self->PC += sizeof(uint64_t);
        break; }
    case QUOSI_INSTR_IEQK: {
        const uint64_t lhs = self->stack[self->SP-1];
        uint32_t rhs;
        memcpy(&rhs, self->code + self->PC, sizeof(uint32_t));
        self->stack[self->SP++] = (uint64_t)(lhs == *ctx(rhs));
        self->PC += sizeof(uint32_t);
        break; }
    case QUOSI_INSTR_LEQ: {
        const uint64_t rhs = self->stack[--self->SP];
        const uint64_t lhs = self->stack[--self->SP];
        self->stack[self->SP++] = (uint64_t)(lhs <= rhs);
        break; }
    case QUOSI_INSTR_LTH: {
        const uint64_t rhs = self->stack[--self->SP];
        const uint64_t lhs = self->stack[--self->SP];
        self->stack[self->SP++] = (uint64_t)(lhs < rhs);
        break; }
    case QUOSI_INSTR_GEQ: {
        const uint64_t rhs = self->stack[--self->SP];
        const uint64_t lhs = self->stack[--self->SP];
        self->stack[self->SP++] = (uint64_t)(lhs >= rhs);
        break; }
    case QUOSI_INSTR_GTH: {
        const uint64_t rhs = self->stack[--self->SP];
        const uint64_t lhs = self->stack[--self->SP];
        self->stack[self->SP++] = (uint64_t)(lhs > rhs);
        break; }
    case QUOSI_INSTR_JUMP:
        memcpy(&self->PC, self->code + self->PC, sizeof(uint32_t));
        if (self->PC == QUOSI_VERTEX_EXIT) return QUOSI_UPCALL_EXIT;
        break;
    case QUOSI_INSTR_JZ:
        if (self->stack[--self->SP] == 0) {
            memcpy(&self->PC, self->code + self->PC, sizeof(uint32_t));
            if (self->PC == QUOSI_VERTEX_EXIT) return QUOSI_UPCALL_EXIT;
        } else {
            self->PC += sizeof(uint32_t);
        }
        break;
    case QUOSI_INSTR_JNZ:
        if (self->stack[--self->SP] != 0) {
            memcpy(&self->PC, self->code + self->PC, sizeof(uint32_t));
            if (self->PC == QUOSI_VERTEX_EXIT) return QUOSI_UPCALL_EXIT;
        } else {
            self->PC += sizeof(uint32_t);
        }
        break;
    case QUOSI_INSTR_SWITCH: {
        const uint32_t off = (uint32_t)self->stack[--self->SP] * sizeof(uint32_t);
        memcpy(&self->PC, self->code + self->PC + off, sizeof(uint32_t));
        if (self->PC == QUOSI_VERTEX_EXIT) return QUOSI_UPCALL_EXIT;
        break; }

    case QUOSI_INSTR_PROP: {
        _InternalProp p;
        memcpy(&p, self->code + self->PC, sizeof(uint32_t) + sizeof(uint8_t));
        self->PC += sizeof(uint32_t) + sizeof(uint8_t);
        _vm_enq_iprop(self, p);
        break; }

    case QUOSI_INSTR_PICK:
        self->B = self->TH;
        return QUOSI_UPCALL_PICK;
    case QUOSI_INSTR_LINE:
        memcpy(&self->A, self->code + self->PC, sizeof(uint32_t));
        self->PC += sizeof(uint32_t);
        memcpy(&self->B, self->code + self->PC, sizeof(uint32_t));
        self->PC += sizeof(uint32_t);
        return QUOSI_UPCALL_LINE;
    case QUOSI_INSTR_EVENT:
        memcpy(&self->B, self->code + self->PC, sizeof(uint32_t));
        self->PC += sizeof(uint32_t);
        return QUOSI_UPCALL_EVENT;
    }
    return (self->SP > 128) ? QUOSI_UPCALL_ABORT : QUOSI_UPCALL_NONE;
}

