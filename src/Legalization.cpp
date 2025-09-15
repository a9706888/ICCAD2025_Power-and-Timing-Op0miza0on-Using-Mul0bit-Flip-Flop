#include "Legalization.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iomanip>

// 移除舊的構造函數，只保留使用 DesignDatabase 的版本
Legalizer::Legalizer(double max_disp, DesignDatabase& db)
    : max_disp_(max_disp), db_(&db) {
    
    // Assign IDs to rows and initialize row properties
    for (int i = 0; i < static_cast<int>(db_->placement_rows.size()); ++i) {
        db_->placement_rows[i].id = i;
        // 確保 height 和 site_width 正確設置
        if (db_->placement_rows[i].height == 0.0) {
            db_->placement_rows[i].height = db_->placement_rows[i].step_y;
        }
        if (db_->placement_rows[i].site_width == 0.0) {
            db_->placement_rows[i].site_width = db_->placement_rows[i].step_x;
        }
        
        // 如果還沒有 subrows，創建初始 subrow
        if (db_->placement_rows[i].subrows.empty()) {
            SubRow initial_sub_row(db_->placement_rows[i].origin.x, 
                                  db_->placement_rows[i].origin.x + 
                                  db_->placement_rows[i].step_x * db_->placement_rows[i].num_x);
            db_->placement_rows[i].subrows.push_back(initial_sub_row);
        }
    }
}

void Legalizer::classify_instances(std::vector<std::shared_ptr<Instance>>& ff_instances,
                                   std::vector<std::shared_ptr<Instance>>& blockage_instances) const {
    // 清空輸入的 vectors
    ff_instances.clear();
    blockage_instances.clear();
    
    std::cout << "Classifying " << db_->instances.size() << " total instances..." << std::endl;
    
    int ff_count = 0;
    int blockage_count = 0;;
    int no_template_count = 0;
    
    for (const auto& pair : db_->instances) {
        const auto& instance = pair.second;
        
        // 判斷是否為 flip-flop
        if (instance->is_flip_flop()) {
            ff_count++;
            ff_instances.push_back(instance);
        } else 
        {
            instance->x_new = instance->position.x;
            instance->y_new = instance->position.y;
            blockage_instances.push_back(instance);
            blockage_count++;
        }
    }
    
    std::cout << "Classification Summary:" << std::endl;
    std::cout << "  Total instances: " << db_->instances.size() << std::endl;
    std::cout << "  Instances without cell_template: " << no_template_count << std::endl;
    std::cout << "  Total flip-flops found: " << ff_count << std::endl;
    std::cout << "  Eligible flip-flops for legalization: " << ff_instances.size() << std::endl;
    std::cout << "  Blockage instances (non-FF, placed): " << blockage_instances.size() << std::endl;
}

