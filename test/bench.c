
#if 0

#define _CRT_SECURE_NO_WARNINGS
#define _GNU_SOURCE
#define VANGO_BENCH_WARMUP 1000
#include <vangotest/casserts2.h>
#include <vangotest/bench.h>
#include "quosi/quosi.h"
#include "quosi/ast.h"
#include <stdlib.h>
#include "fsutil.h"


static uint32_t dummy_ctxf(const char* key) { (void)key; return 0; }
static quosiSymbolCtx dummy_ctx = { dummy_ctxf, dummy_ctxf };

vango_test(bench_memory) {
    char* src = read_to_string("examples/large.qsi");
    vg_assert_non_null(src);
    quosiError errors = { 0 };
    quosiMemoryArena arena = quosi_memory_arena_create(QUOSI_MEMORY_ARENA_PAGE, 200 * 1000);
    quosiAst ast;

    vango_bench(10000, {
        quosi_memory_arena_reset(&arena);
        ast = quosi_ast_parse_from_src(src, &errors, quosi_memory_arena_allocator(&arena));
    });
    vango_bench(10000, {
        ast = quosi_ast_parse_from_src(src, &errors, quosi_malloc_allocator());
        quosi_ast_free(&ast, quosi_malloc_allocator());
    });

    quosi_memory_arena_destroy(&arena);
    if (errors.list) {
        quosi_error_list_free(&errors);
        vg_assert(false);
    }
    free(src);
}

vango_test(bench_parser) {
    char* src = read_to_string("examples/large.qsi");
    vg_assert_non_null(src);
    quosiError errors = { 0, 0 };
    quosiMemoryArena arena = quosi_memory_arena_create(QUOSI_MEMORY_ARENA_PAGE, 100 * 1000);
    quosiAst ast;

    ast = quosi_ast_parse_from_src(src, &errors, quosi_malloc_allocator());
    quosi_ast_free(&ast, quosi_malloc_allocator());

    vango_bench(10000, {
        quosi_memory_arena_reset(&arena);
        ast = quosi_ast_parse_from_src(src, &errors, quosi_memory_arena_allocator(&arena));
    });

    vg_assert_null(errors.list);
    quosi_memory_arena_destroy(&arena);
    free(src);
}

vango_test(bench_codegen) {
    char* src = read_to_string("examples/large.qsi");
    quosiError errors = { 0 };
    quosiMemoryArena ast_arena = quosi_memory_arena_create(QUOSI_MEMORY_ARENA_PAGE, 100 * 1000);
    const quosiAst ast = quosi_ast_parse_from_src(src, &errors, quosi_memory_arena_allocator(&ast_arena));
    quosiFile* file = NULL;

    quosiMemoryArena cmp_arena = quosi_memory_arena_create(QUOSI_MEMORY_ARENA_PAGE, 100 * 1000);

    vango_bench(10000, {
        quosi_memory_arena_reset(&cmp_arena);
        file = quosi_file_compile_from_ast(&ast, dummy_ctx, quosi_memory_arena_allocator(&cmp_arena));
    });

    vg_assert_non_null(file);
    quosi_memory_arena_destroy(&ast_arena);
    quosi_memory_arena_destroy(&cmp_arena);
    free(src);
}

vango_test(bench_compiler) {
    char* src = read_to_string("examples/large.qsi");
    quosiError errors = { 0 };
    quosiMemoryArena arena = quosi_memory_arena_create(QUOSI_MEMORY_ARENA_MALLOC, 100 * 1000);
    quosiFile* file = NULL;

    vango_bench(10000, {
        quosi_memory_arena_reset(&arena);
        file = quosi_file_compile_from_src(src, &errors, dummy_ctx, quosi_memory_arena_allocator(&arena));
    });

    vg_assert_non_null(file);
    quosi_memory_arena_destroy(&arena);
    free(src);
}

#else

void _filler(void) {}

#endif
