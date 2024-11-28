#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <utility>
#include <map>
#include <algorithm>
#include <random>
#include <climits>
#include <cstdlib>
#include <ctime>
#include <nlohmann/json.hpp>
#include <aws/lambda-runtime/runtime.h>

using namespace aws::lambda_runtime;

struct ResultConfig {
    std::vector<std::pair<int, double>> lucky_configs;
    std::vector<std::pair<int, double>> lucky_configs_RTL;
    double straight_config;
    double rumble_config;
};

struct Request {
    int coupon_generate_length;
    int coupon_generate_max_value;
    int coupon_generate_min_value;
    bool is_paired;
};

int couponBitMask(std::string coupon) // Bitmasking Paired Coupon
{   
    int coupon_mask = 0;

    for(int j=0; j<coupon.length(); j+=2) {
        int hold = stoi(coupon.substr(j, 2));
        coupon_mask |= (1<<hold);
    }

    return coupon_mask;
}

double calculatePrizeForPaired(std::vector<int>& coupon_map, std::vector<std::pair<int, double>>& config, int lucky_coupon)
{
    double total = 0;

    for(int i=0; i<coupon_map.size(); ++i) {
        int c = coupon_map[i]&lucky_coupon;
        int match = 0;
        while(c) {
            match += c&1;
            c>>=1;
        }

        for(int j=0; j<config.size(); ++j) {
            if(match >= config[j].first) {
                total += config[j].second;
                break;
            }
        }
    }

    return total;
}

double calculatePrizeForNonPaired(Request request, ResultConfig& configs, std::map<std::string, int>& straight_coupon_map, std::map<std::string, int>& ramble_coupon_map, std::vector<std::string>& lucky_coupons, std::string g_coupon)
{
    double total = 0;

    total += straight_coupon_map[g_coupon] * configs.straight_config;

    for(int i=0; i<lucky_coupons.size(); ++i) {
        int match = 0;
        int RTL_match = 0;
        double LTR_prize = 0;
        double RTL_prize = 0;

        for(int j=0; j<g_coupon.length(); ++j) {
            if(lucky_coupons[i][j] == g_coupon[j]) match++;
            else break;
        }

        for(int j=g_coupon.length()-1; j>=0; --j) {
            if(lucky_coupons[i][j] == g_coupon[j]) RTL_match++;
            else break;
        }

        for(int j=0; j<configs.lucky_configs.size(); ++j) {
            if(match >= configs.lucky_configs[j].first) {
                LTR_prize = configs.lucky_configs[j].second;
                break;
            }
        }

        for(int j=0; j<configs.lucky_configs_RTL.size(); ++j) {
            if(RTL_match >= configs.lucky_configs_RTL[j].first) {
                RTL_prize = configs.lucky_configs_RTL[j].second;
                break;
            }
        }

        total += std::max(LTR_prize, RTL_prize);
    }

    sort(g_coupon.begin(), g_coupon.end());
    total += ramble_coupon_map[g_coupon] * configs.rumble_config;

    return total;    
}

std::pair<std::string, double> generateCombinations(std::vector<std::string>& set, std::vector<int>& coupon_map, std::vector<std::pair<int, double>>& config, std::string g_coupon, int start, int k, std::vector<std::pair<double, std::string>>& stored_coupon) {
    if(g_coupon.length() == k) {
        int coupon_mask = couponBitMask(g_coupon);
        double prize = calculatePrizeForPaired(coupon_map, config, coupon_mask);
        stored_coupon.push_back(make_pair(prize, g_coupon));
        return {g_coupon, prize};
    }

    std::string dummy_coupon = "000000000000";

    std::pair<std::string, double> lucky_coupon = make_pair(dummy_coupon, 10000000.0); //Initial dummy coupon and prize

    for(int i = start; i < set.size(); ++i) {
        if(i==start) {
            lucky_coupon = generateCombinations(set, coupon_map, config, g_coupon + set[i], i + 1, k, stored_coupon);
        }
        else {
            std::pair<std::string, double> ps = generateCombinations(set, coupon_map, config, g_coupon + set[i], i + 1, k, stored_coupon);
            if(ps.second < lucky_coupon.second) lucky_coupon = ps;
        }
    }

    return lucky_coupon;
}

