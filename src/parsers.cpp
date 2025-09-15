#include "parsers.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <set>
#include <dirent.h>
#include <sys/stat.h>

// Forward declarations for helper functions used in parsing
bool is_power_net(const std::string& net_name);
bool is_ground_net(const std::string& net_name);

// =============================================================================
// FILE DISCOVERY AND FILTERING FUNCTIONS
// =============================================================================

// 檢查字串是否以特定後綴結尾 (C++17 compatible)
bool ends_with(const std::string& str, const std::string& suffix) {
    if (suffix.length() > str.length()) return false;
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

// 檢查是否應該解析此Liberty檔案
bool should_parse_liberty_file(const std::string& filepath) {
    // 跳過.db檔案
    if (ends_with(filepath, ".db")) {
        return false;
    }
    
    // 只解析.lib檔案
    if (!ends_with(filepath, ".lib")) {
        return false;
    }
    
    // 只解析tt0p8v25c timing corner的檔案 (選擇標準corner)
    // 根據Q43，我們可以選擇任何一個corner，選擇tt0p8v25c作為標準
    if (filepath.find("tt0p8v25c") == std::string::npos) {
        return false;
    }
    
    return true;
}

// 檢查是否應該解析此LEF檔案
bool should_parse_lef_file(const std::string& filepath) {
    // 只解析.lef檔案
    return ends_with(filepath, ".lef");
}

// 遞迴搜尋目錄中的檔案
void scan_directory_recursive(const std::string& dir_path, std::vector<std::string>& file_paths) {
    DIR* dir = opendir(dir_path.c_str());
    if (!dir) return;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        
        std::string full_path = dir_path + "/" + name;
        
        struct stat file_stat;
        if (stat(full_path.c_str(), &file_stat) == 0) {
            if (S_ISDIR(file_stat.st_mode)) {
                // 遞迴搜尋子目錄
                scan_directory_recursive(full_path, file_paths);
            } else if (S_ISREG(file_stat.st_mode)) {
                // 添加普通檔案
                file_paths.push_back(full_path);
            }
        }
    }
    
    closedir(dir);
}

// 自動發現Liberty檔案
std::vector<std::string> discover_liberty_files(const std::string& testcase_path) {
    std::vector<std::string> all_files;
    std::vector<std::string> liberty_files;
    int skipped_count = 0;
    
    // 遞迴搜尋所有檔案
    scan_directory_recursive(testcase_path, all_files);
    
    // 過濾出需要的Liberty檔案
    for (const std::string& filepath : all_files) {
        if (ends_with(filepath, ".lib") || ends_with(filepath, ".db")) {
            if (should_parse_liberty_file(filepath)) {
                liberty_files.push_back(filepath);
            } else {
                skipped_count++;
            }
        }
    }
    
    std::cout << "  Skipped " << skipped_count << " library files (non-tt0p8v25c timing corners and .db files)" << std::endl;
    
    // 按檔案名稱排序以確保一致的解析順序
    std::sort(liberty_files.begin(), liberty_files.end());
    
    return liberty_files;
}

// 自動發現LEF檔案
std::vector<std::string> discover_lef_files(const std::string& testcase_path) {
    std::vector<std::string> all_files;
    std::vector<std::string> lef_files;
    
    // 遞迴搜尋所有檔案
    scan_directory_recursive(testcase_path, all_files);
    
    // 過濾出需要的LEF檔案
    for (const std::string& filepath : all_files) {
        if (should_parse_lef_file(filepath)) {
            lef_files.push_back(filepath);
        }
    }
    
    // 按檔案名稱排序以確保一致的解析順序
    std::sort(lef_files.begin(), lef_files.end());
    
    return lef_files;
}

// =============================================================================
// LIBERTY PARSER HELPER FUNCTIONS
// =============================================================================

// 提取library名稱從檔案路徑
std::string extract_library_name(const std::string& filepath) {
    size_t slash_pos = filepath.find_last_of("/\\");
    std::string filename = (slash_pos != std::string::npos) ? filepath.substr(slash_pos + 1) : filepath;
    
    // 從類似 "snps25hopt_base_tt0p8v25c.lib" 提取 "hopt_base"
    // 注意：必須先檢查長的名稱，避免被短的匹配到
    if (filename.find("slopt") != std::string::npos) {
        if (filename.find("_base_") != std::string::npos) return "slopt_base";
        if (filename.find("_cg_") != std::string::npos) return "slopt_cg";
        if (filename.find("_dlvl_") != std::string::npos) return "slopt_dlvl";
        if (filename.find("_iso_") != std::string::npos) return "slopt_iso";
        if (filename.find("_pg_") != std::string::npos) return "slopt_pg";
        if (filename.find("_ret_") != std::string::npos) return "slopt_ret";
        if (filename.find("_ulvl_") != std::string::npos) return "slopt_ulvl";
        return "slopt";
    }
    if (filename.find("hopt") != std::string::npos) {
        if (filename.find("_base_") != std::string::npos) return "hopt_base";
        if (filename.find("_cg_") != std::string::npos) return "hopt_cg";
        if (filename.find("_dlvl_") != std::string::npos) return "hopt_dlvl";
        if (filename.find("_iso_") != std::string::npos) return "hopt_iso";
        if (filename.find("_pg_") != std::string::npos) return "hopt_pg";
        if (filename.find("_ret_") != std::string::npos) return "hopt_ret";
        if (filename.find("_ulvl_") != std::string::npos) return "hopt_ulvl";
        return "hopt";
    }
    if (filename.find("lopt") != std::string::npos) {
        if (filename.find("_base_") != std::string::npos) return "lopt_base";
        if (filename.find("_cg_") != std::string::npos) return "lopt_cg";
        if (filename.find("_dlvl_") != std::string::npos) return "lopt_dlvl";
        if (filename.find("_iso_") != std::string::npos) return "lopt_iso";
        if (filename.find("_pg_") != std::string::npos) return "lopt_pg";
        if (filename.find("_ret_") != std::string::npos) return "lopt_ret";
        if (filename.find("_ulvl_") != std::string::npos) return "lopt_ulvl";
        return "lopt";
    }
    if (filename.find("ropt") != std::string::npos) {
        if (filename.find("_base_") != std::string::npos) return "ropt_base";
        if (filename.find("_cg_") != std::string::npos) return "ropt_cg";
        if (filename.find("_dlvl_") != std::string::npos) return "ropt_dlvl";
        if (filename.find("_iso_") != std::string::npos) return "ropt_iso";
        if (filename.find("_pg_") != std::string::npos) return "ropt_pg";
        if (filename.find("_ret_") != std::string::npos) return "ropt_ret";
        if (filename.find("_ulvl_") != std::string::npos) return "ropt_ulvl";
        return "ropt";
    }
    
    return "unknown";
}

// 檢查是否為flip-flop
bool is_flip_flop_cell(const std::string& cell_name) {
    std::vector<std::string> ff_patterns = {
        "FSDN", "FSDNQ", "FSDP", "FSRN", "FSJK", "FSDL", "FSRL", 
        "LSRD", "SSRR", "DFF", "DFFN", "SDFF", "SDFN", 
        "FDP", "FDN", "FSDPQB", "FDPQB", "FDPCBQ"
    };
    
    for(const auto& pattern : ff_patterns) {
        if(cell_name.find(pattern) != std::string::npos) {
            return true;
        }
    }
    return false;
}

// 提取bit width從cell名稱
int extract_bit_width(const std::string& cell_name) {
    // 正確的多bit FF命名規律檢查：
    // FSDN2_* = 2-bit FF
    // FSDN4_* = 4-bit FF  
    // FSDN8_* = 8-bit FF (如果存在)
    // *Q4_* = 4-bit FF (如 LSRDPQ4, SSRRDPQ4)
    
    if (cell_name.find("FSDN2_") != std::string::npos) {
        return 2;
    }
    if (cell_name.find("FSDN4_") != std::string::npos) {
        return 4;
    }
    if (cell_name.find("FSDN8_") != std::string::npos) {
        return 8;
    }
    
    // 檢查 *Q4_* 模式 (如 LSRDPQ4, SSRRDPQ4)
    if (cell_name.find("Q4_") != std::string::npos) {
        return 4;
    }
    
    // 其他所有FF都是1-bit，包括那些帶 _2, _4, _8 後綴的
    // 這些後綴通常是版本號或其他標識，不是bit width
    return 1; // Default single-bit
}

// 提取字串中的數值
double extract_number(const std::string& text, const std::string& keyword) {
    size_t pos = text.find(keyword);
    if (pos == std::string::npos) return 0.0;
    
    pos += keyword.length();
    while (pos < text.length() && (text[pos] == ' ' || text[pos] == ':')) pos++;
    
    size_t end_pos = text.find_first_of(" ;\n", pos);
    if (end_pos == std::string::npos) end_pos = text.length();
    
    std::string num_str = text.substr(pos, end_pos - pos);
    try {
        return std::stod(num_str);
    } catch (...) {
        return 0.0;
    }
}

// 提取引號中的字串
std::string extract_quoted_string(const std::string& text, const std::string& keyword) {
    size_t pos = text.find(keyword);
    if (pos == std::string::npos) return "null";
    
    size_t quote_start = text.find("\"", pos);
    if (quote_start == std::string::npos) return "null";
    
    size_t quote_end = text.find("\"", quote_start + 1);
    if (quote_end == std::string::npos) return "null";
    
    return text.substr(quote_start + 1, quote_end - quote_start - 1);
}

// 解析cell屬性
void parse_cell_properties(CellTemplate& cell, const std::string& cell_block) {
    // 設定cell類型
    if (is_flip_flop_cell(cell.name)) {
        cell.type = CellTemplate::FLIP_FLOP;
        cell.bit_width = extract_bit_width(cell.name);
    } else {
        cell.type = CellTemplate::OTHER;
        cell.bit_width = 1;
    }
    
    // 提取area (logical area)
    cell.area = extract_number(cell_block, "area :");
    
    // 提取leakage power
    cell.leakage_power = extract_number(cell_block, "cell_leakage_power :");
    
    // 提取single_bit_degenerate
    cell.single_bit_degenerate = extract_quoted_string(cell_block, "single_bit_degenerate :");
    
    // 提取clock edge資訊 (只對flip-flop解析)
    if (cell.type == CellTemplate::FLIP_FLOP) {
        std::string clocked_on = extract_quoted_string(cell_block, "clocked_on :");
        if (!clocked_on.empty()) {
            if (clocked_on.find("(!") != std::string::npos || clocked_on.find("~") != std::string::npos) {
                // 包含 "(!CK)" 或 "~CK" 表示falling edge
                cell.clock_edge = CellTemplate::FALLING;
            } else if (clocked_on.find("&") != std::string::npos || clocked_on.find("|") != std::string::npos) {
                // 包含邏輯運算符的複雜條件 (如 CK&SR, CK|EN 等)
                cell.clock_edge = CellTemplate::UNKNOWN_EDGE;
            } else if (clocked_on.find("CK") != std::string::npos || clocked_on.find("CLK") != std::string::npos) {
                // 包含 "CK" 或 "CLK" 表示rising edge
                cell.clock_edge = CellTemplate::RISING;
            }
        } else {
            // 如果沒有clocked_on，嘗試從cell名稱推斷
            // 對於特殊的SR FF或其他類型，可能需要不同的處理
            if (cell.name.find("SR") != std::string::npos || 
                cell.name.find("SSRR") != std::string::npos) {
                // SR flip-flops通常是level-triggered，設為RISING作為默認
                cell.clock_edge = CellTemplate::RISING;
            }
        }
    }
    
    // TODO: 解析pin資訊（較複雜，後續添加）
}

// =============================================================================
// LEF PARSER HELPER FUNCTIONS
// =============================================================================

