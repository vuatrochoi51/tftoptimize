#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <unordered_map>
#include <vector>
#include <string>
#include <iostream>
#include <cstdint>
#include <algorithm>
#include <bit>
#include <utility>

namespace py = pybind11;

// ========================================================================
// 1. CẤU TRÚC GIAO TIẾP PYTHON <-> C++
// ========================================================================
struct TraitData {
    std::string name;
    std::vector<int> breakpoints;
    std::vector<int> scores;
    std::vector<std::string> champion_ids;
};

struct ChampionConfig {
    std::string id;
    std::string role; 
    std::string modifier;
    int slots_left;   
};

struct ItemAssignment {
    std::string champ_id;
    std::vector<std::string> item_names;
    int power_score;
};

struct OptimizationResult {
    std::vector<std::string> recommended_team;
    std::vector<ItemAssignment> item_assignments;
    int max_synergy_score;
    int max_item_power;
};

// ========================================================================
// 2. DỮ LIỆU GAME (META DATA) & BỘ NHỚ RAM
// ========================================================================
const double ROLL_ODDS[11][6] = {
    {0.0, 0.0,  0.0,  0.0,  0.0,  0.0},  
    {0.0, 1.0,  0.0,  0.0,  0.0,  0.0},  
    {0.0, 1.0,  0.0,  0.0,  0.0,  0.0},  
    {0.0, 0.75, 0.25, 0.0,  0.0,  0.0},  
    {0.0, 0.55, 0.30, 0.15, 0.0,  0.0},  
    {0.0, 0.45, 0.33, 0.20, 0.02, 0.0},  
    {0.0, 0.30, 0.40, 0.25, 0.05, 0.0},  
    {0.0, 0.19, 0.35, 0.35, 0.10, 0.01}, 
    {0.0, 0.18, 0.25, 0.36, 0.18, 0.03}, 
    {0.0, 0.10, 0.20, 0.25, 0.35, 0.10}, 
    {0.0, 0.05, 0.10, 0.20, 0.40, 0.25}  
};

const int BASE_POWER[] = {0, 10, 20, 35, 60, 100}; 
const int MAX_TRAITS = 40;

uint64_t trait_champion_masks[MAX_TRAITS]; 
std::vector<int> trait_breakpoints[MAX_TRAITS]; 
std::vector<int> trait_scores[MAX_TRAITS]; 
int num_active_traits = 0;

static std::unordered_map<std::string, int> champion_bit_index;
std::vector<int> champion_costs;
std::vector<bool> is_tank_champion; 
static bool is_initialized = false;

const std::string ITEM_NAMES[] = {
    "Unknown", "Kiếm BF", "Cung Gỗ", "Gậy Quá Khổ", 
    "Nước Mắt", "Giáp Lưới", "Áo Choàng Bạc", 
    "Đai Khổng Lồ", "Găng Đấu Tập", "Xẻng Vàng"
};

// ========================================================================
// 3. KHỞI TẠO ĐỘNG CƠ C++
// ========================================================================
void init_engine(const std::vector<std::string>& all_champion_ids, 
                 const std::vector<int>& all_champion_costs, 
                 const std::vector<bool>& all_champion_tanks,
                 const std::vector<TraitData>& traits_data) {
    champion_bit_index.clear();
    champion_costs = all_champion_costs; 
    is_tank_champion = all_champion_tanks;
    
    num_active_traits = static_cast<int>(std::min(traits_data.size(), static_cast<size_t>(MAX_TRAITS)));
    for(int i = 0; i < MAX_TRAITS; ++i) { trait_champion_masks[i] = 0; trait_breakpoints[i].clear(); trait_scores[i].clear(); }
    for (size_t i = 0; i < all_champion_ids.size(); ++i) { champion_bit_index[all_champion_ids[i]] = static_cast<int>(i); }
    
    for (int i = 0; i < num_active_traits; ++i) {
        trait_breakpoints[i] = traits_data[i].breakpoints; trait_scores[i] = traits_data[i].scores;
        uint64_t mask = 0;
        for (const std::string& champ_id : traits_data[i].champion_ids) {
            auto it = champion_bit_index.find(champ_id);
            if (it != champion_bit_index.end()) mask |= (1ULL << it->second); 
        }
        trait_champion_masks[i] = mask; 
    }
    is_initialized = true;
}

