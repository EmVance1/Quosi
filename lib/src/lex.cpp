#include "lex.h"
#include <cstring>
#include <string>
#include <unordered_set>


namespace quosi {


static bool is_num(char c) {
    return '0' <= c && c <= '9';
}
static bool is_ident_begin(char c) {
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
}
static bool is_ident(char c) {
    return is_ident_begin(c) || is_num(c) || c == '.';
}
static bool is_space(char c, bool* wasnull) {
    if (c == '\0') *wasnull = true;
    return c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\0';
}
static bool is_keyword(std::string_view str) {
    static std::unordered_set<std::string_view> kws = {
        "if", "then", "else", "match", "with", "end", "true", "false", "rename", "module", "endmod"
    };
    return kws.find(str) != kws.end();
}


enum class TokenStream::State {
    Void = 0,
    Ident,
    Number,
    String,
    StringEsc,
    Symbol,
    Comment,
};


TokenStream::TokenStream(const char* text)
    : base(text), ptr(text), state(State::Void), cache(step())
{}

Token TokenStream::step() {
    const char* tbeg = nullptr;
    size_t tlen = 0;
    Span tspan = pos;

    while (!wasnull) {
        Span p = pos;
        const char c = *(ptr++);
        if (c == '\n') { p.row++; p.col = 0; } else { p.col++; }

        switch (state) {
        case State::Void:
            if (is_space(c, &wasnull)) {
                // continue
            } else if (is_ident_begin(c)) {
                state = State::Ident;
                tbeg = ptr-1;
                tlen = 1;
                tspan = p;
            } else if (is_num(c)) {
                state = State::Number;
                tbeg = ptr-1;
                tlen = 1;
                tspan = p;
            } else {
                tbeg = ptr-1;
                tlen = 1;
                tspan = p;
                pos = p;
                switch (c) {
                case '(': return Token{ Token::Type::OpenParen,  std::string_view(tbeg, 1), tspan };
                case ')': return Token{ Token::Type::CloseParen, std::string_view(tbeg, 1), tspan };
                case '[': return Token{ Token::Type::OpenBrack,  std::string_view(tbeg, 1), tspan };
                case ']': return Token{ Token::Type::CloseBrack, std::string_view(tbeg, 1), tspan };
                case '{': return Token{ Token::Type::OpenBrace,  std::string_view(tbeg, 1), tspan };
                case '}': return Token{ Token::Type::CloseBrace, std::string_view(tbeg, 1), tspan };
                case ',': return Token{ Token::Type::Comma,      std::string_view(tbeg, 1), tspan };
                // case ':': return Token{ Token::Type::Colon,      std::string_view(tbeg, 1), tspan };
                case '+': case '-': case '*': case '/': case '<': case '>': case '=': case '!': case '&': case '|': case ':':
                    state = State::Symbol; break;
                case '"':
                    state = State::String; break;
                case '#':
                    state = State::Comment; break;
                default:
                    return Token{ Token::Type::Error, std::string_view("unknown symbol encountered"), tspan };
                }
            }
            break;

        case State::Ident:
            if (is_ident(c)) {
                tlen++;
            } else {
                if (is_space(c, &wasnull)) {
                    pos = p;
                } else {
                    ptr--;
                }
                state = State::Void;
                const auto view = std::string_view(tbeg, tlen);
                if (view == "_") {
                    return Token{ Token::Type::CatchAll, view, tspan };
                } else if (is_keyword(view)) {
                    return Token{ Token::Type::Keyword, view, tspan };
                } else {
                    return Token{ Token::Type::Ident, view, tspan };
                }
            }
            break;

        case State::Number:
            if (is_num(c)) {
                tlen++;
            } else if (is_space(c, &wasnull)) {
                pos = p;
                state = State::Void;
                return Token{ Token::Type::Number, std::string_view(tbeg, tlen), tspan };
            } else {
                ptr--;
                state = State::Void;
                return Token{ Token::Type::Number, std::string_view(tbeg, tlen), tspan };
            }
            break;

        case State::String:
            tlen++;
            if (c == '"') {
                pos = p;
                state = State::Void;
                return Token{ Token::Type::Strlit, std::string_view(tbeg+1, tlen-2), tspan };
            } else if (c == '\\') {
                state = State::StringEsc;
            }
            break;

        case State::StringEsc:
            tlen++;
            state = State::String;
            break;

        case State::Symbol:
            state = State::Void;
            if (c == '=') {
                pos = p;
                switch (*tbeg) {
                case '+': return Token{ Token::Type::AddEq, std::string_view(tbeg, 2), tspan };
                case '-': return Token{ Token::Type::SubEq, std::string_view(tbeg, 2), tspan };
                case '*': return Token{ Token::Type::MulEq, std::string_view(tbeg, 2), tspan };
                case '/': return Token{ Token::Type::DivEq, std::string_view(tbeg, 2), tspan };
                case '<': return Token{ Token::Type::Leq,   std::string_view(tbeg, 2), tspan };
                case '>': return Token{ Token::Type::Geq,   std::string_view(tbeg, 2), tspan };
                case '=': return Token{ Token::Type::Equ,   std::string_view(tbeg, 2), tspan };
                case '!': return Token{ Token::Type::Neq,   std::string_view(tbeg, 2), tspan };
                default:
                    return Token{ Token::Type::Error, std::string_view("unknown double glyph encountered"), tspan };
                }
            } else if (c == '&' && *tbeg == '&') {
                pos = p;
                return Token{ Token::Type::LogAnd, std::string_view(tbeg, 2), tspan };
            } else if (c == '|' && *tbeg == '|') {
                pos = p;
                return Token{ Token::Type::LogOr, std::string_view(tbeg, 2), tspan };
            } else if (c == ':' && *tbeg == ':') {
                pos = p;
                return Token{ Token::Type::Join, std::string_view(tbeg, 2), tspan };
            } else if (c == '>' && *tbeg == '=') {
                pos = p;
                return Token{ Token::Type::Arrow, std::string_view(tbeg, 2), tspan };
            } else {
                ptr--;
                switch (*tbeg) {
                case '+': return Token{ Token::Type::Add,        std::string_view(tbeg, 1), tspan };
                case '-': return Token{ Token::Type::Sub,        std::string_view(tbeg, 1), tspan };
                case '*': return Token{ Token::Type::Mul,        std::string_view(tbeg, 1), tspan };
                case '/': return Token{ Token::Type::Div,        std::string_view(tbeg, 1), tspan };
                case '<': return Token{ Token::Type::Lth,        std::string_view(tbeg, 1), tspan };
                case '>': return Token{ Token::Type::Gth,        std::string_view(tbeg, 1), tspan };
                case '=': return Token{ Token::Type::SetEq,      std::string_view(tbeg, 1), tspan };
                case '!': return Token{ Token::Type::LogNot,     std::string_view(tbeg, 1), tspan };
                case ':': return Token{ Token::Type::Colon,      std::string_view(tbeg, 1), tspan };
                case '&': case '|':
                    return Token{ Token::Type::Error, std::string_view("unknown double glyph encountered"), tspan };
                default:
                    return Token{ Token::Type::Myfault, std::string_view("should be unreachable: " + std::to_string(__LINE__)), tspan };
                }
            }
            break;

        case State::Comment:
            if (c == '\n') {
                state = State::Void;
            }
            break;
        }
        pos = p;
    }

    return Token{ Token::Type::Eof, "[[EOF]]", {} };
}

Token TokenStream::peek() {
    return cache;
}

Token TokenStream::next() {
    const auto temp = cache;
    cache = step();
    return temp;
}

}
