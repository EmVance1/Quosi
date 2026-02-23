#include "quosi/error.h"

namespace quosi {

std::string Error::to_string() const {
    switch (type) {
    case Type::Unknown:
        return "TODO: label error";

    case Type::EarlyEof:
        return "end of file encountered abruptly";
    case Type::MisplacedToken:
        return "unexpected token encountered";
    case Type::BadVertexBegin:
        return "expected node or 'rename' declaration";
    case Type::BadRename:
        return "'rename' declaration is malformed";
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
        return "";
    }
}

bool Error::is_critical() const {
    switch (type) {
    case Type::Unknown:
        return true;

    case Type::EarlyEof:
        return true;
    case Type::MisplacedToken:
        return true;
    case Type::BadVertexBegin:
        return true;
    case Type::BadRename:
        return true;
    case Type::NoElse:
        return false;
    case Type::NoCatchall:
        return false;
    case Type::CaseDuplicate:
        return false;
    case Type::NoEntryPoint:
        return false;
    case Type::MultiVertexName:
        return false;
    case Type::DanglingEdge:
        return false;

    default:
        return true;
    }
}

}

