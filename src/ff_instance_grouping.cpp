#include "parsers.hpp"
#include "timing_repr_hardcoded.hpp"
#include <iostream>
#include <set>
#include <unordered_set>
#include <sstream>
#include <fstream>
#include <limits>
#include <iomanip>

// =============================================================================
// FF INSTANCE GROUPING FOR SUBSTITUTION OPTIMIZATION
// =============================================================================

// Helper function to extract hierarchy from instance name
// Cross-hierarchy banking is prohibited according to Q70 in Problem B_QA_0812.pdf
std::string get_instance_hierarchy(std::shared_ptr<Instance> instance) {
    // Use the parsed module_name field for more accurate hierarchy
    if (!instance->module_name.empty()) {
        return instance->module_name;
    }
    
    // Fallback to parsing instance name if module_name is not set
    std::string name = instance->name;
    
    // Check if this is a hierarchical instance name (contains '/')
    // DEF uses '/' as hierarchy separator, e.g., "hier_top_mod_5/hier_top_mod_4/mid11__372"
    size_t last_slash_pos = name.find_last_of('/');
    if (last_slash_pos != std::string::npos) {
        // Extract the hierarchy path (everything before the last '/')
        return name.substr(0, last_slash_pos);
    }
    
    // If no '/' found, this is a top-level instance (flat design)
    return "TOP_LEVEL";
}

// Removed: pin signature grouping - now handled by three-stage substitution

// Get the clock edge type for an instance
std::string get_instance_clock_edge(std::shared_ptr<Instance> instance, const DesignDatabase& db) {
    if (!instance->cell_template) {
        return "UNKNOWN";
    }
    
    // 簡化版本：直接基於cell name判斷clock edge
    // 更精確的版本需要解析liberty file的clocked_on attribute
    
    // 特殊處理：檢查cell name pattern判斷clock edge
    std::string cell_name = instance->cell_template->name;
    if (cell_name.find("FDN") != std::string::npos || cell_name.find("FSDN") != std::string::npos) {
        return "FALLING";
    } else if (cell_name.find("FDP") != std::string::npos || cell_name.find("FSDP") != std::string::npos) {
        return "RISING";
    }
    
    return "UNKNOWN";
}

// Get the scan chain ID for an instance (if any)
std::string get_instance_scan_chain(std::shared_ptr<Instance> instance, const DesignDatabase& db) {
    // 檢查是否有SI/SE connections來判斷是否在scan chain中
    // Note: In this testcase, there are no SO pins, only SI and SE
    std::string si_net = "";
    std::string se_net = "";
    bool si_connected = false;
    bool se_connected = false;
    
    for (const auto& conn : instance->connections) {
        Pin::FlipFlopPinType pin_type = classify_ff_pin_type(conn.pin_name);
        if (pin_type == Pin::FF_SCAN_INPUT) {
            si_net = conn.net_name;
            if (conn.net_name != "UNCONNECTED" && 
                conn.net_name.find("SYNOPSYS_UNCONNECTED") == std::string::npos) {
                si_connected = true;
            }
        }
        if (pin_type == Pin::FF_SCAN_ENABLE) {
            se_net = conn.net_name;
            if (conn.net_name != "UNCONNECTED" && 
                conn.net_name.find("SYNOPSYS_UNCONNECTED") == std::string::npos) {
                se_connected = true;
            }
        }
    }
    
    // 如果有active SI或SE connection，則需要找到這個FF屬於哪個scan chain
    if (si_connected || se_connected) {
        // 查找這個instance屬於哪個scan chain
        for (size_t chain_idx = 0; chain_idx < db.scan_chains.size(); chain_idx++) {
            const auto& chain = db.scan_chains[chain_idx];
            for (const auto& scan_conn : chain.chain_sequence) {
                if (scan_conn.instance_name == instance->name) {
                    return chain.name; // 返回scan chain名稱
                }
            }
        }
        // 如果沒找到對應的scan chain，可能是scan chain detection的問題
        return "UNASSIGNED_SCAN";
    }
    
    // For instances without scan connections
    return "NON_SCAN";
}

// Get clock domain for an instance (simplified)
std::string get_instance_clock_domain(std::shared_ptr<Instance> instance, const DesignDatabase& db) {
    // 尋找clock connection
    for (const auto& conn : instance->connections) {
        Pin::FlipFlopPinType pin_type = classify_ff_pin_type(conn.pin_name);
        if (pin_type == Pin::FF_CLOCK && is_active_logical_connection(conn, pin_type)) {
            return conn.net_name;
        }
    }
    
    return "UNKNOWN_CLOCK";
}

