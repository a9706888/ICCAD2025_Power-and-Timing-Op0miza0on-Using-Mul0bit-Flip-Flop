#include "data_structures.hpp"
#include "parsers.hpp"
#include <set>
#include <fstream>
#include <algorithm>

// Global storage for debank pin mappings
std::map<std::string, std::string> global_debank_pin_mappings;

// Record debank pin mappings from transformation record
void record_all_debank_pin_mappings_from_record(const TransformationRecord& debank_record) {
    std::cout << "DEBUG: Recording debank pin mappings for " << debank_record.original_instance_name 
              << " -> " << debank_record.result_instance_name << std::endl;
    
    for (const auto& pin_pair : debank_record.pin_mapping) {
        const std::string& original_pin = pin_pair.first;      // e.g., "D[0]"
        const std::string& debanked_pin = pin_pair.second;     // e.g., "D"
        
        std::string original_path = debank_record.original_instance_name + "/" + original_pin;
        std::string debanked_path = debank_record.result_instance_name + "/" + debanked_pin;
        
        global_debank_pin_mappings[original_path] = debanked_path;
        
        std::cout << "  DEBUG: Recorded " << original_path << " -> " << debanked_path << std::endl;
    }
}

// =============================================================================
// SIMPLE PIN MAPPING SYSTEM (NO DEBANK VERSION)
// =============================================================================
// 假設: 沒有DEBANK操作，所以每個original instance最多對應一個final instance
// 路徑: original → (SUBSTITUTE) → (BANK) → final
// =============================================================================

struct SimpleTransformationChain {
    std::string original_instance_name;
    std::string final_instance_name;
    std::vector<std::string> transformation_path;  // 記錄經過的操作類型
    bool is_banked = false;  // 是否被banking掉了
    std::string cluster_id;  // 用於DEBANK instances，標識原始multi-bit FF
};

// 建立original到final的簡單mapping (1對1或1對0)
std::map<std::string, SimpleTransformationChain> build_simple_transformation_chains(
    const DesignDatabase& db) {
    
    std::map<std::string, SimpleTransformationChain> chains;
    
    // 從transformation_history建立chains，而不是從current instances
    // 首先收集所有出現過的original instances，但排除被DEBANK的instances
    std::set<std::string> all_original_instances;
    std::set<std::string> debanked_instances;  // 被DEBANK的original instances
    
    // 先識別所有被DEBANK的original instances
    for (const auto& record : db.transformation_history) {
        if (record.operation == TransformationRecord::DEBANK) {
            debanked_instances.insert(record.original_instance_name);
        }
    }
    
    for (const auto& record : db.transformation_history) {
        if (!record.original_instance_name.empty()) {
            // 只加入沒有被DEBANK的original instances
            if (debanked_instances.count(record.original_instance_name) == 0) {
                all_original_instances.insert(record.original_instance_name);
            }
        }
        // 對於BANK操作，也要處理related_instances
        if (record.operation == TransformationRecord::BANK) {
            for (const auto& related_name : record.related_instances) {
                if (!related_name.empty() && debanked_instances.count(related_name) == 0) {
                    all_original_instances.insert(related_name);
                }
            }
        }
    }
    
    // 初始化chains：每個original instance都有一條chain，一開始final就是自己
    for (const auto& original_name : all_original_instances) {
        SimpleTransformationChain chain;
        chain.original_instance_name = original_name;
        chain.final_instance_name = original_name;  // 預設沒變化
        chains[original_name] = chain;
    }
    
    std::cout << "  DEBUG: Initialized " << chains.size() << " chains from transformation history" << std::endl;
    
    // 遍歷transformation_history來更新chains
    for (const auto& record : db.transformation_history) {
        switch (record.operation) {
            case TransformationRecord::KEEP:
                // KEEP操作：original和result應該相同
                if (!record.original_instance_name.empty() && chains.count(record.original_instance_name)) {
                    chains[record.original_instance_name].final_instance_name = record.result_instance_name;
                    chains[record.original_instance_name].cluster_id = record.cluster_id;
                }
                break;
                
            case TransformationRecord::SUBSTITUTE:
                // 更新final instance name，但仍然是1對1
                if (!record.original_instance_name.empty() && chains.count(record.original_instance_name)) {
                    chains[record.original_instance_name].final_instance_name = record.result_instance_name;
                    chains[record.original_instance_name].transformation_path.push_back("SUBSTITUTE");
                    chains[record.original_instance_name].cluster_id = record.cluster_id;
                }
                break;
                
            case TransformationRecord::POST_SUBSTITUTE:
                // Same as SUBSTITUTE - 1:1 mapping but after banking
                if (!record.original_instance_name.empty() && chains.count(record.original_instance_name)) {
                    chains[record.original_instance_name].final_instance_name = record.result_instance_name;
                    chains[record.original_instance_name].transformation_path.push_back("POST_SUBSTITUTE");
                    chains[record.original_instance_name].cluster_id = record.cluster_id;
                }
                break;
                
            case TransformationRecord::BANK:
                // 多個original instances被banking成一個final instance
                // 處理primary instance
                if (!record.original_instance_name.empty() && chains.count(record.original_instance_name)) {
                    chains[record.original_instance_name].final_instance_name = record.result_instance_name;
                    chains[record.original_instance_name].transformation_path.push_back("BANK");
                    chains[record.original_instance_name].is_banked = true;
                    chains[record.original_instance_name].cluster_id = record.cluster_id;
                }
                
                // 處理related_instances (其他被bank的instances)
                for (const auto& related_name : record.related_instances) {
                    if (!related_name.empty() && chains.count(related_name)) {
                        chains[related_name].final_instance_name = record.result_instance_name;
                        chains[related_name].transformation_path.push_back("BANK");
                        chains[related_name].is_banked = true;
                        chains[related_name].cluster_id = record.cluster_id;
                    }
                }
                break;
                
            case TransformationRecord::DEBANK:
                // 忽略，因為我們假設沒有DEBANK
                std::cout << "WARNING: Found DEBANK operation but should be ignored in simple version" << std::endl;
                break;
        }
    }
    
    return chains;
}

