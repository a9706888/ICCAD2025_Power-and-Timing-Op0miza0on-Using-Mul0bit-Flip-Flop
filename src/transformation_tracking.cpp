// =============================================================================
// TRANSFORMATION TRACKING SYSTEM
// =============================================================================
// This module implements transformation tracking for the ICCAD 2025 Contest
// It tracks all FF transformations (keep, substitute, bank, debank) and 
// generates the required contest output files (.list, .def, .v)
// =============================================================================

#include "parsers.hpp"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cstdio>


// =============================================================================
// TRANSFORMATION RECORDING FUNCTIONS
// =============================================================================

void record_keep_transformation(DesignDatabase& db, 
                               std::shared_ptr<Instance> original_instance) {
    TransformationRecord record(
        original_instance->name,
        original_instance->name,  // Same name for KEEP
        TransformationRecord::KEEP,
        original_instance->cell_type,
        original_instance->cell_type  // Same cell type for KEEP
    );
    
    // Record pin mapping (1:1 for KEEP operations)
    for (const auto& conn : original_instance->connections) {
        record.pin_mapping[conn.pin_name] = conn.pin_name;
    }
    
    // Record position information
    record.result_x = original_instance->position.x;
    record.result_y = original_instance->position.y;
    record.result_orientation = orientation_to_string(original_instance->orientation);
    
    // Enhanced cluster tracking for KEEP operations
    record.cluster_id = original_instance->name;  // Use instance name as cluster ID for KEEP
    record.stage = "KEEP";
    
    db.transformation_history.push_back(record);
}

void record_debank_transformation(DesignDatabase& db,
                                 std::shared_ptr<Instance> original_multibit_instance,
                                 const std::vector<std::shared_ptr<Instance>>& resulting_singlebit_instances,
                                 const std::string& parent_cell_type) {
    // Record the debank operation for each resulting single-bit instance
    std::string cluster_id = original_multibit_instance->name;  // Use original instance name as cluster ID
    
    for (auto& singlebit_instance : resulting_singlebit_instances) {
        TransformationRecord record(
            original_multibit_instance->name,           // Original multi-bit instance name
            singlebit_instance->name,                   // Resulting single-bit instance name  
            TransformationRecord::DEBANK,
            original_multibit_instance->cell_type,     // Original multi-bit cell type
            parent_cell_type                           // Resulting single-bit cell type
        );
        
        // Set enhanced tracking fields
        record.cluster_id = cluster_id;
        record.stage = "DEBANK";
        
        // Record position information
        record.result_x = singlebit_instance->position.x;
        record.result_y = singlebit_instance->position.y;
        record.result_orientation = orientation_to_string(singlebit_instance->orientation);
        
        // Record pin mapping (multi-bit pins to single-bit pins)
        // Map multi-bit pins like D[0], D[1], etc. to single-bit D pin
        // Determine bit index for this single-bit instance
        int bit_index = 0; // Default to 0
        for (size_t i = 0; i < resulting_singlebit_instances.size(); i++) {
            if (resulting_singlebit_instances[i]->name == singlebit_instance->name) {
                bit_index = static_cast<int>(i);
                break;
            }
        }
        
        // Create pin mappings for common FF pins
        std::vector<std::string> ff_pins = {"D", "Q", "QN", "CK", "SI", "SE", "SO", "R", "S"};
        for (const auto& pin : ff_pins) {
            // Check if original multi-bit instance has this pin (with bit index)
            std::string multibit_pin = pin + std::to_string(bit_index);
            bool has_indexed_pin = false;
            
            // Check if the multi-bit instance actually has indexed pins
            for (const auto& conn : original_multibit_instance->connections) {
                if (conn.pin_name == multibit_pin) {
                    has_indexed_pin = true;
                    break;
                }
            }
            
            if (has_indexed_pin) {
                // Multi-bit pin D[0] -> single-bit pin D
                record.pin_mapping[multibit_pin] = pin;
            } else {
                // For shared pins like CK, SI, SE (no bit index)
                bool has_shared_pin = false;
                for (const auto& conn : original_multibit_instance->connections) {
                    if (conn.pin_name == pin) {
                        has_shared_pin = true;
                        break;
                    }
                }
                
                if (has_shared_pin) {
                    // Shared pin CK -> CK (same name)
                    record.pin_mapping[pin] = pin;
                }
            }
        }
        
        // Add all other single-bit instances as related instances
        for (auto& related_inst : resulting_singlebit_instances) {
            if (related_inst->name != singlebit_instance->name) {
                record.related_instances.push_back(related_inst->name);
            }
        }
        
        db.transformation_history.push_back(record);
    }
}

