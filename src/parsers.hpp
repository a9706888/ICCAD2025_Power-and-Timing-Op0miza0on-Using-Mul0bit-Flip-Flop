#pragma once
#include "data_structures.hpp"
#include <set>

// =============================================================================
// PARSER FUNCTION DECLARATIONS
// =============================================================================
// 所有parser都是簡單的函數，直接操作DesignDatabase
// 沒有類，沒有複雜的繼承，就是純粹的功能函數
// =============================================================================

// Liberty parser: 解析.lib檔案，直接添加CellTemplate到db.cell_library
void parse_liberty_file(const std::string& filepath, DesignDatabase& db);

// LEF parser: 解析.lef檔案，為現有CellTemplate添加物理資訊
void parse_lef_file(const std::string& filepath, DesignDatabase& db);

// Verilog parser: 解析.v檔案，創建Instance和Net
void parse_verilog_file(const std::string& filepath, DesignDatabase& db);

// Selective Verilog parser: 只解析DEF中識別的FF instances (performance optimized)
void parse_verilog_file_selective(const std::string& filepath, DesignDatabase& db);
void parse_targeted_ff_connections(const std::string& content, const std::set<std::string>& targets, DesignDatabase& db);

// DEF parser: 解析.def檔案，為現有Instance添加位置資訊
void parse_def_file(const std::string& filepath, DesignDatabase& db);

// Weight parser: 解析weight.txt，設定db.objective_weights
void parse_weight_file(const std::string& filepath, DesignDatabase& db);

// =============================================================================
// VALIDATION AND OUTPUT FUNCTIONS
// =============================================================================

// 建立banking關係（multi-bit FF -> single-bit FF 的反向關係）
void build_banking_relationships(DesignDatabase& db);

// 輸出cell library驗證檔案
void export_cell_library_validation(const DesignDatabase& db, const std::string& output_file = "cell_library_validation.txt");

// 輸出instance驗證檔案
void export_instance_validation(const DesignDatabase& db, const std::string& output_file = "instance_validation.txt");

// =============================================================================
// FILE DISCOVERY AND FILTERING FUNCTIONS
// =============================================================================

// 自動掃描並過濾需要的檔案，跳過不需要的檔案類型
std::vector<std::string> discover_liberty_files(const std::string& testcase_path);
std::vector<std::string> discover_lef_files(const std::string& testcase_path);

// 檔案過濾函數
bool should_parse_liberty_file(const std::string& filepath);
bool should_parse_lef_file(const std::string& filepath);

// Pin類型識別函數
Pin::FlipFlopPinType classify_ff_pin_type(const std::string& pin_name);

// FF instance連線狀態分析函數
void analyze_ff_pin_connections(DesignDatabase& db);
bool is_active_logical_connection(const Instance::Connection& conn, Pin::FlipFlopPinType pin_type);

// Helper function to convert Instance::Orientation enum to string
std::string orientation_to_string(Instance::Orientation orientation);

// Scan chain detection函數
void detect_scan_chains(DesignDatabase& db);
void build_scan_chain_groups(DesignDatabase& db);

// Strategic debanking  
void perform_strategic_debanking(DesignDatabase& db);
void map_multibit_to_singlebit_connections(std::shared_ptr<Instance> multibit_instance,
                                         std::shared_ptr<Instance> singlebit_instance,
                                         int bit_index,
                                         DesignDatabase& db);
std::string map_singlebit_pin_to_multibit(const std::string& singlebit_pin, int bit_index);
std::string find_shared_pin_connection(std::shared_ptr<Instance> multibit_instance, 
                                     const std::string& singlebit_pin);
void export_strategic_debanking_report(const DesignDatabase& db);


// FF grouping結果輸出函數
void export_ff_grouping_report(DesignDatabase& db, const std::string& output_file = "ff_grouping_report.txt");

// FF instance grouping functions
void group_ff_instances(DesignDatabase& db);
void rebuild_ff_instance_groups_for_banking(DesignDatabase& db);
void export_ff_instance_groups_detailed_report(const DesignDatabase& db, const std::string& output_file);

// FF substitution functions
// Three-Stage Substitution System (temporarily disabled)
// void execute_stage1_original_pin_substitution(DesignDatabase& db);
// void execute_stage2_effective_pin_substitution(DesignDatabase& db);