// Generate a simplified group key for an instance
// Based on scan chain, hierarchy, and clock domain (pin patterns handled by three-stage substitution)
// Cross-hierarchy banking is prohibited according to Q70 in Problem B_QA_0812.pdf
// Cross-clock banking is prohibited for timing reasons
std::string generate_instance_group_key(std::shared_ptr<Instance> instance, const DesignDatabase& db) {
    std::string scan_chain = get_instance_scan_chain(instance, db);
    std::string hierarchy = get_instance_hierarchy(instance); // Add hierarchy constraint
    std::string clock_domain = get_instance_clock_domain(instance, db); // Add clock domain constraint
    
    std::ostringstream oss;
    oss << scan_chain << "|" << hierarchy << "|" << clock_domain;
    return oss.str();
}

// Main function to group FF instances
void group_ff_instances(DesignDatabase& db) {
    std::cout << "  Grouping FF instances for substitution optimization..." << std::endl;
    
    db.ff_instance_groups.clear();
    
    // 收集所有FF instances
    std::vector<std::shared_ptr<Instance>> ff_instances;
    for (const auto& inst_pair : db.instances) {
        auto& instance = inst_pair.second;
        if (instance->is_flip_flop()) {
            ff_instances.push_back(instance);
        }
    }
    
    std::cout << "    Found " << ff_instances.size() << " FF instances to group" << std::endl;
    
    // 根據group key分組
    for (auto& instance : ff_instances) {
        std::string group_key = generate_instance_group_key(instance, db);
        db.ff_instance_groups[group_key].push_back(instance);
    }
    
    std::cout << "    Created " << db.ff_instance_groups.size() << " instance groups" << std::endl;
    
    // 顯示分組統計
    int total_grouped_instances = 0;
    for (const auto& group_pair : db.ff_instance_groups) {
        total_grouped_instances += group_pair.second.size();
        if (group_pair.second.size() > 1) {
            std::cout << "      Group [" << group_pair.first << "]: " 
                      << group_pair.second.size() << " instances" << std::endl;
        }
    }
    
    std::cout << "    Total instances grouped: " << total_grouped_instances << std::endl;
}

// Helper function to get clock signal name from instance
std::string get_instance_clock_signal(std::shared_ptr<Instance> instance) {
    // Find the actual clock net name connected to CK pin
    for (const auto& conn : instance->connections) {
        Pin::FlipFlopPinType pin_type = classify_ff_pin_type(conn.pin_name);
        if (pin_type == Pin::FF_CLOCK) {
            if (conn.net_name != "UNCONNECTED" && 
                conn.net_name.find("SYNOPSYS_UNCONNECTED") == std::string::npos &&
                conn.net_name != "VSS" && 
                conn.net_name != "VDD") {
                return conn.net_name;
            }
        }
    }
    return "UNKNOWN_CLK";
}

// Rebuild FF instance groups for banking based on hierarchy + clock signal
void rebuild_ff_instance_groups_for_banking(DesignDatabase& db) {
    std::cout << "  Rebuilding FF instance groups for banking..." << std::endl;
    std::cout << "  Grouping strategy: hierarchy + clock signal (no scan chain)" << std::endl;
    
    // Clear existing groups
    db.ff_instance_groups.clear();
    
    // Collect all FF instances from db.instances
    std::vector<std::shared_ptr<Instance>> ff_instances;
    for (const auto& inst_pair : db.instances) {
        auto& instance = inst_pair.second;
        if (instance->is_flip_flop()) {
            ff_instances.push_back(instance);
        }
    }
    
    std::cout << "    Found " << ff_instances.size() << " FF instances to group" << std::endl;
    
    // Group by hierarchy + clock signal
    for (auto& instance : ff_instances) {
        std::string hierarchy = get_instance_hierarchy(instance);
        std::string clock_signal = get_instance_clock_signal(instance);
        
        // Create group key: hierarchy|clock_signal
        std::string group_key = hierarchy + "|" + clock_signal;
        
        db.ff_instance_groups[group_key].push_back(instance);
    }
    
    std::cout << "    Created " << db.ff_instance_groups.size() << " instance groups" << std::endl;
    
    // Display group statistics
    int total_grouped_instances = 0;
    for (const auto& group_pair : db.ff_instance_groups) {
        total_grouped_instances += group_pair.second.size();
        if (group_pair.second.size() > 1) {
            std::cout << "      Group [" << group_pair.first << "]: " 
                      << group_pair.second.size() << " instances" << std::endl;
        }
    }
    
    std::cout << "    Total instances grouped: " << total_grouped_instances << std::endl;
    
    // Export detailed FF instance groups report
    //export_ff_instance_groups_detailed_report(db, "ff_instance_groups_step16_5.txt");
}

