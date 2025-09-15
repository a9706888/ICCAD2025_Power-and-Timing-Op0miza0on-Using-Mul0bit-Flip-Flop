#include "parsers.hpp"
#include "timing_repr_hardcoded.hpp"
#include <iostream>
#include <algorithm>
#include <limits>
#include <iomanip>
#include <fstream>
#include <ctime>
#include <set>

// =============================================================================
// THREE-STAGE FF SUBSTITUTION STRATEGY
// =============================================================================

// Calculate score for a specific FF type using the thesis formula
double calculate_ff_score(const std::string& cell_name, const DesignDatabase& db) {
    auto cell_it = db.cell_library.find(cell_name);
    if (cell_it == db.cell_library.end()) {
        return std::numeric_limits<double>::max();
    }
    
    auto cell = cell_it->second;
    if (!cell->is_flip_flop()) {
        return std::numeric_limits<double>::max();
    }
    
    // Use thesis formula: Score = (β·Power + γ·Area)/bit + δ
    double power = cell->leakage_power;
    double area = cell->area;
    int bit_width = std::max(1, cell->bit_width);
    
    // Get timing_repr from hardcoded map and multiply by alpha
    double timing_repr = TimingReprMap::get_timing_repr(cell_name);
    double delta = db.objective_weights.alpha * timing_repr;
    
    // Score calculation using thesis formula
    double score = (db.objective_weights.beta * power * 0.001 + 
                   db.objective_weights.gamma * area) / 
                   static_cast<double>(bit_width) + delta;
    
    return score;
}

// Update instance's best FF record if current FF is better
void update_best_ff_record(std::shared_ptr<Instance> instance, 
                           const std::string& ff_name, 
                           const DesignDatabase& db) {
    double score = calculate_ff_score(ff_name, db);
    if (score < instance->best_ff_score) {
        instance->best_ff_from_substitution = ff_name;
        instance->best_ff_score = score;
        
        // Debug output (can be removed later)
        // std::cout << "    Updated best FF for " << instance->name 
        //          << ": " << ff_name << " (score: " << score << ")" << std::endl;
    }
}

// Get hierarchical group key from instance's cell template
std::string get_hierarchical_key_from_instance(std::shared_ptr<Instance> instance, const DesignDatabase& db) {
    if (!instance || !instance->cell_template) {
        return "";
    }
    
    // Get the cell's name
    std::string cell_name = instance->cell_template->name;
    
    // Search in hierarchical_ff_groups to find which group this cell belongs to
    for (const auto& edge_pair : db.hierarchical_ff_groups) {
        for (const auto& pin_pair : edge_pair.second) {
            for (const auto& bit_pair : pin_pair.second) {
                // Check if this cell is in this group
                const auto& cell_list = bit_pair.second;
                if (std::find(cell_list.begin(), cell_list.end(), cell_name) != cell_list.end()) {
                    // Found the group this cell belongs to
                    return edge_pair.first + "|" + pin_pair.first + "|" + std::to_string(bit_pair.first) + "bit";
                }
            }
        }
    }
    
    return "";  // Not found
}

// =============================================================================
// STAGE 1: ORIGINAL PIN PATTERN SUBSTITUTION
// =============================================================================