void Legalizer::Abacus() 
{
    std::cout << "Starting Abacus legalization..." << std::endl;
    
    // Step 1: 分類 instances
    std::vector<std::shared_ptr<Instance>> ff_instances;
    std::vector<std::shared_ptr<Instance>> blockage_instances;
    classify_instances(ff_instances, blockage_instances);
    
    // Step 2: Build sub-rows by splitting around blockages
    buildSubRows(blockage_instances);
    
    // Step 3: Sort flip-flop instances by x-coordinate for processing
    std::sort(ff_instances.begin(), ff_instances.end(), 
              [](const std::shared_ptr<Instance>& a, const std::shared_ptr<Instance>& b) {
                  return a->position.x < b->position.x;
              });
    
    // Step 4: Process each flip-flop instance
    int processed_count = 0;
    for (auto& instance : ff_instances) {
        
        double Cbest = std::numeric_limits<double>::max();
        int originRowIdx = findBestRow(*instance);
        
        if (originRowIdx == -1) {
            std::cerr << "WARN: instance " << instance->name << " cannot fit any [row]; skipping.\n";
            continue;
        }
        
        int bestRowIdx = -1;
        int bestSubRowIdx = -1;
        
        // Search upward and downward from the closest row
        for (int i = 0; i < static_cast<int>(db_->placement_rows.size()); ++i) {
            int rowidx1 = originRowIdx + i; // 向上找
            int rowidx2 = originRowIdx - i; // 向下找
            
            bool up = false, down = false;
            
            // 判斷需不需要執行
            if (rowidx1 >= static_cast<int>(db_->placement_rows.size())) up = false;
            else if (std::abs(instance->position.y - db_->placement_rows[rowidx1].origin.y) < Cbest) up = true;
            
            if (rowidx2 < 0) down = false;
            else if (std::abs(instance->position.y - db_->placement_rows[rowidx2].origin.y) < Cbest) down = true;
            
            if (!up && !down) break;
            
            // Try upward row
            if (up) {
                int subRowidx = findSubrowpos(*instance, db_->placement_rows[rowidx1]);
                if (subRowidx != -1) {
                    auto cost = placeRow(db_->placement_rows[rowidx1], *instance, 
                                       db_->placement_rows[rowidx1].subrows[subRowidx], false, true);
                    if (cost < Cbest) {
                        Cbest = cost;
                        bestRowIdx = rowidx1;
                        bestSubRowIdx = subRowidx;
                    }
                }
            }
            
            // Try downward row
            if (down) {
                int subRowidx = findSubrowpos(*instance, db_->placement_rows[rowidx2]);
                if (subRowidx != -1) {
                    auto cost = placeRow(db_->placement_rows[rowidx2], *instance,
                                       db_->placement_rows[rowidx2].subrows[subRowidx], false, true);
                    if (cost < Cbest) {
                        Cbest = cost;
                        bestRowIdx = rowidx2;
                        bestSubRowIdx = subRowidx;
                    }
                }
            }
        }
        if (bestRowIdx != -1 && bestSubRowIdx != -1) {
            double finalcost = placeRow(db_->placement_rows[bestRowIdx], *instance,
                                      db_->placement_rows[bestRowIdx].subrows[bestSubRowIdx], true, true);
            instance->placement_status = Instance::PLACED;
            processed_count++;
        } else {
            std::cout << "  Warning: Could not place instance " << instance->name << std::endl;
            // 如果無法找到合適位置，至少設置為原始位置
            instance->x_new = instance->position.x;
            instance->y_new = instance->position.y;
        }
    }
    
    std::cout << "Abacus completed. Processed " << processed_count << " instances." << std::endl;
}

