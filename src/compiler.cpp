#include <iostream>
#include <fstream>
#include <optional>
#include <string>
#include <vector>
#include <filesystem>

#include "llvm/IR/Module.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "ast.h"
#include "parser.h"
#include "name-resolver.h"
#include "type-checker.h"
#include "pruner.h"
#include "const-capture-pass.h"
#include "generator.h"
#include "utils/register_runtime.h"
#include "package-manager.h"

using ltm::own;
using ast::Ast;
using std::optional;
using std::nullopt;
using std::string;
using std::vector;
using std::function;
using std::unordered_map;
namespace fs = std::filesystem;

Depot depot;

static void write_file(const string& name, const string& content) {
    std::ofstream out(name, std::ios::binary);
    out.write(content.data(), content.size());
    if (out.bad()) {
        llvm::errs() << "can't write file: " << name << "\n";
        panic();
    }
}

//#define AK_DBG
#ifdef AK_DBG
int ak_main(int argc, const char* argv[]);
int main(int argc, char* argv[]) {
    const char* params[] = {
        "C:\\Users\\ak\\cpp\\ag\\out\\bin\\agc",
        "-g",
        "-src", "C:\\Users\\ak\\cpp\\ag\\out\\examples",
        "-src", "C:\\Users\\ak\\cpp\\ag\\out\\bin\\..\\lib",
        "-start", "sqliteDemo",
        "-O0",
        "-o", "sqliteDemo",
        "-L", "lnk",
        "-D", "dep"
    };
    return ak_main(sizeof(params)/sizeof(params[0]), params);
}
int ak_main(int argc, const char* argv[]) {
    int garbage_argc = argc;
    const char** garbage_argv = argv;
    llvm::InitLLVM X(garbage_argc, garbage_argv);
#else
int main(int argc, char* argv[]) {
    llvm::InitLLVM X(argc, argv);
#endif
    if (argc < 2) {
        llvm::outs() <<
                "Argentum compiler by Andrey Kamlatskiy.\n"
                "Usage: " << argv[0] << " -src path_to_sources -start module_name -o output_file <flags>\n"
                "--help for more info.\n";
        return 0;
    }
    auto target_triple = llvm::sys::getDefaultTargetTriple();
    bool output_bitcode = false;
    bool output_asm = false;
    bool add_debug_info = false;
    string test_filter;
    string start_module_name, out_file_name, opt_level;
    string entry_point_name = "main";
    string link_list_file;
    string dep_list_file;
    for (auto arg = argv + 1, end = argv + argc; arg != end; arg++) {
        auto param = [&] {
            if (++arg == end) {
                llvm::errs() << "expected flag parameter\n";
                exit(1);
            }
            return *arg;
        };
        if (strcmp(*arg, "--help") == 0) {
            llvm::outs() <<
                "Flags\n"
                "  -src directory     : where sources of all modules are located.\n"
                "  -src dir::url      : repository with local cache and remote package storage.\n"
                "  -start module_name : what module is a start module.\n"
                "  -o out_file        : file to store object file or asm or bitcode.\n"
                "  -target <arch><sub>-<vendor>-<sys>-<abi>\n"
                "          Example: x86_64-unknown-linux-gnu\n"
                "                or x86_64-w64-microsoft-windows\n"
                "  -g         : generate debug info\n"
                "  -emit-llvm : output bitcode\n"
                "  -S         : output asm/ll file\n"
                "  -ON        : optimize 0-none, 1-less, 2-default, 3-aggressive\n"
                "  -e fn_name : entry point fn name (default `main`)\n"
                "  -T regexp  : build tests\n"
                "  -L linkFile : filename for compiler to write a list of all libs to link\n"
                "  -D depFile  : filename for compiler to write all used dlls and resources\n"
                "                exported by used modules (if no -L or -D provided, it prints\n"
                "                to stdout\n";
            return 0;
        } else if (strcmp(*arg, "-S") == 0) {
            output_asm = true;
        } else if (strcmp(*arg, "-emit-llvm") == 0) {
            output_bitcode = true;
        } else if (strcmp(*arg, "-g") == 0) {
            add_debug_info = true;
        } else if (strncmp(*arg, "-O", 2) == 0) {
            opt_level = (*arg) + 2;
        } else if (strcmp(*arg, "-target") == 0) {
            target_triple = param();
        } else if (strcmp(*arg, "-e") == 0) {
            entry_point_name = param();
        } else if (strcmp(*arg, "-T") == 0) {
            test_filter = param();
        } else if (strcmp(*arg, "-L") == 0) {
            link_list_file = param();
        } else if (strcmp(*arg, "-D") == 0) {
            dep_list_file = param();
        } else if (strcmp(*arg, "-o") == 0) {
            out_file_name = param();
        } else if (strcmp(*arg, "-start") == 0) {
            start_module_name = param();
        } else if (strcmp(*arg, "-src") == 0) {
            depot.add(param());
        } else {
            llvm::errs() << "unexpected cmdline argument " << *arg << "\n";
            panic();
        }
    }
    auto check_str = [](string& s, const char* name) {
        if (s.empty()) {
            llvm::errs() << name << " name is not provided\n";
            panic();
        }
    };
    check_str(start_module_name, "start module");
    check_str(out_file_name, "output file");
    if (out_file_name.find('.') == string::npos) {
        out_file_name += '.';
        out_file_name +=
            output_bitcode ? (output_asm ? "ll" : "bc") :
            target_triple.find("windows") == string::npos
                ? (output_asm ? "s" : "o")
                : (output_asm ? "asm" : "obj");
    }
    ast::initialize();
    auto ast = own<Ast>::make();
    ast->test_filter = test_filter;
    register_runtime_content(*ast);
    int64_t unused_sys_version = 0;
    string inised_sys_out_path;
    depot.init(target_triple, add_debug_info);
    depot.read_source("sys", unused_sys_version, inised_sys_out_path);
    parse(ast, start_module_name, [&](auto name, auto& version, auto& out_path) {
        return depot.read_source(name, version, out_path);
    });
    resolve_names(ast);
    check_types(ast);
    prune(ast);
    const_capture_pass(ast);
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmPrinters();
    auto threadsafe_module = generate_code(ast, add_debug_info, entry_point_name);
    threadsafe_module.withModuleDo([&](llvm::Module& module) {
        std::error_code err_code;
        llvm::raw_fd_ostream out_file(out_file_name, err_code, llvm::sys::fs::OF_None);
        if (err_code) {
            llvm::errs() << "Could not write file: " << err_code.message() << "\n";
            panic();
        }
        module.setTargetTriple(target_triple);
        if (output_bitcode) {
            if (output_asm)
                module.print(out_file, nullptr);
            else
                llvm::WriteBitcodeToFile(module, out_file);
        } else {
            std::string error_str;
            auto target = llvm::TargetRegistry::lookupTarget(target_triple, error_str);
            if (!target) {
                llvm::errs() << error_str << "\n";
                panic();
            }
            auto target_machine = target->createTargetMachine(
                target_triple,
                "generic",  // cpu
                "",         // features
                llvm::TargetOptions(),
                std::optional<llvm::Reloc::Model>());
            if (add_debug_info && opt_level.empty())
                opt_level = "0";
            if (!opt_level.empty())
                target_machine->setOptLevel(
                    opt_level == "0" ? llvm::CodeGenOptLevel::None :
                    opt_level == "1" ? llvm::CodeGenOptLevel::Less :
                    opt_level == "3" ? llvm::CodeGenOptLevel::Aggressive :
                    llvm::CodeGenOptLevel::Default);
            module.setDataLayout(target_machine->createDataLayout());
            llvm::legacy::PassManager pass_manager;
            if (target_machine->addPassesToEmitFile(pass_manager, out_file, nullptr, output_asm
                ? llvm::CodeGenFileType::AssemblyFile
                : llvm::CodeGenFileType::ObjectFile))
            {
                llvm::errs() << "llvm can't emit a file of this type for target " << target_triple << "\n";
                panic();
            }
            pass_manager.run(module);
        }
        out_file.flush();
        auto [links, deps] = depot.get_links_and_deps();
        links = out_file_name + " " + links;
        if (link_list_file.empty())
            llvm::outs() << links << '\n';
        else
            write_file(link_list_file, links);
        if (dep_list_file.empty())
            llvm::outs() << deps << '\n';
        else
            write_file(dep_list_file, deps);
    });
    return 0;
}

[[noreturn]] void panic() {
    std::quick_exit(1);
}
