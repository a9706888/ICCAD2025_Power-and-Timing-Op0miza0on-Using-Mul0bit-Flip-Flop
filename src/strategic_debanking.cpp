#include "data_structures.hpp"
#include "parsers.hpp"
#include <iostream>
#include <vector>
#include <memory>
#include <algorithm>
#include <iomanip>
#include <fstream>

// Strategic debanking: Convert multi-bit FFs to single-bit FFs for re-optimization
void perform_strategic_debanking(DesignDatabase& db) {
    std::cout << "\n🔧 Step: Strategic Debanking..." << std::endl;
    
    std::vector<std::shared_ptr<Instance>> new_instances;
    std::vector<std::shared_ptr<Instance>> instances_to_remove;
    
    int debanked_count = 0;
    int total_new_instances = 0;
    
    // Process all instances
    for (auto& pair : db.instances) {
        auto& instance = pair.second;
        if (!instance->cell_template) continue;
        
        // Check if this is a multi-bit FF that can be debanked
        if (instance->cell_template->is_flip_flop() && 
            instance->cell_template->single_bit_degenerate != "null") {
            
            std::string parent_cell_name = instance->cell_template->single_bit_degenerate;
            
            // Find the parent single-bit cell template
            auto parent_iter = db.cell_library.find(parent_cell_name);
            if (parent_iter == db.cell_library.end()) {
                std::cout << "  WARNING: Parent cell " << parent_cell_name 
                         << " not found for " << instance->cell_template->name << std::endl;
                continue;
            }
            
            auto parent_template = parent_iter->second;
            int bit_width = instance->cell_template->bit_width;
            
            std::cout << "  Debanking " << instance->name 
                     << " (" << instance->cell_template->name << ", " << bit_width << "-bit)"
                     << " → " << bit_width << "× " << parent_cell_name << std::endl;
            
            // Create individual single-bit instances
            std::vector<std::shared_ptr<Instance>> resulting_singlebit_instances;
            
            for (int bit = 0; bit < bit_width; bit++) {
                auto new_instance = std::make_shared<Instance>();
                new_instance->name = instance->name + "_BIT" + std::to_string(bit);
                new_instance->cell_type = parent_cell_name;  // Set cell type string
                new_instance->cell_template = parent_template;
                new_instance->position.x = instance->position.x;  // Keep same position for now
                new_instance->position.y = instance->position.y;
                new_instance->orientation = instance->orientation;  // Keep same orientation
                
                // Set cluster_id for debank cluster re-banking priority
                new_instance->cluster_id = instance->name;  // Use original multibit instance name as cluster ID
                
                // Preserve module assignment - debanked instances stay in same module
                new_instance->module_name = instance->module_name;
                
                // Map connections from multi-bit to single-bit
                map_multibit_to_singlebit_connections(instance, new_instance, bit, db);
                
                new_instances.push_back(new_instance);
                resulting_singlebit_instances.push_back(new_instance);
                total_new_instances++;
            }
            
            // Record debank transformation with enhanced cluster tracking
            record_debank_transformation(db, instance, resulting_singlebit_instances, 
                                       parent_cell_name);
            
            // Record debank pin mappings to global storage
            // Find the newly created debank transformation records and record their pin mappings
            size_t records_start = db.transformation_history.size() - resulting_singlebit_instances.size();
            for (size_t i = records_start; i < db.transformation_history.size(); i++) {
                if (db.transformation_history[i].operation == TransformationRecord::DEBANK) {
                    record_all_debank_pin_mappings_from_record(db.transformation_history[i]);
                }
            }
            
            // Remove the corresponding KEEP record for this multi-bit instance
            remove_keep_transformation_record(db, instance->name);
            
            // Mark original instance for removal
            instances_to_remove.push_back(instance);
            debanked_count++;
        }
    }
    
    // Remove original multi-bit instances
    for (auto& instance_to_remove : instances_to_remove) {
        // Find and erase from unordered_map
        for (auto it = db.instances.begin(); it != db.instances.end(); ++it) {
            if (it->second == instance_to_remove) {
                db.instances.erase(it);
                break;
            }
        }
    }
    
    // Add new single-bit instances
    for (auto& new_instance : new_instances) {
        db.instances[new_instance->name] = new_instance;
    }
    
    std::cout << "  ✓ Debanked " << debanked_count << " multi-bit FFs → " 
              << total_new_instances << " single-bit FFs" << std::endl;
    std::cout << "  ✓ Total instances: " << db.instances.size() << std::endl;
    
    // Capture DEBANK stage - all instances after debanking operation
    std::cout << "  Capturing DEBANK stage..." << std::endl;
    std::vector<std::shared_ptr<Instance>> all_instances_after_debank;
    for (const auto& inst_pair : db.instances) {
        all_instances_after_debank.push_back(inst_pair.second);
    }
    
    // Get indices of new DEBANK transformation records
    std::vector<size_t> debank_indices;
    for (size_t i = 0; i < db.transformation_history.size(); i++) {
        if (db.transformation_history[i].operation == TransformationRecord::DEBANK) {
            debank_indices.push_back(i);
        }
    }
    
    db.complete_pipeline.capture_stage("DEBANK", all_instances_after_debank, debank_indices, &db.transformation_history);
}

