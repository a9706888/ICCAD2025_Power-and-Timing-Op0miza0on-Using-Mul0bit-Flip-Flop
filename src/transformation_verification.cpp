// =============================================================================
// TRANSFORMATION RECORD VERIFICATION SYSTEM
// =============================================================================
// 這個模組實作驗證函數來檢查TransformationRecord系統的正確性
// 包含各種驗證測試來確保transformation tracking的完整性和正確性
// =============================================================================

#include "parsers.hpp"
#include <iostream>
#include <fstream>
#include <set>
#include <map>

// =============================================================================
// VERIFICATION FUNCTIONS
// =============================================================================

bool verify_transformation_completeness(const DesignDatabase& db) {
    std::cout << "\n🔍 Verifying Transformation Completeness..." << std::endl;
    
    // Get all current FF instances (after all transformations)
    auto current_ffs = db.get_flip_flops();
    std::set<std::string> current_ff_names;
    for (const auto& ff : current_ffs) {
        current_ff_names.insert(ff->name);
    }
    
    // Get all result instances from transformation records
    std::set<std::string> result_ff_names;
    std::set<std::string> original_ff_names; // Track what original FFs were processed
    
    for (const auto& record : db.transformation_history) {
        result_ff_names.insert(record.result_instance_name);
        original_ff_names.insert(record.original_instance_name);
    }
    
    // Check completeness: all current FFs should have transformation records
    bool complete = true;
    std::cout << "  Current FFs in design: " << current_ff_names.size() << std::endl;
    std::cout << "  Result FFs in records: " << result_ff_names.size() << std::endl;
    
    // Find missing result FFs (current FFs not tracked in transformation records)
    std::vector<std::string> missing_result_ffs;
    for (const auto& name : current_ff_names) {
        if (result_ff_names.find(name) == result_ff_names.end()) {
            missing_result_ffs.push_back(name);
            complete = false;
        }
    }
    
    // Find extra result FFs (transformation records for non-existent FFs)
    std::vector<std::string> extra_result_ffs;
    for (const auto& name : result_ff_names) {
        if (current_ff_names.find(name) == current_ff_names.end()) {
            extra_result_ffs.push_back(name);
        }
    }
    
    if (!missing_result_ffs.empty()) {
        std::cout << "  ❌ Current FFs missing from transformation records: " << missing_result_ffs.size() << std::endl;
        for (size_t i = 0; i < std::min(missing_result_ffs.size(), size_t(5)); i++) {
            std::cout << "    - " << missing_result_ffs[i] << std::endl;
        }
        if (missing_result_ffs.size() > 5) {
            std::cout << "    ... +" << (missing_result_ffs.size() - 5) << " more" << std::endl;
        }
    }
    
    if (!extra_result_ffs.empty()) {
        std::cout << "  ⚠️  Result FFs in records but not in current design: " << extra_result_ffs.size() << std::endl;
        for (size_t i = 0; i < std::min(extra_result_ffs.size(), size_t(5)); i++) {
            std::cout << "    - " << extra_result_ffs[i] << std::endl;
        }
    }
    
    if (complete) {
        std::cout << "  ✅ All current FFs are properly tracked in transformation records" << std::endl;
    }
    
    return complete;
}