// 清理空白字符
std::string trim_whitespace(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

// 提取PIN名稱
std::string extract_pin_name(const std::string& pin_line) {
    // "PIN X" -> "X"
    size_t start = pin_line.find("PIN ");
    if (start == 0) {
        start += 4; // Skip "PIN "
        size_t end = pin_line.find_first_of(" \t\n", start);
        if (end == std::string::npos) end = pin_line.length();
        return pin_line.substr(start, end - start);
    }
    return "";
}

// 解析SIZE行
void parse_size_line(const std::string& line, std::shared_ptr<CellTemplate> cell) {
    // "SIZE 0.592 BY 0.6 ;" -> width=0.592, height=0.6
    std::istringstream iss(line);
    std::string token;
    
    iss >> token; // "SIZE"
    iss >> cell->width;
    iss >> token; // "BY"
    iss >> cell->height;
    cell->width*=1000;
    cell->height*=1000; 
}

// 解析PIN section
void parse_pin_section(std::ifstream& file, const std::string& pin_line, std::shared_ptr<CellTemplate> cell) {
    // 提取pin名稱
    std::string pin_name = extract_pin_name(pin_line);
    if (pin_name.empty()) return;
    
    Pin pin;
    pin.name = pin_name;
    
    std::string line;
    while (std::getline(file, line)) {
        line = trim_whitespace(line);
        
        // 檢查PIN結束
        if (line.find("END ") == 0 && line.find(pin_name) != std::string::npos) {
            break;
        }
        
        // 解析DIRECTION
        if (line.find("DIRECTION ") == 0) {
            std::string direction = line.substr(10);
            direction = direction.substr(0, direction.find(" ;"));
            
            if (direction == "INPUT") pin.direction = Pin::INPUT;
            else if (direction == "OUTPUT") pin.direction = Pin::OUTPUT;
            else if (direction == "INOUT") pin.direction = Pin::INOUT;
        }
        
        // 解析USE
        if (line.find("USE ") == 0) {
            std::string use = line.substr(4);
            use = use.substr(0, use.find(" ;"));
            
            if (use == "SIGNAL") pin.usage = Pin::SIGNAL;
            else if (use == "CLOCK") pin.usage = Pin::CLOCK;
            else if (use == "POWER") pin.usage = Pin::POWER;
            else if (use == "GROUND") pin.usage = Pin::GROUND;
        }
    }
    
    // 如果是flip-flop cell，進行詳細的pin類型分類
    if (cell->is_flip_flop()) {
        pin.ff_pin_type = classify_ff_pin_type(pin.name);
    }
    
    cell->pins.push_back(pin);
}

// 從MACRO行提取cell名稱
std::string extract_cell_name_from_macro(const std::string& macro_line) {
    // "MACRO SNPSLOPT25_TIEDIN_PV1ECO_6" -> "SNPSLOPT25_TIEDIN_PV1ECO_6"
    size_t start = macro_line.find("MACRO ");
    if (start == 0) {
        start += 6; // Skip "MACRO "
        size_t end = macro_line.find_first_of(" \t\n", start);
        if (end == std::string::npos) end = macro_line.length();
        return macro_line.substr(start, end - start);
    }
    return "";
}

// 解析MACRO內容
void parse_macro_content(std::ifstream& file, std::shared_ptr<CellTemplate> cell) {
    std::string line;
    
    while (std::getline(file, line)) {
        line = trim_whitespace(line);
        
        // 檢查MACRO結束
        if (line.find("END ") == 0 && line.find(cell->name) != std::string::npos) {
            break;
        }
        
        // 解析SIZE
        if (line.find("SIZE ") == 0) {
            parse_size_line(line, cell);
        }
        
        // 解析PIN
        if (line.find("PIN ") == 0) {
            parse_pin_section(file, line, cell);
        }
    }
}

// 跳過未知MACRO的內容
void skip_macro_content(std::ifstream& file) {
    std::string line;
    while (std::getline(file, line)) {
        if (line.find("END ") == 0) {
            break;
        }
    }
}


// =============================================================================
// VERILOG PARSER HELPER FUNCTIONS
// =============================================================================

// 提取module名稱
std::string extract_module_name(const std::string& content) {
    size_t module_pos = content.find("module ");
    if (module_pos == std::string::npos) return "";
    
    size_t name_start = module_pos + 7; // Skip "module "
    size_t name_end = content.find_first_of(" \t\n(", name_start);
    if (name_end == std::string::npos) return "";
    
    return content.substr(name_start, name_end - name_start);
}

// Parse all modules in the verilog file and record their boundaries
void parse_module_hierarchy(const std::string& file_content, DesignDatabase& db) {
    std::cout << "  Parsing module hierarchy..." << std::endl;
    
    size_t pos = 0;
    while (pos < file_content.length()) {
        // Find module declaration
        size_t module_pos = file_content.find("module ", pos);
        if (module_pos == std::string::npos) break;
        
        // Extract module name
        size_t name_start = module_pos + 7; // Skip "module "
        size_t name_end = file_content.find_first_of(" \t\n(", name_start);
        if (name_end == std::string::npos) {
            pos = module_pos + 7;
            continue;
        }
        
        std::string module_name = file_content.substr(name_start, name_end - name_start);
        
        // Find corresponding endmodule
        size_t endmodule_pos = file_content.find("endmodule", module_pos);
        if (endmodule_pos == std::string::npos) {
            std::cout << "    Warning: No endmodule found for module " << module_name << std::endl;
            pos = module_pos + 7;
            continue;
        }
        
        // Create Module structure
        DesignDatabase::Module module;
        module.name = module_name;
        module.start_pos = module_pos;
        module.end_pos = endmodule_pos + 9; // Include "endmodule"
        
        db.modules.push_back(module);
        std::cout << "    Found module: " << module_name 
                  << " (pos: " << module_pos << "-" << module.end_pos << ")" << std::endl;
        
        // Continue search after this endmodule
        pos = module.end_pos;
    }
    
    std::cout << "    Total modules found: " << db.modules.size() << std::endl;
}

// Parse module hierarchy and instances - fast position-based approach with comment handling
void parse_module_hierarchy_and_instances(const std::string& file_content, DesignDatabase& db) {
    std::cout << "  Parsing module hierarchy and instances..." << std::endl;
    std::cout << "    File content size: " << file_content.length() << " chars" << std::endl;
    
    std::set<std::string> net_names;
    std::string current_module = "";  // No default module - must find first one
    std::vector<std::string> module_stack;
    int instance_count = 0;
    int module_count = 0;
    
    size_t pos = 0;
    while (pos < file_content.length()) {
        // Skip comment lines at the beginning of lines
        if (pos == 0 || file_content[pos-1] == '\n') {
            // Skip whitespace at beginning of line
            while (pos < file_content.length() && (file_content[pos] == ' ' || file_content[pos] == '\t')) {
                pos++;
            }
            // Check for comment line
            if (pos < file_content.length() - 1 && file_content.substr(pos, 2) == "//") {
                // Skip entire comment line
                pos = file_content.find('\n', pos);
                if (pos == std::string::npos) break;
                pos++; // Skip newline
                continue;
            }
        }
        
        // Skip whitespace
        if (pos < file_content.length() && std::isspace(file_content[pos])) {
            pos++;
            continue;
        }
        
        // Progress indicator
        if (instance_count > 0 && instance_count % 1000 == 0 && pos % 1000000 == 0) {
            std::cout << "    Parsed " << instance_count << " instances, progress: " 
                     << (pos * 100 / file_content.length()) << "%" << std::endl;
        }
        
        // Check for module declaration
        if (pos <= file_content.length() - 7 && file_content.substr(pos, 7) == "module ") {
            size_t name_start = pos + 7;
            
            // Skip whitespace after "module "
            while (name_start < file_content.length() && std::isspace(file_content[name_start])) {
                name_start++;
            }
            
            size_t name_end = file_content.find_first_of(" \t\n(", name_start);
            if (name_end != std::string::npos) {
                current_module = file_content.substr(name_start, name_end - name_start);
                module_stack.push_back(current_module);
                
                // Record module in database
                DesignDatabase::Module module;
                module.name = current_module;
                module.start_pos = pos;
                db.modules.push_back(module);
                module_count++;
                
                std::cout << "    Found module " << module_count << ": " << current_module 
                         << " (pos: " << pos << ")" << std::endl;
            }
            pos = name_end;
        }
        // Check for endmodule
        else if (pos <= file_content.length() - 9 && file_content.substr(pos, 9) == "endmodule") {
            if (!db.modules.empty()) {
                db.modules.back().end_pos = pos + 9;
            }
            if (!module_stack.empty()) {
                module_stack.pop_back();
                current_module = module_stack.empty() ? "" : module_stack.back();
            }
            pos += 9;
        }
        // Check for SNPS instances
        else if (pos <= file_content.length() - 4 && file_content.substr(pos, 4) == "SNPS" && 
                 !current_module.empty()) {
            
            // Check this is not in a wire declaration by looking backwards for "wire"
            bool is_wire = false;
            size_t check_pos = pos;
            int chars_back = 0;
            while (check_pos > 0 && file_content[check_pos - 1] != '\n' && chars_back < 50) {
                check_pos--;
                chars_back++;
                if (check_pos >= 4 && file_content.substr(check_pos - 4, 4) == "wire") {
                    is_wire = true;
                    break;
                }
            }
            
            if (!is_wire) {
                auto instance = parse_verilog_instance(file_content, pos, net_names);
                if (instance) {
                    instance->module_name = current_module;
                    db.instances[instance->name] = instance;
                    instance_count++;
                }
                
                // Skip to after the semicolon to continue parsing
                size_t semicolon_pos = file_content.find(';', pos);
                pos = (semicolon_pos != std::string::npos) ? semicolon_pos + 1 : pos + 1;
            } else {
                pos += 4;
            }
        }
        else {
            pos++;
        }
    }
    
    std::cout << "    Found " << module_count << " modules" << std::endl;
    std::cout << "    Parsed " << instance_count << " instances" << std::endl;
    
    // Create nets
    int net_count = 0;
    for (const std::string& net_name : net_names) {
        auto net = std::make_shared<Net>();
        net->name = net_name;
        
        // 檢查是否為時鐘信號
        if (net_name == "clk" || net_name.find("clock") != std::string::npos) {
            net->type = Net::CLOCK;
            net->is_clock_net = true;
        }
        
        db.nets[net_name] = net;
        net_count++;
    }
    
    std::cout << "    Created " << net_count << " nets" << std::endl;
}

// Export detailed module instance distribution report
void export_module_instance_distribution(const DesignDatabase& db, const std::string& output_file) {
    std::ofstream out(output_file);
    if (!out.is_open()) {
        std::cout << "ERROR: Cannot create " << output_file << std::endl;
        return;
    }
    
    std::cout << "  Exporting module instance distribution to: " << output_file << std::endl;
    
    out << "=== Module Instance Distribution Report (Post-Legalization) ===" << std::endl;
    out << "Generated: " << __DATE__ << " " << __TIME__ << std::endl;
    out << "Total modules: " << db.modules.size() << std::endl;
    out << "Total instances: " << db.instances.size() << std::endl << std::endl;
    
    // Create module-wise instance statistics
    std::map<std::string, std::vector<std::shared_ptr<Instance>>> module_instances;
    std::map<std::string, int> module_ff_counts;
    std::map<std::string, int> module_total_counts;
    
    // Group instances by module
    for (const auto& inst_pair : db.instances) {
        const auto& instance = inst_pair.second;
        std::string module = instance->module_name.empty() ? "UNASSIGNED" : instance->module_name;
        
        module_instances[module].push_back(instance);
        module_total_counts[module]++;
        
        if (instance->is_flip_flop()) {
            module_ff_counts[module]++;
        }
    }
    
    // Output module summary
    out << "=== Module Summary ===" << std::endl;
    for (const auto& module : db.modules) {
        std::string module_name = module.name;
        int total_instances = module_total_counts[module_name];
        int ff_instances = module_ff_counts[module_name];
        int comb_instances = total_instances - ff_instances;
        
        out << "Module: " << module_name << std::endl;
        out << "  Total instances: " << total_instances << std::endl;
        out << "  FF instances: " << ff_instances << std::endl;
        out << "  Combinational instances: " << comb_instances << std::endl;
        out << "  Verilog position: " << module.start_pos << "-" << module.end_pos << std::endl;
        out << std::endl;
    }
    
    // Handle unassigned instances if any
    if (module_total_counts.find("UNASSIGNED") != module_total_counts.end()) {
        int total_instances = module_total_counts["UNASSIGNED"];
        int ff_instances = module_ff_counts["UNASSIGNED"];
        int comb_instances = total_instances - ff_instances;
        
        out << "Module: UNASSIGNED" << std::endl;
        out << "  Total instances: " << total_instances << std::endl;
        out << "  FF instances: " << ff_instances << std::endl;
        out << "  Combinational instances: " << comb_instances << std::endl;
        out << std::endl;
    }
    
    // Output detailed instance lists for each module
    out << "=== Detailed Instance Lists ===" << std::endl;
    
    for (const auto& module_pair : module_instances) {
        const std::string& module_name = module_pair.first;
        const auto& instances = module_pair.second;
        
        out << "\n--- Module: " << module_name << " ---" << std::endl;
        out << "Instance count: " << instances.size() << std::endl;
        
        // Sort instances: FFs first, then combinational
        std::vector<std::shared_ptr<Instance>> ff_instances;
        std::vector<std::shared_ptr<Instance>> comb_instances;
        
        for (const auto& instance : instances) {
            if (instance->is_flip_flop()) {
                ff_instances.push_back(instance);
            } else {
                comb_instances.push_back(instance);
            }
        }
        
        // Output FF instances
        if (!ff_instances.empty()) {
            out << "\nFlip-Flop Instances (" << ff_instances.size() << "):" << std::endl;
            int count = 1;
            for (const auto& instance : ff_instances) {
                out << "  [" << count++ << "] " << instance->name;
                if (instance->cell_template) {
                    out << " (" << instance->cell_template->name << ")";
                }
                out << " @ (" << instance->position.x << ", " << instance->position.y << ")";
                if (!instance->cluster_id.empty()) {
                    out << " [cluster: " << instance->cluster_id << "]";
                }
                out << std::endl;
            }
        }
        
        // Output first 20 combinational instances (to avoid extremely long files)
        if (!comb_instances.empty()) {
            out << "\nCombinational Instances (" << comb_instances.size() << ", showing first 20):" << std::endl;
            int count = 1;
            int max_show = std::min(20, (int)comb_instances.size());
            for (int i = 0; i < max_show; i++) {
                const auto& instance = comb_instances[i];
                out << "  [" << count++ << "] " << instance->name;
                if (instance->cell_template) {
                    out << " (" << instance->cell_template->name << ")";
                }
                out << " @ (" << instance->position.x << ", " << instance->position.y << ")" << std::endl;
            }
            if (comb_instances.size() > 20) {
                out << "  ... and " << (comb_instances.size() - 20) << " more combinational instances" << std::endl;
            }
        }
    }
    
    // Final statistics
    out << "\n=== Final Statistics ===" << std::endl;
    int total_ff = 0, total_comb = 0;
    for (const auto& pair : module_ff_counts) {
        total_ff += pair.second;
    }
    for (const auto& pair : module_total_counts) {
        total_comb += pair.second;
    }
    total_comb -= total_ff;
    
    out << "Total FF instances across all modules: " << total_ff << std::endl;
    out << "Total combinational instances across all modules: " << total_comb << std::endl;
    out << "Total instances: " << (total_ff + total_comb) << std::endl;
    
    out.close();
    std::cout << "    Module distribution report exported successfully" << std::endl;
}

// Legacy function - kept for compatibility but not used
void assign_instances_to_modules(DesignDatabase& db) {
    // This function is now obsolete as instances are assigned during parsing
    std::cout << "  Legacy assign_instances_to_modules called - skipping" << std::endl;
}

// 清理net名稱（移除轉義字符等）
std::string clean_net_name(const std::string& raw_name) {
    std::string cleaned = raw_name;
    
    // 移除前後空白
    cleaned = trim_whitespace(cleaned);
    
    // IMPORTANT: 保留轉義字符作為net name的一部分
    // 這樣在最終.v輸出時，轉義的net names會正確保持轉義格式
    // 例如："\qo_foo4[68] " 會被完整保留
    
    // 不移除轉義字符 - 保留原始格式
    // 不移除 [ ] 等字符 - 保留完整的bus名稱
    
    return cleaned;
}

// 解析pin連接 ".CK ( clk )" -> pin="CK", net="clk"
std::pair<std::string, std::string> parse_pin_connection(const std::string& conn_str) {
    // 尋找 .pin_name ( net_name )
    size_t dot_pos = conn_str.find(".");
    if (dot_pos == std::string::npos) return {"", ""};
    
    size_t pin_start = dot_pos + 1;
    size_t pin_end = conn_str.find("(", pin_start);
    if (pin_end == std::string::npos) return {"", ""};
    
    std::string pin_name = trim_whitespace(conn_str.substr(pin_start, pin_end - pin_start));
    
    size_t net_start = pin_end + 1;
    size_t net_end = conn_str.find(")", net_start);
    if (net_end == std::string::npos) return {"", ""};
    
    std::string net_name = clean_net_name(conn_str.substr(net_start, net_end - net_start));
    
    return {pin_name, net_name};
}

// 解析Verilog instance
std::shared_ptr<Instance> parse_verilog_instance(const std::string& content, size_t start_pos, std::set<std::string>& net_names) {
    // 找到cell名稱 (從SNPS開始)
    size_t cell_name_start = start_pos;
    size_t cell_name_end = content.find_first_of(" \t\n", cell_name_start);
    if (cell_name_end == std::string::npos) return nullptr;
    
    std::string cell_type = content.substr(cell_name_start, cell_name_end - cell_name_start);
    
    // 找到instance名稱
    size_t inst_name_start = content.find_first_not_of(" \t\n", cell_name_end);
    if (inst_name_start == std::string::npos) return nullptr;
    
    size_t inst_name_end = content.find_first_of(" \t\n(", inst_name_start);
    if (inst_name_end == std::string::npos) return nullptr;
    
    std::string instance_name = content.substr(inst_name_start, inst_name_end - inst_name_start);
    
    // 找到連接部分 ( ... )
    size_t conn_start = content.find("(", inst_name_end);
    if (conn_start == std::string::npos) return nullptr;
    
    // 找到匹配的結束括號
    int paren_count = 1;
    size_t conn_pos = conn_start + 1;
    while (conn_pos < content.length() && paren_count > 0) {
        if (content[conn_pos] == '(') paren_count++;
        else if (content[conn_pos] == ')') paren_count--;
        conn_pos++;
    }
    
    if (paren_count != 0) return nullptr;
    
    std::string connections_str = content.substr(conn_start + 1, conn_pos - conn_start - 2);
    
    // 創建Instance
    auto instance = std::make_shared<Instance>();
    instance->name = instance_name;
    instance->cell_type = cell_type;
    
    // 解析所有pin連接
    // 分割連接字符串 by ","
    std::vector<std::string> conn_parts;
    size_t pos = 0;
    int paren_level = 0;
    size_t part_start = 0;
    
    while (pos < connections_str.length()) {
        if (connections_str[pos] == '(') {
            paren_level++;
        } else if (connections_str[pos] == ')') {
            paren_level--;
        } else if (connections_str[pos] == ',' && paren_level == 0) {
            // 找到頂層的逗號
            std::string part = trim_whitespace(connections_str.substr(part_start, pos - part_start));
            if (!part.empty()) {
                conn_parts.push_back(part);
            }
            part_start = pos + 1;
        }
        pos++;
    }
    
    // 添加最後一部分
    std::string last_part = trim_whitespace(connections_str.substr(part_start));
    if (!last_part.empty()) {
        conn_parts.push_back(last_part);
    }
    
    // 解析每個連接
    for (const std::string& part : conn_parts) {
        std::pair<std::string, std::string> pin_net = parse_pin_connection(part);
        std::string pin_name = pin_net.first;
        std::string net_name = pin_net.second;
        
        if (!pin_name.empty() && !net_name.empty()) {
            // 規範化特殊連接的名稱，便於後續分群分析
            std::string normalized_net = net_name;
            
            // UNCONNECTED信號統一標記
            if (net_name.find("SYNOPSYS_UNCONNECTED") != std::string::npos) {
                normalized_net = "UNCONNECTED";
            }
            // 電源和地線信號統一標記
            else if (is_power_net(net_name)) {
                normalized_net = "VDD";
            }
            else if (is_ground_net(net_name)) {
                normalized_net = "VSS";
            }
            
            // 記錄規範化後的連接
            instance->connections.emplace_back(pin_name, normalized_net);
            
            // 只有實際信號網路才加入到net_names中創建Net對象
            if (normalized_net != "UNCONNECTED" && normalized_net != "VDD" && normalized_net != "VSS") {
                net_names.insert(net_name);  // 使用原始名稱創建Net對象
            }
        }
    }
    
    return instance;
}

// 建立net連接關係
void build_net_connections(DesignDatabase& db) {
    for (const auto& inst_pair : db.instances) {
        const auto& instance = inst_pair.second;
        
        for (const auto& conn : instance->connections) {
            auto net_it = db.nets.find(conn.net_name);
            if (net_it != db.nets.end()) {
                // 添加連接到net
                net_it->second->connections.emplace_back(instance->name, conn.pin_name);
            }
        }
    }
}

// =============================================================================
// DEF PARSER HELPER FUNCTIONS
// =============================================================================

// 解析DIEAREA行: "DIEAREA ( 0 0 ) ( 0 610000 ) ( 809940 610000 ) ( 809940 0 ) ;"
void parse_diearea_line(const std::string& line, DesignDatabase& db) {
    // 簡化處理：只取第一個和第三個點來確定die area
    std::vector<std::pair<double, double>> points;
    
    size_t pos = 0;
    while (pos < line.length()) {
        size_t open_paren = line.find("(", pos);
        if (open_paren == std::string::npos) break;
        
        size_t close_paren = line.find(")", open_paren);
        if (close_paren == std::string::npos) break;
        
        std::string coord_str = line.substr(open_paren + 1, close_paren - open_paren - 1);
        std::istringstream iss(coord_str);
        double x, y;
        if (iss >> x >> y) {
            points.emplace_back(x , y ); // Convert from nanometers to microns
        }
        
        pos = close_paren + 1;
    }
    
    if (points.size() >= 3) {
        db.die_area.x1 = std::min(points[0].first, points[2].first);
        db.die_area.y1 = std::min(points[0].second, points[2].second);
        db.die_area.x2 = std::max(points[0].first, points[2].first);
        db.die_area.y2 = std::max(points[0].second, points[2].second);
    }
}

// 解析TRACKS行: "TRACKS Y 42 DO 8243 STEP 74 LAYER M1 ;"
void parse_track_line(const std::string& line, DesignDatabase& db) {
    std::istringstream iss(line);
    std::string token;
    
    Track track;
    iss >> token; // "TRACKS"
    
    std::string direction;
    iss >> direction; // "X" or "Y"
    track.direction = (direction == "X") ? Track::X : Track::Y;
    
    iss >> track.start; // starting coordinate
    
    iss >> token; // "DO"
    iss >> track.num; // number of tracks
    iss >> token; // "STEP"
    iss >> track.step; // step size
    
    
    iss >> token; // "LAYER"
    iss >> track.layer; // layer name
    
    db.tracks.push_back(track);
}

// 解析ROW行: "ROW unit_row_1 unit 5000 5000 FS DO 10810 BY 1 STEP 74 0 ;"
void parse_row_line(const std::string& line, DesignDatabase& db) {
    std::istringstream iss(line);
    std::string token;
    
    PlacementRow row;
    iss >> token; // "ROW"
    iss >> row.name; // row name
    iss >> row.site; // site type
    
    double x, y;
    iss >> x >> y; // origin coordinates
    row.origin.x = x ; // Convert to microns
    row.origin.y = y ;
    
    std::string orientation;
    iss >> orientation; // "FS", "N", etc.
    
    iss >> token; // "DO"
    iss >> row.num_x; // number of sites in X
    iss >> token; // "BY"  
    iss >> row.num_y; // number of sites in Y
    iss >> token; // "STEP"
    iss >> row.step_x >> row.step_y; // step size
    
    
    db.placement_rows.push_back(row);

    if(db.placement_rows.size()>1)
    {
        db.placement_rows[db.placement_rows.size()-2].height = row.origin.y-db.placement_rows[db.placement_rows.size()-2].origin.y;
        //row.height=db.placement_rows[db.placement_rows.size()-2].height;
        db.placement_rows[db.placement_rows.size()-1].height = db.placement_rows[db.placement_rows.size()-2].height;
    }
}

// 解析NETS section
void parse_nets_section(std::ifstream& file, DesignDatabase& db) {
    std::string line;
    int parsed_nets = 0;
    int existing_nets = 0;
    
    while (std::getline(file, line)) {
        line = trim_whitespace(line);
        
        // 檢查NETS結束
        if (line == "END NETS") {
            std::cout << "    Finished NETS section" << std::endl;
            break;
        }
        
        // 解析net定義: "- net_name ( instance_name pin_name ) ... ;"
        if (line.find("- ") == 0) {
            size_t name_start = 2; // after "- "
            size_t name_end = line.find(" ", name_start);
            if (name_end == std::string::npos) continue;
            
            std::string net_name = line.substr(name_start, name_end - name_start);
            
            // 創建或更新net (確保所有DEF中的nets都存在)
            auto existing_net = db.nets.find(net_name);
            if (existing_net == db.nets.end()) {
                auto net = std::make_shared<Net>();
                net->name = net_name;
                
                // 檢查是否為時鐘信號
                if (net_name == "clk" || net_name.find("clock") != std::string::npos) {
                    net->type = Net::CLOCK;
                    net->is_clock_net = true;
                }
                
                db.nets[net_name] = net;
                parsed_nets++;
            } else {
                // 更新已存在的net資訊（如果需要的話）
                existing_nets++;
            }
        }
    }
    
    std::cout << "    Parsed " << parsed_nets << " new nets from DEF (total DEF nets: " << (parsed_nets + existing_nets) << ")" << std::endl;
}

// 解析RECT行: "RECT ( x1 y1 ) ( x2 y2 ) ;"
bool parse_rect_line(const std::string& line, Rectangle& rect) {
    // 找到兩個座標點
    size_t first_open = line.find("(");
    if (first_open == std::string::npos) return false;
    
    size_t first_close = line.find(")", first_open);
    if (first_close == std::string::npos) return false;
    
    size_t second_open = line.find("(", first_close);
    if (second_open == std::string::npos) return false;
    
    size_t second_close = line.find(")", second_open);
    if (second_close == std::string::npos) return false;
    
    // 解析第一個點 (x1, y1)
    std::string first_point = line.substr(first_open + 1, first_close - first_open - 1);
    std::istringstream iss1(first_point);
    if (!(iss1 >> rect.x1 >> rect.y1)) return false;
    
    // 解析第二個點 (x2, y2)
    std::string second_point = line.substr(second_open + 1, second_close - second_open - 1);
    std::istringstream iss2(second_point);
    if (!(iss2 >> rect.x2 >> rect.y2)) return false;
    
    return true;
}

// 解析BLOCKAGES section
void parse_blockages_section(std::ifstream& file, DesignDatabase& db) {
    std::string line;
    int parsed_placement_blockages = 0;
    int skipped_layer_blockages = 0;
    
    while (std::getline(file, line)) {
        line = trim_whitespace(line);
        
        // 檢查BLOCKAGES結束
        if (line == "END BLOCKAGES") {
            std::cout << "    Finished BLOCKAGES section" << std::endl;
            break;
        }
        
        // 解析PLACEMENT blockage: "- PLACEMENT"
        if (line == "- PLACEMENT") {
            // 讀取下一行應該是 RECT 定義
            if (std::getline(file, line)) {
                line = trim_whitespace(line);
                if (line.find("RECT") == 0) {
                    Rectangle rect;
                    if (parse_rect_line(line, rect)) {
                        db.placement_blockages.push_back(rect);
                        parsed_placement_blockages++;
                    }
                }
            }
        }
        
        // 跳過LAYER blockages (我們不需要處理)
        else if (line.find("- LAYER") == 0) {
            skipped_layer_blockages++;
            // 讀取下一行(通常是RECT)但不處理
            std::getline(file, line);
        }
    }
    
    std::cout << "    Parsed " << parsed_placement_blockages << " placement blockages" << std::endl;
    std::cout << "    Skipped " << skipped_layer_blockages << " layer blockages" << std::endl;
}

// 解析SPECIALNETS section  
void parse_specialnets_section(std::ifstream& file, DesignDatabase& db) {
    std::string line;
    int parsed_special_nets = 0;
    
    while (std::getline(file, line)) {
        line = trim_whitespace(line);
        
        // 檢查SPECIALNETS結束
        if (line == "END SPECIALNETS") {
            std::cout << "    Finished SPECIALNETS section" << std::endl;
            break;
        }
        
        // 解析special net定義: "- VDD ...", "- VSS ..."
        if (line.find("- ") == 0) {
            size_t name_start = 2; // after "- "
            size_t name_end = line.find(" ", name_start);
            if (name_end == std::string::npos) continue;
            
            std::string net_name = line.substr(name_start, name_end - name_start);
            
            // 創建special net
            auto net = std::make_shared<Net>();
            net->name = net_name;
            
            // 設定net類型
            if (net_name == "VDD" || net_name == "VCC" || net_name.find("VDD") != std::string::npos) {
                net->type = Net::POWER;
            } else if (net_name == "VSS" || net_name == "GND" || net_name.find("VSS") != std::string::npos) {
                net->type = Net::GROUND;
            } else {
                net->type = Net::SIGNAL;
            }
            
            db.nets[net_name] = net;
            parsed_special_nets++;
        }
    }
    
    std::cout << "    Parsed " << parsed_special_nets << " special nets from DEF" << std::endl;
}

// 解析COMPONENT行: " - c_n11 SNPSSLOPT25_INV_4 + PLACED ( 752442 5626 ) N ;" 或 "- c_n11 ..."
bool parse_component_line(const std::string& line, DesignDatabase& db) {
    // 尋找instance名稱 (在" - "或"- "之後)
    size_t dash_pos = line.find(" - ");
    if (dash_pos == std::string::npos) {
        dash_pos = line.find("- ");
        if (dash_pos != 0) return false; // "- " 必須在開頭
        dash_pos = 0; // 調整位置
    }
    
    size_t name_start;
    if (dash_pos == 0) {
        name_start = 2; // "- " 後面
    } else {
        name_start = dash_pos + 3; // " - " 後面
    }
    size_t name_end = line.find(" ", name_start);
    if (name_end == std::string::npos) return false;
    
    std::string instance_name = line.substr(name_start, name_end - name_start);
    
    // 查找對應的instance
    auto inst_it = db.instances.find(instance_name);
    std::string hierarchical_name = instance_name;  // Store original hierarchical name
    
    if (inst_it == db.instances.end()) {
        // Try extracting flat name from hierarchical path
        // e.g., "hier_top_mod_5/.../c_n13530" -> "c_n13530"
        size_t last_slash = instance_name.find_last_of('/');
        if (last_slash != std::string::npos) {
            std::string flat_name = instance_name.substr(last_slash + 1);
            inst_it = db.instances.find(flat_name);
            if (inst_it == db.instances.end()) {
                return false; // Instance not found even with flat name
            }
            // Update the instance name to preserve hierarchy information AND fix db.instances key
            auto instance_ptr = inst_it->second;
            instance_ptr->name = hierarchical_name;
            
            // Remove old flat name key and insert with hierarchical name key
            db.instances.erase(inst_it);
            db.instances[hierarchical_name] = instance_ptr;
            
            // Update inst_it to point to new location
            inst_it = db.instances.find(hierarchical_name);
        } else {
            return false; // Instance not found in netlist
        }
    }
    
    // 尋找PLACED coordinates
    size_t placed_pos = line.find("PLACED (");
    if (placed_pos == std::string::npos) {
        size_t fixed_pos = line.find("FIXED (");
        if (fixed_pos != std::string::npos) {
            placed_pos = fixed_pos;
            inst_it->second->placement_status = Instance::FIXED;
        } else {
            return false;
        }
    } else {
        inst_it->second->placement_status = Instance::PLACED;
    }
    
    // 解析坐標
    size_t coord_start = line.find("(", placed_pos) + 1;
    size_t coord_end = line.find(")", coord_start);
    if (coord_end == std::string::npos) return false;
    
    std::string coord_str = line.substr(coord_start, coord_end - coord_start);
    std::istringstream iss(coord_str);
    double x, y;
    if (iss >> x >> y) {
        inst_it->second->position.x = x ; // Convert to microns
        inst_it->second->position.y = y ;
    }
    
    // 解析orientation (N, S, E, W, FN, FS, FE, FW)
    size_t orient_start = coord_end + 1;
    size_t orient_end = line.find(" ", orient_start);
    if (orient_end == std::string::npos) orient_end = line.find(";", orient_start);
    
    if (orient_start < line.length() && orient_end != std::string::npos) {
        std::string orient_str = trim_whitespace(line.substr(orient_start, orient_end - orient_start));
        if (orient_str == "N") inst_it->second->orientation = Instance::N;
        else if (orient_str == "S") inst_it->second->orientation = Instance::S;
        else if (orient_str == "E") inst_it->second->orientation = Instance::E;
        else if (orient_str == "W") inst_it->second->orientation = Instance::W;
        else if (orient_str == "FN") inst_it->second->orientation = Instance::FN;
        else if (orient_str == "FS") inst_it->second->orientation = Instance::FS;
        else if (orient_str == "FE") inst_it->second->orientation = Instance::FE;
        else if (orient_str == "FW") inst_it->second->orientation = Instance::FW;
    }
    
    return true;
}

// =============================================================================
// PARSER FUNCTION IMPLEMENTATIONS
// =============================================================================

void parse_liberty_file(const std::string& filepath, DesignDatabase& db) {
    std::cout << "  Parsing: " << filepath << std::endl;
    
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cout << "  SKIPPED: Cannot open " << filepath << std::endl;
        return;
    }
    
    // 提取library名稱（從檔案路徑）
    std::string library_name = extract_library_name(filepath);
    std::cout << "    Reading file..." << std::flush;
    
    // 讀取整個檔案內容 (with progress indicator)
    std::string file_content;
    std::string line;
    int line_count = 0;
    while(std::getline(file, line)) {
        file_content += line + "\n";
        line_count++;
        // 每10000行顯示一次進度
        // if (line_count % 10000 == 0) {
        //     std::cout << "." << std::flush;
        // }
    }
    file.close();
    
    std::cout << "\n    File size: " << file_content.length() << " chars, Library: " << library_name << std::endl;
    
    // 解析所有cell定義
    int cell_count = 0;
    size_t pos = 0;
    
    while(pos < file_content.length()) {
        size_t cell_start = file_content.find("cell(", pos);
        if(cell_start == std::string::npos) break;
        
        size_t name_start = cell_start + 5;
        size_t name_end = file_content.find(")", name_start);
        if(name_end == std::string::npos) break;
        
        std::string cell_name = file_content.substr(name_start, name_end - name_start);
        
        // 找到cell block的範圍
        size_t bracket_start = file_content.find("{", name_end);
        if(bracket_start == std::string::npos) break;
        
        // 找到匹配的結束括號
        int bracket_count = 1;
        size_t bracket_pos = bracket_start + 1;
        
        while(bracket_pos < file_content.length() && bracket_count > 0) {
            if(file_content[bracket_pos] == '{') bracket_count++;
            else if(file_content[bracket_pos] == '}') bracket_count--;
            bracket_pos++;
        }
        
        if(bracket_count == 0) {
            std::string cell_block = file_content.substr(cell_start, bracket_pos - cell_start);
            
            // 創建CellTemplate
            auto cell = std::make_shared<CellTemplate>();
            cell->name = cell_name;
            cell->library = library_name;
            
            // 解析cell屬性
            parse_cell_properties(*cell, cell_block);
            
            // 加入資料庫
            db.cell_library[cell->name] = cell;
            cell_count++;
        }
        
        pos = bracket_pos;
    }
    
    std::cout << "    Parsed " << cell_count << " cells" << std::endl;
}