// 檢查pin是否存在於instance中
bool pin_exists_in_instance(const std::string& pin_name, 
                          const std::shared_ptr<Instance>& instance,
                          const DesignDatabase& db) {
    if (!instance || !instance->cell_template) return false;
    
    // 檢查cell template是否有這個pin
    for (const auto& pin : instance->cell_template->pins) {
        if (pin.name == pin_name) {
            return true;
        }
    }
    return false;
}

// 從transformation record中找到原始FF的cell template信息
std::shared_ptr<CellTemplate> get_original_cell_template_from_record(
    const std::string& original_instance_name, 
    const DesignDatabase& db) {
    
    // 在transformation_history中找到這個original instance的記錄
    for (const auto& record : db.transformation_history) {
        if (record.original_instance_name == original_instance_name) {
            // 找到原始cell type對應的template
            auto cell_it = db.cell_library.find(record.original_cell_type);
            if (cell_it != db.cell_library.end()) {
                return cell_it->second;
            }
        }
        // 也檢查related_instances
        if (record.operation == TransformationRecord::BANK) {
            for (const auto& related_name : record.related_instances) {
                if (related_name == original_instance_name) {
                    // 對於BANK record，related instances通常有相同的cell type
                    // 但這裡我們需要更精確的處理...暫時用primary的cell type
                    auto cell_it = db.cell_library.find(record.original_cell_type);
                    if (cell_it != db.cell_library.end()) {
                        return cell_it->second;
                    }
                }
            }
        }
    }
    return nullptr;
}

