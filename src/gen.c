#include "cquosi/quosi.h"
#include "cquosi/ast.h"
#include "cquosi/bc.h"
#include <string.h>
#include <stdio.h>
#define QUOSIDS_ALLOCATOR (ctx->alloc)
#include "vec.h"

#define ALLOC(T) (T*)quosi_allocator_allocate(ctx->alloc, sizeof(T))
#define STREQ(view, cptr) ((view.len == sizeof(cptr)-1) && (strncmp(view.ptr, cptr, view.len) == 0))


typedef struct JumpTarget {
    uint32_t referenced_at;
    uint32_t label;
} LabelTarget;
typedef struct StringTarget {
    uint32_t referenced_at;
    quosiStrView value;
} StringTarget;
typedef struct EffectTarget {
    const quosiEdge* edge;
    uint32_t label;
} EffectTarget;

typedef struct GenContext {
    quosiAllocator alloc;
    quosiSymbolCtx symbol_ctx;
    const quosiGraph* name_lkp;

    // vector
    uint8_t* result;
    // vector
    uint32_t* labels;
    // vector
    LabelTarget* jumps;
    // vector
    EffectTarget* edges;
    // vector
    StringTarget* strings;

    uint32_t edge_index;
    uint32_t symbol_index;

    // std::pmr::unordered_map<std::string_view, uint32_t> symbols;
} GenContext;

void quosi_program_data_free(quosiProgramData* data, quosiAllocator alloc) {
    struct DummyContext { quosiAllocator alloc; } context;
    struct DummyContext* ctx = &context;
    ctx->alloc = alloc;

    for (size_t i = 0; i < quosids_arrlenu(data->mods); i++) {
        quosids_arrfree(data->mods[i].code);
    }
    quosids_arrfree(data->mods);
    quosids_arrfree(data->strs);
    quosids_arrfree(data->syms);
}

static uint32_t gen_label(GenContext* ctx) {
    const uint32_t l = (uint32_t)quosids_arrlenu(ctx->labels);
    quosids_arraddn(ctx->labels, 1);
    return l;
}
static uint32_t resolve_edge(GenContext* ctx, quosiStrView edge) {
    if (STREQ(edge, "EXIT")) {
        return UINT32_MAX;
    }
    for (size_t i = 0; i < quosids_arrlenu(ctx->name_lkp->vertices); i++) {
        if ((ctx->name_lkp->vertices[i].name.len == edge.len) &&
            (strncmp(ctx->name_lkp->vertices[i].name.ptr, edge.ptr, edge.len) == 0))
        {
            return (uint32_t)i;
        }
    }
    return 0;
}
static uint32_t resolve_flag(GenContext* ctx, quosiStrView sym) {
    char* cpy = quosi_allocator_allocate(ctx->alloc, (sym.len+1) * sizeof(char));
    cpy[sym.len] = 0;
    memcpy(cpy, sym.ptr, sym.len);
    return ctx->symbol_ctx.data_lkp(cpy);
}
static uint32_t resolve_speaker(GenContext* ctx, quosiStrView sym) {
    char* cpy = quosi_allocator_allocate(ctx->alloc, (sym.len+1) * sizeof(char));
    cpy[sym.len] = 0;
    memcpy(cpy, sym.ptr, sym.len);
    return ctx->symbol_ctx.speaker_lkp(cpy);
}


static void compile_edge(GenContext* ctx, const quosiEdge* edge);
static void compile_vertex(GenContext* ctx, const quosiVertex* vert);
static void compile_effects(GenContext* ctx, const quosiEffect* effs);
static void compile_expr(GenContext* ctx, const quosiExpr* expr, bool ieq);
static void compile_eblock(GenContext* ctx, const quosiEdgeBlock* block);
static void compile_vblock(GenContext* ctx, const quosiVertexBlock* block);


