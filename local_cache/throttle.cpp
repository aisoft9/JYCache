
#include "throttle.h"
#include "errorcode.h"
#include "glog/logging.h"
#include "common.h"
#include "config.h"

namespace HybridCache {

void Throttle::CleanBlockTime() {
    for (const auto& curr_ : this->job_waiting_) {
        job_waiting_.assign(curr_.first, 0.0);
    }
    job_bandwidth_.clear();
}

std::vector<std::string> ToSplitString(const std::string &input) {
    std::vector<std::string> result;
    std::stringstream ss(input);
    std::string item;
    while (std::getline(ss, item, ',')) {
        result.push_back(item);
    }
    return result;
}

int Throttle::SetNewLimits(const std::string &input) {
    // no change
    if (input == "")
        return 0;
    
    // 传送格式：filename,rate,burst
    std::vector<std::string> result = ToSplitString(input);
    std::string file_name;
    double rate;
    double burst;
    std::ostringstream oss;
    std::string printres="";
    if (result.size()%3 != 0) {
        LOG(ERROR) << "[Throttle] The format of new limits is Wrong";
        return -1;
    }
    auto it = job_tokenlimit_.begin();
    for (size_t i = 0; i < result.size(); i+=3) {
        file_name = result[i];
        rate = std::stod(result[i + 1]);
        burst = std::stod(result[i + 2]);
        auto temp_it = job_tokenlimit_.find(file_name);  // last tokenbucket
        if (temp_it != job_tokenlimit_.end()) {
            // temp_it->second->reset(rate,burst);
            double curr_available = temp_it->second->available();
            auto token_bucket = std::make_shared<folly::TokenBucket>(rate, burst);
            token_bucket->setCapacity(curr_available,std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count() );
            job_tokenlimit_.assign(file_name, token_bucket);
        }
    }
    
    for (auto printtemp : job_tokenlimit_) {
        oss.str("");  // 清空流内容
        oss.clear();  // 清除错误状态
        oss<<printtemp.first<<"\trate:"<<printtemp.second->rate()
            <<"\tburst:"<<printtemp.second->burst()
            <<"\tavailable:"<<printtemp.second->available()<<"\t\t";
        printres += oss.str();
    }
    if (EnableLogging)
        LOG(INFO) << "[Throttle] Current Resize Each File's Flow : \t"<<printres;

    return 0;
}

// blocktime高于平均数的增加对应比例的带宽，
// 低于平均数的减少对应比例的带宽
std::string Throttle::Cal_New4Test() {
    std::string stringres = "";
    double total_waiting_time = 0.0;
    int job_num = 0;
    double rate;
    double TotalBw = 649651540;  // 当前轮次可调整带宽资源
    std::ostringstream oss;

    folly::ConcurrentHashMap<std::string, double> curr_for_cal;
    if (job_waiting_.size() == 0) {  // 当前无任务
        return stringres;
    }
    if (job_waiting_.size() == 1) {  // 单个任务，调整到最大
        oss.str("");  // 清空流内容
        oss.clear();  // 清除错误状态
        // 传送格式：filename,rate,burst
        oss<<job_waiting_.cbegin()->first<<",";
        oss<<649651540<<","<<649651540<<",";
        stringres += oss.str();
        return stringres;
    }

    // 由于是并行执行，复制一个副本防止计算错误
    for (auto& curr_ : job_waiting_) {
        if (curr_.second == 0)  // 在这段时间内未执行写入/未执行完，暂不进行修改，留到下一轮
            continue;
        curr_for_cal.insert(curr_.first, curr_.second);
        ++job_num;
        total_waiting_time += curr_.second;
        if (job_bandwidth_[curr_.first] > TotalBw)  // 防止为0
            TotalBw = job_bandwidth_[curr_.first];
    }
    
    if (job_num == 0)
        return stringres;

    // 在waiting中的在job_tokenlimit_中
    for (auto& curr_ : curr_for_cal) {
        rate = curr_.second / total_waiting_time * TotalBw;
        rate = std::max(rate, TotalBw / 10);
        rate = std::min(rate, TotalBw / 2);
        oss.str("");  // 清空流内容
        oss.clear();  // 清除错误状态
        // 传送格式：filename,rate,burst
        oss << curr_.first << "," << rate << "," << TotalBw <<",";
        stringres += oss.str();
    }
    // if (EnableLogging)
    //     LOG(INFO) << "[Throttle]"<<__func__<<" Resize Each File's Flow : "<<stringres;
    
    return stringres;
}

int Throttle::Put_Consume(const std::string& file_name, size_t sizes) {
    int temp = false;
    auto result = this->job_tokenlimit_.find(file_name);
    if (result == job_tokenlimit_.end()) {
        // 日志：len:131072，res:0，writePagecnt:2，time:0.201659ms
        // 大概是619.38 MB/s  649651540
        auto token_bucket = std::make_shared<folly::TokenBucket>(32768000, 65536000);
        token_bucket->setCapacity(32768000, std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count());
        job_tokenlimit_.insert_or_assign(file_name, token_bucket);
        job_waiting_.insert_or_assign(file_name, 0.0);
        temp = true;
    }
    auto insert_result = job_tokenlimit_.find(file_name);
    std::chrono::steady_clock::time_point BlockTimeStart = std::chrono::steady_clock::now();
    while (!(insert_result->second->consume(sizes)));  // consume
    double BlockTime = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - BlockTimeStart).count();
    job_waiting_.assign(file_name, job_waiting_[file_name] + BlockTime);

    // if(EnableLogging)
    // {
    //     LOG(INFO)<<"[Throttle] "<<file_name <<" BlockTime : "<<BlockTime;
    // }

    // if (EnableLogging)
    // {
    //     LOG(INFO)<<"[LocalWriteCache]"<<file_name<<" burst : "<<job_tokenlimit_[file_name]->burst()<<" rate : "
    //             <<job_tokenlimit_[file_name]->rate()<<" BlockTime : "<<BlockTime<<"ms "
    //             <<"TotalBlockTime : "<<job_waiting_[file_name]<<"ms "
    //             <<"newly insert : "<<temp
    //             <<" size : "<<sizes
    //             <<" avalible : "<<job_tokenlimit_[file_name]->available();
    // }
    return SUCCESS;
}

// 当文件被删除/flush时会用到
void Throttle::Del_File(const std::string& file_name) {
    job_waiting_.erase(file_name);
    job_tokenlimit_.erase(file_name);
    job_bandwidth_.erase(file_name);
}

void Throttle::Close() {
    job_tokenlimit_.clear();
    job_waiting_.clear();
    job_bandwidth_.clear();
}

}
