// =============================================================================
// STRATEGIC BANKING IMPLEMENTATION
// =============================================================================
// This module implements strategic banking for the ICCAD 2025 Contest
// Phase 1: Banking Eligibility Pre-screening - Two-Level Score Analysis
// =============================================================================

#include "parsers.hpp"
#include "timing_repr_hardcoded.hpp"
#include <iostream>
#include <algorithm>
#include <limits>
#include <iomanip>
#include <fstream>
#include <ctime>
#include <set>
#include <sstream>

// =============================================================================
// BANKING ELIGIBILITY ANALYSIS STRUCTURES
// =============================================================================

// Global storage for banking candidates (to share between functions)
static std::vector<struct BankingCandidate> g_banking_candidates;

struct BankingCandidate {
    std::string base_group_key;        // e.g., "FALLING|D_Q_CK"
    std::string single_bit_optimal;    // Best 1-bit FF
    double single_bit_score;
    
    struct MultiBitOption {
        int bit_width;                 // 2 or 4
        std::string optimal_ff;        // Best multi-bit FF
        double score;
        double score_per_bit;          // score / bit_width
        double improvement_per_bit;    // single_bit_score - score_per_bit
        bool is_eligible;              // improvement_per_bit > 0
    };
    
    std::vector<MultiBitOption> multi_bit_options;
    bool has_eligible_options;
};

// =============================================================================
// PHASE 1: TWO-LEVEL SCORE ANALYSIS
// =============================================================================

void analyze_banking_eligibility(DesignDatabase& db) {
    std::cout << "  Analyzing banking eligibility for all FF compatibility groups..." << std::endl;
    
    // Clear any existing banking eligibility data
    db.banking_eligible_groups.clear();
    
    std::vector<BankingCandidate> banking_candidates;
    
    // Extract unique base group keys from optimal FF groups (calculated in Step 15)
    std::set<std::string> base_group_keys;
    for (const auto& group_pair : db.optimal_ff_for_groups) {
        const std::string& full_key = group_pair.first;
        
        // Extract base key (remove |Xbit suffix)
        size_t bit_pos = full_key.find("|1bit");
        if (bit_pos == std::string::npos) {
            bit_pos = full_key.find("|2bit");
            if (bit_pos == std::string::npos) {
                bit_pos = full_key.find("|4bit");
            }
        }
        
        if (bit_pos != std::string::npos) {
            std::string base_key = full_key.substr(0, bit_pos);
            base_group_keys.insert(base_key);
        }
    }
    
    std::cout << "    Found " << base_group_keys.size() << " unique compatibility groups to analyze" << std::endl;
    
    // Analyze each base group for banking eligibility
    for (const std::string& base_key : base_group_keys) {
        BankingCandidate candidate;
        candidate.base_group_key = base_key;
        candidate.has_eligible_options = false;
        
        // Find 1-bit optimal FF and score
        std::string single_bit_key = base_key + "|1bit";
        auto single_bit_it = db.optimal_ff_for_groups.find(single_bit_key);
        
        if (single_bit_it == db.optimal_ff_for_groups.end()) {
            std::cout << "    Skipping " << base_key << " (no 1-bit optimal FF found)" << std::endl;
            continue;
        }
        
        candidate.single_bit_optimal = single_bit_it->second;
        candidate.single_bit_score = calculate_ff_score(single_bit_it->second, db);
        
        // Check 2-bit and 4-bit options
        for (int bit_width : {2, 4}) {
            std::string multi_bit_key = base_key + "|" + std::to_string(bit_width) + "bit";
            auto multi_bit_it = db.optimal_ff_for_groups.find(multi_bit_key);
            
            if (multi_bit_it != db.optimal_ff_for_groups.end()) {
                
                BankingCandidate::MultiBitOption option;
                option.bit_width = bit_width;
                option.optimal_ff = multi_bit_it->second;
                option.score = calculate_ff_score(multi_bit_it->second, db);
                option.score_per_bit = option.score;  // score already per bit from calculate_ff_score
                option.improvement_per_bit = candidate.single_bit_score - option.score_per_bit;
                option.is_eligible = option.improvement_per_bit > 0.0;
                
                if (option.is_eligible) {
                    candidate.has_eligible_options = true;
                    
                    // Add to global eligible groups list
                    std::string eligible_key = base_key + "|" + std::to_string(bit_width) + "bit";
                    db.banking_eligible_groups.push_back(eligible_key);
                }
                
                candidate.multi_bit_options.push_back(option);
            }
        }
        
        if (candidate.has_eligible_options || !candidate.multi_bit_options.empty()) {
            banking_candidates.push_back(candidate);
        }
    }
    
    // Store candidates for export
    g_banking_candidates = banking_candidates;
    
    std::cout << "    Banking eligibility analysis completed:" << std::endl;
    std::cout << "      Total compatibility groups analyzed: " << base_group_keys.size() << std::endl;
    std::cout << "      Groups with eligible banking options: " << std::count_if(banking_candidates.begin(), 
        banking_candidates.end(), [](const BankingCandidate& c) { return c.has_eligible_options; }) << std::endl;
    std::cout << "      Total eligible banking configurations: " << db.banking_eligible_groups.size() << std::endl;
}