// ========================================================================
// 4. LÕI TỐI ƯU ĐỘI HÌNH & HÀM ĐÁNH GIÁ (HEURISTIC)
// ========================================================================
double calculate_board_value(uint64_t team_mask, int level) {
    double total_value = 0.0;
    if (level < 1 || level > 10) return total_value;

    for (int i = 0; i < static_cast<int>(champion_costs.size()); ++i) {
        if (team_mask & (1ULL << i)) {
            int cost = champion_costs[i];
            if (cost >= 1 && cost <= 5) {
                total_value += BASE_POWER[cost] * ROLL_ODDS[level][cost];
            }
        }
    }
    return total_value;
}

int calculate_synergy_score(uint64_t team_mask, int carry_bit) {
    double total_score = 0.0;
    
    for (int i = 0; i < num_active_traits; ++i) {
        int count = std::popcount(team_mask & trait_champion_masks[i]);
        if (count == 0) continue;
        
        int current_trait_score = 0;
        int current_tier_idx = -1;
        int next_tier_req = 0;
        int next_tier_score = 0;
        
        for (size_t j = 0; j < trait_breakpoints[i].size(); ++j) {
            if (count >= trait_breakpoints[i][j]) {
                current_trait_score = trait_scores[i][j];
                current_tier_idx = static_cast<int>(j);
            } else {
                next_tier_req = trait_breakpoints[i][j];
                next_tier_score = trait_scores[i][j];
                break;
            }
        }
        
        double final_trait_score = current_trait_score;
        
        if (next_tier_req > 0 && count == next_tier_req - 1) {
            final_trait_score += (next_tier_score - current_trait_score) * 0.2;
        }
        
        if (final_trait_score > 0 && carry_bit != -1 && (trait_champion_masks[i] & (1ULL << carry_bit))) {
            int total_tiers = static_cast<int>(trait_breakpoints[i].size());
            double multiplier = 1.0;
            
            if (current_tier_idx == total_tiers - 1) multiplier = 3.0; 
            else if (current_tier_idx == 1) multiplier = 1.5; 
            else if (current_tier_idx == 0) multiplier = 1.2; 
            else if (current_tier_idx > 1) multiplier = 2.0; 
            else multiplier = 1.2; 
            
            final_trait_score *= multiplier;
        }
        
        total_score += final_trait_score;
    }
    return static_cast<int>(total_score);
}

int calculate_max_potential(uint64_t team_mask, int current_score, int empty_slots, int carry_bit) {
    if (empty_slots == 0) return current_score;
    
    double max_potential = 0.0;
    
    for (int i = 0; i < num_active_traits; ++i) {
        int count = std::popcount(team_mask & trait_champion_masks[i]);
        if (count == 0) continue;
        
        int opt_count = count + empty_slots; 
        int opt_score = 0;
        int opt_tier_idx = -1;
        int next_opt_score = 0;
        int next_opt_req = 0;
        
        for (size_t j = 0; j < trait_breakpoints[i].size(); ++j) {
            if (opt_count >= trait_breakpoints[i][j]) {
                opt_score = trait_scores[i][j];
                opt_tier_idx = static_cast<int>(j);
            } else {
                next_opt_req = trait_breakpoints[i][j];
                next_opt_score = trait_scores[i][j];
                break;
            }
        }
        
        double final_opt_score = opt_score;
        
        if (next_opt_req > 0 && opt_count == next_opt_req - 1) {
            final_opt_score += (next_opt_score - opt_score) * 0.2;
        }
        
        if (final_opt_score > 0 && carry_bit != -1 && (trait_champion_masks[i] & (1ULL << carry_bit))) {
            int total_tiers = static_cast<int>(trait_breakpoints[i].size());
            double multiplier = 1.0;
            
            if (opt_tier_idx == total_tiers - 1) multiplier = 3.0;
            else if (opt_tier_idx == 1) multiplier = 1.5;
            else if (opt_tier_idx == 0) multiplier = 1.2;
            else if (opt_tier_idx > 1) multiplier = 2.0;
            else multiplier = 1.2; 
            
            final_opt_score *= multiplier;
        }
        
        max_potential += final_opt_score;
    }
    return static_cast<int>(max_potential);
}

