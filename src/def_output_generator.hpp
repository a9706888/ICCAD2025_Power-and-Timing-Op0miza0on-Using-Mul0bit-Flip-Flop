#pragma once
#include "data_structures.hpp"
#include <fstream>
#include <sstream>

// =============================================================================
// DEF OUTPUT GENERATOR FOR ICCAD 2025 MULTI-BIT FF BANKING CONTEST
// =============================================================================

class DefOutputGenerator {
private:
    const DesignDatabase& db;
    
    // DEF文件的各個sections（從原始文件保存）
    struct DefSectionData {
        std::vector<std::string> header_lines;        // VERSION to DIEAREA
        std::vector<std::string> row_lines;          // ROW definitions
        std::vector<std::string> combinational_components; // 非FF的COMPONENTS
        std::vector<std::string> pins_lines;         // PINS section
        std::vector<std::string> pinproperties_lines; // PINPROPERTIES section (optional)
        std::vector<std::string> blockages_lines;    // BLOCKAGES section (optional)
        std::vector<std::string> specialnets_lines;  // SPECIALNETS section (optional)
        std::vector<std::string> footer_lines;       // END DESIGN等
        
        // NET parsing structures
        struct NetConnection {
            std::string instance_name;
            std::string pin_name;
        };
        
        struct Net {
            std::string name;
            std::vector<NetConnection> connections;
            std::string use_type = "SIGNAL";
        };
        
        std::vector<Net> original_nets;
        
        // Statistics
        int total_components_count = 0;
        int pins_count = 0;
        int pinproperties_count = 0;
        int blockages_count = 0;
        int specialnets_count = 0;
    } def_sections;
    
public:
    DefOutputGenerator(const DesignDatabase& database) : db(database) {}
    
    // Main interface - generate complete DEF file up to NETS section
    void generate_def_up_to_nets(const std::string& input_def_path, 
                                 const std::string& output_def_path);
    
    // Generate complete DEF file including NETS section
    void generate_complete_def_file(const std::string& input_def_path,
                                   const std::string& output_def_path);
    
private:
    // Parse original DEF file and save sections we need to copy
    void parse_and_store_original_def_sections(const std::string& input_def_path);
    
    // Write individual sections
    void write_header_section(std::ofstream& out);
    void write_row_section(std::ofstream& out);
    void write_components_section(std::ofstream& out);
    void write_pins_section(std::ofstream& out);
    void write_pinproperties_section(std::ofstream& out);
    void write_blockages_section(std::ofstream& out);
    void write_specialnets_section(std::ofstream& out);
    void write_nets_section(std::ofstream& out);
    
    // NET-specific parsing and processing
    void parse_original_nets_from_def(const std::string& input_def_path);
    void build_wire_to_final_connections_mapping(std::map<std::string, std::vector<DefSectionData::NetConnection>>& mapping);
    bool is_combinational_pin(const std::string& pin_name);
    std::string normalize_net_name(const std::string& net_name);
    
    // Helper functions
    std::string get_def_instance_name(const std::shared_ptr<Instance>& instance);
    std::string get_def_orientation(const std::shared_ptr<Instance>& instance);
    bool is_flip_flop_instance(const std::string& instance_name);
    
    // Get final legalized FF instances (LEGALIZE stage from pipeline)
    std::vector<std::shared_ptr<Instance>> get_final_ff_instances();
};

// =============================================================================
// IMPLEMENTATION
// =============================================================================

void DefOutputGenerator::generate_def_up_to_nets(const std::string& input_def_path,
                                                 const std::string& output_def_path) {
    std::cout << "🔨 Generating DEF output up to NETS section..." << std::endl;
    std::cout << "  Input:  " << input_def_path << std::endl;
    std::cout << "  Output: " << output_def_path << std::endl;
    
    // Step 1: Parse and store original DEF sections
    parse_and_store_original_def_sections(input_def_path);
    
    // Step 2: Generate output DEF file
    std::ofstream out(output_def_path);
    if (!out.is_open()) {
        throw std::runtime_error("Cannot create output DEF file: " + output_def_path);
    }
    
    // Write all sections up to NETS
    write_header_section(out);
    write_row_section(out);
    write_components_section(out);
    write_pins_section(out);
    write_pinproperties_section(out);
    write_blockages_section(out);
    write_specialnets_section(out);
    
    out.close();
    std::cout << "  ✓ DEF file generated successfully (up to NETS section)" << std::endl;
}