quosiProgramData quosi_compile_ast(const quosiAst* ast, quosiSymbolCtx symbol_ctx, quosiAllocator alloc) {
    GenContext context = (GenContext){
        .alloc=alloc,
        .symbol_ctx=symbol_ctx,
    };
    GenContext* ctx = &context;
    quosiProgramData result = {0};

    for (size_t i = 0; i < quosids_arrlenu(ast->modules); i++) {
        const quosiGraph* mod = &ast->modules[i];
        ctx->name_lkp = mod;
        ctx->result = NULL;
        ctx->labels = NULL;
        ctx->jumps = NULL;
        ctx->edges = NULL;
        ctx->strings = NULL;
        ctx->edge_index = 0;
        ctx->symbol_index = 0;

        // FIRST #VERTICES LABELS RESERVED FOR EDGE JUMPS
        quosids_arraddn(ctx->labels, quosids_arrlenu(mod->vertices));
        uint32_t entry_pos = UINT32_MAX;

        // CODE GENERATION
        for (size_t j = 0; j < quosids_arrlenu(mod->vertices); j++) {
            const quosiNamedVertex* v = &mod->vertices[j];
            const uint32_t curr_pos = (uint32_t)quosids_arrlenu(ctx->result);
            ctx->labels[j] = curr_pos;
            if (STREQ(v->name, "START")) entry_pos = curr_pos;
            compile_vblock(ctx, &v->data);
        }

        // JUMP PATCHING
        for (size_t j = 0; j < quosids_arrlenu(ctx->jumps); j++) {
            const LabelTarget* jmp = &ctx->jumps[j];
            if (jmp->label == UINT32_MAX) {
                const uint32_t exit_marker = UINT32_MAX;
                memcpy(ctx->result + jmp->referenced_at, &exit_marker, sizeof(uint32_t));
            } else {
                memcpy(ctx->result + jmp->referenced_at, &ctx->labels[jmp->label], sizeof(uint32_t));
            }
        }

        // STRING PATCHING
        for (size_t j = 0; j < quosids_arrlenu(ctx->strings); j++) {
            const StringTarget* str = &ctx->strings[j];
            const uint32_t pos = (uint32_t)quosids_arrlenu(result.strs);
            memcpy(ctx->result + str->referenced_at, &pos, sizeof(uint32_t));

            bool esc = false;
            for (size_t k = 0; k < str->value.len; k++) {
                const char c = str->value.ptr[k];
                if (esc) {
                    switch (c) {
                    case 'n':  quosids_arrpush(result.strs, '\n'); break;
                    case '"':  quosids_arrpush(result.strs, '"');  break;
                    case '\\': quosids_arrpush(result.strs, '\\'); break;
                    }
                    esc = false;
                } else if (c == '\\') {
                    esc = true;
                } else {
                    quosids_arrpush(result.strs, (uint8_t)c);
                }
            }
            quosids_arrpush(result.strs, (uint8_t)0);
        }

        // DO NOT FREE RESULT -> MUST BE ALIVE AT LOAD TIME
        quosids_arrfree(ctx->labels);
        quosids_arrfree(ctx->jumps);
        quosids_arrfree(ctx->edges);
        quosids_arrfree(ctx->strings);
        quosids_arrpush(result.mods, ((quosiModData){ .name=mod->name, .entry=entry_pos, .code=ctx->result }));
    }

    /* symbol table */
    /*
    if (!symbol_ctx) {
        for (size_t i = 0; i < quosids_arrlenu(result.syms); i++) {
            for (const auto c : k) {
                quosids_arrpush(result.syms, c);
            }
            quosids_arrpush(result.syms, (uint8_t)0);
            quosids_arrpush(result.syms, v);
        }
    }
    */

    return result;
}


