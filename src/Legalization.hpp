#ifndef LEGALIZATION_HPP
#define LEGALIZATION_HPP

#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <cmath>
#include <limits>
#include <algorithm>
#include <fstream>
#include "data_structures.hpp"

// Legalization specific structures
struct LegalizationConfig {
    double max_displacement = std::numeric_limits<double>::max();
};

class Legalizer {
public:
    // 構造函數只接受 DesignDatabase
    Legalizer(double max_disp, DesignDatabase& db);
    
    // Main legalization function - 對應原始的 Abacus 函數
    void Abacus();
    
    // Build sub-rows by splitting around blockages (non-flip-flop instances)
    void buildSubRows( std::vector<std::shared_ptr<Instance>>& blockage_instances);
    
    // Calculate total and maximum displacement
    std::pair<double, double> calculate_displacement() const;
    
    // Write results to file - 對應原始的 writeOutput
    void writeOutput(const std::string& filename) const;
    
    // Update final positions - 對應原始的 place 函數
    void place();

private:
    double max_disp_;
    DesignDatabase* db_;  // 指向整個數據庫
    
    // 輔助函數：分類 instances
    void classify_instances(std::vector<std::shared_ptr<Instance>>& ff_instances,
                           std::vector<std::shared_ptr<Instance>>& blockage_instances) const;
    
    // Helper functions - 對應原始的私有函數
    int findBestRow(const Instance& instance);
    int findSubrowpos(const Instance& instance, const PlacementRow& row);
    
    // Cluster management - 對應原始的 cluster 操作
    void AddCell(Cluster& cluster, Instance& instance, double tempXpos, double placeCellwidth);
    void AddCluster(Cluster& pred, Cluster& curr);
    void Collapse(Cluster& cluster, double xmin, double xmax, SubRow& sr, double sitew);
    
    // Place instance in row - 對應原始的 placeRow
    double placeRow(const PlacementRow& row, Instance& instance, SubRow& sr, 
                   bool final, bool check);
};

// Utility functions
double calculate_euclidean_distance(const Point& p1, const Point& p2);

#endif // LEGALIZATION_HPP