void DefOutputGenerator::generate_complete_def_file(const std::string& input_def_path,
                                                   const std::string& output_def_path) {
    std::cout << "🔨 Generating complete DEF output including NETS section..." << std::endl;
    std::cout << "  Input:  " << input_def_path << std::endl;
    std::cout << "  Output: " << output_def_path << std::endl;
    
    // Step 1: Parse and store original DEF sections (including NETS)
    parse_and_store_original_def_sections(input_def_path);
    parse_original_nets_from_def(input_def_path);
    
    // Step 2: Generate output DEF file
    std::ofstream out(output_def_path);
    if (!out.is_open()) {
        throw std::runtime_error("Cannot create output DEF file: " + output_def_path);
    }
    
    // Write all sections including NETS
    write_header_section(out);
    write_row_section(out);
    write_components_section(out);
    write_pins_section(out);
    write_pinproperties_section(out);
    write_blockages_section(out);
    write_specialnets_section(out);
    write_nets_section(out);
    
    out << "END DESIGN" << std::endl;
    out.close();
    std::cout << "  ✓ Complete DEF file generated successfully" << std::endl;
}

void DefOutputGenerator::parse_and_store_original_def_sections(const std::string& input_def_path) {
    std::cout << "  📖 Parsing original DEF sections..." << std::endl;
    
    std::ifstream file(input_def_path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open input DEF file: " + input_def_path);
    }
    
    std::string line;
    std::string current_section = "HEADER";
    
    while (std::getline(file, line)) {
        std::string trimmed = line;
        
        // Determine current section
        if (trimmed.find("ROW ") == 0) {
            current_section = "ROW";
        }
        else if (trimmed.find("COMPONENTS ") == 0) {
            current_section = "COMPONENTS";
            // Extract component count
            std::istringstream iss(trimmed);
            std::string word;
            iss >> word >> def_sections.total_components_count;
            continue; // Don't store the COMPONENTS line itself
        }
        else if (trimmed == "END COMPONENTS") {
            current_section = "POST_COMPONENTS";
            continue;
        }
        else if (trimmed.find("PINS ") == 0) {
            current_section = "PINS";
            // Extract pins count
            std::istringstream iss(trimmed);
            std::string word;
            iss >> word >> def_sections.pins_count;
        }
        else if (trimmed == "END PINS") {
            def_sections.pins_lines.push_back(line);
            current_section = "POST_PINS";
            continue;
        }
        else if (trimmed.find("PINPROPERTIES ") == 0) {
            current_section = "PINPROPERTIES";
            // Extract pinproperties count
            std::istringstream iss(trimmed);
            std::string word;
            iss >> word >> def_sections.pinproperties_count;
        }
        else if (trimmed == "END PINPROPERTIES") {
            def_sections.pinproperties_lines.push_back(line);
            current_section = "POST_PINPROPERTIES";
            continue;
        }
        else if (trimmed.find("BLOCKAGES ") == 0) {
            current_section = "BLOCKAGES";
            // Extract blockages count
            std::istringstream iss(trimmed);
            std::string word;
            iss >> word >> def_sections.blockages_count;
        }
        else if (trimmed == "END BLOCKAGES") {
            def_sections.blockages_lines.push_back(line);
            current_section = "POST_BLOCKAGES";
            continue;
        }
        else if (trimmed.find("SPECIALNETS ") == 0) {
            current_section = "SPECIALNETS";
            // Extract specialnets count
            std::istringstream iss(trimmed);
            std::string word;
            iss >> word >> def_sections.specialnets_count;
        }
        else if (trimmed == "END SPECIALNETS") {
            def_sections.specialnets_lines.push_back(line);
            current_section = "POST_SPECIALNETS";
            continue;
        }
        else if (trimmed.find("NETS ") == 0) {
            // We stop here - NETS section will be handled separately
            break;
        }
        
        // Store line in appropriate section
        if (current_section == "HEADER") {
            def_sections.header_lines.push_back(line);
        }
        else if (current_section == "ROW") {
            def_sections.row_lines.push_back(line);
        }
        else if (current_section == "COMPONENTS") {
            // Only store non-FF components for later copying
            if (line.find("SNPS") != std::string::npos) {
                // Check if this is a flip-flop - use same patterns as in get_final_ff_instances
                bool is_ff = false;
                if (line.find("FDN") != std::string::npos ||
                    line.find("FSD") != std::string::npos ||
                    line.find("FDP") != std::string::npos ||
                    line.find("LSRD") != std::string::npos ||
                    line.find("SSRR") != std::string::npos) {
                    is_ff = true;
                }
                
                if (!is_ff) {
                    def_sections.combinational_components.push_back(line);
                }
            } else {
                // Non-SNPS component, keep it
                def_sections.combinational_components.push_back(line);
            }
        }
        else if (current_section == "PINS") {
            def_sections.pins_lines.push_back(line);
        }
        else if (current_section == "PINPROPERTIES") {
            def_sections.pinproperties_lines.push_back(line);
        }
        else if (current_section == "BLOCKAGES") {
            def_sections.blockages_lines.push_back(line);
        }
        else if (current_section == "SPECIALNETS") {
            def_sections.specialnets_lines.push_back(line);
        }
    }
    
    file.close();
    
    std::cout << "    ✓ Header lines: " << def_sections.header_lines.size() << std::endl;
    std::cout << "    ✓ ROW lines: " << def_sections.row_lines.size() << std::endl;
    std::cout << "    ✓ Combinational components: " << def_sections.combinational_components.size() << std::endl;
    std::cout << "    ✓ PINS lines: " << def_sections.pins_lines.size() << std::endl;
    std::cout << "    ✓ PINPROPERTIES lines: " << def_sections.pinproperties_lines.size() << std::endl;
    std::cout << "    ✓ BLOCKAGES lines: " << def_sections.blockages_lines.size() << std::endl;
    std::cout << "    ✓ SPECIALNETS lines: " << def_sections.specialnets_lines.size() << std::endl;
}