void execute_stage1_substitution(DesignDatabase& db) {
    std::cout << "\n🔄 Stage 1: Original Pin Pattern Substitution..." << std::endl;
    std::cout << "  Strategy: Replace each instance with optimal FF for its cell template's compatibility group" << std::endl;
    
//     std::ofstream stage1_report("stage1_substitution_report.txt");
    // if (!stage1_report.is_open()) {
    //     std::cout << "    ERROR: Cannot create stage1_substitution_report.txt" << std::endl;
    //     return;
    // }
    
// stage1_report << "=== STAGE 1: ORIGINAL PIN PATTERN SUBSTITUTION REPORT ===" << std::endl;
    std::time_t now = std::time(nullptr);
// stage1_report << "Generated: " << std::asctime(std::localtime(&now));
// stage1_report << std::endl;
    
    int total_groups = db.ff_instance_groups.size();
    int successful_groups = 0;
    int failed_groups = 0;
    int total_instances_processed = 0;
    int total_instances_substituted = 0;
    
    std::cout << "  Processing " << total_groups << " FF instance groups..." << std::endl;
// stage1_report << "Processing " << total_groups << " FF instance groups..." << std::endl << std::endl;
    
    // Process each ff_instance_group
    for (auto& group_pair : db.ff_instance_groups) {
        const std::string& group_key = group_pair.first;
        std::vector<std::shared_ptr<Instance>>& group_instances = group_pair.second;
        
        if (group_instances.empty()) {
            continue;
        }
        
        total_instances_processed += group_instances.size();
        
// stage1_report << "=== GROUP: " << group_key << " (" << group_instances.size() << " instances) ===" << std::endl;
        std::cout << "    Processing group: " << group_key << " (" << group_instances.size() << " instances)" << std::endl;
        
        // Analyze current group composition and find unique cell templates
        std::map<std::string, int> current_composition;
        std::set<std::string> unique_hierarchical_keys;
        
        for (const auto& instance : group_instances) {
            current_composition[instance->cell_template->name]++;
            
            // Find hierarchical key for this instance
            std::string hierarchical_key = get_hierarchical_key_from_instance(instance, db);
            if (!hierarchical_key.empty()) {
                unique_hierarchical_keys.insert(hierarchical_key);
            }
        }
        
// stage1_report << "Current composition:" << std::endl;
        for (const auto& comp_pair : current_composition) {
            double score = calculate_ff_score(comp_pair.first, db);
// stage1_report << "  " << comp_pair.second << "x " << comp_pair.first 
//                          << " (score: " << std::fixed << std::setprecision(6) << score << ")" << std::endl;
        }
        
// stage1_report << "Unique compatibility groups found: " << unique_hierarchical_keys.size() << std::endl;
        for (const auto& key : unique_hierarchical_keys) {
// stage1_report << "  " << key << std::endl;
        }
        
        // Process instances individually based on their cell template's group
        int instance_substitutions = 0;
// stage1_report << "Instance substitution details:" << std::endl;
// stage1_report.flush(); // Force write header
        
        for (auto& instance : group_instances) {
            std::string hierarchical_key = get_hierarchical_key_from_instance(instance, db);
            
            if (hierarchical_key.empty()) {
// stage1_report << "  " << instance->name << ": ERROR - Cannot find hierarchical key (cell: " 
//                              << instance->cell_template->name << ")" << std::endl;
// stage1_report.flush(); // Force write error
                continue;
            }
            
            // Look up optimal FF for this instance's compatibility group
            auto optimal_it = db.optimal_ff_for_groups.find(hierarchical_key);
            if (optimal_it == db.optimal_ff_for_groups.end()) {
// stage1_report << "  " << instance->name << ": ERROR - No optimal FF found for group: " 
//                              << hierarchical_key << std::endl;
// stage1_report.flush(); // Force write error
                continue;
            }
            
            std::string optimal_ff_name = optimal_it->second;
            auto optimal_cell_it = db.cell_library.find(optimal_ff_name);
            if (optimal_cell_it == db.cell_library.end()) {
// stage1_report << "  " << instance->name << ": ERROR - Optimal FF " << optimal_ff_name 
//                              << " not found in cell library" << std::endl;
// stage1_report.flush(); // Force write error
                continue;
            }
            
            // Always show what happens to each instance
            std::string original_cell_name = instance->cell_template->name;
            if (original_cell_name != optimal_ff_name) {
                // Execute substitution
                instance->cell_template = optimal_cell_it->second;
                instance_substitutions++;
                
                // Update best FF record
                update_best_ff_record(instance, optimal_ff_name, db);
                
// stage1_report << "  " << instance->name << ": " << original_cell_name 
//                              << " → " << optimal_ff_name << " (SUBSTITUTED)" << std::endl;
                
                // Note: Do not record SUBSTITUTE here - will be recorded at the end based on final result
            } else {
// stage1_report << "  " << instance->name << ": " << original_cell_name 
//                              << " (already optimal)" << std::endl;
            }
            
            // Force write every 100 instances to prevent buffer issues
            static int count = 0;
            count++;
            if (count % 100 == 0) {
// stage1_report.flush();
            }
        }
        
        if (instance_substitutions > 0) {
            successful_groups++;
            total_instances_substituted += instance_substitutions;
// stage1_report << "SUBSTITUTION EXECUTED: " << instance_substitutions 
//                          << " instances substituted with their optimal FFs" << std::endl;
            std::cout << "      SUBSTITUTED: " << instance_substitutions << " instances" << std::endl;
        } else {
            successful_groups++;
// stage1_report << "NO SUBSTITUTION NEEDED: All instances already using optimal FFs" << std::endl;
            std::cout << "      NO CHANGE: All instances already optimal" << std::endl;
        }
        
// stage1_report << std::endl;
    }
    
    // Summary statistics
    std::cout << "  Stage 1 Summary:" << std::endl;
    std::cout << "    Total groups: " << total_groups << std::endl;
    std::cout << "    Successful groups: " << successful_groups << std::endl;
    std::cout << "    Failed groups: " << failed_groups << std::endl;
    std::cout << "    Total instances processed: " << total_instances_processed << std::endl;
    std::cout << "    Total instances substituted: " << total_instances_substituted << std::endl;
    
// stage1_report << "=== STAGE 1 SUMMARY ===" << std::endl;
// stage1_report << "Total groups: " << total_groups << std::endl;
// stage1_report << "Successful groups: " << successful_groups << std::endl;
// stage1_report << "Failed groups: " << failed_groups << std::endl;
// stage1_report << "Total instances processed: " << total_instances_processed << std::endl;
// stage1_report << "Total instances substituted: " << total_instances_substituted << std::endl;
    
// stage1_report.close();
    // std::cout << "    Report generated: stage1_substitution_report.txt" << std::endl;
}

