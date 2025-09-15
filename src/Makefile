# Clean Parser Architecture Makefile
# 簡潔的編譯配置，一個main.cpp就能測試整個架構

CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2 -I.

# Source files
SOURCES = main.cpp parsers.cpp argument_parser.cpp scan_chain_detection.cpp strategic_debanking.cpp ff_instance_grouping.cpp substitution.cpp banking.cpp transformation_tracking.cpp transformation_verification.cpp Legalization.cpp simple_pin_mapping.cpp
HEADERS = data_structures.hpp parsers.hpp argument_parser.hpp substitution.hpp def_output_generator.hpp Legalization.hpp

# Target executable
TARGET = cadb_1060_final

.PHONY: all clean test

# Default target
all: $(TARGET)

# Build the clean parser
$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ $(SOURCES)

# Test with testcase1
test: $(TARGET)
	@echo "Testing clean parser architecture..."
	./$(TARGET) ../2025/testcase1

# Clean build artifacts
clean:
	rm -f $(TARGET)
	rm -f *.o
	rm -f *.txt
	rm -f *.list
	rm -f *.v
	rm -f *.def

testcase1:
	./cadb_1060_final -weight "D:/git_new/2025/testcase1/testcase1_weight" -lib "D:/git_new/2025/testcase1/SNPSHOPT25/liberty/nldm/base/snps25hopt_base_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSHOPT25/liberty/nldm/cg/snps25hopt_cg_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSHOPT25/liberty/nldm/dlvl/snps25hopt_dlvl_tt0p8v25c_i0p8v.lib" "D:/git_new/2025/testcase1/SNPSHOPT25/liberty/nldm/iso/snps25hopt_iso_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSHOPT25/liberty/nldm/pg/snps25hopt_pg_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSHOPT25/liberty/nldm/ret/snps25hopt_ret_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSHOPT25/liberty/nldm/ulvl/snps25hopt_ulvl_tt0p8v25c_i0p65v.lib" "D:/git_new/2025/testcase1/SNPSHOPT25/liberty/nldm/ulvl/snps25hopt_ulvl_tt0p8v25c_i0p8v.lib" "D:/git_new/2025/testcase1/SNPSLOPT25/liberty/nldm/base/snps25lopt_base_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSLOPT25/liberty/nldm/cg/snps25lopt_cg_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSLOPT25/liberty/nldm/dlvl/snps25lopt_dlvl_tt0p8v25c_i0p8v.lib" "D:/git_new/2025/testcase1/SNPSLOPT25/liberty/nldm/iso/snps25lopt_iso_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSLOPT25/liberty/nldm/pg/snps25lopt_pg_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSLOPT25/liberty/nldm/ret/snps25lopt_ret_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSLOPT25/liberty/nldm/ulvl/snps25lopt_ulvl_tt0p8v25c_i0p65v.lib" "D:/git_new/2025/testcase1/SNPSLOPT25/liberty/nldm/ulvl/snps25lopt_ulvl_tt0p8v25c_i0p8v.lib" "D:/git_new/2025/testcase1/SNPSROPT25/liberty/nldm/base/snps25ropt_base_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSROPT25/liberty/nldm/cg/snps25ropt_cg_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSROPT25/liberty/nldm/dlvl/snps25ropt_dlvl_tt0p8v25c_i0p8v.lib" "D:/git_new/2025/testcase1/SNPSROPT25/liberty/nldm/iso/snps25ropt_iso_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSROPT25/liberty/nldm/pg/snps25ropt_pg_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSROPT25/liberty/nldm/ret/snps25ropt_ret_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSROPT25/liberty/nldm/ulvl/snps25ropt_ulvl_tt0p8v25c_i0p65v.lib" "D:/git_new/2025/testcase1/SNPSROPT25/liberty/nldm/ulvl/snps25ropt_ulvl_tt0p8v25c_i0p8v.lib" "D:/git_new/2025/testcase1/SNPSSLOPT25/liberty/nldm/base/snps25slopt_base_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSSLOPT25/liberty/nldm/cg/snps25slopt_cg_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSSLOPT25/liberty/nldm/dlvl/snps25slopt_dlvl_tt0p8v25c_i0p8v.lib" "D:/git_new/2025/testcase1/SNPSSLOPT25/liberty/nldm/iso/snps25slopt_iso_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSSLOPT25/liberty/nldm/pg/snps25slopt_pg_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSSLOPT25/liberty/nldm/ret/snps25slopt_ret_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSSLOPT25/liberty/nldm/ulvl/snps25slopt_ulvl_tt0p8v25c_i0p65v.lib" "D:/git_new/2025/testcase1/SNPSSLOPT25/liberty/nldm/ulvl/snps25slopt_ulvl_tt0p8v25c_i0p8v.lib" -lef "D:/git_new/2025/testcase1/SNPSHOPT25/lef/snps25hopt.lef" "D:/git_new/2025/testcase1/SNPSLOPT25/lef/snps25lopt.lef" "D:/git_new/2025/testcase1/SNPSROPT25/lef/snps25ropt.lef" "D:/git_new/2025/testcase1/SNPSSLOPT25/lef/snps25slopt.lef" -v "D:/git_new/2025/testcase1/testcase1.v" -def "D:/git_new/2025/testcase1/testcase1.def" -sdc "D:/git_new/2025/testcase1/testcase1.sdc" -out "cadb_1060_final"