// From substitution.cpp - Three-stage FF substitution system
void execute_three_stage_substitution(DesignDatabase& db);
void execute_post_banking_substitution(DesignDatabase& db);
bool is_single_bit_ff(std::shared_ptr<Instance> instance);
std::string convert_instance_key_to_hierarchical_key(const std::string& instance_key);

// FF scoring and banking utility functions
double calculate_ff_score(const std::string& cell_name, const DesignDatabase& db);
bool can_be_banked_from_single_bits(const std::string& multibit_ff_name, const DesignDatabase& db);
std::string find_banking_compatible_single_bit(const std::string& multibit_ff_name, const DesignDatabase& db);
void export_ff_instance_grouping_report(const DesignDatabase& db, const std::string& output_file = "ff_instance_grouping_report.txt");

// FF scoring and optimal selection functions
void calculate_optimal_ff_for_instance_groups(DesignDatabase& db);
std::string get_instance_clock_edge(std::shared_ptr<Instance> instance, const DesignDatabase& db);
std::string get_instance_clock_domain(std::shared_ptr<Instance> instance, const DesignDatabase& db);


// Strategic banking functions
void analyze_banking_eligibility(DesignDatabase& db);
void export_banking_eligibility_report(const DesignDatabase& db, const std::string& output_file = "banking_eligibility_report.txt");

// Instance group filtering functions
void filter_banking_eligible_instance_groups(DesignDatabase& db);
void export_banking_candidate_instance_groups_report(const DesignDatabase& db, const std::string& output_file = "banking_candidate_instance_groups_report.txt");

// Banking合法性檢查函數
bool can_bank_flip_flops(const std::vector<std::shared_ptr<Instance>>& ff_instances, 
                        const DesignDatabase& db);
bool is_same_clock_domain(const std::vector<std::shared_ptr<Instance>>& ff_instances, 
                         const DesignDatabase& db);
bool check_scan_chain_compatibility(const std::vector<std::shared_ptr<Instance>>& ff_instances, 
                                   const DesignDatabase& db);
double calculate_manhattan_distance(const Instance& ff1, const Instance& ff2);

// FF兼容性檢查函數
bool check_ff_compatibility(const std::shared_ptr<Instance>& ff1, 
                            const std::shared_ptr<Instance>& ff2,
                            const DesignDatabase& db);
bool check_clock_edge_compatibility(const std::shared_ptr<Instance>& ff1,
                                   const std::shared_ptr<Instance>& ff2,
                                   const DesignDatabase& db);
bool check_pin_interface_compatibility(const std::shared_ptr<Instance>& ff1,
                                      const std::shared_ptr<Instance>& ff2,
                                      const DesignDatabase& db);
bool check_connection_status_compatibility(const std::shared_ptr<Instance>& ff1,
                                          const std::shared_ptr<Instance>& ff2,
                                          const DesignDatabase& db);
std::vector<std::vector<std::shared_ptr<Instance>>> 
group_compatible_flip_flops(const DesignDatabase& db);

// FF Cell template compatibility grouping
void build_ff_cell_compatibility_groups(DesignDatabase& db);

// =============================================================================
// TRANSFORMATION TRACKING FUNCTIONS
// =============================================================================

// Record transformation operations (with enhanced cluster tracking)
void record_keep_transformation(DesignDatabase& db, std::shared_ptr<Instance> original_instance);
void record_substitute_transformation(DesignDatabase& db, const std::string& instance_name, const std::string& original_cell_type, const std::string& final_cell_type);
void record_substitution_transformation_complete(DesignDatabase& db, const std::string& instance_name, const std::string& original_cell_type, const std::string& final_cell_type);
void record_debank_transformation(DesignDatabase& db, std::shared_ptr<Instance> original_multibit_instance, const std::vector<std::shared_ptr<Instance>>& resulting_singlebit_instances, const std::string& parent_cell_type);
void record_all_debank_pin_mappings_from_record(const TransformationRecord& debank_record);

// Forward declaration for SimpleTransformationChain
struct SimpleTransformationChain;

