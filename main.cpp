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

using namespace std;
using json = nlohmann::json;
using namespace aws::lambda_runtime;

string dummy_coupon = "782346120987";
vector<pair<double, string>> stored_coupon;

struct ResultConfig {
    vector<pair<int, double>> lucky_configs;
    vector<pair<int, double>> lucky_configs_RTL;
    double straight_config;
    double rumble_config;
};

struct Request {
    int coupon_generate_length;
    int coupon_generate_max_value;
    int coupon_generate_min_value;
    bool is_paired;
};

int couponBitMask(string coupon) // Bitmasking Paired Coupon
{   
    int coupon_mask = 0;

    for(int j=0; j<coupon.length(); j+=2) {
        int hold = stoi(coupon.substr(j, 2));
        coupon_mask |= (1<<hold);
    }

    return coupon_mask;
}

double calculatePrizeForPaired(vector<int>& coupon_map, vector<pair<int, double>>& config, int lucky_coupon)
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

double calculatePrizeForNonPaired(Request request, ResultConfig& configs, map<string, int>& straight_coupon_map, map<string, int>& ramble_coupon_map, vector<string>& lucky_coupons, string g_coupon)
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

        total += max(LTR_prize, RTL_prize);
    }

    sort(g_coupon.begin(), g_coupon.end());
    total += ramble_coupon_map[g_coupon] * configs.rumble_config;

    return total;    
}

pair<string, double> generateCombinations(vector<string>& set, vector<int>& coupon_map, vector<pair<int, double>>& config, string g_coupon, int start, int k) {
    if(g_coupon.length() == k) {
        int coupon_mask = couponBitMask(g_coupon);
        double prize = calculatePrizeForPaired(coupon_map, config, coupon_mask);
        stored_coupon.push_back(make_pair(prize, g_coupon));
        return {g_coupon, prize};
    }

    pair<string, double> lucky_coupon = make_pair(dummy_coupon, 10000000.0); //Initial dummy coupon and prize

    for(int i = start; i < set.size(); ++i) {
        if(i==start) {
            lucky_coupon = generateCombinations(set, coupon_map, config, g_coupon + set[i], i + 1, k);
        }
        else {
            pair<string, double> ps = generateCombinations(set, coupon_map, config, g_coupon + set[i], i + 1, k);
            if(ps.second < lucky_coupon.second) lucky_coupon = ps;
        }
    }

    return lucky_coupon;
}

map<pair<int, char>, int> luckyCouponMapping(vector<string>& lucky_coupons)
{
    map<pair<int, char>, int> position_number_map;

    for(int i=0; i<lucky_coupons.size(); ++i) {
        for(int j=0; j<lucky_coupons[i].length(); ++j) {
            position_number_map[{j, lucky_coupons[i][j]}]++;
        }
    }

    return position_number_map;
}

map<string, int> straightAndRambleMapping(vector<string>& coupons)
{
    map<string, int> coupon_map;

    for(int i=0; i<coupons.size(); ++i) {
        coupon_map[coupons[i]]++;
    }

    return coupon_map;
}

