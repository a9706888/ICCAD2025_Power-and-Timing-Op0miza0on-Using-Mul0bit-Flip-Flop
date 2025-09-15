// =============================================================================
// STRATEGIC BANKING IMPLEMENTATION
// =============================================================================
// This module implements strategic banking for ICCAD 2025 Contest
// Step 1: Banking Preparation - Assign banking types to FF instances
// =============================================================================

#include "parsers.hpp"
#include <iostream>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <set>

#define FSDN_2BIT_BANKING_distance 10000.0
#define FSDN_4BIT_BANKING_distance 10000.0
#define LSRDPQ_4BIT_BANKING_distance 10000.0

// =============================================================================
// BANKING OPERATION TRACKING STRUCTURE
// =============================================================================

struct BankingOperation {
    std::vector<std::shared_ptr<Instance>> source_instances;
    std::string result_instance_name;
    std::string target_cell_type;
    std::map<std::string, std::string> pin_mapping;
    std::string operation_type; // "DEBANK_CLUSTER_REBANK", "FSDN_2BIT_BANKING", "FSDN_4BIT_BANKING", "LSRDPQ_4BIT_BANKING"
};

// Global banking operations collector
static std::vector<BankingOperation> banking_operations;

// Track original 1-bit sources for complete pin mapping
static std::map<std::string, std::vector<std::shared_ptr<Instance>>> original_sources_map;

// Generate complete pin mapping from original single-bit FFs to final multi-bit FF
void generate_complete_banking_pin_mapping(
    const std::vector<std::shared_ptr<Instance>>& original_sources,
    std::shared_ptr<Instance> final_multibit_ff,
    std::map<std::string, std::string>& pin_mapping) {

        for (size_t i = 0; i < original_sources.size(); i++) {
            const auto& source = original_sources[i];

            // Map each pin from original FF to corresponding multi-bit pin
            for (const auto& conn : source->connections) {
                std::string original_pin = source->name + "/" + conn.pin_name;
                std::string final_pin;

                // Data pins get bit indexing
                if (conn.pin_name == "D" || conn.pin_name == "Q" || conn.pin_name == "QN") {
                    final_pin = final_multibit_ff->name + "/" + conn.pin_name + "[" + std::to_string(i) + "]";
                }
                // Shared pins (CK, SI, SE, R, S) don't get bit indexing
                else {
                    final_pin = final_multibit_ff->name + "/" + conn.pin_name;
                }

                pin_mapping[original_pin] = final_pin;
            }
      }
  }

// Process remaining 2-bit FFs that couldn't be banked to 4-bit
void finalize_2bit_banking_records() {
    for (const auto& mapping : original_sources_map) {
        const std::string& twobit_name = mapping.first;
        const std::vector<std::shared_ptr<Instance>>& original_sources = mapping.second;
        
        // Check if this 2-bit FF was used in 4-bit banking
        bool used_in_4bit = false;
        for (const auto& op : banking_operations) {
            if (op.operation_type == "FSDN_4BIT_BANKING" || op.operation_type == "LSRDPQ_4BIT_BANKING") {
                // Check if any of the original sources are in this 4-bit operation
                for (const auto& source : original_sources) {
                    for (const auto& op_source : op.source_instances) {
                        if (source->name == op_source->name) {
                            used_in_4bit = true;
                            break;
                        }
                    }
                    if (used_in_4bit) break;
                }
            }
            if (used_in_4bit) break;
        }

        // If not used in 4-bit banking, record as final 2-bit banking
        if (!used_in_4bit) {
            BankingOperation op;
            op.source_instances = original_sources;
            op.result_instance_name = twobit_name;
            op.target_cell_type = ""; // Will be filled from actual instance
            op.pin_mapping = std::map<std::string, std::string>(); // Will be generated in record phase
            op.operation_type = "FSDN_2BIT_BANKING";
            banking_operations.push_back(op);
        }
    }
}

// =============================================================================
// STEP 1: BANKING PREPARATION
// =============================================================================

void assign_banking_types(DesignDatabase& db) {
    std::cout << "  Assigning banking types to FF instances..." << std::endl;
    
    int fsdn_count = 0;
    int rising_lsrdpq_count = 0;
    int none_count = 0;
    int total_ff_count = 0;
    
    for (auto& inst_pair : db.instances) {
        auto instance = inst_pair.second;
        if (!instance->is_flip_flop()) continue;
        
        total_ff_count++;
        
        std::string clock_edge = get_instance_clock_edge(instance, db);
        std::string cell_name = instance->cell_template->name;
        
        if (clock_edge == "FALLING" && cell_name.find("FSDN") != std::string::npos) {
            instance->banking_type = BankingType::FSDN;
            fsdn_count++;
        } 
        else if (clock_edge == "RISING" && 
                (cell_name.find("FDP") != std::string::npos || 
                 cell_name.find("LSRDPQ") != std::string::npos)) {
            instance->banking_type = BankingType::RISING_LSRDPQ;
            rising_lsrdpq_count++;
        }
        else {
            instance->banking_type = BankingType::NONE;
            none_count++;
        }
    }
    
    std::cout << "    Banking type assignment completed:" << std::endl;
    std::cout << "      Total FF instances: " << total_ff_count << std::endl;
    std::cout << "      FSDN (FALLING): " << fsdn_count << std::endl;
    std::cout << "      RISING_LSRDPQ (RISING): " << rising_lsrdpq_count << std::endl;
    std::cout << "      NONE (cannot bank): " << none_count << std::endl;
}

void verify_cluster_ids(DesignDatabase& db) {
    std::cout << "  Verifying cluster IDs..." << std::endl;
    
    int total_ff_count = 0;
    int with_cluster_id = 0;
    int without_cluster_id = 0;
    
    for (auto& inst_pair : db.instances) {
        auto instance = inst_pair.second;
        if (!instance->is_flip_flop()) continue;
        
        total_ff_count++;
        
        if (!instance->cluster_id.empty()) {
            with_cluster_id++;
        } else {
            without_cluster_id++;
        }
    }
    
    std::cout << "    Cluster ID verification completed:" << std::endl;
    std::cout << "      Total FF instances: " << total_ff_count << std::endl;
    std::cout << "      With cluster_id: " << with_cluster_id << std::endl;
    std::cout << "      Without cluster_id: " << without_cluster_id << std::endl;
}

