#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <iostream>
#include <map>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <limits>

// =============================================================================
// CLEAN UNIFIED DATA STRUCTURES FOR FLIP-FLOP BANKING COMPETITION
// =============================================================================
// 設計原則：
// 1. 一個結構服務所有parser，不再有轉換
// 2. 簡單清晰的命名和組織
// 3. 包含所有EDA資訊但避免過度複雜化
// =============================================================================

// Forward declarations
struct CellTemplate;
struct Instance;
struct Net;
struct Pin;

// =============================================================================
// 1. BASIC GEOMETRIC TYPES
// =============================================================================

struct Point {
    double x = 0.0;
    double y = 0.0;
    
    Point() = default;
    Point(double x_, double y_) : x(x_), y(y_) {}
    
    double distance_to(const Point& other) const {
        double dx = x - other.x;
        double dy = y - other.y;
        return sqrt(dx*dx + dy*dy);
    }
};

struct Rectangle {
    double x1 = 0.0, y1 = 0.0;  // Lower-left
    double x2 = 0.0, y2 = 0.0;  // Upper-right
    
    double width() const { return x2 - x1; }
    double height() const { return y2 - y1; }
    double area() const { return width() * height(); }
};

// =============================================================================
// 2. PIN STRUCTURE (from LEF + Verilog connections)
// =============================================================================

struct Pin {
    std::string name;                // e.g., "D", "Q", "CK"
    
    // Pin direction (from LEF)
    enum Direction { 
        INPUT, OUTPUT, INOUT, UNKNOWN_DIR 
    } direction = UNKNOWN_DIR;
    
    // Pin usage (from LEF)
    enum Usage { 
        SIGNAL, CLOCK, POWER, GROUND, UNKNOWN_USE 
    } usage = UNKNOWN_USE;
    
    // Flip-flop specific pin type (for detailed classification)
    enum FlipFlopPinType {
        FF_DATA_INPUT,      // D pin
        FF_DATA_OUTPUT,     // Q pin (normal output)
        FF_DATA_OUTPUT_N,   // QN pin (inverted output)  
        FF_CLOCK,          // CLK, CK pin
        FF_SCAN_INPUT,     // SI pin
        FF_SCAN_OUTPUT,    // SO pin
        FF_SCAN_ENABLE,    // SE pin
        FF_RESET,          // R, RST pin
        FF_SET,            // S, SET pin
        FF_RD,             // RD pin (specific reset/disable)
        FF_SD,             // SD pin (specific set/disable)  
        FF_SR,             // SR pin (set/reset combo)
        FF_RS,             // RS pin (reset/set combo)
        FF_VDDR,           // VDDR pin (retention power)
        FF_OTHER,          // Other flip-flop pins
        FF_NOT_FF_PIN      // Not a flip-flop pin
    } ff_pin_type = FF_NOT_FF_PIN;
    
    // Physical location relative to cell origin (from LEF)
    Point offset;
    
    // Connection info (filled during netlist parsing)
    std::string connected_net_name;
    
    std::string get_ff_pin_type_string() const {
        switch (ff_pin_type) {
            case FF_DATA_INPUT: return "D";
            case FF_DATA_OUTPUT: return "Q";
            case FF_DATA_OUTPUT_N: return "QN";
            case FF_CLOCK: return "CLK";
            case FF_SCAN_INPUT: return "SI";
            case FF_SCAN_OUTPUT: return "SO";
            case FF_SCAN_ENABLE: return "SE";
            case FF_RESET: return "RST";
            case FF_SET: return "S";
            case FF_RD: return "RD";
            case FF_SD: return "SD";
            case FF_SR: return "SR";
            case FF_RS: return "RS";
            case FF_VDDR: return "VDDR";
            case FF_OTHER: return "FF_OTHER";
            case FF_NOT_FF_PIN: return "N/A";
            default: return "UNKNOWN";
        }
    }
    
    void print() const {
        std::cout << "    Pin " << name << " (" 
                  << (direction == INPUT ? "IN" : direction == OUTPUT ? "OUT" : "INOUT")
                  << ", " << (usage == CLOCK ? "CLK" : "SIG");
        
        // 如果是flip-flop pin，顯示詳細類型
        if (ff_pin_type != FF_NOT_FF_PIN) {
            std::cout << ", " << get_ff_pin_type_string();
        }
        
        std::cout << ")";
        if (!connected_net_name.empty()) {
            std::cout << " -> " << connected_net_name;
        }
        std::cout << std::endl;
    }
};

// =============================================================================
// 3. CELL TEMPLATE (LEF + Liberty combined)
// =============================================================================

