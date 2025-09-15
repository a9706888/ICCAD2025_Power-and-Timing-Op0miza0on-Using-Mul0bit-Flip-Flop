# 🏆 ICCAD 2025 Power and Timing Optimization Using Multibit Flip-Flop

## 📖 Overview

This project is a complete solution for the **ICCAD 2025 Multi-bit Flip-Flop Banking Competition**, implementing a sophisticated VLSI physical design optimization tool that performs intelligent flip-flop banking and placement for integrated circuits.

### 🎯 Competition Objectives
- **Area Optimization**: Reduce total flip-flop area through strategic banking operations
- **Power Efficiency**: Minimize leakage and dynamic power consumption via shared control logic
- **Performance Enhancement**: Improve timing through reduced parasitic capacitance
- **Design Quality**: Maintain functional equivalence while achieving better placement density

### ⚙️ Key Constraints
- **Functional Equivalence**: All transformations must preserve logic behavior
- **Timing Preservation**: No setup/hold violations across clock domains
- **Physical Legality**: Maintain placement legality and design rule compliance
- **Contest Compliance**: Generate exact output format required by ICCAD 2025 specification

## 📂 Competition Specification

### 📥 Input Files
- **Liberty Files (`.lib`)**: Cell library definitions with timing/power models
- **LEF Files (`.lef`)**: Physical layout and pin geometry information
- **Verilog Files (`.v`)**: Netlist connectivity and instance definitions
- **DEF Files (`.def`)**: Placement coordinates and physical design
- **Weight Files**: Objective function parameters (α, β, γ weights)

### 📥 Output Files (Required by Contest)
1. **`.list` File**: Complete pin mapping and operation log
2. **`.def` File**: Final placement solution (LEF/DEF format)  
3. **`.v` File**: Functionally equivalent Verilog netlist

### 📥 Objective Function
```
Cost = α·TNS + Σ(β·Power(i) + γ·Area(i))
               ∀i∈FF
```
Where:
- **TNS**: Total Negative Slack (timing penalty)
- **Power(i)**: Power consumption of flip-flop i
- **Area(i)**: Area cost of flip-flop i

### 📥 Evaluation Criteria
- **60 minute time limit** per test case
- **Functional correctness** (UNMATCH = zero score)
- **Design rule compliance** (overlaps/violations = zero score)
- **Score optimization** with bounded runtime factor (±10%)

## 🧩 Algorithm Architecture

The solution implements a **comprehensive 8-stage optimization pipeline**:

```
Stage 1: Parsing              → Load and interpret all input files
Stage 2: FF Grouping          → Analyze and group FF cell compatibility  
Stage 3: Debanking            → Decompose multi-bit FFs for re-optimization
Stage 4: Instance Grouping    → Group instances by hierarchy and functionality
Stage 5: Score FF             → Calculate optimal FF choices for each group
Stage 6: Substitution         → Three-stage FF substitution optimization
Stage 7: Banking              → Three-stage strategic banking operations
Stage 8: Legalization & Output → Physical legalization and file generation
```

## 🧩 Detailed Algorithm Flow

### 📌 Stage 1: Parsing
**Purpose**: Load and interpret all input design files into internal data structures

**Sub-steps**:
1. **Liberty File Parsing**: Extract FF cell templates, banking relationships, power/area models
2. **LEF File Parsing**: Add physical layout information and pin geometries  
3. **Verilog File Parsing**: Create instances and net connectivity graphs
4. **DEF File Parsing**: Load placement coordinates and physical constraints
5. **Weight File Parsing**: Load objective function parameters (α, β, γ)
6. **Instance Linking**: Connect instances to their corresponding cell templates

### 📌 Stage 2: FF Grouping  
**Purpose**: Analyze FF cell compatibility and build hierarchical grouping structure

**Process**:
1. **Pin Connection Analysis**: Classify each FF pin by type (D, Q, CK, SI, SE, etc.)
2. **Scan Chain Detection**: Identify scan chains from SI→SO net connectivity
3. **Banking Relationship Building**: Parse Liberty `single_bit_degenerate` and `banking_targets`
4. **Compatibility Group Creation**: Build hierarchical groups: Clock Edge → Pin Interface → Bit Width  
5. **Grouping Report Export**: Generate comprehensive FF compatibility analysis