void Legalizer::buildSubRows(std::vector<std::shared_ptr<Instance>>& blockage_instances) {
    const double eps = 1e-6;

    std::cout << "buildSubRows: Processing " << blockage_instances.size() << " blockage instances" << std::endl;

    // 排序，確保左到右
    std::sort(blockage_instances.begin(), blockage_instances.end(),
              [](const std::shared_ptr<Instance>& a, const std::shared_ptr<Instance>& b) {
                  return a->position.x < b->position.x;
              });

    for (const auto& blk : blockage_instances) {
        if (!blk->cell_template) {
            std::cout << blk->name << " no template" << std::endl;
            continue;
        }

        double MINx = blk->position.x;
        double MAXx = blk->position.x + blk->cell_template->width;
        double MINy = blk->position.y;
        double MAXy = blk->position.y + blk->cell_template->height;

        if(blk->name=="c_n34785")
        {
            std::cout<<"Hi-before"<<std::endl;
             std::cout<<MINx<<" "<<MAXx<<" "<<MINy<<" "<<MAXy<<std::endl;
        }


        int affected_rows = 0;
        for (auto& row : db_->placement_rows) 
        {   
            //if (row.origin.y!=MINy) continue;
            if (!(row.origin.y + row.height > blk->position.y && row.origin.y < blk->position.y +blk->get_height()))continue;
            affected_rows++;

            // site 對齊邊界，加入 eps 避免卡在中間
            double front = row.origin.x + 
                          std::floor((MINx - row.origin.x) / row.site_width + eps) * row.site_width;
            double back  = row.origin.x + 
                          std::ceil((MAXx - row.origin.x) / row.site_width - eps) * row.site_width;
            //double front = MINx;
            //double back = MAXx;


            auto& sr = row.subrows;
            for (auto slice = sr.begin(); slice != sr.end();) 
            {
                // X 向不重疊（允許相接）
                if (slice->x_max <= front|| back <= slice->x_min) {
                    ++slice;
                    continue;
                }

                // 1. 完全覆蓋整個 subrow
                if (front <= slice->x_min&& back >= slice->x_max) {
                    slice = sr.erase(slice);
                }
                // 2. 覆蓋 subrow 左端
                else if (front <= slice->x_min&& back < slice->x_max) {
                    slice->x_min = back;
                    slice->Usewidth = slice->x_max - slice->x_min;
                    ++slice;
                }
                // 3. 覆蓋 subrow 右端
                else if (front > slice->x_min&& back >= slice->x_max) {
                    slice->x_max = front;
                    slice->Usewidth = slice->x_max - slice->x_min;
                    ++slice;
                }
                // 4. 中間切成兩段
                else if (front > slice->x_min&& back < slice->x_max) {
                    SubRow left(slice->x_min, front);
                    SubRow right(back, slice->x_max);
                    slice = sr.erase(slice);
                    slice = sr.insert(slice, left);
                    slice = sr.insert(std::next(slice), right);
                    ++slice;
                }
            }
        }

    }

    for (const auto& rect : db_->placement_blockages) 
    {

        double MINx = rect.x1 ;
        double MAXx = rect.x2 ;
        double MINy = rect.y1 ;
        double MAXy = rect.y2 ;


        int affected_rows = 0;
        for (auto& row : db_->placement_rows) 
        {   
            //if (row.origin.y!=MINy) continue;
            if (!(row.origin.y + row.height > MINy && row.origin.y < MAXy))continue;
            affected_rows++;

            // site 對齊邊界，加入 eps 避免卡在中間
            double front = row.origin.x + 
                          std::floor((MINx - row.origin.x) / row.site_width + eps) * row.site_width;
            double back  = row.origin.x + 
                          std::ceil((MAXx - row.origin.x) / row.site_width - eps) * row.site_width;
            //double front = MINx;
            //double back = MAXx;


            auto& sr = row.subrows;
            for (auto slice = sr.begin(); slice != sr.end();) 
            {
                // X 向不重疊（允許相接）
                if (slice->x_max <= front|| back <= slice->x_min) {
                    ++slice;
                    continue;
                }

                // 1. 完全覆蓋整個 subrow
                if (front <= slice->x_min&& back >= slice->x_max) {
                    slice = sr.erase(slice);
                }
                // 2. 覆蓋 subrow 左端
                else if (front <= slice->x_min&& back < slice->x_max) {
                    slice->x_min = back;
                    slice->Usewidth = slice->x_max - slice->x_min;
                    ++slice;
                }
                // 3. 覆蓋 subrow 右端
                else if (front > slice->x_min&& back >= slice->x_max) {
                    slice->x_max = front;
                    slice->Usewidth = slice->x_max - slice->x_min;
                    ++slice;
                }
                // 4. 中間切成兩段
                else if (front > slice->x_min&& back < slice->x_max) {
                    SubRow left(slice->x_min, front);
                    SubRow right(back, slice->x_max);
                    slice = sr.erase(slice);
                    slice = sr.insert(slice, left);
                    slice = sr.insert(std::next(slice), right);
                    ++slice;
                }
            }
        }

    }
    
    // 輸出所有 rows 和 subrows 的信息到文件
    // std::ofstream ofs("rows_subrows_info.txt");
    // if (ofs.is_open()) {
    //     ofs << "=== Rows and SubRows Information ===" << std::endl;
    //     ofs << "Total Rows: " << db_->placement_rows.size() << std::endl;
    //     ofs << "Total Blockages: " << blockage_instances.size() << std::endl;
    //     ofs << std::endl;
        
    //     for (const auto& row : db_->placement_rows) {
    //         ofs << "Row: " << row.name << " (ID: " << row.id << ")" << std::endl;
    //         ofs << "  Origin: (" << row.origin.x << ", " << row.origin.y << ")" << std::endl;
    //         ofs << "  Size: " << row.num_x << " x " << row.num_y << std::endl;
    //         ofs << "  Step: " << row.step_x << " x " << row.step_y << std::endl;
    //         ofs << "  Site Width: " << row.site_width << ", Height: " << row.height << std::endl;
    //         ofs << "  SubRows: " << row.subrows.size() << std::endl;
            
    //         for (size_t i = 0; i < row.subrows.size(); ++i) {
    //             const auto& subrow = row.subrows[i];
    //             ofs << "    SubRow[" << i << "]: "
    //                 << "x_min=" << subrow.x_min 
    //                 << ", x_max=" << subrow.x_max
    //                 << ", width=" << (subrow.x_max - subrow.x_min)
    //                 << ", Usewidth=" << subrow.Usewidth << std::endl;
    //         }
    //         ofs << std::endl;
    //     }
        
    //     // 也輸出 blockage 信息
    //     ofs << "=== Blockage Instances ===" << std::endl;
    //     for (const auto& blk : blockage_instances) {
    //         if (blk->cell_template) {
    //             ofs << "Blockage: " << blk->name 
    //                 << " at (" << blk->position.x << ", " << blk->position.y << ")"
    //                 << " size: " << blk->cell_template->width << " x " << blk->cell_template->height
    //                 << " type: " << blk->cell_type << std::endl;
    //         }
    //     }

    //     ofs << "=== Blockage placement ===" << std::endl;
    //     for (const auto& rect : db_->placement_blockages)
    //     {
    //             ofs << "Blockage: at (" << rect.x1 << ", " << rect.x2 << " , "<<rect.y1<<" , "<<rect.y2 << " ) " << std::endl;
    //     }
        
    //     ofs.close();
    //     std::cout << "Row and SubRow information written to 'rows_subrows_info.txt'" << std::endl;
    // } else {
    //     std::cerr << "Error: Could not open 'rows_subrows_info.txt' for writing" << std::endl;
    // }
}

