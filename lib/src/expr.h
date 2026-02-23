#pragma once
#include "quosi/ast.h"
#include "ctx.h"


namespace quosi::ast {

void parse_expr(ParseContext& ctx, Expr& result);
void parse_value(ParseContext& ctx, Expr& result);

}
