#include "quosi/quosi.h"
#include "quosi/vm.h"
#include <filesystem>
#include <iostream>
#include <fstream>
#include "vmem/map.h"


static std::string read_to_string(const std::filesystem::path& filename, int& status) {
    status = 0;
    auto f = std::ifstream(filename.c_str(), std::ios::binary);
    if (!f.is_open()) {
        status = 1;
        return "";
    }
    f.seekg(0, std::ios::end);
    const auto size = f.tellg();
    f.seekg(0, std::ios::beg);
    auto buf = std::string((size_t)size, 'a');
    f.read(buf.data(), size);
    return buf;
}


static uint64_t* varctx(uint32_t, bool) { static uint64_t dummy = 0; return &dummy; }


int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "[qdbg] no file arg provided\n";
        std::cout << VANGO_PKG_NAME " " VANGO_PKG_VERSION "\n";
        std::cout << "usage:\n";
        std::cout << "    quosi <FILE> [-c|-t]\n\n";
        return 1;
    } if (argc < 3) {
        std::cout << "[qdbg] no mode arg provided\n";
        std::cout << VANGO_PKG_NAME " " VANGO_PKG_VERSION "\n";
        std::cout << "usage:\n";
        std::cout << "    quosi <FILE> [-c|-t]\n\n";
        return 1;
    }

    auto path = std::filesystem::path(argv[1]);
    auto mapp = vmem::MappedFile();
    auto file = quosi::File();
    if (path.extension() == ".qsi") {
        int err = 0;
        const auto src = read_to_string(path, err);
        if (err != 0) {
            std::cout << "[qdbg] file not found\n\n";
            return 1;
        }
        auto errors = quosi::ErrorList();
        file = quosi::File::compile_from_src(src.c_str(), errors);
        if (!errors.list.empty()) {
            for (const auto& e : errors.list) {
                std::cout << "[qdbg] (" << e.span.row << ":" << e.span.col << ") " << e.to_string() << "\n";
            }
            std::cout << "\n";
            return 1;
        }
    } else if (path.extension() == ".bsi") {
        mapp.map(path);
        file = quosi::File::init_from_raw(mapp.bytes(), false);
    } else {
        std::cout << "[qdbg] invalid file format\n\n";
        return 1;
    }

    if (strncmp(argv[2], "-c", 3) == 0) {
        file.prettyprint();

    } else if (strncmp(argv[2], "-t", 3) == 0) {
        auto vm = quosi::VirtualMachine(file.code());
        bool quit = false;

        while (!quit) {
            switch (vm.exec(varctx)) {
            case quosi::VirtualMachine::UpCall::Line:
                std::cout << (int)vm.id() << ": \"" << vm.line() << "\"\n";
                break;
            case quosi::VirtualMachine::UpCall::Pick: {
                auto pindex = std::vector<uint32_t>();
                for (uint32_t i = 0; i < vm.nq(); i++) {
                    const auto prop = vm.deq_text();
                    std::cout << "  " << pindex.size()+1 << ": \"" << prop.s << "\"\n";
                    pindex.push_back(prop.i);
                }
                std::cout << "\n> ";
                size_t pick;
                std::cin >> pick;
                std::cout << "\n";
                vm.push(pindex[pick-1]);
                break; }
            case quosi::VirtualMachine::UpCall::Event:
                std::cout << "EVENT: " << vm.line() << "\n\n";
                break;
            case quosi::VirtualMachine::UpCall::Exit:
                quit = true;
                break;
            default:
                quit = true;
                break;
            }
        }

        std::cout << "\n[qdbg] end of file reached\n";
        return 0;
    }
}