string generateCouponNumber(Request request, map<pair<int, char>, int>& coupon_map, string coupon, bool is_random)
{
    if(is_random) {
        // Generate Random Positions
        int number_of_random_positions = request.coupon_generate_length/1;

        random_device dev;
        mt19937 rng(dev());
        uniform_int_distribution<mt19937::result_type> dist6(0, request.coupon_generate_length);

        vector<int> random_positions;
        for(int i=0; i<number_of_random_positions; ++i) {
            random_positions.push_back(dist6(rng));
        }

        uniform_int_distribution<mt19937::result_type> dist7(request.coupon_generate_min_value, request.coupon_generate_max_value);
        
        for(int i=0; i<random_positions.size(); ++i) {
            int pos = random_positions[i];
            int rand_val = dist7(rng); // Generate Random Value For Random Position
            coupon[pos] = '0'+rand_val;
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

pair<string, double> pairedCoupon(vector<pair<int, double>>& config, int coupon_length, int coupon_generate_max_value, const json& input)
{
    // Coupon Input
    int coupon_number = input["couponSize"];
    vector<int> coupon_map;

    for(int i=0; i<coupon_number; ++i) {
        string coupon = input["coupons"][i]["couponNumber"];
        string coupon_type = input["coupons"][i]["type"];
        int quantity = input["coupons"][i]["quantity"];

        while(quantity--) coupon_map.push_back(couponBitMask(coupon));
    }

    // Create a set of available digits
    vector<string> set;
    for(int i=1; i<=coupon_generate_max_value; ++i) {
        string s="";
        if(i<10) s+='0';
        s += to_string(i);
        set.push_back(s);
    }

    pair<string, double> lucky_coupon = generateCombinations(set, coupon_map, config, "", 0, coupon_length);

    // Sort the lucky coupon
    vector<string> sortedCoupon;
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

pair<string, double> nonPairedCoupon(Request& request, ResultConfig& configs, const json& input)
{
    int coupon_size = input["couponSize"];
    vector<string> straight_coupons;
    vector<string> lucky_coupons;
    vector<string> ramble_coupons;

    double total_selling_price = coupon_size * 0.5;

    for(int i=0; i<coupon_size; ++i) {
        string coupon = input["coupons"][i]["couponNumber"];
        string coupon_type = input["coupons"][i]["type"];
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

    map<pair<int, char>, int> lucky_coupon_map = luckyCouponMapping(lucky_coupons);
    map<string, int> straight_coupon_map = straightAndRambleMapping(straight_coupons);
    map<string, int> ramble_coupon_map = straightAndRambleMapping(ramble_coupons);
    map<string, int> stored_coupon_map;

    string initial_coupon = "";
    for(int i=0; i<request.coupon_generate_length; ++i) initial_coupon += '0';

    string generated_coupon_number = generateCouponNumber(request, lucky_coupon_map, initial_coupon, false);

    int threshold = 100000;
    double prize_amount;
    double min_prize = 1000000000.0;
    string min_prized_coupon;

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
    string payload = request.payload;
    json input = json::parse(payload);

    json output;

    Request requests;

    // Result Config Input
    int result_config_size = input["configSize"];

    ResultConfig configs;
    vector<pair<int, double>> lucky_configs;
    vector<pair<int, double>> lucky_configs_RTL;

    for(int i=0; i<result_config_size; ++i) {
        int lucky_match = input["configs"][i]["luckyMatch"];
        double prize = input["configs"][i]["prize"];
        string result_type = input["configs"][i]["resultType"];
        bool left_to_right = input["configs"][i]["leftToRight"];

        if(result_type.find("Lucky") != string::npos) {
            if(left_to_right) lucky_configs.push_back(make_pair(lucky_match, prize));
            else lucky_configs_RTL.push_back(make_pair(lucky_match, prize));
        }
        else if(result_type=="Straight") configs.straight_config = prize;
        else if(result_type=="Rumble") configs.rumble_config = prize;
    }
    configs.lucky_configs = lucky_configs;
    configs.lucky_configs_RTL = lucky_configs_RTL;

    sort(configs.lucky_configs.rbegin(), configs.lucky_configs.rend());
    sort(configs.lucky_configs_RTL.rbegin(), configs.lucky_configs_RTL.rend());

    requests.coupon_generate_length = input["couponGenerateLength"];
    requests.coupon_generate_max_value = input["couponGenerateMaxValue"];
    requests.coupon_generate_min_value = input["couponGenerateMinValue"];
    requests.is_paired = input["isPaired"];

    if(requests.is_paired) {
        pair<string, double> lucky_coupon = pairedCoupon(configs.lucky_configs, requests.coupon_generate_length, requests.coupon_generate_max_value, input);
    }
    else {
        pair<string, double> lucky_coupon = nonPairedCoupon(requests, configs, input);
    }
    json suggestionArray = json::array();

    sort(stored_coupon.begin(), stored_coupon.end());
    
    int numberOfResult = input["numberOfResult"];
    int resultSize = stored_coupon.size();

    for(int i=0; i<min(numberOfResult, resultSize); ++i) {
        json obj;
        obj["couponNumber"] = stored_coupon[i].second;
        obj["prize"] = stored_coupon[i].first;
        suggestionArray.push_back(obj);
    }

    output["suggestions"] = suggestionArray;

    return invocation_response::success(output.dump(), "application/json");
}

int main()
{
    run_handler(my_handler);
    return 0;
}