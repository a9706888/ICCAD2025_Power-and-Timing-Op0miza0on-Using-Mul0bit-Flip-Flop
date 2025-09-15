#include "data_structures.hpp"
#include "parsers.hpp"
#include "argument_parser.hpp"
#include "substitution.hpp"
#include "def_output_generator.hpp"
/*Legalization*/
#include "Legalization.hpp"
/*Legalization*/
#include <iostream>
#include <chrono>
#include <fstream>

// =============================================================================
// CLEAN PARSER MAIN ENTRY POINT
// =============================================================================
// 極簡架構：
// 1. 創建DesignDatabase
// 2. 依序呼叫parser函數
// 3. 輸出統計結果
// =============================================================================

// =============================================================================
// MAIN FUNCTION
// =============================================================================

int main(int argc, char* argv[]) {
    // 設定標準輸出為無緩衝模式，確保即時顯示
    std::cout.setf(std::ios::unitbuf);
    std::cout << "=== ICCAD 2025 Flip-Flop Banking Competition Parser ===" << std::endl;
    
    // 解析命令行參數
    ProgramArguments args = parse_arguments(argc, argv);
    
    // 驗證參數
    if (!args.validate()) {
        std::cout << "\nUse --help for usage information." << std::endl;
        return 1;
    }
    
    // 顯示解析結果
    args.print_summary();
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        // Create design database
        DesignDatabase db;
        db.design_name = "ICCAD_2025_Design";
        
        // Step 1: Parse Liberty files (from command line arguments)
        std::cout << "\n📚 Step 1: Parsing Liberty files..." << std::endl;
        std::cout.flush();
        for (const auto& lib_file : args.lib_files) {
            parse_liberty_file(lib_file, db);
        }
        
        // 建立banking關係
        build_banking_relationships(db);
        
        // 建立FF cell相容性分群
        build_ff_cell_compatibility_groups(db);
        
        // Step 2: Parse LEF files to add physical information to cells
        std::cout << "\n🏗️  Step 2: Parsing LEF files..." << std::endl;
        std::cout.flush();
        for (const auto& lef_file : args.lef_files) {
            parse_lef_file(lef_file, db);
        }
        
        // 輸出完整的Cell Library驗證報告（包含物理資訊）
        //export_cell_library_validation(db);
        
        // Step 3: Parse Verilog files first to create instances and connections
        std::cout << "\n🔌 Step 3: Parsing Verilog netlist..." << std::endl;
        std::cout.flush();
        for (const auto& verilog_file : args.verilog_files) {
            parse_verilog_file(verilog_file, db);
        }
        
        // Step 4: Parse DEF files to add placement information to existing instances
        std::cout << "\n📍 Step 4: Parsing DEF placement..." << std::endl;
        std::cout.flush();
        for (const auto& def_file : args.def_files) {
            parse_def_file(def_file, db);
        }
        
        // Step 5: Parse Weight file for objective function
        std::cout << "\n⚖️  Step 5: Parsing objective weights..." << std::endl;
        std::cout.flush();
        parse_weight_file(args.weight_file, db);
        
        // Step 6: Link instances to cell templates and finalize
        std::cout << "\n🔗 Step 6: Linking instances to cells..." << std::endl;
        std::cout.flush();
        int linked_count = 0;
        for (const auto& pair : db.instances) {
            auto& instance = pair.second;
            auto cell = db.get_cell(instance->cell_type);
            if (cell) {
                instance->cell_template = cell;
                linked_count++;
            } else {
                std::cout << "  WARNING: Cell " << instance->cell_type 
                         << " not found in library" << std::endl;
            }
        }
        std::cout << "  Linked " << linked_count << " instances to cell templates" << std::endl;
        
        // 輸出完整的Instance驗證報告（包含placement和linking資訊）
        //export_instance_validation(db);
        
        // Step 7: Analyze FF pin connections for compatibility checking
        std::cout << "\n🔍 Step 7: Analyzing FF pin connections..." << std::endl;
        std::cout.flush();
        analyze_ff_pin_connections(db);
        
        // Step 8: Detect scan chains from netlist connections
        std::cout << "\n🔗 Step 8: Detecting scan chains..." << std::endl;
        std::cout.flush();
        detect_scan_chains(db);
        
        // Step 9: Build scan chain banking groups
        std::cout << "\n🏗️  Step 9: Building scan chain banking groups..." << std::endl;
        std::cout.flush();
        build_scan_chain_groups(db);
        
        // Step 10: Export FF grouping analysis report
        std::cout << "\n📊 Step 10: Exporting FF grouping analysis report..." << std::endl;
        std::cout.flush();
        export_ff_grouping_report(db);
        
        // Step 11: Initialize Transformation Tracking System
        std::cout << "\n📋 Step 11: Initializing Transformation Tracking..." << std::endl;
        std::cout.flush();
        initialize_transformation_tracking(db);
        

        // Step 12: Strategic Debanking - Convert multi-bit FFs to single-bit for re-optimization
        std::cout << "\n🔧 Step 12: Strategic Debanking..." << std::endl;
        std::cout.flush();
        perform_strategic_debanking(db);
        //export_strategic_debanking_report(db);
        
        // Step 13: Group FF instances for substitution (temporary)
        std::cout << "\n🔗 Step 13: Grouping FF instances for substitution..." << std::endl;
        std::cout.flush();
        group_ff_instances(db);
        
        // Step 14: Calculate optimal FF for each group (cell-level analysis)
        std::cout << "\n⚡ Step 14: Calculating optimal FF for each compatibility group..." << std::endl;
        std::cout.flush();
        calculate_optimal_ff_for_instance_groups(db);
        
        // Step 15: Three-Stage FF Substitution
        std::cout << "\n🔄 Step 15: Three-Stage FF Substitution..." << std::endl;
        std::cout.flush();
        execute_three_stage_substitution(db);
        
        // Step 16: Assign banking types before grouping (critical for correct grouping)
        std::cout << "\n🏷️  Step 16: Assigning banking types..." << std::endl;
        std::cout.flush();
        assign_banking_types(db);
        
        // Step 16.5: Rebuild FF instance groups for banking (after banking type assignment)
        std::cout << "\n🔗 Step 16.5: Rebuilding FF instance groups for banking..." << std::endl;
        std::cout.flush();
        // Clear old groups completely
        db.ff_instance_groups.clear();
        std::cout << "  Cleared old ff_instance_groups" << std::endl;
        
        // Rebuild groups based on hierarchy + clock signal (no scan chain)
        rebuild_ff_instance_groups_for_banking(db);
        
        // Step 17: Export FF instance grouping report
        std::cout << "\n📋 Step 17: Exporting FF instance grouping report..." << std::endl;
        std::cout.flush();
        //export_ff_instance_grouping_report(db);
        
        // Step 18: Strategic Banking
        std::cout << "\n🏦 Step 18: Strategic Banking..." << std::endl;
        std::cout.flush();
        execute_banking_preparation(db);
        
        // Step 17.1: Debank Cluster Re-banking
        execute_debank_cluster_rebanking(db);
        
        // Step 17.2: FSDN Two-Phase Banking
        execute_fsdn_two_phase_banking(db);
        
        // Step 17.3: LSRDPQ4 Single-Phase Banking  
        execute_lsrdpq_single_phase_banking(db);
        
        // Record all banking transformations after all banking steps completed
        record_all_banking_transformations(db);
        
        // Step 18.5: Post-Banking SBFF Substitution
        std::cout << "\n🔄 Step 18.5: Post-Banking SBFF Substitution..." << std::endl;
        std::cout.flush();
        execute_post_banking_substitution(db);
        
        // Capture POST_BANKING stage for complete pipeline report
        std::cout << "  Capturing POST_BANKING stage..." << std::endl;
        std::vector<std::shared_ptr<Instance>> all_instances_after_post_banking;
        for (const auto& inst_pair : db.instances) {
            if (inst_pair.second->is_flip_flop()) {
                all_instances_after_post_banking.push_back(inst_pair.second);
            }
        }
        
        // Get indices of POST_SUBSTITUTE transformation records
        std::vector<size_t> post_substitute_indices;
        for (size_t i = 0; i < db.transformation_history.size(); ++i) {
            if (db.transformation_history[i].operation == TransformationRecord::POST_SUBSTITUTE) {
                post_substitute_indices.push_back(i);
            }
        }
        
        std::cout << "    Found " << post_substitute_indices.size() << " POST_SUBSTITUTE transformation records" << std::endl;
        
        db.complete_pipeline.capture_stage("POST_BANKING", all_instances_after_post_banking, post_substitute_indices, &db.transformation_history);
        
        /*Legalization*/
        std::cout << "\n⚖️  Step 19: Legalization..." << std::endl;
        std::cout.flush();
        Legalizer legalizer(std::numeric_limits<double>::max(), db);  // 傳入整個 DesignDatabase
        legalizer.Abacus();                          // 不需要參數
        legalizer.place();                           // 不需要參數
        //legalizer.writeOutput("legalization_result.txt"); // 只需要文件名
        
        // Legalization完成，但不記錄transformation records
        // (legalization不改變邏輯功能，contest不需要記錄)
        /*Legalization*/

        // Step 17.6: Export Module Instance Distribution Report
        std::cout << "\n📊 Step 17.6: Exporting Module Instance Distribution..." << std::endl;
        std::cout.flush();
        //export_module_instance_distribution(db, "module_instance_distribution.txt");

        // Step 18: Export Complete Pipeline Report for Debugging
        std::cout << "\n📋 Step 18: Exporting Complete Pipeline Report..." << std::endl;
        std::cout.flush();
        export_transformation_report(db, "complete_pipeline_report.txt");
        
        // Step 19: Generate final .v file output
        std::cout << "\n🏆 Step 19: Generating final .v file..." << std::endl;
        std::cout.flush();
        std::string verilog_filename = args.output_name + ".v";
        generate_final_verilog_file(db, verilog_filename);
        
        // Step 20: Generate complete .list file (Pin Mapping + Operation Log)
        std::cout << "\n📝 Step 20: Generating complete .list file with pin mapping..." << std::endl;
        std::cout.flush();
        
        // First generate pin mapping to temporary file
        generate_simple_pin_mapping_file(db, "temp_pin_mapping.list");
        
        // Then generate operation log and merge them
        std::string list_filename = args.output_name + ".list";
        generate_operation_log_file(db, list_filename);
        
        // Step 21: Generate final DEF file with optimized FF placement
        std::cout << "\n🏗️ Step 21: Generating final DEF file..." << std::endl;
        std::cout.flush();
        
        // Determine input DEF file path
        std::string input_def_path;
        if (!args.def_files.empty()) {
            input_def_path = args.def_files[0];  // Use first DEF file
        } else {
            std::cerr << "Error: No DEF file provided for output generation" << std::endl;
            return 1;
        }
        
        // Debug: Check FF instances before DEF generation
        int ff_count_before_def = 0;
        for (const auto& inst_pair : db.instances) {
            if (inst_pair.second->is_flip_flop()) {
                ff_count_before_def++;
            }
        }
        std::cout << "  DEBUG: Found " << ff_count_before_def << " FF instances before DEF generation" << std::endl;
        
        DefOutputGenerator def_generator(db);
        std::string def_filename = args.output_name + ".def";
        def_generator.generate_complete_def_file(input_def_path, def_filename);
        
        std::cout << "  ✓ Generated complete testcase_solution.def (including NETS section)" << std::endl;
        
        // Step 22: Test Simple Pin Mapping System (No DEBANK version)
        // std::cout << "\n🔗 Step 22: Testing Simple Pin Mapping System..." << std::endl;
        // std::cout.flush();
        // export_simple_transformation_chains_report(db, "transformation_chains_report.txt");
        // generate_simple_pin_mapping_file(db, "simple_pin_mapping.list");
        
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cout << "\n❌ Error: " << e.what() << std::endl;
        return 1;
    }
}