// Simple Pin Mapping functions and data
extern std::map<std::string, std::string> global_debank_pin_mappings;
std::map<std::string, SimpleTransformationChain> build_simple_transformation_chains(const DesignDatabase& db);
std::vector<std::string> generate_pin_mapping_for_chain(const SimpleTransformationChain& chain, const DesignDatabase& db);
void record_bank_transformation(DesignDatabase& db, const std::vector<std::shared_ptr<Instance>>& original_singlebit_ffs, const std::string& resulting_multibit_name, const std::string& multibit_cell_type, const std::map<std::string, std::string>& pin_mapping);
void record_legalization_transformations(DesignDatabase& db);
void remove_keep_transformation_record(DesignDatabase& db, const std::string& instance_name);

// Banking functions
void execute_banking_preparation(DesignDatabase& db);
void assign_banking_types(DesignDatabase& db);
void export_banking_preparation_report(DesignDatabase& db, const std::string& output_file);
void execute_debank_cluster_rebanking(DesignDatabase& db);
void execute_fsdn_two_phase_banking(DesignDatabase& db);
void execute_lsrdpq_single_phase_banking(DesignDatabase& db);
void export_banking_operations_record(const std::string& output_file);
void export_banking_step_report(const DesignDatabase& db, const std::string& step_name, const std::string& output_file);
void record_all_banking_transformations(DesignDatabase& db);
void map_singlebit_to_multibit_connections(const std::vector<std::shared_ptr<Instance>>& singlebit_instances,
                                          std::shared_ptr<Instance> multibit_instance,
                                          int target_bit_width,
                                          DesignDatabase& db);

// Contest output generation
void generate_pin_mapping_list_file(const DesignDatabase& db, const std::string& output_file);
void generate_final_def_file(const DesignDatabase& db, const std::string& output_file);
void generate_final_verilog_file(const DesignDatabase& db, const std::string& output_file);

// Simple pin mapping system (no DEBANK version)
void generate_simple_pin_mapping_file(const DesignDatabase& db, const std::string& output_file);
void export_simple_transformation_chains_report(const DesignDatabase& db, const std::string& output_file);


// Transformation system management
void initialize_transformation_tracking(DesignDatabase& db);
void export_transformation_report(const DesignDatabase& db, const std::string& output_file);

void generate_contest_output_files(const DesignDatabase& db, const std::string& base_name);

// Operation log generation functions (ICCAD 2025 Contest format)
std::vector<std::string> generate_split_multibit_operations(DesignDatabase& db);
std::vector<std::string> generate_size_cell_operations(const DesignDatabase& db);
std::vector<std::string> generate_create_multibit_operations(const DesignDatabase& db);
void generate_operation_log_file(DesignDatabase& db, const std::string& output_file);

// Transformation verification functions
bool run_transformation_verification(const DesignDatabase& db, const std::string& output_base_name = "testcase_solution");
void export_transformation_verification_report(const DesignDatabase& db, const std::string& output_file = "transformation_verification_report.txt");

// =============================================================================
// HELPER FUNCTION DECLARATIONS
// =============================================================================

// Verilog parser helpers
std::string extract_module_name(const std::string& content);
void parse_module_hierarchy(const std::string& file_content, DesignDatabase& db);
void assign_instances_to_modules(DesignDatabase& db);
void parse_module_hierarchy_and_instances(const std::string& file_content, DesignDatabase& db);

// Module distribution reporting
void export_module_instance_distribution(const DesignDatabase& db, const std::string& output_file = "module_instance_distribution.txt");
std::string clean_net_name(const std::string& raw_name);
std::pair<std::string, std::string> parse_pin_connection(const std::string& conn_str);
std::shared_ptr<Instance> parse_verilog_instance(const std::string& content, size_t start_pos, std::set<std::string>& net_names);
void build_net_connections(DesignDatabase& db);

// DEF parser helpers
void parse_diearea_line(const std::string& line, DesignDatabase& db);
void parse_track_line(const std::string& line, DesignDatabase& db);
void parse_row_line(const std::string& line, DesignDatabase& db);
bool parse_component_line(const std::string& line, DesignDatabase& db);
void parse_scandef_section(std::ifstream& file, DesignDatabase& db);
void parse_nets_section(std::ifstream& file, DesignDatabase& db);
void parse_blockages_section(std::ifstream& file, DesignDatabase& db);
void parse_specialnets_section(std::ifstream& file, DesignDatabase& db);
bool parse_rect_line(const std::string& line, Rectangle& rect);