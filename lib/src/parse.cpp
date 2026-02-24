#include "quosi/ast.h"
#include "quosi/error.h"
#include "quosi/bc.h"
#include "lex.h"
#include "expr.h"
#include <unordered_map>
// #include <iostream>

namespace quosi::ast {

using namespace bc;


#define EH_FAIL(tok, E) do { ctx.errors->list.push_back({ Error::Type::E, tok.span }); \
    if (ctx.errors->list.back().is_critical()) { ctx.errors->fail = true; return; } } while (0)
#define EH_CHECK(tok, T, E) do { if (tok.type != Token::Type::T) { EH_FAIL(tok, E); } } while (0)
#define EH_PROP() do { if (ctx.errors->fail) return; } while (0)

#define EH_FAIL_RET(tok, E) do { ctx.errors->list.push_back({ Error::Type::E, tok.span }); \
    if (ctx.errors->list.back().is_critical()) { ctx.errors->fail = true; return result; } } while (0)
#define EH_CHECK_RET(tok, T, E) do { if (tok.type != Token::Type::T) { EH_FAIL_RET(tok, E); } } while (0)
#define EH_PROP_RET() do { if (ctx.errors->fail) return result; } while (0)

static void parse_graph(ParseContext& ctx, Graph& result);
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


std::unique_ptr<Ast> parse(const char* src, ErrorList& errors) {
    auto result = std::make_unique<Ast>();
    auto ctx = ParseContext{
        &result->arena,
        TokenStream(src),
        &errors,
        std::pmr::vector<Token>(&result->arena),
    };
    result->graphs = std::pmr::unordered_map<std::string_view, Graph>(ctx.arena);

    auto n = ctx.tokens.next();

    while (n.type != Token::Type::Eof) {
        // module NAME
        EH_CHECK_RET(n, Keyword, BadGraphBegin);
        if (n.value != "module") EH_FAIL_RET(n, BadGraphBegin);
        const auto name = ctx.tokens.next();
        EH_CHECK_RET(name, Ident, BadGraphBegin);

        result->graphs.emplace(name.value, Graph());
        parse_graph(ctx, result->graphs[name.value]);
        EH_PROP_RET();
        n = ctx.tokens.next();
    }

    return result;
}

void parse_graph(ParseContext& ctx, Graph& result) {
    result.vert_names = std::pmr::unordered_map<std::string_view, size_t>(ctx.arena);
    result.verts = std::pmr::vector<std::pair<std::string_view, VertexBlock>>(ctx.arena);
    result.rename_table = std::pmr::unordered_map<std::string_view, std::string_view>(ctx.arena);

    auto n = ctx.tokens.next();

    while (n.type != Token::Type::Eof) {
        // rename INIT => NEW
        if (n.type == Token::Type::Keyword) {
            if (n.value == "endmod") return;
            else if (n.value != "rename") EH_FAIL(n, BadVertexBegin);
            const auto init = ctx.tokens.next(); // n = <INIT>
            EH_CHECK(init, Ident, BadRename);
            n = ctx.tokens.next();
            EH_CHECK(n, Arrow, BadRename);
            const auto rend = ctx.tokens.next(); // n = <NEW>
            EH_CHECK(rend, Ident, BadRename);
            result.rename_table.emplace(rend.value, init.value);
            n = ctx.tokens.next(); // FOLLOW(rename)
            continue;
        }
        // IDENT =
        EH_CHECK(n, Ident, BadVertexBegin);
        const auto name = n;
        n = ctx.tokens.next();
        EH_CHECK(n, SetEq, MisplacedToken);

        auto& vert = result.verts.emplace_back();
        vert.first = name.value;
        if (contains_key(result.vert_names, vert.first)) EH_FAIL(name, MultiVertexName);
        result.vert_names.emplace(name.value, result.verts.size() - 1);
        parse_vert_if_body(ctx, vert.second);
        EH_PROP();
        n = ctx.tokens.next();
    }

    if (!contains_key(result.vert_names, "START")) EH_FAIL(n, NoEntryPoint);
    for (const auto& edge : ctx.edges) {
        if (edge.value == "START" || edge.value == "EXIT") continue;
        const auto e = edge.value;
        if (!contains_key(result.vert_names, e)) {
             EH_FAIL(edge, DanglingEdge);
        }
    }

    EH_FAIL(n, EarlyEof);
}