void export_banking_eligibility_report(const DesignDatabase& db, const std::string& output_file) {
    std::cout << "  Exporting banking eligibility report to: " << output_file << std::endl;
    
    // Access the global candidates (populated by analyze_banking_eligibility)
    
    std::ofstream out(output_file);
    if (!out.is_open()) {
        std::cerr << "Error: Cannot open " << output_file << " for writing" << std::endl;
        return;
    }
    
    out << "=== BANKING ELIGIBILITY ANALYSIS REPORT ===" << std::endl;
    out << "Generated: " << __DATE__ << " " << __TIME__ << std::endl;
    out << std::endl;
    
    // Summary statistics
    int total_groups = 0;
    int eligible_groups = 0;
    int total_eligible_configs = 0;
    
    for (const auto& candidate : g_banking_candidates) {
        total_groups++;
        if (candidate.has_eligible_options) {
            eligible_groups++;
            for (const auto& option : candidate.multi_bit_options) {
                if (option.is_eligible) {
                    total_eligible_configs++;
                }
            }
        }
    }
    
    out << "=== SUMMARY ===" << std::endl;
    out << "Total compatibility groups analyzed: " << total_groups << std::endl;
    out << "Groups with eligible banking options: " << eligible_groups << std::endl;
    out << "Total eligible banking configurations: " << total_eligible_configs << std::endl;
    out << std::endl;
    
    // Detailed analysis
    out << "=== DETAILED BANKING ELIGIBILITY ANALYSIS ===" << std::endl;
    out << std::endl;
    
    for (const auto& candidate : g_banking_candidates) {
        out << "Compatibility Group: " << candidate.base_group_key << std::endl;
        out << "  1-bit optimal: " << candidate.single_bit_optimal 
            << " (score: " << std::fixed << std::setprecision(2) << candidate.single_bit_score << ")" << std::endl;
        
        if (candidate.multi_bit_options.empty()) {
            out << "  No multi-bit options available" << std::endl;
        } else {
            for (const auto& option : candidate.multi_bit_options) {
                out << "  " << option.bit_width << "-bit option: " << option.optimal_ff << std::endl;
                out << "    Total score: " << std::fixed << std::setprecision(2) << option.score << std::endl;
                out << "    Score per bit: " << std::fixed << std::setprecision(2) << option.score_per_bit << std::endl;
                out << "    Improvement per bit: " << std::fixed << std::setprecision(2) << option.improvement_per_bit << std::endl;
                out << "    Banking eligible: " << (option.is_eligible ? "YES" : "NO") << std::endl;
            }
        }
        
        out << "  Overall eligibility: " << (candidate.has_eligible_options ? "ELIGIBLE" : "NOT ELIGIBLE") << std::endl;
        out << std::endl;
    }
    
    // List of all eligible banking configurations
    out << "=== ELIGIBLE BANKING CONFIGURATIONS ===" << std::endl;
    if (db.banking_eligible_groups.empty()) {
        out << "No eligible banking configurations found." << std::endl;
    } else {
        for (const std::string& eligible_group : db.banking_eligible_groups) {
            out << "  " << eligible_group << std::endl;
        }
    }
    
    out.close();
    std::cout << "    Banking eligibility report exported successfully" << std::endl;
}

// =============================================================================
// PHASE 1: INSTANCE GROUP FILTERING
// =============================================================================