void record_substitute_transformation(DesignDatabase& db,
                                    const std::string& instance_name,
                                    const std::string& original_cell_type,
                                    const std::string& final_cell_type) {
    // This function is now used by record_final_substitution_operations
    // to record the complete substitution from original to final cell type
    TransformationRecord record(
        instance_name,                      // original_instance_name
        instance_name,                      // result_instance_name (same instance, different cell)
        TransformationRecord::SUBSTITUTE,
        original_cell_type,                 // original_cell_type
        final_cell_type                     // result_cell_type
    );
    
    // Find the instance to get current information
    auto instance_it = db.instances.find(instance_name);
    if (instance_it == db.instances.end()) {
        std::cerr << "ERROR: Cannot find instance " << instance_name << " for substitution record" << std::endl;
        return;
    }
    auto instance = instance_it->second;
    
    // Record pin mapping (1:1 for substitutions - same pin names)
    std::vector<std::string> common_ff_pins = {"D", "Q", "QN", "CK", "SI", "SE", "SO", "R", "S"};
    for (const auto& pin : common_ff_pins) {
        // Check if instance has this pin connection
        bool has_pin = false;
        for (const auto& conn : instance->connections) {
            if (conn.pin_name == pin) {
                has_pin = true;
                break;
            }
        }
        
        if (has_pin) {
            // For substitution: pin names stay the same (D->D, Q->Q, etc.)
            record.pin_mapping[pin] = pin;
        }
    }
    
    // Record position information
    record.result_x = instance->position.x;
    record.result_y = instance->position.y;
    record.result_orientation = orientation_to_string(instance->orientation);
    
    // Enhanced cluster tracking: Inherit cluster_id from previous record
    std::string inherited_cluster_id = "";
    for (const auto& existing_record : db.transformation_history) {
        bool is_matching_record = (existing_record.original_instance_name == instance_name) ||
                                 (existing_record.result_instance_name == instance_name);
        if (is_matching_record && !existing_record.cluster_id.empty()) {
            inherited_cluster_id = existing_record.cluster_id;
            break;
        }
    }
    
    // Set cluster_id and stage info
    record.cluster_id = inherited_cluster_id.empty() ? instance_name : inherited_cluster_id;
    record.stage = "SUBSTITUTE";
    
    db.transformation_history.push_back(record);
}

// Helper function for three-stage substitution record tracking
void record_substitution_transformation_complete(DesignDatabase& db,
                                                const std::string& instance_name,
                                                const std::string& original_cell_type,
                                                const std::string& final_cell_type) {
    // Call the updated record_substitute_transformation function
    record_substitute_transformation(db, instance_name, original_cell_type, final_cell_type);
}

void record_bank_transformation(DesignDatabase& db,
                              const std::vector<std::shared_ptr<Instance>>& original_singlebit_ffs,
                              const std::string& resulting_multibit_name,
                              const std::string& multibit_cell_type,
                              const std::map<std::string, std::string>& pin_mapping) {
    // Create one record for the banking operation
    // We'll use the first original FF as the "primary" one
    if (original_singlebit_ffs.empty()) return;
    
    TransformationRecord record(
        original_singlebit_ffs[0]->name,  // Primary original instance
        resulting_multibit_name,
        TransformationRecord::BANK,
        original_singlebit_ffs[0]->cell_type,
        multibit_cell_type
    );
    
    // Record all other original instances as related
    for (size_t i = 1; i < original_singlebit_ffs.size(); i++) {
        record.related_instances.push_back(original_singlebit_ffs[i]->name);
    }
    
    // Record pin mapping
    record.pin_mapping = pin_mapping;
    
    // Position information (use first FF's position)
    record.result_x = original_singlebit_ffs[0]->position.x;
    record.result_y = original_singlebit_ffs[0]->position.y;
    record.result_orientation = orientation_to_string(original_singlebit_ffs[0]->orientation);
    
    // Enhanced cluster tracking: Try to inherit cluster_id from primary instance
    std::string inherited_cluster_id = "";
    for (const auto& existing_record : db.transformation_history) {
        bool is_matching_record = (existing_record.original_instance_name == original_singlebit_ffs[0]->name) ||
                                 (existing_record.result_instance_name == original_singlebit_ffs[0]->name);
        if (is_matching_record && !existing_record.cluster_id.empty()) {
            inherited_cluster_id = existing_record.cluster_id;
            break;
        }
    }
    
    // Set cluster information
    record.cluster_id = inherited_cluster_id;
    record.stage = "BANK";
    
    db.transformation_history.push_back(record);
}

// =============================================================================
// OPERATION LOG GENERATION FUNCTIONS
// =============================================================================