static void parse_vert(ParseContext& ctx, Vertex& result) {
    result.lines = std::pmr::vector<LineSet>( ctx.arena );
    result.edges = std::pmr::vector<EdgeBlock>( ctx.arena );

    auto n = ctx.tokens.next();
    while (n.type == Token::Type::Lth) {
        // <IDENT:
        auto& line = result.lines.emplace_back();
        line.lines = std::pmr::vector<std::string_view>( ctx.arena );
        n = ctx.tokens.next();
        EH_CHECK(n, Ident, Unknown);
        line.speaker = n.value;
        n = ctx.tokens.next();
        EH_CHECK(n, Colon, Unknown);

        // STRING,... >
        n = ctx.tokens.next();
        while (n.type != Token::Type::Gth) {
            EH_CHECK(n, Strlit, Unknown);
            line.lines.emplace_back(n.value);
            n = ctx.tokens.next();
            switch (n.type) {
            case Token::Type::Comma:
                n = ctx.tokens.next();
                break;
            case Token::Type::Gth:
                goto endlines;

            default:
                EH_FAIL(n, Unknown);
            }
        }
endlines:
        n = ctx.tokens.next();
    }

    switch (n.type) {
    case Token::Type::Arrow:
        // => IDENT
        n = ctx.tokens.next();
        EH_CHECK(n, Ident, Unknown);
        result.next = n.value;
        ctx.edges.push_back(std::move(n));
        break;
    case Token::Type::OpenParen:
        // ( ... )
        result.edges = std::pmr::vector<EdgeBlock>(ctx.arena);
        parse_edge_if_body(ctx, result.edges, true);
        EH_PROP();
        n = ctx.tokens.next();
        EH_CHECK(n, CloseParen, Unknown);
        break;

    default:
        EH_FAIL(n, Unknown);
    }

    return;
}
static void parse_vert_if(ParseContext& ctx, VertexBlock::MyIfElse& result) {
    auto n = ctx.tokens.next();

    while (n.type != Token::Type::Eof) {
        auto block = &result.conds.emplace_back();

        // if (EXPR) then
        n = ctx.tokens.peek();
        EH_CHECK(n, OpenParen, Unknown);
        parse_expr(ctx, block->c);
        EH_PROP();
        n = ctx.tokens.next();
        EH_CHECK(n, Keyword, Unknown);
        if (n.value != "then") EH_FAIL(n, Unknown);

        block->b = std::pmr::vector<VertexBlock>(ctx.arena);
        parse_vert_if_body(ctx, block->b.emplace_back());
        EH_PROP();

        n = ctx.tokens.next();
        EH_CHECK(n, Keyword, Unknown);
        if (n.value == "else") {
            if (ctx.tokens.peek().value == "if") {
                ctx.tokens.next();
                continue;
            } else {
                result.catchall = std::pmr::vector<VertexBlock>(ctx.arena);
                parse_vert_if_body(ctx, result.catchall->emplace_back());
                n = ctx.tokens.next();
                EH_CHECK(n, Keyword, Unknown);
                if (n.value != "end") EH_FAIL(n, Unknown);
                return;
            }
        } else if (n.value == "end") {
            EH_FAIL(n, NoElse);
            return;
        }
    }

    EH_FAIL(n, EarlyEof);
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
        EH_FAIL(n, Unknown);
    }

    return;
}
static void parse_vert_match(ParseContext& ctx, VertexBlock::MyMatch& result) {
    auto n = ctx.tokens.next();
    bool has_catchall = false;

    // match (EXPR) with
    n = ctx.tokens.peek();
    EH_CHECK(n, OpenParen, Unknown);
    parse_expr(ctx, result.ex);
    EH_PROP();
    n = ctx.tokens.next();
    EH_CHECK(n, Keyword, Unknown);
    if (n.value != "with") EH_FAIL(n, Unknown);

    n = ctx.tokens.next();
    while (n.type != Token::Type::Eof) {
        switch (n.type) {
        case Token::Type::Keyword:
            if (n.value != "end") EH_FAIL(n, Unknown);
            if (!has_catchall)    EH_FAIL(n, NoCatchall);
            return;

        case Token::Type::OpenParen: {
            Vertex* arm_vert;
            if (ctx.tokens.peek().type == Token::Type::CatchAll) {
                ctx.tokens.next();
                if (has_catchall) EH_FAIL(n, Unknown);
                result.catchall = Vertex();
                arm_vert = &result.catchall.value();
                has_catchall = true;
                n = ctx.tokens.next();
                EH_CHECK(n, CloseParen, Unknown);
            } else {
                auto& arm = result.arms.emplace_back();
                parse_value(ctx, arm.c);
                n = ctx.tokens.next();
                EH_CHECK(n, CloseParen, Unknown);
                arm_vert = &arm.b;
            }
            EH_PROP();
            n = ctx.tokens.peek();
            EH_CHECK(n, Lth, Unknown);
            parse_vert(ctx, *arm_vert);
            EH_PROP();
            break; }

        default:
            EH_FAIL(n, Unknown);
            break;
        }
        n = ctx.tokens.next();
    }

    EH_FAIL(n, EarlyEof);
}

