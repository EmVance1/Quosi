#include "quosi/quosi.h"
#include "vec.h"
#include "lex.h"


void quosi_error_list_push(quosiError* errors, quosiErrorValue err) {
    quosids_arrpush(errors->list, err);
}

size_t quosi_error_list_len(const quosiError* errors) {
    return quosids_arrlenu(errors->list);
}

void quosi_error_list_free(quosiError* errors) {
    quosids_arrfree(errors->list);
}


int quosi_internal_error_handle(quosiError* errs, quosiToken tok, int type) {
    const quosiErrorValue errv = ((quosiErrorValue){ .type=type, .span=tok.span });
    quosi_error_list_push(errs, errv);
    if (quosi_error_is_critical(errv)) {
        errs->critical = true;
        return 1;
    }
    return 0;
}

int quosi_internal_error_check(quosiError* errs, quosiToken tok, int expect, int failtype) {
    if      (tok.type == QUOSI_TOKEN_ERROR) return quosi_internal_error_handle(errs, tok, QUOSI_ERR_INVALID_TOKEN);
    else if (tok.type == QUOSI_TOKEN_EOF)   return quosi_internal_error_handle(errs, tok, QUOSI_ERR_EARLY_EOF);
    else if (tok.type != expect)            return quosi_internal_error_handle(errs, tok, failtype);
    return 0;
}


const char* quosi_error_to_string(quosiErrorValue e) {
    switch (e.type) {
    case QUOSI_ERR_UNREACHABLE:
        return "INTERNAL PROGRAMMING ERROR - UNREACHABLE REACHED";
    case QUOSI_ERR_UNKNOWN:
        return "TODO: label error";

    case QUOSI_ERR_EARLY_EOF:
        return "end of file encountered abruptly";
    case QUOSI_ERR_INVALID_TOKEN:
        return "invalid token encountered";
    case QUOSI_ERR_MISPLACED_TOKEN:
        return "unexpected token encountered";

    case QUOSI_ERR_BAD_RENAME:
        return "'rename' declaration is malformed";
    case QUOSI_ERR_BAD_GRAPH_BEGIN:
        return "expected 'module' declaration";
    case QUOSI_ERR_BAD_VERTEX_BEGIN:
        return "expected node or 'rename' declaration";
    case QUOSI_ERR_BAD_VERTEX_BLOCK:
        return "expected 'if', 'match' or node definition";
    case QUOSI_ERR_BAD_EDGE_BLOCK:
        return "expected 'if', 'match' or edge definition";
    case QUOSI_ERR_BAD_MATCH_ARM:
        return "invalid match arm definition";

    case QUOSI_ERR_NO_ENTRY:
        return "must have one initial node 'START'";
    case QUOSI_ERR_NO_ELSE:
        return "'if' in node definition requires catchall case 'else'";
    case QUOSI_ERR_NO_CATCHALL:
        return "'match' in node definition requires catchall case '_'";
    case QUOSI_ERR_DUPLICATE_CASE:
        return "'match' expression canot have duplicate cases";
    case QUOSI_ERR_DUPLICATE_VERTEX:
        return "cannot have duplicate node name";
    case QUOSI_ERR_DANGLING_EDGE:
        return "node being pointed to does not exist";

    case QUOSI_ERR_INVALID_ATOM:
        return "atom in expression must be number, identifier, or (expression)";
    case QUOSI_ERR_INVALID_OPERATOR:
        return "operator in expression is not a valid operator";
    case QUOSI_ERR_INVALID_ASSIGN:
        return "left hand side of assignment expression must be an identifier";

    case QUOSI_ERR_UNCLOSED_PAREN:
        return "opening parenthese must be closed here";
    case QUOSI_ERR_UNCLOSED_ANGLE:
        return "opening angle brackets must be closed here";
    case QUOSI_ERR_UNCLOSED_CONDITIONAL:
        return "conditional was opened with no corresponding 'end' label";

    default:
        return "INTERNAL PROGRAMMING ERROR - NON-EXHAUSTIVE SWITCH CASE";
    }
}

bool quosi_error_is_critical(quosiErrorValue e) {
    switch (e.type) {
    case QUOSI_ERR_UNKNOWN:
        return true;

    case QUOSI_ERR_EARLY_EOF:
        return true;
    case QUOSI_ERR_INVALID_TOKEN:
        return true;
    case QUOSI_ERR_MISPLACED_TOKEN:
        return true;

    case QUOSI_ERR_BAD_RENAME:
        return true;
    case QUOSI_ERR_BAD_GRAPH_BEGIN:
        return true;
    case QUOSI_ERR_BAD_VERTEX_BEGIN:
        return true;
    case QUOSI_ERR_BAD_VERTEX_BLOCK:
        return true;
    case QUOSI_ERR_BAD_EDGE_BLOCK:
        return true;
    case QUOSI_ERR_BAD_MATCH_ARM:
        return true;

    case QUOSI_ERR_NO_ENTRY:
        return false;
    case QUOSI_ERR_NO_ELSE:
        return false;
    case QUOSI_ERR_NO_CATCHALL:
        return false;
    case QUOSI_ERR_DUPLICATE_CASE:
        return false;
    case QUOSI_ERR_DUPLICATE_VERTEX:
        return false;
    case QUOSI_ERR_DANGLING_EDGE:
        return false;

    case QUOSI_ERR_INVALID_ATOM:
        return true;
    case QUOSI_ERR_INVALID_OPERATOR:
        return true;
    case QUOSI_ERR_INVALID_ASSIGN:
        return false;

    case QUOSI_ERR_UNCLOSED_PAREN:
        return true;
    case QUOSI_ERR_UNCLOSED_ANGLE:
        return true;
    case QUOSI_ERR_UNCLOSED_CONDITIONAL:
        return true;

    default:
        return true;
    }
}