static void compile_expr(GenContext* ctx, const quosiExpr* e, bool ieq) {
    switch (e->tag) {
    case QUOSI_EXPR_IDENT: {
        quosids_arrpush(ctx->result, (uint8_t)(ieq ? QUOSI_INSTR_IEQK : QUOSI_INSTR_LOAD));
        const uint32_t ref = resolve_flag(ctx, e->value.ident);
        uint8_t* begin = quosids_arraddnptr(ctx->result, sizeof(uint32_t));
        memcpy(begin, &ref, sizeof(uint32_t));
        break; }
    case QUOSI_EXPR_IMM: {
        quosids_arrpush(ctx->result, (uint8_t)(ieq ? QUOSI_INSTR_IEQV : QUOSI_INSTR_PUSH));
        uint8_t* begin = quosids_arraddnptr(ctx->result, sizeof(uint64_t));
        memcpy(begin, &e->value.imm, sizeof(uint64_t));
        break; }
    case QUOSI_EXPR_OP:
        if (e->value.op == QUOSI_INSTR_LNOT) {
            compile_expr(ctx, e->lhs, false);
            quosids_arrpush(ctx->result, e->value.op);
        } else if (e->value.op == QUOSI_INSTR_STORE) {
            compile_expr(ctx, e->rhs, false);
            quosids_arrpush(ctx->result, (uint8_t)QUOSI_INSTR_DUP);
            quosids_arrpush(ctx->result, (uint8_t)QUOSI_INSTR_STORE);
            const uint32_t ref = resolve_flag(ctx, e->lhs->value.ident);
            uint8_t* begin = quosids_arraddnptr(ctx->result, sizeof(uint32_t));
            memcpy(begin, &ref, sizeof(uint32_t));
        } else {
            compile_expr(ctx, e->lhs, false);
            compile_expr(ctx, e->rhs, false);
            quosids_arrpush(ctx->result, e->value.op);
        }
        break;
    }
}
static void compile_effects(GenContext* ctx, const quosiEffect* actions) {
    for (size_t i = 0; i < quosids_arrlenu(actions); i++) {
        const quosiEffect* e = &actions[i];
        if (e->op == QUOSI_EFFECT_EVENT) {
            quosids_arrpush(ctx->result, (uint8_t)QUOSI_INSTR_EVENT);
            quosids_arrpush(ctx->strings, ((StringTarget){ (uint32_t)quosids_arrlenu(ctx->result), e->lhs }));
            quosids_arraddn(ctx->result, sizeof(uint32_t));
        } else {
            if (e->op != QUOSI_EFFECT_SET) {
                quosids_arrpush(ctx->result, (uint8_t)QUOSI_INSTR_LOAD);
                const uint32_t ref = resolve_flag(ctx, e->lhs);
                uint8_t* begin = quosids_arraddnptr(ctx->result, sizeof(uint32_t));
                memcpy(begin, &ref, sizeof(uint32_t));
            }
            compile_expr(ctx, &e->rhs, false);
            switch (e->op) {
            case QUOSI_EFFECT_ADD:
                quosids_arrpush(ctx->result, (uint8_t)QUOSI_INSTR_ADD);
                break;
            case QUOSI_EFFECT_SUB:
                quosids_arrpush(ctx->result, (uint8_t)QUOSI_INSTR_SUB);
                break;
            default:
                break;
            }
            quosids_arrpush(ctx->result, (uint8_t)QUOSI_INSTR_STORE);
            const uint32_t ref2 = resolve_flag(ctx, e->lhs);
            uint8_t* begin2 = quosids_arraddnptr(ctx->result, sizeof(uint32_t));
            memcpy(begin2, &ref2, sizeof(uint32_t));
        }
    }
}