struct CellTemplate {
    std::string name;                // e.g., "SNPSHOPT25_FSDN_V2_1"
    std::string library;             // "hopt", "lopt", etc.
    
    // Physical properties (from LEF)
    double width = 0.0;              // microns
    double height = 0.0;             // microns
    std::string site = "core";       // Placement site
    std::vector<Pin> pins;
    
    // Electrical properties (from Liberty)
    double area = 0.0;               // Logical area
    double leakage_power = 0.0;      // Power consumption
    
    // Flip-flop banking properties (from Liberty)
    std::string single_bit_degenerate = "null";  // Parent cell for banking (multi-bit FF指向single-bit FF)
    std::vector<std::string> banking_targets;     // Banking targets (single-bit FF可以banking到的multi-bit FF們)
    int bit_width = 1;               // Number of bits (1,2,4,8...)
    
    // Clock edge information (from Liberty)
    enum ClockEdge {
        RISING,                      // clocked_on : "CK" 
        FALLING,                     // clocked_on : "(!CK)"
        UNKNOWN_EDGE                 // Not parsed or not a flip-flop
    } clock_edge = UNKNOWN_EDGE;
    
    // Cell type classification
    enum CellType {
        FLIP_FLOP,                   // Can participate in banking
        LOGIC_GATE,                  // Combinational logic
        BUFFER,                      // Buffers/inverters
        FILL_CELL,                   // Fill/decap/tap cells
        OTHER
    } type = OTHER;
    
    // Derived properties
    bool is_flip_flop() const { return type == FLIP_FLOP; }
    bool can_be_banked() const { 
        return is_flip_flop() && (!banking_targets.empty() || single_bit_degenerate != "null"); 
    }
    bool is_multibit() const { return bit_width > 1; }
    
    Pin* find_pin(const std::string& pin_name) {
        for (auto& pin : pins) {
            if (pin.name == pin_name) return &pin;
        }
        return nullptr;
    }
    
    std::string get_clock_edge_string() const {
        switch (clock_edge) {
            case RISING: return "RISING";
            case FALLING: return "FALLING"; 
            case UNKNOWN_EDGE: return "UNKNOWN";
            default: return "ERROR";
        }
    }
    
    void print() const {
        std::cout << "Cell " << name << " (" << library << ")" << std::endl;
        std::cout << "  Size: " << width << " x " << height << std::endl;
        std::cout << "  Type: " << (type == FLIP_FLOP ? "FF" : "LOGIC") 
                  << ", Bits: " << bit_width << std::endl;
        if (type == FLIP_FLOP) {
            std::cout << "  Clock Edge: " << get_clock_edge_string() << std::endl;
        }
        std::cout << "  Area: " << area << ", Power: " << leakage_power << std::endl;
        std::cout << "  Banking: " << single_bit_degenerate << std::endl;
        std::cout << "  Pins: " << pins.size() << std::endl;
        for (const auto& pin : pins) {
            pin.print();
        }
    }
};

// =============================================================================
// 4. BANKING TYPES FOR STRATEGIC BANKING
// =============================================================================

enum class BankingType {
    FSDN,           // FALLING edge, can bank to FSDN2/FSDN4
    RISING_LSRDPQ,  // RISING edge (FDP/LSRDPQ), can bank to LSRDPQ4
    NONE            // Cannot be banked
};

// =============================================================================
// 5. INSTANCE (Verilog + DEF combined)
// =============================================================================

struct Instance {
    std::string name;                // Instance name
    std::string cell_type;           // Reference to CellTemplate
    std::shared_ptr<CellTemplate> cell_template = nullptr;  // Resolved link
    
    // Module information (for hierarchical design)
    std::string module_name = "";                  // Which module this instance belongs to
    
    // Banking properties
    BankingType banking_type = BankingType::NONE;  // Banking eligibility
    std::string cluster_id = "";                   // Cluster ID for transformation tracking
    
    // Post-banking substitution properties
    std::string best_ff_from_substitution = "";    // Best FF discovered during 3-stage substitution
    double best_ff_score = std::numeric_limits<double>::max();  // Corresponding score
    
    // Physical placement (from DEF)
    Point position;                  // Placement location
    enum Orientation { 
        N, S, E, W, FN, FS, FE, FW 
    } orientation = N;
    
    enum PlacementStatus {
        UNPLACED, PLACED, FIXED
    } placement_status = UNPLACED;
    
    // Pin connections (from Verilog)
    struct Connection {
        std::string pin_name;        // .CK, .D, .Q
        std::string net_name;        // Connected signal
        
        Connection() = default;
        Connection(const std::string& pin, const std::string& net) 
            : pin_name(pin), net_name(net) {}
    };
    std::vector<Connection> connections;
    