void DefOutputGenerator::write_header_section(std::ofstream& out) {
    // Write all header lines (VERSION, DIVIDERCHAR, DESIGN, UNITS, DIEAREA)
    for (const auto& line : def_sections.header_lines) {
        out << line << std::endl;
    }
}

void DefOutputGenerator::write_row_section(std::ofstream& out) {
    // Write all ROW definitions exactly as they were
    for (const auto& line : def_sections.row_lines) {
        out << line << std::endl;
    }
}

void DefOutputGenerator::write_components_section(std::ofstream& out) {
    // Get final FF instances after legalization
    auto final_ff_instances = get_final_ff_instances();
    
    // Calculate total component count (FF + combinational)
    int total_components = final_ff_instances.size() + def_sections.combinational_components.size();
    
    out << "COMPONENTS " << total_components << " ;" << std::endl;
    
    // Write FF instances with updated placement positions
    std::cout << "  📍 Writing " << final_ff_instances.size() << " FF instances..." << std::endl;
    for (const auto& instance : final_ff_instances) {
        std::string def_name = get_def_instance_name(instance);
        std::string cell_type = instance->cell_template->name;
        std::string orientation = get_def_orientation(instance);
        
        out << " - " << def_name << " " << cell_type 
            << " + PLACED ( " 
            << static_cast<int>(instance->x_new) << " " 
            << static_cast<int>(instance->y_new) << " ) " 
            << orientation << " ;" << std::endl;
    }
    
    // Write combinational components exactly as they were
    std::cout << "  🔧 Writing " << def_sections.combinational_components.size() << " combinational components..." << std::endl;
    for (const auto& line : def_sections.combinational_components) {
        out << line << std::endl;
    }
    
    out << "END COMPONENTS" << std::endl;
}

void DefOutputGenerator::write_pins_section(std::ofstream& out) {
    // Write PINS section exactly as it was in the original
    for (const auto& line : def_sections.pins_lines) {
        out << line << std::endl;
    }
}

void DefOutputGenerator::write_pinproperties_section(std::ofstream& out) {
    // Write PINPROPERTIES section if it exists
    if (!def_sections.pinproperties_lines.empty()) {
        out << "PINPROPERTIES " << def_sections.pinproperties_count << " ;" << std::endl;
        for (const auto& line : def_sections.pinproperties_lines) {
            if (line.find("PINPROPERTIES") != 0 && line.find("END PINPROPERTIES") != 0) {
                out << line << std::endl;
            } else if (line.find("END PINPROPERTIES") == 0) {
                out << line << std::endl;
            }
        }
    }
}

