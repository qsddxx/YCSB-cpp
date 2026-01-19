#ifndef MULTI_ZIPFIAN_GENERATOR_H_
#define MULTI_ZIPFIAN_GENERATOR_H_

#include <cstdint>
#include <stdexcept>
#include <random>
#include <mutex>
#include <vector>
#include <algorithm>
#include <iostream>
#include <cmath>
#include <cassert>

#include "zipfian_generator.h"
#include "utils/utils.h"

namespace ycsbc {
class AutoHotZoneSelector : public Generator<uint64_t> {
public:
  // 固定配置：目标累计概率80%，原始Zipf最大键数限制
  static constexpr double kTargetCumulativeProb = 0.8;
  static constexpr uint64_t kMaxNumItems = (UINT64_MAX >> 24);
  AutoHotZoneSelector(uint64_t min_key, uint64_t max_key,
                      uint32_t read_center_num = 1,
                      uint32_t write_center_num = 1,
                      double read_theta = 0.99,
                      double write_theta = 0.99) {
    if (min_key >= max_key) {
      throw std::invalid_argument("min_key 必须小于 max_key");
    }
    if (read_center_num == 0 || write_center_num == 0) {
      throw std::invalid_argument("读写热区数量不能为0");
    }
    if (read_theta <= 0 || write_theta <= 0) {
      throw std::invalid_argument("倾斜系数theta必须大于0");
    }
    min_key_ = min_key;
    max_key_ = max_key;
    total_keys_ = max_key - min_key + 1;
    read_center_num_ = read_center_num;
    write_center_num_ = write_center_num;
    read_theta_ = read_theta;
    write_theta_ = write_theta;
    read_hot_length_ = CalculateHotLength(read_theta_, total_keys_, kTargetCumulativeProb);
    write_hot_length_ = CalculateHotLength(write_theta_, total_keys_, kTargetCumulativeProb);
    std::random_device rd;
    GenerateHotCenters(read_centers_, read_center_num_, {});
    GenerateHotCenters(write_centers_, write_center_num_, read_centers_);
    read_zipfs_=new ZipfianGenerator(0, total_keys_ - 1, read_theta_);
    write_zipfs_=new ZipfianGenerator(0, total_keys_ - 1, write_theta_);
    Next(OperationType::kRead);
    Next(OperationType::kWrite);
  }
  ~AutoHotZoneSelector() {
    delete read_zipfs_;
    delete write_zipfs_;
  }
  uint64_t Next(OperationType op_type) override{
    std::lock_guard<std::mutex> lock(mutex_);

    if (op_type == OperationType::kRead) {
      uint32_t center_idx = RandomSelectCenterIndex(read_center_num_);
      uint64_t center = read_centers_[center_idx];
      uint64_t offset = read_zipfs_->Next();
      uint64_t mapped_offset = (center + offset) % total_keys_;
      last_read_key_ = min_key_ + mapped_offset;
      return last_read_key_;
    } else {
      uint32_t center_idx = RandomSelectCenterIndex(write_center_num_);
      uint64_t center = write_centers_[center_idx];
      uint64_t offset = write_zipfs_->Next();
      uint64_t mapped_offset = (center + offset) % total_keys_;
      last_write_key_ = min_key_ + mapped_offset;
      return last_write_key_;
    }
  }
  uint64_t Next() override {
    return Next(OperationType::kRead);
  }
  uint64_t Last(OperationType op_type) {
    std::lock_guard<std::mutex> lock(mutex_);
    return (op_type == OperationType::kRead) ? last_read_key_ : last_write_key_;
  }
  uint64_t Last() override{
    return  Next(OperationType::kRead);
  }
private:
  uint64_t CalculateHotLength(double theta, uint64_t total_items, double target_prob) {
    if (total_items < 2) {
      throw std::invalid_argument("总键数必须≥2");
    }
    double zeta_total = 0.0;
    for (uint64_t k = 1; k <= total_items; ++k) {
      zeta_total += 1.0 / std::pow(k, theta);
    }
    double cumulative_prob = 0.0;
    uint64_t hot_length = 0;
    for (uint64_t k = 1; k <= total_items; ++k) {
      double single_prob = (1.0 / std::pow(k, theta)) / zeta_total;
      cumulative_prob += single_prob;
      hot_length = k;
      if (cumulative_prob >= target_prob) {
        break;
      }
    }
    return hot_length;
  }
  void GenerateHotCenters(std::vector<uint64_t>& centers, uint32_t num, const std::vector<uint64_t>& avoid_centers) {
    std::uniform_int_distribution<uint64_t> center_dist(0, total_keys_ - 1);
    std::random_device rd_middle;
    std::mt19937_64 rng_middle = std::mt19937_64(rd_middle());
    for (uint32_t i = 0; i < num; ++i) {
      uint64_t center;
      do {
        center = center_dist(rng_middle);
      } while (std::find(centers.begin(), centers.end(), center) != centers.end() ||
               std::find(avoid_centers.begin(), avoid_centers.end(), center) != avoid_centers.end());
      centers.push_back(center);
    }
  }
  uint32_t RandomSelectCenterIndex(uint32_t num) {
    static thread_local std::once_flag init_flag;
    std::call_once(init_flag, InitThreadLocalRng);
    std::uniform_int_distribution<uint32_t> idx_dist(0, num - 1);
    return idx_dist(rng_);
  }
  static void InitThreadLocalRng() {
    static thread_local std::random_device rd;
    rng_ = std::mt19937_64(rd());
  }
  uint64_t min_key_;
  uint64_t max_key_;
  uint64_t total_keys_;
  uint32_t read_center_num_;
  uint32_t write_center_num_;
  double read_theta_;
  double write_theta_;
  uint64_t read_hot_length_;
  uint64_t write_hot_length_;
  std::vector<uint64_t> read_centers_;
  std::vector<uint64_t> write_centers_;
  ZipfianGenerator* read_zipfs_;
  ZipfianGenerator* write_zipfs_;
  uint64_t last_read_key_ = 0;
  uint64_t last_write_key_ = 0;
  static thread_local std::mt19937_64 rng_;
  std::mutex mutex_;
};
thread_local std::mt19937_64 ycsbc::AutoHotZoneSelector::rng_;
}
#endif