    /*Legalization*/
    double x_new = 0.0, y_new = 0.0;   // 新位置
    int weight = 1;                     // 權重  
    int row_id = -1;                    // 所屬 row ID
    /*Legalization*/
    
    // Pin connection status analysis (for compatibility checking)
    struct PinConnectionStatus {
        std::string pin_name;
        enum Status {
            CONNECTED,               // Connected to a real signal
            UNCONNECTED,            // Not connected (SYNOPSYS_UNCONNECTED)
            TIED_TO_GROUND,         // Connected to VSS/GND
            TIED_TO_POWER,          // Connected to VDD/VCC
            MISSING                 // Pin doesn't exist in cell template
        } status = MISSING;
        std::string net_name;       // What it's connected to
        
        PinConnectionStatus() = default;
        PinConnectionStatus(const std::string& pin, Status stat, const std::string& net = "")
            : pin_name(pin), status(stat), net_name(net) {}
    };
    std::vector<PinConnectionStatus> pin_status;  // Analyzed pin connection status
    
    // Derived properties
    bool is_flip_flop() const { 
        return cell_template && cell_template->is_flip_flop(); 
    }
    bool can_be_banked() const { 
        return cell_template && cell_template->can_be_banked(); 
    }
    int get_bit_width() const { 
        return cell_template ? cell_template->bit_width : 1; 
    }
    
    /*Legalization*/
    Point get_original_position() const { return position; }
    double get_width() const { 
        return cell_template ? cell_template->width : 0.0; 
    }
    double get_height() const { 
        return cell_template ? cell_template->height : 0.0; 
    }
    /*Legalization*/
    
    Connection* find_connection(const std::string& pin_name) {
        for (auto& conn : connections) {
            if (conn.pin_name == pin_name) return &conn;
        }
        return nullptr;
    }
    
    void print() const {
        std::cout << "Instance " << name << " (" << cell_type << ")" << std::endl;
        std::cout << "  Position: (" << position.x << ", " << position.y << ")" << std::endl;
        std::cout << "  Status: " << (placement_status == PLACED ? "PLACED" : "UNPLACED") << std::endl;
        std::cout << "  Connections: " << connections.size() << std::endl;
        for (const auto& conn : connections) {
            std::cout << "    ." << conn.pin_name << "(" << conn.net_name << ")" << std::endl;
        }
    }
};

// =============================================================================
// 5. NET (Verilog + DEF combined)
// =============================================================================

struct Net {
    std::string name;                // Net name
    
    // All instance.pin connections on this net
    struct NetConnection {
        std::string instance_name;   // Instance name (or "PIN" for top-level)
        std::string pin_name;        // Pin name
        
        NetConnection() = default;
        NetConnection(const std::string& inst, const std::string& pin)
            : instance_name(inst), pin_name(pin) {}
    };
    std::vector<NetConnection> connections;
    
    // Net properties
    enum NetType { 
        SIGNAL, CLOCK, POWER, GROUND 
    } type = SIGNAL;
    
    bool is_clock_net = false;       // Analyzed clock signal
    
    size_t fanout() const { return connections.size(); }
    
    void print() const {
        std::cout << "Net " << name << " (" 
                  << (type == CLOCK ? "CLK" : "SIG") << ")" << std::endl;
        std::cout << "  Fanout: " << fanout() << std::endl;
        for (const auto& conn : connections) {
            std::cout << "    " << conn.instance_name << "." << conn.pin_name << std::endl;
        }
    }
};

// =============================================================================
// 6. SCAN CHAIN INFO (from DEF SCANDEF)
// =============================================================================

struct ScanChain {
    std::string name;                    // Scan chain name
    std::string scan_in_pin;            // External scan input pin
    std::string scan_out_pin;           // External scan output pin
    
    // Scan chain sequence: SI -> FF1 -> FF2 -> ... -> SO
    struct ScanConnection {
        std::string instance_name;       // Instance name
        std::string scan_in_pin;        // SI pin name
        std::string scan_out_pin;       // SO pin name
        
        ScanConnection() = default;
        ScanConnection(const std::string& inst, const std::string& si, const std::string& so)
            : instance_name(inst), scan_in_pin(si), scan_out_pin(so) {}
    };
    std::vector<ScanConnection> chain_sequence;
    
    size_t length() const { return chain_sequence.size(); }
    
    void print() const {
        std::cout << "Scan Chain " << name << " (length: " << length() << ")" << std::endl;
        std::cout << "  Input: " << scan_in_pin << " -> Output: " << scan_out_pin << std::endl;
        for (size_t i = 0; i < chain_sequence.size(); i++) {
            const auto& conn = chain_sequence[i];
            std::cout << "  [" << i << "] " << conn.instance_name 
                      << " (" << conn.scan_in_pin << " -> " << conn.scan_out_pin << ")" << std::endl;
        }
    }
};

