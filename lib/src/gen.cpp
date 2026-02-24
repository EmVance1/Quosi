#include "quosi/quosi.h"
#include "quosi/ast.h"
#include "quosi/bc.h"
#include "bytevec.h"
#include "num.h"
#include <memory_resource>


namespace quosi::bc {

using namespace ast;

struct LabelTarget {
    u32 pos;
    std::pmr::string target;
};
struct EffectTarget {
    const Edge* edge;
    std::pmr::string target;
};

struct GenContext {
    ByteVec result;

    std::pmr::memory_resource* arena;
    std::pmr::unordered_map<std::pmr::string, u32> labels;
    std::pmr::unordered_map<std::string_view, u32> symbols;
    std::pmr::vector<LabelTarget> jumps;
    std::pmr::vector<LabelTarget> strings;
    std::pmr::vector<EffectTarget> edges;
    SymbolContext symbol_ctx;
    u8 current_edge_index = 0;
    size_t label_index = 0;
    uint32_t symbol_index = 0;
};

static std::pmr::string gen_label(GenContext& ctx) {
    auto tmp = ctx.label_index++;
    if (tmp == 0) return std::pmr::string(".0", ctx.arena);
    auto sym = std::pmr::string(".", ctx.arena);
    while (tmp > 0) {
        sym.push_back('0' + (tmp % 10));
        tmp /= 10;
    }
    return sym;
}
static uint32_t resolve_sym(GenContext& ctx, std::string_view sym) {
    if (ctx.symbol_ctx) {
        const auto cpy = std::pmr::string(sym, ctx.arena);
        return ctx.symbol_ctx(cpy.c_str());
    } else {
        auto it = ctx.symbols.find(sym);
        if (it == ctx.symbols.end()) {
            const auto id = ctx.symbol_index++;
            ctx.symbols.emplace(sym, id);
            return id;
        } else {
            return it->second;
        }
    }
}


static void compile_edge(GenContext& ctx, const Edge& e);
static void compile_vertex(GenContext& ctx, const Vertex& v);
static void compile_expr(GenContext& ctx, const Expr& e, bool ieq = false) {
    switch (e.value.index()) {
    case 0:
        ctx.result.push((byte)(ieq ? Instr::IeqK : Instr::Load));
        ctx.result.push(resolve_sym(ctx, std::get<std::string_view>(e.value)));
        break;
    case 1:
        ctx.result.push((byte)(ieq ? Instr::IeqV : Instr::Push));
        ctx.result.push(std::get<uint64_t>(e.value));
        break;
    case 2:
        const auto op = std::get<Instr>(e.value);
        if (op == Instr::Lnot) {
            compile_expr(ctx, e.children[0]);
            ctx.result.push(op);
        } else {
            compile_expr(ctx, e.children[0]);
            compile_expr(ctx, e.children[1]);
            ctx.result.push(op);
        }
        break;
    }
}
static void compile_effect(GenContext& ctx, const std::pmr::vector<Effect>& actions) {
    for (const auto& e : actions) {
        switch (e.op) {
        case Effect::Action::Event:
            ctx.result.push((byte)Instr::Event);
            ctx.strings.push_back({ (u32)ctx.result.size(), std::pmr::string(e.lhs, ctx.arena) });
            ctx.result.push((u32)0);
            break;
        default:
            ctx.result.push((byte)Instr::Load);
            ctx.result.push(resolve_sym(ctx, e.lhs));
            compile_expr(ctx, e.rhs);
            switch (e.op) {
            case Effect::Action::Add:
                ctx.result.push((byte)Instr::Add);
                break;
            case Effect::Action::Sub:
                ctx.result.push((byte)Instr::Sub);
                break;
            default:
                break;
            }
            ctx.result.push((byte)Instr::Store);
            ctx.result.push(resolve_sym(ctx, e.lhs));
            break;
        }
    }
}

static void compile_eblock(GenContext& ctx, const EdgeBlock& b) {
    switch (b.node.index()) {
    case 0:
        for (const auto& e : std::get<EdgeBlock::MyT>(b.node)) {
            compile_edge(ctx, e);
        }
        break;
    case 1: {
        const auto& mc = std::get<EdgeBlock::MyMatch>(b.node);
        const auto end_lbl = gen_label(ctx);
        compile_expr(ctx, mc.ex);
        for (size_t i = 0; i < mc.arms.size(); i++) {
            const auto next_lbl = gen_label(ctx);
            compile_expr(ctx, mc.arms[i].c, true);
            ctx.result.push((byte)Instr::Jz);
            ctx.jumps.push_back({ (u32)ctx.result.size(), next_lbl });
            ctx.result.push((u32)0);
            compile_edge(ctx, mc.arms[i].b);
            ctx.result.push((byte)Instr::Jump);
            ctx.jumps.push_back({ (u32)ctx.result.size(), end_lbl });
            ctx.result.push((u32)0);
            ctx.labels.emplace(next_lbl, (u32)ctx.result.size());
        }
        if (mc.catchall.has_value()) {
            compile_edge(ctx, *mc.catchall);
        }
        ctx.labels.emplace(end_lbl, (u32)ctx.result.size());
        ctx.result.push((byte)Instr::Pop);
        break; }
    case 2: {
        const auto& ie = std::get<EdgeBlock::MyIfElse>(b.node);
        const auto end_lbl = gen_label(ctx);
        size_t idx = 0;
        for (const auto& branch : ie.conds) {
            const auto next_lbl = gen_label(ctx);
            compile_expr(ctx, branch.c);
            ctx.result.push((byte)Instr::Jz);
            ctx.jumps.push_back({ (u32)ctx.result.size(), next_lbl });
            ctx.result.push((u32)0);
            for (const auto& br : branch.b) {
                compile_eblock(ctx, br);
            }
            if (ie.catchall.has_value() || idx < ie.conds.size() - 1) { // micro-opt for superfluous tail branches
                ctx.result.push((byte)Instr::Jump);
                ctx.jumps.push_back({ (u32)ctx.result.size(), end_lbl });
                ctx.result.push((u32)0);
            }
            ctx.labels.emplace(next_lbl, (u32)ctx.result.size());
            idx++;
        }
        if (const auto& end = ie.catchall) {
            for (const auto& br : *end) {
                compile_eblock(ctx, br);
            }
        }
        ctx.labels.emplace(end_lbl, (u32)ctx.result.size());
        break; }
    }
    // we do not need tail jumps since edges are innately jumps
}
static void compile_vblock(GenContext& ctx, const VertexBlock& b) {
    switch (b.node.index()) {
    case 0:
        compile_vertex(ctx, std::get<VertexBlock::MyT>(b.node));
        break;
    case 1: {
        const auto& mc = std::get<VertexBlock::MyMatch>(b.node);
        const auto end_lbl = gen_label(ctx);
        compile_expr(ctx, mc.ex);
        for (size_t i = 0; i < mc.arms.size(); i++) {
            const auto next_lbl = gen_label(ctx);
            compile_expr(ctx, mc.arms[i].c, true);
            ctx.result.push((byte)Instr::Jz);
            ctx.jumps.push_back({ (u32)ctx.result.size(), next_lbl });
            ctx.result.push((u32)0);
            ctx.result.push((byte)Instr::Pop);
            compile_vertex(ctx, mc.arms[i].b);
            ctx.labels.emplace(next_lbl, (u32)ctx.result.size());
        }
        ctx.result.push((byte)Instr::Pop);
        compile_vertex(ctx, *mc.catchall);
        ctx.labels.emplace(end_lbl, (u32)ctx.result.size());
        break; }
    case 2: {
        const auto& ie = std::get<VertexBlock::MyIfElse>(b.node);
        const auto end_lbl = gen_label(ctx);
        for (const auto& branch : ie.conds) {
            const auto next_lbl = gen_label(ctx);
            compile_expr(ctx, branch.c);
            ctx.result.push((byte)Instr::Jz);
            ctx.jumps.push_back({ (u32)ctx.result.size(), next_lbl });
            ctx.result.push((u32)0);
            compile_vblock(ctx, branch.b[0]);
            ctx.labels.emplace(next_lbl, (u32)ctx.result.size());
        }
        if (const auto& end = ie.catchall) {
            compile_vblock(ctx, end.value()[0]);
        }
        ctx.labels.emplace(end_lbl, (u32)ctx.result.size());
        break; }
    }
    // we do not need tail jumps since vertices always lead to jumps eventually
}
static void compile_edge(GenContext& ctx, const Edge& e) {
    ctx.result.push((byte)Instr::Prop);
    ctx.strings.push_back({ (u32)ctx.result.size(), std::pmr::string(e.line, ctx.arena) });
    ctx.result.push((u32)0);
    ctx.result.push((u8)0);
    ctx.result.last() = ctx.current_edge_index++;
    if (e.effect.empty()) {
        ctx.edges.push_back(EffectTarget{ &e, "" });
    } else {
        ctx.edges.push_back(EffectTarget{ &e, gen_label(ctx) });
    }
}
static void compile_vertex(GenContext& ctx, const Vertex& v) {
    for (const auto& lvec : v.lines) {
        for (const auto& line : lvec.lines) {
            ctx.result.push((byte)Instr::Line);
            ctx.strings.push_back({ (u32)(ctx.result.size() + sizeof(u32)), std::pmr::string(line, ctx.arena) });
            ctx.result.push(resolve_sym(ctx, lvec.speaker)); // speaker ID
            ctx.result.push((u32)0); // line
        }
    }

    if (v.edges.empty()) {
        ctx.result.push((byte)Instr::Jump);
        ctx.jumps.push_back({ (u32)ctx.result.size(), std::pmr::string(v.next, ctx.arena) });
        ctx.result.push((u32)0);
    } else {
        ctx.current_edge_index = 0;
        ctx.edges.clear();
        for (const auto& e : v.edges) {
            compile_eblock(ctx, e);
        }
        ctx.result.push((byte)Instr::Pick);
        ctx.result.push((byte)Instr::Switch);
        for (const auto& e : ctx.edges) {
            if (e.edge->effect.empty()) {
                ctx.jumps.push_back({ (u32)ctx.result.size(), std::pmr::string(e.edge->next, ctx.arena) });
            } else {
                ctx.jumps.push_back({ (u32)ctx.result.size(), e.target });
            }
            ctx.result.push((u32)0);
        }
        for (const auto& e : ctx.edges) {
            if (!e.edge->effect.empty()) {
                ctx.labels[e.target] = (uint32_t)ctx.result.size();
                compile_effect(ctx, e.edge->effect);
                ctx.result.push((byte)Instr::Jump);
                ctx.jumps.push_back({ (u32)ctx.result.size(), std::pmr::string(e.edge->next, ctx.arena) });
                ctx.result.push((u32)0);
            }
        }
    }
}


ProgramData::~ProgramData() {
    free(data);
}

ProgramData compile_ast(const Ast& ast, SymbolContext symctx) {
    auto arena = std::pmr::monotonic_buffer_resource();
    auto ctx = GenContext{
        ByteVec(256),
        &arena,
        std::pmr::unordered_map<std::pmr::string, u32>( &arena ),
        std::pmr::unordered_map<std::string_view, u32>( &arena ),
        std::pmr::vector<LabelTarget>( &arena ),
        std::pmr::vector<LabelTarget>( &arena ),
        std::pmr::vector<EffectTarget>( &arena ),
        symctx
    };

    /* code gen */
    ctx.labels["START"] = 0;
    ctx.labels["EXIT"]  = UINT32_MAX;
    ctx.labels["ABORT"] = UINT32_MAX-1;
    for (const auto& [name, graph] : ast.graphs) {
        compile_vblock(ctx, graph.verts[graph.vert_names.at("START")].second);
        for (const auto& [k, vert] : graph.verts) {
            if (k == "START") continue;
            const auto pos = (u32)ctx.result.size();
            ctx.labels.emplace(k, pos);
            compile_vblock(ctx, vert);
        }
        ctx.result.push((byte)Instr::Eof);
    }

    const auto strloc = ctx.result.size();
    /* string patching */
    for (const auto&  v : ctx.strings) {
        const auto pos = (u32)ctx.result.size();
        memcpy(ctx.result.data + v.pos, &pos, sizeof(u32));
        bool esc = false;
        for (auto c : v.target) {
            if (esc) {
                switch (c) {
                case 'n':  ctx.result.push('\n'); break;
                case '"':  ctx.result.push('"'); break;
                case '\\': ctx.result.push('\\'); break;
                }
                esc = false;
            } else {
                if (c == '\\') {
                    esc = true;
                } else {
                    ctx.result.push(c);
                }
            }
        }
        ctx.result.push((u8)0);
    }

    const auto symloc = symctx ? 0 : ctx.result.size();
    /* symbol table */
    if (!symctx) {
        for (const auto& [k, v] : ctx.symbols) {
            for (const auto c : k) {
                ctx.result.push(c);
            }
            ctx.result.push((u8)0);
            ctx.result.push(v);
        }
    }

    /* jump patching */
    for (const auto& j : ctx.jumps) {
        memcpy(ctx.result.data + j.pos, &ctx.labels.at(std::pmr::string(j.target, ctx.arena)), sizeof(u32));
    }

    auto result = ProgramData{
        File::Header{
            VANGO_PKG_VERSION_MAJOR,
            VANGO_PKG_VERSION_MINOR,
            VANGO_PKG_VERSION_PATCH,
            { 'q', 'u', 'o', 's', 'i' },
            ctx.result.size(),
            strloc,
            symloc
        },
        ctx.result.leak()
    };
    return result;
}

}