void parse_lef_file(const std::string& filepath, DesignDatabase& db) {
    std::cout << "  Parsing: " << filepath << std::endl;
    
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cout << "  ERROR: Cannot open " << filepath << std::endl;
        return;
    }
    
    std::string line;
    int updated_cells = 0;
    int unknown_cells = 0;
    
    while (std::getline(file, line)) {
        // 找到MACRO定義
        if (line.find("MACRO ") == 0) {
            std::string cell_name = extract_cell_name_from_macro(line);
            
            if (!cell_name.empty()) {
                // 在cell_library中查找對應的cell
                auto cell_it = db.cell_library.find(cell_name);
                if (cell_it != db.cell_library.end()) {
                    // 解析這個MACRO的內容
                    parse_macro_content(file, cell_it->second);
                    updated_cells++;
                } else {
                    // 跳過未知的cell
                    skip_macro_content(file);
                    unknown_cells++;
                }
            }
        }
    }
    
    file.close();
    std::cout << "    Updated " << updated_cells << " cells with physical info" << std::endl;
    std::cout << "    Skipped " << unknown_cells << " unknown cells" << std::endl;
}

void parse_verilog_file(const std::string& filepath, DesignDatabase& db) {
    std::cout << "  Parsing: " << filepath << std::endl;
    
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cout << "  ERROR: Cannot open " << filepath << std::endl;
        return;
    }
    
    std::string file_content;
    std::string line;
    while(std::getline(file, line)) {
        file_content += line + "\n";
    }
    file.close();
    
    std::cout << "    File size: " << file_content.length() << " chars" << std::endl;
    
    // Store input verilog file path for later use
    db.input_verilog_path = filepath;
    
    // Parse module hierarchy and instances simultaneously
    parse_module_hierarchy_and_instances(file_content, db);
    
    // 解析module名稱 (keep for compatibility)
    std::string module_name = extract_module_name(file_content);
    if (!module_name.empty()) {
        db.design_name = module_name;
        std::cout << "    Design name: " << module_name << std::endl;
    }
    
    // 建立net連接關係
    build_net_connections(db);
    
    std::cout << "    Verilog parsing completed successfully" << std::endl;
}