// =============================================================================
// STAGE 2: EFFECTIVE PIN CONNECTIONS SUBSTITUTION
// =============================================================================

// Get effective pin pattern by filtering out UNCONNECTED/VSS pins
std::string get_effective_pin_pattern(std::shared_ptr<Instance> instance, const DesignDatabase& db) {
    if (!instance || !instance->cell_template) {
        return "";
    }
    
    std::vector<std::string> active_pins;
    
    // Check each connection to see if it's effectively connected
    for (const auto& connection : instance->connections) {
        // Skip UNCONNECTED and VSS connections (treat as inactive)
        if (connection.net_name == "UNCONNECTED" || connection.net_name == "VSS") {
            continue;
        }
        
        // Find the pin in cell template to get its FF pin type
        Pin* pin = instance->cell_template->find_pin(connection.pin_name);
        if (!pin) continue;
        
        // Add active pin based on FF pin type
        if (pin->ff_pin_type == Pin::FlipFlopPinType::FF_DATA_INPUT) {
            active_pins.push_back("D");
        } else if (pin->ff_pin_type == Pin::FlipFlopPinType::FF_DATA_OUTPUT) {
            active_pins.push_back("Q");
        } else if (pin->ff_pin_type == Pin::FlipFlopPinType::FF_DATA_OUTPUT_N) {
            active_pins.push_back("QN");
        } else if (pin->ff_pin_type == Pin::FlipFlopPinType::FF_CLOCK) {
            active_pins.push_back("CK");
        } else if (pin->ff_pin_type == Pin::FlipFlopPinType::FF_SCAN_INPUT) {
            active_pins.push_back("SI");
        } else if (pin->ff_pin_type == Pin::FlipFlopPinType::FF_SCAN_ENABLE) {
            active_pins.push_back("SE");
        } else if (pin->ff_pin_type == Pin::FlipFlopPinType::FF_RESET) {
            active_pins.push_back("R");
        } else if (pin->ff_pin_type == Pin::FlipFlopPinType::FF_RD) {
            active_pins.push_back("RD");
        } else if (pin->ff_pin_type == Pin::FlipFlopPinType::FF_SET) {
            active_pins.push_back("S");
        } else if (pin->ff_pin_type == Pin::FlipFlopPinType::FF_SD) {
            active_pins.push_back("SD");
        } else if (pin->ff_pin_type == Pin::FlipFlopPinType::FF_RS) {
            active_pins.push_back("RS");
        } else if (pin->ff_pin_type == Pin::FlipFlopPinType::FF_SR) {
            active_pins.push_back("SR");
        }
    }
    
    // Create pattern in logical order (not alphabetical) to match hierarchical group keys
    // Use the same order as hierarchical FF groups: D, Q, QN, CK, SI, SE, R, RD, S, SD, RS, SR
    std::vector<std::string> ordered_pins;
    std::vector<std::string> pin_order = {"D", "Q", "QN", "CK", "SI", "SE", "R", "RD", "S", "SD", "RS", "SR"};
    
    for (const std::string& pin_type : pin_order) {
        if (std::find(active_pins.begin(), active_pins.end(), pin_type) != active_pins.end()) {
            ordered_pins.push_back(pin_type);
        }
    }
    
    std::string pattern = "";
    for (size_t i = 0; i < ordered_pins.size(); ++i) {
        if (i > 0) pattern += "_";
        pattern += ordered_pins[i];
    }
    
    return pattern;
}

// Convert effective pattern to hierarchical key
std::string convert_effective_pattern_to_hierarchical_key(const std::string& effective_pattern, 
                                                          const std::string& clock_edge) {
    if (effective_pattern.empty() || clock_edge.empty()) {
        return "";
    }
    
    return clock_edge + "|" + effective_pattern + "|1bit";
}


