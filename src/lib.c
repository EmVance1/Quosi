#define _CRT_SECURE_NO_WARNINGS
#include "quosi/quosi.h"
#include "quosi/ast.h"
#include "quosi/bc.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#define QUOSIDS_ALLOCATOR (ctx->alloc)
#include "vec.h"


quosiFile* quosi_file_internal_merge_blobs(const quosiProgramData* pdata, quosiAllocator alloc);

quosiFile* quosi_file_compile_from_src(const char* src, quosiError* errors, quosiSymbolCtx symbol_ctx, quosiAllocator alloc) {
    *errors = (quosiError){ 0 };
    quosiMemoryArena ast_arena = quosi_memory_arena_create(QUOSI_MEMORY_ARENA_PAGE, 100 * 1000);
    const quosiAst ast = quosi_ast_parse_from_src(src, errors, quosi_memory_arena_allocator(&ast_arena));

    if (errors->list == NULL) {
        quosiMemoryArena pdata_arena = quosi_memory_arena_create(QUOSI_MEMORY_ARENA_PAGE, 100 * 1000);
        quosiProgramData pdata = quosi_compile_ast(&ast, symbol_ctx, quosi_memory_arena_allocator(&pdata_arena));
        quosi_memory_arena_destroy(&ast_arena);
        quosiFile* result = quosi_file_internal_merge_blobs(&pdata, alloc);
        quosi_memory_arena_destroy(&pdata_arena);
        return result;
    } else {
        quosi_memory_arena_destroy(&ast_arena);
        return NULL;
    }
}

quosiFile* quosi_file_compile_from_ast(const quosiAst* ast, quosiSymbolCtx symbol_ctx, quosiAllocator alloc) {
    quosiMemoryArena arena = quosi_memory_arena_create(QUOSI_MEMORY_ARENA_PAGE, 100 * 1000);
    quosiProgramData pdata = quosi_compile_ast(ast, symbol_ctx, quosi_memory_arena_allocator(&arena));
    quosiFile* result = quosi_file_internal_merge_blobs(&pdata, alloc);
    quosi_memory_arena_destroy(&arena);
    return result;
}

quosiFile* quosi_file_internal_merge_blobs(const quosiProgramData* pdata, quosiAllocator alloc) {
    const size_t meta_size = sizeof(quosiFileHeader) + quosids_arrlenu(pdata->syms);
    size_t mods_size = 0;
    size_t code_size = 0;
    size_t strs_size = quosids_arrlenu(pdata->strs);
    for (size_t i = 0; i < quosids_arrlenu(pdata->mods); i++) {
        mods_size += 4 * sizeof(uint32_t);
        code_size += quosids_arrlenu(pdata->mods[i].code);
        strs_size += (pdata->mods[i].name.len + 1);
    }
    const size_t file_size = meta_size + mods_size + code_size + strs_size;

    uint8_t* base_ptr = quosi_allocator_allocate(alloc, file_size);
    quosiFileHeader* header = (quosiFileHeader*)base_ptr;
    *header = (quosiFileHeader){
        .magic={ 'q', 'u', 'o', 's', 'i' },
        .majver=VANGO_PKG_VERSION_MAJOR,
        .minver=VANGO_PKG_VERSION_MINOR,
        .patch =VANGO_PKG_VERSION_PATCH,
        .fsize =(uint32_t)file_size,
        .nmods =(uint32_t)quosids_arrlenu(pdata->mods),
        .code_pos=(uint32_t)(sizeof(quosiFileHeader) + mods_size),
        .strs_pos=(uint32_t)(sizeof(quosiFileHeader) + mods_size + code_size),
        .syms_pos=(uint32_t)(sizeof(quosiFileHeader) + mods_size + code_size + strs_size),
    };

    uint8_t* current = base_ptr + sizeof(quosiFileHeader);
    uint32_t code_pos = header->code_pos;
    uint32_t name_pos = header->strs_pos + (uint32_t)quosids_arrlenu(pdata->strs);
    for (size_t i = 0; i < quosids_arrlenu(pdata->mods); i++) {
        const quosiModData* g = &pdata->mods[i];
        const uint32_t code_len = (uint32_t)quosids_arrlenu(g->code);
        memcpy(current,                        &name_pos, sizeof(uint32_t));
        memcpy(current + 1 * sizeof(uint32_t), &code_pos, sizeof(uint32_t));
        memcpy(current + 2 * sizeof(uint32_t), &code_len, sizeof(uint32_t));
        memcpy(current + 3 * sizeof(uint32_t), &g->entry, sizeof(uint32_t));
        current += 4 * sizeof(uint32_t);
        code_pos += code_len;

        memcpy(base_ptr + name_pos, g->name.ptr, g->name.len);
        base_ptr[name_pos + g->name.len] = 0;

        name_pos += (uint32_t)g->name.len + 1;
    }

    current = base_ptr + header->code_pos;
    for (size_t i = 0; i < quosids_arrlenu(pdata->mods); i++) {
        const quosiModData* g = &pdata->mods[i];
        const size_t code_len = quosids_arrlenu(g->code);
        memcpy(current, g->code, code_len);
        current += code_len;
    }

    memcpy(base_ptr + header->strs_pos, pdata->strs, quosids_arrlenu(pdata->strs));
    memcpy(base_ptr + header->syms_pos, pdata->syms, quosids_arrlenu(pdata->syms));
    return (quosiFile*)base_ptr;
}