void DefOutputGenerator::write_blockages_section(std::ofstream& out) {
    // Write BLOCKAGES section if it exists
    if (!def_sections.blockages_lines.empty()) {
        out << "BLOCKAGES " << def_sections.blockages_count << " ;" << std::endl;
        for (const auto& line : def_sections.blockages_lines) {
            if (line.find("BLOCKAGES") != 0 && line.find("END BLOCKAGES") != 0) {
                out << line << std::endl;
            } else if (line.find("END BLOCKAGES") == 0) {
                out << line << std::endl;
            }
        }
    }
}

void DefOutputGenerator::write_specialnets_section(std::ofstream& out) {
    // Write SPECIALNETS section if it exists
    if (!def_sections.specialnets_lines.empty()) {
        out << "SPECIALNETS " << def_sections.specialnets_count << " ;" << std::endl;
        for (const auto& line : def_sections.specialnets_lines) {
            if (line.find("SPECIALNETS") != 0 && line.find("END SPECIALNETS") != 0) {
                out << line << std::endl;
            } else if (line.find("END SPECIALNETS") == 0) {
                out << line << std::endl;
            }
        }
    }
}

std::string DefOutputGenerator::get_def_instance_name(const std::shared_ptr<Instance>& instance) {
    // Fix hierarchy prefix issue: instance->name already contains the correct full path
    // DEF files expect the full hierarchical path as stored in instance->name
    return instance->name;
}

std::string DefOutputGenerator::get_def_orientation(const std::shared_ptr<Instance>& instance) {
    // For now, return "N" (North) as default
    // This could be enhanced to support actual orientation optimization
    return "N";
}

std::vector<std::shared_ptr<Instance>> DefOutputGenerator::get_final_ff_instances() {
    std::vector<std::shared_ptr<Instance>> ff_instances;
    std::set<std::string> result_instance_names;
    
    // Get final result instances from transformation history
    for (const auto& record : db.transformation_history) {
        result_instance_names.insert(record.result_instance_name);
    }
    
    // Find the corresponding instances in db.instances
    for (const std::string& instance_name : result_instance_names) {
        auto inst_it = db.instances.find(instance_name);
        if (inst_it != db.instances.end()) {
            const auto& instance = inst_it->second;
            
            // Verify this is a flip-flop (should be, but double-check)
            if (instance->cell_template && 
                (instance->cell_template->name.find("FDN") != std::string::npos ||
                 instance->cell_template->name.find("FSD") != std::string::npos ||
                 instance->cell_template->name.find("FDP") != std::string::npos ||
                 instance->cell_template->name.find("LSRD") != std::string::npos||
                 instance->cell_template->name.find("SSRR") != std::string::npos)) {
                ff_instances.push_back(instance);
            }
        }
    }
    
    std::cout << "    ✓ Found " << ff_instances.size() << " final FF instances for DEF output" << std::endl;
    std::cout << "    ✓ From " << result_instance_names.size() << " transformation result names" << std::endl;
    return ff_instances;
}