// Export detailed FF instance groups report for Step 16.5
void export_ff_instance_groups_detailed_report(const DesignDatabase& db, const std::string& output_file) {
    std::cout << "  Exporting detailed FF instance groups report to: " << output_file << std::endl;
    
    std::ofstream out(output_file);
    if (!out.is_open()) {
        std::cerr << "Error: Cannot open " << output_file << " for writing" << std::endl;
        return;
    }
    
    out << "=== FF INSTANCE GROUPS REPORT (Step 16.5) ===" << std::endl;
    out << "Generated: " << __DATE__ << " " << __TIME__ << std::endl;
    out << "Grouping strategy: hierarchy + clock signal" << std::endl;
    out << std::endl;
    
    out << "=== SUMMARY ===" << std::endl;
    out << "Total groups: " << db.ff_instance_groups.size() << std::endl;
    
    int total_instances = 0;
    int single_instance_groups = 0;
    int multi_instance_groups = 0;
    
    for (const auto& group_pair : db.ff_instance_groups) {
        total_instances += group_pair.second.size();
        if (group_pair.second.size() == 1) {
            single_instance_groups++;
        } else {
            multi_instance_groups++;
        }
    }
    
    out << "Total FF instances: " << total_instances << std::endl;
    out << "Single-instance groups: " << single_instance_groups << std::endl;
    out << "Multi-instance groups: " << multi_instance_groups << std::endl;
    out << std::endl;
    
    // Detailed group information
    out << "=== DETAILED GROUP INFORMATION ===" << std::endl;
    out << std::endl;
    
    int group_num = 1;
    for (const auto& group_pair : db.ff_instance_groups) {
        const std::string& group_key = group_pair.first;
        const auto& instances = group_pair.second;
        
        out << "Group " << group_num++ << ": [" << group_key << "]" << std::endl;
        out << "  Instance count: " << instances.size() << std::endl;
        
        // Count by banking type
        std::map<BankingType, int> banking_type_counts;
        for (const auto& instance : instances) {
            banking_type_counts[instance->banking_type]++;
        }
        
        out << "  Banking type distribution:" << std::endl;
        out << "    FSDN: " << banking_type_counts[BankingType::FSDN] << std::endl;
        out << "    RISING_LSRDPQ: " << banking_type_counts[BankingType::RISING_LSRDPQ] << std::endl;
        out << "    NONE: " << banking_type_counts[BankingType::NONE] << std::endl;
        
        // List all instances in this group
        out << "  All instances in this group:" << std::endl;
        for (size_t i = 0; i < instances.size(); i++) {
            auto& instance = instances[i];
            out << "    " << (i+1) << ". " << instance->name;
            out << " (" << instance->cell_template->name << ")";
            
            // Show banking type
            std::string banking_type_str;
            switch(instance->banking_type) {
                case BankingType::FSDN: banking_type_str = "FSDN"; break;
                case BankingType::RISING_LSRDPQ: banking_type_str = "RISING_LSRDPQ"; break;
                case BankingType::NONE: banking_type_str = "NONE"; break;
            }
            out << " [" << banking_type_str << "]";
            
            // Show position
            out << " @(" << instance->position.x << "," << instance->position.y << ")";
            out << std::endl;
        }
        out << std::endl;
    }
    
    out.close();
    std::cout << "    Detailed FF instance groups report exported successfully" << std::endl;
}