void parse_targeted_ff_connections(const std::string& content, 
                                   const std::set<std::string>& targets,
                                   DesignDatabase& db) {
    std::set<std::string> net_names;
    size_t pos = 0;
    int found_count = 0;
    int skipped_count = 0;
    
    while ((pos = content.find("SNPS", pos)) != std::string::npos) {
        // Quick validation - not in wire declaration or comment
        size_t line_start = content.rfind('\n', pos);
        if (line_start == std::string::npos) line_start = 0;
        else line_start++;
        
        std::string line_prefix = content.substr(line_start, pos - line_start);
        if (line_prefix.find("wire") != std::string::npos || line_prefix.find("//") != std::string::npos) {
            pos += 4;
            continue;
        }
        
        auto instance = parse_verilog_instance(content, pos, net_names);
        if (instance && targets.count(instance->name)) {
            // Update existing instance with connection information
            auto existing = db.instances[instance->name];
            existing->connections = instance->connections;
            found_count++;
            
            if (found_count % 1000 == 0) {
                std::cout << "    Found " << found_count << " FF connections..." << std::endl;
            }
        } else if (instance) {
            skipped_count++;
        }
        
        // Smart jump: skip to end of instance declaration
        size_t semicolon_pos = content.find(';', pos);
        pos = (semicolon_pos != std::string::npos) ? semicolon_pos + 1 : pos + 4;
    }
    
    std::cout << "    Found " << found_count << " FF connections, skipped " 
              << skipped_count << " non-FF instances" << std::endl;
    
    // Create nets from collected net names
    int net_count = 0;
    for (const std::string& net_name : net_names) {
        auto net = std::make_shared<Net>();
        net->name = net_name;
        
        // Check if it's a clock signal
        if (net_name == "clk" || net_name.find("clock") != std::string::npos ||
            net_name.find("clk") != std::string::npos) {
            net->type = Net::CLOCK;
            net->is_clock_net = true;
        }
        
        db.nets[net_name] = net;
        net_count++;
    }
    
    std::cout << "    Created " << net_count << " nets" << std::endl;
}