static void parse_edge(ParseContext& ctx, Edge& result) {
    // STRING :: (EFFECT) => IDENT
    auto n = ctx.tokens.next();
    EH_CHECK(n, Strlit, Unknown);
    result.line = n.value;
    n = ctx.tokens.next();
    switch (n.type) {
    case Token::Type::Join:
        result.effect = Effect();
        parse_effect(ctx, *result.effect);
        EH_PROP();
        n = ctx.tokens.next();
        EH_CHECK(n, CloseParen, Unknown);
        n = ctx.tokens.next();
        EH_CHECK(n, Arrow, Unknown);
        break;

    case Token::Type::Arrow:
        break;

    default:
        EH_FAIL(n, Unknown);
    }
    n = ctx.tokens.next();
    EH_CHECK(n, Ident, Unknown);
    result.next = n.value;
    ctx.edges.push_back(std::move(n));
}
static void parse_edge_if(ParseContext& ctx, EdgeBlock::MyIfElse& result) {
    result.conds = std::pmr::vector<IfElse<std::pmr::vector<EdgeBlock>>::Block>( ctx.arena );

    auto n = ctx.tokens.next();

    while (n.type != Token::Type::Eof) {
        auto block = &result.conds.emplace_back();

        // if (EXPR) then
        n = ctx.tokens.peek();
        EH_CHECK(n, OpenParen, Unknown);
        parse_expr(ctx, block->c);
        EH_PROP();
        n = ctx.tokens.next();
        EH_CHECK(n, Keyword, Unknown);
        if (n.value != "then") EH_FAIL(n, Unknown);

        block->b = std::pmr::vector<EdgeBlock>(ctx.arena);
        parse_edge_if_body(ctx, block->b, false);
        EH_PROP();

        n = ctx.tokens.next();
        EH_CHECK(n, Keyword, Unknown);
        if (n.value == "else") {
            if (ctx.tokens.peek().value == "if") {
                ctx.tokens.next();
                continue;
            } else {
                result.catchall = std::pmr::vector<EdgeBlock>(ctx.arena);
                parse_edge_if_body(ctx, *result.catchall, false);
                EH_PROP();
                n = ctx.tokens.next();
                EH_CHECK(n, Keyword, Unknown);
                if (n.value != "end") EH_FAIL(n, Unknown);
                return;
            }
        } else if (n.value == "end") {
            return;
        }
    }

    EH_FAIL(n, EarlyEof);
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
                EH_FAIL(n, Unknown);
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
                    EH_FAIL(n, Unknown);
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
            EH_FAIL(n, Unknown);
        }

        EH_PROP();
    }

    EH_FAIL(n, EarlyEof);
}
static void parse_edge_match(ParseContext& ctx, Match<Edge>& result) {
    result.arms = std::pmr::vector<Match<Edge>::Arm>( ctx.arena );

    auto n = ctx.tokens.next();
    bool has_catchall = false;

    // match (EXPR) with
    n = ctx.tokens.peek();
    EH_CHECK(n, OpenParen, Unknown);
    parse_expr(ctx, result.ex);
    EH_PROP();
    n = ctx.tokens.next();
    EH_CHECK(n, Keyword, Unknown);
    if (n.value != "with") EH_FAIL(n, Unknown);

    n = ctx.tokens.next();
    while (n.type != Token::Type::Eof) {
        switch (n.type) {
        case Token::Type::Keyword:
            if (n.value != "end") EH_FAIL(n, Unknown);
            return;

        case Token::Type::OpenParen: {
            Edge* arm_es;
            if (ctx.tokens.peek().type == Token::Type::CatchAll) {
                if (has_catchall) EH_FAIL(n, Unknown);
                ctx.tokens.next();
                result.catchall = Edge();
                arm_es = &result.catchall.value();
                has_catchall = true;
                n = ctx.tokens.next();
                EH_CHECK(n, CloseParen, Unknown);
            } else {
                auto& arm = result.arms.emplace_back();
                parse_value(ctx, arm.c);
                n = ctx.tokens.next();
                EH_CHECK(n, CloseParen, Unknown);
                arm_es = &arm.b;
            }
            EH_PROP();
            n = ctx.tokens.peek();
            EH_CHECK(n, Strlit, Unknown);
            parse_edge(ctx, *arm_es);
            EH_PROP();
            break; }

        default:
            EH_FAIL(n, Unknown);
            break;
        }
        n = ctx.tokens.next();
    }

    EH_FAIL(n, EarlyEof);
}
static void parse_effect(ParseContext& ctx, Effect& result) {
    // TODO
    while (ctx.tokens.peek().type != Token::Type::CloseParen) {
        ctx.tokens.next();
    }
}

}
