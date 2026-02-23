#include "quosi/ast.h"
#include "quosi/bc.h"
#include "ctx.h"
#include "lex.h"
#include "num.h"
#include <charconv>


namespace quosi::ast {

using namespace bc;

struct Assoc { float lhs; float rhs; };

static Assoc binding(const Token& t) {
    switch (t.type) {
    case Token::Type::LogOr:
        return { 1.1f, 1.f };
    case Token::Type::LogAnd:
        return { 2.1f, 2.f };
    // case Token::Type::LogNot:
    //     return { 3.1f, 3.f };
    case Token::Type::Equ: case Token::Type::Neq:
        return { 4.1f, 4.f };
    case Token::Type::Lth: case Token::Type::Gth: case Token::Type::Leq: case Token::Type::Geq:
        return { 5.1f, 5.f };
    case Token::Type::Add: case Token::Type::Sub:
        return { 6.1f, 6.f };
    case Token::Type::Mul: case Token::Type::Div:
        return { 7.1f, 7.f };

    default:
        return { 0, 0 };
    }
}
static Instr opcode(const Token& t) {
    switch (t.type) {
    case Token::Type::LogOr:  return Instr::Lor;
    case Token::Type::LogAnd: return Instr::Land;
    // case Token::Type::LogNot: return Instr::Lnot;
    case Token::Type::Equ:    return Instr::Equ;
    case Token::Type::Neq:    return Instr::Neq;
    case Token::Type::Lth:    return Instr::Lth;
    case Token::Type::Gth:    return Instr::Gth;
    case Token::Type::Leq:    return Instr::Leq;
    case Token::Type::Geq:    return Instr::Geq;
    case Token::Type::Add:    return Instr::Add;
    case Token::Type::Sub:    return Instr::Sub;
    case Token::Type::Mul:    return Instr::Mul;
    case Token::Type::Div:    return Instr::Div;
    default: return Instr::Eof;
    }
}


#define SOFT_FAIL(tok, E) do { ctx.errors->list.push_back({ Error::Type::E, tok.span }); } while (0)
#define CRIT_FAIL(tok, E) do { ctx.errors->list.push_back({ Error::Type::E, tok.span }); ctx.errors->fail = true; return; } while (0)
#define SOFT_CHECK(tok, T, E) do { if (tok.type != Token::Type::T) { SOFT_FAIL(tok, E); } } while (0)
#define CRIT_CHECK(tok, T, E) do { if (tok.type != Token::Type::T) { CRIT_FAIL(tok, E); } } while (0)
#define PROP() do { if (ctx.errors->fail) { return; } } while (0)


static void parse_expr_impl(ParseContext& ctx, Expr& result, float minbp, int l);

void parse_expr(ParseContext& ctx, Expr& result) {
    parse_expr_impl(ctx, result, 0.f, 0);
}
void parse_value(ParseContext& ctx, Expr& result) {
    auto n = ctx.tokens.next();
    switch (n.type) {
    case Token::Type::Ident:
        result.value = std::pmr::string(n.value, ctx.arena);
        break;

    case Token::Type::Keyword:
        if (n.value == "true") {
            result.value = (u64)1;
        } else if (n.value == "false") {
            result.value = (u64)0;
        } else {
            CRIT_FAIL(n, Unknown);
        }
        break;

     case Token::Type::Number: {
        u64 val;
        auto temp = std::from_chars(n.value.data(), n.value.data() + n.value.size(), val);
        if (temp.ec == std::errc::invalid_argument) CRIT_FAIL(n, Unknown);
        result.value = val;
        break; }

    default:
        CRIT_FAIL(n, Unknown);
        break;
    }
    return;
}

static void parse_expr_impl(ParseContext& ctx, Expr& result, float minbp, int l) {
    auto n = ctx.tokens.peek();
    switch (n.type) {
    case Token::Type::OpenParen:
        ctx.tokens.next();
        parse_expr_impl(ctx, result, 0, l + 1);
        n = ctx.tokens.next();
        CRIT_CHECK(n, CloseParen, Unknown);
        if (l == 0) return;
        break;

    case Token::Type::Ident: case Token::Type::Keyword: case Token::Type::Number:
        parse_value(ctx, result);
        break;

    default:
        CRIT_FAIL(n, Unknown);
        break;
    }

    while (true) {
        const auto op = ctx.tokens.peek();
        if (op.type == Token::Type::CloseParen) break;
        const auto [lbp, rbp] = binding(op);
        if (lbp < minbp) break;
        ctx.tokens.next();
        auto children = std::pmr::vector<Expr>(ctx.arena);
        children.emplace_back(result);
        auto& rhs = children.emplace_back();
        parse_expr_impl(ctx, rhs, rbp, l);
        result.children = std::move(children);
        result.value = opcode(op);
    }

    return;
}

}