/*Legalization*/
struct Cluster {
    double x = 0.0;                     // 左端座標
    double width = 0.0;                 // 寬度
    double weight = 0.0;                // 權重
    double q = 0.0;                     // 加權位置和
    Cluster* leftCluster = nullptr;     // 前一個 cluster
    std::vector<Instance*> cellInCluster; // 改為 Instance*
};
// 修改 SubRow 結構，使其與原始邏輯一致
struct SubRow {
    double x_min = 0.0, x_max = 0.0;
    double Usewidth = 0.0;              // 改名為 Usewidth 以匹配原始代碼
    Cluster* lastCluster = nullptr;
    
    SubRow() = default;
    SubRow(double xmin, double xmax)
        : x_min(xmin), x_max(xmax), Usewidth(xmax - xmin), lastCluster(nullptr) {}
};
/*Legalization*/

// =============================================================================
// 7. DESIGN LAYOUT INFO (from DEF)
// =============================================================================

struct DesignPin {
    std::string name;                // Top-level pin name
    std::string net_name;            // Connected internal net
    
    enum Direction { 
        INPUT, OUTPUT, INOUT 
    } direction = INPUT;
    
    Point position;                  // Physical location
    std::string layer;               // Metal layer
    
    void print() const {
        std::cout << "Design Pin " << name << " -> " << net_name 
                  << " (" << (direction == INPUT ? "IN" : "OUT") << ")" << std::endl;
    }
};

struct PlacementRow {
    std::string name;                // Row name
    std::string site;                // Site type
    Point origin;                    // Starting point
    int num_x = 0, num_y = 0;        // Site count
    double step_x = 0.0, step_y = 0.0;  // Site spacing

    /*Legalization*/
    double height = 0.0;        // 從 step_y 獲取
    double site_width = 0.0;    // 從 step_x 獲取
    int id = -1;                // Row ID
    std::vector<SubRow> subrows; // 原本就有，但確保使用正確的 SubRow
    /*Legalization*/
    
    void print() const {
        std::cout << "Row " << name << " @ (" << origin.x << ", " << origin.y 
                  << ") [" << num_x << "x" << num_y << "]" << std::endl;
    }
};

struct Track {
    enum Direction { X, Y } direction = X;
    double start = 0.0;              // Starting coordinate
    int num = 0;                     // Number of tracks
    double step = 0.0;               // Track spacing
    std::string layer;               // Metal layer
    
    void print() const {
        std::cout << "Track " << (direction == X ? "X" : "Y") 
                  << " @ " << start << " [" << num << " tracks, step " << step 
                  << "] Layer " << layer << std::endl;
    }
};

// =============================================================================
// 7. OBJECTIVE FUNCTION (from Weight file)
// =============================================================================

struct ObjectiveWeights {
    double alpha = 0.0;              // TNS weight
    double beta = 0.0;               // Power weight
    double gamma = 0.0;              // Area weight
    
    // Initial design metrics
    double initial_tns = 0.0;
    double initial_power = 0.0;
    double initial_area = 0.0;
    
    double calculate_objective(double tns, double power, double area) const {
        return alpha * tns + beta * power + gamma * area;
    }
    
    void print() const {
        std::cout << "Objective: " << alpha << "*TNS + " << beta 
                  << "*Power + " << gamma << "*Area" << std::endl;
        std::cout << "Initial: TNS=" << initial_tns << ", Power=" 
                  << initial_power << ", Area=" << initial_area << std::endl;
    }
};

// =============================================================================
// 8. TRANSFORMATION RECORD SYSTEM (for ICCAD 2025 Contest Output)
// =============================================================================

struct TransformationRecord {
    enum Operation {
        KEEP,           // Instance kept unchanged
        DEBANK,         // Multi-bit FF split into multiple single-bit FFs
        BANK,           // Multiple single-bit FFs combined into multi-bit FF
        SUBSTITUTE,     // FF substituted with different cell type (same functionality)
        POST_SUBSTITUTE // Post-banking FF substitution (separate from main substitution)
    };
    
    std::string original_instance_name;                    // Input instance name
    std::string result_instance_name;                      // Output instance name (may be same as original)
    std::map<std::string, std::string> pin_mapping;        // original_pin -> result_pin mapping
    Operation operation;                                   // Type of transformation applied
    std::string original_cell_type;                        // Original cell type
    std::string result_cell_type;                          // Result cell type
    std::string stage = "";                                 // Stage identifier for operation ordering
    