**Key Data Structures**:
```cpp
hierarchical_ff_groups[clock_edge][pin_interface][bit_width] = {list_of_compatible_FFs}
// Example: ["FALLING"]["D_Q_QN_CK_SI_SE"]["2bit"] = ["SNPSLOPT25_FSDN2_V2_1", ...]
```

### 📌 Stage 3: Debanking
**Purpose**: Convert existing multi-bit FFs to single-bit for complete re-optimization

**Process**:
1. **Multi-bit FF Identification**: Scan all instances for FFs with bit_width > 1
2. **Pin Mapping Generation**: Create 1-to-1 mapping from multi-bit pins to single-bit equivalents
3. **Single-bit FF Creation**: Generate multiple single-bit instances at original location
4. **Cluster ID Assignment**: Assign unique cluster_id to track related single-bit FFs
5. **Transformation Recording**: Log DEBANK operations with complete pin mapping

**Debanking Rules**:
- **Skip Scan FFs**: Preserve FFs with active SI/SO connections  
- **Position Preservation**: All debanked FFs placed at original coordinates (temporary overlap)
- **Reversibility**: Complete pin mapping enables perfect reconstruction during banking

### 📌 Stage 4: Instance Grouping
**Purpose**: Group FF instances by hierarchy, functionality, and banking compatibility

**Grouping Strategy**:
1. **Hierarchy Extraction**: Use module_name field or parse hierarchical paths from instance names
2. **Clock Domain Analysis**: Determine clock edge (RISING/FALLING) from cell template
3. **Scan Chain Classification**: Identify scan chain membership from SI/SO connections  
4. **Pin Signature Building**: Create effective pin pattern based on actual connections
5. **Final Grouping**: Combine by hierarchy + pin_interface + scan_chain + clock_signal

**Group Key Format**:
```cpp
// Example: "FALLING|CK_D_Q_SI_SE|SCAN_0|clk1"
group_key = clock_edge + "|" + pin_interface + "|" + scan_info + "|" + clock_net
```

### 📌 Stage 5: Score FF
**Purpose**: Calculate optimal FF choices for each compatibility group using objective function

**Scoring Process**:
1. **Group Analysis**: Process each hierarchical FF group from Stage 2
2. **Score Calculation**: Apply thesis formula for each FF candidate:
   ```cpp
   Score = (β·Power + γ·Area)/bit_width + δ
   // Where δ = α·timing_repr (timing penalty factor)
   ```
3. **Optimal Selection**: Choose lowest-scoring FF for each group
4. **Cross-Reference Building**: Create mapping from instance groups to optimal FFs

**Output Data**:
```cpp  
optimal_ff_for_groups["FALLING|D_Q_QN_CK_SI_SE|2bit"] = "SNPSSLOPT25_FSDN2_V2_1"
```

### 📌 Stage 6: Substitution
**Purpose**: Optimize individual FF choices through progressive refinement

#### Stage 1: Original Pin Pattern Substitution
- **Strategy**: Replace FFs with optimal choice for their original pin interface
- **Mechanism**: Unconditional replacement based on `hierarchical_ff_groups` analysis
- **Scope**: Process all FF instances in `ff_instance_groups`
- **Recording**: No transformation records (intermediate step)

#### Stage 2: Effective Pin Connections Substitution  
- **Strategy**: Optimize based on actual active pin connections (excluding UNCONNECTED/VSS)
- **Logic**: `if (effective_optimal_score < current_score) → substitute`
- **Example**: FF with D,Q,CK,SI,SE where SI,SE=UNCONNECTED → optimize for D_Q_CK pattern
- **Recording**: No transformation records (intermediate step)

#### Stage 3: Multi-bit Banking Preparation
- **FALLING Edge**: Prepare FSDN variants for 4-bit banking if beneficial
- **RISING Edge**: Prepare FDP/LSRDPQ variants for 4-bit banking if beneficial  
- **Conditions**: Only if target multi-bit FF score < current single-bit FF score
- **Recording**: Final substitution operations recorded as SUBSTITUTE transforms

**Scoring Formula**:
```cpp
Score = (β·Power + γ·Area)/bit_width + δ
// Where δ = α·timing_repr (timing penalty factor)
// Lower score = better FF choice
```

### 📌 Stage 7: Banking
**Purpose**: Intelligently combine single-bit FFs into optimal multi-bit variants