void DefOutputGenerator::parse_original_nets_from_def(const std::string& input_def_path) {
    std::cout << "  📖 Parsing original NETS section..." << std::endl;
    
    std::ifstream file(input_def_path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open input DEF file for NETS parsing: " + input_def_path);
    }
    
    std::string line;
    bool in_nets_section = false;
    DefSectionData::Net current_net;
    
    while (std::getline(file, line)) {
        std::string trimmed = line;
        // Remove leading/trailing whitespace
        size_t start = trimmed.find_first_not_of(" \t");
        if (start != std::string::npos) {
            trimmed = trimmed.substr(start);
        }
        
        if (trimmed.find("NETS ") == 0) {
            in_nets_section = true;
            continue;
        }
        
        if (trimmed == "END NETS") {
            // Add the last net if it has a name
            if (!current_net.name.empty()) {
                def_sections.original_nets.push_back(current_net);
            }
            break;
        }
        
        if (in_nets_section) {
            if (trimmed.find("- ") == 0) {
                // New net definition
                if (!current_net.name.empty()) {
                    // Save previous net
                    def_sections.original_nets.push_back(current_net);
                }
                
                // Parse net name
                current_net = DefSectionData::Net();
                current_net.name = trimmed.substr(2); // Remove "- "
            }
            else if (trimmed.find("( ") == 0 && trimmed.find(" )") != std::string::npos) {
                // Parse connection: ( instance_name pin_name )
                size_t start_paren = trimmed.find("( ");
                size_t end_paren = trimmed.find(" )");
                if (start_paren != std::string::npos && end_paren != std::string::npos) {
                    std::string connection_str = trimmed.substr(start_paren + 2, end_paren - start_paren - 2);
                    std::istringstream iss(connection_str);
                    std::string instance_name, pin_name;
                    if (iss >> instance_name >> pin_name) {
                        DefSectionData::NetConnection conn;
                        conn.instance_name = instance_name;
                        conn.pin_name = pin_name;
                        current_net.connections.push_back(conn);
                    }
                }
            }
            else if (trimmed.find("+ USE ") == 0) {
                // Parse USE type
                std::istringstream iss(trimmed);
                std::string plus, use, type;
                if (iss >> plus >> use >> type) {
                    current_net.use_type = type;
                }
            }
        }
    }
    
    file.close();
    std::cout << "    ✓ Parsed " << def_sections.original_nets.size() << " original nets" << std::endl;
}

std::string DefOutputGenerator::normalize_net_name(const std::string& net_name) {
    std::string normalized = net_name;
    
    // Step 1: Remove leading backslash if present (for transformation record wires like \q_mid12[586])
    if (!normalized.empty() && normalized[0] == '\\') {
        normalized = normalized.substr(1);
    }
    
    // Step 2: Remove hierarchy prefixes (anything before the last '/')
    size_t last_slash = normalized.find_last_of('/');
    if (last_slash != std::string::npos) {
        normalized = normalized.substr(last_slash + 1);
    }
    
    // Step 3: Remove ALL backslashes (DEF escaping like qo_foo13\[496\] → qo_foo13[496])
    std::string result;
    for (char c : normalized) {
        if (c != '\\') {
            result += c;
        }
    }
    
    return result;
}

void DefOutputGenerator::build_wire_to_final_connections_mapping(
    std::map<std::string, std::vector<DefSectionData::NetConnection>>& mapping) {
    
    std::cout << "  🔗 Building wire to final connections mapping..." << std::endl;
    
    // Build mapping from normalized wire names to final FF connections
    // Include ALL FF instances, not just those from transformation_history
    int total_ff_count = 0;
    int unconnected_pin_count = 0;
    
    for (const auto& inst_pair : db.instances) {
        const auto& instance = inst_pair.second;
        
        // Only process flip-flop instances
        if (!instance->is_flip_flop()) {
            continue;
        }
        
        total_ff_count++;
        
        // Validate pins against current cell template to avoid phantom pins
        if (!instance->cell_template) {
            continue; // Skip instances without valid cell templates
        }
        
        // Build set of valid pins from current cell template
        std::set<std::string> valid_pins;
        for (const auto& pin : instance->cell_template->pins) {
            valid_pins.insert(pin.name);
        }
        
        // Go through all pin connections of this FF instance
        for (const auto& connection : instance->connections) {
            const std::string& pin_name = connection.pin_name;
            const std::string& net_name = connection.net_name;
            
            // CRITICAL: Only include pins that exist in current cell template
            if (valid_pins.find(pin_name) == valid_pins.end()) {
                // Pin doesn't exist in current cell template - skip it
                continue;
            }
            
            DefSectionData::NetConnection final_conn;
            final_conn.instance_name = instance->name;
            final_conn.pin_name = pin_name;
            
            if (net_name == "UNCONNECTED") {
                // Special handling for UNCONNECTED pins
                mapping["UNCONNECTED"].push_back(final_conn);
                unconnected_pin_count++;
            } else {
                // All nets including VSS, VDD, and regular signal nets
                std::string normalized_net = normalize_net_name(net_name);
                mapping[normalized_net].push_back(final_conn);
            }
        }
    }
    
    std::cout << "    ✓ Built mapping for " << mapping.size() << " wires" << std::endl;
    std::cout << "    ✓ Processed " << total_ff_count << " FF instances" << std::endl;
    std::cout << "    ✓ Found " << unconnected_pin_count << " UNCONNECTED pins" << std::endl;
}

