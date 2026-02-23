#pragma once
#include <string_view>
#include "quosi/error.h"


namespace quosi {

struct Token {
    enum class Type {
        Myfault = -2,
        Error = -1,
        None = 0,

        OpenParen,
        CloseParen,
        OpenBrack,
        CloseBrack,
        OpenBrace,
        CloseBrace,

        Add,
        Sub,
        Mul,
        Div,

        Equ,
        Neq,
        Lth,
        Gth,
        Leq,
        Geq,

        LogNot,
        LogAnd,
        LogOr,

        AddEq,
        SubEq,
        MulEq,
        DivEq,
        SetEq,

        Arrow,
        Comma,
        Colon,
        Join,
        CatchAll,

        Ident,
        Number,
        Strlit,
        Keyword,

        Eof,
    };
    Type type;
    std::string_view value;
    Span span;
};

class TokenStream {
private:
    enum class State;
    const char* base;
    const char* ptr;
    Span pos = { 1, 0 };
    bool wasnull = false;
    State state;
    Token cache;

private:
    Token step();

public:
    TokenStream(const char* text);
    Token peek();
    Token next();
};

}