std::vector<std::string> generate_split_multibit_operations(DesignDatabase& db) {
    std::vector<std::string> operations;
    
    // Group DEBANK records by original_instance_name
    std::map<std::string, std::vector<const TransformationRecord*>> debank_groups;
    
    for (const auto& record : db.transformation_history) {
        if (record.operation == TransformationRecord::DEBANK) {
            debank_groups[record.original_instance_name].push_back(&record);
        }
    }
    
    // Generate split_multibit operation for each debank group
    for (const auto& group : debank_groups) {
        const std::string& original_multibit_name = group.first;
        const std::vector<const TransformationRecord*>& debank_records = group.second;
        
        if (debank_records.empty()) continue;
        
        // Extract bit width and library info
        int bit_width = debank_records.size();
        const std::string& original_lib = debank_records[0]->original_cell_type;
        const std::string& result_lib = debank_records[0]->result_cell_type;
        
        // Generate operation string
        std::stringstream op;
        op << "split_multibit { ";
        
        // Input multibit FF
        op << "{" << original_multibit_name << " " << original_lib << " " << bit_width << "} ";
        
        // Output single-bit FFs (with dummy names and mapping)
        for (size_t i = 0; i < debank_records.size(); i++) {
            std::string dummy_name = "dummy_" + std::to_string(db.global_dummy_counter++);
            std::string real_name = debank_records[i]->result_instance_name;
            
            // Build dummy mapping
            db.dummy_to_real_mapping[dummy_name] = real_name;
            db.real_to_dummy_mapping[real_name] = dummy_name;
            
            op << "{" << dummy_name << " " << result_lib << " 1} ";
        }
        
        op << "}";
        operations.push_back(op.str());
    }
    
    return operations;
}

std::vector<std::string> generate_size_cell_operations(const DesignDatabase& db) {
    std::vector<std::string> operations;
    
    // Find all SUBSTITUTE records
    for (const auto& record : db.transformation_history) {
        if (record.operation == TransformationRecord::SUBSTITUTE) {
            // Determine instance name to use (dummy if exists, otherwise real name)
            std::string instance_name = record.original_instance_name;
            
            // Check if this instance has a dummy mapping
            auto dummy_it = db.real_to_dummy_mapping.find(record.original_instance_name);
            if (dummy_it != db.real_to_dummy_mapping.end()) {
                instance_name = dummy_it->second;  // Use dummy name
            }
            
            // Generate size_cell operation
            std::stringstream op;
            op << "size_cell {" << instance_name << " " 
               << record.original_cell_type << " " << record.result_cell_type << "}";
            operations.push_back(op.str());
        }
    }
    
    return operations;
}

std::vector<std::string> generate_post_substitute_operations(const DesignDatabase& db) {
    std::vector<std::string> operations;
    
    // Find all POST_SUBSTITUTE records
    for (const auto& record : db.transformation_history) {
        if (record.operation == TransformationRecord::POST_SUBSTITUTE) {
            // Generate size_cell operation for post-substitution
            std::stringstream op;
            op << "size_cell {" << record.original_instance_name 
               << " " << record.original_cell_type 
               << " " << record.result_cell_type << "}";
            operations.push_back(op.str());
        }
    }
    
    return operations;
}

std::vector<std::string> generate_create_multibit_operations(const DesignDatabase& db) {
    std::vector<std::string> operations;
    
    // Find all BANK records
    for (const auto& record : db.transformation_history) {
        if (record.operation == TransformationRecord::BANK) {
            // Extract bit width from related instances + primary instance
            int bit_width = 1 + record.related_instances.size();
            
            // Generate create_multibit operation
            std::stringstream op;
            op << "create_multibit { ";
            
            // Primary input FF (use dummy name if available)
            std::string primary_name = record.original_instance_name;
            auto dummy_it = db.real_to_dummy_mapping.find(record.original_instance_name);
            if (dummy_it != db.real_to_dummy_mapping.end()) {
                primary_name = dummy_it->second;
            }
            
            op << "{" << primary_name << " " << record.original_cell_type << " 1} ";
            
            // Related input FFs
            for (const auto& related_name : record.related_instances) {
                std::string input_name = related_name;
                auto related_dummy_it = db.real_to_dummy_mapping.find(related_name);
                if (related_dummy_it != db.real_to_dummy_mapping.end()) {
                    input_name = related_dummy_it->second;
                }
                op << "{" << input_name << " " << record.original_cell_type << " 1} ";
            }
            
            // Output multibit FF
            op << "{" << record.result_instance_name << " " << record.result_cell_type << " " << bit_width << "} ";
            
            op << "}";
            operations.push_back(op.str());
        }
    }
    
    return operations;
}