bool DefOutputGenerator::is_combinational_pin(const std::string& pin_name) {
    // Combinational gates typically use A*, X* pins
    return pin_name.length() > 0 && (pin_name[0] == 'A' || pin_name[0] == 'X');
}

void DefOutputGenerator::write_nets_section(std::ofstream& out) {
    std::cout << "  🔌 Writing NETS section..." << std::endl;
    
    // Step 1: Build mapping from wire names to final FF connections
    std::map<std::string, std::vector<DefSectionData::NetConnection>> wire_to_final_connections;
    build_wire_to_final_connections_mapping(wire_to_final_connections);
    
    // Step 2: Collect final FF pins that connect to UNCONNECTED
    std::vector<DefSectionData::NetConnection> new_unconnected_pins;
    for (const auto& wire_pair : wire_to_final_connections) {
        if (wire_pair.first == "UNCONNECTED") {
            for (const auto& conn : wire_pair.second) {
                new_unconnected_pins.push_back(conn);
            }
        }
    }
    
    // Step 3: Calculate total nets count (all original nets + 1 for new UNCONNECTED if needed)
    int total_nets = def_sections.original_nets.size();
    if (!new_unconnected_pins.empty()) {
        total_nets += 1; // Add one new UNCONNECTED net
    }
    
    out << "NETS " << total_nets << " ;" << std::endl;
    
    // Step 4: Process all original nets
    int nets_processed = 0;
    int nets_updated = 0;
    
    for (const auto& original_net : def_sections.original_nets) {
        out << " - " << original_net.name << std::endl;
        
        // Process each connection in this net
        bool net_has_ff_updates = false;
        
        // First, write any final FF connections for this net
        // Use normalized net name for matching
        std::string normalized_original_net = normalize_net_name(original_net.name);
        if (wire_to_final_connections.count(normalized_original_net)) {
            for (const auto& final_conn : wire_to_final_connections[normalized_original_net]) {
                out << "   ( " << final_conn.instance_name << " " << final_conn.pin_name << " )" << std::endl;
            }
            net_has_ff_updates = true;
        }
        
        // Then, process original connections
        bool is_synopsys_unconnected = (original_net.name.find("SYNOPSYS_UNCONNECTED") != std::string::npos);
        
        for (const auto& orig_conn : original_net.connections) {
            if (is_combinational_pin(orig_conn.pin_name)) {
                // Combinational pins (A*, X*) → always copy
                out << "   ( " << orig_conn.instance_name << " " << orig_conn.pin_name << " )" << std::endl;
            } else {
                // Non-combinational pins (FF or latch pins)
                if (is_synopsys_unconnected) {
                    // For SYNOPSYS_UNCONNECTED nets, skip FF pins (QN, Q, D, etc.)
                    // Only A*, X* pins are copied above
                } else {
                    // For regular nets, only copy if this net doesn't have final FF connections
                    if (!net_has_ff_updates) {
                        out << "   ( " << orig_conn.instance_name << " " << orig_conn.pin_name << " )" << std::endl;
                    }
                    // If net_has_ff_updates is true, skip original FF connections 
                    // because they've been replaced by final FF connections above
                }
            }
        }
        
        if (net_has_ff_updates) {
            nets_updated++;
        }
        
        // Write USE clause
        out << "   + USE " << original_net.use_type;
        if (original_net.use_type.find(" ;") == std::string::npos) {
            out << " ;";
        }
        out << std::endl;
        
        nets_processed++;
    }
    
    // Step 5: Write new UNCONNECTED net for final FF pins that connect to UNCONNECTED
    if (!new_unconnected_pins.empty()) {
        out << " - UNCONNECTED" << std::endl;
        for (const auto& conn : new_unconnected_pins) {
            out << "   ( " << conn.instance_name << " " << conn.pin_name << " )" << std::endl;
        }
        out << "   + USE SIGNAL ;" << std::endl;
        nets_processed++;
    }
    
    out << "END NETS" << std::endl;
    
    std::cout << "    ✓ Processed " << nets_processed << " nets" << std::endl;
    std::cout << "    ✓ Updated " << nets_updated << " nets with final FF connections" << std::endl;
    std::cout << "    ✓ Preserved " << (nets_processed - nets_updated) << " nets with original connections" << std::endl;
}