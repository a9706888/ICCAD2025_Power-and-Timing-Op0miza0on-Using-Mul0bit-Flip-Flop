#include "argument_parser.hpp"
#include <iostream>
#include <string>

// =============================================================================
// COMMAND LINE ARGUMENT PARSER IMPLEMENTATION
// =============================================================================

void print_usage(const char* program_name) {
    std::cout << "=== ICCAD 2025 Flip-Flop Banking Competition Parser ===" << std::endl;
    std::cout << "Usage: " << program_name << " [options]" << std::endl;
    std::cout << std::endl;
    std::cout << "Required options:" << std::endl;
    std::cout << "  -weight <file>          Weight file for objective function" << std::endl;
    std::cout << "  -lib <file1> [file2]... Liberty library files (.lib)" << std::endl;
    std::cout << "  -lef <file1> [file2]... LEF layout files (.lef)" << std::endl;
    std::cout << "  -v <file1> [file2]...   Verilog netlist files (.v)" << std::endl;
    std::cout << "  -def <file1> [file2]... DEF placement files (.def)" << std::endl;
    std::cout << std::endl;
    std::cout << "Optional options:" << std::endl;
    std::cout << "  -db <file1> [file2]...  Database files (ignored)" << std::endl;
    std::cout << "  -tf <file1> [file2]...  Technology files (ignored)" << std::endl;
    std::cout << "  -sdc <file1> [file2]... SDC timing files (ignored)" << std::endl;
    std::cout << "  -out <name>             Output name (future use)" << std::endl;
    std::cout << std::endl;
    std::cout << "Example:" << std::endl;
    std::cout << "  " << program_name << " -weight testcase1_weight \\" << std::endl;
    std::cout << "                       -lib lib1.lib lib2.lib \\" << std::endl;
    std::cout << "                       -lef layout1.lef layout2.lef \\" << std::endl;
    std::cout << "                       -v design.v \\" << std::endl;
    std::cout << "                       -def placement.def" << std::endl;
}

ProgramArguments parse_arguments(int argc, char* argv[]) {
    ProgramArguments args;
    
    if (argc < 2) {
        print_usage(argv[0]);
        exit(1);
    }
    
    std::vector<std::string>* current_list = nullptr;
    std::string* current_single = nullptr;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        // 檢查是否為選項標誌
        if (arg == "-weight") {
            current_list = nullptr;
            current_single = &args.weight_file;
        }
        else if (arg == "-lib") {
            current_single = nullptr;
            current_list = &args.lib_files;
        }
        else if (arg == "-lef") {
            current_single = nullptr;
            current_list = &args.lef_files;
        }
        else if (arg == "-db") {
            current_single = nullptr;
            current_list = &args.db_files;
        }
        else if (arg == "-tf") {
            current_single = nullptr;
            current_list = &args.tf_files;
        }
        else if (arg == "-sdc") {
            current_single = nullptr;
            current_list = &args.sdc_files;
        }
        else if (arg == "-v") {
            current_single = nullptr;
            current_list = &args.verilog_files;
        }
        else if (arg == "-def") {
            current_single = nullptr;
            current_list = &args.def_files;
        }
        else if (arg == "-out") {
            current_list = nullptr;
            current_single = &args.output_name;
        }
        else if (arg.length() > 0 && arg[0] == '-') {
            std::cout << "Warning: Unknown option " << arg << std::endl;
            current_list = nullptr;
            current_single = nullptr;
        }
        else {
            // 這是一個檔案名稱
            if (current_single != nullptr) {
                if (current_single->empty()) {
                    *current_single = arg;
                } else {
                    std::cout << "Warning: Multiple values for single option, using first one" << std::endl;
                }
            }
            else if (current_list != nullptr) {
                current_list->push_back(arg);
            }
            else {
                std::cout << "Warning: Orphaned argument " << arg << std::endl;
            }
        }
    }
    
    return args;
}