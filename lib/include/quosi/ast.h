#pragma once
#include "quosi/bc.h"
#include <string>
#include <vector>
#include <variant>
#include <optional>
#include <unordered_map>
#include <memory_resource>


namespace quosi { namespace ast {

struct Expr {
    std::variant<
        std::string_view, // identifier
        uint64_t,         // immediate
        bc::Instr         // operation => implies child nodes
    > value;
    std::pmr::vector<Expr> children;
};

using Effect = std::variant<std::pmr::string, Expr>;

struct Edge {
    std::string_view line;
    std::string_view next;
    std::optional<Effect> effect;
};

template<typename T>
struct IfElse {
    struct Block {
        Expr c;
        T b;
    };

    std::pmr::vector<Block> conds;
    std::optional<T> catchall;
};

template<typename T>
struct Match {
    struct Arm {
        Expr c;
        T b;
    };

    Expr ex;
    std::pmr::vector<Arm> arms;
    std::optional<T> catchall;
};

struct EdgeBlock {
    using MyT = std::pmr::vector<Edge>;
    using MyMatch = Match<Edge>;
    using MyIfElse = IfElse<std::pmr::vector<EdgeBlock>>;

    using Node = std::variant<
        MyT,
        MyMatch,
        MyIfElse
    >;
    Node node;
};

struct LineSet {
    std::string_view speaker;
    std::pmr::vector<std::string_view> lines;
};

struct Vertex {
    std::pmr::vector<LineSet> lines;
    std::pmr::vector<EdgeBlock> edges;
    std::string_view next;
};

struct VertexBlock {
    using MyT = Vertex;
    using MyMatch = Match<Vertex>;
    using MyIfElse = IfElse<std::pmr::vector<VertexBlock>>;

    using Node = std::variant<
        MyT,
        MyMatch,
        MyIfElse
    >;
    Node node;
};

struct Graph {
    std::pmr::monotonic_buffer_resource arena;

    std::pmr::string name;
    std::pmr::unordered_map<std::string_view, size_t> vert_names;
    std::pmr::vector<std::pair<std::string_view, VertexBlock>> verts;
    std::pmr::unordered_map<std::string_view, std::string_view> rename_table;
};

}

struct ErrorList;
class TokenStream;

namespace ast {

std::unique_ptr<Graph> parse(const char* src, ErrorList& errors);

} }
