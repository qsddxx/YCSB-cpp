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
namespace ycsbc
{

  class AutoHotZoneSelector : public Generator<uint64_t>
  {
  public:
    AutoHotZoneSelector(uint64_t min_key, uint64_t max_key,
                        double read_center_pos = 0.0,
                        double write_center_pos = 0.0,
                        double read_theta = 0.99,
                        double write_theta = 0.99);
    ~AutoHotZoneSelector();
    uint64_t Next(OperationType op_type) override;
    uint64_t Next() override;
    uint64_t Last(OperationType op_type) override;
    uint64_t Last() override;

  private:
    uint64_t CalculateCenter(uint64_t total_keys, double pos) const;
    uint64_t min_key_;
    uint64_t max_key_;
    uint64_t total_keys_;
    double read_theta_;
    double write_theta_;
    double read_center_pos_;
    double write_center_pos_;
    uint64_t read_center_;
    uint64_t write_center_;
    ZipfianGenerator read_zipfs_;
    ZipfianGenerator write_zipfs_;
    uint64_t last_read_key_ = 0;
    uint64_t last_write_key_ = 0;
  };

}
#endif