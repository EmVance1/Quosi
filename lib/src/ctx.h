#pragma once
#include "quosi/ast.h"
#include "lex.h"
#include <memory_resource>


namespace quosi {

struct ParseContext {
    std::pmr::memory_resource* arena;
    TokenStream tokens;
    ErrorList* errors;
    std::pmr::vector<Token> edges;
};

}