std::map<std::pair<int, char>, int> luckyCouponMapping(std::vector<std::string>& lucky_coupons)
{
    std::map<std::pair<int, char>, int> position_number_map;

    for(int i=0; i<lucky_coupons.size(); ++i) {
        for(int j=0; j<lucky_coupons[i].length(); ++j) {
            position_number_map[{j, lucky_coupons[i][j]}]++;
        }
    }

    return position_number_map;
}

std::map<std::string, int> straightAndRambleMapping(std::vector<std::string>& coupons)
{
    std::map<std::string, int> coupon_map;

    for(int i=0; i<coupons.size(); ++i) {
        coupon_map[coupons[i]]++;
    }

    return coupon_map;
}

std::string generateCouponNumber(Request request, std::map<std::pair<int, char>, int>& coupon_map, std::string coupon, bool is_random)
{
    if(is_random) {
        for(int i=0; i<request.coupon_generate_length; ++i) {
            int rand_val = rand() % (request.coupon_generate_max_value+1); // Generate Random Value For Random Position
            coupon[i] = '0'+rand_val;
        }
    }
    else {
        for(int i=0; i<request.coupon_generate_length; ++i) {
            int min_char = coupon_map[{i, '0'+request.coupon_generate_min_value}];
        
            for(int j=request.coupon_generate_min_value; j<=request.coupon_generate_max_value; ++j) {
                if(coupon_map[{i, '0'+j}] < min_char) {
                    min_char = coupon_map[{i, '0'+j}];
                    coupon[i] = '0'+j;
                }
            }
        }
    }

    return coupon;
}

std::pair<std::string, double> pairedCoupon(std::vector<std::pair<int, double>>& config, int coupon_length, int coupon_generate_max_value, const nlohmann::json& input, std::vector<std::pair<double, std::string>>& stored_coupon)
{
    // Coupon Input
    int coupon_number = input["couponSize"];
    std::vector<int> coupon_map;

    for(int i=0; i<coupon_number; ++i) {
        std::string coupon = input["coupons"][i]["couponNumber"];
        std::string coupon_type = input["coupons"][i]["type"];
        int quantity = input["coupons"][i]["quantity"];

        while(quantity--) coupon_map.push_back(couponBitMask(coupon));
    }

    // Create a set of available digits
    std::vector<std::string> set;
    for(int i=1; i<=coupon_generate_max_value; ++i) {
        std::string s="";
        if(i<10) s+='0';
        s += std::to_string(i);
        set.push_back(s);
    }

    std::pair<std::string, double> lucky_coupon = generateCombinations(set, coupon_map, config, "", 0, coupon_length, stored_coupon);

    // Sort the lucky coupon
    std::vector<std::string> sortedCoupon;
    for(int i=0; i<coupon_length; i+=2) {
        sortedCoupon.push_back(lucky_coupon.first.substr(i, 2));
    }
    sort(sortedCoupon.begin(), sortedCoupon.end());
    lucky_coupon.first = "";
    for(int i=0; i<sortedCoupon.size(); ++i) {
        lucky_coupon.first += sortedCoupon[i];
    }

    return lucky_coupon;
}

std::pair<std::string, double> nonPairedCoupon(Request& request, ResultConfig& configs, const nlohmann::json& input, std::vector<std::pair<double, std::string>>& stored_coupon)
{
    int coupon_size = input["couponSize"];
    std::vector<std::string> straight_coupons;
    std::vector<std::string> lucky_coupons;
    std::vector<std::string> ramble_coupons;

    double total_selling_price = coupon_size * 0.5;

    for(int i=0; i<coupon_size; ++i) {
        std::string coupon = input["coupons"][i]["couponNumber"];
        std::string coupon_type = input["coupons"][i]["type"];
        int quantity = input["coupons"][i]["quantity"];

        if(coupon_type=="Straight") {
            while(quantity--) straight_coupons.push_back(coupon);
        }
        else if(coupon_type=="Lucky") {
            while(quantity--) lucky_coupons.push_back(coupon);
        }
        else {
            sort(coupon.begin(), coupon.end());
            while(quantity--) ramble_coupons.push_back(coupon);
        }
    }

    std::map<std::pair<int, char>, int> lucky_coupon_map = luckyCouponMapping(lucky_coupons);
    std::map<std::string, int> straight_coupon_map = straightAndRambleMapping(straight_coupons);
    std::map<std::string, int> ramble_coupon_map = straightAndRambleMapping(ramble_coupons);
    std::map<std::string, int> stored_coupon_map;

    std::string initial_coupon = "";
    for(int i=0; i<request.coupon_generate_length; ++i) initial_coupon += '0';

    std::string generated_coupon_number = generateCouponNumber(request, lucky_coupon_map, initial_coupon, false);

    int threshold = 100000;
    double prize_amount;
    double min_prize = 1000000000.0;
    std::string min_prized_coupon;

    while(true) {
        prize_amount = calculatePrizeForNonPaired(request, configs, straight_coupon_map, ramble_coupon_map, lucky_coupons, generated_coupon_number);
        if(!stored_coupon_map[generated_coupon_number]) {
            stored_coupon.push_back(make_pair(prize_amount, generated_coupon_number));
            stored_coupon_map[generated_coupon_number]++;
        }
        if(prize_amount < min_prize) {
            min_prize = prize_amount;
            min_prized_coupon = generated_coupon_number;
        }
        
        if(!threshold) break;

        generated_coupon_number = generateCouponNumber(request, lucky_coupon_map, initial_coupon, true);
        initial_coupon = generated_coupon_number;
        threshold--;
    }

    return {min_prized_coupon, min_prize};
}