struct BnBContext { 
    int best_score = -1; 
    uint64_t best_team = 0; 
    int current_level = 8;
    int main_carry_bit = -1;
    std::vector<std::pair<int, double>> candidate_with_power; 
};

void solve_bnb(uint64_t current_team, int empty_slots, int start_idx, int current_tanks, BnBContext& ctx) {
    int req_tanks = (ctx.current_level <= 5) ? 1 : 2; 
    
    int synergy_score = calculate_synergy_score(current_team, ctx.main_carry_bit);
    int board_power = static_cast<int>(calculate_board_value(current_team, ctx.current_level));
    int current_total = synergy_score + board_power;

    if (empty_slots == 0) {
        if (current_tanks < req_tanks) {
            current_total -= 500 * (req_tanks - current_tanks); 
        }
        if (current_total > ctx.best_score) { 
            ctx.best_score = current_total; 
            ctx.best_team = current_team; 
        }
        return;
    }

    int max_potential_synergy = calculate_max_potential(current_team, synergy_score, empty_slots, ctx.main_carry_bit);
    double max_potential_power = calculate_board_value(current_team, ctx.current_level);
    
    int max_possible_tanks = current_tanks + empty_slots;
    if (max_possible_tanks < req_tanks) {
        max_potential_synergy -= 500 * (req_tanks - max_possible_tanks);
    }

    int slots_counted = 0;
    for (int i = start_idx; i < static_cast<int>(ctx.candidate_with_power.size()); ++i) {
        if (slots_counted >= empty_slots) break;
        int idx = ctx.candidate_with_power[i].first;
        if ((current_team & (1ULL << idx)) == 0) {
            max_potential_power += ctx.candidate_with_power[i].second;
            slots_counted++;
        }
    }

    if ((max_potential_synergy + static_cast<int>(max_potential_power)) <= ctx.best_score) return; 

    for (int i = start_idx; i < static_cast<int>(ctx.candidate_with_power.size()); ++i) {
        int champ_idx = ctx.candidate_with_power[i].first;
        if ((current_team & (1ULL << champ_idx)) == 0) {
            int next_tanks = current_tanks + (is_tank_champion[champ_idx] ? 1 : 0);
            solve_bnb(current_team | (1ULL << champ_idx), empty_slots - 1, i + 1, next_tanks, ctx);
        }
    }
}

