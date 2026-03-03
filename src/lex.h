#ifndef CQUOSI_IMPL_LEX_H
#define CQUOSI_IMPL_LEX_H
#include "quosi/quosi.h"
#include <stdbool.h>
#include <stdint.h>


typedef struct quosiToken {
    enum quosiTokenType {
        QUOSI_TOKEN_MYFAULT = -2,
        QUOSI_TOKEN_ERROR = -1,
        QUOSI_TOKEN_NONE = 0,

        QUOSI_TOKEN_OPENPAREN,
        QUOSI_TOKEN_CLOSEPAREN,
        QUOSI_TOKEN_OPENBRACK,
        QUOSI_TOKEN_CLOSEBRACK,
        QUOSI_TOKEN_OPENBRACE,
        QUOSI_TOKEN_CLOSEBRACE,

        QUOSI_TOKEN_ADD,
        QUOSI_TOKEN_SUB,
        QUOSI_TOKEN_MUL,
        QUOSI_TOKEN_DIV,

        QUOSI_TOKEN_EQU,
        QUOSI_TOKEN_NEQ,
        QUOSI_TOKEN_LTH,
        QUOSI_TOKEN_GTH,
        QUOSI_TOKEN_LEQ,
        QUOSI_TOKEN_GEQ,

        QUOSI_TOKEN_LOGNOT,
        QUOSI_TOKEN_LOGAND,
        QUOSI_TOKEN_LOGOR,

        QUOSI_TOKEN_ADDEQ,
        QUOSI_TOKEN_SUBEQ,
        QUOSI_TOKEN_MULEQ,
        QUOSI_TOKEN_DIVEQ,
        QUOSI_TOKEN_SETEQ,

        QUOSI_TOKEN_ARROW,
        QUOSI_TOKEN_COMMA,
        QUOSI_TOKEN_COLON,
        QUOSI_TOKEN_JOIN,
        QUOSI_TOKEN_CATCHALL,

        QUOSI_TOKEN_IDENT,
        QUOSI_TOKEN_NUMBER,
        QUOSI_TOKEN_STRLIT,
        QUOSI_TOKEN_KEYWORD,

        QUOSI_TOKEN_EOF,
    } type;
    quosiStrView value;
    quosiErrorSpan span;
} quosiToken;

typedef struct quosiTokenStream {
    const char* base;
    const char* ptr;
    quosiErrorSpan pos;
    bool wasnull;
    int state;
    quosiToken cache;
} quosiTokenStream;

quosiTokenStream quosi_token_stream_init(const char* text);
quosiToken quosi_token_stream_peek(const quosiTokenStream* self);
quosiToken quosi_token_stream_next(quosiTokenStream* self);


#endif
