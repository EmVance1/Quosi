#pragma once
#include <filesystem>
#include <cstdint>
#include "error.h"


namespace quosi {

namespace ast { struct Graph; }

using SymbolContext = uint32_t(*)(const char* key);

struct FileImplT;

class File {
public:
    struct Header {
        uint16_t majver;
        uint16_t minver;
        uint16_t patch;
        char magic[5];
        size_t data_len;
        size_t str_loc;
        size_t sym_loc;
    };
private:
    File(FileImplT* impl, bool manage);

    FileImplT* m_impl = nullptr;
    bool m_owns_ptr = true;

public:
    File() = default;
    File(const File&) = delete;
    File(File&&) noexcept;
    ~File();

    File& operator=(const File&) = delete;
    File& operator=(File&&) noexcept;

    static File compile_from_src(const char* src, ErrorList& errors, SymbolContext ctx = nullptr);
    static File compile_from_ast(const ast::Graph& graph, SymbolContext ctx = nullptr);
    static File load_from_file(const std::filesystem::path& path);
    static File init_from_raw(void* bin, bool manage = true);

    void write_to_file(const std::filesystem::path& path) const;
    void prettyprint() const;

    const File::Header& header() const;
    const uint8_t* data() const;
    size_t data_len() const;
    const uint8_t* code() const;
    size_t code_len() const;
    const uint8_t* strs() const;
    size_t strs_len() const;
    const uint8_t* syms() const;
    size_t syms_len() const;

    const uint8_t* end() const;
};

}