const quosiFileHeader* quosi_file_header(const quosiFile* file) {
    return (quosiFileHeader*)file;
}
quosiFileModTableEntry quosi_file_module(const quosiFile* file, const char* module) {
    const uint8_t* base_ptr = (const uint8_t*)file;
    const uint8_t* ptr = quosi_file_mod_table(file);
    for (uint32_t i = 0; i < quosi_file_header(file)->nmods; i++) {
        uint32_t name_pos, code_pos, code_len, code_beg;
        memcpy(&name_pos, ptr,                        sizeof(uint32_t));
        memcpy(&code_pos, ptr + 1 * sizeof(uint32_t), sizeof(uint32_t));
        memcpy(&code_len, ptr + 2 * sizeof(uint32_t), sizeof(uint32_t));
        memcpy(&code_beg, ptr + 3 * sizeof(uint32_t), sizeof(uint32_t));
        const char* name = (const char*)base_ptr + name_pos;
        const size_t name_len = strlen((const char*)ptr);
        if (strncmp(name, module, name_len+1) == 0) {
            return (quosiFileModTableEntry){ .code=base_ptr+code_pos, .len=code_len, .entry=code_beg };
        }
        ptr += 4 * sizeof(uint32_t);
    }
    return (quosiFileModTableEntry){ 0 };
}

const uint8_t* quosi_file_end(const quosiFile* file) {
    return (uint8_t*)file + quosi_file_header(file)->fsize;
}
size_t quosi_file_len(const quosiFile* file) {
    return quosi_file_header(file)->fsize;
}

const uint8_t* quosi_file_mod_table(const quosiFile* file) {
    return (uint8_t*)file + sizeof(quosiFileHeader);
}
size_t quosi_file_mod_table_len(const quosiFile* file) {
    return (size_t)(quosi_file_code(file) - quosi_file_mod_table(file));
}
const uint8_t* quosi_file_code(const quosiFile* file) {
    return (uint8_t*)file + quosi_file_header(file)->code_pos;
}
size_t quosi_file_code_len(const quosiFile* file) {
    return (size_t)(quosi_file_strs(file) - quosi_file_code(file));
}
const uint8_t* quosi_file_strs(const quosiFile* file) {
    return (uint8_t*)file + quosi_file_header(file)->strs_pos;
}
size_t quosi_file_strs_len(const quosiFile* file) {
    return (size_t)(quosi_file_syms(file) - quosi_file_strs(file));
}
const uint8_t* quosi_file_syms(const quosiFile* file) {
    return (uint8_t*)file + quosi_file_header(file)->syms_pos;
}
size_t quosi_file_syms_len(const quosiFile* file) {
    return (size_t)(quosi_file_end(file) - quosi_file_syms(file));
}

