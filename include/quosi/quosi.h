#ifndef CQUOSI_LIB_H
#define CQUOSI_LIB_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


typedef struct quosiStrView {
    const char* ptr;
    size_t len;
} quosiStrView;

typedef struct quosiErrorSpan {
    uint32_t row;
    uint32_t col;
} quosiErrorSpan;

typedef struct quosiErrorValue {
    enum quosiErrorValueType {
        QUOSI_ERR_UNREACHABLE = -1,
        QUOSI_ERR_UNKNOWN = 0,
        QUOSI_ERR_EARLY_EOF,
        QUOSI_ERR_INVALID_TOKEN,
        QUOSI_ERR_MISPLACED_TOKEN,
        QUOSI_ERR_BAD_RENAME,
        QUOSI_ERR_BAD_GRAPH_BEGIN,
        QUOSI_ERR_BAD_VERTEX_BEGIN,
        QUOSI_ERR_BAD_VERTEX_BLOCK,
        QUOSI_ERR_BAD_EDGE_BLOCK,
        QUOSI_ERR_BAD_MATCH_ARM,
        QUOSI_ERR_NO_ENTRY,
        QUOSI_ERR_NO_ELSE,
        QUOSI_ERR_NO_CATCHALL,
        QUOSI_ERR_DUPLICATE_CASE,
        QUOSI_ERR_DUPLICATE_VERTEX,
        QUOSI_ERR_DANGLING_EDGE,
        QUOSI_ERR_INVALID_ATOM,
        QUOSI_ERR_INVALID_OPERATOR,
        QUOSI_ERR_INVALID_ASSIGN,
        QUOSI_ERR_UNCLOSED_PAREN,
        QUOSI_ERR_UNCLOSED_ANGLE,
        QUOSI_ERR_UNCLOSED_CONDITIONAL,
    } type;
    quosiErrorSpan span;
} quosiErrorValue;

const char* quosi_error_to_string(quosiErrorValue e);
bool quosi_error_is_critical(quosiErrorValue e);

// error list result from parsing. if no errors were found, list == NULL.
typedef struct quosiError  {
    quosiErrorValue* list;
    bool critical;
} quosiError;

void quosi_error_list_push(quosiError* errors, quosiErrorValue err);
size_t quosi_error_list_len(const quosiError* errors);
void quosi_error_list_free(quosiError* errors);


// polymorphic allocator vtable, primarily for internal use
typedef struct quosiAllocator {
    void* impl;
    void*(*allocate)  (void* self, size_t nbytes);
    void (*deallocate)(void* self, void* ptr);
    void*(*reallocate)(void* self, void* oldptr, size_t oldbytes, size_t newbytes);
} quosiAllocator;

void* quosi_allocator_allocate  (quosiAllocator alloc, size_t nbytes);
void  quosi_allocator_deallocate(quosiAllocator alloc, void* ptr);
void* quosi_allocator_reallocate(quosiAllocator alloc, void* oldptr, size_t oldbytes, size_t newbytes);

enum quosiMemoryArenaSource {
    // enables growing, increased syscall overhead
    QUOSI_MEMORY_ARENA_PAGE,
     // disables growing, decreased syscall overhead, prefer for smaller workloads
    QUOSI_MEMORY_ARENA_MALLOC,
};
enum quosiMemoryArenaGrowthPolicy {
     // fixed size determined at creation time
    QUOSI_MEMORY_ARENA_STATIC,
     // grow linearly in increments of 'initial'
    QUOSI_MEMORY_ARENA_LINEAR,
     // grow geometrically in exponents of 'initial'
    QUOSI_MEMORY_ARENA_GEOMETRIC,
};
typedef struct quosiMemoryArena {
    uint8_t* base_ptr;
    uint8_t* curr_ptr;
    uint8_t* last_ptr;
    size_t blocksize;
    size_t reserve;
    size_t commit;
    int source;
    int policy;
} quosiMemoryArena;

typedef struct quosiMemoryArenaConfig {
    // initial buffer size of the arena, rounded up to a multiple of the system page size
    size_t initial;
    // number of virtual pages to reserve; does not apply when source == QUOSI_MEMORY_ARENA_MALLOC
    size_t max_reserve;
    // see 'quosiMemoryArenaGrowthPolicy', always STATIC when growth_policy == QUOSI_MEMORY_ARENA_MALLOC
    int source;
    // defers initial buffer allocation to the first object allocation call
    int lazyinit;
    // growth function of the buffer; does nothing when source == QUOSI_MEMORY_ARENA_MALLOC
    int growth_policy;
} quosiMemoryArenaConfig;

quosiMemoryArena quosi_memory_arena_create(int source, size_t initial);
quosiMemoryArena quosi_memory_arena_createex(quosiMemoryArenaConfig cfg);
// free all memory belonging to arena
void quosi_memory_arena_destroy(quosiMemoryArena* self);
// resets all pointers, may free excess buffer space
void quosi_memory_arena_reset(quosiMemoryArena* self);

quosiAllocator quosi_memory_arena_allocator(quosiMemoryArena* arena);
quosiAllocator quosi_malloc_allocator(void);


struct quosiAst;
struct quosiFile;
typedef struct quosiFile quosiFile;
typedef struct quosiSymbolCtx {
    // maps script variables to integer IDs
    uint32_t(*data_lkp)(const char*);
    // maps speaker names to integer IDs
    uint32_t(*speaker_lkp)(const char*);
} quosiSymbolCtx;

// metadata for the compiled binary, always makes up first N bytes of the blob
typedef struct quosiFileHeader {
    char magic[5];
    uint16_t majver;
    uint16_t minver;
    uint16_t patch;
    uint32_t fsize;
    uint32_t nmods;
    uint32_t code_pos;
    uint32_t strs_pos;
    uint32_t syms_pos;
} quosiFileHeader;

// metadata for a single module, entry is an offset from *code, not *file
typedef struct quosiFileModTableEntry {
    const uint8_t* code;
    uint32_t len;
    uint32_t entry;
} quosiFileModTableEntry;

// return complete compiled binary as single contiguous blob, including header, module table, modules and strings
quosiFile* quosi_file_compile_from_src(const char* src, quosiError* errors, quosiSymbolCtx ctx, quosiAllocator alloc);
// return complete compiled binary as single contiguous blob, including header, module table, modules and strings
quosiFile* quosi_file_compile_from_ast(const struct quosiAst* ast, quosiSymbolCtx ctx, quosiAllocator alloc);
// outputs human readable (asm-like) representation of a single module
void quosi_file_prettyprint(const quosiFile* file, const char* module, void* stdstream);

const quosiFileHeader* quosi_file_header(const quosiFile* file);
quosiFileModTableEntry quosi_file_module(const quosiFile* file, const char* module);

// one byte past the end of the whole blob
const uint8_t* quosi_file_end(const quosiFile* file);
size_t quosi_file_len(const quosiFile* file);

const uint8_t* quosi_file_mod_table(const quosiFile* file);
size_t quosi_file_mod_table_len(const quosiFile* file);
const uint8_t* quosi_file_code(const quosiFile* file);
size_t quosi_file_code_len(const quosiFile* file);
const uint8_t* quosi_file_strs(const quosiFile* file);
size_t quosi_file_strs_len(const quosiFile* file);
const uint8_t* quosi_file_syms(const quosiFile* file);
size_t quosi_file_syms_len(const quosiFile* file);


#endif