    // For multi-instance transformations (banking/debanking)
    std::vector<std::string> related_instances;            // Other instances involved in the transformation
    
    // Position information (for final placement)
    double result_x = 0.0, result_y = 0.0;                // Final position
    std::string result_orientation = "N";                  // Final orientation
    
    // Enhanced tracking fields
    std::string cluster_id;                                // 用來群組相關transformations
    
    // Constructor
    TransformationRecord(const std::string& orig_name, const std::string& result_name, 
                        Operation op, const std::string& orig_cell, const std::string& result_cell)
        : original_instance_name(orig_name), result_instance_name(result_name), 
          operation(op), original_cell_type(orig_cell), result_cell_type(result_cell) {}
    
    // Helper functions
    std::string operation_string() const {
        switch (operation) {
            case KEEP: return "KEEP";
            case DEBANK: return "DEBANK";
            case BANK: return "BANK";
            case SUBSTITUTE: return "SUBSTITUTE";
            case POST_SUBSTITUTE: return "POST_SUBSTITUTE";
            default: return "UNKNOWN";
        }
    }
    
    void print() const {
        std::cout << "Transform [" << operation_string() << "]: " 
                  << original_instance_name << " (" << original_cell_type << ") -> " 
                  << result_instance_name << " (" << result_cell_type << ")" << std::endl;
        if (!pin_mapping.empty()) {
            std::cout << "  Pin mapping: ";
            bool first = true;
            for (const auto& pair : pin_mapping) {
                if (!first) std::cout << ", ";
                std::cout << pair.first << "->" << pair.second;
                first = false;
            }
            std::cout << std::endl;
        }
        if (!related_instances.empty()) {
            std::cout << "  Related instances: ";
            for (size_t i = 0; i < related_instances.size(); i++) {
                if (i > 0) std::cout << ", ";
                std::cout << related_instances[i];
            }
            std::cout << std::endl;
        }
    }
};

// =============================================================================
// 8.5. STAGE-BASED PIPELINE SYSTEM (for Complete Transformation Tracking)
// =============================================================================

struct InstanceSnapshot {
    std::string instance_name;                       // Current instance name
    std::string cell_type;                           // Current cell type
    double x = 0.0, y = 0.0;                        // Current position
    std::string orientation = "N";                   // Current orientation
    std::map<std::string, std::string> pin_connections; // pin_name -> net_name
    
    // Tracking information
    std::string cluster_id;                          // Grouping information
    std::string original_name;                       // Original instance name (before any transformation)
    TransformationRecord::Operation last_operation = TransformationRecord::KEEP;
    
    InstanceSnapshot() = default;
    InstanceSnapshot(const std::string& name, const std::string& cell, 
                    double pos_x, double pos_y, const std::string& orient = "N")
        : instance_name(name), cell_type(cell), x(pos_x), y(pos_y), 
          orientation(orient), original_name(name) {}
    
    // Create snapshot from Instance
    static InstanceSnapshot from_instance(const std::shared_ptr<Instance>& inst) {
        InstanceSnapshot snapshot;
        snapshot.instance_name = inst->name;
        // Use cell_template->name instead of cell_type to capture current (possibly substituted) cell type
        snapshot.cell_type = inst->cell_template ? inst->cell_template->name : inst->cell_type;
        snapshot.x = inst->position.x;
        snapshot.y = inst->position.y;
        snapshot.orientation = "N";  // Convert enum to string if needed
        snapshot.original_name = inst->name;  // Will be updated if this is a transformation result
        
        // Extract pin connections
        for (const auto& conn : inst->connections) {
            snapshot.pin_connections[conn.pin_name] = conn.net_name;
        }
        
        return snapshot;
    }
    
    void print() const {
        std::cout << "  Instance " << instance_name << " (" << cell_type << ")"
                  << " @ (" << x << ", " << y << ") " << orientation << std::endl;
        std::cout << "    Original: " << original_name << ", Cluster: " << cluster_id << std::endl;
        if (!pin_connections.empty()) {
            std::cout << "    Pins: ";
            bool first = true;
            for (const auto& conn : pin_connections) {
                if (!first) std::cout << ", ";
                std::cout << conn.first << "->" << conn.second;
                first = false;
            }
            std::cout << std::endl;
        }
    }
};

struct StagePipeline {
    std::string stage_name;                          // "ORIGINAL", "DEBANK", "SUBSTITUTE", "BANK", "LEGALIZE"
    std::vector<InstanceSnapshot> instances;         // 這個階段的所有FF instances
    std::vector<size_t> transformation_indices;     // 對應的transformation records indices
    