// Helper function to map connections from multi-bit FF to single-bit FF
void map_multibit_to_singlebit_connections(std::shared_ptr<Instance> multibit_instance,
                                         std::shared_ptr<Instance> singlebit_instance,
                                         int bit_index,
                                         DesignDatabase& db) {
    
    // For each pin in the single-bit template, find corresponding connection
    for (const auto& singlebit_pin : singlebit_instance->cell_template->pins) {
        std::string singlebit_pin_name = singlebit_pin.name;
        
        // Find corresponding pin in multi-bit instance
        std::string multibit_pin_name = map_singlebit_pin_to_multibit(singlebit_pin_name, bit_index);
        
        // Look for this connection in the multi-bit instance connections vector
        std::string connected_net = "";
        for (const auto& conn : multibit_instance->connections) {
            if (conn.pin_name == multibit_pin_name) {
                connected_net = conn.net_name;
                break;
            }
        }
        
        // If found, add to single-bit instance
        if (!connected_net.empty()) {
            singlebit_instance->connections.emplace_back(singlebit_pin_name, connected_net);
        } else {
            // If not found, check for shared pins (like clock, reset)
            std::string shared_pin = find_shared_pin_connection(multibit_instance, singlebit_pin_name);
            if (!shared_pin.empty()) {
                for (const auto& conn : multibit_instance->connections) {
                    if (conn.pin_name == shared_pin) {
                        singlebit_instance->connections.emplace_back(singlebit_pin_name, conn.net_name);
                        break;
                    }
                }
            }
        }
    }
}

// Map single-bit pin name to multi-bit pin name based on bit index
std::string map_singlebit_pin_to_multibit(const std::string& singlebit_pin, int bit_index) {
    // Common multi-bit pin naming patterns:
    // D → D0, D1, D2, D3 for 4-bit FF  
    // Q → Q0, Q1, Q2, Q3 for 4-bit FF
    // SI → SI (shared, no bit index)
    // SE → SE (shared, no bit index)
    
    if (singlebit_pin == "D") {
        return "D" + std::to_string(bit_index);
    }
    if (singlebit_pin == "Q") {
        return "Q" + std::to_string(bit_index);
    }
    if (singlebit_pin == "QN") {
        return "QN" + std::to_string(bit_index);
    }
    
    // For scan pins and other control pins, they are typically shared across all bits
    // So no bit index suffix is needed
    return singlebit_pin;
}

// Find shared pin connections (clock, reset, enable, etc.)
std::string find_shared_pin_connection(std::shared_ptr<Instance> multibit_instance, 
                                     const std::string& singlebit_pin) {
    // These pins are typically shared across all bits in multi-bit FFs:
    std::vector<std::string> shared_pins = {"CK", "CLK", "CP", "R", "RB", "S", "SB", "SE", "RD", "SD"};
    
    for (const std::string& shared_pin : shared_pins) {
        if (singlebit_pin == shared_pin) {
            // Check if this pin exists in multi-bit instance connections
            for (const auto& conn : multibit_instance->connections) {
                if (conn.pin_name == shared_pin) {
                    return shared_pin;
                }
            }
        }
    }
    
    return "";  // Not found
}

// Helper function to remove KEEP transformation record for a specific instance
void remove_keep_transformation_record(DesignDatabase& db, const std::string& instance_name) {
    auto it = db.transformation_history.begin();
    while (it != db.transformation_history.end()) {
        if (it->operation == TransformationRecord::KEEP && 
            it->original_instance_name == instance_name) {
            it = db.transformation_history.erase(it);
        } else {
            ++it;
        }
    }
}

// Export debanking results for analysis
void export_strategic_debanking_report(const DesignDatabase& db) {
    std::ofstream report("strategic_debanking_report.txt");
    
    report << "=== Strategic Debanking Report ===" << std::endl;
    report << "Generated after debanking multi-bit FFs to single-bit FFs" << std::endl;
    report << std::endl;
    
    // Count FF instances by type
    std::map<std::string, int> ff_count;
    int total_ff_instances = 0;
    
    for (const auto& pair : db.instances) {
        const auto& instance = pair.second;
        if (instance->cell_template && instance->cell_template->is_flip_flop()) {
            ff_count[instance->cell_template->name]++;
            total_ff_instances++;
        }
    }
    
    report << "FF Instance Count After Debanking:" << std::endl;
    report << "Total FF instances: " << total_ff_instances << std::endl;
    report << std::endl;
    
    for (const auto& pair : ff_count) {
        report << std::setw(30) << pair.first << ": " << std::setw(6) << pair.second << " instances" << std::endl;
    }
    
    report << std::endl;
    report << "Note: All listed FFs should now be single-bit FFs ready for re-optimization." << std::endl;
    
    report.close();
    std::cout << "📄 Strategic debanking report exported to: strategic_debanking_report.txt" << std::endl;
}