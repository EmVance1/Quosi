#ifndef CQUOSI_INTERPRETER_H
#define CQUOSI_INTERPRETER_H
#include "cquosi/quosi.h"
#include <stdint.h>


enum quosiUpcall {
    QUOSI_UPCALL_NONE = 0,
    QUOSI_UPCALL_LINE,
    QUOSI_UPCALL_PICK,
    QUOSI_UPCALL_EVENT,
    QUOSI_UPCALL_EXIT,
    QUOSI_UPCALL_ABORT,
};
typedef uint64_t*(*quosiVmCtx)(uint32_t key);
typedef struct quosiProposition {
    const char* str;
    uint8_t idx;
} quosiProposition;

#ifndef QUOSI_PROP_QUEUE_SIZE
#define QUOSI_PROP_QUEUE_SIZE 16
#endif

#ifndef QUOSI_VALUE_STACK_SIZE
#define QUOSI_VALUE_STACK_SIZE 128
#endif

#define QUOSI_VERTEX_START 0
#define QUOSI_VERTEX_EXIT  UINT32_MAX

typedef struct quosiVm {
    quosiProposition text[QUOSI_PROP_QUEUE_SIZE];
    uint64_t stack[QUOSI_VALUE_STACK_SIZE];
    uint32_t PC, SP;
    uint32_t TH, TT;
    uint32_t A,  B;
    const uint8_t* base;
    const uint8_t* code;
    const uint8_t* strs;
} quosiVm;

void quosi_vm_init(quosiVm* self, const quosiFile* file, const char* module);

const char* quosi_vm_line(const quosiVm* self);
uint32_t    quosi_vm_id(const quosiVm* self);
uint32_t    quosi_vm_nq(const quosiVm* self);

void             quosi_vm_push_value(quosiVm* self, uint64_t val);
uint64_t         quosi_vm_pop_value(quosiVm* self);
uint64_t         quosi_vm_top_value(const quosiVm* self);
quosiProposition quosi_vm_dequeue_text(quosiVm* self);

int quosi_vm_exec(quosiVm* self, quosiVmCtx ctx);


#endif
