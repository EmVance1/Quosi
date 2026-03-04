#ifndef CQUOSI_IMPL_EXPR_H
#define CQUOSI_IMPL_EXPR_H
#include "quosi/ast.h"
#include "ctx.h"

void quosi_internal_parse_expr(quosiParseCtx* ctx, quosiExpr* result);
void quosi_internal_parse_value(quosiParseCtx* ctx, quosiExpr* result);

#endif