testcase2:
	./cadb_1060_final -weight "D:/git_new/2025/testcase2/testcase2_weight" -lib "D:/git_new/2025/testcase1/SNPSHOPT25/liberty/nldm/base/snps25hopt_base_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSHOPT25/liberty/nldm/cg/snps25hopt_cg_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSHOPT25/liberty/nldm/dlvl/snps25hopt_dlvl_tt0p8v25c_i0p8v.lib" "D:/git_new/2025/testcase1/SNPSHOPT25/liberty/nldm/iso/snps25hopt_iso_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSHOPT25/liberty/nldm/pg/snps25hopt_pg_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSHOPT25/liberty/nldm/ret/snps25hopt_ret_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSHOPT25/liberty/nldm/ulvl/snps25hopt_ulvl_tt0p8v25c_i0p65v.lib" "D:/git_new/2025/testcase1/SNPSHOPT25/liberty/nldm/ulvl/snps25hopt_ulvl_tt0p8v25c_i0p8v.lib" "D:/git_new/2025/testcase1/SNPSLOPT25/liberty/nldm/base/snps25lopt_base_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSLOPT25/liberty/nldm/cg/snps25lopt_cg_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSLOPT25/liberty/nldm/dlvl/snps25lopt_dlvl_tt0p8v25c_i0p8v.lib" "D:/git_new/2025/testcase1/SNPSLOPT25/liberty/nldm/iso/snps25lopt_iso_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSLOPT25/liberty/nldm/pg/snps25lopt_pg_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSLOPT25/liberty/nldm/ret/snps25lopt_ret_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSLOPT25/liberty/nldm/ulvl/snps25lopt_ulvl_tt0p8v25c_i0p65v.lib" "D:/git_new/2025/testcase1/SNPSLOPT25/liberty/nldm/ulvl/snps25lopt_ulvl_tt0p8v25c_i0p8v.lib" "D:/git_new/2025/testcase1/SNPSROPT25/liberty/nldm/base/snps25ropt_base_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSROPT25/liberty/nldm/cg/snps25ropt_cg_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSROPT25/liberty/nldm/dlvl/snps25ropt_dlvl_tt0p8v25c_i0p8v.lib" "D:/git_new/2025/testcase1/SNPSROPT25/liberty/nldm/iso/snps25ropt_iso_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSROPT25/liberty/nldm/pg/snps25ropt_pg_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSROPT25/liberty/nldm/ret/snps25ropt_ret_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSROPT25/liberty/nldm/ulvl/snps25ropt_ulvl_tt0p8v25c_i0p65v.lib" "D:/git_new/2025/testcase1/SNPSROPT25/liberty/nldm/ulvl/snps25ropt_ulvl_tt0p8v25c_i0p8v.lib" "D:/git_new/2025/testcase1/SNPSSLOPT25/liberty/nldm/base/snps25slopt_base_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSSLOPT25/liberty/nldm/cg/snps25slopt_cg_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSSLOPT25/liberty/nldm/dlvl/snps25slopt_dlvl_tt0p8v25c_i0p8v.lib" "D:/git_new/2025/testcase1/SNPSSLOPT25/liberty/nldm/iso/snps25slopt_iso_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSSLOPT25/liberty/nldm/pg/snps25slopt_pg_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSSLOPT25/liberty/nldm/ret/snps25slopt_ret_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSSLOPT25/liberty/nldm/ulvl/snps25slopt_ulvl_tt0p8v25c_i0p65v.lib" "D:/git_new/2025/testcase1/SNPSSLOPT25/liberty/nldm/ulvl/snps25slopt_ulvl_tt0p8v25c_i0p8v.lib" -lef "D:/git_new/2025/testcase1/SNPSHOPT25/lef/snps25hopt.lef" "D:/git_new/2025/testcase1/SNPSLOPT25/lef/snps25lopt.lef" "D:/git_new/2025/testcase1/SNPSROPT25/lef/snps25ropt.lef" "D:/git_new/2025/testcase1/SNPSSLOPT25/lef/snps25slopt.lef" -v "D:/git_new/2025/testcase2/testcase2.v" -def "D:/git_new/2025/testcase2/testcase2.def" -sdc "D:/git_new/2025/testcase2/testcase2.sdc" -out "cadb_1060_final"