void export_banking_preparation_report(DesignDatabase& db, const std::string& output_file) {
    std::cout << "  Exporting banking preparation report to: " << output_file << std::endl;
    
    std::ofstream out(output_file);
    if (!out.is_open()) {
        std::cerr << "Error: Cannot open " << output_file << " for writing" << std::endl;
        return;
    }
    
    out << "=== BANKING PREPARATION REPORT ===" << std::endl;
    out << "Generated: " << __DATE__ << " " << __TIME__ << std::endl;
    out << std::endl;
    
    // Statistics by banking type
    std::map<BankingType, int> type_counts;
    std::map<BankingType, std::map<std::string, int>> type_cluster_counts;
    
    for (auto& inst_pair : db.instances) {
        auto instance = inst_pair.second;
        if (!instance->is_flip_flop()) continue;
        
        type_counts[instance->banking_type]++;
        
        if (!instance->cluster_id.empty()) {
            type_cluster_counts[instance->banking_type][instance->cluster_id]++;
        }
    }
    
    out << "=== BANKING TYPE SUMMARY ===" << std::endl;
    out << "FSDN instances: " << type_counts[BankingType::FSDN] << std::endl;
    out << "RISING_LSRDPQ instances: " << type_counts[BankingType::RISING_LSRDPQ] << std::endl;
    out << "NONE instances: " << type_counts[BankingType::NONE] << std::endl;
    out << std::endl;
    
    // Detailed cluster analysis
    out << "=== CLUSTER ANALYSIS ===" << std::endl;
    
    for (auto& type_pair : type_cluster_counts) {
        BankingType banking_type = type_pair.first;
        auto& cluster_map = type_pair.second;
        
        std::string type_name = (banking_type == BankingType::FSDN) ? "FSDN" :
                               (banking_type == BankingType::RISING_LSRDPQ) ? "RISING_LSRDPQ" : "NONE";
        
        out << type_name << " Clusters (" << cluster_map.size() << " unique clusters):" << std::endl;
        
        for (auto& cluster_pair : cluster_map) {
            std::string cluster_id = cluster_pair.first;
            int instance_count = cluster_pair.second;
            
            out << "  Cluster " << cluster_id << ": " << instance_count << " instances";
            
            // Banking potential analysis
            if (banking_type == BankingType::FSDN) {
                if (instance_count >= 4) {
                    out << " → Can form 2x2-bit or 1x4-bit";
                } else if (instance_count >= 2) {
                    out << " → Can form 1x2-bit";
                } else {
                    out << " → Cannot bank (insufficient instances)";
                }
            } else if (banking_type == BankingType::RISING_LSRDPQ) {
                if (instance_count >= 4) {
                    out << " → Can form 1x4-bit LSRDPQ";
                } else {
                    out << " → Cannot bank (need 4+ instances)";
                }
            }
            
            out << std::endl;
        }
        out << std::endl;
    }
    
    out.close();
    std::cout << "    Banking preparation report exported successfully" << std::endl;
}

void execute_banking_preparation(DesignDatabase& db) {
    std::cout << "\n🏦 Step 1: Banking Preparation..." << std::endl;
    
    // Banking types already assigned in Step 16.5
    std::cout << "  Banking types already assigned - skipping assignment" << std::endl;
    
    // Verify cluster IDs
    verify_cluster_ids(db);
    
    // Export preparation report
    // export_banking_preparation_report(db, "banking_preparation_report.txt");
    
    std::cout << "✅ Banking preparation completed!" << std::endl;
}

// =============================================================================
// STEP 2: FSDN TWO-PHASE BANKING - REIMPLEMENTATION
// =============================================================================

// Convert instance group key to hierarchical group key by examining instances in the group
std::string convert_instance_key_to_hierarchical_key(const std::string& instance_key) {
    // This function is a placeholder - the actual conversion will be done
    // by examining the instances in the group during banking
    return "";
}

// Helper function to determine hierarchical key from instances in a group
std::string get_hierarchical_key_from_group(const std::vector<std::shared_ptr<Instance>>& instances, const DesignDatabase& db) {
    if (instances.empty()) return "";
    
    // Use the first instance to determine the hierarchical group
    auto first_instance = instances[0];
    if (!first_instance || !first_instance->cell_template) return "";
    
    std::string cell_name = first_instance->cell_template->name;
    
    // Search in hierarchical_ff_groups to find which group this cell belongs to
    for (const auto& edge_pair : db.hierarchical_ff_groups) {
        for (const auto& pin_pair : edge_pair.second) {
            for (const auto& bit_pair : pin_pair.second) {
                const auto& cell_list = bit_pair.second;
                if (std::find(cell_list.begin(), cell_list.end(), cell_name) != cell_list.end()) {
                    // Found the group - return the hierarchical key
                    return edge_pair.first + "|" + pin_pair.first + "|" + std::to_string(bit_pair.first) + "bit";
                }
            }
        }
    }
    
    return "";
}

// Helper function to extract hierarchy prefix
std::string extract_hierarchy_prefix(const std::string& instance_name) {
    size_t last_slash = instance_name.find_last_of('/');
    if (last_slash == std::string::npos) {
        return "";  // No hierarchy, top level
    }
    return instance_name.substr(0, last_slash);
}

// Complete banking report generation (no simplification)
void export_complete_banking_report(const DesignDatabase& db, const std::string& filename) {
    std::cout << "  Exporting complete banking report: " << filename << std::endl;
    
    std::ofstream out(filename);
    if (!out.is_open()) {
        std::cerr << "Error: Cannot open " << filename << " for writing" << std::endl;
        return;
    }
    
    out << "=== COMPLETE BANKING REPORT ===" << std::endl;
    out << "Generated: " << __DATE__ << " " << __TIME__ << std::endl;
    out << std::endl;
    
    // Count FF instances by banking type
    std::map<BankingType, std::vector<std::shared_ptr<Instance>>> type_instances;
    
    for (const auto& inst_pair : db.instances) {
        auto instance = inst_pair.second;
        if (!instance->is_flip_flop()) continue;
        
        type_instances[instance->banking_type].push_back(instance);
    }
    
    // Summary statistics
    out << "=== FF INSTANCE SUMMARY ===" << std::endl;
    out << "FSDN instances: " << type_instances[BankingType::FSDN].size() << std::endl;
    out << "RISING_LSRDPQ instances: " << type_instances[BankingType::RISING_LSRDPQ].size() << std::endl;
    out << "NONE instances: " << type_instances[BankingType::NONE].size() << std::endl;
    out << "Total FF instances: " << (type_instances[BankingType::FSDN].size() + 
                                     type_instances[BankingType::RISING_LSRDPQ].size() + 
                                     type_instances[BankingType::NONE].size()) << std::endl;
    out << std::endl;
    
    // Complete instance listing by type
    for (auto& type_pair : type_instances) {
        BankingType banking_type = type_pair.first;
        auto& instances = type_pair.second;
        
        std::string type_name = (banking_type == BankingType::FSDN) ? "FSDN" :
                               (banking_type == BankingType::RISING_LSRDPQ) ? "RISING_LSRDPQ" : "NONE";
        
        out << "=== " << type_name << " INSTANCES ===" << std::endl;
        out << "Total " << type_name << " instances: " << instances.size() << std::endl;
        
        // List all instances (no limitation)
        for (const auto& instance : instances) {
            out << "  - " << instance->name << " (" << instance->cell_template->name << ") at (" 
                << instance->position.x << ", " << instance->position.y << ")" << std::endl;
        }
        out << std::endl;
    }
    
    out.close();
    std::cout << "    Complete banking report exported: " << filename << std::endl;
}

