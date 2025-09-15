// =============================================================================
// SCAN CHAIN DETECTION AND GROUPING
// =============================================================================
// This module implements automatic scan chain detection from netlist connections
// and creates banking groups within each scan chain
// =============================================================================

#include "parsers.hpp"
#include "timing_repr_hardcoded.hpp"
#include <set>
#include <queue>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <limits>

// =============================================================================
// SCAN CHAIN DETECTION
// =============================================================================

void detect_scan_chains(DesignDatabase& db) {
    std::cout << "  Detecting scan-capable FFs from netlist connections..." << std::endl;
    
    db.scan_chains.clear();
    
    // Get all flip-flop instances
    auto flip_flops = db.get_flip_flops();
    std::cout << "    Total flip-flops in design: " << flip_flops.size() << std::endl;
    if (flip_flops.empty()) {
        std::cout << "    No flip-flops found for scan chain detection" << std::endl;
        return;
    }
    
    // Find all FFs that have scan pins (SI and/or SE)
    // Note: In this testcase, there are no SO pins, only SI and SE
    std::vector<std::shared_ptr<Instance>> scan_capable_ffs;
    int connected_scan_ffs = 0;
    
    for (const auto& ff : flip_flops) {
        bool has_si = false, has_se = false;
        bool si_connected = false, se_connected = false;
        
        // Check if this FF has scan pins and their connection status
        for (const auto& conn : ff->connections) {
            Pin::FlipFlopPinType pin_type = classify_ff_pin_type(conn.pin_name);
            if (pin_type == Pin::FlipFlopPinType::FF_SCAN_INPUT) {
                has_si = true;
                if (conn.net_name != "UNCONNECTED" && 
                    conn.net_name.find("SYNOPSYS_UNCONNECTED") == std::string::npos) {
                    si_connected = true;
                }
            }
            if (pin_type == Pin::FlipFlopPinType::FF_SCAN_ENABLE) {
                has_se = true;
                if (conn.net_name != "UNCONNECTED" && 
                    conn.net_name.find("SYNOPSYS_UNCONNECTED") == std::string::npos) {
                    se_connected = true;
                }
            }
        }
        
        // FF is scan-capable if it has SI and/or SE pins
        if (has_si || has_se) {
            scan_capable_ffs.push_back(ff);
            // Count as actively connected scan FF if either SI or SE is connected
            if (si_connected || se_connected) {
                connected_scan_ffs++;
            }
        }
    }
    
    std::cout << "    Found " << scan_capable_ffs.size() << " scan-capable FFs (with SI/SE pins)" << std::endl;
    std::cout << "    Found " << connected_scan_ffs << " FFs with active scan connections" << std::endl;
    
    // For this testcase, there are no actual scan chains (all scan pins are UNCONNECTED)
    // So we return early and treat all FFs as non-scan FFs
    if (connected_scan_ffs == 0) {
        std::cout << "    No active scan chains detected (all scan pins are UNCONNECTED)" << std::endl;
        std::cout << "    All FFs will be treated as functional (non-scan) FFs for banking purposes" << std::endl;
        return;
    }
    
    // Performance optimization: if we have too many scan-capable FFs but no actual chains
    // (likely all connected to the same scan signals), return early after quick check
    if (scan_capable_ffs.size() > 10000) {
        std::cout << "    Large design detected (" << scan_capable_ffs.size() << " scan FFs)" << std::endl;
        std::cout << "    Performing quick scan chain validation..." << std::endl;
        
        // Quick check: count unique SI and SE nets
        std::set<std::string> unique_si_nets, unique_se_nets;
        for (const auto& ff : scan_capable_ffs) {
            for (const auto& conn : ff->connections) {
                Pin::FlipFlopPinType pin_type = classify_ff_pin_type(conn.pin_name);
                if (pin_type == Pin::FlipFlopPinType::FF_SCAN_INPUT && 
                    conn.net_name != "UNCONNECTED" && 
                    conn.net_name.find("SYNOPSYS_UNCONNECTED") == std::string::npos) {
                    unique_si_nets.insert(conn.net_name);
                }
                else if (pin_type == Pin::FlipFlopPinType::FF_SCAN_ENABLE && 
                         conn.net_name != "UNCONNECTED" && 
                         conn.net_name.find("SYNOPSYS_UNCONNECTED") == std::string::npos) {
                    unique_se_nets.insert(conn.net_name);
                }
            }
        }
        
        std::cout << "    Unique SI nets: " << unique_si_nets.size() 
                  << ", Unique SE nets: " << unique_se_nets.size() << std::endl;
        
        // If most FFs are connected to the same few scan signals, skip detailed chain building
        if (unique_si_nets.size() < 10 && unique_se_nets.size() < 10) {
            std::cout << "    Most FFs connected to same few scan signals - treating as functional FFs" << std::endl;
            return;
        }
    }
    
    // Simplified approach: just check for actual scan connectivity patterns
    std::cout << "    Building SI->SE connectivity map..." << std::endl;
    
    // Build maps: SI net -> FFs using it, SE net -> FFs using it
    std::unordered_map<std::string, std::vector<std::shared_ptr<Instance>>> si_net_to_ffs;
    std::unordered_map<std::string, std::vector<std::shared_ptr<Instance>>> se_net_to_ffs;
    
    for (const auto& ff : scan_capable_ffs) {
        for (const auto& conn : ff->connections) {
            Pin::FlipFlopPinType pin_type = classify_ff_pin_type(conn.pin_name);
            
            if (pin_type == Pin::FlipFlopPinType::FF_SCAN_INPUT && 
                conn.net_name != "UNCONNECTED" && 
                conn.net_name.find("SYNOPSYS_UNCONNECTED") == std::string::npos) {
                si_net_to_ffs[conn.net_name].push_back(ff);
            }
            else if (pin_type == Pin::FlipFlopPinType::FF_SCAN_ENABLE && 
                     conn.net_name != "UNCONNECTED" && 
                     conn.net_name.find("SYNOPSYS_UNCONNECTED") == std::string::npos) {
                se_net_to_ffs[conn.net_name].push_back(ff);
            }
        }
    }
    
    std::cout << "    Found " << si_net_to_ffs.size() << " unique SI nets, " 
              << se_net_to_ffs.size() << " unique SE nets" << std::endl;
    
    // Simple heuristic: if there are real scan chains, SI and SE nets should form chains
    // If most FFs are connected to same few nets, treat as functional FFs
    bool has_real_scan_chains = false;
    
    // Check if any SE net is connected to an SI net (forming a chain)
    for (const auto& se_pair : se_net_to_ffs) {
        const std::string& se_net = se_pair.first;
        if (si_net_to_ffs.count(se_net)) {
            // This SE net is also used as SI net - possible chain connection
            has_real_scan_chains = true;
            break;
        }
    }
    
    if (!has_real_scan_chains) {
        std::cout << "    No SE->SI connections found - all FFs treated as functional FFs" << std::endl;
        return;
    }
    
    std::cout << "    Building scan chains from SI->SE connections..." << std::endl;
    
    // Build complete scan chains by following SI->SE->SI connections
    int chain_counter = 0;
    std::set<std::string> visited_ffs;
    
    // Find all chain head FFs (FFs whose SI is NOT connected to any SE)
    std::vector<std::shared_ptr<Instance>> chain_heads;
    for (const auto& si_pair : si_net_to_ffs) {
        const std::string& si_net = si_pair.first;
        const auto& ffs_using_si = si_pair.second;
        
        // If this SI net is not produced by any SE, it's an external input (chain head)
        if (!se_net_to_ffs.count(si_net)) {
            for (const auto& ff : ffs_using_si) {
                chain_heads.push_back(ff);
            }
        }
    }
    
    std::cout << "    Found " << chain_heads.size() << " chain head FFs" << std::endl;
    
    // Build chains starting from each head
    for (const auto& head_ff : chain_heads) {
        if (visited_ffs.count(head_ff->name)) continue;
        
        ScanChain chain;
        chain.name = "chain_" + std::to_string(chain_counter++);
        
        // Follow the chain from head to tail
        std::shared_ptr<Instance> current_ff = head_ff;
        while (current_ff && !visited_ffs.count(current_ff->name)) {
            visited_ffs.insert(current_ff->name);
            
            // Add this FF to chain
            ScanChain::ScanConnection scan_conn;
            scan_conn.instance_name = current_ff->name;
            scan_conn.scan_in_pin = "SI";
            scan_conn.scan_out_pin = "SE";
            chain.chain_sequence.push_back(scan_conn);
            
            // Find next FF in chain by looking at SE output
            std::shared_ptr<Instance> next_ff = nullptr;
            for (const auto& se_pair : se_net_to_ffs) {
                const std::string& se_net = se_pair.first;
                const auto& ffs_producing_se = se_pair.second;
                
                // Check if current FF produces this SE net
                bool current_ff_produces_se = false;
                for (const auto& ff : ffs_producing_se) {
                    if (ff->name == current_ff->name) {
                        current_ff_produces_se = true;
                        break;
                    }
                }
                
                if (current_ff_produces_se) {
                    // This SE net is produced by current FF, find FF that consumes it as SI
                    if (si_net_to_ffs.count(se_net)) {
                        for (const auto& ff : si_net_to_ffs[se_net]) {
                            if (!visited_ffs.count(ff->name)) {
                                next_ff = ff;
                                break;
                            }
                        }
                    }
                    break;
                }
            }
            
            current_ff = next_ff;
        }
        
        if (!chain.chain_sequence.empty()) {
            db.scan_chains.push_back(chain);
        }
    }
    
    std::cout << "    Detected " << db.scan_chains.size() << " scan chains:" << std::endl;
    for (const auto& chain : db.scan_chains) {
        std::cout << "      " << chain.name << ": " << chain.length() << " FFs" << std::endl;
    }
}