void generate_operation_log_file(DesignDatabase& db, const std::string& output_file) {
    std::cout << "  Generating .list file with operation log: " << output_file << std::endl;
    
    std::ofstream out(output_file);
    if (!out.is_open()) {
        std::cerr << "Error: Cannot open " << output_file << " for writing" << std::endl;
        return;
    }
    
    // Count current FF instances
    int ff_count = 0;
    for (const auto& inst_pair : db.instances) {
        if (inst_pair.second->is_flip_flop()) {
            ff_count++;
        }
    }
    
    // CellInst section with pin mappings
    out << "CellInst " << ff_count << std::endl;
    
    // Read and include pin mappings from temporary file
    std::ifstream temp_file("temp_pin_mapping.list");
    if (temp_file.is_open()) {
        std::string line;
        bool skip_first_line = true;  // Skip "CellInst XXXX" line from temp file
        while (std::getline(temp_file, line)) {
            if (skip_first_line) {
                skip_first_line = false;
                continue;
            }
            out << line << std::endl;
        }
        temp_file.close();
        
        // Clean up temporary file
        std::remove("temp_pin_mapping.list");
    } else {
        out << "# Pin mapping generation failed" << std::endl;
    }
    
    out << std::endl;
    
    // Generate all operations
    auto debank_operations = generate_split_multibit_operations(db);
    auto substitute_operations = generate_size_cell_operations(db);
    auto bank_operations = generate_create_multibit_operations(db);
    auto post_substitute_operations = generate_post_substitute_operations(db);
    
    int total_operations = debank_operations.size() + substitute_operations.size() + 
                          bank_operations.size() + post_substitute_operations.size();
    
    // OPERATION section
    out << "OPERATION " << total_operations << std::endl;
    
    // Output operations in logical order: DEBANK -> SUBSTITUTE -> BANK -> POST_SUBSTITUTE
    for (const auto& operation : debank_operations) {
        out << operation << std::endl;
    }
    
    for (const auto& operation : substitute_operations) {
        out << operation << std::endl;
    }
    
    for (const auto& operation : bank_operations) {
        out << operation << std::endl;
    }
    
    for (const auto& operation : post_substitute_operations) {
        out << operation << std::endl;
    }
    
    out.close();
    std::cout << "    .list file generated: " << ff_count << " FFs, " << total_operations << " operations ("
              << debank_operations.size() << " DEBANK + " << substitute_operations.size() << " SUBSTITUTE + " 
              << bank_operations.size() << " BANK + " << post_substitute_operations.size() << " POST_SUBSTITUTE)" << std::endl;
}

// =============================================================================
// CONTEST OUTPUT GENERATION FUNCTIONS
// =============================================================================

void generate_pin_mapping_list_file(const DesignDatabase& db, const std::string& output_file) {
    std::cout << "  Generating pin mapping list file: " << output_file << std::endl;
    
    std::ofstream out(output_file);
    if (!out.is_open()) {
        std::cerr << "Error: Cannot open " << output_file << " for writing" << std::endl;
        return;
    }
    
    // Header comment
    out << "# Pin mapping file for ICCAD 2025 Contest" << std::endl;
    out << "# Format: original_instance.original_pin -> result_instance.result_pin" << std::endl;
    out << std::endl;
    
    // Generate pin mappings from transformation history
    for (const auto& record : db.transformation_history) {
        // Get the result cell template to check which pins exist
        auto result_cell_it = db.cell_library.find(record.result_cell_type);
        std::set<std::string> result_cell_pins;
        if (result_cell_it != db.cell_library.end()) {
            for (const auto& pin : result_cell_it->second->pins) {
                result_cell_pins.insert(pin.name);
            }
        }
        
        for (const auto& pin_pair : record.pin_mapping) {
            // Check if result cell actually has this pin
            if (!result_cell_pins.empty() && result_cell_pins.find(pin_pair.second) == result_cell_pins.end()) {
                // Skip pins that don't exist in the result cell
                std::cout << "    Skipping pin mapping " << pin_pair.first << " -> " << pin_pair.second 
                          << " (pin not found in " << record.result_cell_type << ")" << std::endl;
                continue;
            }
            
            out << record.original_instance_name << "." << pin_pair.first 
                << " -> " << record.result_instance_name << "." << pin_pair.second << std::endl;
        }
    }
    
    out.close();
    std::cout << "    Pin mapping list file generated successfully" << std::endl;
}

void generate_final_def_file(const DesignDatabase& db, const std::string& output_file) {
    std::cout << "  Generating final DEF file: " << output_file << std::endl;
    
    std::ofstream out(output_file);
    if (!out.is_open()) {
        std::cerr << "Error: Cannot open " << output_file << " for writing" << std::endl;
        return;
    }
    
    // DEF header
    out << "VERSION 5.8 ;" << std::endl;
    out << "DIVIDERCHAR \"/\" ;" << std::endl;
    out << "BUSBITCHARS \"[]\" ;" << std::endl;
    out << std::endl;
    out << "DESIGN " << db.design_name << " ;" << std::endl;
    out << std::endl;
    out << "UNITS DISTANCE MICRONS 2000 ;" << std::endl;
    out << std::endl;
    
    // Die area
    out << "DIEAREA ( " << (int)db.die_area.x1 << " " << (int)db.die_area.y1 << " ) ( " 
        << (int)db.die_area.x2 << " " << (int)db.die_area.y2 << " ) ;" << std::endl;
    out << std::endl;
    
    // Components section - based on transformation history
    std::set<std::string> result_instances;
    for (const auto& record : db.transformation_history) {
        result_instances.insert(record.result_instance_name);
    }
    
    out << "COMPONENTS " << result_instances.size() << " ;" << std::endl;
    for (const auto& record : db.transformation_history) {
        // Only output each result instance once
        if (result_instances.count(record.result_instance_name)) {
            result_instances.erase(record.result_instance_name); // Remove to avoid duplicates
            
            out << "- " << record.result_instance_name << " " << record.result_cell_type 
                << " + PLACED ( " << (int)record.result_x << " " << (int)record.result_y 
                << " ) " << record.result_orientation << " ;" << std::endl;
        }
    }
    out << "END COMPONENTS" << std::endl;
    out << std::endl;
    
    // Copy PINS section from original (assuming no pin changes)
    out << "PINS " << db.design_pins.size() << " ;" << std::endl;
    for (const auto& pin : db.design_pins) {
        out << "- " << pin.name << " + NET " << pin.net_name 
            << " + DIRECTION " << (pin.direction == DesignPin::INPUT ? "INPUT" : 
                                  pin.direction == DesignPin::OUTPUT ? "OUTPUT" : "INOUT")
            << " + USE SIGNAL ;" << std::endl;
    }
    out << "END PINS" << std::endl;
    out << std::endl;
    
    // NETS section - this would need to be updated based on transformations
    // For now, we'll copy the original nets (this is simplified)
    out << "NETS " << db.nets.size() << " ;" << std::endl;
    for (const auto& net_pair : db.nets) {
        const auto& net = net_pair.second;
        out << "- " << net->name;
        for (const auto& conn : net->connections) {
            out << " ( " << conn.instance_name << " " << conn.pin_name << " )";
        }
        out << " ;" << std::endl;
    }
    out << "END NETS" << std::endl;
    out << std::endl;
    
    out << "END DESIGN" << std::endl;
    
    out.close();
    std::cout << "    Final DEF file generated successfully" << std::endl;
}