static void compile_eblock(GenContext* ctx, const quosiEdgeBlock* b) {
    switch (b->tag) {
    case QUOSI_EBLOCK_T:
        for (size_t i = 0; i < quosids_arrlenu(b->value.edges); i++) {
            compile_edge(ctx, &b->value.edges[i]);
        }
        break;
    case QUOSI_EBLOCK_MATCH: {
        const quosiEdgeMatch* mc = &b->value.match;
        const uint32_t end_lbl = gen_label(ctx);
        compile_expr(ctx, &mc->expr, false);
        for (size_t i = 0; i < quosids_arrlenu(mc->arms); i++) {
            const uint32_t next_lbl = gen_label(ctx);
            compile_expr(ctx, &mc->arms[i].cond, true);
            quosids_arrpush(ctx->result, (uint8_t)QUOSI_INSTR_JZ);
            quosids_arrpush(ctx->jumps, ((LabelTarget){ (uint32_t)quosids_arrlenu(ctx->result), next_lbl }));

            quosids_arraddn(ctx->result, sizeof(uint32_t));
            compile_edge(ctx, &mc->arms[i].body);
            quosids_arrpush(ctx->result, (uint8_t)QUOSI_INSTR_JUMP);
            quosids_arrpush(ctx->jumps, ((LabelTarget){ (uint32_t)quosids_arrlenu(ctx->result), end_lbl }));
            quosids_arraddn(ctx->result, sizeof(uint32_t));
            ctx->labels[next_lbl] = (uint32_t)quosids_arrlenu(ctx->result);
        }
        if (mc->catchall.exists) {
            compile_edge(ctx, &mc->catchall.arm);
        }
        ctx->labels[end_lbl] = (uint32_t)quosids_arrlenu(ctx->result);
        quosids_arrpush(ctx->result, (uint8_t)QUOSI_INSTR_POP);
        break; }
    case QUOSI_EBLOCK_IFELSE: {
        const quosiEdgeIfElse* ie = &b->value.ifelse;
        const uint32_t end_lbl = gen_label(ctx);
        size_t idx = 0;
        for (size_t i = 0; i < quosids_arrlenu(ie->blocks); i++) {
            const uint32_t next_lbl = gen_label(ctx);
            compile_expr(ctx, &ie->blocks[i].cond, false);
            quosids_arrpush(ctx->result, (uint8_t)QUOSI_INSTR_JZ);
            quosids_arrpush(ctx->jumps, ((LabelTarget){ (uint32_t)quosids_arrlenu(ctx->result), next_lbl }));
            quosids_arraddn(ctx->result, sizeof(uint32_t));
            for (size_t j = 0; j < quosids_arrlenu(ie->blocks[i].body); j++) {
                compile_eblock(ctx, &ie->blocks[i].body[j]);
            }
            if (ie->catchall != NULL || idx < quosids_arrlenu(ie->blocks) - 1) { // micro-opt for superfluous tail branches
                quosids_arrpush(ctx->result, (uint8_t)QUOSI_INSTR_JUMP);
                quosids_arrpush(ctx->jumps, ((LabelTarget){ (uint32_t)quosids_arrlenu(ctx->result), end_lbl }));
                quosids_arraddn(ctx->result, sizeof(uint32_t));
            }
            ctx->labels[next_lbl] = (uint32_t)quosids_arrlenu(ctx->result);
            idx++;
        }
        if (ie->catchall) {
            for (size_t i = 0; i < quosids_arrlenu(ie->catchall); i++) {
                compile_eblock(ctx, &ie->catchall[i]);
            }
        }
        ctx->labels[end_lbl] = (uint32_t)quosids_arrlenu(ctx->result);
        break; }
    }
    // we do not need tail jumps since edges are innately jumps
}
static void compile_vblock(GenContext* ctx, const quosiVertexBlock* b) {
    switch (b->tag) {
    case QUOSI_VBLOCK_T:
        compile_vertex(ctx, &b->value.vertex);
        break;
    case QUOSI_VBLOCK_MATCH: {
        const quosiVertexMatch* mc = &b->value.match;
        compile_expr(ctx, &mc->expr, false);
        for (size_t i = 0; i < quosids_arrlenu(mc->arms); i++) {
            const uint32_t next_lbl = gen_label(ctx);
            compile_expr(ctx, &mc->arms[i].cond, true);
            quosids_arrpush(ctx->result, (uint8_t)QUOSI_INSTR_JZ);
            quosids_arrpush(ctx->jumps, ((LabelTarget){ (uint32_t)quosids_arrlenu(ctx->result), next_lbl }));
            quosids_arraddn(ctx->result, sizeof(uint32_t));
            quosids_arrpush(ctx->result, (uint8_t)QUOSI_INSTR_POP);
            compile_vertex(ctx, &mc->arms[i].body);
            ctx->labels[next_lbl] = (uint32_t)quosids_arrlenu(ctx->result);
        }
        quosids_arrpush(ctx->result, (uint8_t)QUOSI_INSTR_POP);
        compile_vertex(ctx, &mc->catchall);
        break; }
    case QUOSI_VBLOCK_IFELSE: {
        const quosiVertexIfElse* ie = &b->value.ifelse;
        for (size_t i = 0; i < quosids_arrlenu(ie->blocks); i++) {
            const uint32_t next_lbl = gen_label(ctx);
            compile_expr(ctx, &ie->blocks[i].cond, false);
            quosids_arrpush(ctx->result, (uint8_t)QUOSI_INSTR_JZ);
            quosids_arrpush(ctx->jumps, ((LabelTarget){ (uint32_t)quosids_arrlenu(ctx->result), next_lbl }));
            quosids_arraddn(ctx->result, sizeof(uint32_t));
            compile_vblock(ctx, ie->blocks[i].data);
            ctx->labels[next_lbl] = (uint32_t)quosids_arrlenu(ctx->result);
        }
        compile_vblock(ctx, ie->catchall);
        break; }
    }
    // we do not need tail jumps since vertices always lead to jumps eventually
}
static void compile_edge(GenContext* ctx, const quosiEdge* e) {
    quosids_arrpush(ctx->result, (uint8_t)QUOSI_INSTR_PROP);
    quosids_arrpush(ctx->strings, ((StringTarget){ (uint32_t)quosids_arrlenu(ctx->result), e->line }));
    quosids_arraddn(ctx->result, sizeof(uint32_t));
    quosids_arrpush(ctx->result, (uint8_t)ctx->edge_index++);
    if (e->effects == NULL) {
        quosids_arrpush(ctx->edges, ((EffectTarget){ e, resolve_edge(ctx, e->next) }));
    } else {
        quosids_arrpush(ctx->edges, ((EffectTarget){ e, gen_label(ctx) }));
    }
}
static void compile_vertex(GenContext* ctx, const quosiVertex* v) {
    for (size_t i = 0; i < quosids_arrlenu(v->lineset); i++) {
        for (size_t j = 0; j < quosids_arrlenu(v->lineset[i].lines); j++) {
            quosids_arrpush(ctx->result, (uint8_t)QUOSI_INSTR_LINE);
            const uint32_t id = resolve_speaker(ctx, v->lineset[i].speaker);
            uint8_t* begin = quosids_arraddnptr(ctx->result, sizeof(uint32_t));
            memcpy(begin, &id, sizeof(uint32_t));         // speaker ID

            quosids_arrpush(ctx->strings, ((StringTarget){ (uint32_t)(quosids_arrlenu(ctx->result)), v->lineset[i].lines[j] }));
            quosids_arraddn(ctx->result, sizeof(uint32_t)); // line
        }
    }

    if (v->type == QUOSI_VERTEX_JUMP) {
        if (v->jump.effects) {
            compile_effects(ctx, v->jump.effects);
        }
        quosids_arrpush(ctx->result, (uint8_t)QUOSI_INSTR_JUMP);
        quosids_arrpush(ctx->jumps, ((LabelTarget){ (uint32_t)quosids_arrlenu(ctx->result), resolve_edge(ctx, v->jump.next) }));
        quosids_arraddn(ctx->result, sizeof(uint32_t));
    } else {
        ctx->edge_index = 0;
        quosids_arrfree(ctx->edges);
        for (size_t i = 0; i < quosids_arrlenu(v->edges); i++) {
            compile_eblock(ctx, &v->edges[i]);
        }
        quosids_arrpush(ctx->result, (uint8_t)QUOSI_INSTR_PICK);
        quosids_arrpush(ctx->result, (uint8_t)QUOSI_INSTR_SWITCH);
        for (size_t i = 0; i < quosids_arrlenu(ctx->edges); i++) {
            const EffectTarget* e = &ctx->edges[i];
            quosids_arrpush(ctx->jumps, ((LabelTarget){ (uint32_t)quosids_arrlenu(ctx->result), e->label }));
            quosids_arraddn(ctx->result, sizeof(uint32_t));
        }
        for (size_t i = 0; i < quosids_arrlenu(ctx->edges); i++) {
            const EffectTarget* e = &ctx->edges[i];
            if (e->edge->effects != NULL) {
                ctx->labels[e->label] = (uint32_t)quosids_arrlenu(ctx->result);
                compile_effects(ctx, e->edge->effects);
                quosids_arrpush(ctx->result, (uint8_t)QUOSI_INSTR_JUMP);
                quosids_arrpush(ctx->jumps,  ((LabelTarget){ (uint32_t)quosids_arrlenu(ctx->result), resolve_edge(ctx, e->edge->next) }));
                quosids_arraddn(ctx->result, sizeof(uint32_t));
            }
        }
    }
}