// Collect FSDN instances for specific group
// SIMPLIFIED: Direct collection based on cell name patterns and banking_type
std::vector<std::shared_ptr<Instance>> collect_fsdn_instances_for_group(
    const DesignDatabase& db, const std::string& group_key) {
    
    std::vector<std::shared_ptr<Instance>> fsdn_instances;
    
    // Find the group
    auto group_it = db.ff_instance_groups.find(group_key);
    if (group_it == db.ff_instance_groups.end()) {
        return fsdn_instances;  // Empty vector
    }
    
    int total_in_group = 0;
    int valid_instances = 0;
    int fsdn_instances_found = 0;
    
    // SIMPLIFIED: Collect all 1-bit FSDN instances from the group
    for (const auto& group_instance : group_it->second) {
        total_in_group++;
        if (!group_instance || !group_instance->is_flip_flop()) continue;
        
        // SIMPLIFIED: Check cell name for FSDN pattern (avoid complex validation)
        std::string cell_name = group_instance->cell_template->name;
        bool is_fsdn = cell_name.find("FSDN") != std::string::npos;
        bool is_1bit = cell_name.find("FSDN2") == std::string::npos && 
                       cell_name.find("FSDN4") == std::string::npos;
        
        if (is_fsdn && is_1bit) {
            // Double-check that this instance exists in db.instances
            auto db_it = db.instances.find(group_instance->name);
            if (db_it != db.instances.end()) {
                fsdn_instances.push_back(db_it->second);  // Use fresh instance from db.instances
                fsdn_instances_found++;
                valid_instances++;
            }
        } else {
            valid_instances++;  // Non-FSDN instances are still valid
        }
    }
    
    
    return fsdn_instances;
}

// Count total FF instances for verification
int count_ff_instances(const DesignDatabase& db) {
    int count = 0;
    for (const auto& inst_pair : db.instances) {
        if (inst_pair.second->is_flip_flop()) {
            count++;
        }
    }
    return count;
}

// =============================================================================
// SPATIAL CLUSTERING ALGORITHM
// =============================================================================

// Calculate Manhattan distance between two instances
double manhattan_distance(const std::shared_ptr<Instance>& inst1, 
                         const std::shared_ptr<Instance>& inst2) {
    double dx = std::abs(inst1->position.x - inst2->position.x);
    double dy = std::abs(inst1->position.y - inst2->position.y);
    return dx + dy;
}

// Simple distance threshold clustering for banking
std::vector<std::vector<std::shared_ptr<Instance>>> 
simple_distance_clustering(const std::vector<std::shared_ptr<Instance>>& instances, 
                          int target_cluster_size,
                          double max_distance_threshold) {
    
    
    std::vector<std::vector<std::shared_ptr<Instance>>> clusters;
    std::vector<bool> used(instances.size(), false);
    
    for (size_t i = 0; i < instances.size(); i++) {
        if (used[i]) continue;
        
        std::vector<std::shared_ptr<Instance>> cluster;
        cluster.push_back(instances[i]);
        used[i] = true;
        
        // Find nearby instances to add to cluster
        for (size_t j = i + 1; j < instances.size() && cluster.size() < (size_t)target_cluster_size; j++) {
            if (used[j]) continue;
            
            double dist = manhattan_distance(instances[i], instances[j]);
            if (dist <= max_distance_threshold) {
                cluster.push_back(instances[j]);
                used[j] = true;
            }
        }
        
        // Only keep clusters that meet minimum size requirement
        if (cluster.size() >= (size_t)target_cluster_size) {
            clusters.push_back(cluster);
        }
    }
    
    return clusters;
}

// Helper function to map 2-bit connections to 4-bit pins
void map_2bit_to_4bit_connections(const std::vector<std::shared_ptr<Instance>>& twobit_instances,
                                 std::shared_ptr<Instance> fourbit_instance,
                                 DesignDatabase& db) {
    // Clear existing connections
    fourbit_instance->connections.clear();
    
    // Map two 2-bit instances to one 4-bit instance
    // First 2-bit instance → bits [0], [1]
    // Second 2-bit instance → bits [2], [3]
    
    for (size_t i = 0; i < 2 && i < twobit_instances.size(); i++) {
        auto& twobit = twobit_instances[i];
        
        for (const auto& conn : twobit->connections) {
            if (conn.pin_name == "D0") {
                fourbit_instance->connections.push_back({
                    "D" + std::to_string(i * 2), conn.net_name
                });
            } else if (conn.pin_name == "D1") {
                fourbit_instance->connections.push_back({
                    "D" + std::to_string(i * 2 + 1), conn.net_name
                });
            } else if (conn.pin_name == "Q0") {
                fourbit_instance->connections.push_back({
                    "Q" + std::to_string(i * 2), conn.net_name
                });
            } else if (conn.pin_name == "Q1") {
                fourbit_instance->connections.push_back({
                    "Q" + std::to_string(i * 2 + 1), conn.net_name
                });
            } else if (conn.pin_name == "QN0") {
                fourbit_instance->connections.push_back({
                    "QN" + std::to_string(i * 2), conn.net_name
                });
            } else if (conn.pin_name == "QN1") {
                fourbit_instance->connections.push_back({
                    "QN" + std::to_string(i * 2 + 1), conn.net_name
                });
            } else {
                // Shared pins (CK, SI, SE, R, S, etc.) - use first instance's connection
                if (i == 0) {
                    fourbit_instance->connections.push_back(conn);
                }
            }
        }
    }
}

// =============================================================================
// TWO-PHASE BANKING IMPLEMENTATION  
// =============================================================================

