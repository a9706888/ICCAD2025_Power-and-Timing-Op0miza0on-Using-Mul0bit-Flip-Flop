#ifndef SUBSTITUTION_HPP
#define SUBSTITUTION_HPP

#include "data_structures.hpp"

// =============================================================================
// THREE-STAGE FF SUBSTITUTION STRATEGY
// =============================================================================

// Score calculation
double calculate_ff_score(const std::string& cell_name, const DesignDatabase& db);

// Key conversion utilities
std::string convert_instance_key_to_hierarchical_key(const std::string& instance_key);

// Stage 1: Original Pin Pattern Substitution
void execute_stage1_substitution(DesignDatabase& db);

// Stage 2: Effective Pin Connections Substitution 
void execute_stage2_substitution(DesignDatabase& db);

// Stage 3: FALLING Edge MBFF Banking Preparation
void execute_stage3_substitution(DesignDatabase& db);

// Main orchestrator
void execute_three_stage_substitution(DesignDatabase& db);

#endif // SUBSTITUTION_HPP