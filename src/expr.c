#include "cquosi/ast.h"
#include "cquosi/bc.h"
#include "ctx.h"
#include "lex.h"
#include <string.h>
#include <stdio.h>
#define QUOSIDS_ALLOCATOR (ctx->alloc)
#include "vec.h"


struct Assoc { float lhs; float rhs; };

static struct Assoc binding(quosiToken t) {
    switch (t.type) {
    case QUOSI_TOKEN_LOGOR:
        return (struct Assoc){ 1.1f, 1.f };
    case QUOSI_TOKEN_LOGAND:
        return (struct Assoc){ 2.1f, 2.f };
    case QUOSI_TOKEN_EQU: case QUOSI_TOKEN_NEQ:
        return (struct Assoc){ 3.1f, 3.f };
    case QUOSI_TOKEN_LTH: case QUOSI_TOKEN_GTH: case QUOSI_TOKEN_LEQ: case QUOSI_TOKEN_GEQ:
        return (struct Assoc){ 4.1f, 4.f };
    case QUOSI_TOKEN_ADD: case QUOSI_TOKEN_SUB:
        return (struct Assoc){ 5.1f, 5.f };
    case QUOSI_TOKEN_MUL: case QUOSI_TOKEN_DIV:
        return (struct Assoc){ 6.1f, 6.f };
    case QUOSI_TOKEN_COLON:
        return (struct Assoc){ 7.1f, 7.f };
    case QUOSI_TOKEN_LOGNOT:
        return (struct Assoc){ 0.f, 8.f };

    default:
        return (struct Assoc){ 0, 0 };
    }
}
static uint8_t opcode(quosiToken t) {
    switch (t.type) {
    case QUOSI_TOKEN_LOGOR:  return QUOSI_INSTR_LOR;
    case QUOSI_TOKEN_LOGAND: return QUOSI_INSTR_LAND;
    case QUOSI_TOKEN_LOGNOT: return QUOSI_INSTR_LNOT;
    case QUOSI_TOKEN_EQU:    return QUOSI_INSTR_EQU;
    case QUOSI_TOKEN_NEQ:    return QUOSI_INSTR_NEQ;
    case QUOSI_TOKEN_LTH:    return QUOSI_INSTR_LTH;
    case QUOSI_TOKEN_GTH:    return QUOSI_INSTR_GTH;
    case QUOSI_TOKEN_LEQ:    return QUOSI_INSTR_LEQ;
    case QUOSI_TOKEN_GEQ:    return QUOSI_INSTR_GEQ;
    case QUOSI_TOKEN_ADD:    return QUOSI_INSTR_ADD;
    case QUOSI_TOKEN_SUB:    return QUOSI_INSTR_SUB;
    case QUOSI_TOKEN_MUL:    return QUOSI_INSTR_MUL;
    case QUOSI_TOKEN_DIV:    return QUOSI_INSTR_DIV;
    case QUOSI_TOKEN_COLON:  return QUOSI_INSTR_STORE;
    default: return QUOSI_INSTR_EOF;
    }
}
static uint64_t toconstant(quosiStrView s) {
    uint64_t acc = 0;
    for (size_t i = 0; i < s.len; i++) {
        acc *= 10;
        acc += (uint64_t)(s.ptr[i] - '0');
    }
    return acc;
}

#define NEXT(stream) quosi_token_stream_next(stream)
#define PEEK(stream) quosi_token_stream_peek(stream)
#define ALLOC(T) (T*)quosi_allocator_allocate(ctx->alloc, sizeof(T))
#define STREQ(view, cptr) ((view.len == sizeof(cptr)-1) && (strncmp(view.ptr, cptr, view.len) == 0))

#define EH_FAIL(tok, E) do { quosids_arrpush(ctx->errors->list, ((quosiErrorValue){ .type=QUOSI_ERR_##E, .span=tok.span })); \
    if (quosi_error_is_critical(quosids_arrlast(ctx->errors->list))) { ctx->errors->critical = true; return; } } while (0)
#define EH_CHECK(tok, T, E) do { if (tok.type != QUOSI_TOKEN_##T) { EH_FAIL(tok, E); } } while (0)
#define EH_PROP() do { if (ctx->errors->critical) return; } while (0)


static void parse_expr_impl(quosiParseCtx* ctx, quosiExpr* result, float minbp, int l);

void quosi_internal_parse_expr(quosiParseCtx* ctx, quosiExpr* result) {
    parse_expr_impl(ctx, result, 0.f, 0);
}
void quosi_internal_parse_value(quosiParseCtx* ctx, quosiExpr* result) {
    quosiToken n = NEXT(&ctx->tokens);
    switch (n.type) {
    case QUOSI_TOKEN_IDENT:
        result->value.ident = n.value;
        result->tag = QUOSI_EXPR_IDENT;
        break;

    case QUOSI_TOKEN_KEYWORD:
        if (STREQ(n.value, "true")) {
            result->value.imm = (uint64_t)1;
            result->tag = QUOSI_EXPR_IMM;
        } else if (STREQ(n.value, "false")) {
            result->value.imm = (uint64_t)0;
            result->tag = QUOSI_EXPR_IMM;
        } else {
            EH_FAIL(n, UNKNOWN);
        }
        break;

     case QUOSI_TOKEN_NUMBER:
        result->value.imm = toconstant(n.value);
        result->tag = QUOSI_EXPR_IMM;
        break;

    default:
        EH_FAIL(n, UNKNOWN);
        break;
    }
    return;
}

static void parse_expr_impl(quosiParseCtx* ctx, quosiExpr* result, float minbp, int l) {
    quosiToken n = PEEK(&ctx->tokens);
    switch (n.type) {
    case QUOSI_TOKEN_OPENPAREN:
        NEXT(&ctx->tokens);
        parse_expr_impl(ctx, result, 0, l + 1);
        EH_PROP();
        n = NEXT(&ctx->tokens);
        EH_CHECK(n, CLOSEPAREN, UNKNOWN);
        if (l == 0) return;
        break;

    case QUOSI_TOKEN_LOGNOT: {
        NEXT(&ctx->tokens);
        result->lhs = ALLOC(quosiExpr);
        parse_expr_impl(ctx, result->lhs, 0, l);
        EH_PROP();
        result->value.op = QUOSI_INSTR_LNOT;
        result->tag = QUOSI_EXPR_OP;
        break; }

    case QUOSI_TOKEN_IDENT: case QUOSI_TOKEN_KEYWORD: case QUOSI_TOKEN_NUMBER:
        quosi_internal_parse_value(ctx, result);
        EH_PROP();
        break;

    default:
        EH_FAIL(n, UNKNOWN);
        break;
    }

    while (true) {
        const quosiToken op = PEEK(&ctx->tokens);
        if (op.type == QUOSI_TOKEN_COMMA) break;
        if (op.type == QUOSI_TOKEN_CLOSEPAREN) break;
        const struct Assoc bindstr = binding(op);
        if (bindstr.lhs < minbp) break;
        NEXT(&ctx->tokens);
        quosiExpr* lhs = ALLOC(quosiExpr);
        *lhs = *result;
        result->lhs = lhs;
        result->rhs = ALLOC(quosiExpr);
        parse_expr_impl(ctx, result->rhs, bindstr.rhs, l);
        result->value.op = opcode(op);
        result->tag = QUOSI_EXPR_OP;
        if (PEEK(&ctx->tokens).type == QUOSI_TOKEN_COMMA) break;
        EH_PROP();
    }

    return;
}