void parse_verilog_file_selective(const std::string& filepath, DesignDatabase& db) {
    std::cout << "  Parsing: " << filepath << " (selective FF-only)" << std::endl;
    
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cout << "  ERROR: Cannot open " << filepath << std::endl;
        return;
    }
    
    std::string file_content;
    std::string line;
    while(std::getline(file, line)) {
        file_content += line + "\n";
    }
    file.close();
    
    std::cout << "    File size: " << file_content.length() << " chars" << std::endl;
    
    // Store input verilog file path for later use
    db.input_verilog_path = filepath;
    
    // Build target FF instance set from DEF-parsed instances
    std::set<std::string> ff_instance_names;
    int total_instances = 0;
    for (const auto& inst : db.instances) {
        total_instances++;
        if (inst.second->is_flip_flop()) {
            ff_instance_names.insert(inst.second->name);
        }
    }
    
    std::cout << "    Total instances from DEF: " << total_instances << std::endl;
    std::cout << "    Target FF instances: " << ff_instance_names.size() << std::endl;
    
    // Parse targeted FF connections only
    parse_targeted_ff_connections(file_content, ff_instance_names, db);
    
    // Extract design name for compatibility
    std::string module_name = extract_module_name(file_content);
    if (!module_name.empty()) {
        db.design_name = module_name;
        std::cout << "    Design name: " << module_name << std::endl;
    }
    
    // Build net connections for FFs only
    build_net_connections(db);
    
    std::cout << "    Selective verilog parsing completed successfully" << std::endl;
}

void parse_def_file(const std::string& filepath, DesignDatabase& db) {
    std::cout << "  Parsing: " << filepath << std::endl;
    
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cout << "  ERROR: Cannot open " << filepath << std::endl;
        return;
    }
    
    std::string line;
    int placed_instances = 0;
    int rows_parsed = 0;
    int tracks_parsed = 0;
    bool in_components = false;
    
    while (std::getline(file, line)) {
        line = trim_whitespace(line);
        
        // 解析DIEAREA
        if (line.find("DIEAREA") == 0) {
            parse_diearea_line(line, db);
        }
        
        // 解析ROW
        else if (line.find("ROW ") == 0) {
            parse_row_line(line, db);
            rows_parsed++;
        }
        
        // 解析TRACKS
        else if (line.find("TRACKS ") == 0) {
            parse_track_line(line, db);
            tracks_parsed++;
        }
        
        // 檢查COMPONENTS開始
        else if (line.find("COMPONENTS ") == 0) {
            in_components = true;
            std::cout << "    Found COMPONENTS section" << std::endl;
        }
        
        // 檢查COMPONENTS結束
        else if (line == "END COMPONENTS") {
            in_components = false;
            std::cout << "    Finished COMPONENTS section" << std::endl;
        }
        
        // 檢查NETS開始
        else if (line.find("NETS ") == 0) {
            std::cout << "    Found NETS section" << std::endl;
            parse_nets_section(file, db);
        }
        
        // 檢查BLOCKAGES開始
        else if (line.find("BLOCKAGES ") == 0) {
            std::cout << "    Found BLOCKAGES section" << std::endl;
            parse_blockages_section(file, db);
        }
        
        // 檢查SPECIALNETS開始
        else if (line.find("SPECIALNETS ") == 0) {
            std::cout << "    Found SPECIALNETS section" << std::endl;
            parse_specialnets_section(file, db);
        }
        
        // 檢查SCANDEF開始
        else if (line == "SCANCHAINS" || line.find("SCANCHAINS ") == 0) {
            std::cout << "    Found SCANCHAINS section" << std::endl;
            parse_scandef_section(file, db);
        }
        
        // 解析instance placement (檢查兩種格式：" - " 和 "- ")
        else if (in_components && (line.find(" - ") == 0 || line.find("- ") == 0)) {
            if (parse_component_line(line, db)) {
                placed_instances++;
            }
        }
    }
    
    file.close();
    std::cout << "    Placed " << placed_instances << " instances" << std::endl;
    std::cout << "    Parsed " << rows_parsed << " placement rows" << std::endl;
    std::cout << "    Parsed " << tracks_parsed << " routing tracks" << std::endl;
    std::cout << "    Die area: " << db.die_area.width() << " x " << db.die_area.height() << std::endl;
}

void parse_weight_file(const std::string& filepath, DesignDatabase& db) {
    std::cout << "  Parsing: " << filepath << std::endl;
    
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cout << "  ERROR: Cannot open " << filepath << std::endl;
        return;
    }
    
    std::string line;
    int values_parsed = 0;
    
    while (std::getline(file, line)) {
        line = trim_whitespace(line);
        if (line.empty()) continue;
        
        std::istringstream iss(line);
        std::string key;
        double value;
        
        if (iss >> key >> value) {
            if (key == "Alpha") {
                db.objective_weights.alpha = value;
                values_parsed++;
            }
            else if (key == "Beta") {
                db.objective_weights.beta = value;
                values_parsed++;
            }
            else if (key == "Gamma") {
                db.objective_weights.gamma = value;
                values_parsed++;
            }
            else if (key == "TNS") {
                db.objective_weights.initial_tns = value;
                values_parsed++;
            }
            else if (key == "TPO") {
                db.objective_weights.initial_power = value;
                values_parsed++;
            }
            else if (key == "Area") {
                db.objective_weights.initial_area = value;
                values_parsed++;
            }
            else {
                std::cout << "    WARNING: Unknown weight parameter: " << key << std::endl;
            }
        }
    }
    
    file.close();
    std::cout << "    Parsed " << values_parsed << " weight parameters" << std::endl;
    std::cout << "    Objective: " << db.objective_weights.alpha << "*TNS + " 
              << db.objective_weights.beta << "*Power + " 
              << db.objective_weights.gamma << "*Area" << std::endl;
    std::cout << "    Initial values: TNS=" << db.objective_weights.initial_tns 
              << ", Power=" << db.objective_weights.initial_power 
              << ", Area=" << db.objective_weights.initial_area << std::endl;
}

// =============================================================================
// VALIDATION AND OUTPUT FUNCTIONS
// =============================================================================

void build_banking_relationships(DesignDatabase& db) {
    std::cout << "  Building banking relationships..." << std::endl;
    
    // 遍歷所有cells，找到有single_bit_degenerate的multi-bit FF
    for (const auto& pair : db.cell_library) {
        const auto& cell = pair.second;
        
        // 如果這個cell有single_bit_degenerate且不是"null"
        if (cell->is_flip_flop() && cell->single_bit_degenerate != "null") {
            // 找到對應的single-bit FF
            auto target_it = db.cell_library.find(cell->single_bit_degenerate);
            if (target_it != db.cell_library.end()) {
                // 檢查是否已經存在，避免重複添加
                auto& targets = target_it->second->banking_targets;
                if (std::find(targets.begin(), targets.end(), cell->name) == targets.end()) {
                    // 在single-bit FF的banking_targets中加入這個multi-bit FF
                    targets.push_back(cell->name);
                }
            } else {
                std::cout << "    WARNING: " << cell->name << " references unknown single-bit cell: " 
                         << cell->single_bit_degenerate << std::endl;
            }
        }
    }
    
    // 統計banking關係
    int single_bit_with_targets = 0;
    int multi_bit_with_parents = 0;
    
    for (const auto& pair : db.cell_library) {
        const auto& cell = pair.second;
        if (cell->is_flip_flop()) {
            if (!cell->banking_targets.empty()) {
                single_bit_with_targets++;
            }
            if (cell->single_bit_degenerate != "null") {
                multi_bit_with_parents++;
            }
        }
    }
    
    std::cout << "    Single-bit FFs with banking targets: " << single_bit_with_targets << std::endl;
    std::cout << "    Multi-bit FFs with parents: " << multi_bit_with_parents << std::endl;
}

void export_cell_library_validation(const DesignDatabase& db, const std::string& output_file) {
    std::ofstream out(output_file);
    if (!out.is_open()) {
        std::cout << "ERROR: Cannot create " << output_file << std::endl;
        return;
    }
    
    out << "=== Cell Library Validation Report ===" << std::endl;
    out << "Total cells: " << db.cell_library.size() << std::endl;
    out << "Generated: " << __DATE__ << " " << __TIME__ << std::endl << std::endl;
    
    // 統計各library的cell數量
    std::map<std::string, int> library_stats;
    std::map<std::string, int> type_stats;
    
    for (const auto& pair : db.cell_library) {
        const auto& cell = pair.second;
        library_stats[cell->library]++;
        
        if (cell->type == CellTemplate::FLIP_FLOP) {
            type_stats["FLIP_FLOP"]++;
        } else {
            type_stats["OTHER"]++;
        }
    }
    
    out << "=== Library Statistics ===" << std::endl;
    for (const auto& pair : library_stats) {
        out << pair.first << ": " << pair.second << " cells" << std::endl;
    }
    
    out << "\n=== Cell Type Statistics ===" << std::endl;
    for (const auto& pair : type_stats) {
        out << pair.first << ": " << pair.second << " cells" << std::endl;
    }
    
    out << "\n=== All Flip-Flop Cells (complete listing) ===" << std::endl;
    int count = 0;
    for (const auto& pair : db.cell_library) {
        const auto& cell = pair.second;
        
        // 只輸出flip-flop cells
        if (cell->type != CellTemplate::FLIP_FLOP) {
            continue;
        }
        
        out << "\n[" << (count + 1) << "] " << cell->name << std::endl;
        out << "  Library: " << cell->library << std::endl;
        out << "  Type: " << (cell->type == CellTemplate::FLIP_FLOP ? "FLIP_FLOP" : "OTHER") << std::endl;
        
        out << "  Clock Edge: " << cell->get_clock_edge_string() << std::endl;
        
        std::map<Pin::FlipFlopPinType, int> pin_type_count;
        for (const auto& pin : cell->pins) {
            pin_type_count[pin.ff_pin_type]++;
        }
        
        out << "  Pin Types: ";
        bool first = true;
        for (const auto& pair : pin_type_count) {
            if (pair.second > 0 && pair.first != Pin::FF_NOT_FF_PIN) {
                if (!first) out << ", ";
                switch (pair.first) {
                    case Pin::FF_DATA_INPUT: out << "D(" << pair.second << ")"; break;
                    case Pin::FF_DATA_OUTPUT: out << "Q(" << pair.second << ")"; break;
                    case Pin::FF_DATA_OUTPUT_N: out << "QN(" << pair.second << ")"; break;
                    case Pin::FF_CLOCK: out << "CLK(" << pair.second << ")"; break;
                    case Pin::FF_SCAN_INPUT: out << "SI(" << pair.second << ")"; break;
                    case Pin::FF_SCAN_OUTPUT: out << "SO(" << pair.second << ")"; break;
                    case Pin::FF_SCAN_ENABLE: out << "SE(" << pair.second << ")"; break;
                    case Pin::FF_RESET: out << "RST(" << pair.second << ")"; break;
                    case Pin::FF_SET: out << "S(" << pair.second << ")"; break;
                    case Pin::FF_RD: out << "RD(" << pair.second << ")"; break;
                    case Pin::FF_SD: out << "SD(" << pair.second << ")"; break;
                    case Pin::FF_SR: out << "SR(" << pair.second << ")"; break;
                    case Pin::FF_RS: out << "RS(" << pair.second << ")"; break;
                    case Pin::FF_VDDR: out << "VDDR(" << pair.second << ")"; break;
                    case Pin::FF_OTHER: out << "OTHER(" << pair.second << ")"; break;
                    default: break;
                }
                first = false;
            }
        }
        out << std::endl;
        out << "  Bit Width: " << cell->bit_width << std::endl;
        out << "  Physical Size: " << cell->width << " x " << cell->height 
            << " (Area: " << (cell->width * cell->height) << ")" << std::endl;
        out << "  Logical Area: " << cell->area << std::endl;
        out << "  Leakage Power: " << cell->leakage_power << std::endl;
        out << "  Single-bit Degenerate: " << cell->single_bit_degenerate << std::endl;
        if (!cell->banking_targets.empty()) {
            out << "  Banking Targets: ";
            for (size_t i = 0; i < cell->banking_targets.size(); i++) {
                if (i > 0) out << ", ";
                out << cell->banking_targets[i];
            }
            out << std::endl;
        }
        out << "  Pins: " << cell->pins.size() << std::endl;
        
        // 顯示詳細pin信息
        for (const auto& pin : cell->pins) {
            out << "    Pin " << pin.name << " (";
            out << (pin.direction == Pin::INPUT ? "IN" : pin.direction == Pin::OUTPUT ? "OUT" : "INOUT");
            out << ", " << (pin.usage == Pin::CLOCK ? "CLK" : "SIG");
            if (pin.ff_pin_type != Pin::FF_NOT_FF_PIN) {
                out << ", " << pin.get_ff_pin_type_string();
            }
            out << ")" << std::endl;
        }
        
        count++;
    }
    
    out.close();
    std::cout << "\n📄 Cell library validation exported to: " << output_file << std::endl;
}