// =============================================================================
// SCAN CHAIN GROUPING
// =============================================================================

void build_scan_chain_groups(DesignDatabase& db) {
    std::cout << "  Building scan chain banking groups..." << std::endl;
    
    if (db.scan_chains.empty()) {
        std::cout << "    No scan chains found, skipping scan chain grouping" << std::endl;
        return;
    }
    
    int total_groups = 0;
    
    for (const auto& chain : db.scan_chains) {
        std::cout << "    Analyzing chain " << chain.name << " (" << chain.length() << " FFs):" << std::endl;
        
        if (chain.chain_sequence.size() < 2) {
            std::cout << "      Chain too short for banking" << std::endl;
            continue;
        }
        
        // Group consecutive compatible FFs in this chain
        std::vector<std::vector<std::string>> chain_groups;
        std::vector<std::string> current_group;
        
        for (size_t i = 0; i < chain.chain_sequence.size(); i++) {
            const std::string& ff_name = chain.chain_sequence[i].instance_name;
            auto ff_inst = db.instances[ff_name];
            
            if (current_group.empty()) {
                // Start new group
                current_group.push_back(ff_name);
            } else {
                // Check compatibility with previous FF in group
                auto prev_ff = db.instances[current_group.back()];
                
                if (check_ff_compatibility(prev_ff, ff_inst, db)) {
                    // Add to current group
                    current_group.push_back(ff_name);
                } else {
                    // Start new group
                    if (current_group.size() >= 2) {
                        chain_groups.push_back(current_group);
                    }
                    current_group.clear();
                    current_group.push_back(ff_name);
                }
            }
        }
        
        // Don't forget the last group
        if (current_group.size() >= 2) {
            chain_groups.push_back(current_group);
        }
        
        std::cout << "      Found " << chain_groups.size() << " bankable groups in chain:" << std::endl;
        for (size_t g = 0; g < chain_groups.size(); g++) {
            std::cout << "        Group " << g << ": " << chain_groups[g].size() << " FFs (";
            for (size_t i = 0; i < std::min(chain_groups[g].size(), size_t(3)); i++) {
                if (i > 0) std::cout << ", ";
                std::cout << chain_groups[g][i];
            }
            if (chain_groups[g].size() > 3) std::cout << "...";
            std::cout << ")" << std::endl;
        }
        
        total_groups += chain_groups.size();
    }
    
    std::cout << "    Total scan chain banking groups: " << total_groups << std::endl;
}