// ========================================================================
// 5. LÕI TỐI ƯU TRANG BỊ
// ========================================================================
int get_item_score(const std::string& role, const std::string& modifier, int item1, int item2) {
    if (item1 > item2) std::swap(item1, item2);
    
    // ĐÃ KHÔI PHỤC: Thêm lại is_ap_item
    bool is_tank_item = (item1 >= 5 && item1 <= 7) || (item2 >= 5 && item2 <= 7);
    bool is_ap_item   = (item1 >= 3 && item1 <= 4) || (item2 >= 3 && item2 <= 4); 
    bool has_sword    = (item1 == 1 || item2 == 1);
    bool has_bow      = (item1 == 2 || item2 == 2);
    bool has_rod      = (item1 == 3 || item2 == 3);
    bool has_tear     = (item1 == 4 || item2 == 4);
    
    if (role == "tank") {
        if (has_sword || has_bow || has_rod || has_tear) return -9999;
        if (item1 == 5 && item2 == 5) return 100;
        if (item1 == 7 && item2 == 7) return 100;
        if ((item1 == 5 && item2 == 7) || (item1 == 6 && item2 == 7)) return 90;
        return 10;
    }
    
    if (role == "ap_carry") {
        if (is_tank_item || has_bow) return -9999; 
        
        int score_rabadon = 90;
        int score_jeweled = 90;
        int score_archangel = 90;
        int score_shojin = 80; 
        
        if (modifier == "has_crit") {
            score_jeweled = 10; score_rabadon = 100;
        } else if (modifier == "high_ap") {
            score_rabadon = 40; score_jeweled = 100;
        } else if (modifier == "time_scale") {
            score_archangel = 100; score_rabadon = 60; score_shojin = 100;
        } else {
            score_archangel = 100;
        }

        if (item1 == 3 && item2 == 3) return score_rabadon;
        if (item1 == 3 && item2 == 8) return score_jeweled;
        if (item1 == 3 && item2 == 4) return score_archangel;
        if (item1 == 4 && item2 == 4) return 80;           
        if (item1 == 1 && item2 == 4) return score_shojin; 
        if (item1 == 1 && item2 == 3) return 70;           
        return 10;
    }
    
    if (role == "ad_carry") {
        bool is_pure_tank = (item1 >= 5 && item1 <= 7 && item2 >= 5 && item2 <= 7);
        bool is_pure_ap   = (item1 >= 3 && item1 <= 4 && item2 >= 3 && item2 <= 4);
        
        if (modifier != "balanced_mix") {
            if (is_tank_item || is_ap_item) return -9999; 
        } else {
            if (is_pure_tank || is_pure_ap) return -9999;
        }
        
        int score_gs = 90;     
        int score_db = 90;     
        int score_rb = 90;     
        int score_lw = 80;     
        int score_ie = 80;     
        int score_shojin = 70; 
        
        int score_bt = 10;     
        int score_eon = 10;    
        int score_hoj = 10;    
        int score_qss = 10;    
        int score_grb = 10;    
        
        if (modifier == "high_as") { 
            score_db = 100; score_ie = 100; score_rb = 40; 
        } else if (modifier == "need_arp") { 
            score_lw = 100; score_gs = 95;
        } else if (modifier == "physical_caster") { 
            score_ie = 100; score_shojin = 100; score_rb = 10; 
        } else if (modifier == "balanced_mix") {
            score_ie = 100; score_db = 95; score_gs = 90;
            score_grb = 100; score_rb = 95; 
            score_bt = 100; score_hoj = 95; score_eon = 95; score_qss = 90;
        } else {
            score_gs = 100; score_rb = 100; 
        }

        if (item1 == 1 && item2 == 6) return score_bt;
        if (item1 == 1 && item2 == 5) return score_eon;
        if (item1 == 4 && item2 == 8) return score_hoj;
        if (item1 == 6 && item2 == 8) return score_qss;
        if (item1 == 2 && item2 == 3) return score_grb;

        if (item1 == 1 && item2 == 2) return score_gs;
        if (item1 == 1 && item2 == 1) return score_db;
        if (item1 == 2 && item2 == 2) return score_rb;
        if (item1 == 2 && item2 == 8) return score_lw;
        if (item1 == 1 && item2 == 8) return score_ie;
        if (item1 == 1 && item2 == 4) return score_shojin;
        
        return 10;
    }
    
    return 0; 
}

int solve_champ_dp(int mask, int pairs_left, const std::vector<int>& flat_items, const std::string& role, const std::string& modifier, 
                   std::vector<std::vector<int>>& memo, std::vector<std::vector<std::pair<int, int>>>& best_choice) {
    if (pairs_left == 0 || std::popcount(static_cast<unsigned int>(mask)) < 2) return 0;
    if (memo[mask][pairs_left] != -1) return memo[mask][pairs_left];

    int max_score = 0; 
    std::pair<int, int> best_pair = {-1, -1}; 

    int first_bit = -1;
    for (int i = 0; i < static_cast<int>(flat_items.size()); ++i) {
        if (mask & (1 << i)) { first_bit = i; break; }
    }
    if (first_bit == -1) return 0;

    int skip_mask = mask ^ (1 << first_bit);
    int score_skip = solve_champ_dp(skip_mask, pairs_left, flat_items, role, modifier, memo, best_choice);
    if (score_skip >= max_score) {
        max_score = score_skip;
        best_pair = {-1, -1}; 
    }

    for (int j = first_bit + 1; j < static_cast<int>(flat_items.size()); ++j) {
        if (mask & (1 << j)) {
            int new_mask = mask ^ (1 << first_bit) ^ (1 << j); 
            int score_pair = get_item_score(role, modifier, flat_items[first_bit], flat_items[j]) 
                           + solve_champ_dp(new_mask, pairs_left - 1, flat_items, role, modifier, memo, best_choice);
            if (score_pair > max_score) {
                max_score = score_pair;
                best_pair = {first_bit, j};
            }
        }
    }

    best_choice[mask][pairs_left] = best_pair;
    return memo[mask][pairs_left] = max_score;
}