void export_instance_validation(const DesignDatabase& db, const std::string& output_file) {
    std::ofstream out(output_file);
    if (!out.is_open()) {
        std::cout << "ERROR: Cannot create " << output_file << std::endl;
        return;
    }
    
    out << "=== Instance and Net Validation Report ===" << std::endl;
    out << "Total instances: " << db.instances.size() << std::endl;
    out << "Total nets: " << db.nets.size() << std::endl;
    out << "Design: " << db.design_name << std::endl;
    out << "Total modules: " << db.modules.size() << std::endl;
    out << "Placement blockages: " << db.placement_blockages.size() << std::endl;
    out << "Generated: " << __DATE__ << " " << __TIME__ << std::endl << std::endl;
    
    // 統計不同類型的instances
    std::map<std::string, int> cell_type_stats;
    std::map<std::string, int> library_stats;
    std::map<std::string, int> module_stats;  // Module assignment statistics
    int flip_flop_count = 0;
    int linked_count = 0;
    
    for (const auto& pair : db.instances) {
        const auto& inst = pair.second;
        cell_type_stats[inst->cell_type]++;
        
        // Count module assignments
        module_stats[inst->module_name.empty() ? "UNASSIGNED" : inst->module_name]++;
        
        if (inst->cell_template) {
            linked_count++;
            library_stats[inst->cell_template->library]++;
            if (inst->is_flip_flop()) {
                flip_flop_count++;
            }
        }
    }
    
    out << "=== Instance Statistics ===" << std::endl;
    out << "Linked to cell templates: " << linked_count << "/" << db.instances.size() << std::endl;
    out << "Flip-flops: " << flip_flop_count << std::endl;
    
    out << "\n=== Cell Type Usage ===" << std::endl;
    for (const auto& pair : cell_type_stats) {
        out << pair.first << ": " << pair.second << " instances" << std::endl;
    }
    
    out << "\n=== Library Usage ===" << std::endl;
    for (const auto& pair : library_stats) {
        out << pair.first << ": " << pair.second << " instances" << std::endl;
    }
    
    out << "\n=== Module Assignment ===" << std::endl;
    if (!db.modules.empty()) {
        out << "Module details:" << std::endl;
        for (const auto& module : db.modules) {
            out << "  " << module.name << " (pos: " << module.start_pos << "-" << module.end_pos << ")" << std::endl;
        }
        out << std::endl;
    }
    out << "Instance distribution by module:" << std::endl;
    for (const auto& pair : module_stats) {
        out << "  " << pair.first << ": " << pair.second << " instances" << std::endl;
    }
    
    // 顯示net統計
    int clock_nets = 0;
    std::map<int, int> fanout_distribution;
    
    for (const auto& pair : db.nets) {
        const auto& net = pair.second;
        if (net->is_clock_net) clock_nets++;
        
        int fanout = net->fanout();
        fanout_distribution[fanout]++;
    }
    
    out << "\n=== Net Statistics ===" << std::endl;
    out << "Clock nets: " << clock_nets << std::endl;
    out << "Fanout distribution:" << std::endl;
    for (const auto& pair : fanout_distribution) {
        if (pair.first <= 10 || pair.second > 100) {
            out << "  Fanout " << pair.first << ": " << pair.second << " nets" << std::endl;
        }
    }
    
    // 列出所有instances的詳細信息
    out << "\n=== All Instance Details ===" << std::endl;
    int count = 0;
    for (const auto& pair : db.instances) {
        
        const auto& inst = pair.second;
        out << "\n[" << (count + 1) << "] " << inst->name << std::endl;
        out << "  Cell Type: " << inst->cell_type << std::endl;
        out << "  Module: " << (inst->module_name.empty() ? "UNASSIGNED" : inst->module_name) << std::endl;
        if (inst->cell_template) {
            out << "  Library: " << inst->cell_template->library << std::endl;
            out << "  Is Flip-Flop: " << (inst->is_flip_flop() ? "Yes" : "No") << std::endl;
            if (inst->is_flip_flop()) {
                out << "  Bit Width: " << inst->get_bit_width() << std::endl;
                if (inst->get_bit_width() == 1) {
                    // Single-bit FF: check if it has banking targets (can be banked)
                    bool can_bank = !inst->cell_template->banking_targets.empty();
                    out << "  Can be banked: " << (can_bank ? "Yes" : "No") << std::endl;
                    if (can_bank) {
                        out << "  Banking targets (" << inst->cell_template->banking_targets.size() << "): ";
                        for (size_t i = 0; i < inst->cell_template->banking_targets.size(); i++) {
                            if (i > 0) out << ", ";
                            out << inst->cell_template->banking_targets[i];
                        }
                        out << std::endl;
                    }
                } else {
                    // Multi-bit FF: check if it can be de-banked
                    bool can_debank = (inst->cell_template->single_bit_degenerate != "null");
                    out << "  Can be de-banked: " << (can_debank ? "Yes" : "No") << std::endl;
                    if (can_debank) {
                        out << "  De-banking target: " << inst->cell_template->single_bit_degenerate << std::endl;
                    }
                }
            }
        } else {
            out << "  [WARNING: Not linked to cell template]" << std::endl;
        }
        out << "  Position: (" << inst->position.x << ", " << inst->position.y << ")" << std::endl;
        out << "  Placement Status: ";
        if (inst->placement_status == Instance::PLACED) out << "PLACED";
        else if (inst->placement_status == Instance::FIXED) out << "FIXED";
        else out << "UNPLACED";
        out << std::endl;
        
        out << "  Orientation: ";
        if (inst->orientation == Instance::N) out << "N";
        else if (inst->orientation == Instance::S) out << "S";
        else if (inst->orientation == Instance::E) out << "E";
        else if (inst->orientation == Instance::W) out << "W";
        else if (inst->orientation == Instance::FN) out << "FN";
        else if (inst->orientation == Instance::FS) out << "FS";
        else if (inst->orientation == Instance::FE) out << "FE";
        else if (inst->orientation == Instance::FW) out << "FW";
        out << std::endl;
        
        out << "  Connections: " << inst->connections.size() << std::endl;
        for (const auto& conn : inst->connections) {
            out << "    ." << conn.pin_name << " -> " << conn.net_name << std::endl;
        }
        
        // 如果是flip-flop，顯示pin connection status
        if (inst->is_flip_flop() && !inst->pin_status.empty()) {
            out << "  Pin Connection Status:" << std::endl;
            for (const auto& pin_stat : inst->pin_status) {
                out << "    " << pin_stat.pin_name << ": ";
                switch (pin_stat.status) {
                    case Instance::PinConnectionStatus::CONNECTED:
                        out << "CONNECTED -> " << pin_stat.net_name; break;
                    case Instance::PinConnectionStatus::UNCONNECTED:
                        out << "UNCONNECTED -> " << pin_stat.net_name; break;
                    case Instance::PinConnectionStatus::TIED_TO_GROUND:
                        out << "TIED_TO_GROUND -> " << pin_stat.net_name; break;
                    case Instance::PinConnectionStatus::TIED_TO_POWER:
                        out << "TIED_TO_POWER -> " << pin_stat.net_name; break;
                    case Instance::PinConnectionStatus::MISSING:
                        out << "MISSING (not connected in netlist)"; break;
                }
                out << std::endl;
            }
        }
        
        count++;
    }
    
    // 列出高fanout nets
    out << "\n=== High Fanout Nets (>100 connections) ===" << std::endl;
    std::vector<std::pair<int, std::shared_ptr<Net>>> high_fanout_nets;
    for (const auto& pair : db.nets) {
        if (pair.second->fanout() > 100) {
            high_fanout_nets.emplace_back(pair.second->fanout(), pair.second);
        }
    }
    
    std::sort(high_fanout_nets.begin(), high_fanout_nets.end(), 
            [](const std::pair<int, std::shared_ptr<Net>>& a, const std::pair<int,std::shared_ptr<Net>>& b) {
                return a.first > b.first;
            });
    
    for (size_t i = 0; i < std::min(size_t(10), high_fanout_nets.size()); i++) {
        const auto& net = high_fanout_nets[i].second;
        out << net->name << ": " << net->fanout() << " connections" 
            << (net->is_clock_net ? " [CLOCK]" : "") << std::endl;
    }
    
    // 顯示Layout資訊 (ROW和TRACKS)
    out << "\n=== Layout Information ===" << std::endl;
    out << "Placement Rows: " << db.placement_rows.size() << std::endl;
    if (!db.placement_rows.empty()) {
        out << "Sample rows (first 5):" << std::endl;
        for (size_t i = 0; i < std::min(size_t(5), db.placement_rows.size()); i++) {
            const auto& row = db.placement_rows[i];
            out << "  " << row.name << " @ (" << row.origin.x << ", " << row.origin.y 
                << ") [" << row.num_x << "x" << row.num_y << "] step(" 
                << row.step_x << ", " << row.step_y << ")" << std::endl;
        }
    }
    
    out << "\nRouting Tracks: " << db.tracks.size() << std::endl;
    if (!db.tracks.empty()) {
        // 按layer分組顯示tracks
        std::map<std::string, std::vector<Track>> tracks_by_layer;
        for (const auto& track : db.tracks) {
            tracks_by_layer[track.layer].push_back(track);
        }
        
        for (const auto& layer_tracks : tracks_by_layer) {
            out << "  Layer " << layer_tracks.first << ": " << layer_tracks.second.size() << " tracks" << std::endl;
            for (const auto& track : layer_tracks.second) {
                out << "    " << (track.direction == Track::X ? "X" : "Y") 
                    << " @ " << track.start << " [" << track.num << " tracks, step " 
                    << track.step << "]" << std::endl;
            }
        }
    }
    
    // 顯示Objective Function資訊
    out << "\n=== Objective Function ===" << std::endl;
    out << "Weights: α=" << db.objective_weights.alpha 
        << ", β=" << db.objective_weights.beta 
        << ", γ=" << db.objective_weights.gamma << std::endl;
    out << "Formula: " << db.objective_weights.alpha << "*TNS + " 
        << db.objective_weights.beta << "*Power + " 
        << db.objective_weights.gamma << "*Area" << std::endl;
    out << "Initial values:" << std::endl;
    out << "  TNS: " << db.objective_weights.initial_tns << std::endl;
    out << "  Power: " << db.objective_weights.initial_power << std::endl;
    out << "  Area: " << db.objective_weights.initial_area << std::endl;
    
    // 計算初始objective value
    double initial_objective = db.objective_weights.calculate_objective(
        db.objective_weights.initial_tns,
        db.objective_weights.initial_power, 
        db.objective_weights.initial_area);
    out << "Initial objective value: " << initial_objective << std::endl;
    
    // 添加placement blockages詳細資訊
    out << "\n=== Placement Blockages Information ===" << std::endl;
    out << "Total placement blockages: " << db.placement_blockages.size() << std::endl;
    if (!db.placement_blockages.empty()) {
        out << "\nBlockage details:" << std::endl;
        for (size_t i = 0; i < db.placement_blockages.size(); i++) {
            const auto& rect = db.placement_blockages[i];
            out << "  [" << (i+1) << "] RECT (" << rect.x1 << " " << rect.y1 << ") (" 
                << rect.x2 << " " << rect.y2 << ") - Size: " << rect.width() << " x " << rect.height() 
                << " (Area: " << rect.area() << ")" << std::endl;
        }
        
        // 計算total blockage area
        double total_blockage_area = 0.0;
        for (const auto& rect : db.placement_blockages) {
            total_blockage_area += rect.area();
        }
        double die_area = db.die_area.area();
        double blockage_percentage = (die_area > 0) ? (total_blockage_area / die_area * 100.0) : 0.0;
        
        out << "\nBlockage summary:" << std::endl;
        out << "  Total blockage area: " << total_blockage_area << std::endl;
        out << "  Die area: " << die_area << std::endl;
        out << "  Blockage percentage: " << blockage_percentage << "%" << std::endl;
    } else {
        out << "No placement blockages found." << std::endl;
    }
    
    out.close();
    std::cout << "\n📄 Instance validation exported to: " << output_file << std::endl;
}

