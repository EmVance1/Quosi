#include "quosi/ast.h"
#include "lex.h"
#include "expr.h"
#include <string.h>
// #include <stdio.h>
#define QUOSIDS_ALLOCATOR (ctx->alloc)
#include "vec.h"


#define NEXT(stream)  quosi_token_stream_next(stream)
#define PEEK(stream)  quosi_token_stream_peek(stream)
#define ALLOC(T) (T*) quosi_allocator_allocate(ctx->alloc, sizeof(T))
#define STREQ(view, cptr) ((view.len == sizeof(cptr)-1) && (strncmp(view.ptr, cptr, view.len) == 0))

#define EH_FAIL(tok, E) do { const quosiErrorValue errv = ((quosiErrorValue){ .type=QUOSI_ERR_##E, .span=tok.span }); \
    quosi_error_list_push(ctx->errors, errv); \
    if (quosi_error_is_critical(errv)) { ctx->errors->critical = true; return; } } while (0)
#define EH_CHECK(tok, T, E) do { if (tok.type != QUOSI_TOKEN_##T) { EH_FAIL(tok, E); } } while (0)
#define EH_PROP() do { if (ctx->errors->critical) return; } while (0)

#define EH_FAIL_RET(tok, E) do { const quosiErrorValue errv = ((quosiErrorValue){ .type=QUOSI_ERR_##E, .span=tok.span }); \
    quosi_error_list_push(ctx->errors, errv); \
    if (quosi_error_is_critical(errv)) { ctx->errors->critical = true; return result; } } while (0)
#define EH_CHECK_RET(tok, T, E) do { if (tok.type != QUOSI_TOKEN_##T) { EH_FAIL_RET(tok, E); } } while (0)
#define EH_PROP_RET() do { if (ctx->errors->critical) return result; } while (0)

static void parse_graph(quosiParseCtx* ctx, quosiGraph* result);
static void parse_vert(quosiParseCtx* ctx, quosiVertex* result);
static void parse_vert_if(quosiParseCtx* ctx, quosiVertexIfElse* result);
static void parse_vert_if_body(quosiParseCtx* ctx, quosiVertexBlock* result);
static void parse_vert_match(quosiParseCtx* ctx, quosiVertexMatch* result);
static void parse_edge(quosiParseCtx* ctx, quosiEdge* result);
static void parse_edge_if(quosiParseCtx* ctx, quosiEdgeIfElse* result);
static void parse_edge_if_body(quosiParseCtx* ctx, quosiEdgeBlock** result, bool top);
static void parse_edge_match(quosiParseCtx* ctx, quosiEdgeMatch* result);
static void parse_effect(quosiParseCtx* ctx, quosiEffect** result);

// #define contains_key(map, key) (map.find(key) != map.end())


quosiAst quosi_ast_parse_from_src(const char* src, quosiError* errors, quosiAllocator alloc) {
    quosiParseCtx context = {
        .alloc=alloc,
        .tokens=quosi_token_stream_init(src),
        .errors=errors,
        .edges=NULL,
    };
    quosiParseCtx* ctx = &context;
    quosiAst result;
    result.modules = NULL;

    quosiToken n = NEXT(&ctx->tokens);
    while (n.type != QUOSI_TOKEN_EOF) {
        // module NAME
        EH_CHECK_RET(n, KEYWORD, BAD_GRAPH_BEGIN);
        if (!STREQ(n.value, "module")) EH_FAIL_RET(n, BAD_GRAPH_BEGIN);
        const quosiToken name = NEXT(&ctx->tokens);
        EH_CHECK_RET(name, IDENT, BAD_GRAPH_BEGIN);

        quosiGraph* graph = quosids_arraddnptr(result.modules, 1);
        graph->name = name.value;
        graph->vertices = NULL;
        parse_graph(ctx, graph);
        EH_PROP_RET();
        n = NEXT(&ctx->tokens);
    }
    quosids_arrfree(context.edges);

    return result;
}