bool verify_pin_mapping_consistency(const DesignDatabase& db) {
    std::cout << "\n🔍 Verifying Pin Mapping Consistency..." << std::endl;
    
    bool consistent = true;
    int checked_records = 0;
    int inconsistent_records = 0;
    
    for (const auto& record : db.transformation_history) {
        checked_records++;
        
        // For DEBANK operations, the original instance no longer exists - this is expected
        if (record.operation == TransformationRecord::DEBANK) {
            // Check that the result instance exists
            auto result_inst_it = db.instances.find(record.result_instance_name);
            if (result_inst_it == db.instances.end()) {
                std::cout << "  ❌ Result instance not found after DEBANK: " << record.result_instance_name << std::endl;
                consistent = false;
                inconsistent_records++;
            }
            continue; // Skip original instance check for DEBANK - it's expected to be gone
        }
        
        // Find original instance (for KEEP, SUBSTITUTE, BANK operations)
        auto orig_inst_it = db.instances.find(record.original_instance_name);
        if (orig_inst_it == db.instances.end()) {
            std::cout << "  ❌ Original instance not found: " << record.original_instance_name << std::endl;
            consistent = false;
            inconsistent_records++;
            continue;
        }
        
        auto original_instance = orig_inst_it->second;
        
        // For KEEP operations, check that all pins are mapped 1:1
        if (record.operation == TransformationRecord::KEEP) {
            std::set<std::string> original_pins;
            for (const auto& conn : original_instance->connections) {
                original_pins.insert(conn.pin_name);
            }
            
            std::set<std::string> mapped_original_pins;
            std::set<std::string> mapped_result_pins;
            for (const auto& mapping : record.pin_mapping) {
                mapped_original_pins.insert(mapping.first);
                mapped_result_pins.insert(mapping.second);
            }
            
            // Check if all original pins are mapped
            bool all_pins_mapped = true;
            for (const auto& pin : original_pins) {
                if (mapped_original_pins.find(pin) == mapped_original_pins.end()) {
                    std::cout << "  ❌ Missing pin mapping for " << record.original_instance_name 
                              << "." << pin << std::endl;
                    consistent = false;
                    all_pins_mapped = false;
                }
            }
            
            // For KEEP operations, result pins should be identical to original pins
            if (mapped_original_pins != mapped_result_pins) {
                std::cout << "  ❌ KEEP operation with non-identical pin mapping: " 
                          << record.original_instance_name << std::endl;
                consistent = false;
                all_pins_mapped = false;
            }
            
            if (!all_pins_mapped) {
                inconsistent_records++;
            }
        }
    }
    
    std::cout << "  Checked records: " << checked_records << std::endl;
    std::cout << "  Inconsistent records: " << inconsistent_records << std::endl;
    
    if (consistent) {
        std::cout << "  ✅ Pin mapping consistency verified" << std::endl;
    }
    
    return consistent;
}

bool verify_position_information(const DesignDatabase& db) {
    std::cout << "\n🔍 Verifying Position Information..." << std::endl;
    
    bool positions_valid = true;
    int checked_records = 0;
    int invalid_positions = 0;
    
    for (const auto& record : db.transformation_history) {
        checked_records++;
        
        // For KEEP operations, check position consistency with original instance
        if (record.operation == TransformationRecord::KEEP) {
            auto orig_inst_it = db.instances.find(record.original_instance_name);
            if (orig_inst_it == db.instances.end()) {
                continue; // Skip if original instance not found
            }
            
            auto original_instance = orig_inst_it->second;
            if (std::abs(record.result_x - original_instance->position.x) > 0.01 ||
                std::abs(record.result_y - original_instance->position.y) > 0.01) {
                std::cout << "  ❌ Position mismatch for " << record.original_instance_name 
                          << ": original(" << original_instance->position.x << "," << original_instance->position.y 
                          << ") vs result(" << record.result_x << "," << record.result_y << ")" << std::endl;
                positions_valid = false;
                invalid_positions++;
            }
        }
        // For DEBANK operations, check that result instance has valid position
        else if (record.operation == TransformationRecord::DEBANK) {
            auto result_inst_it = db.instances.find(record.result_instance_name);
            if (result_inst_it != db.instances.end()) {
                auto result_instance = result_inst_it->second;
                // Check if recorded position matches actual instance position
                if (std::abs(record.result_x - result_instance->position.x) > 0.01 ||
                    std::abs(record.result_y - result_instance->position.y) > 0.01) {
                    std::cout << "  ❌ Position mismatch for debanked " << record.result_instance_name 
                              << ": recorded(" << record.result_x << "," << record.result_y 
                              << ") vs actual(" << result_instance->position.x << "," << result_instance->position.y << ")" << std::endl;
                    positions_valid = false;
                    invalid_positions++;
                }
            }
        }
    }
    
    std::cout << "  Checked records: " << checked_records << std::endl;
    std::cout << "  Invalid positions: " << invalid_positions << std::endl;
    
    if (positions_valid) {
        std::cout << "  ✅ Position information verified" << std::endl;
    }
    
    return positions_valid;
}

