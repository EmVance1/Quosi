#define _CRT_SECURE_NO_WARNINGS
#define _GNU_SOURCE
// #define VANGO_TEST_MEMORY_LEAKS
#define VANGO_BENCH_WARMUP 1000
#define VANGO_TEST_ROOT
#include <vangotest/casserts2.h>
#include <vangotest/bench.h>
#include "quosi/quosi.h"
#include "quosi/ast.h"
#include "quosi/vm.h"
#include <stdlib.h>
#include <stdio.h>


static char* read_to_string(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    const size_t size = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)calloc(size + 1, sizeof(char));
    fread(buf, sizeof(char), size, f);
    fclose(f);
    return buf;
}


uint32_t dummy_ctxf(const char* key) { (void)key; return 0; }
quosiSymbolCtx dummy_ctx = { dummy_ctxf, dummy_ctxf };
uint64_t* vm_ctx(uint32_t key) { (void)key; static uint64_t val = 0; return &val; }

vango_test(cmp_example) {
    char* src = read_to_string("examples/doall.qsi");
    vg_assert_non_null(src);
    quosiError errors = { 0 };
    quosiFile* file = quosi_file_compile_from_src(src, &errors, dummy_ctx, quosi_malloc_allocator());
    free(src);

    if (errors.list != NULL) {
        for (size_t i = 0; i < quosi_error_list_len(&errors); i++) {
            printf("quosi compile error (%u:%u): %s\n", errors.list[i].span.row, errors.list[i].span.col, quosi_error_to_string(errors.list[i]));
        }
        quosi_error_list_free(&errors);
        vg_assert(false);
    }

    quosi_file_prettyprint(file, "Default", stdout);
    quosi_allocator_deallocate(quosi_malloc_allocator(), file);
}

vango_test(run_example) {
    char* src = read_to_string("examples/doall.qsi");
    vg_assert_non_null(src);
    quosiError errors = { 0 };
    quosiFile* file = quosi_file_compile_from_src(src, &errors, dummy_ctx, quosi_malloc_allocator());
    free(src);

    quosiVm vm;
    quosi_vm_init(&vm, file, "Default");

    while (true) {
        switch (quosi_vm_exec(&vm, vm_ctx)) {
        case QUOSI_UPCALL_LINE:
            printf("%u: \"%s\"\n", quosi_vm_id(&vm), quosi_vm_line(&vm));
            break;
        case QUOSI_UPCALL_PICK: {
            uint32_t pindex[QUOSI_PROP_QUEUE_SIZE];
            for (uint32_t i = 0; i < quosi_vm_nq(&vm); i++) {
                const quosiProposition prop = quosi_vm_dequeue_text(&vm);
                printf("  %u: \"%s\"\n", i+1, prop.str);
                pindex[i] = prop.idx;
            }
            printf("\n> ");
            uint32_t pick;
            scanf("%u", &pick);
            printf("\n");
            quosi_vm_push_value(&vm, pindex[pick-1]);
            break; }
        case QUOSI_UPCALL_EVENT:
            printf("EVENT: %s\n\n", quosi_vm_line(&vm));
            break;
        case QUOSI_UPCALL_EXIT:
            printf("\nEOF\n");
            quosi_allocator_deallocate(quosi_malloc_allocator(), file);
            return;
        default:
            printf("\nERROR\n");
            quosi_allocator_deallocate(quosi_malloc_allocator(), file);
            vg_assert(false);
        }
    }
}

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

    vango_bench(100000, {
        quosi_memory_arena_reset(&arena);
        file = quosi_file_compile_from_src(src, &errors, dummy_ctx, quosi_memory_arena_allocator(&arena));
    });

    vg_assert_non_null(file);
    quosi_memory_arena_destroy(&arena);
    free(src);
}