testcase3:
	./cadb_1060_final -weight "D:/git_new/2025/testcase3/testcase3_weight" -lib "D:/git_new/2025/testcase1/SNPSHOPT25/liberty/nldm/base/snps25hopt_base_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSHOPT25/liberty/nldm/cg/snps25hopt_cg_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSHOPT25/liberty/nldm/dlvl/snps25hopt_dlvl_tt0p8v25c_i0p8v.lib" "D:/git_new/2025/testcase1/SNPSHOPT25/liberty/nldm/iso/snps25hopt_iso_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSHOPT25/liberty/nldm/pg/snps25hopt_pg_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSHOPT25/liberty/nldm/ret/snps25hopt_ret_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSHOPT25/liberty/nldm/ulvl/snps25hopt_ulvl_tt0p8v25c_i0p65v.lib" "D:/git_new/2025/testcase1/SNPSHOPT25/liberty/nldm/ulvl/snps25hopt_ulvl_tt0p8v25c_i0p8v.lib" "D:/git_new/2025/testcase1/SNPSLOPT25/liberty/nldm/base/snps25lopt_base_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSLOPT25/liberty/nldm/cg/snps25lopt_cg_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSLOPT25/liberty/nldm/dlvl/snps25lopt_dlvl_tt0p8v25c_i0p8v.lib" "D:/git_new/2025/testcase1/SNPSLOPT25/liberty/nldm/iso/snps25lopt_iso_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSLOPT25/liberty/nldm/pg/snps25lopt_pg_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSLOPT25/liberty/nldm/ret/snps25lopt_ret_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSLOPT25/liberty/nldm/ulvl/snps25lopt_ulvl_tt0p8v25c_i0p65v.lib" "D:/git_new/2025/testcase1/SNPSLOPT25/liberty/nldm/ulvl/snps25lopt_ulvl_tt0p8v25c_i0p8v.lib" "D:/git_new/2025/testcase1/SNPSROPT25/liberty/nldm/base/snps25ropt_base_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSROPT25/liberty/nldm/cg/snps25ropt_cg_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSROPT25/liberty/nldm/dlvl/snps25ropt_dlvl_tt0p8v25c_i0p8v.lib" "D:/git_new/2025/testcase1/SNPSROPT25/liberty/nldm/iso/snps25ropt_iso_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSROPT25/liberty/nldm/pg/snps25ropt_pg_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSROPT25/liberty/nldm/ret/snps25ropt_ret_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSROPT25/liberty/nldm/ulvl/snps25ropt_ulvl_tt0p8v25c_i0p65v.lib" "D:/git_new/2025/testcase1/SNPSROPT25/liberty/nldm/ulvl/snps25ropt_ulvl_tt0p8v25c_i0p8v.lib" "D:/git_new/2025/testcase1/SNPSSLOPT25/liberty/nldm/base/snps25slopt_base_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSSLOPT25/liberty/nldm/cg/snps25slopt_cg_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSSLOPT25/liberty/nldm/dlvl/snps25slopt_dlvl_tt0p8v25c_i0p8v.lib" "D:/git_new/2025/testcase1/SNPSSLOPT25/liberty/nldm/iso/snps25slopt_iso_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSSLOPT25/liberty/nldm/pg/snps25slopt_pg_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSSLOPT25/liberty/nldm/ret/snps25slopt_ret_tt0p8v25c.lib" "D:/git_new/2025/testcase1/SNPSSLOPT25/liberty/nldm/ulvl/snps25slopt_ulvl_tt0p8v25c_i0p65v.lib" "D:/git_new/2025/testcase1/SNPSSLOPT25/liberty/nldm/ulvl/snps25slopt_ulvl_tt0p8v25c_i0p8v.lib" -lef "D:/git_new/2025/testcase1/SNPSHOPT25/lef/snps25hopt.lef" "D:/git_new/2025/testcase1/SNPSLOPT25/lef/snps25lopt.lef" "D:/git_new/2025/testcase1/SNPSROPT25/lef/snps25ropt.lef" "D:/git_new/2025/testcase1/SNPSSLOPT25/lef/snps25slopt.lef" -v "D:/git_new/2025/testcase3/testcase3.v" -def "D:/git_new/2025/testcase3/testcase3.def" -sdc "D:/git_new/2025/testcase3/testcase3.sdc" -out "cadb_1060_final"