// Phase 1: 1-bit FSDN → 2-bit FSDN Banking
int execute_1bit_to_2bit_banking(DesignDatabase& db, const std::string& group_key) {
    
    // Collect 1-bit FSDN instances for this group
    auto fsdn_instances = collect_fsdn_instances_for_group(db, group_key);
    
    if (fsdn_instances.size() < 2) {
        return 0;
    }
    
    // Use spatial clustering to find pairs (distance threshold: 5000)
    auto two_bit_clusters = simple_distance_clustering(fsdn_instances, 2, FSDN_2BIT_BANKING_distance);
    
    static int ff_counter = 1;
    int created_2bit = 0;
    
    for (const auto& cluster : two_bit_clusters) {
        if (cluster.size() != 2) continue;  // Only process exact pairs
        
        // Find target 2-bit FSDN FF using instances in cluster
        std::string hierarchical_key = get_hierarchical_key_from_group(cluster, db);
        if (hierarchical_key.empty()) {
            continue;
        }
        
        // Replace bit width with 2bit for target key
        size_t last_pipe = hierarchical_key.rfind('|');
        if (last_pipe == std::string::npos) continue;
        std::string target_key = hierarchical_key.substr(0, last_pipe) + "|2bit";
        
        auto optimal_it = db.optimal_ff_for_groups.find(target_key);
        if (optimal_it == db.optimal_ff_for_groups.end()) {
            continue;
        }
        
        std::string optimal_ff = optimal_it->second;
        
        // Calculate center position
        double center_x = (cluster[0]->position.x + cluster[1]->position.x) / 2.0;
        double center_y = (cluster[0]->position.y + cluster[1]->position.y) / 2.0;
        
        // Create new 2-bit instance with proper hierarchy naming
        auto new_2bit = std::make_shared<Instance>();
        std::string hierarchy_prefix = extract_hierarchy_prefix(cluster[0]->name);
        if (hierarchy_prefix.empty()) {
            new_2bit->name = "ff_fsdn2_" + std::to_string(ff_counter++);
        } else {
            new_2bit->name = hierarchy_prefix + "/ff_fsdn2_" + std::to_string(ff_counter++);
        }
        
        new_2bit->cell_type = optimal_ff;
        new_2bit->cell_template = db.cell_library[optimal_ff];
        new_2bit->position.x = center_x;
        new_2bit->position.y = center_y;
        new_2bit->orientation = cluster[0]->orientation;
        new_2bit->banking_type = BankingType::FSDN;
        
        // Preserve module assignment - banked instances stay in same module
        new_2bit->module_name = cluster[0]->module_name;
        
        // Map connections from single-bit to multi-bit
        map_singlebit_to_multibit_connections(cluster, new_2bit, 2, db);
        
        // Add new instance and remove old ones (critical for correct counting)
        db.instances[new_2bit->name] = new_2bit;
        
        // Remove old instances from db.instances
        for (const auto& inst : cluster) {
            db.instances.erase(inst->name);
        }
        
        // CRITICAL: Rebuild this group's instance list (safer than std::remove)
        auto group_it = db.ff_instance_groups.find(group_key);
        if (group_it != db.ff_instance_groups.end()) {
            // Create set of instances to remove for fast lookup
            std::set<std::shared_ptr<Instance>> to_remove(cluster.begin(), cluster.end());
            
            // Rebuild the group list excluding removed instances
            std::vector<std::shared_ptr<Instance>> new_group_list;
            for (const auto& inst : group_it->second) {
                if (to_remove.find(inst) == to_remove.end()) {
                    new_group_list.push_back(inst);
                }
            }
            
            // Add new 2-bit instance to the group so Phase 2 can find it
            new_group_list.push_back(new_2bit);
            
            // Replace the old list with the new one
            group_it->second = std::move(new_group_list);
        }
        
        created_2bit++;
                 
        // Track original 1-bit sources for later complete pin mapping
        original_sources_map[new_2bit->name] = cluster;

        // Don't record banking operation yet - wait until final 4-bit or keep as 2-bit final
    }
    
    return created_2bit;
}

// Phase 2: 2-bit FSDN → 4-bit FSDN Banking  
int execute_2bit_to_4bit_banking(DesignDatabase& db, const std::string& group_key) {
    
    // CORRECTED: Phase 2 should ONLY process 2-bit FSDN instances in this specific group
    std::vector<std::shared_ptr<Instance>> twobit_instances;
    
    // Only search within the specified group (maintain group boundaries)
    auto group_it = db.ff_instance_groups.find(group_key);
    if (group_it == db.ff_instance_groups.end()) {
        return 0;
    }
    
    for (const auto& instance : group_it->second) {
        if (!instance->is_flip_flop()) continue;
        
        // SIMPLIFIED: Only collect 2-bit FSDN instances by checking cell name
        std::string cell_name = instance->cell_template->name;
        bool is_2bit_fsdn = cell_name.find("FSDN2") != std::string::npos;
        
        if (is_2bit_fsdn) {
            // Double-check that this instance exists in db.instances
            auto db_it = db.instances.find(instance->name);
            if (db_it != db.instances.end()) {
                twobit_instances.push_back(db_it->second);  // Use fresh instance
            }
        }
    }
    
    if (twobit_instances.size() < 2) {
        return 0;
    }
    
    
    // Use spatial clustering to find pairs (distance threshold: 8000)
    auto four_bit_clusters = simple_distance_clustering(twobit_instances, 2, FSDN_4BIT_BANKING_distance);
    
    static int ff_counter_4bit = 1;
    int created_4bit = 0;
    
    for (const auto& cluster : four_bit_clusters) {
        if (cluster.size() != 2) continue;  // Only process exact pairs
        
        // Find target 4-bit FSDN FF using instances in cluster
        std::string hierarchical_key = get_hierarchical_key_from_group(cluster, db);
        if (hierarchical_key.empty()) {
            continue;
        }
        
        // Replace bit width with 4bit for target key
        size_t last_pipe = hierarchical_key.rfind('|');
        if (last_pipe == std::string::npos) continue;
        std::string target_key = hierarchical_key.substr(0, last_pipe) + "|4bit";
        
        auto optimal_it = db.optimal_ff_for_groups.find(target_key);
        if (optimal_it == db.optimal_ff_for_groups.end()) {
            continue;
        }
        
        std::string optimal_ff = optimal_it->second;
        
        // Calculate center position
        double center_x = (cluster[0]->position.x + cluster[1]->position.x) / 2.0;
        double center_y = (cluster[0]->position.y + cluster[1]->position.y) / 2.0;
        
        // Create new 4-bit instance with proper hierarchy naming
        auto new_4bit = std::make_shared<Instance>();
        std::string hierarchy_prefix = extract_hierarchy_prefix(cluster[0]->name);
        if (hierarchy_prefix.empty()) {
            new_4bit->name = "ff_fsdn4_" + std::to_string(ff_counter_4bit++);
        } else {
            new_4bit->name = hierarchy_prefix + "/ff_fsdn4_" + std::to_string(ff_counter_4bit++);
        }
        
        new_4bit->cell_type = optimal_ff;
        new_4bit->cell_template = db.cell_library[optimal_ff];
        new_4bit->position.x = center_x;
        new_4bit->position.y = center_y;
        new_4bit->orientation = cluster[0]->orientation;
        new_4bit->banking_type = BankingType::FSDN;
        
        // Preserve module assignment - banked instances stay in same module
        new_4bit->module_name = cluster[0]->module_name;
        
        // Map connections from 2-bit to 4-bit (special handling)
        map_2bit_to_4bit_connections(cluster, new_4bit, db);
        
        // Add new instance and remove old ones
        db.instances[new_4bit->name] = new_4bit;
        
        // CRITICAL: Rebuild this group's instance list for Phase 2
        auto group_it = db.ff_instance_groups.find(group_key);
        if (group_it != db.ff_instance_groups.end()) {
            // Create set of instances to remove for fast lookup
            std::set<std::shared_ptr<Instance>> to_remove(cluster.begin(), cluster.end());
            
            // Rebuild the group list excluding removed instances
            std::vector<std::shared_ptr<Instance>> new_group_list;
            for (const auto& inst : group_it->second) {
                if (to_remove.find(inst) == to_remove.end()) {
                    new_group_list.push_back(inst);
                }
            }
            
            // Add new 4-bit instance to the group
            new_group_list.push_back(new_4bit);
            
            // Replace the old list with the new one
            group_it->second = std::move(new_group_list);
        }
        
        for (const auto& inst : cluster) {
            db.instances.erase(inst->name);
        }
        
        created_4bit++;
                 
        // Record banking operation for output generation
        // Collect ALL original 1-bit sources (from both 2-bit FFs in this cluster)
        std::vector<std::shared_ptr<Instance>> all_original_sources;
        for (const auto& twobit_ff : cluster) {
            auto sources_it = original_sources_map.find(twobit_ff->name);
            if (sources_it != original_sources_map.end()) {
                // Add original 1-bit sources from this 2-bit FF
                all_original_sources.insert(all_original_sources.end(),
                    sources_it->second.begin(), sources_it->second.end());
            }
         }

        // Generate complete pin mapping from original 1-bit to final 4-bit
        std::map<std::string, std::string> complete_pin_mapping;
        generate_complete_banking_pin_mapping(all_original_sources, new_4bit, complete_pin_mapping);

        // Record final banking operation (1-bit sources → 4-bit result)
        BankingOperation op;
        op.source_instances = all_original_sources;
        op.result_instance_name = new_4bit->name;
        op.target_cell_type = optimal_ff;
        op.pin_mapping = complete_pin_mapping;
        op.operation_type = "FSDN_4BIT_BANKING";
        banking_operations.push_back(op);
    }
    
    return created_4bit;
}