// Helper functions for verilog generation
std::string get_module_local_instance_name(const std::string& full_name) {
    // Convert hierarchical names to module-local names
    // "hier_inst_4/U12345" -> "U12345"
    size_t last_slash = full_name.find_last_of('/');
    if (last_slash != std::string::npos) {
        return full_name.substr(last_slash + 1);
    }
    return full_name;
}

std::string get_module_local_net_name(const std::string& full_name) {
    // Convert hierarchical net names to module-local names
    std::string local_name;
    size_t last_slash = full_name.find_last_of('/');
    if (last_slash != std::string::npos) {
        local_name = full_name.substr(last_slash + 1);
    } else {
        local_name = full_name;
    }
    
    // IMPORTANT: Net names now preserve original escaping from parsing
    // clean_net_name() keeps the original format:
    // - "\qo_foo4[68] " (with escaping) -> "\qo_foo4[68] "
    // - "q_mid4[68]" (without escaping) -> "q_mid4[68]"  
    // - "clk" -> "clk"
    //
    // No additional processing needed - return as-is
    
    return local_name;
}

void extract_original_module_structure(const std::string& input_verilog_path, 
                                      const std::string& module_name,
                                      std::string& module_header,
                                      std::string& wire_declarations) {
    std::ifstream file(input_verilog_path);
    if (!file.is_open()) {
        std::cout << "    WARNING: Cannot read original verilog file: " << input_verilog_path << std::endl;
        module_header = "module " + module_name + " ();\n";
        wire_declarations = "";
        return;
    }
    
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    
    // Find module declaration
    std::string search_pattern = "module " + module_name;
    size_t module_start = content.find(search_pattern);
    if (module_start == std::string::npos) {
        module_header = "module " + module_name + " ();\n";
        wire_declarations = "";
        return;
    }
    
    // Extract module header (from "module" to first instance or "endmodule")
    size_t header_end = content.find("SNPS", module_start);
    size_t endmodule_pos = content.find("endmodule", module_start);
    
    if (header_end != std::string::npos && header_end < endmodule_pos) {
        // Extract everything from module start to first instance
        module_header = content.substr(module_start, header_end - module_start);
        
        // Look for wire declarations in the header
        size_t wire_start = module_header.find("wire");
        if (wire_start != std::string::npos) {
            wire_declarations = module_header.substr(wire_start);
            module_header = module_header.substr(0, wire_start);
        } else {
            wire_declarations = "";
        }
    } else {
        // Simple module with no instances
        module_header = "module " + module_name + " ();\n";
        wire_declarations = "";
    }
    
    // Clean up module header - remove any trailing instances
    size_t cleanup_pos = module_header.find("SNPS");
    if (cleanup_pos != std::string::npos) {
        module_header = module_header.substr(0, cleanup_pos);
    }
}