// 為單一transformation chain生成pin mapping
std::vector<std::string> generate_pin_mapping_for_chain(
    const SimpleTransformationChain& chain,
    const DesignDatabase& db) {
    
    std::vector<std::string> pin_mappings;
    
    // 所有FF都需要pin mapping，不管是否有變化
    
    // 找到final instance (應該存在於current instances中)
    auto final_inst = db.instances.find(chain.final_instance_name);
    if (final_inst == db.instances.end()) {
        std::cout << "WARNING: Final instance not found: " << chain.final_instance_name << std::endl;
        return pin_mappings;
    }
    
    // 對於original instance，由於可能已經被刪除，我們從transformation record中重建
    auto original_cell_template = get_original_cell_template_from_record(chain.original_instance_name, db);
    if (!original_cell_template) {
        std::cout << "WARNING: Cannot find original cell template for: " << chain.original_instance_name << std::endl;
        return pin_mappings;
    }
    
    // 獲取original instance的實際connections (基於transformation history中的實際連接)
    std::set<std::string> original_pins_with_connections;
    
    // 從transformation history中找到original instance的實際connections
    for (const auto& record : db.transformation_history) {
        if (record.original_instance_name == chain.original_instance_name) {
            // 使用pin_mapping中的keys作為原始pins (這些是實際有connections的pins)
            for (const auto& pin_pair : record.pin_mapping) {
                std::string original_pin = pin_pair.first;  // 例如：D0, Q0, CK等
                original_pins_with_connections.insert(original_pin);
            }
            break;  // 找到第一個matching record就夠了
        }
    }
    
    // 如果transformation history中沒有pin mapping資訊，fallback到cell template
    if (original_pins_with_connections.empty()) {
        // Fallback: 假設所有cell template pins都有connections (用於非DEBANK cases)
        for (const auto& pin : original_cell_template->pins) {
            original_pins_with_connections.insert(pin.name);
        }
    }
    
    
    // 對於每個有實際connection的original pin，檢查是否也存在於final instance
    for (const auto& pin_name : original_pins_with_connections) {
        if (pin_exists_in_instance(pin_name, final_inst->second, db)) {
            // Banking情況下，需要處理bit indexing
            if (chain.is_banked) {
                // 找到這個original instance在banking中的bit index
                // 這需要從transformation record中找到相關資訊
                std::string final_pin_name = pin_name;  // 預設無變化
                
                // TODO: 這裡需要更精確的bit index mapping
                // 暫時使用簡單版本：假設banking後pin名字不變
                
                std::string mapping = chain.original_instance_name + "/" + pin_name + 
                                    " map " + chain.final_instance_name + "/" + final_pin_name;
                pin_mappings.push_back(mapping);
            } else {
                // 非banking情況，直接mapping
                std::string mapping = chain.original_instance_name + "/" + pin_name + 
                                    " map " + chain.final_instance_name + "/" + pin_name;
                pin_mappings.push_back(mapping);
            }
        }
        // 如果final instance沒有這個pin，就不mapping（符合你的策略）
    }
    
    return pin_mappings;
}

// 生成完整的pin mapping list file
void generate_simple_pin_mapping_file(const DesignDatabase& db, const std::string& output_file) {
    std::cout << "\n📍 Generating simple pin mapping file: " << output_file << std::endl;
    
    // 1. 建立transformation chains
    auto chains = build_simple_transformation_chains(db);
    std::cout << "  Built " << chains.size() << " transformation chains" << std::endl;
    
    // 2. 統計final instances數量
    std::set<std::string> final_instances;
    for (const auto& chain_pair : chains) {
        final_instances.insert(chain_pair.second.final_instance_name);
    }
    
    std::cout << "  Total final FF instances: " << final_instances.size() << std::endl;
    
    // 3. 生成pin mappings
    std::vector<std::string> all_pin_mappings;
    int chains_with_mappings = 0;
    
    for (const auto& chain_pair : chains) {
        auto pin_mappings = generate_pin_mapping_for_chain(chain_pair.second, db);
        if (!pin_mappings.empty()) {
            chains_with_mappings++;
            all_pin_mappings.insert(all_pin_mappings.end(), 
                                   pin_mappings.begin(), pin_mappings.end());
        }
    }
    
    std::cout << "  Generated mappings for " << chains_with_mappings << " chains" << std::endl;
    std::cout << "  Total pin mappings: " << all_pin_mappings.size() << std::endl;
    
    // 3.5. Stage 2: Replace _BIT instance names using global debank pin mappings
    std::cout << "  Applying Stage 2: Debank pin mapping replacement..." << std::endl;
    
    // Create reverse mapping: debanked_pin_path -> original_pin_path
    std::map<std::string, std::string> reverse_debank_mappings;
    for (const auto& debank_pair : global_debank_pin_mappings) {
        const std::string& original_pin_path = debank_pair.first;   // foo1__test_multibit_4/D0
        const std::string& debanked_pin_path = debank_pair.second;  // foo1__test_multibit_4_BIT0/D
        reverse_debank_mappings[debanked_pin_path] = original_pin_path;
    }
    
    std::cout << "  Created reverse debank mappings: " << reverse_debank_mappings.size() << std::endl;
    
    
    int replacements_made = 0;
    for (auto& mapping : all_pin_mappings) {
        // Parse mapping format: "original_instance/pin map final_instance/pin"
        size_t map_pos = mapping.find(" map ");
        if (map_pos == std::string::npos) continue;
        
        std::string original_part = mapping.substr(0, map_pos);
        std::string final_part = mapping.substr(map_pos + 5); // " map " = 5 chars
        
        // Check if original_part contains "_BIT" pattern
        if (original_part.find("_BIT") != std::string::npos) {
            // Look for replacement in reverse_debank_mappings
            auto reverse_it = reverse_debank_mappings.find(original_part);
            if (reverse_it != reverse_debank_mappings.end()) {
                // Replace the original_part with the true original pin path
                mapping = reverse_it->second + " map " + final_part;
                replacements_made++;
            } else {
                // For shared pins (CK, SI, SE) that may not be in reverse mappings,
                // try to infer the original pin path
                size_t slash_pos = original_part.find('/');
                if (slash_pos != std::string::npos) {
                    std::string instance_part = original_part.substr(0, slash_pos);  // foo1__test_multibit_1_BIT2
                    std::string pin_part = original_part.substr(slash_pos + 1);      // CK
                    
                    // Extract the base instance name (remove _BIT* suffix)
                    size_t bit_pos = instance_part.find("_BIT");
                    if (bit_pos != std::string::npos) {
                        std::string base_instance = instance_part.substr(0, bit_pos);  // foo1__test_multibit_1
                        
                        // For shared pins (CK, SI, SE, SO, R, S), they map to the same pin name
                        if (pin_part == "CK" || pin_part == "SI" || pin_part == "SE" || 
                            pin_part == "SO" || pin_part == "R" || pin_part == "S") {
                            std::string inferred_original = base_instance + "/" + pin_part;
                            mapping = inferred_original + " map " + final_part;
                            replacements_made++;
                        }
                    }
                }
            }
        }
    }
    
    std::cout << "  Made " << replacements_made << " debank pin mapping replacements" << std::endl;
    std::cout << "  Final pin mappings: " << all_pin_mappings.size() << std::endl;
    
    // 4. 寫入文件
    std::ofstream out(output_file);
    if (!out.is_open()) {
        std::cerr << "Error: Cannot open " << output_file << " for writing" << std::endl;
        return;
    }
    
    // Header
    out << "CellInst " << final_instances.size() << std::endl;
    
    // Pin mappings
    for (const auto& mapping : all_pin_mappings) {
        out << mapping << std::endl;
    }
    
    out.close();
    std::cout << "✅ Simple pin mapping file generated successfully" << std::endl;
}