// ========================================================================
// 6. WRAPPER GIAO TIẾP VỚI PYTHON
// ========================================================================
OptimizationResult run_optimization(int level, std::vector<std::string> team_ids, 
                                    std::vector<int> inventory, std::vector<ChampionConfig> board_configs) {
    OptimizationResult result;
    if (!is_initialized) throw std::runtime_error("C++ Engine chua duoc khoi tao!");

    uint64_t base_team_mask = 0;
    for (const auto& id : team_ids) {
        auto it = champion_bit_index.find(id);
        if (it != champion_bit_index.end()) base_team_mask |= (1ULL << it->second);
    }
    int current_team_size = std::popcount(base_team_mask);

    int carry_bit_pos = -1;
    // BẢN VÁ LỖI TÌM CARRY: Đã mở rộng nhận diện thêm ap_carry và ad_carry
    for (const auto& conf : board_configs) {
        if (conf.role == "carry" || conf.role == "ap_carry" || conf.role == "ad_carry") {
            auto it = champion_bit_index.find(conf.id);
            if (it != champion_bit_index.end()) {
                carry_bit_pos = it->second;
                break;
            }
        }
    }

    BnBContext ctx; 
    ctx.best_score = -1; 
    ctx.best_team = base_team_mask;
    ctx.current_level = level;
    ctx.main_carry_bit = carry_bit_pos;

    for (int c = 0; c < static_cast<int>(champion_bit_index.size()); ++c) {
        if (base_team_mask & (1ULL << c)) continue; 
        int cost = champion_costs[c];
        double p_roll = (cost >= 1 && cost <= 5 && level >= 1 && level <= 10) ? ROLL_ODDS[level][cost] : 0.0;
        if (p_roll <= 0.0) continue; 
        double power_val = BASE_POWER[cost] * p_roll;
        ctx.candidate_with_power.push_back({c, power_val});
    }

    std::sort(ctx.candidate_with_power.begin(), ctx.candidate_with_power.end(), 
              [](const std::pair<int, double>& a, const std::pair<int, double>& b) {
                  return a.second > b.second;
              });

    int base_tanks = 0;
    for (int i = 0; i < static_cast<int>(champion_costs.size()); ++i) {
        if ((base_team_mask & (1ULL << i)) && is_tank_champion[i]) base_tanks++;
    }

    int empty_slots = level - current_team_size;
    if (empty_slots > 0) {
        solve_bnb(base_team_mask, empty_slots, 0, base_tanks, ctx);
    } else {
        int req_tanks = (level <= 5) ? 1 : 2;
        int penalty = (base_tanks < req_tanks) ? 500 * (req_tanks - base_tanks) : 0;
        ctx.best_score = calculate_synergy_score(base_team_mask, carry_bit_pos) + 
                         static_cast<int>(calculate_board_value(base_team_mask, level)) - penalty;
    }

    result.max_synergy_score = ctx.best_score;
    for (const auto& pair : champion_bit_index) {
        int bit_pos = pair.second;
        if ((ctx.best_team & (1ULL << bit_pos)) && !(base_team_mask & (1ULL << bit_pos))) {
            result.recommended_team.push_back(pair.first);
        }
    }

    std::vector<int> flat_items;
    for (size_t i = 0; i < inventory.size(); ++i) {
        for (int count = 0; count < inventory[i]; ++count) flat_items.push_back(static_cast<int>(i) + 1); 
    }

    int n_items = static_cast<int>(flat_items.size());
    result.max_item_power = 0;

    if (n_items >= 2) {
        int global_mask = (1 << n_items) - 1; 
        for (const auto& config : board_configs) {
            
            // BẢN VÁ LỖI GHÉP ĐỒ: Từ chối phân bổ đồ cho các tướng bị đánh dấu là "utility"
            if (config.role == "utility" || config.role.empty()) continue;

            if (config.slots_left <= 0 || std::popcount(static_cast<unsigned int>(global_mask)) < 2) continue;

            std::vector<std::vector<int>> memo(1 << n_items, std::vector<int>(config.slots_left + 1, -1));
            std::vector<std::vector<std::pair<int, int>>> best_choice(1 << n_items, std::vector<std::pair<int, int>>(config.slots_left + 1, {-1, -1}));
            
            int score = solve_champ_dp(global_mask, config.slots_left, flat_items, config.role, config.modifier, memo, best_choice);
            
            int temp_mask = global_mask;
            int p_left = config.slots_left;
            ItemAssignment assignment;
            assignment.champ_id = config.id;
            assignment.power_score = score;

            while (p_left > 0 && std::popcount(static_cast<unsigned int>(temp_mask)) >= 2) {
                int first_bit = -1;
                for (int i = 0; i < n_items; ++i) {
                    if (temp_mask & (1 << i)) { first_bit = i; break; }
                }
                if (first_bit == -1) break;

                auto choice = best_choice[temp_mask][p_left];
                if (choice.first == -1 && choice.second == -1) {
                    temp_mask ^= (1 << first_bit); 
                } else {
                    int i = choice.first;
                    int j = choice.second;
                    assignment.item_names.push_back(ITEM_NAMES[flat_items[i]] + " + " + ITEM_NAMES[flat_items[j]]);
                    temp_mask ^= (1 << i) ^ (1 << j);
                    global_mask ^= (1 << i) ^ (1 << j); 
                    p_left--;
                }
            }
            if (!assignment.item_names.empty()) {
                result.item_assignments.push_back(assignment);
                result.max_item_power += score;
            }
        }
    }
    return result;
}