    // Stage metadata
    size_t total_instances = 0;
    size_t ff_instances = 0;
    double total_area = 0.0;
    double total_power = 0.0;
    
    StagePipeline() = default;
    StagePipeline(const std::string& name) : stage_name(name) {}
    
    void add_instance(const InstanceSnapshot& snapshot) {
        instances.push_back(snapshot);
        total_instances++;
        // Note: area/power calculation would need cell template access
    }
    
    void add_transformation_index(size_t index) {
        transformation_indices.push_back(index);
    }
    
    void print() const {
        std::cout << "\n=== Stage: " << stage_name << " ===" << std::endl;
        std::cout << "Total instances: " << total_instances << std::endl;
        std::cout << "FF instances: " << ff_instances << std::endl;
        std::cout << "Associated transformations: " << transformation_indices.size() << std::endl;
        
        if (!instances.empty()) {
            std::cout << "Instance list:" << std::endl;
            for (const auto& instance : instances) {
                instance.print();
            }
        }
        
        if (!transformation_indices.empty()) {
            std::cout << "Transformation indices: ";
            for (size_t i = 0; i < transformation_indices.size(); i++) {
                if (i > 0) std::cout << ", ";
                std::cout << transformation_indices[i];
            }
            std::cout << std::endl;
        }
    }
};

struct CompletePipeline {
    std::vector<StagePipeline> stages;               // 所有階段的pipeline
    std::map<std::string, size_t> stage_index_map;  // stage_name -> index mapping
    
    CompletePipeline() {
        // Initialize with standard stages
        add_stage("ORIGINAL");
        add_stage("DEBANK");
        add_stage("SUBSTITUTION");  // Three-stage substitution system
        add_stage("BANK");
        add_stage("POST_BANKING");  // Post-banking SBFF substitution
        add_stage("LEGALIZE");
    }
    
    void add_stage(const std::string& stage_name) {
        size_t index = stages.size();
        stages.emplace_back(stage_name);
        stage_index_map[stage_name] = index;
    }
    
    StagePipeline* get_stage(const std::string& stage_name) {
        auto it = stage_index_map.find(stage_name);
        if (it != stage_index_map.end()) {
            return &stages[it->second];
        }
        return nullptr;
    }
    
    const StagePipeline* get_stage(const std::string& stage_name) const {
        auto it = stage_index_map.find(stage_name);
        if (it != stage_index_map.end()) {
            return &stages[it->second];
        }
        return nullptr;
    }
    
    void capture_stage(const std::string& stage_name, 
                      const std::vector<std::shared_ptr<Instance>>& all_instances,
                      const std::vector<size_t>& new_transformation_indices = {},
                      const std::vector<TransformationRecord>* transformation_history = nullptr) {
        StagePipeline* stage = get_stage(stage_name);
        if (!stage) {
            std::cout << "Warning: Unknown stage " << stage_name << std::endl;
            return;
        }
        
        // Clear existing data
        stage->instances.clear();
        stage->transformation_indices.clear();
        stage->total_instances = 0;
        stage->ff_instances = 0;
        
        // Create a map from instance name to the LATEST transformation record for quick lookup
        std::map<std::string, const TransformationRecord*> instance_to_record;
        if (transformation_history) {
            // Iterate through records to find the latest one for each instance
            for (size_t i = 0; i < transformation_history->size(); ++i) {
                const auto& record = (*transformation_history)[i];
                // Map both original and result instance names to the record
                // Later records will overwrite earlier ones, giving us the latest transformation
                instance_to_record[record.original_instance_name] = &record;
                instance_to_record[record.result_instance_name] = &record;
            }
        }
        
        // Capture all FF instances
        for (const auto& inst : all_instances) {
            if (inst->is_flip_flop()) {
                InstanceSnapshot snapshot = InstanceSnapshot::from_instance(inst);
                
                // Try to find transformation record for this instance
                auto record_it = instance_to_record.find(inst->name);
                if (record_it != instance_to_record.end()) {
                    const TransformationRecord* record = record_it->second;
                    
                    // Set cluster_id and original_name from transformation record
                    snapshot.cluster_id = record->cluster_id;
                    snapshot.last_operation = record->operation;
                    
                    // For DEBANK operations, the original_name should be the original multi-bit FF
                    if (record->operation == TransformationRecord::DEBANK) {
                        snapshot.original_name = record->original_instance_name;
                    } else {
                        // For other operations, try to trace back to the original
                        snapshot.original_name = record->original_instance_name;
                    }
                }
                
                stage->add_instance(snapshot);
                stage->ff_instances++;
            }
        }
        
        // Add transformation indices
        for (size_t index : new_transformation_indices) {
            stage->add_transformation_index(index);
        }
        
        std::cout << "Captured stage " << stage_name << " with " 
                  << stage->ff_instances << " FF instances" << std::endl;
    }
    
