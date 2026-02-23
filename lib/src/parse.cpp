#include "quosi/ast.h"
#include "quosi/error.h"
#include "quosi/bc.h"
#include "lex.h"
#include "expr.h"
// #include <iostream>

namespace quosi::ast {

using namespace bc;


#define SOFT_FAIL(tok, E) do { ctx.errors->list.push_back({ Error::Type::E, tok.span }); } while (0)
#define CRIT_FAIL(tok, E) do { ctx.errors->list.push_back({ Error::Type::E, tok.span }); ctx.errors->fail = true; return; } while (0)
#define SOFT_CHECK(tok, T, E) do { if (tok.type != Token::Type::T) { SOFT_FAIL(tok, E); } } while (0)
#define CRIT_CHECK(tok, T, E) do { if (tok.type != Token::Type::T) { CRIT_FAIL(tok, E); } } while (0)
#define PROP() do { if (ctx.errors->fail) return; } while (0)

static void parse_vert(ParseContext& ctx, Vertex& result);
static void parse_vert_if(ParseContext& ctx, VertexBlock::MyIfElse& result);
static void parse_vert_if_body(ParseContext& ctx, VertexBlock& result);
static void parse_vert_match(ParseContext& ctx, VertexBlock::MyMatch& result);
static void parse_edge(ParseContext& ctx, Edge& result);
static void parse_edge_if(ParseContext& ctx, EdgeBlock::MyIfElse& result);
static void parse_edge_if_body(ParseContext& ctx, std::pmr::vector<EdgeBlock>& result, bool top);
static void parse_edge_match(ParseContext& ctx, EdgeBlock::MyMatch& result);
static void parse_effect(ParseContext& ctx, Effect& result);

#define contains_key(map, key) (map.find(key) != map.end())


std::unique_ptr<Graph> parse(const char* src, ErrorList& errors) {
    auto result = std::make_unique<Graph>();
    auto ctx = ParseContext{
        &result->arena,
        TokenStream(src),
        &errors,
        std::pmr::vector<Token>(&result->arena),
    };
    auto n = ctx.tokens.next();

    while (n.type != Token::Type::Eof) {
        // rename INIT => NEW
        if (n.type == Token::Type::Keyword) {
            const auto init = ctx.tokens.next(); // n = <INIT>
            if (init.type != Token::Type::Ident) {
                ctx.errors->list.push_back({ Error::Type::BadRename, init.span });
                ctx.errors->fail = true;
                return result;
            }
            n = ctx.tokens.next();
            if (n.type != Token::Type::Arrow) {
                ctx.errors->list.push_back({ Error::Type::BadRename, n.span });
                ctx.errors->fail = true;
                return result;
            }
            const auto rend = ctx.tokens.next(); // n = <NEW>
            if (rend.type != Token::Type::Ident) {
                ctx.errors->list.push_back({ Error::Type::BadRename, rend.span });
                ctx.errors->fail = true;
                return result;
            }
            result->rename_table.emplace(std::pmr::string(rend.value, ctx.arena), std::pmr::string(init.value, ctx.arena));
            n = ctx.tokens.next(); // FOLLOW(rename)
            continue;
        }
        // IDENT =
        if (n.type != Token::Type::Ident) {
            ctx.errors->list.push_back({ Error::Type::BadVertexBegin, n.span });
            ctx.errors->fail = true;
            return result;
        }
        const auto name = n;
        n = ctx.tokens.next();
        if (n.type != Token::Type::SetEq) {
            ctx.errors->list.push_back({ Error::Type::MisplacedToken, n.span });
            ctx.errors->fail = true;
            return result;
        }

        auto& vert = result->verts.emplace_back();
        vert.first = std::pmr::string(name.value, ctx.arena);
        if (contains_key(result->vert_names, vert.first)) SOFT_FAIL(name, MultiVertexName);
        result->vert_names.emplace(std::pmr::string(name.value, ctx.arena), result->verts.size() - 1);
        parse_vert_if_body(ctx, vert.second);
        if (ctx.errors->fail) return result;
        n = ctx.tokens.next();
    }

    if (!contains_key(result->vert_names, "START")) SOFT_FAIL(n, NoEntryPoint);
    for (const auto& edge : ctx.edges) {
        if (edge.value == "START" || edge.value == "EXIT") continue;
        const auto e = std::pmr::string(edge.value, ctx.arena);
        if (!contains_key(result->vert_names, e)) {
             SOFT_FAIL(edge, DanglingEdge);
        }
    }

    return result;
}

static void parse_vert(ParseContext& ctx, Vertex& result) {
    result.lines = std::pmr::vector<LineSet>( ctx.arena );
    result.edges = std::pmr::vector<EdgeBlock>( ctx.arena );

    auto n = ctx.tokens.next();
    while (n.type == Token::Type::Lth) {
        // <IDENT:
        auto& line = result.lines.emplace_back();
        line.lines = std::pmr::vector<std::pmr::string>( ctx.arena );
        n = ctx.tokens.next();
        CRIT_CHECK(n, Ident, Unknown);
        line.speaker = std::pmr::string(n.value, ctx.arena);
        n = ctx.tokens.next();
        CRIT_CHECK(n, Colon, Unknown);

        // STRING,... >
        n = ctx.tokens.next();
        while (n.type != Token::Type::Gth) {
            CRIT_CHECK(n, Strlit, Unknown);
            line.lines.emplace_back(std::pmr::string(n.value, ctx.arena));
            n = ctx.tokens.next();
            switch (n.type) {
            case Token::Type::Comma:
                n = ctx.tokens.next();
                break;
            case Token::Type::Gth:
                goto endlines;

            default:
                CRIT_FAIL(n, Unknown);
            }
        }
endlines:
        n = ctx.tokens.next();
    }

    switch (n.type) {
    case Token::Type::Arrow:
        // => IDENT
        n = ctx.tokens.next();
        CRIT_CHECK(n, Ident, Unknown);
        result.next = std::pmr::string(n.value, ctx.arena);
        ctx.edges.push_back(std::move(n));
        break;
    case Token::Type::OpenParen:
        // ( ... )
        result.edges = std::pmr::vector<EdgeBlock>(ctx.arena);
        parse_edge_if_body(ctx, result.edges, true);
        PROP();
        n = ctx.tokens.next();
        CRIT_CHECK(n, CloseParen, Unknown);
        break;

    default:
        CRIT_FAIL(n, Unknown);
    }

    return;
}
static void parse_vert_if(ParseContext& ctx, VertexBlock::MyIfElse& result) {
    auto n = ctx.tokens.next();

    while (n.type != Token::Type::Eof) {
        auto block = &result.conds.emplace_back();

        // if (EXPR) then
        n = ctx.tokens.peek();
        CRIT_CHECK(n, OpenParen, Unknown);
        parse_expr(ctx, block->c);
        PROP();
        n = ctx.tokens.next();
        CRIT_CHECK(n, Keyword, Unknown);
        if (n.value != "then") CRIT_FAIL(n, Unknown);

        block->b = std::pmr::vector<VertexBlock>(ctx.arena);
        parse_vert_if_body(ctx, block->b.emplace_back());
        PROP();

        n = ctx.tokens.next();
        CRIT_CHECK(n, Keyword, Unknown);
        if (n.value == "else") {
            if (ctx.tokens.peek().value == "if") {
                ctx.tokens.next();
                continue;
            } else {
                result.catchall = std::pmr::vector<VertexBlock>(ctx.arena);
                parse_vert_if_body(ctx, result.catchall->emplace_back());
                n = ctx.tokens.next();
                CRIT_CHECK(n, Keyword, Unknown);
                if (n.value != "end") CRIT_FAIL(n, Unknown);
                return;
            }
        } else if (n.value == "end") {
            SOFT_FAIL(n, NoElse);
            return;
        }
    }

    CRIT_FAIL(n, EarlyEof);
}
static void parse_vert_if_body(ParseContext& ctx, VertexBlock& result) {
    auto n = ctx.tokens.peek();
    switch (n.type) {
    case Token::Type::Keyword:
        if (n.value == "match") {
            result.node = VertexBlock::MyMatch();
            parse_vert_match(ctx, std::get<VertexBlock::MyMatch>(result.node));
        } else if (n.value == "if") {
            result.node = VertexBlock::MyIfElse();
            parse_vert_if(ctx, std::get<VertexBlock::MyIfElse>(result.node));
        }
        break;
    case Token::Type::Lth:
        result.node = VertexBlock::MyT();
        parse_vert(ctx, std::get<VertexBlock::MyT>(result.node));
        break;

    default:
        CRIT_FAIL(n, Unknown);
    }

    return;
}
static void parse_vert_match(ParseContext& ctx, VertexBlock::MyMatch& result) {
    auto n = ctx.tokens.next();
    bool has_catchall = false;

    // match (EXPR) with
    n = ctx.tokens.peek();
    CRIT_CHECK(n, OpenParen, Unknown);
    parse_expr(ctx, result.ex);
    PROP();
    n = ctx.tokens.next();
    CRIT_CHECK(n, Keyword, Unknown);
    if (n.value != "with") CRIT_FAIL(n, Unknown);

    n = ctx.tokens.next();
    while (n.type != Token::Type::Eof) {
        switch (n.type) {
        case Token::Type::Keyword:
            if (n.value != "end") CRIT_FAIL(n, Unknown);
            if (!has_catchall)    SOFT_FAIL(n, NoCatchall);
            return;

        case Token::Type::OpenParen: {
            Vertex* arm_vert;
            if (ctx.tokens.peek().type == Token::Type::CatchAll) {
                ctx.tokens.next();
                if (has_catchall) CRIT_FAIL(n, Unknown);
                result.catchall = Vertex();
                arm_vert = &result.catchall.value();
                has_catchall = true;
                n = ctx.tokens.next();
                CRIT_CHECK(n, CloseParen, Unknown);
            } else {
                auto& arm = result.arms.emplace_back();
                parse_value(ctx, arm.c);
                n = ctx.tokens.next();
                CRIT_CHECK(n, CloseParen, Unknown);
                arm_vert = &arm.b;
            }
            PROP();
            n = ctx.tokens.peek();
            CRIT_CHECK(n, Lth, Unknown);
            parse_vert(ctx, *arm_vert);
            PROP();
            break; }

        default:
            CRIT_FAIL(n, Unknown);
            break;
        }
        n = ctx.tokens.next();
    }

    CRIT_FAIL(n, EarlyEof);
}

static void parse_edge(ParseContext& ctx, Edge& result) {
    // STRING :: (EFFECT) => IDENT
    auto n = ctx.tokens.next();
    CRIT_CHECK(n, Strlit, Unknown);
    result.line = std::pmr::string(n.value, ctx.arena);
    n = ctx.tokens.next();
    switch (n.type) {
    case Token::Type::Join:
        result.effect = Effect();
        parse_effect(ctx, *result.effect);
        PROP();
        n = ctx.tokens.next();
        CRIT_CHECK(n, CloseParen, Unknown);
        n = ctx.tokens.next();
        CRIT_CHECK(n, Arrow, Unknown);
        break;

    case Token::Type::Arrow:
        break;

    default:
        CRIT_FAIL(n, Unknown);
    }
    n = ctx.tokens.next();
    CRIT_CHECK(n, Ident, Unknown);
    result.next = std::pmr::string(n.value, ctx.arena);
    ctx.edges.push_back(std::move(n));
}
static void parse_edge_if(ParseContext& ctx, EdgeBlock::MyIfElse& result) {
    result.conds = std::pmr::vector<IfElse<std::pmr::vector<EdgeBlock>>::Block>( ctx.arena );

    auto n = ctx.tokens.next();

    while (n.type != Token::Type::Eof) {
        auto block = &result.conds.emplace_back();

        // if (EXPR) then
        n = ctx.tokens.peek();
        CRIT_CHECK(n, OpenParen, Unknown);
        parse_expr(ctx, block->c);
        PROP();
        n = ctx.tokens.next();
        CRIT_CHECK(n, Keyword, Unknown);
        if (n.value != "then") CRIT_FAIL(n, Unknown);

        block->b = std::pmr::vector<EdgeBlock>(ctx.arena);
        parse_edge_if_body(ctx, block->b, false);
        PROP();

        n = ctx.tokens.next();
        CRIT_CHECK(n, Keyword, Unknown);
        if (n.value == "else") {
            if (ctx.tokens.peek().value == "if") {
                ctx.tokens.next();
                continue;
            } else {
                result.catchall = std::pmr::vector<EdgeBlock>(ctx.arena);
                parse_edge_if_body(ctx, *result.catchall, false);
                PROP();
                n = ctx.tokens.next();
                CRIT_CHECK(n, Keyword, Unknown);
                if (n.value != "end") CRIT_FAIL(n, Unknown);
                return;
            }
        } else if (n.value == "end") {
            return;
        }
    }

    CRIT_FAIL(n, EarlyEof);
}
static void parse_edge_if_body(ParseContext& ctx, std::pmr::vector<EdgeBlock>& result, bool top) {
    bool last_was_edge = false;
    auto n = ctx.tokens.peek();

    while (n.type != Token::Type::Eof) {
        n = ctx.tokens.peek();
        switch (n.type) {
        case Token::Type::CloseParen:
            if (top) {
                return;
            } else {
                CRIT_FAIL(n, Unknown);
            }
            break;
        case Token::Type::Keyword:
            if (n.value == "match") {
                result.push_back({ EdgeBlock::MyMatch() });
                auto& ref = std::get<EdgeBlock::MyMatch>(result.back().node);
                parse_edge_match(ctx, ref);
            } else if (n.value == "if") {
                result.push_back({ EdgeBlock::MyIfElse() });
                auto& ref = std::get<EdgeBlock::MyIfElse>(result.back().node);
                parse_edge_if(ctx, ref);
            } else if (n.value == "else" || n.value == "end") {
                if (!top) {
                    return;
                } else {
                    CRIT_FAIL(n, Unknown);
                }
            }
            last_was_edge = false;
            break;
        case Token::Type::Strlit:
            if (last_was_edge) {
                auto& vec = std::get<EdgeBlock::MyT>(result.back().node);
                parse_edge(ctx, vec.emplace_back());
            } else {
                auto& vec = std::get<EdgeBlock::MyT>(result.emplace_back().node);
                parse_edge(ctx, vec.emplace_back());
            }
            last_was_edge = true;
            break;

        default:
            CRIT_FAIL(n, Unknown);
        }

        PROP();
    }

    CRIT_FAIL(n, EarlyEof);
}
static void parse_edge_match(ParseContext& ctx, Match<Edge>& result) {
    result.arms = std::pmr::vector<Match<Edge>::Arm>( ctx.arena );

    auto n = ctx.tokens.next();
    bool has_catchall = false;

    // match (EXPR) with
    n = ctx.tokens.peek();
    CRIT_CHECK(n, OpenParen, Unknown);
    parse_expr(ctx, result.ex);
    PROP();
    n = ctx.tokens.next();
    CRIT_CHECK(n, Keyword, Unknown);
    if (n.value != "with") CRIT_FAIL(n, Unknown);

    n = ctx.tokens.next();
    while (n.type != Token::Type::Eof) {
        switch (n.type) {
        case Token::Type::Keyword:
            if (n.value != "end") CRIT_FAIL(n, Unknown);
            return;

        case Token::Type::OpenParen: {
            Edge* arm_es;
            if (ctx.tokens.peek().type == Token::Type::CatchAll) {
                if (has_catchall) CRIT_FAIL(n, Unknown);
                ctx.tokens.next();
                result.catchall = Edge();
                arm_es = &result.catchall.value();
                has_catchall = true;
                n = ctx.tokens.next();
                CRIT_CHECK(n, CloseParen, Unknown);
            } else {
                auto& arm = result.arms.emplace_back();
                parse_value(ctx, arm.c);
                n = ctx.tokens.next();
                CRIT_CHECK(n, CloseParen, Unknown);
                arm_es = &arm.b;
            }
            PROP();
            n = ctx.tokens.peek();
            CRIT_CHECK(n, Strlit, Unknown);
            parse_edge(ctx, *arm_es);
            PROP();
            break; }

        default:
            CRIT_FAIL(n, Unknown);
            break;
        }
        n = ctx.tokens.next();
    }

    CRIT_FAIL(n, EarlyEof);
}
static void parse_effect(ParseContext& ctx, Effect& result) {
    // TODO
    while (ctx.tokens.peek().type != Token::Type::CloseParen) {
        ctx.tokens.next();
    }
}

}
