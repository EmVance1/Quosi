#include "quosi/error.h"

namespace quosi {

std::string Error::to_string() const {
    switch (type) {
    case Type::EarlyEof:
        return "end of file encountered abruptly";
    case Type::MisplacedToken:
        return "unexpected token encountered";
    case Type::BadVertexBegin:
        return "expected node declaration";
    case Type::NoElse:
        return "'if' in node definition requires catchall case 'else'";
    case Type::NoCatchall:
        return "'match' in node definition requires catchall case '_'";
    case Type::CaseDuplicate:
        return "'match' expression canot have duplicate cases";
    case Type::NoEntryPoint:
        return "must have one initial node 'START'";
    case Type::MultiVertexName:
        return "cannot have duplicate node name";
    case Type::DanglingEdge:
        return "node being pointed to does not exist";

    default:
        return "unknown error";
    }
}

}