// Helper function to extract base compatibility key from instance group key
// Instance group key: "FALLING|CK_D_Q|scan_chain_1|clk|hier1" 
// Base compatibility key: "FALLING|D_Q_CK"
std::string extract_base_compatibility_key(const std::string& instance_group_key) {
    std::vector<std::string> parts;
    std::stringstream ss(instance_group_key);
    std::string part;
    
    // Split by '|'
    while (std::getline(ss, part, '|')) {
        parts.push_back(part);
    }
    
    if (parts.size() >= 2) {
        std::string clock_edge = parts[0];
        std::string pin_signature = parts[1];
        
        // Convert instance pin signature format to optimal FF format
        // Instance format: "CK_D_Q" -> Optimal format: "D_Q_CK"
        // Special handling for different pin patterns
        
        if (pin_signature == "CK_D_Q") {
            return clock_edge + "|D_Q_CK";
        } else if (pin_signature == "CK") {
            // This might correspond to D_Q_QN_CK pattern (missing QN, D, Q due to debanking)
            return clock_edge + "|D_Q_QN_CK";
        } else if (pin_signature.find("CK") != std::string::npos) {
            // For more complex pin patterns, try to reorder
            std::set<std::string> pins;
            std::stringstream pin_ss(pin_signature);
            std::string pin;
            while (std::getline(pin_ss, pin, '_')) {
                pins.insert(pin);
            }
            
            // Reconstruct in canonical order: D, Q, QN, CK, SI, SE, R, S, RD, SD, RS, SR
            std::vector<std::string> ordered_pins;
            if (pins.count("D")) ordered_pins.push_back("D");
            if (pins.count("Q")) ordered_pins.push_back("Q");
            if (pins.count("QN")) ordered_pins.push_back("QN");
            if (pins.count("CK")) ordered_pins.push_back("CK");
            if (pins.count("SI")) ordered_pins.push_back("SI");
            if (pins.count("SE")) ordered_pins.push_back("SE");
            if (pins.count("R")) ordered_pins.push_back("R");
            if (pins.count("S")) ordered_pins.push_back("S");
            if (pins.count("RD")) ordered_pins.push_back("RD");
            if (pins.count("SD")) ordered_pins.push_back("SD");
            if (pins.count("RS")) ordered_pins.push_back("RS");
            if (pins.count("SR")) ordered_pins.push_back("SR");
            
            std::string canonical_signature;
            for (const auto& p : ordered_pins) {
                if (!canonical_signature.empty()) canonical_signature += "_";
                canonical_signature += p;
            }
            
            return clock_edge + "|" + canonical_signature;
        }
    }
    
    return instance_group_key; // fallback
}

void filter_banking_eligible_instance_groups(DesignDatabase& db) {
    std::cout << "  Filtering instance groups for banking candidates..." << std::endl;
    
    // Clear previous results
    db.banking_candidate_instance_groups.clear();
    
    int total_groups = 0;
    int groups_with_eligible_configs = 0;
    int groups_marked_for_banking = 0;
    
    // Process all FF instance groups
    for (const auto& group_pair : db.ff_instance_groups) {
        const std::string& instance_group_key = group_pair.first;
        const auto& instances = group_pair.second;
        
        total_groups++;
        
        if (instances.empty()) {
            std::cout << "    Skipping empty group: " << instance_group_key << std::endl;
            continue;
        }
        
        // Extract base compatibility key
        std::string base_key = extract_base_compatibility_key(instance_group_key);
        
        // Calculate current single-bit instance score
        std::string current_cell_type = instances[0]->cell_type;
        double current_single_bit_score = calculate_ff_score(current_cell_type, db);
        
        // Check if this base group has any eligible banking configurations
        bool has_eligible_configs = false;
        bool should_mark_for_banking = false;
        
        for (const auto& eligible_config : db.banking_eligible_groups) {
            // Check if eligible_config matches this base_key
            if (eligible_config.find(base_key) == 0) {  // prefix match
                has_eligible_configs = true;
                
                // Get optimal multi-bit FF for this configuration
                auto multi_bit_it = db.optimal_ff_for_groups.find(eligible_config);
                if (multi_bit_it != db.optimal_ff_for_groups.end()) {
                    double multi_bit_score = calculate_ff_score(multi_bit_it->second, db);
                    
                    // Core logic: Compare current single-bit score vs multi-bit score
                    // Both scores are already per-bit due to formula: Score = (β·Power + γ·Area)/bit + δ
                    if (current_single_bit_score > multi_bit_score) {
                        should_mark_for_banking = true;
                        
                        std::cout << "    Found banking opportunity in group [" << instance_group_key << "]:" << std::endl;
                        std::cout << "      Current: " << current_cell_type 
                                  << " (score: " << std::fixed << std::setprecision(6) << current_single_bit_score << ")" << std::endl;
                        std::cout << "      Better option: " << multi_bit_it->second 
                                  << " (score: " << std::fixed << std::setprecision(6) << multi_bit_score << ")" << std::endl;
                        std::cout << "      Improvement: " << std::fixed << std::setprecision(6) 
                                  << (current_single_bit_score - multi_bit_score) << std::endl;
                        break;
                    }
                }
            }
        }
        
        if (has_eligible_configs) {
            groups_with_eligible_configs++;
        }
        
        // Mark for banking if improvement found
        if (should_mark_for_banking) {
            db.banking_candidate_instance_groups.push_back(instance_group_key);
            groups_marked_for_banking++;
        }
    }
    
    std::cout << "    Instance group filtering completed:" << std::endl;
    std::cout << "      Total instance groups: " << total_groups << std::endl;
    std::cout << "      Groups with eligible banking configs: " << groups_with_eligible_configs << std::endl;
    std::cout << "      Groups marked for banking: " << groups_marked_for_banking << std::endl;
}