void generate_final_verilog_file(const DesignDatabase& db, const std::string& output_file) {
    std::cout << "  Generating final Verilog file: " << output_file << std::endl;
    
    std::ofstream out(output_file);
    if (!out.is_open()) {
        std::cerr << "ERROR: Cannot open " << output_file << " for writing" << std::endl;
        return;
    }
    
    // Verify we have instances with proper connections
    int ff_count = 0, comb_count = 0, empty_conn_count = 0;
    for (const auto& inst_pair : db.instances) {
        const auto& instance = inst_pair.second;
        if (instance->is_flip_flop()) {
            ff_count++;
        } else {
            comb_count++;
        }
        if (instance->connections.empty()) {
            empty_conn_count++;
        }
    }
    
    std::cout << "    FF instances: " << ff_count << ", Combinational: " << comb_count << std::endl;
    if (empty_conn_count > 0) {
        std::cout << "    WARNING: " << empty_conn_count << " instances have no connections" << std::endl;
    }
    
    // Group instances by module
    std::map<std::string, std::vector<std::shared_ptr<Instance>>> module_instances;
    for (const auto& inst_pair : db.instances) {
        const auto& instance = inst_pair.second;
        std::string module = instance->module_name.empty() ? "top" : instance->module_name;
        module_instances[module].push_back(instance);
    }
    
    std::cout << "    Generating " << module_instances.size() << " modules" << std::endl;
    
    // Generate each module
    for (const auto& module : db.modules) {
        const std::string& module_name = module.name;
        
        std::cout << "    Generating module: " << module_name << std::endl;
        
        // Extract original module structure
        std::string module_header, wire_declarations;
        extract_original_module_structure(db.input_verilog_path, module_name, module_header, wire_declarations);
        
        // Output module header
        out << module_header;
        if (!module_header.empty() && module_header.back() != '\n') {
            out << std::endl;
        }
        
        // Output wire declarations
        if (!wire_declarations.empty()) {
            out << wire_declarations;
            if (wire_declarations.back() != '\n') {
                out << std::endl;
            }
        }
        
        // Add UNCONNECTED wire declaration for this module
        out << "wire UNCONNECTED;" << std::endl;
        out << std::endl;
        
        // Output all instances in this module
        auto instances_it = module_instances.find(module_name);
        if (instances_it != module_instances.end()) {
            const auto& instances = instances_it->second;
            std::cout << "      Outputting " << instances.size() << " instances" << std::endl;
            
            for (const auto& instance : instances) {
                // Use local instance name (remove hierarchy prefix)
                std::string local_name = get_module_local_instance_name(instance->name);
                
                // Use current cell template name (after substitution) instead of original cell_type
                std::string current_cell_type = instance->cell_template ? 
                    instance->cell_template->name : instance->cell_type;
                
                out << current_cell_type << " " << local_name << " (" << std::endl;
                
                // Get cell template to ensure all pins are output
                auto cell_it = db.cell_library.find(current_cell_type);
                std::set<std::string> cell_pins;
                if (cell_it != db.cell_library.end()) {
                    for (const auto& pin : cell_it->second->pins) {
                        cell_pins.insert(pin.name);
                    }
                }
                
                // Create connection map for quick lookup
                std::map<std::string, std::string> conn_map;
                for (const auto& conn : instance->connections) {
                    conn_map[conn.pin_name] = conn.net_name;
                }
                
                // Output ALL pins (including unconnected ones)
                bool first_conn = true;
                for (const std::string& pin_name : cell_pins) {
                    if (!first_conn) out << "," << std::endl;
                    
                    std::string net_name;
                    auto conn_it = conn_map.find(pin_name);
                    if (conn_it != conn_map.end()) {
                        net_name = conn_it->second;
                        // Normalize unconnected net names to single "UNCONNECTED"
                        if (net_name == "SYNOPSYS_UNCONNECTED" || 
                            net_name.find("UNCONNECTED") != std::string::npos) {
                            net_name = "UNCONNECTED";
                        }
                    } else {
                        // Pin not found in connections - must be unconnected
                        net_name = "UNCONNECTED";
                    }
                    
                    // Use local net name
                    std::string local_net = get_module_local_net_name(net_name);
                    out << "    ." << pin_name <<" "<< "("<<" " << local_net <<" "<< ")"<<" ";
                    first_conn = false;
                }
                out << std::endl <<" "<< ")" <<" "<<";"<< std::endl << std::endl;
            }
        }
        
        out << "endmodule" << std::endl << std::endl;
    }
    
    out.close();
    std::cout << "    Verilog file generation completed: " << output_file << std::endl;
}

// =============================================================================
// INITIALIZATION AND REPORTING FUNCTIONS
// =============================================================================

void initialize_transformation_tracking(DesignDatabase& db) {
    std::cout << "  Initializing transformation tracking system..." << std::endl;
    
    db.transformation_history.clear();
    
    // Initialize with KEEP records for all current FF instances
    for (const auto& inst_pair : db.instances) {
        if (inst_pair.second->is_flip_flop()) {
            // Set cluster_id for original instances (use instance name as cluster ID)
            inst_pair.second->cluster_id = inst_pair.second->name;
            record_keep_transformation(db, inst_pair.second);
        }
    }
    
    std::cout << "    Initialized with " << db.transformation_history.size() 
              << " KEEP transformation records" << std::endl;
    
    // Capture ORIGINAL stage - all instances before any transformation
    std::cout << "  Capturing ORIGINAL stage..." << std::endl;
    std::vector<std::shared_ptr<Instance>> all_instances;
    for (const auto& inst_pair : db.instances) {
        all_instances.push_back(inst_pair.second);
    }
    db.complete_pipeline.capture_stage("ORIGINAL", all_instances, {}, &db.transformation_history);
}

