#ifndef CQUOSI_IMPL_PARSECTX_H
#define CQUOSI_IMPL_PARSECTX_H
#include "quosi/ast.h"
#include "lex.h"


typedef struct quosiParseCtx {
    quosiAllocator alloc;
    quosiTokenStream tokens;
    quosiError* errors;
    // vector
    quosiToken* edges;
} quosiParseCtx;


#endif
