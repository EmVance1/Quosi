#include "cquosi/quosi.h"
#include "vec.h"


const char* quosi_error_to_string(quosiErrorValue e) {
    switch (e.type) {
    case QUOSI_ERR_UNKNOWN:
        return "TODO: label error";

    case QUOSI_ERR_EARLY_EOF:
        return "end of file encountered abruptly";
    case QUOSI_ERR_MISPLACED_TOKEN:
        return "unexpected token encountered";
    case QUOSI_ERR_BAD_GRAPH_BEGIN:
        return "expected 'module' declaration";
    case QUOSI_ERR_BAD_VERTEX_BEGIN:
        return "expected node or 'rename' declaration";
    case QUOSI_ERR_BAD_RENAME:
        return "'rename' declaration is malformed";
    case QUOSI_ERR_NO_ELSE:
        return "'if' in node definition requires catchall case 'else'";
    case QUOSI_ERR_NO_CATCHALL:
        return "'match' in node definition requires catchall case '_'";
    case QUOSI_ERR_DUPLICATE_CASE:
        return "'match' expression canot have duplicate cases";
    case QUOSI_ERR_NO_ENTRY:
        return "must have one initial node 'START'";
    case QUOSI_ERR_DUPLICATE_VERTEX:
        return "cannot have duplicate node name";
    case QUOSI_ERR_DANGLING_EDGE:
        return "node being pointed to does not exist";

    default:
        return "";
    }
}

bool quosi_error_is_critical(quosiErrorValue e) {
    switch (e.type) {
    case QUOSI_ERR_UNKNOWN:
        return true;

    case QUOSI_ERR_EARLY_EOF:
        return true;
    case QUOSI_ERR_MISPLACED_TOKEN:
        return true;
    case QUOSI_ERR_BAD_GRAPH_BEGIN:
        return true;
    case QUOSI_ERR_BAD_VERTEX_BEGIN:
        return true;
    case QUOSI_ERR_BAD_RENAME:
        return true;
    case QUOSI_ERR_NO_ELSE:
        return false;
    case QUOSI_ERR_NO_CATCHALL:
        return false;
    case QUOSI_ERR_DUPLICATE_CASE:
        return false;
    case QUOSI_ERR_NO_ENTRY:
        return false;
    case QUOSI_ERR_DUPLICATE_VERTEX:
        return false;
    case QUOSI_ERR_DANGLING_EDGE:
        return false;

    default:
        return true;
    }
}


void quosi_error_list_push(quosiError* errors, quosiErrorValue err) {
    quosids_arrpush(errors->list, err);
}

size_t quosi_error_list_len(const quosiError* errors) {
    return quosids_arrlenu(errors->list);
}

void quosi_error_list_free(quosiError* errors) {
    quosids_arrfree(errors->list);
}

