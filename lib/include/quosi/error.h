#pragma once
#include <cstdint>
#include <string>
#include <vector>


namespace quosi {

struct Span {
    uint32_t row;
    uint32_t col;
};

struct Error {
    enum class Type {
        Unknown = 0,
        EarlyEof,
        MisplacedToken,
        BadVertexBegin,
        BadRename,
        NoElse,
        NoCatchall,
        CaseDuplicate,
        NoEntryPoint,
        MultiVertexName,
        DanglingEdge,
    };
    Type type;
    Span span;

    std::string to_string() const;
};

struct ErrorList {
    std::vector<Error> list;
    bool fail = false;
};


}