// Export FF instance grouping report
void export_ff_instance_grouping_report(const DesignDatabase& db, const std::string& output_file) {
    std::cout << "  Exporting FF instance grouping report to: " << output_file << std::endl;
    
    std::ofstream out(output_file);
    if (!out.is_open()) {
        std::cerr << "Error: Cannot open " << output_file << " for writing" << std::endl;
        return;
    }
    
    out << "=== FF INSTANCE GROUPING REPORT ===" << std::endl;
    out << "Generated: " << __DATE__ << " " << __TIME__ << std::endl;
    out << std::endl;
    
    out << "=== GROUPING SUMMARY ===" << std::endl;
    out << "Total FF instance groups: " << db.ff_instance_groups.size() << std::endl;
    
    int total_instances = 0;
    int single_instance_groups = 0;
    int multi_instance_groups = 0;
    
    for (const auto& group_pair : db.ff_instance_groups) {
        total_instances += group_pair.second.size();
        if (group_pair.second.size() == 1) {
            single_instance_groups++;
        } else {
            multi_instance_groups++;
        }
    }
    
    out << "Total FF instances: " << total_instances << std::endl;
    out << "Single-instance groups: " << single_instance_groups << std::endl;
    out << "Multi-instance groups: " << multi_instance_groups << std::endl;
    out << std::endl;
    
    // 詳細分組資訊
    out << "=== DETAILED GROUP INFORMATION ===" << std::endl;
    
    int group_num = 1;
    for (const auto& group_pair : db.ff_instance_groups) {
        const std::string& group_key = group_pair.first;
        const auto& instances = group_pair.second;
        
        out << std::endl;
        out << "Group " << group_num++ << ": [" << group_key << "]" << std::endl;
        out << "  Instance count: " << instances.size() << std::endl;
        
        if (instances.size() > 0) {
            // 顯示第一個instance的詳細資訊作為代表
            auto representative = instances[0];
            out << "  Representative instance: " << representative->name << std::endl;
            out << "  Current cell type: " << representative->cell_type << std::endl;
            
            // Note: Pin connection details are now handled by three-stage substitution
            out << "  Group constraints: scan chain and hierarchy only" << std::endl;
            
            // 如果group有多個instances，列出所有instance names
            if (instances.size() > 1) {
                out << "  All instances in this group:" << std::endl;
                for (size_t i = 0; i < instances.size(); i++) {
                    out << "    " << (i+1) << ". " << instances[i]->name;
                    out << " (" << instances[i]->cell_type << ")";
                    out << std::endl;
                }
            }
        }
    }
    
    out.close();
    std::cout << "    Instance grouping report exported successfully" << std::endl;
}

// Calculate optimal FF for each hierarchical group using the same formula as preprocessing
void calculate_optimal_ff_for_instance_groups(DesignDatabase& db) {
    std::cout << "  Calculating optimal FF for each compatibility group..." << std::endl;
    std::cout << "  Using scoring formula: Score = (β·Power + γ·Area)/bit + δ, where δ=α·timing_repr" << std::endl;
    
    db.optimal_ff_for_groups.clear();
    
    int total_groups = 0;
    int groups_with_optimal = 0;
    
    // Process hierarchical groups: clock_edge -> pin_interface -> bit_width -> cell_names
    for (const auto& edge_pair : db.hierarchical_ff_groups) {
        const std::string& clock_edge = edge_pair.first;
        
        for (const auto& pin_pair : edge_pair.second) {
            const std::string& pin_interface = pin_pair.first;
            
            for (const auto& bit_pair : pin_pair.second) {
                int bit_width = bit_pair.first;
                const std::vector<std::string>& cell_names = bit_pair.second;
                
                total_groups++;
                
                // Create group key matching hierarchical format
                std::string group_key = clock_edge + "|" + pin_interface + "|" + std::to_string(bit_width) + "bit";
                
                // Find optimal FF using the preprocessing scoring formula
                std::string best_cell = "";
                double best_score = std::numeric_limits<double>::max();
                
                for (const std::string& cell_name : cell_names) {
                    auto cell_it = db.cell_library.find(cell_name);
                    if (cell_it == db.cell_library.end()) continue;
                    
                    auto cell = cell_it->second;
                    if (!cell->is_flip_flop()) continue;
                    
                    // Use same formula as preprocessing: Score = (β·Power + γ·Area)/bit + δ
                    double power = cell->leakage_power;
                    double area = cell->area;
                    int effective_bit_width = (bit_width > 0) ? bit_width : 1;
                    
                    // Get timing_repr from hardcoded map and multiply by alpha
                    double timing_repr = TimingReprMap::get_timing_repr(cell_name);
                    double delta = db.objective_weights.alpha * timing_repr * 1000;
                    
                    double score = (db.objective_weights.beta * power * 0.001 + 
                                   db.objective_weights.gamma * area) / 
                                   static_cast<double>(effective_bit_width) + delta;
                    
                    if (score < best_score) {
                        best_score = score;
                        best_cell = cell_name;
                    }
                }
                
                if (!best_cell.empty()) {
                    db.optimal_ff_for_groups[group_key] = best_cell;
                    groups_with_optimal++;
                    
                    std::cout << "    [" << group_key << "]: " << best_cell 
                              << " (score: " << std::fixed << std::setprecision(6) << best_score << ")" << std::endl;
                }
            }
        }
    }
    
    std::cout << "    Found optimal FFs for " << groups_with_optimal << "/" << total_groups << " groups" << std::endl;
}