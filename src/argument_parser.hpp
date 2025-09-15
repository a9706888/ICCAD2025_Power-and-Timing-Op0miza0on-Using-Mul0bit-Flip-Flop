#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>

// =============================================================================
// COMMAND LINE ARGUMENT PARSER FOR ICCAD 2025 COMPETITION FORMAT
// =============================================================================
// 支持主辦方指定的命令行格式：
// ./cadb_0000 -weight <weightFile> -lib <libFile1> <libFile2> ... 
//             -lef <lefFile1> <lefFile2> ... -db <dbFile1> <dbFile2> ...
//             -tf <tfFile1> <tfFile2> ... -sdc <sdcFile1> <sdcFile2> ...
//             -v <verilogFile1> <verilogFile2> ... -def <defFile1> <defFile2> ...
//             -out <outputName>
// =============================================================================

struct ProgramArguments {
    // 檔案路徑列表
    std::string weight_file;
    std::vector<std::string> lib_files;
    std::vector<std::string> lef_files;
    std::vector<std::string> db_files;        // 將被忽略
    std::vector<std::string> tf_files;        // 將被忽略
    std::vector<std::string> sdc_files;       // 將被忽略
    std::vector<std::string> verilog_files;
    std::vector<std::string> def_files;
    std::string output_name;
    
    // 驗證所有必要檔案是否存在
    bool validate() const {
        bool valid = true;
        
        if (weight_file.empty()) {
            std::cout << "Error: No weight file specified" << std::endl;
            valid = false;
        }
        
        if (lib_files.empty()) {
            std::cout << "Error: No library files specified" << std::endl;
            valid = false;
        }
        
        if (lef_files.empty()) {
            std::cout << "Error: No LEF files specified" << std::endl;
            valid = false;
        }
        
        if (verilog_files.empty()) {
            std::cout << "Error: No Verilog files specified" << std::endl;
            valid = false;
        }
        
        if (def_files.empty()) {
            std::cout << "Error: No DEF files specified" << std::endl;
            valid = false;
        }
        
        return valid;
    }
    
    // 打印解析結果摘要
    void print_summary() const {
        std::cout << "=== Parsed Arguments ===" << std::endl;
        std::cout << "Weight file: " << weight_file << std::endl;
        std::cout << "Library files: " << lib_files.size() << std::endl;
        std::cout << "LEF files: " << lef_files.size() << std::endl;
        std::cout << "Verilog files: " << verilog_files.size() << std::endl;
        std::cout << "DEF files: " << def_files.size() << std::endl;
        
        if (!db_files.empty()) {
            std::cout << "DB files (ignored): " << db_files.size() << std::endl;
        }
        if (!tf_files.empty()) {
            std::cout << "TF files (ignored): " << tf_files.size() << std::endl;
        }
        if (!sdc_files.empty()) {
            std::cout << "SDC files (ignored): " << sdc_files.size() << std::endl;
        }
        
        if (!output_name.empty()) {
            std::cout << "Output name: " << output_name << std::endl;
        }
        std::cout << std::endl;
    }
};

// 解析命令行參數
ProgramArguments parse_arguments(int argc, char* argv[]);

// 顯示使用說明
void print_usage(const char* program_name);