int Legalizer::findBestRow(const Instance& instance) {
    double best = std::numeric_limits<double>::infinity();
    int br = -1;
    
    for (int i = 0; i < static_cast<int>(db_->placement_rows.size()); ++i) {
        double dy = std::abs(instance.position.y - db_->placement_rows[i].origin.y);
        if (dy < best) {
            br = i;
            best = dy;
        }
    }
    return br;
}

int Legalizer::findSubrowpos(const Instance& instance, const PlacementRow& row) {
    if (!instance.cell_template) return -1;
    
    int subRow = -1;
    double minDisplacement = std::numeric_limits<double>::max();
    
    for (int idx = 0; idx < static_cast<int>(row.subrows.size()); ++idx) {
        if (instance.cell_template->width > row.subrows[idx].Usewidth) continue;
        
        double move = 0;
        if (instance.position.x < row.subrows[idx].x_min) {
            move = row.subrows[idx].x_min - instance.position.x;
        } else if (instance.position.x + instance.cell_template->width > row.subrows[idx].x_max) {
            move = instance.position.x + instance.cell_template->width - row.subrows[idx].x_max;
        }
        
        if (minDisplacement > move) {
            minDisplacement = move;
            subRow = idx;
        } else {
            break;
        }
    }
    return subRow;
}

void Legalizer::AddCell(Cluster& cluster, Instance& instance, double tempXpos, double placeCellwidth) {
    cluster.cellInCluster.push_back(&instance);
    cluster.weight += instance.weight;
    cluster.q += instance.weight * (tempXpos - cluster.width);
    cluster.width += placeCellwidth;
}

void Legalizer::AddCluster(Cluster& pred, Cluster& curr) {
    pred.cellInCluster.insert(pred.cellInCluster.end(), 
                             curr.cellInCluster.begin(), curr.cellInCluster.end());
    
    double oldWidth = pred.width;
    pred.weight += curr.weight;
    pred.q += curr.q - curr.weight * oldWidth;
    pred.width += curr.width;
}