invocation_response my_handler(invocation_request const& request)
{
    try
    {
        std::string payload = request.payload;
        nlohmann::json input = nlohmann::json::parse(payload);

        Request requests;

        // Result Config Input
        int result_config_size = input["configSize"];

        ResultConfig configs;
        std::vector<std::pair<int, double>> lucky_configs;
        std::vector<std::pair<int, double>> lucky_configs_RTL;
        std::vector<std::pair<double, std::string>> stored_coupon;

        for(int i=0; i<result_config_size; ++i) {
            int lucky_match = input["configs"][i]["luckyMatch"];
            double prize = input["configs"][i]["prize"];
            std::string result_type = input["configs"][i]["resultType"];
            bool left_to_right = input["configs"][i]["leftToRight"];

            if(result_type.find("Lucky") != std::string::npos) {
                if(left_to_right) lucky_configs.push_back(std::make_pair(lucky_match, prize));
                else lucky_configs_RTL.push_back(std::make_pair(lucky_match, prize));
            }
            else if(result_type=="Straight") configs.straight_config = prize;
            else if(result_type=="Rumble") configs.rumble_config = prize;
        }
        configs.lucky_configs = lucky_configs;
        configs.lucky_configs_RTL = lucky_configs_RTL;

        std::sort(configs.lucky_configs.rbegin(), configs.lucky_configs.rend());
        std::sort(configs.lucky_configs_RTL.rbegin(), configs.lucky_configs_RTL.rend());

        requests.coupon_generate_length = input["couponGenerateLength"];
        requests.coupon_generate_max_value = input["couponGenerateMaxValue"];
        requests.coupon_generate_min_value = input["couponGenerateMinValue"];
        requests.is_paired = input["isPaired"];

        if(requests.is_paired) {
            std::pair<std::string, double> lucky_coupon = pairedCoupon(configs.lucky_configs, requests.coupon_generate_length, requests.coupon_generate_max_value, input, stored_coupon);
        }
        else {
            std::pair<std::string, double> lucky_coupon = nonPairedCoupon(requests, configs, input, stored_coupon);
        }
        nlohmann::json suggestionArray = nlohmann::json::array();

        std::sort(stored_coupon.begin(), stored_coupon.end(), [](const std::pair<double, std::string>& a, const std::pair<double, std::string>& b) {
            if(a.first == b.first) {
                return a.second > b.second;
            }
            return a.first < b.first;
        });
        
        int numberOfResult = input["numberOfResult"];
        int resultSize = stored_coupon.size();

        for(int i=0; i<std::min(numberOfResult, resultSize); ++i) {
            nlohmann::json obj;
            obj["couponNumber"] = stored_coupon[i].second;
            obj["prize"] = stored_coupon[i].first;
            suggestionArray.push_back(obj);
        }
        nlohmann::json output;
        
        output["suggestions"] = suggestionArray;

        return invocation_response::success(output.dump(), "application/json");
    }
    catch(const std::exception& e)
    {
        return invocation_response::failure(e.what(), "application/json");
    }
}

int main()
{
    run_handler(my_handler);
    return 0;
}