//
//  measurements.h
//  YCSB-cpp
//
//  Copyright (c) 2020 Youngjae Lee <ls4154.lee@gmail.com>.
//

#ifndef YCSB_C_MEASUREMENTS_H_
#define YCSB_C_MEASUREMENTS_H_

//#include "core_workload.h"
#include "utils/properties.h"
#include "operation.h"
#include <atomic>

#ifdef HDRMEASUREMENT
#include <hdr/hdr_histogram.h>
#endif

typedef unsigned int uint;

namespace ycsbc {

class Measurements {
 public:
  virtual void Report(Operation op, uint64_t latency) = 0;
  virtual std::string GetStatusMsg() = 0;
  virtual void Reset() = 0;
  virtual void SetTaskNum(uint64_t task_num)
  {
    uint64_t prev_min = undo_task_num[1].load(std::memory_order_relaxed);
    while (prev_min > task_num
          && !undo_task_num[1].compare_exchange_weak(prev_min, task_num, std::memory_order_relaxed));
    uint64_t prev_max = undo_task_num[0].load(std::memory_order_relaxed);
    while (prev_max < task_num
          && !undo_task_num[0].compare_exchange_weak(prev_max, task_num, std::memory_order_relaxed));
    }
  virtual void ClearTaskNum()
  {
    undo_task_num[0].store(0);
    undo_task_num[1].store(std::numeric_limits<uint64_t>::max());
  }
  std::atomic<uint64_t> undo_task_num[2];
};

class BasicMeasurements : public Measurements {
 public:
  BasicMeasurements();
  void Report(Operation op, uint64_t latency) override;
  std::string GetStatusMsg() override;
  void Reset() override;
  
 private:
  std::atomic<uint> count_[MAXOPTYPE];
  std::atomic<uint64_t> latency_sum_[MAXOPTYPE];
  std::atomic<uint64_t> latency_min_[MAXOPTYPE];
  std::atomic<uint64_t> latency_max_[MAXOPTYPE];
  
};

#ifdef HDRMEASUREMENT
class HdrHistogramMeasurements : public Measurements {
 public:
  HdrHistogramMeasurements();
  void Report(Operation op, uint64_t latency) override;
  std::string GetStatusMsg() override;
  void Reset() override;
  void ClearTaskNum() override
  {
    hdr_reset(his_task_num[0]);
  }
  void SetTaskNum(uint64_t task_num) override
  {
    hdr_record_value_atomic(his_task_num[0], task_num);
  }
  
 private:
  hdr_histogram *histogram_[MAXOPTYPE];
  hdr_histogram *his_task_num[1];
  //std::atomic<uint64_t> undo_task_num[2];
};
#endif

Measurements *CreateMeasurements(utils::Properties *props);

} // ycsbc

#endif // YCSB_C_MEASUREMENTS