void execute_stage2_substitution(DesignDatabase& db) {
    std::cout << "\n🔄 Stage 2: Effective Pin Connections Substitution..." << std::endl;
    std::cout << "  Strategy: Conditional substitution based on effective pin connections (only if better)" << std::endl;
    
//     std::ofstream stage2_report("stage2_substitution_report.txt");
    // if (!stage2_report.is_open()) {
    //     std::cout << "    ERROR: Cannot create stage2_substitution_report.txt" << std::endl;
    //     return;
    // }
    
// stage2_report << "=== STAGE 2: EFFECTIVE PIN CONNECTIONS SUBSTITUTION REPORT ===" << std::endl;
    std::time_t now = std::time(nullptr);
// stage2_report << "Generated: " << std::asctime(std::localtime(&now));
// stage2_report << std::endl;
    
    int total_instances_processed = 0;
    int total_instances_substituted = 0;
    
    // Process all FF instances across all groups
    for (auto& group_pair : db.ff_instance_groups) {
        std::vector<std::shared_ptr<Instance>>& group_instances = group_pair.second;
        
        for (auto& instance : group_instances) {
            total_instances_processed++;
            
            // Get current instance score
            double current_score = calculate_ff_score(instance->cell_template->name, db);
            
            // Analyze effective pin connections
            std::string effective_pattern = get_effective_pin_pattern(instance, db);
            std::string clock_edge = get_instance_clock_edge(instance, db);
            std::string effective_key = convert_effective_pattern_to_hierarchical_key(effective_pattern, clock_edge);
            
            
            if (effective_key.empty()) {
                continue; // Skip if cannot determine effective key
            }
            
            // Find optimal FF for effective pattern group
            auto optimal_it = db.optimal_ff_for_groups.find(effective_key);
            if (optimal_it != db.optimal_ff_for_groups.end()) {
                std::string optimal_ff_name = optimal_it->second;
                double optimal_score = calculate_ff_score(optimal_ff_name, db);
                
                
                // Conditional substitution: only if effective pattern's optimal FF is better
                if (optimal_score < current_score) {
                    auto optimal_cell_it = db.cell_library.find(optimal_ff_name);
                    if (optimal_cell_it != db.cell_library.end()) {
                        // Execute substitution
                        std::string original_cell_name = instance->cell_template->name;
                        instance->cell_template = optimal_cell_it->second;
                        total_instances_substituted++;
                        
                        // Update best FF record
                        update_best_ff_record(instance, optimal_ff_name, db);
                        
                        // Record substitution
// stage2_report << "  " << instance->name << ": " << original_cell_name 
//                                      << " → " << optimal_ff_name << std::endl;
                        
                        // Note: Do not record SUBSTITUTE here - will be recorded at the end based on final result
                    }
                }
            }
            
            // Force write every 100 instances
            static int count = 0;
            count++;
            if (count % 100 == 0) {
// stage2_report.flush();
            }
        }
    }
    
    // Summary
    std::cout << "  Stage 2 Summary:" << std::endl;
    std::cout << "    Total instances processed: " << total_instances_processed << std::endl;
    std::cout << "    Total instances substituted: " << total_instances_substituted << std::endl;
    
// stage2_report << std::endl << "=== STAGE 2 SUMMARY ===" << std::endl;
// stage2_report << "Total instances processed: " << total_instances_processed << std::endl;
// stage2_report << "Total instances substituted: " << total_instances_substituted << std::endl;
    
// stage2_report.close();
    // std::cout << "    Report generated: stage2_substitution_report.txt" << std::endl;
}

// =============================================================================
// STAGE 3: FALLING EDGE MBFF BANKING PREPARATION
// =============================================================================

// Check if instance has RD/SD pins that would prevent MBFF banking
bool has_rd_sd_pins(std::shared_ptr<Instance> instance) {
    if (!instance || !instance->cell_template) {
        return false;
    }
    
    for (const auto& connection : instance->connections) {
        // Skip UNCONNECTED pins
        if (connection.net_name == "UNCONNECTED" || connection.net_name == "VSS") {
            continue;
        }
        
        // Find the pin in cell template to get its FF pin type
        Pin* pin = instance->cell_template->find_pin(connection.pin_name);
        if (!pin) continue;
        
        // Check if this is an active RD or SD pin
        if (pin->ff_pin_type == Pin::FlipFlopPinType::FF_RD || 
            pin->ff_pin_type == Pin::FlipFlopPinType::FF_SD) {
            return true;
        }
    }
    
    return false;
}