void Legalizer::Collapse(Cluster& cluster, double xmin, double xmax, SubRow& sr, double sitew) {
    while (true) {
        cluster.x = cluster.q / cluster.weight;
        cluster.x = std::floor((cluster.x - sr.x_min) / sitew) * sitew + sr.x_min;
        
        if (cluster.x < xmin) cluster.x = xmin;
        if (cluster.x + cluster.width > xmax) cluster.x = xmax - cluster.width;
        
        Cluster* pred = cluster.leftCluster;
        if (pred && pred->x + pred->width > cluster.x) {
            AddCluster(*pred, cluster);
            cluster = *pred;
        } else {
            break;
        }
    }
    sr.lastCluster = &cluster;
}

double Legalizer::placeRow(const PlacementRow& row, Instance& instance, SubRow& sr, 
                          bool final, bool check) {
    if (!instance.cell_template) return std::numeric_limits<double>::max();
    
    double placeCellwidth = std::ceil(instance.cell_template->width / row.site_width) * row.site_width;
    
    if (final) {
        sr.Usewidth -= placeCellwidth;
        
        double tempXpos = instance.position.x;
        if (tempXpos <= sr.x_min) {
            tempXpos = sr.x_min;
        } else if (tempXpos + instance.cell_template->width >= sr.x_max) {
            tempXpos = sr.x_max - instance.cell_template->width;
            tempXpos = std::floor((tempXpos - sr.x_min) / row.site_width) * row.site_width + sr.x_min;
        } else {
            tempXpos = std::floor((tempXpos - sr.x_min) / row.site_width) * row.site_width + sr.x_min;
        }
        
        if (!sr.lastCluster || sr.lastCluster->x + sr.lastCluster->width <= tempXpos) {
            Cluster* prev = sr.lastCluster;
            Cluster* cur = new Cluster();
            cur->x = tempXpos;
            cur->width = 0.0;
            cur->weight = 0.0;
            cur->q = 0.0;
            cur->leftCluster = prev;
            sr.lastCluster = cur;
            
            instance.x_new = tempXpos;
            instance.y_new = row.origin.y;
            AddCell(*sr.lastCluster, instance, tempXpos, placeCellwidth);
        } else {
            AddCell(*sr.lastCluster, instance, tempXpos, placeCellwidth);
            Collapse(*sr.lastCluster, sr.x_min, sr.x_max, sr, row.site_width);
        }
    } else {
        // Temporary placement for cost calculation
        double tempXpos = instance.position.x;
        if (tempXpos <= sr.x_min) {
            tempXpos = sr.x_min;
        } else if (tempXpos + instance.cell_template->width >= sr.x_max) {
            tempXpos = sr.x_max - instance.cell_template->width;
            tempXpos = std::floor((tempXpos - sr.x_min) / row.site_width) * row.site_width + sr.x_min;
        } else {
            tempXpos = std::floor((tempXpos - sr.x_min) / row.site_width) * row.site_width + sr.x_min;
        }
        
        if (!sr.lastCluster || sr.lastCluster->x + sr.lastCluster->width <= tempXpos) {
            instance.x_new = tempXpos;
            instance.y_new = row.origin.y;
        } else {
            // Simulate cluster operations
            double TempWeight = sr.lastCluster->weight + instance.weight;
            double TempQ = sr.lastCluster->q + instance.weight * (tempXpos - sr.lastCluster->width);
            double TempWidth = sr.lastCluster->width + placeCellwidth;
            double Tempx = 0;
            
            std::vector<Cluster*> checkmaxdis;
            Cluster* curr = sr.lastCluster;
            
            while (true) {
                Tempx = TempQ / TempWeight;
                Tempx = sr.x_min + std::floor((Tempx - sr.x_min) / row.site_width) * row.site_width;
                
                if (Tempx < sr.x_min) Tempx = sr.x_min;
                if (Tempx + TempWidth > sr.x_max) Tempx = sr.x_max - TempWidth;
                
                checkmaxdis.emplace_back(curr);
                
                Cluster* pred = curr->leftCluster;
                if (pred && pred->x + pred->width > Tempx) {
                    TempQ += pred->q - TempWeight * pred->width;
                    TempWeight += pred->weight;
                    TempWidth += pred->width;
                    curr = pred;
                } else {
                    break;
                }
            }
            
            instance.x_new = Tempx + TempWidth - placeCellwidth;
            instance.y_new = row.origin.y;
            
            if (check) {
                for (auto* c : checkmaxdis) {
                    for (Instance* cp : c->cellInCluster) {
                        double displacement = std::sqrt((cp->position.x - Tempx) * (cp->position.x - Tempx) + 
                                                       (cp->position.y - row.origin.y) * (cp->position.y - row.origin.y));
                        if (displacement > max_disp_) {
                            return std::numeric_limits<double>::max();
                        }
                        Tempx += std::ceil(cp->cell_template->width / row.site_width) * row.site_width;
                    }
                }
            }
        }
    }
    
    double dis = std::sqrt((instance.position.x - instance.x_new) * (instance.position.x - instance.x_new) + 
                          (instance.position.y - row.origin.y) * (instance.position.y - row.origin.y));
    
    if (check && dis > max_disp_) {
        return std::numeric_limits<double>::max();
    }
    
    return dis;
}

