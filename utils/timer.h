//
//  timer.h
//  YCSB-cpp
//
//  Copyright (c) 2020 Youngjae Lee <ls4154.lee@gmail.com>.
//  Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>.
//

#ifndef YCSB_C_TIMER_H_
#define YCSB_C_TIMER_H_

#include <./core/operation.h>
#include <chrono>

namespace ycsbc {

namespace utils {

template <typename R=uint64_t, typename P = std::ratio<1>>
class Timer {
 public:
 using Duration = std::chrono::duration<R, P>;
  using Clock = std::chrono::high_resolution_clock;
  void Start() {
    time_ = Clock::now();
  }

  R End() {
    Duration span;
    Clock::time_point t = Clock::now();
    span = std::chrono::duration_cast<Duration>(t - time_);
    return span.count();
  }
  void SetStartTime(Clock::time_point new_time_)
  {
    time_=new_time_;
  }
  R End(Clock::time_point end_time)
  {
    Duration span;
    span = std::chrono::duration_cast<Duration>(end_time - time_);
    return span.count();
  }
  R EndUseEndtime() {
    Duration span;
    //Clock::time_point t = Clock::now();
    span = std::chrono::duration_cast<Duration>(end_time - time_);
    return span.count();
  }
  R GetElpased(Clock::time_point end_time,Clock::time_point start_time)
  {
    Duration span;
    span = std::chrono::duration_cast<Duration>(end_time-start_time);
    return span.count();
  }


  //using Duration = std::chrono::duration<R, P>;
  //using Clock = std::chrono::high_resolution_clock;
  ycsbc::Operation operation;
  Clock::time_point time_;
  Clock::time_point end_time;
};

} // utils

} // ycsbc

#endif // YCSB_C_TIMER_H_