// Main FSDN Two-Phase Banking function
void execute_fsdn_two_phase_banking(DesignDatabase& db) {
    std::cout << "\n🏦 Step 2: FSDN Two-Phase Banking..." << std::endl;
    std::cout << "  Strategy: 1-bit→2-bit→4-bit spatial clustering banking for FSDN instances" << std::endl;
    
    // ff_instance_groups already rebuilt in Step 16.5 with banking types assigned - no need to rebuild
    
    // Verify initial FF count
    int initial_ff_count = count_ff_instances(db);
    
    
    // Export "before" state with complete report
    // export_complete_banking_report(db, "banking_step2_before.txt");
    
    int total_groups_processed = 0;
    int total_2bit_created = 0;
    int total_4bit_created = 0;
    int initial_fsdn_count = 0;
    
    // Process each ff_instance_group
    for (auto& group_pair : db.ff_instance_groups) {
        const std::string& group_key = group_pair.first;
        const auto& group_instances = group_pair.second;
        
        // Count initial FSDN instances for this group
        auto initial_fsdn_instances = collect_fsdn_instances_for_group(db, group_key);
        if (initial_fsdn_instances.size() < 2) {
            continue;  // Skip groups with insufficient instances
        }
        
        initial_fsdn_count += initial_fsdn_instances.size();
        total_groups_processed++;
        
        
        // Phase 1: 1-bit → 2-bit Banking
        int created_2bit = execute_1bit_to_2bit_banking(db, group_key);
        total_2bit_created += created_2bit;
        
        // Phase 2: 2-bit → 4-bit Banking (only if we created 2-bit FFs)
        if (created_2bit > 0) {
            int created_4bit = execute_2bit_to_4bit_banking(db, group_key);
            total_4bit_created += created_4bit;
        }
        
    }
    
    // Finalize remaining 2-bit banking records for FFs that couldn't be banked to 4-bit
    std::cout << "  Finalizing 2-bit banking records..." << std::endl;
    finalize_2bit_banking_records();
    
    // Final verification
    int final_ff_count = count_ff_instances(db);
    int ff_reduction = initial_ff_count - final_ff_count;
    
    
    // Export "after" state with complete report
    // export_complete_banking_report(db, "banking_step2_after.txt");
    
    // Export banking operations record
    // export_banking_operations_record("banking_operations.txt");
    
    std::cout << "✅ FSDN Two-Phase Banking completed!" << std::endl;
}

// =============================================================================
// STEP 3: LSRDPQ SINGLE-PHASE BANKING
// =============================================================================

// Collect LSRDPQ instances for specific group  
// Similar to collect_fsdn_instances_for_group but for RISING edge FFs
std::vector<std::shared_ptr<Instance>> collect_lsrdpq_instances_for_group(
    const DesignDatabase& db, const std::string& group_key) {
    
    std::vector<std::shared_ptr<Instance>> lsrdpq_instances;
    
    // Find the group
    auto group_it = db.ff_instance_groups.find(group_key);
    if (group_it == db.ff_instance_groups.end()) {
        return lsrdpq_instances;  // Empty vector
    }
    
    // Collect all 1-bit LSRDPQ/FDP instances from the group
    for (const auto& group_instance : group_it->second) {
        if (!group_instance || !group_instance->is_flip_flop()) continue;
        
        // Check cell name for LSRDPQ/FDP pattern (1-bit only)
        std::string cell_name = group_instance->cell_template->name;
        bool is_lsrdpq = cell_name.find("LSRDPQ") != std::string::npos;
        bool is_fdp = cell_name.find("FDP") != std::string::npos;
        bool is_1bit = cell_name.find("LSRDPQ4") == std::string::npos; // Exclude existing 4-bit
        
        if ((is_lsrdpq && is_1bit) || is_fdp) {
            // Verify instance exists in db.instances
            auto db_it = db.instances.find(group_instance->name);
            if (db_it != db.instances.end()) {
                lsrdpq_instances.push_back(db_it->second);
            }
        }
    }
    
    return lsrdpq_instances;
}