void Legalizer::place() {
    std::cout << "Starting place() function..." << std::endl;
    
    for (auto& row : db_->placement_rows) {
        for (auto& sub : row.subrows) {
            Cluster* cluster = sub.lastCluster;
            while (cluster) {
                double x = sub.x_min + std::floor((cluster->x - sub.x_min) / row.site_width) * row.site_width;               
                for (Instance* instance : cluster->cellInCluster) 
                {
                    instance->x_new = x;
                    instance->y_new = row.origin.y;
                    x += std::ceil(instance->cell_template->width / row.site_width) * row.site_width;
                }
                cluster = cluster->leftCluster;
            }
        }
    }
    // 調試：檢查所有 flip-flop instances 的最終位置
    int placed_count = 0;
    for (const auto& pair : db_->instances) {
        if (pair.second->is_flip_flop()) {
            if (pair.second->x_new != 0.0 || pair.second->y_new != 0.0) {
                placed_count++;
            }
        }
    }
    std::cout << "Total placed flip-flops: " << placed_count << std::endl;
}

std::pair<double, double> Legalizer::calculate_displacement() const {
    double total_displacement = 0.0;
    double max_displacement = 0.0;
    
    for (const auto& pair : db_->instances) {
        const auto& instance = pair.second;
        double displacement = std::sqrt((instance->x_new - instance->position.x) * (instance->x_new - instance->position.x) + 
                                       (instance->y_new - instance->position.y) * (instance->y_new - instance->position.y));
        total_displacement += displacement;
        max_displacement = std::max(max_displacement, displacement);
    }
    
    return {total_displacement, max_displacement};
}