void export_transformation_report(const DesignDatabase& db, const std::string& output_file) {
    std::cout << "  Exporting transformation report to: " << output_file << std::endl;
    
    std::ofstream out(output_file);
    if (!out.is_open()) {
        std::cerr << "Error: Cannot open " << output_file << " for writing" << std::endl;
        return;
    }
    
    out << "=== STAGE-BY-STAGE PIPELINE REPORT ===" << std::endl;
    out << "Generated: " << __DATE__ << " " << __TIME__ << std::endl;
    out << std::endl;
    
    // Removed transformation summary as not needed
    
    // Stage-by-stage instance listings
    for (const auto& stage : db.complete_pipeline.stages) {
        out << "=== STAGE: " << stage.stage_name << " ===" << std::endl;
        out << "Total FF instances: " << stage.ff_instances << std::endl;
        out << "Total instances captured: " << stage.instances.size() << std::endl;
        
        if (!stage.instances.empty()) {
            out << std::endl;
            // List all instances in this stage with numbering starting from 1
            for (size_t i = 0; i < stage.instances.size(); i++) {
                const auto& instance = stage.instances[i];
                out << std::setw(4) << (i + 1) << ". " << instance.instance_name 
                    << " (" << instance.cell_type << ")" << std::endl;
                
                // Position information
                out << "      Position: (" << std::fixed << std::setprecision(0) 
                    << instance.x << ", " << instance.y << ") " 
                    << instance.orientation << std::endl;
                
                // Cluster and original info
                out << "      Cluster ID: " << (instance.cluster_id.empty() ? "N/A" : instance.cluster_id) << std::endl;
                out << "      Original name: " << instance.original_name << std::endl;
                
                // Pin connections
                if (!instance.pin_connections.empty()) {
                    out << "      Pin connections:" << std::endl;
                    for (const auto& pin_conn : instance.pin_connections) {
                        out << "        " << pin_conn.first << " -> " << pin_conn.second << std::endl;
                    }
                } else {
                    out << "      Pin connections: None captured" << std::endl;
                }
                
                // Find the most appropriate transformation record for this instance in this stage
                const TransformationRecord* appropriate_record = nullptr;
                
                // Stage-aware record selection
                if (stage.stage_name == "ORIGINAL") {
                    // In ORIGINAL stage, only show KEEP records
                    for (const auto& record : db.transformation_history) {
                        if (record.result_instance_name == instance.instance_name && 
                            record.operation == TransformationRecord::KEEP) {
                            appropriate_record = &record;
                            break;
                        }
                    }
                } else if (stage.stage_name == "DEBANK") {
                    // In DEBANK stage, show DEBANK operations for this instance, otherwise KEEP
                    // BUT exclude future operations
                    for (const auto& record : db.transformation_history) {
                        if (record.result_instance_name == instance.instance_name) {
                            // Skip future operations that shouldn't be shown in DEBANK stage
                            if (record.operation == TransformationRecord::SUBSTITUTE) continue;
                            if (record.operation == TransformationRecord::BANK) continue;
                            if (record.operation == TransformationRecord::KEEP && 
                                !record.stage.empty() && record.stage == "LEGALIZATION") continue;
                                
                            if (record.operation == TransformationRecord::DEBANK) {
                                appropriate_record = &record;
                                break; // Found DEBANK, use this
                            } else if (record.operation == TransformationRecord::KEEP && !appropriate_record) {
                                appropriate_record = &record; // Use KEEP as fallback
                            }
                        }
                    }
                } else if (stage.stage_name == "SUBSTITUTE") {
                    // In SUBSTITUTE stage, prioritize SUBSTITUTE operations, then DEBANK, then KEEP
                    // BUT exclude future operations (BANK, LEGALIZATION)
                    const TransformationRecord* substitute_record = nullptr;
                    const TransformationRecord* debank_record = nullptr;
                    const TransformationRecord* keep_record = nullptr;
                    
                    for (const auto& record : db.transformation_history) {
                        if (record.result_instance_name == instance.instance_name) {
                            // Skip future operations that shouldn't be shown in SUBSTITUTE stage
                            if (record.operation == TransformationRecord::BANK) continue;
                            // Skip KEEP operations with LEGALIZATION stage info
                            if (record.operation == TransformationRecord::KEEP && 
                                !record.stage.empty() && record.stage == "LEGALIZATION") continue;
                            
                            if (record.operation == TransformationRecord::SUBSTITUTE) {
                                substitute_record = &record;
                            } else if (record.operation == TransformationRecord::DEBANK) {
                                debank_record = &record;
                            } else if (record.operation == TransformationRecord::KEEP) {
                                keep_record = &record;
                            }
                        }
                    }
                    
                    // Select based on priority
                    if (substitute_record) {
                        appropriate_record = substitute_record;
                    } else if (debank_record) {
                        appropriate_record = debank_record;
                    } else if (keep_record) {
                        appropriate_record = keep_record;
                    }
                } else if (stage.stage_name == "BANK") {
                    // In BANK stage, prioritize BANK operations, then SUBSTITUTE, then DEBANK, then KEEP
                    // BUT exclude future operations (LEGALIZATION)
                    const TransformationRecord* bank_record = nullptr;
                    const TransformationRecord* substitute_record = nullptr;
                    const TransformationRecord* debank_record = nullptr;
                    const TransformationRecord* keep_record = nullptr;
                    
                    for (const auto& record : db.transformation_history) {
                        if (record.result_instance_name == instance.instance_name) {
                            // Skip KEEP operations with LEGALIZATION stage info
                            if (record.operation == TransformationRecord::KEEP && 
                                !record.stage.empty() && record.stage == "LEGALIZATION") continue;
                                
                            if (record.operation == TransformationRecord::BANK) {
                                bank_record = &record;
                            } else if (record.operation == TransformationRecord::SUBSTITUTE) {
                                substitute_record = &record;
                            } else if (record.operation == TransformationRecord::DEBANK) {
                                debank_record = &record;
                            } else if (record.operation == TransformationRecord::KEEP) {
                                keep_record = &record;
                            }
                        }
                    }
                    
                    // Select based on priority
                    if (bank_record) {
                        appropriate_record = bank_record;
                    } else if (substitute_record) {
                        appropriate_record = substitute_record;
                    } else if (debank_record) {
                        appropriate_record = debank_record;
                    } else if (keep_record) {
                        appropriate_record = keep_record;
                    }
                } else {
                    // For other stages (LEGALIZE), find the chronologically latest record for this instance
                    for (const auto& record : db.transformation_history) {
                        if (record.result_instance_name == instance.instance_name) {
                            appropriate_record = &record;  // Keep updating to get the latest
                        }
                    }
                }
                
                if (appropriate_record) {
                    out << "      Last operation: " << appropriate_record->operation_string();
                    if (!appropriate_record->stage.empty()) {
                        out << " (" << appropriate_record->stage << ")";
                    }
                    out << std::endl;
                } else {
                    out << "      Last operation: N/A" << std::endl;
                }
                
                out << std::endl;
            }
        } else {
            out << "  (No instances captured for this stage)" << std::endl;
        }
        out << std::endl;
    }
    
    out.close();
    std::cout << "    Transformation report exported successfully" << std::endl;
}