// =============================================================================
// BANKING LEGALITY CHECK FUNCTIONS
// =============================================================================

bool can_bank_flip_flops(const std::vector<std::shared_ptr<Instance>>& ff_instances, 
                        const DesignDatabase& db) {
    if (ff_instances.size() < 2) {
        return false; // 至少需要2個FF才能banking
    }
    
    // 檢查1: 相同時鐘域
    if (!is_same_clock_domain(ff_instances, db)) {
        return false;
    }
    
    // 檢查2: 掃描鏈相容性
    if (!check_scan_chain_compatibility(ff_instances, db)) {
        return false;
    }
    
    // 檢查3: 所有FF必須是可banking的類型
    for (const auto& ff : ff_instances) {
        if (!ff->can_be_banked()) {
            return false;
        }
    }
    
    // 檢查4: 檢查是否有合適的target multi-bit FF
    // 這裡簡化處理，實際上需要檢查library中是否有對應的multi-bit FF
    
    return true;
}

bool is_same_clock_domain(const std::vector<std::shared_ptr<Instance>>& ff_instances, 
                         const DesignDatabase& db) {
    if (ff_instances.empty()) return false;
    
    std::string reference_clock_net;
    bool first = true;
    
    for (const auto& ff : ff_instances) {
        // 找到時鐘pin的連接
        std::string clock_net;
        for (const auto& conn : ff->connections) {
            // 檢查是否是時鐘pin (CK, CLK等)
            if (conn.pin_name == "CK" || conn.pin_name == "CLK" || 
                conn.pin_name == "C" || conn.pin_name == "CP") {
                clock_net = conn.net_name;
                break;
            }
        }
        
        if (clock_net.empty()) {
            return false; // FF沒有時鐘連接
        }
        
        if (first) {
            reference_clock_net = clock_net;
            first = false;
        } else {
            if (clock_net != reference_clock_net) {
                return false; // 不同的時鐘域
            }
        }
    }
    
    return true;
}

bool check_scan_chain_compatibility(const std::vector<std::shared_ptr<Instance>>& ff_instances, 
                                   const DesignDatabase& db) {
    // 檢查掃描鏈相容性
    // 如果FF在掃描鏈中，banking後必須保持掃描鏈的連續性
    
    std::vector<std::string> ff_names;
    for (const auto& ff : ff_instances) {
        ff_names.push_back(ff->name);
    }
    
    // 檢查每個掃描鏈
    for (const auto& scan_chain : db.scan_chains) {
        std::vector<int> ff_positions_in_chain;
        
        // 找到這些FF在掃描鏈中的位置
        for (size_t i = 0; i < scan_chain.chain_sequence.size(); i++) {
            const auto& chain_inst = scan_chain.chain_sequence[i];
            for (const auto& ff_name : ff_names) {
                if (chain_inst.instance_name == ff_name) {
                    ff_positions_in_chain.push_back(i);
                    break;
                }
            }
        }
        
        // 如果有FF在這個掃描鏈中
        if (!ff_positions_in_chain.empty()) {
            // 檢查它們是否是連續的
            std::sort(ff_positions_in_chain.begin(), ff_positions_in_chain.end());
            for (size_t i = 1; i < ff_positions_in_chain.size(); i++) {
                if (ff_positions_in_chain[i] != ff_positions_in_chain[i-1] + 1) {
                    return false; // 不連續，不能banking
                }
            }
        }
    }
    
    return true;
}

double calculate_manhattan_distance(const Instance& ff1, const Instance& ff2) {
    double dx = std::abs(ff1.position.x - ff2.position.x);
    double dy = std::abs(ff1.position.y - ff2.position.y);
    return dx + dy;
}

// =============================================================================
// FF COMPATIBILITY CHECKING FUNCTIONS
// =============================================================================

bool check_ff_compatibility(const std::shared_ptr<Instance>& ff1, 
                            const std::shared_ptr<Instance>& ff2,
                            const DesignDatabase& db) {
    // 檢查1: Clock edge compatibility
    if (!check_clock_edge_compatibility(ff1, ff2, db)) {
        return false;
    }
    
    // 檢查2: Pin interface compatibility
    if (!check_pin_interface_compatibility(ff1, ff2, db)) {
        return false;
    }
    
    // 檢查3: Connection status compatibility
    if (!check_connection_status_compatibility(ff1, ff2, db)) {
        return false;
    }
    
    // 檢查4: Same clock domain (已存在的檢查)
    std::vector<std::shared_ptr<Instance>> temp_vec = {ff1, ff2};
    if (!is_same_clock_domain(temp_vec, db)) {
        return false;
    }
    
    return true;
}

bool check_clock_edge_compatibility(const std::shared_ptr<Instance>& ff1,
                                   const std::shared_ptr<Instance>& ff2,
                                   const DesignDatabase& db) {
    // 取得兩個FF的cell template
    auto cell1_it = db.cell_library.find(ff1->cell_type);
    auto cell2_it = db.cell_library.find(ff2->cell_type);
    
    if (cell1_it == db.cell_library.end() || cell2_it == db.cell_library.end()) {
        return false; // Cell template not found
    }
    
    // 比較clock edge
    CellTemplate::ClockEdge edge1 = cell1_it->second->clock_edge;
    CellTemplate::ClockEdge edge2 = cell2_it->second->clock_edge;
    
    // 兩個FF必須有相同的clock edge才能banking
    return edge1 == edge2 && edge1 != CellTemplate::UNKNOWN_EDGE;
}

bool check_pin_interface_compatibility(const std::shared_ptr<Instance>& ff1,
                                      const std::shared_ptr<Instance>& ff2,
                                      const DesignDatabase& db) {
    // 取得兩個FF的cell template
    auto cell1_it = db.cell_library.find(ff1->cell_type);
    auto cell2_it = db.cell_library.find(ff2->cell_type);
    
    if (cell1_it == db.cell_library.end() || cell2_it == db.cell_library.end()) {
        return false;
    }
    
    const auto& template1 = cell1_it->second;
    const auto& template2 = cell2_it->second;
    
    // 比較所有pin types (除了VDD/VSS)
    std::set<Pin::FlipFlopPinType> pins1, pins2;
    
    // 收集FF1的pin types
    for (const auto& pin : template1->pins) {
        Pin::FlipFlopPinType type = classify_ff_pin_type(pin.name);
        if (type != Pin::FF_VDDR && type != Pin::FF_OTHER) {
            pins1.insert(type);
        }
    }
    
    // 收集FF2的pin types
    for (const auto& pin : template2->pins) {
        Pin::FlipFlopPinType type = classify_ff_pin_type(pin.name);
        if (type != Pin::FF_VDDR && type != Pin::FF_OTHER) {
            pins2.insert(type);
        }
    }
    
    // Pin interface必須完全相同才能banking
    return pins1 == pins2;
}

bool check_connection_status_compatibility(const std::shared_ptr<Instance>& ff1,
                                          const std::shared_ptr<Instance>& ff2,
                                          const DesignDatabase& db) {
    // 檢查連線狀態相容性
    // 如果某個pin沒有連線(UNCONNECTED)或連到VSS/VDD，可以視為"沒有這個pin"
    // 特別是SI/SE連接到VSS/VDD表示scan功能被禁用，等同於沒有這些pins
    
    std::set<Pin::FlipFlopPinType> active_pins1, active_pins2;
    
    // 分析FF1的active pins (有實際邏輯連線的pins)
    for (const auto& conn : ff1->connections) {
        Pin::FlipFlopPinType type = classify_ff_pin_type(conn.pin_name);
        if (type != Pin::FF_VDDR && type != Pin::FF_OTHER) {
            // 檢查是否為有效的邏輯連線
            if (is_active_logical_connection(conn, type)) {
                active_pins1.insert(type);
            }
        }
    }
    
    // 分析FF2的active pins
    for (const auto& conn : ff2->connections) {
        Pin::FlipFlopPinType type = classify_ff_pin_type(conn.pin_name);
        if (type != Pin::FF_VDDR && type != Pin::FF_OTHER) {
            if (is_active_logical_connection(conn, type)) {
                active_pins2.insert(type);
            }
        }
    }
    
    // Active pins必須相同
    return active_pins1 == active_pins2;
}

std::vector<std::vector<std::shared_ptr<Instance>>> 
group_compatible_flip_flops(const DesignDatabase& db) {
    std::vector<std::vector<std::shared_ptr<Instance>>> compatibility_groups;
    std::vector<std::shared_ptr<Instance>> all_ffs;
    
    // 收集所有FF instances
    for (const auto& inst_pair : db.instances) {
        const auto& instance = inst_pair.second;
        if (instance->is_flip_flop()) {
            all_ffs.push_back(instance);
        }
    }
    
    // 使用Union-Find或簡單的群組演算法
    std::vector<bool> assigned(all_ffs.size(), false);
    
    for (size_t i = 0; i < all_ffs.size(); i++) {
        if (assigned[i]) continue;
        
        // 建立新群組
        std::vector<std::shared_ptr<Instance>> current_group;
        current_group.push_back(all_ffs[i]);
        assigned[i] = true;
        
        // 找所有與current_group中任何FF相容的FF
        for (size_t j = i + 1; j < all_ffs.size(); j++) {
            if (assigned[j]) continue;
            
            bool compatible_with_group = true;
            for (const auto& group_ff : current_group) {
                if (!check_ff_compatibility(all_ffs[j], group_ff, db)) {
                    compatible_with_group = false;
                    break;
                }
            }
            
            if (compatible_with_group) {
                current_group.push_back(all_ffs[j]);
                assigned[j] = true;
            }
        }
        
        compatibility_groups.push_back(current_group);
    }
    
    return compatibility_groups;
}

// =============================================================================
// PIN TYPE CLASSIFICATION
// =============================================================================

Pin::FlipFlopPinType classify_ff_pin_type(const std::string& pin_name) {
    std::string upper_pin = pin_name;
    // 轉換為大寫進行比較
    std::transform(upper_pin.begin(), upper_pin.end(), upper_pin.begin(), ::toupper);
    
    // 數據輸入 (D pins)
    if (upper_pin == "D" || upper_pin == "D0" || upper_pin == "D1" || upper_pin == "D2" || upper_pin == "D3" ||
        upper_pin == "D4" || upper_pin == "D5" || upper_pin == "D6" || upper_pin == "D7" ||
        upper_pin.find("D[") == 0) {  // D[0], D[1], etc.
        return Pin::FF_DATA_INPUT;
    }
    
    // 數據輸出反相 (QN pins - 優先檢查)
    if (upper_pin == "QN" || upper_pin == "QN0" || upper_pin == "QN1" || upper_pin == "QN2" || 
        upper_pin == "QN3" || upper_pin == "QN4" || upper_pin == "QN5" || upper_pin == "QN6" || 
        upper_pin == "QN7" || upper_pin.find("QN[") == 0) {
        return Pin::FF_DATA_OUTPUT_N;
    }
    
    // 數據輸出 (Q pins)  
    if (upper_pin == "Q" || upper_pin == "Q0" || upper_pin == "Q1" || upper_pin == "Q2" || upper_pin == "Q3" ||
        upper_pin == "Q4" || upper_pin == "Q5" || upper_pin == "Q6" || upper_pin == "Q7" ||
        upper_pin.find("Q[") == 0) {
        return Pin::FF_DATA_OUTPUT;
    }
    
    // 時鐘 (Clock pins)
    if (upper_pin == "CLK" || upper_pin == "CK" || upper_pin == "CLOCK" || upper_pin == "CP") {
        return Pin::FF_CLOCK;
    }
    
    // 掃描輸入 (Scan Input)
    if (upper_pin == "SI" || upper_pin == "SCAN_IN" || upper_pin == "SCIN" || upper_pin == "TI") {
        return Pin::FF_SCAN_INPUT;
    }
    
    // 掃描輸出 (Scan Output)
    if (upper_pin == "SO" || upper_pin == "SCAN_OUT" || upper_pin == "SCOUT" || upper_pin == "TO") {
        return Pin::FF_SCAN_OUTPUT;
    }
    
    // 掃描使能 (Scan Enable)
    if (upper_pin == "SE" || upper_pin == "SCAN_EN" || upper_pin == "SCAN_ENABLE" || upper_pin == "TE") {
        return Pin::FF_SCAN_ENABLE;
    }
    
    // 特殊的具體pin名稱 - 直接用pin名稱分類
    if (upper_pin == "RD") {
        return Pin::FF_RD;
    }
    
    if (upper_pin == "SD") {
        return Pin::FF_SD;
    }
    
    if (upper_pin == "SR") {
        return Pin::FF_SR;
    }
    
    if (upper_pin == "RS") {
        return Pin::FF_RS;
    }
    
    if (upper_pin == "VDDR") {
        return Pin::FF_VDDR;
    }
    
    // 重置 (Reset pins) - 一般重置pin
    if (upper_pin == "R" || upper_pin == "RST" || upper_pin == "RESET" || upper_pin == "RN" || 
        upper_pin == "RESETN" || upper_pin == "RSTB" || upper_pin == "CDN" ||
        upper_pin == "RSTN" || upper_pin == "CLR" || upper_pin == "CLRN") {
        return Pin::FF_RESET;
    }
    
    // 設置 (Set pins) - 一般設置pin  
    if (upper_pin == "S" || upper_pin == "SET" || upper_pin == "SN" || upper_pin == "SETN" || 
        upper_pin == "SETB" || upper_pin == "SDN" || 
        upper_pin == "PRE" || upper_pin == "PREN" || upper_pin == "PRESET") {
        return Pin::FF_SET;
    }
    
    // 電源相關 - 一般電源pin (不包括VDDR，它已經被特別分類)
    if (upper_pin == "VDD" || upper_pin == "VSS" || upper_pin == "VDDPE" || upper_pin == "VSSE" ||
        upper_pin == "VNW" || upper_pin == "VPW" || upper_pin == "VSDR" ||
        upper_pin == "AVDD" || upper_pin == "AVSS" || upper_pin == "DVDD" || upper_pin == "DVSS") {
        return Pin::FF_NOT_FF_PIN;  // 電源pin不算flip-flop功能pin
    }
    
    // 其他可能的flip-flop pins
    return Pin::FF_OTHER;
}