// =============================================================================
// FF GROUPING REPORT EXPORT
// =============================================================================

void export_ff_grouping_report(DesignDatabase& db, const std::string& output_file) {
    std::cout << "  Exporting FF grouping report to: " << output_file << std::endl;
    
    std::ofstream out(output_file);
    if (!out.is_open()) {
        std::cerr << "Error: Cannot open " << output_file << " for writing" << std::endl;
        return;
    }
    
    // Report header
    out << "=== FLIP-FLOP BANKING GROUPING ANALYSIS REPORT ===" << std::endl;
    out << "Generated: " << __DATE__ << " " << __TIME__ << std::endl;
    out << std::endl;
    
    // Design statistics
    auto flip_flops = db.get_flip_flops();
    out << "=== DESIGN STATISTICS ===" << std::endl;
    out << "Total flip-flops: " << flip_flops.size() << std::endl;
    out << "Total scan chains: " << db.scan_chains.size() << std::endl;
    out << "Total nets: " << db.nets.size() << std::endl;
    out << "Total cell templates: " << db.cell_library.size() << std::endl;
    out << std::endl;
    
    // FF cell template compatibility groups with functional subdivision
    out << "=== FF CELL TEMPLATE COMPATIBILITY GROUPS ===" << std::endl;
    
    // Build hierarchical grouping and store in database permanently
    db.hierarchical_ff_groups.clear();
    
    for (const auto& cell_pair : db.cell_library) {
        const auto& cell = cell_pair.second;
        
        if (cell->type == CellTemplate::FLIP_FLOP) {
            std::string clock_edge = cell->get_clock_edge_string();
            
            // 生成pin interface signature（不包含clock edge）
            std::set<Pin::FlipFlopPinType> pin_types;
            for (const auto& pin : cell->pins) {
                Pin::FlipFlopPinType type = classify_ff_pin_type(pin.name);
                if (type != Pin::FF_VDDR && type != Pin::FF_OTHER && type != Pin::FF_NOT_FF_PIN) {
                    pin_types.insert(type);
                }
            }
            
            std::string pin_signature;
            for (auto type : pin_types) {
                if (!pin_signature.empty()) pin_signature += "_";
                
                switch (type) {
                    case Pin::FF_DATA_INPUT: pin_signature += "D"; break;
                    case Pin::FF_DATA_OUTPUT: pin_signature += "Q"; break;
                    case Pin::FF_DATA_OUTPUT_N: pin_signature += "QN"; break;
                    case Pin::FF_CLOCK: pin_signature += "CK"; break;
                    case Pin::FF_SCAN_INPUT: pin_signature += "SI"; break;
                    case Pin::FF_SCAN_OUTPUT: pin_signature += "SO"; break;
                    case Pin::FF_SCAN_ENABLE: pin_signature += "SE"; break;
                    case Pin::FF_RESET: pin_signature += "R"; break;
                    case Pin::FF_SET: pin_signature += "S"; break;
                    case Pin::FF_RD: pin_signature += "RD"; break;
                    case Pin::FF_SD: pin_signature += "SD"; break;
                    case Pin::FF_SR: pin_signature += "SR"; break;
                    case Pin::FF_RS: pin_signature += "RS"; break;
                    default: break;
                }
            }
            
            if (pin_signature.empty()) {
                pin_signature = "BASIC"; // 只有基本pins的FF
            }
            
            // 按bit width分組
            int bit_width = cell->bit_width;
            if (bit_width <= 0) bit_width = 1; // 預設為1 bit
            
            db.hierarchical_ff_groups[clock_edge][pin_signature][bit_width].push_back(cell->name);
        }
    }
    
    int total_groups = 0;
    for (const auto& edge_group : db.hierarchical_ff_groups) {
        for (const auto& pin_group : edge_group.second) {
            total_groups += pin_group.second.size();
        }
    }
    
    out << "Total compatibility groups: " << total_groups << std::endl;
    out << "(Hierarchical: " << db.hierarchical_ff_groups.size() << " clock edges, each with functional subgroups)" << std::endl;
    out << std::endl;
    
    for (const auto& edge_pair : db.hierarchical_ff_groups) {
        const std::string& clock_edge = edge_pair.first;
        const auto& pin_groups = edge_pair.second;
        
        out << "=== CLOCK EDGE [" << clock_edge << "] ====" << std::endl;
        out << "Functional subgroups: " << pin_groups.size() << std::endl;
        out << std::endl;
        
        for (const auto& pin_group : pin_groups) {
            const std::string& pin_signature = pin_group.first;
            const auto& bit_width_groups = pin_group.second;
            
            out << "--- Pin Interface [" << pin_signature << "] ---" << std::endl;
            out << "Bit width variants: " << bit_width_groups.size() << std::endl;
            
            for (const auto& bit_group : bit_width_groups) {
                const int bit_width = bit_group.first;
                const std::vector<std::string>& cells = bit_group.second;
                
                out << std::endl;
                out << "Group [" << clock_edge << "|" << pin_signature << "|" << bit_width << "bit]: " 
                    << cells.size() << " cell types" << std::endl;
                out << "  Compatible cell types (" << bit_width << "-bit variants):" << std::endl;
                
                // Calculate scores for all FFs in this group and find the best one
                std::vector<std::pair<std::string, double>> ff_scores;
                double best_score = std::numeric_limits<double>::max();
                std::string best_ff = "";
                
                for (const std::string& cell_name : cells) {
                    double score = calculate_ff_score(cell_name, db);
                    ff_scores.push_back({cell_name, score});
                    if (score < best_score) {
                        best_score = score;
                        best_ff = cell_name;
                    }
                }
                
                for (size_t i = 0; i < cells.size(); i++) {
                    out << "    " << (i+1) << ". " << cells[i];
                    
                    // Show instance count and bit width from cell template
                    int instance_count = 0;
                    int actual_bit_width = 1;
                    for (const auto& inst_pair : db.instances) {
                        if (inst_pair.second->cell_type == cells[i] && inst_pair.second->is_flip_flop()) {
                            instance_count++;
                            if (inst_pair.second->cell_template) {
                                actual_bit_width = std::max(1, inst_pair.second->cell_template->bit_width);
                            }
                        }
                    }
                    
                    // Add FF score information
                    double score = ff_scores[i].second;
                    out << " (" << instance_count << " instances, " << actual_bit_width << "-bit, score: " 
                        << std::fixed << std::setprecision(2) << score << ")";
                    
                    // Mark best FF in this group
                    if (cells[i] == best_ff) {
                        out << " ⭐ BEST";
                    }
                    
                    out << std::endl;
                }
                
                // Show group summary with best FF
                out << "  → Optimal FF for this group: " << best_ff 
                    << " (score: " << std::fixed << std::setprecision(2) << best_score << ")" << std::endl;
            }
            out << std::endl;
        }
    }
    
    // Scan chain analysis
    out << "=== SCAN CHAIN ANALYSIS ===" << std::endl;
    if (db.scan_chains.empty()) {
        out << "No scan chains detected in the design." << std::endl;
        out << std::endl;
        
        // If no scan chains, analyze functional FFs
        out << "=== FUNCTIONAL FF ANALYSIS ===" << std::endl;
        
        // Group functional FFs by clock domain
        std::map<std::string, std::vector<std::shared_ptr<Instance>>> clock_groups;
        
        for (const auto& ff : flip_flops) {
            // Find clock signal for this FF
            std::string clock_signal = "UNKNOWN";
            for (const auto& conn : ff->connections) {
                Pin::FlipFlopPinType pin_type = classify_ff_pin_type(conn.pin_name);
                if (pin_type == Pin::FlipFlopPinType::FF_CLOCK) {
                    clock_signal = conn.net_name;
                    break;
                }
            }
            clock_groups[clock_signal].push_back(ff);
        }
        
        out << "Clock domains found: " << clock_groups.size() << std::endl;
        out << std::endl;
        
        for (const auto& clock_pair : clock_groups) {
            const std::string& clock_signal = clock_pair.first;
            const auto& ffs = clock_pair.second;
            
            out << "Clock domain [" << clock_signal << "]: " << ffs.size() << " FFs" << std::endl;
            
            // Group by cell type within this clock domain
            std::map<std::string, std::vector<std::shared_ptr<Instance>>> cell_type_groups;
            for (const auto& ff : ffs) {
                cell_type_groups[ff->cell_type].push_back(ff);
            }
            
            for (const auto& type_pair : cell_type_groups) {
                const std::string& cell_type = type_pair.first;
                const auto& type_ffs = type_pair.second;
                
                out << "  Cell type " << cell_type << ": " << type_ffs.size() << " FFs" << std::endl;
                
                // Show first few instances as examples
                out << "    Examples: ";
                for (size_t i = 0; i < std::min(type_ffs.size(), size_t(5)); i++) {
                    if (i > 0) out << ", ";
                    out << type_ffs[i]->name;
                }
                if (type_ffs.size() > 5) {
                    out << "... +" << (type_ffs.size() - 5) << " more";
                }
                out << std::endl;
            }
            out << std::endl;
        }
        
    } else {
        // Detailed scan chain analysis
        for (size_t chain_idx = 0; chain_idx < db.scan_chains.size(); chain_idx++) {
            const auto& chain = db.scan_chains[chain_idx];
            
            out << "Scan Chain " << (chain_idx + 1) << ": " << chain.name << std::endl;
            out << "  Length: " << chain.length() << " FFs" << std::endl;
            out << "  Input: " << chain.scan_in_pin << " -> Output: " << chain.scan_out_pin << std::endl;
            out << std::endl;
            
            out << "  FF sequence:" << std::endl;
            for (size_t i = 0; i < chain.chain_sequence.size(); i++) {
                const auto& scan_conn = chain.chain_sequence[i];
                
                // Get instance details
                auto inst_it = db.instances.find(scan_conn.instance_name);
                if (inst_it != db.instances.end()) {
                    const auto& inst = inst_it->second;
                    
                    out << "    [" << (i+1) << "] " << inst->name << " (" << inst->cell_type << ")" << std::endl;
                    out << "        SI: " << scan_conn.scan_in_pin 
                        << " -> SO: " << scan_conn.scan_out_pin << std::endl;
                    
                    // Show position if available
                    if (inst->placement_status != Instance::UNPLACED) {
                        out << "        Position: (" << inst->position.x << ", " << inst->position.y << ")" << std::endl;
                    }
                }
            }
            out << std::endl;
            
            // Banking opportunities analysis
            out << "  Banking opportunities:" << std::endl;
            
            // Group consecutive compatible FFs
            std::vector<std::vector<std::string>> banking_groups;
            std::vector<std::string> current_group;
            
            for (size_t i = 0; i < chain.chain_sequence.size(); i++) {
                const std::string& ff_name = chain.chain_sequence[i].instance_name;
                auto ff_inst = db.instances.at(ff_name);
                
                if (current_group.empty()) {
                    current_group.push_back(ff_name);
                } else {
                    auto prev_ff = db.instances.at(current_group.back());
                    
                    // Simple compatibility check - same cell type
                    if (prev_ff->cell_type == ff_inst->cell_type) {
                        current_group.push_back(ff_name);
                    } else {
                        if (current_group.size() >= 2) {
                            banking_groups.push_back(current_group);
                        }
                        current_group.clear();
                        current_group.push_back(ff_name);
                    }
                }
            }
            
            // Don't forget the last group
            if (current_group.size() >= 2) {
                banking_groups.push_back(current_group);
            }
            
            if (banking_groups.empty()) {
                out << "    No banking opportunities found (all FFs have different types)" << std::endl;
            } else {
                out << "    Found " << banking_groups.size() << " potential banking groups:" << std::endl;
                
                for (size_t g = 0; g < banking_groups.size(); g++) {
                    const auto& group = banking_groups[g];
                    auto first_inst = db.instances.at(group[0]);
                    
                    out << "      Group " << (g+1) << ": " << group.size() 
                        << " consecutive FFs of type " << first_inst->cell_type << std::endl;
                    out << "        FFs: ";
                    for (size_t j = 0; j < std::min(group.size(), size_t(3)); j++) {
                        if (j > 0) out << ", ";
                        out << group[j];
                    }
                    if (group.size() > 3) {
                        out << "... +" << (group.size() - 3) << " more";
                    }
                    out << std::endl;
                }
            }
            out << std::endl;
        }
    }
    
    // Summary recommendations
    out << "=== BANKING RECOMMENDATIONS ===" << std::endl;
    
    int total_banking_opportunities = 0;
    int total_potential_savings = 0;
    
    for (const auto& chain : db.scan_chains) {
        // Simple analysis - count consecutive same-type FFs
        std::vector<int> group_sizes;
        int current_size = 1;
        std::string current_type = "";
        
        for (const auto& scan_conn : chain.chain_sequence) {
            auto inst = db.instances.at(scan_conn.instance_name);
            
            if (current_type.empty()) {
                current_type = inst->cell_type;
            } else if (inst->cell_type == current_type) {
                current_size++;
            } else {
                if (current_size >= 2) {
                    group_sizes.push_back(current_size);
                }
                current_type = inst->cell_type;
                current_size = 1;
            }
        }
        
        // Don't forget last group
        if (current_size >= 2) {
            group_sizes.push_back(current_size);
        }
        
        for (int size : group_sizes) {
            total_banking_opportunities++;
            total_potential_savings += (size - 1); // Each n-bit bank saves (n-1) cells
        }
    }
    
    out << "Total potential banking groups: " << total_banking_opportunities << std::endl;
    out << "Estimated FF count reduction: " << total_potential_savings << " FFs" << std::endl;
    if (!flip_flops.empty()) {
        double savings_percentage = (double)total_potential_savings / flip_flops.size() * 100.0;
        out << "Potential area/power savings: " << std::fixed << std::setprecision(1) 
            << savings_percentage << "%" << std::endl;
    }
    
    out << std::endl;
    out << "Report generation completed." << std::endl;
    
    out.close();
    std::cout << "  FF grouping report exported successfully" << std::endl;
}