static void parse_graph(quosiParseCtx* ctx, quosiGraph* result) {
    quosiToken n = NEXT(&ctx->tokens);
    while (n.type != QUOSI_TOKEN_EOF) {
        // rename INIT => NEW
        if (n.type == QUOSI_TOKEN_KEYWORD) {
            if (STREQ(n.value, "endmod")) {
                return;
            } else if (!STREQ(n.value, "rename")) {
                EH_FAIL(n, BAD_VERTEX_BEGIN);
            }
            const quosiToken init = NEXT(&ctx->tokens); // n = <INIT>
            EH_CHECK(init, IDENT, BAD_RENAME);
            n = NEXT(&ctx->tokens);
            EH_CHECK(n, ARROW, BAD_RENAME);
            const quosiToken rend = NEXT(&ctx->tokens); // n = <NEW>
            EH_CHECK(rend, IDENT, BAD_RENAME);
            // result.rename_table.emplace(rend.value, init.value);
            n = NEXT(&ctx->tokens); // FOLLOW(rename)
            continue;
        }
        // IDENT =
        EH_CHECK(n, IDENT, BAD_VERTEX_BEGIN);
        const quosiToken name = n;
        n = NEXT(&ctx->tokens);
        EH_CHECK(n, SETEQ, MISPLACED_TOKEN);

        quosids_arrpush(result->vertices, ((quosiNamedVertex){ name.value, { 0 } }));
        quosiVertexBlock* vert = &quosids_arrlast(result->vertices).data;
        // if (contains_key(result.vert_names, vert.first)) EH_FAIL(name, DUPLICATE_VERTEX);
        parse_vert_if_body(ctx, vert);
        EH_PROP();
        n = NEXT(&ctx->tokens);
    }

    // if (!contains_key(result.vert_names, "START")) EH_FAIL(n, NO_ENTRY);
    // for (const auto& edge : ctx.edges) {
    //     if (STREQ(edge.value, "START") || STREQ(edge.value, "EXIT")) continue;
    //     const auto e = edge.value;
    //     if (!contains_key(result.vert_names, e)) {
    //          EH_FAIL(edge, DANGLING_EDGE);
    //     }
    // }

    EH_FAIL(n, EARLY_EOF);
}