void record_legalization_transformations(DesignDatabase& db) {
    std::cout << "  Recording legalization transformations..." << std::endl;
    
    int legalization_count = 0;
    std::vector<size_t> legalization_indices;
    
    for (const auto& inst_pair : db.instances) {
        auto instance = inst_pair.second;
        if (!instance->is_flip_flop()) continue;
        
        // Check if position changed during legalization
        double displacement = std::sqrt((instance->x_new - instance->position.x) * (instance->x_new - instance->position.x) + 
                                       (instance->y_new - instance->position.y) * (instance->y_new - instance->position.y));
        
        if (displacement > 1e-6) { // Position changed
            TransformationRecord record(instance->name, instance->name, TransformationRecord::KEEP, 
                                      instance->cell_template->name, instance->cell_template->name);
            
            // Set result position (after legalization)
            record.result_x = instance->x_new;
            record.result_y = instance->y_new;
            record.result_orientation = instance->orientation;
            
            record.cluster_id = instance->name;
            record.stage = "LEGALIZATION";
            
            // Pin mapping stays the same for legalization (only position changes)
            // No need to set pin_mapping since it's just position adjustment
            
            db.transformation_history.push_back(record);
            legalization_indices.push_back(db.transformation_history.size() - 1);
            legalization_count++;
            
            // Update instance position to the legalized position
            instance->position.x = instance->x_new;
            instance->position.y = instance->y_new;
        }
    }
    
    std::cout << "    Recorded " << legalization_count << " legalization transformations" << std::endl;
    
    // Capture LEGALIZE stage
    std::vector<std::shared_ptr<Instance>> all_instances_after_legalization;
    for (const auto& inst_pair : db.instances) {
        all_instances_after_legalization.push_back(inst_pair.second);
    }
    
    db.complete_pipeline.capture_stage("LEGALIZE", all_instances_after_legalization, legalization_indices, &db.transformation_history);
    std::cout << "    LEGALIZE stage captured successfully" << std::endl;
}

void generate_contest_output_files(const DesignDatabase& db, const std::string& base_name) {
    std::cout << "\n🎯 Generating ICCAD 2025 Contest Output Files..." << std::endl;
    
    // Generate required contest output files
    generate_pin_mapping_list_file(db, base_name + ".list");
    generate_final_def_file(db, base_name + "_final.def");
    generate_final_verilog_file(db, base_name + "_final.v");
    export_transformation_report(db, base_name + "_transformations.txt");
    
    std::cout << "✅ Contest output files generated successfully!" << std::endl;
}