// Single-Phase: 1-bit LSRDPQ/FDP → 4-bit LSRDPQ Banking
int execute_lsrdpq_4bit_banking(DesignDatabase& db, const std::string& group_key) {
    
    // Collect 1-bit LSRDPQ/FDP instances for this group
    auto lsrdpq_instances = collect_lsrdpq_instances_for_group(db, group_key);
    
    if (lsrdpq_instances.size() < 4) {
        return 0; // Need at least 4 instances for 4-bit banking
    }
    
    // Use spatial clustering to find groups of 4 (distance threshold: 10000 for 4 instances)
    auto four_bit_clusters = simple_distance_clustering(lsrdpq_instances, 4, LSRDPQ_4BIT_BANKING_distance);

    static int lsrdpq_counter = 1;
    int created_4bit = 0;
    
    for (const auto& cluster : four_bit_clusters) {
        if (cluster.size() != 4) continue;  // Only process exact groups of 4
        
        // Target: RISING|D_Q_QN_CK|4bit group
        std::string target_key = "RISING|D_Q_QN_CK|4bit";
        
        auto optimal_it = db.optimal_ff_for_groups.find(target_key);
        if (optimal_it == db.optimal_ff_for_groups.end()) {
            continue;
        }
        
        std::string optimal_ff = optimal_it->second;
        
        // Calculate center position of 4 instances
        double center_x = 0, center_y = 0;
        for (const auto& inst : cluster) {
            center_x += inst->position.x;
            center_y += inst->position.y;
        }
        center_x /= 4.0;
        center_y /= 4.0;
        
        // Create new 4-bit LSRDPQ instance
        auto new_4bit = std::make_shared<Instance>();
        std::string hierarchy_prefix = extract_hierarchy_prefix(cluster[0]->name);
        if (hierarchy_prefix.empty()) {
            new_4bit->name = "ff_lsrdpq4_" + std::to_string(lsrdpq_counter++);
        } else {
            new_4bit->name = hierarchy_prefix + "/ff_lsrdpq4_" + std::to_string(lsrdpq_counter++);
        }
        
        new_4bit->cell_type = optimal_ff;
        new_4bit->cell_template = db.cell_library[optimal_ff];
        new_4bit->position.x = center_x;
        new_4bit->position.y = center_y;
        new_4bit->orientation = cluster[0]->orientation;
        new_4bit->banking_type = BankingType::RISING_LSRDPQ;
        
        // Preserve module assignment - banked instances stay in same module
        new_4bit->module_name = cluster[0]->module_name;
        
        // Map connections from 4 single-bit to 4-bit
        map_singlebit_to_multibit_connections(cluster, new_4bit, 4, db);
        
        // Add new instance and remove old ones
        db.instances[new_4bit->name] = new_4bit;
        
        // Remove old instances from db.instances
        for (const auto& inst : cluster) {
            db.instances.erase(inst->name);
        }
        
        // Update this group's instance list
        auto group_it = db.ff_instance_groups.find(group_key);
        if (group_it != db.ff_instance_groups.end()) {
            // Create set of instances to remove for fast lookup
            std::set<std::shared_ptr<Instance>> to_remove(cluster.begin(), cluster.end());
            
            // Rebuild the group list excluding removed instances
            std::vector<std::shared_ptr<Instance>> new_group_list;
            for (const auto& inst : group_it->second) {
                if (to_remove.find(inst) == to_remove.end()) {
                    new_group_list.push_back(inst);
                }
            }
            
            // Add new 4-bit instance to the group
            new_group_list.push_back(new_4bit);
            
            // Replace the old list with the new one
            group_it->second = std::move(new_group_list);
        }
        
        created_4bit++;
                 
        // Generate complete pin mapping from 1-bit to 4-bit
        std::map<std::string, std::string> complete_pin_mapping;
        generate_complete_banking_pin_mapping(cluster, new_4bit, complete_pin_mapping);
        
        // Record banking operation for output generation
        BankingOperation op;
        op.source_instances = cluster;
        op.result_instance_name = new_4bit->name;
        op.target_cell_type = optimal_ff;
        op.pin_mapping = complete_pin_mapping;
        op.operation_type = "LSRDPQ_4BIT_BANKING";
        banking_operations.push_back(op);
    }
    
    return created_4bit;
}

// Main LSRDPQ Single-Phase Banking function
void execute_lsrdpq_single_phase_banking(DesignDatabase& db) {
    std::cout << "\n🏦 Step 3: LSRDPQ Single-Phase Banking..." << std::endl;
    std::cout << "  Strategy: Direct 1-bit→4-bit banking for RISING edge instances" << std::endl;
    
    // Verify initial FF count
    int initial_ff_count = count_ff_instances(db);
    
    // Export "before" state
    // export_complete_banking_report(db, "banking_step3_before.txt");
    
    int total_groups_processed = 0;
    int total_4bit_created = 0;
    int initial_lsrdpq_count = 0;
    
    // Process each ff_instance_group
    for (auto& group_pair : db.ff_instance_groups) {
        const std::string& group_key = group_pair.first;
        
        // Count initial LSRDPQ instances for this group
        auto initial_lsrdpq_instances = collect_lsrdpq_instances_for_group(db, group_key);
        if (initial_lsrdpq_instances.size() < 4) {
            continue;  // Skip groups with insufficient instances
        }
        
        initial_lsrdpq_count += initial_lsrdpq_instances.size();
        total_groups_processed++;
        
        // Single-Phase: 1-bit → 4-bit Banking
        int created_4bit = execute_lsrdpq_4bit_banking(db, group_key);
        total_4bit_created += created_4bit;
    }
    
    // Final verification
    int final_ff_count = count_ff_instances(db);
    
    // Export "after" state
    // export_complete_banking_report(db, "banking_step3_after.txt");
    
    // Export banking operations record
    // export_banking_operations_record("banking_operations_lsrdpq.txt");
    
    std::cout << "✅ LSRDPQ Single-Phase Banking completed!" << std::endl;
}