bool verify_output_file_consistency(const DesignDatabase& db, const std::string& base_name) {
    std::cout << "\n🔍 Verifying Output File Consistency..." << std::endl;
    
    bool consistent = true;
    
    // Check if all required files exist
    std::vector<std::string> required_files = {
        base_name + ".list",
        base_name + "_final.def", 
        base_name + "_final.v",
        base_name + "_transformations.txt"
    };
    
    for (const auto& file : required_files) {
        std::ifstream test_file(file);
        if (!test_file.good()) {
            std::cout << "  ❌ Missing output file: " << file << std::endl;
            consistent = false;
        } else {
            std::cout << "  ✅ Found: " << file << std::endl;
        }
    }
    
    // Verify pin mapping list file format
    std::ifstream list_file(base_name + ".list");
    if (list_file.good()) {
        std::string line;
        int pin_mapping_count = 0;
        int malformed_lines = 0;
        
        while (std::getline(list_file, line)) {
            if (line.empty() || line[0] == '#') continue;
            
            // Check format: instance.pin -> instance.pin
            if (line.find(" -> ") != std::string::npos) {
                pin_mapping_count++;
            } else {
                malformed_lines++;
                if (malformed_lines <= 3) {
                    std::cout << "  ❌ Malformed line in .list: " << line << std::endl;
                }
            }
        }
        
        std::cout << "  Pin mappings in .list file: " << pin_mapping_count << std::endl;
        std::cout << "  Malformed lines: " << malformed_lines << std::endl;
        
        if (malformed_lines > 0) {
            consistent = false;
        }
    }
    
    return consistent;
}

bool verify_transformation_logic(const DesignDatabase& db) {
    std::cout << "\n🔍 Verifying Transformation Logic..." << std::endl;
    
    bool logic_valid = true;
    
    // Count operation types
    std::map<TransformationRecord::Operation, int> operation_counts;
    for (const auto& record : db.transformation_history) {
        operation_counts[record.operation]++;
    }
    
    std::cout << "  Operation distribution:" << std::endl;
    std::cout << "    KEEP: " << operation_counts[TransformationRecord::KEEP] << std::endl;
    std::cout << "    SUBSTITUTE: " << operation_counts[TransformationRecord::SUBSTITUTE] << std::endl;
    std::cout << "    DEBANK: " << operation_counts[TransformationRecord::DEBANK] << std::endl;
    std::cout << "    BANK: " << operation_counts[TransformationRecord::BANK] << std::endl;
    
    // For now, we expect mostly KEEP operations since we haven't implemented optimization yet
    if (operation_counts[TransformationRecord::KEEP] == 0 && db.transformation_history.size() > 0) {
        std::cout << "  ⚠️  No KEEP operations found - this might be unexpected for initial implementation" << std::endl;
    }
    
    // Check for logical consistency of operations
    for (const auto& record : db.transformation_history) {
        switch (record.operation) {
            case TransformationRecord::KEEP:
                // For KEEP, original and result should be identical
                if (record.original_instance_name != record.result_instance_name ||
                    record.original_cell_type != record.result_cell_type) {
                    std::cout << "  ❌ KEEP operation with changes: " << record.original_instance_name << std::endl;
                    logic_valid = false;
                }
                break;
                
            case TransformationRecord::SUBSTITUTE:
                // For SUBSTITUTE, instance name same but cell type different
                if (record.original_instance_name != record.result_instance_name ||
                    record.original_cell_type == record.result_cell_type) {
                    std::cout << "  ❌ SUBSTITUTE operation logic error: " << record.original_instance_name << std::endl;
                    logic_valid = false;
                }
                break;
                
            case TransformationRecord::POST_SUBSTITUTE:
                // For POST_SUBSTITUTE, same logic as SUBSTITUTE
                if (record.original_instance_name != record.result_instance_name ||
                    record.original_cell_type == record.result_cell_type) {
                    std::cout << "  ❌ POST_SUBSTITUTE operation logic error: " << record.original_instance_name << std::endl;
                    logic_valid = false;
                }
                break;
                
            case TransformationRecord::DEBANK:
                // For DEBANK, should have related instances
                if (record.related_instances.empty()) {
                    std::cout << "  ❌ DEBANK operation without related instances: " << record.original_instance_name << std::endl;
                    logic_valid = false;
                }
                break;
                
            case TransformationRecord::BANK:
                // For BANK, should have related instances  
                if (record.related_instances.empty()) {
                    std::cout << "  ❌ BANK operation without related instances: " << record.original_instance_name << std::endl;
                    logic_valid = false;
                }
                break;
        }
    }
    
    if (logic_valid) {
        std::cout << "  ✅ Transformation logic verified" << std::endl;
    }
    
    return logic_valid;
}