PYBIND11_MODULE(tft_core_engine, m) {
    py::class_<TraitData>(m, "TraitData")
        .def(py::init<>())
        .def_readwrite("name", &TraitData::name)
        .def_readwrite("breakpoints", &TraitData::breakpoints)
        .def_readwrite("scores", &TraitData::scores)
        .def_readwrite("champion_ids", &TraitData::champion_ids);

    py::class_<ChampionConfig>(m, "ChampionConfig")
        .def(py::init<>())
        .def_readwrite("id", &ChampionConfig::id)
        .def_readwrite("role", &ChampionConfig::role)
        .def_readwrite("modifier", &ChampionConfig::modifier) 
        .def_readwrite("slots_left", &ChampionConfig::slots_left);

    py::class_<ItemAssignment>(m, "ItemAssignment")
        .def_readonly("champ_id", &ItemAssignment::champ_id)
        .def_readonly("item_names", &ItemAssignment::item_names)
        .def_readonly("power_score", &ItemAssignment::power_score);

    py::class_<OptimizationResult>(m, "OptimizationResult")
        .def_readonly("recommended_team", &OptimizationResult::recommended_team)
        .def_readonly("item_assignments", &OptimizationResult::item_assignments) 
        .def_readonly("max_synergy_score", &OptimizationResult::max_synergy_score)
        .def_readonly("max_item_power", &OptimizationResult::max_item_power);

    m.def("init_engine", &init_engine, py::arg("all_champion_ids"), py::arg("all_champion_costs"), py::arg("all_champion_tanks"), py::arg("traits_data"));
    m.def("run_optimization", &run_optimization, py::arg("level"), py::arg("team_ids"), py::arg("inventory"), py::arg("board_configs"), py::call_guard<py::gil_scoped_release>());
}