// Export banking operations record for output generation
void export_banking_operations_record(const std::string& output_file) {
    std::cout << "  Exporting banking operations record to: " << output_file << std::endl;
    
    std::ofstream out(output_file);
    if (!out.is_open()) {
        std::cerr << "Error: Cannot open " << output_file << " for writing" << std::endl;
        return;
    }
    
    out << "=== BANKING OPERATIONS RECORD ===" << std::endl;
    out << "Generated: " << __DATE__ << " " << __TIME__ << std::endl;
    out << "Total banking operations: " << banking_operations.size() << std::endl;
    out << std::endl;
    
    // Group by operation type
    std::map<std::string, std::vector<BankingOperation*>> operations_by_type;
    for (auto& op : banking_operations) {
        operations_by_type[op.operation_type].push_back(&op);
    }
    
    for (auto& type_pair : operations_by_type) {
        const std::string& op_type = type_pair.first;
        auto& ops = type_pair.second;
        
        out << "=== " << op_type << " ===" << std::endl;
        out << "Count: " << ops.size() << std::endl;
        out << std::endl;
        
        for (size_t i = 0; i < ops.size(); i++) {
            auto& op = *ops[i];
            out << "Operation " << (i+1) << ":" << std::endl;
            out << "  Result: " << op.result_instance_name << " (" << op.target_cell_type << ")" << std::endl;
            out << "  Sources: ";
            for (size_t j = 0; j < op.source_instances.size(); j++) {
                if (j > 0) out << " + ";
                out << op.source_instances[j]->name;
            }
            out << std::endl;
            out << std::endl;
        }
    }
    
    out.close();
    std::cout << "    Banking operations record exported: " << output_file << std::endl;
}

// =============================================================================
// STEP 2: DEBANK CLUSTER RE-BANKING
// =============================================================================

void export_banking_step_report(const DesignDatabase& db, const std::string& step_name, const std::string& output_file) {
    std::ofstream out(output_file);
    if (!out.is_open()) {
        std::cerr << "Error: Cannot open " << output_file << " for writing" << std::endl;
        return;
    }
    
    out << "=== BANKING STEP REPORT: " << step_name << " ===" << std::endl;
    out << "Generated: " << __DATE__ << " " << __TIME__ << std::endl;
    out << std::endl;
    
    // Count FF instances by banking type
    std::map<BankingType, int> type_counts;
    std::map<BankingType, std::map<std::string, std::vector<std::string>>> type_instances;
    
    for (const auto& inst_pair : db.instances) {
        auto instance = inst_pair.second;
        if (!instance->is_flip_flop()) continue;
        
        type_counts[instance->banking_type]++;
        type_instances[instance->banking_type][instance->cluster_id].push_back(instance->name);
    }
    
    out << "=== CURRENT FF INSTANCE SUMMARY ===" << std::endl;
    out << "FSDN instances: " << type_counts[BankingType::FSDN] << std::endl;
    out << "RISING_LSRDPQ instances: " << type_counts[BankingType::RISING_LSRDPQ] << std::endl;
    out << "NONE instances: " << type_counts[BankingType::NONE] << std::endl;
    out << "Total FF instances: " << (type_counts[BankingType::FSDN] + type_counts[BankingType::RISING_LSRDPQ] + type_counts[BankingType::NONE]) << std::endl;
    out << std::endl;
    
    // Detailed instance listing by type and cluster
    for (auto& type_pair : type_instances) {
        BankingType banking_type = type_pair.first;
        auto& cluster_map = type_pair.second;
        
        std::string type_name = (banking_type == BankingType::FSDN) ? "FSDN" :
                               (banking_type == BankingType::RISING_LSRDPQ) ? "RISING_LSRDPQ" : "NONE";
        
        out << "=== " << type_name << " INSTANCES ===" << std::endl;
        
        for (auto& cluster_pair : cluster_map) {
            const std::string& cluster_id = cluster_pair.first;
            const auto& instance_names = cluster_pair.second;
            
            out << "Cluster '" << cluster_id << "' (" << instance_names.size() << " instances):" << std::endl;
            
            for (const auto& name : instance_names) {
                auto it = db.instances.find(name);
                if (it != db.instances.end()) {
                    auto instance = it->second;
                    out << "  - " << name << " (" << instance->cell_template->name << ") at (" 
                        << instance->position.x << ", " << instance->position.y << ")" << std::endl;
                } else {
                    out << "  - " << name << " (INSTANCE NOT FOUND)" << std::endl;
                }
            }
        }
        out << std::endl;
    }
    
    out.close();
    std::cout << "    Banking step report exported: " << output_file << std::endl;
}

void execute_debank_cluster_rebanking(DesignDatabase& db) {
    std::cout << "\n🏦 Step 2: Debank Cluster Re-banking..." << std::endl;
    std::cout << "  Strategy: Priority re-banking for instances from same original multi-bit FF" << std::endl;
    
    // Export "before" state
    // export_banking_step_report(db, "BEFORE_DEBANK_CLUSTER_REBANKING", "banking_step1_before.txt");
    
    // Group instances by cluster_id (exclude NONE banking type)
    std::map<std::string, std::vector<std::shared_ptr<Instance>>> clusters;
    
    for (auto& inst_pair : db.instances) {
        auto instance = inst_pair.second;
        if (!instance->is_flip_flop() || instance->cluster_id.empty()) continue;
        if (instance->banking_type == BankingType::NONE) continue; // Skip NONE banking type
        
        clusters[instance->cluster_id].push_back(instance);
    }
    
    int total_clusters_processed = 0;
    int total_instances_banked = 0;
    int total_new_mbffs = 0;
    
    // Process each cluster
    for (auto& cluster_pair : clusters) {
        const std::string& cluster_id = cluster_pair.first;
        auto& instances = cluster_pair.second;
        
        if (instances.size() < 2) continue; // Need at least 2 instances for banking
        
        // Determine banking type (all instances in same cluster should have same banking type)
        BankingType banking_type = instances[0]->banking_type;
        
        // Determine target FF based on banking type and instance count
        std::string target_key;
        if (banking_type == BankingType::FSDN) {
            if (instances.size() >= 4) {
                target_key = "FALLING|D_Q_QN_CK_SI_SE|4bit"; // FSDN4 group
            } else if (instances.size() >= 2) {
                target_key = "FALLING|D_Q_QN_CK_SI_SE|2bit"; // FSDN2 group
            }
        } else if (banking_type == BankingType::RISING_LSRDPQ) {
            if (instances.size() >= 4) {
                target_key = "RISING|D_Q_QN_CK|4bit"; // LSRDPQ4 group
            } else {
                continue; // LSRDPQ requires 4 instances minimum
            }
        }
        
        if (target_key.empty()) continue;
        
        // Find optimal FF for target group
        auto optimal_it = db.optimal_ff_for_groups.find(target_key);
        if (optimal_it == db.optimal_ff_for_groups.end()) continue;
        
        std::string optimal_ff = optimal_it->second;
        int target_bit_width = (instances.size() >= 4) ? 4 : 2;
        
        
        // Calculate center position
        double center_x = 0, center_y = 0;
        for (auto& inst : instances) {
            center_x += inst->position.x;
            center_y += inst->position.y;
        }
        center_x /= instances.size();
        center_y /= instances.size();
        
        // Create new multi-bit instance
        auto new_mbff = std::make_shared<Instance>();
        new_mbff->name = cluster_id + "_REBANKED";
        new_mbff->cell_type = optimal_ff;
        new_mbff->cell_template = db.cell_library[optimal_ff];
        new_mbff->position.x = center_x;
        new_mbff->position.y = center_y;
        new_mbff->orientation = instances[0]->orientation;
        new_mbff->banking_type = banking_type;
        new_mbff->cluster_id = cluster_id;
        
        // Preserve module assignment - banked instances stay in same module
        new_mbff->module_name = instances[0]->module_name;
        
        // Map connections from single-bit to multi-bit
        map_singlebit_to_multibit_connections(instances, new_mbff, target_bit_width, db);
        
        // Collect banking operation (do not record yet)
        std::map<std::string, std::string> pin_mapping;
        
        // Generate complete pin mapping for this banking operation
        for (size_t i = 0; i < instances.size(); i++) {
            const auto& source = instances[i];
            for (const auto& conn : source->connections) {
                std::string original_pin = source->name + "/" + conn.pin_name;
                std::string final_pin;
                
                // Data pins get bit indexing
                if (conn.pin_name == "D" || conn.pin_name == "Q" || conn.pin_name == "QN") {
                    final_pin = new_mbff->name + "/" + conn.pin_name + "[" + std::to_string(i) + "]";
                }
                // Shared pins (CK, SI, SE, R, S) don't get bit indexing
                else {
                    final_pin = new_mbff->name + "/" + conn.pin_name;
                }
                
                pin_mapping[original_pin] = final_pin;
            }
        }
        
        BankingOperation op;
        op.source_instances = instances;
        op.result_instance_name = new_mbff->name;
        op.target_cell_type = optimal_ff;
        op.pin_mapping = pin_mapping; // Now properly populated
        op.operation_type = "DEBANK_CLUSTER_REBANK";
        banking_operations.push_back(op);
        
        // Add new instance and remove old ones
        db.instances[new_mbff->name] = new_mbff;
        for (auto& inst : instances) {
            db.instances.erase(inst->name);
        }
        
        total_clusters_processed++;
        total_instances_banked += instances.size();
        total_new_mbffs++;
    }
    
    
    // Export "after" state
    // export_banking_step_report(db, "AFTER_DEBANK_CLUSTER_REBANKING", "banking_step1_after.txt");
    
    std::cout << "✅ Debank cluster re-banking completed!" << std::endl;
}