    void print() const {
        std::cout << "\n=== COMPLETE PIPELINE REPORT ===" << std::endl;
        std::cout << "Total stages: " << stages.size() << std::endl;
        
        for (const auto& stage : stages) {
            stage.print();
        }
    }
    
    // Generate stage comparison report
    void print_stage_comparison() const {
        std::cout << "\n=== STAGE COMPARISON ===" << std::endl;
        std::cout << "Stage            | Instances | FF Count | Transformations" << std::endl;
        std::cout << "-----------------|-----------|----------|----------------" << std::endl;
        
        for (const auto& stage : stages) {
            std::cout << std::setw(16) << std::left << stage.stage_name << " | "
                      << std::setw(9) << stage.total_instances << " | "
                      << std::setw(8) << stage.ff_instances << " | "
                      << stage.transformation_indices.size() << std::endl;
        }
    }
};

// =============================================================================
// 8.5. DEBANK CLUSTER STRUCTURES FOR STRATEGIC BANKING
// =============================================================================


// =============================================================================
// 9. MAIN DESIGN DATABASE
// =============================================================================

class DesignDatabase {
public:
    // Design metadata
    std::string design_name;
    std::string testcase_path;
    std::string input_verilog_path;  // Store input verilog file path
    
    // Module hierarchy information
    struct Module {
        std::string name;
        size_t start_pos = 0;        // Starting position in verilog file
        size_t end_pos = 0;          // Ending position in verilog file
        std::vector<std::string> instance_names;  // Instances belonging to this module
    };
    std::vector<Module> modules;
    
    // Core data collections
    std::unordered_map<std::string, std::shared_ptr<CellTemplate>> cell_library;
    std::unordered_map<std::string, std::shared_ptr<Instance>> instances;
    std::unordered_map<std::string, std::shared_ptr<Net>> nets;
    
    // Layout information  
    std::vector<DesignPin> design_pins;
    std::vector<PlacementRow> placement_rows;
    std::vector<Track> tracks;
    Rectangle die_area;
    
    // Placement blockages (regions where instances cannot be placed)
    std::vector<Rectangle> placement_blockages;
    
    // Scan chain information
    std::vector<ScanChain> scan_chains;
    
    // Objective function
    ObjectiveWeights objective_weights;
    
    // FF Cell compatibility groups (for substitution optimization)
    std::unordered_map<std::string, std::vector<std::string>> ff_compatibility_groups;
    
    // Hierarchical FF grouping: clock_edge -> pin_interface -> bit_width -> cell_names
    std::map<std::string, std::map<std::string, std::map<int, std::vector<std::string>>>> hierarchical_ff_groups;
    
    // FF Instance groups for substitution: group_id -> list of instances
    std::map<std::string, std::vector<std::shared_ptr<Instance>>> ff_instance_groups;
    
    // Optimal FF mapping: group_key -> optimal_cell_name
    std::map<std::string, std::string> optimal_ff_for_groups;
    
    // Banking eligibility analysis
    std::vector<std::string> banking_eligible_groups;
    
    // Banking candidate instance groups (Phase 1: Instance Group Filtering)
    std::vector<std::string> banking_candidate_instance_groups;
    
    // Transformation tracking system (for ICCAD 2025 Contest Output)
    std::vector<TransformationRecord> transformation_history;
    
    // Stage-based pipeline system (for Complete Transformation Tracking)
    CompletePipeline complete_pipeline;
    
    // ICCAD 2025 Contest operation log support
    std::map<std::string, std::string> dummy_to_real_mapping;  // dummy_1 -> actual_instance_name
    std::map<std::string, std::string> real_to_dummy_mapping;  // actual_instance_name -> dummy_1  
    mutable int global_dummy_counter = 1;
    
    // Statistics
    struct Stats {
        int total_instances = 0;
        int flip_flop_count = 0;
        int bankable_ff_count = 0;
        int total_nets = 0;
        double total_area = 0.0;
        double total_power = 0.0;
    } stats;
    
    // =============================================================================
    // QUERY FUNCTIONS
    // =============================================================================
    
    std::shared_ptr<CellTemplate> get_cell(const std::string& name) const {
        auto it = cell_library.find(name);
        return (it != cell_library.end()) ? it->second : nullptr;
    }
    
    std::vector<std::shared_ptr<Instance>> get_flip_flops() const {
        std::vector<std::shared_ptr<Instance>> ffs;
        for (const auto& pair : instances) {
            if (pair.second->is_flip_flop()) {
                ffs.push_back(pair.second);
            }
        }
        return ffs;
    }
    
