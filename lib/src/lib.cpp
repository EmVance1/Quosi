#include "quosi/quosi.h"
#include "quosi/ast.h"
#include "quosi/bc.h"
#include <fstream>
#include <cstdlib>
#include <iostream>


namespace quosi {

struct FileImplT {
    File::Header header;
    size_t marker;
};

File::File(FileImplT* impl, bool manage)
    : m_impl(impl), m_owns_ptr(manage)
{}

File::File(File&& other) noexcept
    : m_impl(other.m_impl), m_owns_ptr(other.m_owns_ptr)
{
    other.m_impl = nullptr;
}

File::~File() {
    if (m_owns_ptr) {
        free(m_impl);
    }
}

File& File::operator=(File&& other) noexcept {
    this->~File();
    m_impl = other.m_impl;
    m_owns_ptr = other.m_owns_ptr;
    other.m_impl = nullptr;
    return *this;
}


File File::compile_from_src(const char* src, ErrorList& errors, SymbolContext ctx) {
    const auto ast = ast::parse(src, errors);
    if (errors.list.empty()) {
        return compile_from_ast(*ast, ctx);
    } else {
        return File(nullptr, true);
    }
}

File File::compile_from_ast(const ast::Ast& ast, SymbolContext ctx) {
    const auto pdata = bc::compile_ast(ast, ctx);
    const auto size = (size_t)(sizeof(File::Header) + pdata.header.data_len);
    auto result = File((FileImplT*)malloc((size_t)size), true);
    memcpy((char*)result.m_impl, &pdata.header, sizeof(File::Header));
    memcpy((char*)result.m_impl + sizeof(File::Header), pdata.data, pdata.header.data_len);
    return result;
}

File File::load_from_file(const std::filesystem::path& path) {
    auto f = std::ifstream(path.c_str(), std::ios::binary);
    if (!f.is_open()) return File(nullptr, true);
    f.seekg(0, std::ios::end);
    const auto size = f.tellg();
    f.seekg(0, std::ios::beg);
    auto result = File((FileImplT*)malloc((size_t)size), true);
    f.read((char*)result.m_impl, size);
    return result;
}

File File::init_from_raw(void* bin, bool manage) {
    return File((FileImplT*)bin, manage);
}

void File::write_to_file(const std::filesystem::path& path) const {
    auto f = std::ofstream(path.c_str(), std::ios::binary);
    if (!f.is_open()) return;
    f.write((char*)m_impl, (std::streamsize)(sizeof(File::Header) + m_impl->header.data_len));
}

void File::prettyprint() const {
    quosi::bc::prettyprint(data(), data_len());
}


const File::Header& File::header() const {
    return m_impl->header;
}
const uint8_t* File::data() const {
    return (const uint8_t*)&m_impl->marker;
}
size_t File::data_len() const {
    return m_impl->header.data_len;
}
const uint8_t* File::code() const {
    return data();
}
size_t File::code_len() const {
    return (size_t)(strs() - code());
}
const uint8_t* File::strs() const {
    return data() + m_impl->header.str_loc;
}
size_t File::strs_len() const {
    return (size_t)(syms() - strs());
}
const uint8_t* File::syms() const {
    return data() + m_impl->header.sym_loc;
}
size_t File::syms_len() const {
    return (size_t)(end() - strs());
}
const uint8_t* File::end() const {
    return data() + m_impl->header.data_len;
}


}