// Helper function to map single-bit connections to multi-bit pins
void map_singlebit_to_multibit_connections(const std::vector<std::shared_ptr<Instance>>& singlebit_instances,
                                          std::shared_ptr<Instance> multibit_instance,
                                          int target_bit_width,
                                          DesignDatabase& db) {
    // Clear existing connections
    multibit_instance->connections.clear();
    
    // Determine pin indexing based on cell type
    bool is_lsrdpq = (multibit_instance->cell_template && 
                      multibit_instance->cell_template->name.find("LSRDPQ") != std::string::npos);
    int pin_offset = is_lsrdpq ? 1 : 0;  // LSRDPQ starts from 1, FSDN starts from 0
    
    // Map each single-bit instance to corresponding bit index (up to target_bit_width)
    for (int i = 0; i < target_bit_width && i < (int)singlebit_instances.size(); i++) {
        auto& singlebit = singlebit_instances[i];
        
        for (const auto& conn : singlebit->connections) {
            if (conn.pin_name == "D") {
                multibit_instance->connections.push_back({
                    "D" + std::to_string(i + pin_offset), conn.net_name
                });
            } else if (conn.pin_name == "Q") {
                multibit_instance->connections.push_back({
                    "Q" + std::to_string(i + pin_offset), conn.net_name
                });
            } else if (conn.pin_name == "QN") {
                multibit_instance->connections.push_back({
                    "QN" + std::to_string(i + pin_offset), conn.net_name
                });
            } else {
                // Shared pins (CK, SI, SE, R, S, etc.) - use first instance's connection
                if (i == 0) {
                    multibit_instance->connections.push_back(conn);
                }
            }
        }
    }
}

// =============================================================================
// UNIFIED BANKING RECORD FUNCTION
// =============================================================================

void record_all_banking_transformations(DesignDatabase& db) {
    std::cout << "  Recording all banking transformations..." << std::endl;
    
    int total_operations = banking_operations.size();
    int total_source_instances = 0;
    
    for (const auto& op : banking_operations) {
        // Fill in missing information for finalized operations
        std::string target_cell_type = op.target_cell_type;
        std::map<std::string, std::string> pin_mapping = op.pin_mapping;
        
        if (target_cell_type.empty() || pin_mapping.empty()) {
            // Find the actual result instance to get missing information
            auto result_it = db.instances.find(op.result_instance_name);
            if (result_it != db.instances.end()) {
                if (target_cell_type.empty()) {
                    target_cell_type = result_it->second->cell_template->name;
                }
                if (pin_mapping.empty()) {
                    generate_complete_banking_pin_mapping(op.source_instances, result_it->second, pin_mapping);
                }
            }
        }
        
        // Record bank transformation for each operation
        record_bank_transformation(db, op.source_instances, op.result_instance_name, 
                                  target_cell_type, pin_mapping);
        total_source_instances += op.source_instances.size();
    }
    
    std::cout << "    Recorded " << total_operations << " banking operations" << std::endl;
    std::cout << "    Total source instances: " << total_source_instances << std::endl;
    std::cout << "    Total result instances: " << total_operations << std::endl;
    
    // Capture BANK stage - all instances after banking operations
    std::cout << "  Capturing BANK stage..." << std::endl;
    std::vector<std::shared_ptr<Instance>> all_ff_instances;
    for (const auto& inst_pair : db.instances) {
        if (inst_pair.second->is_flip_flop()) {
            all_ff_instances.push_back(inst_pair.second);
        }
    }
    
    // Find all BANK transformation records for this stage
    std::vector<size_t> bank_indices;
    for (size_t i = 0; i < db.transformation_history.size(); i++) {
        if (db.transformation_history[i].operation == TransformationRecord::Operation::BANK) {
            bank_indices.push_back(i);
        }
    }
    
    db.complete_pipeline.capture_stage("BANK", all_ff_instances, bank_indices, &db.transformation_history);
    std::cout << "Captured stage BANK with " << all_ff_instances.size() << " FF instances" << std::endl;
    
    // Clear operations and tracking maps after recording
    banking_operations.clear();
    original_sources_map.clear();
}