void Legalizer::writeOutput(const std::string& filename) const {
    auto displacement_result = calculate_displacement();
    double total_disp = displacement_result.first;
    double max_disp = displacement_result.second;
    
    std::ofstream ofs(filename);
    ofs << std::fixed << std::setprecision(0);
    if (!ofs.is_open()) {
        std::cerr << "Error: Cannot open file " << filename << " for writing." << std::endl;
        return;
    }
    
    ofs << "TotalDisplacement " << static_cast<long>(std::ceil(total_disp)) << std::endl;
    ofs << "MaxDisplacement " << static_cast<long>(std::ceil(max_disp)) << std::endl;
    ofs << "NumCells " << db_->instances.size() << std::endl;
    ofs << std::endl;
    
    // 檢查格式說明
    ofs << "Format: name original_x original_y new_x new_y width height [row_id site_offset status]" << std::endl;
    ofs << "Status: OK(aligned), ERROR(misaligned), NONFF-nocheck(non-flip-flop)" << std::endl;
    ofs << std::endl;
    
    int ok_count = 0;
    int error_count = 0;
    int nonff_count = 0;
    
    for (const auto& pair : db_->instances) {
        const auto& instance = pair.second;
        ofs << instance->name << " " 
            << (instance->position.x) << " " 
            << (instance->position.y) << " "
            << (instance->x_new) << " "
            << (instance->y_new) <<" "
            << instance->get_width() << " "
            << instance->get_height();
        
        // 如果不是 flip-flop，標記為 NONFF-nocheck
        if (!instance->is_flip_flop()) {
            ofs << " NONFF-nocheck" << std::endl;
            nonff_count++;
            continue;
        }
        
        // 對 flip-flop 進行對齊檢查
        bool aligned = true;
        std::string status = "";
        int matched_row_id = -1;
        double site_offset = 0.0;
        
        // 找到匹配的 row
        for (const auto& row : db_->placement_rows) {
            if (std::abs(instance->y_new - row.origin.y) < 0.001) { // 允許小誤差
                matched_row_id = row.id;
                
                // 檢查 x 座標是否對齊 site
                if (row.site_width > 0) {
                    double offset_from_row = instance->x_new - row.origin.x;
                    site_offset = offset_from_row / row.site_width;
                    
                    // 檢查是否為整數倍
                    double rounded_offset = std::round(site_offset);
                    
                    // 如果四捨五入後相等，認為是對齊的
                    if (std::abs(site_offset - rounded_offset) < 1e-9) {
                        aligned = true;
                        site_offset = rounded_offset;
                    } else {
                        aligned = false;
                    }
                } else {
                    aligned = false; // site_width 為 0 是錯誤的
                }
                break;
            }
        }
        
        if (matched_row_id == -1) {
            // 沒找到匹配的 row
            ofs << " -1 0 ERROR(no_matching_row)" << std::endl;
            error_count++;
        } else if (aligned) {
            ofs << " " << matched_row_id << " " << static_cast<int>(site_offset) << " OK" << std::endl;
            ok_count++;
        } else {
            ofs << " " << matched_row_id << " " << site_offset << " ERROR(misaligned)" << std::endl;
            error_count++;
        }
    }
    for (const auto& rect : db_->placement_blockages) 
    {
        ofs << "Blockage" << " " 
            << rect.x1 << " " 
            << rect.y1 << " "
            << rect.x1 << " "
            << rect.y1 <<" "
            << rect.width() << " "
            << rect.height()<< " "
            <<"NONFF-nocheck" << std::endl;
    }
    
    // 寫入統計信息
    ofs << std::endl;
    ofs << "=== Alignment Check Summary ===" << std::endl;
    ofs << "Flip-flops aligned correctly (OK): " << ok_count << std::endl;
    ofs << "Flip-flops with alignment errors (ERROR): " << error_count << std::endl;
    ofs << "Non-flip-flop instances (NONFF-nocheck): " << nonff_count << std::endl;
    ofs << "Total instances: " << db_->instances.size() << std::endl;
    
    if (error_count == 0) {
        ofs << "*** ALL FLIP-FLOPS PROPERLY ALIGNED! ***" << std::endl;
    } else {
        ofs << "*** " << error_count << " FLIP-FLOPS HAVE ALIGNMENT ISSUES ***" << std::endl;
    }
    
    ofs.close();
    
    std::cout << "Results written to " << filename << std::endl;
    std::cout << "TotalDisplacement " << static_cast<long>(std::ceil(total_disp)) << std::endl;
    std::cout << "MaxDisplacement " << static_cast<long>(std::ceil(max_disp)) << std::endl;
    std::cout << "Alignment Summary: " << ok_count << " OK, " << error_count << " ERROR, " << nonff_count << " NONFF" << std::endl;
}

double calculate_euclidean_distance(const Point& p1, const Point& p2) {
    return std::sqrt((p1.x - p2.x) * (p1.x - p2.x) + (p1.y - p2.y) * (p1.y - p2.y));
}