// Find the best FSDN4 target across all libraries
std::string find_best_fsdn4_target(const DesignDatabase& db) {
    auto fsdn4_it = db.optimal_ff_for_groups.find("FALLING|D_Q_QN_CK_SI_SE|4bit");
    if (fsdn4_it != db.optimal_ff_for_groups.end()) {
        return fsdn4_it->second;
    }
    
    return ""; // No FSDN4 found
}

// Find the best LSRDPQ4 target across all libraries
std::string find_best_lsrdpq4_target(const DesignDatabase& db) {
    auto lsrdpq4_it = db.optimal_ff_for_groups.find("RISING|D_Q_QN_CK|4bit");
    if (lsrdpq4_it != db.optimal_ff_for_groups.end()) {
        return lsrdpq4_it->second;
    }
    
    return ""; // No LSRDPQ4 found
}

// Check if a RISING edge instance is eligible for LSRDPQ4 banking preparation
// Must be in D_Q_CK or D_QN_CK groups (can upgrade to D_Q_QN_CK pattern)
bool is_eligible_for_lsrdpq4_banking(std::shared_ptr<Instance> instance, const DesignDatabase& db) {
    if (!instance || !instance->cell_template) {
        return false;
    }
    
    std::string effective_pattern = get_effective_pin_pattern(instance, db);
    std::string clock_edge = get_instance_clock_edge(instance, db);
    
    if (clock_edge != "RISING") {
        return false;
    }
    
    // Convert to hierarchical key format for comparison
    std::string hierarchical_key = convert_effective_pattern_to_hierarchical_key(effective_pattern, clock_edge);
    
    // Check if in D_Q_CK or D_QN_CK groups (both can upgrade to D_Q_QN_CK pattern for LSRDPQ4)
    // D_Q_CK: has D,Q,CK pins, missing QN -> can add QN to become D_Q_QN_CK
    // D_QN_CK: has D,QN,CK pins, missing Q -> can add Q to become D_Q_QN_CK
    return (hierarchical_key == "RISING|D_Q_CK|1bit" || 
            hierarchical_key == "RISING|D_QN_CK|1bit");
}