// 導出transformation chains報告供檢查
void export_simple_transformation_chains_report(const DesignDatabase& db, 
                                               const std::string& output_file = "transformation_chains_report.txt") {
    auto chains = build_simple_transformation_chains(db);
    
    std::ofstream out(output_file);
    if (!out.is_open()) {
        std::cerr << "Error: Cannot open " << output_file << " for writing" << std::endl;
        return;
    }
    
    out << "=== SIMPLE TRANSFORMATION CHAINS REPORT ===" << std::endl;
    out << "Total chains: " << chains.size() << std::endl;
    out << std::endl;
    
    // 統計不同類型的transformations
    int keep_count = 0, substitute_count = 0, bank_count = 0;
    for (const auto& chain_pair : chains) {
        const auto& chain = chain_pair.second;
        if (chain.transformation_path.empty()) {
            keep_count++;
        } else if (std::find(chain.transformation_path.begin(), 
                           chain.transformation_path.end(), "BANK") != chain.transformation_path.end()) {
            bank_count++;
        } else if (std::find(chain.transformation_path.begin(), 
                           chain.transformation_path.end(), "SUBSTITUTE") != chain.transformation_path.end()) {
            substitute_count++;
        }
    }
    
    out << "KEEP chains: " << keep_count << std::endl;
    out << "SUBSTITUTE chains: " << substitute_count << std::endl;
    out << "BANK chains: " << bank_count << std::endl;
    out << std::endl;
    
    // 詳細列出每個chain
    out << "=== DETAILED CHAINS ===" << std::endl;
    for (const auto& chain_pair : chains) {
        const auto& chain = chain_pair.second;
        out << "Original: " << chain.original_instance_name << std::endl;
        out << "Final: " << chain.final_instance_name << std::endl;
        out << "Path: ";
        if (chain.transformation_path.empty()) {
            out << "KEEP";
        } else {
            for (size_t i = 0; i < chain.transformation_path.size(); i++) {
                if (i > 0) out << " -> ";
                out << chain.transformation_path[i];
            }
        }
        out << std::endl;
        out << "Is banked: " << (chain.is_banked ? "Yes" : "No") << std::endl;
        if (!chain.cluster_id.empty()) {
            out << "Cluster ID: " << chain.cluster_id << std::endl;
        }
        out << std::endl;
    }
    
    out.close();
    std::cout << "  Transformation chains report exported: " << output_file << std::endl;
}