test_multibit_debank: $(TARGET)
	@echo "Testing with multibit debank test case (complete 28 lib + 4 lef)..."
	./cadb_1060_final \
	-weight "D:/git_new/test_multibit_debank/test_multibit_weight" \
	-lib "D:/git_new/2025/testcase1/SNPSHOPT25/liberty/nldm/base/snps25hopt_base_tt0p8v25c.lib" \
	"D:/git_new/2025/testcase1/SNPSHOPT25/liberty/nldm/cg/snps25hopt_cg_tt0p8v25c.lib" \
	"D:/git_new/2025/testcase1/SNPSHOPT25/liberty/nldm/dlvl/snps25hopt_dlvl_tt0p8v25c_i0p8v.lib" \
	"D:/git_new/2025/testcase1/SNPSHOPT25/liberty/nldm/iso/snps25hopt_iso_tt0p8v25c.lib" \
	"D:/git_new/2025/testcase1/SNPSHOPT25/liberty/nldm/pg/snps25hopt_pg_tt0p8v25c.lib" \
	"D:/git_new/2025/testcase1/SNPSHOPT25/liberty/nldm/ret/snps25hopt_ret_tt0p8v25c.lib" \
	"D:/git_new/2025/testcase1/SNPSHOPT25/liberty/nldm/ulvl/snps25hopt_ulvl_tt0p8v25c_i0p65v.lib" \
	"D:/git_new/2025/testcase1/SNPSHOPT25/liberty/nldm/ulvl/snps25hopt_ulvl_tt0p8v25c_i0p8v.lib" \
	"D:/git_new/2025/testcase1/SNPSLOPT25/liberty/nldm/base/snps25lopt_base_tt0p8v25c.lib" \
	"D:/git_new/2025/testcase1/SNPSLOPT25/liberty/nldm/cg/snps25lopt_cg_tt0p8v25c.lib" \
	"D:/git_new/2025/testcase1/SNPSLOPT25/liberty/nldm/dlvl/snps25lopt_dlvl_tt0p8v25c_i0p8v.lib" \
	"D:/git_new/2025/testcase1/SNPSLOPT25/liberty/nldm/iso/snps25lopt_iso_tt0p8v25c.lib" \
	"D:/git_new/2025/testcase1/SNPSLOPT25/liberty/nldm/pg/snps25lopt_pg_tt0p8v25c.lib" \
	"D:/git_new/2025/testcase1/SNPSLOPT25/liberty/nldm/ret/snps25lopt_ret_tt0p8v25c.lib" \
	"D:/git_new/2025/testcase1/SNPSLOPT25/liberty/nldm/ulvl/snps25lopt_ulvl_tt0p8v25c_i0p65v.lib" \
	"D:/git_new/2025/testcase1/SNPSLOPT25/liberty/nldm/ulvl/snps25lopt_ulvl_tt0p8v25c_i0p8v.lib" \
	"D:/git_new/2025/testcase1/SNPSROPT25/liberty/nldm/base/snps25ropt_base_tt0p8v25c.lib" \
	"D:/git_new/2025/testcase1/SNPSROPT25/liberty/nldm/cg/snps25ropt_cg_tt0p8v25c.lib" \
	"D:/git_new/2025/testcase1/SNPSROPT25/liberty/nldm/dlvl/snps25ropt_dlvl_tt0p8v25c_i0p8v.lib" \
	"D:/git_new/2025/testcase1/SNPSROPT25/liberty/nldm/iso/snps25ropt_iso_tt0p8v25c.lib" \
	"D:/git_new/2025/testcase1/SNPSROPT25/liberty/nldm/pg/snps25ropt_pg_tt0p8v25c.lib" \
	"D:/git_new/2025/testcase1/SNPSROPT25/liberty/nldm/ret/snps25ropt_ret_tt0p8v25c.lib" \
	"D:/git_new/2025/testcase1/SNPSROPT25/liberty/nldm/ulvl/snps25ropt_ulvl_tt0p8v25c_i0p65v.lib" \
	"D:/git_new/2025/testcase1/SNPSROPT25/liberty/nldm/ulvl/snps25ropt_ulvl_tt0p8v25c_i0p8v.lib" \
	"D:/git_new/2025/testcase1/SNPSSLOPT25/liberty/nldm/base/snps25slopt_base_tt0p8v25c.lib" \
	"D:/git_new/2025/testcase1/SNPSSLOPT25/liberty/nldm/cg/snps25slopt_cg_tt0p8v25c.lib" \
	"D:/git_new/2025/testcase1/SNPSSLOPT25/liberty/nldm/dlvl/snps25slopt_dlvl_tt0p8v25c_i0p8v.lib" \
	"D:/git_new/2025/testcase1/SNPSSLOPT25/liberty/nldm/iso/snps25slopt_iso_tt0p8v25c.lib" \
	"D:/git_new/2025/testcase1/SNPSSLOPT25/liberty/nldm/pg/snps25slopt_pg_tt0p8v25c.lib" \
	"D:/git_new/2025/testcase1/SNPSSLOPT25/liberty/nldm/ret/snps25slopt_ret_tt0p8v25c.lib" \
	"D:/git_new/2025/testcase1/SNPSSLOPT25/liberty/nldm/ulvl/snps25slopt_ulvl_tt0p8v25c_i0p65v.lib" \
	"D:/git_new/2025/testcase1/SNPSSLOPT25/liberty/nldm/ulvl/snps25slopt_ulvl_tt0p8v25c_i0p8v.lib" \
	-lef "D:/git_new/2025/testcase1/SNPSHOPT25/lef/snps25hopt.lef" \
	"D:/git_new/2025/testcase1/SNPSLOPT25/lef/snps25lopt.lef" \
	"D:/git_new/2025/testcase1/SNPSROPT25/lef/snps25ropt.lef" \
	"D:/git_new/2025/testcase1/SNPSSLOPT25/lef/snps25slopt.lef" \
	-v "D:/git_new/test_multibit_debank/test_multibit.v" \
	-def "D:/git_new/test_multibit_debank/test_multibit.def" -out "cadb_1060_final"

# Display help
help:
	@echo "Clean Parser Architecture Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all     - Build clean parser"
	@echo "  test    - Build and test with testcase1"
	@echo "  clean   - Remove built files"
	@echo "  test_multibit_debank - Test with multibit debank (28 lib + 4 lef)"
	@echo "  help    - Show this help"
	@echo ""
	@echo "Usage:"
	@echo "  make all"
	@echo "  make test"
	@echo "  make test_multibit_debank"
	@echo "  ./clean_parser <testcase_path>"