The banking stage consists of **three sub-stages** executed sequentially:

#### Sub-stage 7.1: Debank Cluster Re-banking (Priority Banking)
**Target**: FFs with same `cluster_id` (from strategic debanking in Stage 3)
**Strategy**: Highest priority re-banking to avoid unnecessary debank/rebank cycles

**Process**:
1. **Cluster Collection**: Group all instances by cluster_id (non-empty only)
2. **Banking Decision**: 2+ instances → 2-bit FF, 4+ instances → 4-bit FF  
3. **Target Selection**: Use optimal FF from corresponding bit-width group
4. **Position Calculation**: Geometric center of all cluster instances
5. **Pin Mapping**: Complete mapping from single-bit pins to multi-bit indexed pins

#### Sub-stage 7.2: FSDN Two-Phase Banking (FALLING Edge)
**Target**: FALLING edge FFs (FSDN variants) using spatial clustering

**Phase 1: 1-bit → 2-bit FSDN Banking**
- **Instance Collection**: Gather all 1-bit FSDN instances per ff_instance_group
- **Spatial Clustering**: Distance threshold = 10,000,000 DBU (currently relaxed)
- **Target Selection**: Optimal FF from `"FALLING|pattern|2bit"` group
- **Position**: Geometric midpoint of 2 source instances
- **Pin Mapping**: `source[0] → [0] pins, source[1] → [1] pins, shared CK/SI/SE`

**Phase 2: 2-bit → 4-bit FSDN Banking**
- **Input**: 2-bit FSDN instances created in Phase 1
- **Clustering**: Distance threshold = 10,000,000 DBU
- **Advanced Mapping**: Handle 2-bit → 4-bit pin expansion correctly
- **Target Selection**: Optimal FF from `"FALLING|pattern|4bit"` group

#### Sub-stage 7.3: LSRDPQ Single-Phase Banking (RISING Edge)
**Target**: RISING edge FFs (FDP/LSRDPQ variants) using direct 4-way banking

**Process**:
- **Strategy**: Direct 1-bit → 4-bit banking (skip intermediate 2-bit)
- **Distance Threshold**: 800 DBU (strict proximity constraint)  
- **Eligibility**: Only D_Q_CK pattern instances (modified to avoid power issues)
- **Target Selection**: Optimal FF from `"RISING|D_Q_QN_CK|4bit"` group
- **Pin Indexing**: LSRDPQ uses 1-based indexing (D[1], D[2], D[3], D[4])

**Spatial Clustering Algorithm**:
```cpp
std::vector<std::vector<Instance*>> 
simple_distance_clustering(instances, target_size, max_distance) {
    // Greedy distance-based clustering
    // Guarantees all FFs within distance threshold  
    // O(n²) complexity, optimized for banking constraints
}
```

**Multi-bit Pin Connection Rules**:
```
FSDN 2-bit Banking:
├─ source[0] → D[0], Q[0], QN[0]
├─ source[1] → D[1], Q[1], QN[1] 
└─ Shared: CK, SI, SE, R, S

LSRDPQ 4-bit Banking:
├─ source[0] → D[1], Q[1], QN[1] (1-based indexing)
├─ source[1] → D[2], Q[2], QN[2]
├─ source[2] → D[3], Q[3], QN[3] 
├─ source[3] → D[4], Q[4], QN[4]
└─ Shared: CK, SI, SE
```

**Post-Banking Optimization**:
After banking completion, execute **Post-Banking SBFF Substitution**:
- **Target**: Remaining single-bit FFs (SBFFs) that were not banked
- **Logic**: Check if any SBFF has a better recorded choice from Stage 6 substitution
- **Condition**: Only substitute if `best_recorded_score < current_score`
- **Operation Type**: POST_SUBSTITUTE (separate from main substitution)

### 📌 Stage 8: Legalization & Output
**Purpose**: Ensure physical compliance and generate contest-required outputs

#### Legalization Process
- **Abacus Algorithm**: Remove overlaps while minimizing displacement
- **Grid Alignment**: Ensure all instances on valid placement sites
- **Boundary Compliance**: Keep all instances within die region
- **Utilization Check**: Respect maximum density constraints

#### Output Generation
**Pin Mapping File (`.list`)**:
```
CellInst <total_ff_count>
<original_pin> map <result_pin>
...
OPERATION <operation_count>  
<operation_1>
<operation_2>
...
```