// =============================================================================
// MAIN VERIFICATION FUNCTION
// =============================================================================

bool run_transformation_verification(const DesignDatabase& db, const std::string& output_base_name) {
    std::cout << "\n🧪 === TRANSFORMATION RECORD VERIFICATION SUITE ===" << std::endl;
    std::cout << "Design: " << db.design_name << std::endl;
    std::cout << "Total transformation records: " << db.transformation_history.size() << std::endl;
    
    bool all_tests_passed = true;
    
    // Run all verification tests
    all_tests_passed &= verify_transformation_completeness(db);
    all_tests_passed &= verify_pin_mapping_consistency(db);  
    all_tests_passed &= verify_position_information(db);
    all_tests_passed &= verify_transformation_logic(db);
    all_tests_passed &= verify_output_file_consistency(db, output_base_name);
    
    // Final result
    std::cout << "\n🏆 === VERIFICATION SUMMARY ===" << std::endl;
    if (all_tests_passed) {
        std::cout << "✅ ALL VERIFICATION TESTS PASSED" << std::endl;
        std::cout << "TransformationRecord system is working correctly!" << std::endl;
    } else {
        std::cout << "❌ SOME VERIFICATION TESTS FAILED" << std::endl;
        std::cout << "Please review the errors above and fix the issues." << std::endl;
    }
    
    return all_tests_passed;
}

void export_transformation_verification_report(const DesignDatabase& db, const std::string& output_file) {
    std::cout << "\n📋 Exporting verification report to: " << output_file << std::endl;
    
    std::ofstream out(output_file);
    if (!out.is_open()) {
        std::cerr << "Error: Cannot open " << output_file << " for writing" << std::endl;
        return;
    }
    
    out << "=== TRANSFORMATION RECORD VERIFICATION REPORT ===" << std::endl;
    out << "Generated: " << __DATE__ << " " << __TIME__ << std::endl;
    out << "Design: " << db.design_name << std::endl;
    out << std::endl;
    
    // Summary statistics
    std::map<TransformationRecord::Operation, int> op_counts;
    for (const auto& record : db.transformation_history) {
        op_counts[record.operation]++;
    }
    
    out << "=== TRANSFORMATION STATISTICS ===" << std::endl;
    out << "Total records: " << db.transformation_history.size() << std::endl;
    out << "KEEP operations: " << op_counts[TransformationRecord::KEEP] << std::endl;
    out << "SUBSTITUTE operations: " << op_counts[TransformationRecord::SUBSTITUTE] << std::endl;
    out << "DEBANK operations: " << op_counts[TransformationRecord::DEBANK] << std::endl;
    out << "BANK operations: " << op_counts[TransformationRecord::BANK] << std::endl;
    out << std::endl;
    
    // Detailed analysis
    out << "=== DETAILED ANALYSIS ===" << std::endl;
    
    // Pin mapping analysis
    int total_pin_mappings = 0;
    for (const auto& record : db.transformation_history) {
        total_pin_mappings += record.pin_mapping.size();
    }
    out << "Total pin mappings: " << total_pin_mappings << std::endl;
    
    // Position analysis
    int valid_positions = 0;
    for (const auto& record : db.transformation_history) {
        if (record.result_x != 0.0 || record.result_y != 0.0) {
            valid_positions++;
        }
    }
    out << "Records with valid positions: " << valid_positions << std::endl;
    
    out.close();
    std::cout << "    Verification report exported successfully" << std::endl;
}