void execute_stage3_substitution(DesignDatabase& db) {
    std::cout << "\n🔄 Stage 3: FALLING & RISING Edge MBFF Banking Preparation..." << std::endl;
    std::cout << "  Strategy: Prepare FFs for banking by substituting to optimal single-bit variants" << std::endl;
    std::cout << "    - FALLING edge: FSDN4 banking preparation" << std::endl;
    std::cout << "    - RISING edge: LSRDPQ4 banking preparation" << std::endl;
    
//     std::ofstream stage3_report("stage3_substitution_report.txt");
    // if (!stage3_report.is_open()) {
    //     std::cout << "    ERROR: Cannot create stage3_substitution_report.txt" << std::endl;
    //     return;
    // }
    
// stage3_report << "=== STAGE 3: FALLING & RISING EDGE MBFF BANKING PREPARATION REPORT ===" << std::endl;
    std::time_t now = std::time(nullptr);
// stage3_report << "Generated: " << std::asctime(std::localtime(&now));
// stage3_report << std::endl;
    
    // Step 1: Find best FSDN4 target (FALLING edge)
    std::string best_fsdn4 = find_best_fsdn4_target(db);
    double fsdn4_score = 0.0;
    std::string optimal_single_fsdn = "";
    double single_fsdn_score = 0.0;
    bool fsdn4_available = false;
    
    if (!best_fsdn4.empty()) {
        fsdn4_score = calculate_ff_score(best_fsdn4, db);
// stage3_report << "FALLING Edge Analysis:" << std::endl;
// stage3_report << "  Best FSDN4 target: " << best_fsdn4 << " (score: " << std::fixed << std::setprecision(6) << fsdn4_score << ")" << std::endl;
        
        // Find optimal single-bit FSDN
        auto single_it = db.optimal_ff_for_groups.find("FALLING|D_Q_QN_CK_SI_SE|1bit");
        if (single_it != db.optimal_ff_for_groups.end()) {
            optimal_single_fsdn = single_it->second;
            single_fsdn_score = calculate_ff_score(optimal_single_fsdn, db);
// stage3_report << "  Optimal single FSDN: " << optimal_single_fsdn << " (score: " << std::fixed << std::setprecision(6) << single_fsdn_score << ")" << std::endl;
            fsdn4_available = true;
        } else {
// stage3_report << "  No optimal single-bit FSDN found" << std::endl;
        }
    } else {
// stage3_report << "FALLING Edge Analysis: No FSDN4 target found" << std::endl;
    }
// stage3_report << std::endl;
    
    // Step 2: Find best LSRDPQ4 target (RISING edge)
    std::string best_lsrdpq4 = find_best_lsrdpq4_target(db);
    double lsrdpq4_score = 0.0;
    std::string optimal_single_lsrdpq = "";
    double single_lsrdpq_score = 0.0;
    bool lsrdpq4_available = false;
    
    if (!best_lsrdpq4.empty()) {
        lsrdpq4_score = calculate_ff_score(best_lsrdpq4, db);
// stage3_report << "RISING Edge Analysis:" << std::endl;
// stage3_report << "  Best LSRDPQ4 target: " << best_lsrdpq4 << " (score: " << std::fixed << std::setprecision(6) << lsrdpq4_score << ")" << std::endl;
        
        // Find optimal single-bit LSRDPQ (from the target D_Q_QN_CK group)
        auto single_lsrdpq_it = db.optimal_ff_for_groups.find("RISING|D_Q_QN_CK|1bit");
        if (single_lsrdpq_it != db.optimal_ff_for_groups.end()) {
            optimal_single_lsrdpq = single_lsrdpq_it->second;
            single_lsrdpq_score = calculate_ff_score(optimal_single_lsrdpq, db);
// stage3_report << "  Optimal single LSRDPQ: " << optimal_single_lsrdpq << " (score: " << std::fixed << std::setprecision(6) << single_lsrdpq_score << ")" << std::endl;
            lsrdpq4_available = true;
        } else {
// stage3_report << "  No optimal single-bit LSRDPQ found" << std::endl;
        }
    } else {
// stage3_report << "RISING Edge Analysis: No LSRDPQ4 target found" << std::endl;
    }
// stage3_report << std::endl;
    
    // Early exit if no targets available
    if (!fsdn4_available && !lsrdpq4_available) {
// stage3_report << "No MBFF targets found for either FALLING or RISING edge. Skipping Stage 3." << std::endl;
// stage3_report.close();
        std::cout << "    No MBFF targets found. Skipping Stage 3." << std::endl;
        return;
    }
    
    int total_instances_processed = 0;
    int total_instances_substituted = 0;
    int falling_substituted = 0;
    int rising_substituted = 0;
    
    // Step 3: Process all FF instances
    for (auto& group_pair : db.ff_instance_groups) {
        std::vector<std::shared_ptr<Instance>>& group_instances = group_pair.second;
        
        for (auto& instance : group_instances) {
            total_instances_processed++;
            
            std::string clock_edge = get_instance_clock_edge(instance, db);
            double current_score = calculate_ff_score(instance->cell_template->name, db);
            
            // Handle FALLING edge FFs for FSDN4 banking preparation
            if (clock_edge == "FALLING" && fsdn4_available) {
                // Exclude FFs with RD/SD pins (MBFF typically don't have these)
                if (has_rd_sd_pins(instance)) {
                    continue;
                }
                
                // Check if FSDN4 target is better than current instance
                if (fsdn4_score < current_score) {
                    // Substitute to optimal single-bit FSDN for banking preparation
                    auto optimal_cell_it = db.cell_library.find(optimal_single_fsdn);
                    if (optimal_cell_it != db.cell_library.end()) {
                        std::string original_cell_name = instance->cell_template->name;
                        instance->cell_template = optimal_cell_it->second;
                        total_instances_substituted++;
                        falling_substituted++;
                        
                        // Update best FF record
                        update_best_ff_record(instance, optimal_single_fsdn, db);
                        
                        // Record substitution
// stage3_report << "  FALLING: " << instance->name << ": " << original_cell_name 
//                                      << " → " << optimal_single_fsdn << " (FSDN4 prep)" << std::endl;
                        
                        // Note: Do not record SUBSTITUTE here - will be recorded at the end based on final result
                    }
                }
            }
            // Handle RISING edge FFs for LSRDPQ4 banking preparation
            else if (clock_edge == "RISING" && lsrdpq4_available) {
                // Check if this FF is eligible for LSRDPQ4 banking (D_Q_CK or D_QN_CK groups)
                // D_Q_CK: has D,Q,CK, missing QN -> can upgrade to D_Q_QN_CK pattern
                // D_QN_CK: has D,QN,CK, missing Q -> can upgrade to D_Q_QN_CK pattern
                if (!is_eligible_for_lsrdpq4_banking(instance, db)) {
                    continue;
                }
                
                // Check if LSRDPQ4 target is better than current instance
                if (lsrdpq4_score < current_score) {
                    // Substitute to optimal single-bit FF from D_Q_QN_CK group for banking preparation
                    auto optimal_cell_it = db.cell_library.find(optimal_single_lsrdpq);
                    if (optimal_cell_it != db.cell_library.end()) {
                        std::string original_cell_name = instance->cell_template->name;
                        instance->cell_template = optimal_cell_it->second;
                        total_instances_substituted++;
                        rising_substituted++;
                        
                        // Update best FF record
                        update_best_ff_record(instance, optimal_single_lsrdpq, db);
                        
                        // Record substitution
// stage3_report << "  RISING: " << instance->name << ": " << original_cell_name 
//                                      << " → " << optimal_single_lsrdpq << " (LSRDPQ4 prep)" << std::endl;
                        
                        // Note: Do not record SUBSTITUTE here - will be recorded at the end based on final result
                    }
                }
            }
            
            // Force write every 100 instances
            static int count = 0;
            count++;
            if (count % 100 == 0) {
// stage3_report.flush();
            }
        }
    }
    
    // Summary
    std::cout << "  Stage 3 Summary:" << std::endl;
    std::cout << "    Total instances processed: " << total_instances_processed << std::endl;
    std::cout << "    Total instances substituted: " << total_instances_substituted << std::endl;
    std::cout << "      FALLING edge (FSDN4 prep): " << falling_substituted << std::endl;
    std::cout << "      RISING edge (LSRDPQ4 prep): " << rising_substituted << std::endl;
    
// stage3_report << std::endl << "=== STAGE 3 SUMMARY ===" << std::endl;
// stage3_report << "Total instances processed: " << total_instances_processed << std::endl;
// stage3_report << "Total instances substituted: " << total_instances_substituted << std::endl;
// stage3_report << "  FALLING edge (FSDN4 preparation): " << falling_substituted << std::endl;
// stage3_report << "  RISING edge (LSRDPQ4 preparation): " << rising_substituted << std::endl;
    
// stage3_report.close();
    // std::cout << "    Report generated: stage3_substitution_report.txt" << std::endl;
}