// =============================================================================  
// FF PIN CONNECTION ANALYSIS FUNCTIONS
// =============================================================================

bool is_power_net(const std::string& net_name) {
    std::string upper_net = net_name;
    std::transform(upper_net.begin(), upper_net.end(), upper_net.begin(), ::toupper);
    return (upper_net == "VDD" || upper_net == "VCC" || upper_net == "VDDPE" || 
            upper_net == "VDDR" || upper_net == "AVDD" || upper_net == "DVDD");
}

bool is_ground_net(const std::string& net_name) {
    std::string upper_net = net_name;
    std::transform(upper_net.begin(), upper_net.end(), upper_net.begin(), ::toupper);
    return (upper_net == "VSS" || upper_net == "GND" || upper_net == "VSSE" || 
            upper_net == "AVSS" || upper_net == "DVSS");
}

bool is_unconnected_net(const std::string& net_name) {
    return net_name.find("SYNOPSYS_UNCONNECTED") != std::string::npos;
}

// 判斷某個pin連接是否為有效的邏輯連線
bool is_active_logical_connection(const Instance::Connection& conn, Pin::FlipFlopPinType pin_type) {
    const std::string& net_name = conn.net_name;
    
    // 基本的非連接檢查
    if (net_name == "UNCONNECTED" || net_name.empty()) {
        return false;
    }
    
    // 檢查 SYNOPSYS_UNCONNECTED patterns
    if (net_name.find("SYNOPSYS_UNCONNECTED") != std::string::npos) {
        return false;
    }
    
    // 如果pin連接到VSS (ground)，視為"沒有這個pin"
    // 這包括所有類型的pins：SI, SE, S, RD, SD, QN, Q等
    // 連接到VSS表示該pin功能被禁用或不使用
    if (net_name == "VSS") {
        return false;  // 任何pin連到VSS = 沒有這個pin
    }
    
    // 連接到VDD的pins仍然算是有效連接
    // （某些control pins可能需要tied high來啟用功能）
    return true;
}

// Helper function to convert Instance::Orientation enum to string
std::string orientation_to_string(Instance::Orientation orientation) {
    switch (orientation) {
        case Instance::N:  return "N";
        case Instance::S:  return "S";
        case Instance::E:  return "E";
        case Instance::W:  return "W";
        case Instance::FN: return "FN";
        case Instance::FS: return "FS";
        case Instance::FE: return "FE";
        case Instance::FW: return "FW";
        default: return "N";
    }
}

void analyze_ff_pin_connections(DesignDatabase& db) {
    std::cout << "  Analyzing FF pin connection status..." << std::endl;
    
    int analyzed_ff_count = 0;
    std::map<std::string, int> pin_status_stats;
    
    for (auto& inst_pair : db.instances) {
        auto& instance = inst_pair.second;
        
        // 只分析flip-flop instances
        if (!instance->is_flip_flop() || !instance->cell_template) {
            continue;
        }
        
        instance->pin_status.clear();
        
        // 檢查cell template中的每個pin
        for (const auto& template_pin : instance->cell_template->pins) {
            // 跳過電源pins，因為它們不影響兼容性
            if (template_pin.ff_pin_type == Pin::FF_NOT_FF_PIN) {
                continue;
            }
            
            // 在instance連線中查找這個pin
            auto conn = instance->find_connection(template_pin.name);
            
            Instance::PinConnectionStatus::Status status;
            std::string connected_net = "";
            
            if (conn == nullptr) {
                // Pin在verilog中沒有連線，視為missing
                status = Instance::PinConnectionStatus::MISSING;
            } else {
                connected_net = conn->net_name;
                
                if (is_unconnected_net(connected_net)) {
                    status = Instance::PinConnectionStatus::UNCONNECTED;
                } else if (is_ground_net(connected_net)) {
                    status = Instance::PinConnectionStatus::TIED_TO_GROUND;
                } else if (is_power_net(connected_net)) {
                    status = Instance::PinConnectionStatus::TIED_TO_POWER;
                } else {
                    status = Instance::PinConnectionStatus::CONNECTED;
                }
            }
            
            instance->pin_status.emplace_back(template_pin.name, status, connected_net);
            
            // 統計狀態
            std::string status_name;
            switch (status) {
                case Instance::PinConnectionStatus::CONNECTED: status_name = "CONNECTED"; break;
                case Instance::PinConnectionStatus::UNCONNECTED: status_name = "UNCONNECTED"; break;
                case Instance::PinConnectionStatus::TIED_TO_GROUND: status_name = "TIED_TO_GROUND"; break;
                case Instance::PinConnectionStatus::TIED_TO_POWER: status_name = "TIED_TO_POWER"; break;
                case Instance::PinConnectionStatus::MISSING: status_name = "MISSING"; break;
            }
            pin_status_stats[status_name]++;
        }
        
        analyzed_ff_count++;
    }
    
    std::cout << "    Analyzed pin connections for " << analyzed_ff_count << " flip-flops" << std::endl;
    std::cout << "    Pin status distribution:" << std::endl;
    for (const auto& pair : pin_status_stats) {
        std::cout << "      " << pair.first << ": " << pair.second << " pins" << std::endl;
    }
}

// =============================================================================
// SCANDEF PARSER IMPLEMENTATION
// =============================================================================

void parse_scandef_section(std::ifstream& file, DesignDatabase& db) {
    std::string line;
    int parsed_chains = 0;
    
    while (std::getline(file, line)) {
        line = trim_whitespace(line);
        
        // 檢查SCANCHAINS結束
        if (line == "END SCANCHAINS") {
            std::cout << "    Finished SCANCHAINS section" << std::endl;
            break;
        }
        
        // 解析單個scan chain定義
        // 格式：- CHAIN_NAME START PIN_NAME STOP PIN_NAME
        //      ( INST_NAME ( SI PIN_NAME ) ( SO PIN_NAME ) )
        //      ( INST_NAME ( SI PIN_NAME ) ( SO PIN_NAME ) ) ...
        //      ;
        if (line.find("- ") == 0) {
            ScanChain chain;
            
            // 解析chain header: "- CHAIN_NAME START PIN_NAME STOP PIN_NAME"
            std::istringstream iss(line);
            std::string token;
            iss >> token; // "-"
            iss >> chain.name; // chain name
            iss >> token; // "START"
            iss >> chain.scan_in_pin; // input pin
            iss >> token; // "STOP"
            iss >> chain.scan_out_pin; // output pin
            
            // 讀取chain中的instance序列
            bool in_chain_definition = true;
            while (in_chain_definition && std::getline(file, line)) {
                line = trim_whitespace(line);
                
                if (line == ";") {
                    in_chain_definition = false;
                    break;
                }
                
                // 解析instance line: "( INST_NAME ( SI PIN_NAME ) ( SO PIN_NAME ) )"
                if (line.find("( ") == 0) {
                    std::istringstream inst_iss(line);
                    std::string paren, inst_name, si_pin, so_pin;
                    
                    inst_iss >> paren; // "("
                    inst_iss >> inst_name; // instance name
                    inst_iss >> paren; // "("
                    inst_iss >> token; // "SI"
                    inst_iss >> si_pin; // SI pin name
                    inst_iss >> paren; // ")"
                    inst_iss >> paren; // "("
                    inst_iss >> token; // "SO"
                    inst_iss >> so_pin; // SO pin name
                    inst_iss >> paren; // ")"
                    inst_iss >> paren; // ")"
                    
                    // 加入到scan chain
                    chain.chain_sequence.emplace_back(inst_name, si_pin, so_pin);
                }
            }
            
            // 加入到資料庫
            db.scan_chains.push_back(chain);
            parsed_chains++;
            
            std::cout << "      Parsed scan chain: " << chain.name 
                      << " (length: " << chain.length() << ")" << std::endl;
        }
    }
    
    std::cout << "    Parsed " << parsed_chains << " scan chains total" << std::endl;
}

// =============================================================================
// FF CELL COMPATIBILITY GROUPING
// =============================================================================

void build_ff_cell_compatibility_groups(DesignDatabase& db) {
    std::cout << "  Building FF cell compatibility groups..." << std::endl;
    
    db.ff_compatibility_groups.clear();
    
    // 用來將pin interface轉換為signature string的函數
    auto get_pin_signature = [](const std::shared_ptr<CellTemplate>& cell) -> std::string {
        if (cell->type != CellTemplate::FLIP_FLOP) return "";
        
        std::set<Pin::FlipFlopPinType> pin_types;
        
        // 收集所有非電源pins
        for (const auto& pin : cell->pins) {
            Pin::FlipFlopPinType type = classify_ff_pin_type(pin.name);
            if (type != Pin::FF_VDDR && type != Pin::FF_OTHER) {
                pin_types.insert(type);
            }
        }
        
        // 生成排序的signature string
        std::string signature;
        for (auto type : pin_types) {
            if (!signature.empty()) signature += "_";
            
            switch (type) {
                case Pin::FF_DATA_INPUT: signature += "D"; break;
                case Pin::FF_DATA_OUTPUT: signature += "Q"; break;
                case Pin::FF_DATA_OUTPUT_N: signature += "QN"; break;
                case Pin::FF_CLOCK: signature += "CK"; break;
                case Pin::FF_SCAN_INPUT: signature += "SI"; break;
                case Pin::FF_SCAN_OUTPUT: signature += "SO"; break;
                case Pin::FF_SCAN_ENABLE: signature += "SE"; break;
                case Pin::FF_RESET: signature += "R"; break;
                case Pin::FF_SET: signature += "S"; break;
                case Pin::FF_RD: signature += "RD"; break;
                case Pin::FF_SD: signature += "SD"; break;
                default: break;
            }
        }
        
        // 加入clock edge信息
        signature += "_" + cell->get_clock_edge_string();
        
        return signature;
    };
    
    // 按pin signature分組所有FF cells
    std::unordered_map<std::string, std::vector<std::string>> signature_groups;
    int ff_cell_count = 0;
    
    for (const auto& cell_pair : db.cell_library) {
        const auto& cell = cell_pair.second;
        
        if (cell->type == CellTemplate::FLIP_FLOP) {
            std::string signature = get_pin_signature(cell);
            if (!signature.empty()) {
                signature_groups[signature].push_back(cell->name);
                ff_cell_count++;
                
                // Debug: Print first few cells with their signatures
                if (ff_cell_count <= 5) {
                    std::cout << "    DEBUG: " << cell->name << " -> signature: [" << signature << "]" << std::endl;
                    std::cout << "      Pins (" << cell->pins.size() << "): ";
                    for (const auto& pin : cell->pins) {
                        Pin::FlipFlopPinType type = classify_ff_pin_type(pin.name);
                        std::cout << pin.name << "(" << static_cast<int>(type) << ") ";
                    }
                    std::cout << std::endl;
                    std::cout << "      Clock edge: " << cell->get_clock_edge_string() << std::endl;
                }
            }
        }
    }
    
    // 將分組結果存入database
    db.ff_compatibility_groups = signature_groups;
    
    std::cout << "    Processed " << ff_cell_count << " FF cells into " 
              << signature_groups.size() << " compatibility groups:" << std::endl;
    
    // 顯示分組統計
    for (const auto& group_pair : signature_groups) {
        const std::string& signature = group_pair.first;
        const auto& cells = group_pair.second;
        
        std::cout << "      " << signature << ": " << cells.size() << " cells (";
        for (size_t i = 0; i < std::min(cells.size(), size_t(3)); i++) {
            if (i > 0) std::cout << ", ";
            std::cout << cells[i];
        }
        if (cells.size() > 3) std::cout << "...";
        std::cout << ")" << std::endl;
    }
}