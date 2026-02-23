#define VANGO_TEST_ROOT
#include <vangotest/asserts2.h>
#include <vangotest/bench.h>
#include <filesystem>
#include <iostream>
#include <fstream>
#include "quosi/quosi.h"
#include "quosi/ast.h"
#include "lex.h"
#include "memprof.h"


static std::string read_to_string(const std::filesystem::path& filename) {
    auto f = std::ifstream(filename.c_str(), std::ios::binary);
    if (!f.is_open()) return "";
    f.seekg(0, std::ios::end);
    const auto size = f.tellg();
    f.seekg(0, std::ios::beg);
    auto buf = std::string((size_t)size, 'a');
    f.read(buf.data(), size);
    return buf;
}


vango_test(compiler) {
    const auto src = read_to_string("../examples/basic.qsi");
    auto errors = quosi::ErrorList();
    const auto file = quosi::File::compile_from_src(src.c_str(), errors);
    if (!errors.list.empty()) {
        for (const auto& e : errors.list) {
            std::cout << "(" << e.span.row << ":" << e.span.col << ") " << e.to_string() << "\n";
        }
        vg_assert(false);
    }
    file.prettyprint();
}

vango_test(fileio) {
    const auto src = read_to_string("../examples/basic.qsi");
    auto errors = quosi::ErrorList();
    const auto f1 = quosi::File::compile_from_src(src.c_str(), errors);
    if (!errors.list.empty()) {
        for (const auto& e : errors.list) {
            std::cout << "(" << e.span.row << ":" << e.span.col << ") " << e.to_string() << "\n";
        }
        vg_assert(false);
    }
    f1.write_to_file("../examples/basic.bsi");
    const auto f2 = quosi::File::load_from_file("../examples/basic.bsi");
    f2.prettyprint();
}

vango_test(stats) {
    const auto src = read_to_string("../examples/basic.qsi");
    auto errors = quosi::ErrorList();

    size_t allocs_before = alloc_count();
    const auto graph = quosi::ast::parse(src.c_str(), errors);
    if (!errors.list.empty()) {
        for (const auto& e : errors.list) {
            std::cout << "(" << e.span.row << ":" << e.span.col << ") " << e.to_string() << "\n";
        }
        vg_assert(false);
    }
    size_t allocs_after = alloc_count();
    const size_t allocs_parser = allocs_after - allocs_before;
    allocs_before = allocs_after;
    const auto file = quosi::File::compile_from_ast(*graph);
    allocs_after = alloc_count();
    const size_t allocs_codegen = allocs_after - allocs_before;

    std::cout << "text size: " << src.size() << "\n";
    std::cout << "code size: " << file.data_len() << "\n";
    std::cout << "malloc calls (parser):  " << (allocs_parser) << "\n";
    std::cout << "malloc calls (codegen): " << (allocs_codegen) << "\n";
}

vango_test(lexer) {
    const auto src = read_to_string("../examples/basic.qsi");
    auto lexer = quosi::TokenStream(src.c_str());
    auto token = lexer.next();
    while (token.type != quosi::Token::Type::Eof) {
        // std::cout << "(" << token.span.row << ":" << token.span.col << ")  " << token.value << "\n";
        token = lexer.next();
    }
}

vango_test(bench_parser) {
    std::cin.get();
    const auto src = read_to_string("../examples/basic.qsi");
    auto errors = quosi::ErrorList();
    auto graph = std::unique_ptr<quosi::ast::Graph>();

    vango_bench(100, [&](){
        graph = quosi::ast::parse(src.c_str(), errors);
    });
    vango_bench(1000, [&](){
        graph = quosi::ast::parse(src.c_str(), errors);
    });
    vango_bench(10000, [&](){
        graph = quosi::ast::parse(src.c_str(), errors);
    });

    if (!errors.list.empty()) {
        for (const auto& e : errors.list) {
            std::cout << "(" << e.span.row << ":" << e.span.col << ") " << e.to_string() << "\n";
        }
        vg_assert(false);
    }
}

vango_test(bench_codegen) {
    const auto src = read_to_string("../examples/basic.qsi");
    auto errors = quosi::ErrorList();
    const auto graph = quosi::ast::parse(src.c_str(), errors);
    auto file = quosi::File();

    vango_bench(100, [&](){
        file = quosi::File::compile_from_ast(*graph);
    });
    vango_bench(1000, [&](){
        file = quosi::File::compile_from_ast(*graph);
    });
    vango_bench(10000, [&](){
        file = quosi::File::compile_from_ast(*graph);
    });

    if (!errors.list.empty()) {
        for (const auto& e : errors.list) {
            std::cout << "(" << e.span.row << ":" << e.span.col << ") " << e.to_string() << "\n";
        }
        vg_assert(false);
    }
}

vango_test(bench_compiler) {
    const auto src = read_to_string("../examples/basic.qsi");
    auto errors = quosi::ErrorList();
    auto file = quosi::File();

    vango_bench(100, [&](){
        file = quosi::File::compile_from_src(src.c_str(), errors);
    });
    vango_bench(1000, [&](){
        file = quosi::File::compile_from_src(src.c_str(), errors);
    });
    vango_bench(10000, [&](){
        file = quosi::File::compile_from_src(src.c_str(), errors);
    });

    if (!errors.list.empty()) {
        for (const auto& e : errors.list) {
            std::cout << "(" << e.span.row << ":" << e.span.col << ") " << e.to_string() << "\n";
        }
        vg_assert(false);
    }
}