// =============================================================================
// FINAL SUBSTITUTION RECORDING
// =============================================================================

// Create a map to store original cell types before any substitution
std::map<std::string, std::string> original_cell_types;

void record_final_substitution_operations(DesignDatabase& db) {
    int substitution_count = 0;
    
    // Compare current cell types with original cell types
    for (const auto& inst_pair : db.instances) {
        auto instance = inst_pair.second;
        if (!instance->is_flip_flop()) continue;
        
        // Find original cell type
        auto original_it = original_cell_types.find(instance->name);
        if (original_it == original_cell_types.end()) {
            continue; // Skip if no original record found
        }
        
        std::string original_cell_name = original_it->second;
        std::string current_cell_name = instance->cell_template->name;
        
        // Only record SUBSTITUTE if cell type actually changed
        if (original_cell_name != current_cell_name) {
            // Use the updated record_substitute_transformation function
            record_substitute_transformation(db, instance->name, original_cell_name, current_cell_name);
            substitution_count++;
        }
    }
    
    std::cout << "    Recorded " << substitution_count << " actual SUBSTITUTE operations" << std::endl;
}

// =============================================================================
// MAIN THREE-STAGE SUBSTITUTION ORCHESTRATOR
// =============================================================================

void execute_three_stage_substitution(DesignDatabase& db) {
    std::cout << "\n🎯 Executing Three-Stage FF Substitution Strategy..." << std::endl;
    
    // Store original cell types before any substitution
    std::cout << "  Recording original cell types..." << std::endl;
    original_cell_types.clear();
    for (const auto& inst_pair : db.instances) {
        auto instance = inst_pair.second;
        if (instance->is_flip_flop()) {
            original_cell_types[instance->name] = instance->cell_template->name;
        }
    }
    std::cout << "    Recorded " << original_cell_types.size() << " original FF cell types" << std::endl;
    
    // Execute Stage 1: Original Pin Pattern Substitution (UNCONDITIONAL)
    execute_stage1_substitution(db);
    
    // Execute Stage 2: Effective Pin Connections Substitution (CONDITIONAL)
    execute_stage2_substitution(db);
    
    // Execute Stage 3: FALLING Edge MBFF Banking Preparation (CONDITIONAL)
    execute_stage3_substitution(db);
    
    // Record final SUBSTITUTE operations based on actual changes
    std::cout << "  Recording final SUBSTITUTE operations..." << std::endl;
    record_final_substitution_operations(db);
    
    // Capture final substitution result
    std::cout << "  Capturing SUBSTITUTION stage..." << std::endl;
    std::vector<std::shared_ptr<Instance>> all_instances_after_substitution;
    for (const auto& inst_pair : db.instances) {
        all_instances_after_substitution.push_back(inst_pair.second);
    }
    
    // Get indices of SUBSTITUTION transformation records
    std::vector<size_t> substitution_indices;
    for (size_t i = 0; i < db.transformation_history.size(); ++i) {
        if (db.transformation_history[i].operation == TransformationRecord::SUBSTITUTE) {
            substitution_indices.push_back(i);
        }
    }
    
    std::cout << "    Found " << substitution_indices.size() << " SUBSTITUTE transformation records" << std::endl;
    
    db.complete_pipeline.capture_stage("SUBSTITUTION", all_instances_after_substitution, substitution_indices, &db.transformation_history);
    
    std::cout << "\n✅ Three-Stage Substitution Completed!" << std::endl;
}

