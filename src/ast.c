#include "quosi/ast.h"
#include "quosi/bc.h"
#include "quosi/quosi.h"
#define QUOSIDS_ALLOCATOR alloc
#include "vec.h"
#include <stdio.h>


static void quosi_expr_free(quosiExpr* expr, quosiAllocator alloc) {
    if (expr->tag == QUOSI_EXPR_OP) {
        quosi_expr_free(expr->lhs, alloc);
        quosi_allocator_deallocate(alloc, expr->lhs);
        if (expr->value.op != QUOSI_INSTR_NEG && expr->value.op != QUOSI_INSTR_LNOT) {
            quosi_expr_free(expr->rhs, alloc);
            quosi_allocator_deallocate(alloc, expr->rhs);
        }
    }
}

static void quosi_edgeblock_free(quosiEdgeBlock* block, quosiAllocator alloc);
static void quosi_edge_free(quosiEdge* edge, quosiAllocator alloc) {
    for (size_t i = 0; i < quosids_arrlenu(edge->effects); i++) {
        if (edge->effects[i].op != QUOSI_EFFECT_EVENT) {
            quosi_expr_free(&edge->effects[i].rhs, alloc);
        }
    }
    quosids_arrfree(edge->effects);
}
static void quosi_edgematch_free(quosiEdgeMatch* match, quosiAllocator alloc) {
    quosi_expr_free(&match->expr, alloc);
    for (size_t i = 0; i < quosids_arrlenu(match->arms); i++) {
        quosi_expr_free(&match->arms[i].cond, alloc);
        quosi_edge_free(&match->arms[i].body, alloc);
    }
    quosids_arrfree(match->arms);
    if (match->catchall.exists) {
        quosi_edge_free(&match->catchall.arm, alloc);
    }
}
static void quosi_edgeifelse_free(quosiEdgeIfElse* ifelse, quosiAllocator alloc) {
    for (size_t i = 0; i < quosids_arrlenu(ifelse->blocks); i++) {
        quosi_expr_free(&ifelse->blocks[i].cond, alloc);
        for (size_t j = 0; j < quosids_arrlenu(ifelse->blocks[i].body); j++) {
            quosi_edgeblock_free(&ifelse->blocks[i].body[j], alloc);
        }
        quosids_arrfree(ifelse->blocks[i].body);
    }
    quosids_arrfree(ifelse->blocks);
    if (ifelse->catchall) {
        for (size_t i = 0; i < quosids_arrlenu(ifelse->catchall); i++) {
            quosi_edgeblock_free(&ifelse->catchall[i], alloc);
        }
        quosids_arrfree(ifelse->catchall);
    }
}

static void quosi_edgeblock_free(quosiEdgeBlock* block, quosiAllocator alloc) {
    switch (block->tag) {
    case QUOSI_EBLOCK_T:
        for (size_t i = 0; i < quosids_arrlenu(block->value.edges); i++) {
            quosi_edge_free(&block->value.edges[i], alloc);
        }
        quosids_arrfree(block->value.edges);
        break;
    case QUOSI_EBLOCK_MATCH:
        quosi_edgematch_free(&block->value.match, alloc);
        break;
    case QUOSI_EBLOCK_IFELSE:
        quosi_edgeifelse_free(&block->value.ifelse, alloc);
        break;
    }
}

static void quosi_vertblock_free(quosiVertexBlock* block, quosiAllocator alloc);
static void quosi_vertex_free(quosiVertex* vert, quosiAllocator alloc) {
    for (size_t i = 0; i < quosids_arrlenu(vert->lineset); i++) {
        quosids_arrfree(vert->lineset[i].lines);
    }
    quosids_arrfree(vert->lineset);
    for (size_t i = 0; i < quosids_arrlenu(vert->edges); i++) {
        quosi_edgeblock_free(&vert->edges[i], alloc);
    }
    if (vert->type == QUOSI_VERTEX_JUMP) {
        quosids_arrfree(vert->jump.effects);
    } else {
        quosids_arrfree(vert->edges);
    }
}
static void quosi_vertmatch_free(quosiVertexMatch* match, quosiAllocator alloc) {
    quosi_expr_free(&match->expr, alloc);
    for (size_t i = 0; i < quosids_arrlenu(match->arms); i++) {
        quosi_expr_free(&match->arms[i].cond, alloc);
        quosi_vertex_free(&match->arms[i].body, alloc);
    }
    quosids_arrfree(match->arms);
    quosi_vertex_free(&match->catchall, alloc);
}
static void quosi_vertifelse_free(quosiVertexIfElse* ifelse, quosiAllocator alloc) {
    for (size_t i = 0; i < quosids_arrlenu(ifelse->blocks); i++) {
        quosi_expr_free(&ifelse->blocks[i].cond, alloc);
        quosi_vertblock_free(ifelse->blocks[i].data, alloc);
        quosi_allocator_deallocate(alloc, ifelse->blocks[i].data);
    }
    quosids_arrfree(ifelse->blocks);
    quosi_vertblock_free(ifelse->catchall, alloc);
    quosi_allocator_deallocate(alloc, ifelse->catchall);
}

static void quosi_vertblock_free(quosiVertexBlock* block, quosiAllocator alloc) {
    switch (block->tag) {
    case QUOSI_VBLOCK_T:
        quosi_vertex_free(&block->value.vertex, alloc);
        break;
    case QUOSI_VBLOCK_MATCH:
        quosi_vertmatch_free(&block->value.match, alloc);
        break;
    case QUOSI_VBLOCK_IFELSE:
        quosi_vertifelse_free(&block->value.ifelse, alloc);
        break;
    }
}

static void quosi_graph_free(quosiGraph* graph, quosiAllocator alloc) {
    for (size_t i = 0; i < quosids_arrlenu(graph->vertices); i++) {
        quosi_vertblock_free(&graph->vertices[i].data, alloc);
    }
    quosids_arrfree(graph->vertices);
}

void quosi_ast_free(const quosiAst* _ast, quosiAllocator alloc) {
    quosiAst* ast = (quosiAst*)_ast;
    for (size_t i = 0; i < quosids_arrlenu(ast->modules); i++) {
        quosi_graph_free(&ast->modules[i], alloc);
    }
    quosids_arrfree(ast->modules);
}