static void parse_vert(quosiParseCtx* ctx, quosiVertex* result) {
    quosiToken n = NEXT(&ctx->tokens);
    while (n.type == QUOSI_TOKEN_LTH) {
        // <IDENT:
        quosiVertexLineSet* lines = quosids_arraddnptr(result->lineset, 1);
        lines->lines = NULL;
        n = NEXT(&ctx->tokens);
        EH_CHECK(n, IDENT, UNKNOWN);
        lines->speaker = n.value;
        n = NEXT(&ctx->tokens);
        EH_CHECK(n, COLON, UNKNOWN);

        // STRING,... >
        n = NEXT(&ctx->tokens);
        while (n.type != QUOSI_TOKEN_GTH) {
            EH_CHECK(n, STRLIT, UNKNOWN);
            quosids_arrpush(lines->lines, n.value);
            n = NEXT(&ctx->tokens);
            switch (n.type) {
            case QUOSI_TOKEN_COMMA:
                n = NEXT(&ctx->tokens);
                break;
            case QUOSI_TOKEN_GTH:
                goto endlines;

            default:
                EH_FAIL(n, UNKNOWN);
            }
        }
endlines:
        n = NEXT(&ctx->tokens);
    }

    switch (n.type) {
    case QUOSI_TOKEN_JOIN:
        // :: (EFFS) => IDENT
        result->type = QUOSI_VERTEX_JUMP;
        result->jump.effects = NULL;
        parse_effect(ctx, &result->jump.effects);
        EH_PROP();
        n = NEXT(&ctx->tokens);
        EH_CHECK(n, ARROW, UNKNOWN);
        n = NEXT(&ctx->tokens);
        EH_CHECK(n, IDENT, UNKNOWN);
        result->jump.next = n.value;
        quosids_arrpush(ctx->edges, n);
        break;

    case QUOSI_TOKEN_ARROW:
        // => IDENT
        result->type = QUOSI_VERTEX_JUMP;
        n = NEXT(&ctx->tokens);
        EH_CHECK(n, IDENT, UNKNOWN);
        result->jump.next = n.value;
        quosids_arrpush(ctx->edges, n);
        break;
    case QUOSI_TOKEN_OPENPAREN:
        // ( ... )
        parse_edge_if_body(ctx, &result->edges, true);
        EH_PROP();
        n = NEXT(&ctx->tokens);
        EH_CHECK(n, CLOSEPAREN, UNKNOWN);
        break;

    default:
        EH_FAIL(n, UNKNOWN);
    }

    return;
}
static void parse_vert_if(quosiParseCtx* ctx, quosiVertexIfElse* result) {
    quosiToken n = NEXT(&ctx->tokens);
    while (n.type != QUOSI_TOKEN_EOF) {
        quosiVertexIfElseBlock* block = quosids_arraddnptr(result->blocks, 1);

        // if (EXPR) then
        n = PEEK(&ctx->tokens);
        EH_CHECK(n, OPENPAREN, UNKNOWN);
        quosi_internal_parse_expr(ctx, &block->cond);
        EH_PROP();
        n = NEXT(&ctx->tokens);
        EH_CHECK(n, KEYWORD, UNKNOWN);
        if (!STREQ(n.value, "then")) EH_FAIL(n, UNKNOWN);

        block->data = ALLOC(quosiVertexBlock);
        parse_vert_if_body(ctx, block->data);
        EH_PROP();

        n = NEXT(&ctx->tokens);
        EH_CHECK(n, KEYWORD, UNKNOWN);
        if (STREQ(n.value, "else")) {
            if (STREQ(PEEK(&ctx->tokens).value, "if")) {
                NEXT(&ctx->tokens);
                continue;
            } else {
                result->catchall = ALLOC(quosiVertexBlock);
                parse_vert_if_body(ctx, result->catchall);
                n = NEXT(&ctx->tokens);
                EH_CHECK(n, KEYWORD, UNKNOWN);
                if (!STREQ(n.value, "end")) EH_FAIL(n, UNKNOWN);
                return;
            }
        } else if (STREQ(n.value, "end")) {
            EH_FAIL(n, NO_ELSE);
            return;
        }
    }

    EH_FAIL(n, EARLY_EOF);
}
static void parse_vert_if_body(quosiParseCtx* ctx, quosiVertexBlock* result) {
    quosiToken n = PEEK(&ctx->tokens);
    switch (n.type) {
    case QUOSI_TOKEN_KEYWORD:
        if (STREQ(n.value, "match")) {
            result->tag = QUOSI_VBLOCK_MATCH;
            result->value.match.arms = NULL;
            parse_vert_match(ctx, &result->value.match);
        } else if (STREQ(n.value, "if")) {
            result->tag = QUOSI_VBLOCK_IFELSE;
            result->value.ifelse.blocks = NULL;
            parse_vert_if(ctx, &result->value.ifelse);
        }
        break;
    case QUOSI_TOKEN_LTH:
        result->tag = QUOSI_VBLOCK_T;
        result->value.vertex.lineset = NULL;
        result->value.vertex.edges = NULL;
        parse_vert(ctx, &result->value.vertex);
        break;

    default:
        EH_FAIL(n, UNKNOWN);
    }

    return;
}
static void parse_vert_match(quosiParseCtx* ctx, quosiVertexMatch* result) {
    quosiToken n = NEXT(&ctx->tokens);
    bool has_catchall = false;

    // match (EXPR) with
    n = PEEK(&ctx->tokens);
    EH_CHECK(n, OPENPAREN, UNKNOWN);
    quosi_internal_parse_expr(ctx, &result->expr);
    EH_PROP();
    n = NEXT(&ctx->tokens);
    EH_CHECK(n, KEYWORD, UNKNOWN);
    if (!STREQ(n.value, "with")) EH_FAIL(n, UNKNOWN);

    n = NEXT(&ctx->tokens);
    while (n.type != QUOSI_TOKEN_EOF) {
        switch (n.type) {
        case QUOSI_TOKEN_KEYWORD:
            if (!STREQ(n.value, "end")) EH_FAIL(n, UNKNOWN);
            if (!has_catchall)          EH_FAIL(n, NO_CATCHALL);
            return;

        case QUOSI_TOKEN_OPENPAREN: {
            quosiVertex* arm_vert;
            if (PEEK(&ctx->tokens).type == QUOSI_TOKEN_CATCHALL) {
                NEXT(&ctx->tokens);
                if (has_catchall) EH_FAIL(n, UNKNOWN);
                result->catchall = (quosiVertex){ 0 };
                arm_vert = &result->catchall;
                has_catchall = true;
            } else {
                quosiVertexMatchArm* arm = quosids_arraddnptr(result->arms, 1);
                quosi_internal_parse_value(ctx, &arm->cond);
                arm_vert = &arm->body;
            }
            n = NEXT(&ctx->tokens);
            EH_CHECK(n, CLOSEPAREN, UNKNOWN);
            n = PEEK(&ctx->tokens);
            EH_CHECK(n, LTH, UNKNOWN);
            arm_vert->lineset = NULL;
            arm_vert->edges = NULL;
            parse_vert(ctx, arm_vert);
            EH_PROP();
            break; }

        default:
            EH_FAIL(n, UNKNOWN);
            break;
        }
        n = NEXT(&ctx->tokens);
    }

    EH_FAIL(n, EARLY_EOF);
}