    std::vector<std::shared_ptr<Instance>> get_bankable_flip_flops() const {
        std::vector<std::shared_ptr<Instance>> ffs;
        for (const auto& pair : instances) {
            if (pair.second->can_be_banked()) {
                ffs.push_back(pair.second);
            }
        }
        return ffs;
    }
    
    std::vector<std::shared_ptr<Net>> get_clock_nets() const {
        std::vector<std::shared_ptr<Net>> clocks;
        for (const auto& pair : nets) {
            if (pair.second->is_clock_net) {
                clocks.push_back(pair.second);
            }
        }
        return clocks;
    }
    
    // Get all compatible cell types for a given clock edge, pin interface, and bit width
    std::vector<std::string> get_compatible_ff_cells(const std::string& clock_edge, 
                                                    const std::string& pin_interface, 
                                                    int bit_width) const {
        auto edge_it = hierarchical_ff_groups.find(clock_edge);
        if (edge_it == hierarchical_ff_groups.end()) return {};
        
        auto pin_it = edge_it->second.find(pin_interface);
        if (pin_it == edge_it->second.end()) return {};
        
        auto bit_it = pin_it->second.find(bit_width);
        if (bit_it == pin_it->second.end()) return {};
        
        return bit_it->second;
    }
    
    // Get all available bit widths for a given clock edge and pin interface
    std::vector<int> get_available_bit_widths(const std::string& clock_edge, 
                                            const std::string& pin_interface) const {
        std::vector<int> bit_widths;
        
        auto edge_it = hierarchical_ff_groups.find(clock_edge);
        if (edge_it == hierarchical_ff_groups.end()) return bit_widths;
        
        auto pin_it = edge_it->second.find(pin_interface);
        if (pin_it == edge_it->second.end()) return bit_widths;
        
        for (const auto& bit_pair : pin_it->second) {
            bit_widths.push_back(bit_pair.first);
        }
        
        std::sort(bit_widths.begin(), bit_widths.end());
        return bit_widths;
    }
    
    // Get all FF instances for a specific group key
    std::vector<std::shared_ptr<Instance>> get_ff_instance_group(const std::string& group_key) const {
        auto group_it = ff_instance_groups.find(group_key);
        if (group_it != ff_instance_groups.end()) {
            return group_it->second;
        }
        return {};
    }
    
    // Get all group keys
    std::vector<std::string> get_all_ff_instance_group_keys() const {
        std::vector<std::string> keys;
        for (const auto& pair : ff_instance_groups) {
            keys.push_back(pair.first);
        }
        return keys;
    }
    
    // Get statistics about instance groups
    struct InstanceGroupStats {
        int total_groups = 0;
        int total_instances = 0;
        int largest_group_size = 0;
        std::string largest_group_key;
    };
    
    InstanceGroupStats get_instance_group_stats() const {
        InstanceGroupStats stats;
        stats.total_groups = ff_instance_groups.size();
        
        for (const auto& pair : ff_instance_groups) {
            int group_size = pair.second.size();
            stats.total_instances += group_size;
            
            if (group_size > stats.largest_group_size) {
                stats.largest_group_size = group_size;
                stats.largest_group_key = pair.first;
            }
        }
        
        return stats;
    }
    
    void update_statistics() {
        stats = Stats();  // Reset
        
        stats.total_instances = instances.size();
        stats.total_nets = nets.size();
        
        for (const auto& pair : instances) {
            const auto& inst = pair.second;
            if (inst->is_flip_flop()) {
                stats.flip_flop_count++;
                if (inst->can_be_banked()) {
                    stats.bankable_ff_count++;
                }
            }
            
            if (inst->cell_template) {
                stats.total_area += inst->cell_template->area;
                stats.total_power += inst->cell_template->leakage_power;
            }
        }
    }
    
    void print_statistics() const {
        std::cout << "\n=== Design Statistics ===" << std::endl;
        std::cout << "Design: " << design_name << std::endl;
        std::cout << "Cells: " << cell_library.size() << std::endl;
        std::cout << "Instances: " << stats.total_instances << std::endl;
        std::cout << "Nets: " << stats.total_nets << std::endl;
        std::cout << "Flip-flops: " << stats.flip_flop_count 
                  << " (bankable: " << stats.bankable_ff_count << ")" << std::endl;
        std::cout << "Total Area: " << stats.total_area << std::endl;
        std::cout << "Total Power: " << stats.total_power << std::endl;
        std::cout << "Die: " << die_area.width() << " x " << die_area.height() << std::endl;
        std::cout << "Scan Chains: " << scan_chains.size() << std::endl;
    }
};