void export_banking_candidate_instance_groups_report(const DesignDatabase& db, const std::string& output_file) {
    std::cout << "  Exporting banking candidate instance groups report to: " << output_file << std::endl;
    
    std::ofstream out(output_file);
    if (!out.is_open()) {
        std::cerr << "Error: Cannot open " << output_file << " for writing" << std::endl;
        return;
    }
    
    out << "=== BANKING CANDIDATE INSTANCE GROUPS REPORT ===\n";
    out << "Generated: " << __DATE__ << " " << __TIME__ << "\n\n";
    
    // Summary
    out << "=== SUMMARY ===\n";
    out << "Total banking candidate instance groups: " << db.banking_candidate_instance_groups.size() << "\n\n";
    
    // Detailed information for each candidate group
    out << "=== BANKING CANDIDATE DETAILS ===\n\n";
    
    int group_num = 1;
    for (const auto& candidate_group_key : db.banking_candidate_instance_groups) {
        out << "Candidate Group " << group_num++ << ": [" << candidate_group_key << "]\n";
        
        // Find the actual instances in this group
        auto group_it = db.ff_instance_groups.find(candidate_group_key);
        if (group_it != db.ff_instance_groups.end()) {
            const auto& instances = group_it->second;
            out << "  Instance count: " << instances.size() << "\n";
            
            if (!instances.empty()) {
                // Current FF information
                std::string current_cell = instances[0]->cell_type;
                double current_score = calculate_ff_score(current_cell, db);
                out << "  Current FF: " << current_cell 
                    << " (score: " << std::fixed << std::setprecision(6) << current_score << ")\n";
                
                // Show available banking options
                std::string base_key = extract_base_compatibility_key(candidate_group_key);
                out << "  Available banking options:\n";
                
                for (const auto& eligible_config : db.banking_eligible_groups) {
                    if (eligible_config.find(base_key) == 0) {
                        auto multi_bit_it = db.optimal_ff_for_groups.find(eligible_config);
                        if (multi_bit_it != db.optimal_ff_for_groups.end()) {
                            double multi_bit_score = calculate_ff_score(multi_bit_it->second, db);
                            double improvement = current_score - multi_bit_score;
                            
                            out << "    " << eligible_config << ": " << multi_bit_it->second 
                                << " (score: " << std::fixed << std::setprecision(6) << multi_bit_score 
                                << ", improvement: " << std::fixed << std::setprecision(6) << improvement << ")\n";
                        }
                    }
                }
                
                // List some instance names
                out << "  Sample instances:\n";
                for (size_t i = 0; i < std::min(instances.size(), size_t(5)); i++) {
                    out << "    " << instances[i]->name << "\n";
                }
                if (instances.size() > 5) {
                    out << "    ... and " << (instances.size() - 5) << " more\n";
                }
            }
        }
        
        out << "\n";
    }
    
    out.close();
    std::cout << "    Banking candidate instance groups report exported successfully" << std::endl;
}