static void parse_edge(quosiParseCtx* ctx, quosiEdge* result) {
    // STRING :: (EFFECT) => IDENT
    quosiToken n = NEXT(&ctx->tokens);
    EH_CHECK(n, STRLIT, UNKNOWN);
    result->line = n.value;
    n = NEXT(&ctx->tokens);
    switch (n.type) {
    case QUOSI_TOKEN_JOIN:
        parse_effect(ctx, &result->effects);
        EH_PROP();
        n = NEXT(&ctx->tokens);
        EH_CHECK(n, ARROW, UNKNOWN);
        break;

    case QUOSI_TOKEN_ARROW:
        break;

    default:
        EH_FAIL(n, UNKNOWN);
    }
    n = NEXT(&ctx->tokens);
    EH_CHECK(n, IDENT, UNKNOWN);
    result->next = n.value;
    quosids_arrpush(ctx->edges, n);
}
static void parse_edge_if(quosiParseCtx* ctx, quosiEdgeIfElse* result) {
    quosiToken n = NEXT(&ctx->tokens);
    while (n.type != QUOSI_TOKEN_EOF) {
        quosiEdgeIfElseBlock* block = quosids_arraddnptr(result->blocks, 1);

        // if (EXPR) then
        n = PEEK(&ctx->tokens);
        EH_CHECK(n, OPENPAREN, UNKNOWN);
        quosi_internal_parse_expr(ctx, &block->cond);
        EH_PROP();
        n = NEXT(&ctx->tokens);
        EH_CHECK(n, KEYWORD, UNKNOWN);
        if (!STREQ(n.value, "then")) EH_FAIL(n, UNKNOWN);

        block->body = NULL;
        parse_edge_if_body(ctx, &block->body, false);
        EH_PROP();

        n = NEXT(&ctx->tokens);
        EH_CHECK(n, KEYWORD, UNKNOWN);
        if (STREQ(n.value, "else")) {
            if (STREQ(PEEK(&ctx->tokens).value, "if")) {
                NEXT(&ctx->tokens);
                continue;
            } else {
                parse_edge_if_body(ctx, &result->catchall, false);
                EH_PROP();
                n = NEXT(&ctx->tokens);
                EH_CHECK(n, KEYWORD, UNKNOWN);
                if (!STREQ(n.value, "end")) EH_FAIL(n, UNKNOWN);
                return;
            }
        } else if (STREQ(n.value, "end")) {
            return;
        }
    }

    EH_FAIL(n, EARLY_EOF);
}
static void parse_edge_if_body(quosiParseCtx* ctx, quosiEdgeBlock** result, bool top) {
    bool last_was_edge = false;
    quosiToken n = PEEK(&ctx->tokens);

    while (n.type != QUOSI_TOKEN_EOF) {
        n = PEEK(&ctx->tokens);
        switch (n.type) {
        case QUOSI_TOKEN_CLOSEPAREN:
            if (top) {
                return;
            } else {
                EH_FAIL(n, UNKNOWN);
            }
            break;
        case QUOSI_TOKEN_KEYWORD:
            if (STREQ(n.value, "match")) {
                quosiEdgeBlock* ref = quosids_arraddnptr(*result, 1);
                ref->tag = QUOSI_EBLOCK_MATCH;
                ref->value.match.arms = NULL;
                ref->value.match.catchall.exists = false;
                parse_edge_match(ctx, &ref->value.match);
            } else if (STREQ(n.value, "if")) {
                quosiEdgeBlock* ref = quosids_arraddnptr(*result, 1);
                ref->tag = QUOSI_EBLOCK_IFELSE;
                ref->value.ifelse.blocks = NULL;
                ref->value.ifelse.catchall = NULL;
                parse_edge_if(ctx, &ref->value.ifelse);
            } else if (STREQ(n.value, "else") || STREQ(n.value, "end")) {
                if (!top) {
                    return;
                } else {
                    EH_FAIL(n, UNKNOWN);
                }
            }
            last_was_edge = false;
            break;
        case QUOSI_TOKEN_STRLIT:
            if (!last_was_edge) {
                quosiEdgeBlock* ref = quosids_arraddnptr(*result, 1);
                ref->tag = QUOSI_EBLOCK_T;
                ref->value.edges = NULL;
            }
            quosiEdgeBlock* ref = &quosids_arrlast(*result);
            quosiEdge* edge = quosids_arraddnptr(ref->value.edges, 1);
            edge->effects = NULL;
            parse_edge(ctx, edge);
            last_was_edge = true;
            break;

        default:
            EH_FAIL(n, UNKNOWN);
        }

        EH_PROP();
    }

    EH_FAIL(n, EARLY_EOF);
}
static void parse_edge_match(quosiParseCtx* ctx, quosiEdgeMatch* result) {
    quosiToken n = NEXT(&ctx->tokens);

    // match (EXPR) with
    n = PEEK(&ctx->tokens);
    EH_CHECK(n, OPENPAREN, UNKNOWN);
    quosi_internal_parse_expr(ctx, &result->expr);
    EH_PROP();
    n = NEXT(&ctx->tokens);
    EH_CHECK(n, KEYWORD, UNKNOWN);
    if (!STREQ(n.value, "with")) EH_FAIL(n, UNKNOWN);

    n = NEXT(&ctx->tokens);
    while (n.type != QUOSI_TOKEN_EOF) {
        switch (n.type) {
        case QUOSI_TOKEN_KEYWORD:
            if (!STREQ(n.value, "end")) EH_FAIL(n, UNKNOWN);
            return;

        case QUOSI_TOKEN_OPENPAREN: {
            quosiEdge* arm_es;
            if (PEEK(&ctx->tokens).type == QUOSI_TOKEN_CATCHALL) {
                if (result->catchall.exists) EH_FAIL(n, UNKNOWN);
                NEXT(&ctx->tokens);
                result->catchall.exists = true;
                arm_es = &result->catchall.arm;
            } else {
                quosiEdgeMatchArm* arm = quosids_arraddnptr(result->arms, 1);
                quosi_internal_parse_value(ctx, &arm->cond);
                arm_es = &arm->body;
            }
            n = NEXT(&ctx->tokens);
            EH_CHECK(n, CLOSEPAREN, UNKNOWN);
            n = PEEK(&ctx->tokens);
            EH_CHECK(n, STRLIT, UNKNOWN);
            arm_es->effects = NULL;
            parse_edge(ctx, arm_es);
            EH_PROP();
            break; }

        default:
            EH_FAIL(n, UNKNOWN);
            break;
        }
        n = NEXT(&ctx->tokens);
    }

    EH_FAIL(n, EARLY_EOF);
}
static void parse_effect(quosiParseCtx* ctx, quosiEffect** result) {
    quosiToken n = NEXT(&ctx->tokens);
    EH_CHECK(n, OPENPAREN, UNKNOWN);

    n = NEXT(&ctx->tokens);
    while (n.type != QUOSI_TOKEN_CLOSEPAREN) {
        EH_CHECK(n, IDENT, UNKNOWN);
        quosiEffect* effect = quosids_arraddnptr(*result, 1);
        effect->lhs = n.value;
        effect->op = QUOSI_EFFECT_EVENT;

        n = NEXT(&ctx->tokens);
        switch (n.type) {
        case QUOSI_TOKEN_ADDEQ: case QUOSI_TOKEN_SUBEQ: case QUOSI_TOKEN_SETEQ:
            switch (n.type) {
            case QUOSI_TOKEN_ADDEQ:
                effect->op = QUOSI_EFFECT_ADD; break;
            case QUOSI_TOKEN_SUBEQ:
                effect->op = QUOSI_EFFECT_SUB; break;
            case QUOSI_TOKEN_SETEQ:
                effect->op = QUOSI_EFFECT_SET; break;
            default:
                EH_FAIL(n, UNKNOWN);
                break;
            }
            quosi_internal_parse_expr(ctx, &effect->rhs);
            n = NEXT(&ctx->tokens);
            switch (n.type) {
            case QUOSI_TOKEN_COMMA:
                n = NEXT(&ctx->tokens);
                break;
            case QUOSI_TOKEN_CLOSEPAREN:
                break;

            default:
                EH_FAIL(n, UNKNOWN);
            }
            break;

        case QUOSI_TOKEN_COMMA:
            n = NEXT(&ctx->tokens);
            break;
        case QUOSI_TOKEN_CLOSEPAREN:
            return;

        default:
            EH_FAIL(n, UNKNOWN);
        }
    }
}