// =============================================================================
// POST-BANKING SUBSTITUTION FOR REMAINING SBFFs
// =============================================================================

// Check if an instance is a single-bit flip-flop
bool is_single_bit_ff(std::shared_ptr<Instance> instance) {
    if (!instance || !instance->is_flip_flop()) {
        return false;
    }
    
    // Check bit width from cell template
    int bit_width = instance->get_bit_width();
    return (bit_width == 1);
}

// Record post-banking substitution transformation
void record_post_substitution_transformation(DesignDatabase& db, 
                                            std::shared_ptr<Instance> instance,
                                            const std::string& old_ff,
                                            const std::string& new_ff) {
    // Create POST_SUBSTITUTE transformation record
    TransformationRecord record(
        instance->name,                         // original_instance_name
        instance->name,                         // result_instance_name (same instance, different cell)
        TransformationRecord::POST_SUBSTITUTE, // operation
        old_ff,                                 // original_cell_type
        new_ff                                  // result_cell_type
    );
    record.stage = "POST_BANKING";
    
    // Record pin mapping (1:1 for post-substitutions - same pin names)
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
            record.pin_mapping[pin] = pin;  // 1:1 mapping for substitution
        }
    }
    
    // Record position
    record.result_x = instance->position.x;
    record.result_y = instance->position.y;
    record.result_orientation = "N";  // Default orientation
    
    db.transformation_history.push_back(record);
}

// Execute post-banking substitution for remaining single-bit FFs
void execute_post_banking_substitution(DesignDatabase& db) {
    std::cout << "\n🔄 Post-Banking SBFF Substitution..." << std::endl;
    std::cout << "  Strategy: Revert remaining SBFFs to best substitution choice if beneficial" << std::endl;
    
    int total_reverted = 0;
    int sbff_checked = 0;
    
    std::cout << "  Total instances in db: " << db.instances.size() << std::endl;
    
    for (auto& inst_pair : db.instances) {
        auto& instance = inst_pair.second;
        
        // Only process single-bit FFs
        if (!is_single_bit_ff(instance)) {
            continue;
        }
        
        sbff_checked++;
        
        // Progress report every 1000 instances
        if (sbff_checked % 1000 == 0) {
            std::cout << "    Processed " << sbff_checked << " SBFF instances..." << std::endl;
        }
        
        // Get current FF and its score
        std::string current_ff = instance->cell_template->name;
        double current_score = calculate_ff_score(current_ff, db);
        
        // Check if we have a recorded better FF choice
        if (!instance->best_ff_from_substitution.empty() && 
            instance->best_ff_score < current_score) {
            
            // Verify the best FF exists in cell library
            auto best_cell_it = db.cell_library.find(instance->best_ff_from_substitution);
            if (best_cell_it != db.cell_library.end()) {
                // Execute substitution
                std::string old_ff = current_ff;
                instance->cell_template = best_cell_it->second;
                total_reverted++;
                
                // Record transformation for output generation
                record_post_substitution_transformation(db, instance, old_ff, instance->best_ff_from_substitution);
                
                // Debug output
                std::cout << "    " << instance->name << ": " << old_ff 
                         << " → " << instance->best_ff_from_substitution 
                         << " (score: " << std::fixed << std::setprecision(6) 
                         << current_score << " → " << instance->best_ff_score << ")" << std::endl;
            }
        }
    }
    
    std::cout << "  Post-banking substitution summary:" << std::endl;
    std::cout << "    SBFF instances checked: " << sbff_checked << std::endl;
    std::cout << "    Instances reverted: " << total_reverted << std::endl;
    std::cout << "✅ Post-Banking SBFF Substitution completed!" << std::endl;
}