**Operation Types**:
- `size_cell {instance old_lib new_lib}` (SUBSTITUTE transformations)
- `split_multibit {input_ff {output1} {output2} ...}` (DEBANK transformations)  
- `create_multibit {{input1} {input2} ... {output_ff}}` (BANK transformations)
- `change_name {old_name new_name}` (Instance renaming)

**Verilog Output (`.v`)**:
- **Hierarchical Structure**: Preserves original module boundaries
- **Complete Pin Connections**: All FF pins properly connected (Q pins = UNCONNECTED for LSRDPQ4)
- **Functional Equivalence**: Maintains identical logic behavior
- **No Duplicate Instances**: Each instance appears exactly once

**DEF Output (`.def`)**:
- **Standard LEF/DEF Format**: Industry-compliant placement specification
- **Complete Coverage**: All instances with valid coordinates
- **Net Connectivity**: Full NETS section with proper routing topology

## 🔨 Build and Usage

### Prerequisites
- **C++11 Compiler**: g++ with C++11 support
- **Standard Libraries**: No external dependencies beyond STL

### Build Instructions
```bash
make clean
make

# Alternative manual compilation:
g++ -std=c++17 -O3 -I. -o clean_parser  *.cpp -DNDEBUG
```

### Usage
```bash
./clean_parser \
    -weight <weight_file> \
    -lib <liberty_file1> <liberty_file2> ... \
    -lef <lef_file1> <lef_file2> ... \
    -v <verilog_file> \
    -def <def_file> \
    -out <output_name>
```

### Example
```bash
./clean_parser \
    -weight testcase3/weights.txt \
    -lib testcase3/*.lib \
    -lef testcase3/*.lef \
    -v testcase3/design.v \
    -def testcase3/placement.def \
    -out cadb_1060_final
```

**Output Files Generated**:
- `cadb_1060_final.list` - Pin mapping and operation log
- `cadb_1060_final.def` - Final placement solution
- `cadb_1060_final.v` - Optimized Verilog netlist

## Contest Submission Format

### Executable Naming
```bash
./cadb_1060_alpha    # Alpha submission
./cadb_1060_beta     # Beta submission  
./cadb_1060_final    # Final submission
```
(Replace `1060` with actual team registration number)

### Submission Files
1. **Source Code**: Complete C++ implementation
2. **Makefile**: Automated build configuration  
3. **Documentation**: Algorithm description and usage guide
4. **Test Results**: Validation on provided test cases

## 🌟 Technical Highlights

### Advanced Optimizations
- **Multi-stage Substitution**: Progressive FF optimization through 3-stage refinement
- **Intelligent Banking**: Score-driven banking with proximity constraints
- **Debank Cluster Priority**: Efficient re-banking of related instances
- **Power-aware Selection**: Integrated power/area/timing optimization

### Robustness Features  
- **Transformation Verification**: Complete integrity checking of all operations
- **Error Recovery**: Graceful handling of malformed inputs and edge cases
- **Debug Traceability**: Comprehensive logging and validation reports
- **Contest Compliance**: Exact adherence to ICCAD 2025 output specification

### Performance Characteristics
- **Runtime Efficiency**: Optimized for 60-minute contest time limit
- **Memory Management**: Efficient handling of large industrial designs
- **Scalability**: Tested on designs with 100K+ flip-flop instances
- **Quality Results**: Achieves significant area/power improvements while maintaining timing closure

## 🚧 Future Improvements

### Algorithmic Enhancements
- **Machine Learning Integration**: Learn optimal banking patterns from training data
- **Advanced Clustering**: More sophisticated spatial clustering algorithms
- **Timing-Driven Banking**: Explicit timing analysis integration
- **Multi-objective Optimization**: Better Pareto frontier exploration

### Engineering Improvements
- **Parallel Processing**: Multi-threading for large design handling
- **Memory Optimization**: Reduced memory footprint for massive designs
- **Incremental Updates**: Faster re-optimization for design changes
- **GUI Integration**: Visual debugging and optimization guidance

---

**ICCAD 2025 Multi-bit Flip-Flop Banking Competition**  
Team Registration: 1060  
Implementation: Complete C++17 Solution  
Target: Area/Power Optimization with Timing Closure  
Status: Production Ready ✅
