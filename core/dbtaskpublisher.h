#ifndef YCSB_C_DBTASKPUBLISHER_H_
#define YCSB_C_DBTASKPUBLISHER_H_
#include "utils/properties.h"
#include "utils/timer.h"
#include "utils/rate_limit.h"
#include "measurements.h"
#include "core_workload.h"
#include <chrono>
#include <folly/concurrency/UnboundedQueue.h>
#include <folly/futures/Future.h>
#include <folly/futures/Promise.h>
#include <folly/executors/CPUThreadPoolExecutor.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include "db.h"
namespace ycsbc
{

  class DBTaskPublisher
  {
  public:
    DBTaskPublisher(int load_total_ops_, int transaction_total_ops_, CoreWorkload *wl_, int64_t task_per_second_, int thread_num_, bool async_test_, Measurements *m,AcknowledgedCounterGenerator* counter,int num_per_batch_, int producer_thread_num_=4) : wl(wl_),
                                                                                                                                                                                                 load_total_ops(load_total_ops_),
                                                                                                                                                                                                 transaction_total_ops(transaction_total_ops_),
                                                                                                                                                                                                 task_per_second(task_per_second_),
                                                                                                                                                                                                 thread_num(thread_num_),
                                                                                                                                                                                                 async_test(async_test_),
                                                                                                                                                                                                 measurements_(m),
                                                                                                                                                                                                 counter_(counter),
                                                                                                                                                                                                 producer_thread_num(producer_thread_num_),
                                                                                                                                                                                                 num_per_batch(num_per_batch_)
                                                                                                                                                                                                
    {
      //workerThread = std::thread(&DBTaskPublisher::workerLoop, this);
      for (int i = 0; i < producer_thread_num; ++i)
      {
          int tid=i;
          std::cout<<"The first producer is "<<tid<<std::endl;
          producer_threads.emplace_back(&DBTaskPublisher::workerLoop, this,std::move(tid));  // 传入线程ID，用于分片
      }
    }
    ~DBTaskPublisher()
    {
      stopFlag.store(true);
      cv_task.notify_all();
      //workerThread.join();
      for (auto &t : producer_threads)
      {
          if (t.joinable())
          {
              t.join();
          }
      }
    }
    void BeginLoading(bool init, bool clean)
    {
      measurements_->ClearTaskNum();
      is_loading = true;
      should_clean_up = clean;
      should_init_db = true;
      total_complete_num.store(0);
      cv_task.notify_all();
    }
    void BeginTransaction(bool init, bool clean)
    {
      measurements_->ClearTaskNum();
      is_loading = false;
      should_clean_up = true;
      should_init_db = init;
      total_complete_num.store(0);
      cv_task.notify_all();
    }
    void SetRateLimiter(utils::RateLimiter *r)
    {
      rlimt_.emplace_back(r);
    }
    void Clear()
    {
      if (!rlimt_.empty())
      {
        rlimt_.clear();
      }
      rlimt_ = {};
      total_complete_num.store(0);
    }
    void SetDB(std::vector<DB *> *dblist)
    {
      for (auto &db : *dblist)
      {
        DBList.emplace_back(db);
      }
    }
    void SetTimerList(int num)
    {
      timer_list.clear();
      timer_list.resize(producer_thread_num);
      for(int i=0;i<producer_thread_num;i++)
      {
        int64_t local_load_total_ops = this->load_total_ops / producer_thread_num;
        if (i == producer_thread_num - 1)
        {
          local_load_total_ops += this->load_total_ops % producer_thread_num;
        }
        timer_list[i].clear();
        //timer_index=0;
        timer_list[i].resize(local_load_total_ops);
        std::cout<<" line "<<i<<" size "<<local_load_total_ops<<std::endl;
      }
      timer_index=0;
    }
    void FinishReport(const int interval)
    {
      std::cout<<"Begin Report "<<std::endl;
      std::vector<utils::Timer<uint64_t,std::nano>> middle_timer_list;
      int size=0;
      for(auto& tl:timer_list)
      {
        size+=tl.size();
      }
      middle_timer_list.reserve(size);
      for(auto& tl:timer_list )
      {
        for(auto&t :tl)
        {
          middle_timer_list.emplace_back(t);
        }
      }
      std::sort(middle_timer_list.begin(), middle_timer_list.end(),
            [](const utils::Timer<uint64_t, std::nano>& a, const utils::Timer<uint64_t, std::nano>& b) {
              return a.end_time < b.end_time;
            });
      Clock::time_point current_start = middle_timer_list[0].time_;
      auto interval_duration = std::chrono::seconds(interval);
      Clock::time_point current_end = current_start + interval_duration;
      size_t processed_count = 0;
      const size_t total_timer = middle_timer_list.size();
      int num_report=1;
      bool should_do=false;
      while(processed_count<total_timer)
      {
        for (size_t i = processed_count; i < total_timer; ++i)
        {
          auto& timer = middle_timer_list[i];
          if (timer.end_time < current_end)
          {
            should_do=true;
            switch(timer.operation)
            {
              case READ:
                this->measurements_->Report(READ, timer.EndUseEndtime());
                break;
              case SCAN:
                this->measurements_->Report(SCAN, timer.EndUseEndtime());
                break;
              case INSERT:
                this->measurements_->Report(INSERT, timer.EndUseEndtime());
                break;
              case UPDATE:
                this->measurements_->Report(UPDATE, timer.EndUseEndtime());
                break;
              default:
                break;
            }
            processed_count++;
          }
          else
          {
            should_do=false;
            std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
            std::time_t now_c = std::chrono::system_clock::to_time_t(now);
            std::cout << std::put_time(std::localtime(&now_c), "%F %T") << ' '
              << static_cast<long long>(num_report*interval) << " sec: ";
            std::cout << measurements_->GetStatusMsg() << std::endl;
            num_report++;
            current_start = current_end;
            current_end = current_start + interval_duration;
            break;
          }
        } 
      }
      if(should_do)
      {
        std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        std::cout << std::put_time(std::localtime(&now_c), "%F %T") << ' '
          << static_cast<long long>(num_report*interval) << " sec: ";
        std::cout << measurements_->GetStatusMsg() << std::endl;
      }
    }
    void CleanUpDirectly()
    {
      for(int i=0;i<producer_thread_num;i++)
      {
       (*(DBList[i])).CleanUpDirectly();
      }
    }
    folly::UnboundedQueue<DB::Task, false, false, false> TaskList;
    std::vector<DB *> DBList;
    std::atomic<int> total_complete_num;
    std::vector<std::vector<utils::Timer<uint64_t,std::nano>>> timer_list;

  private:
    void workerLoop(int i)
    {
      // using Clock = std::chrono::high_resolution_clock;
      thread_local int index=i;
      std::cout<<"The index is "<<index<<std::endl;
      while (!stopFlag.load())
      {
        {
          std::unique_lock<std::mutex> lock(mtx);
          cv_task.wait(lock);
          lock.unlock();
        }
        if (stopFlag.load())
        {
          break;
        }
        thread_local int64_t total = 0;
        thread_local std::vector<DB::Task> middle_task;
        thread_local int batch_num = 0;
        thread_local uint64_t timer_index_=0;
        thread_local uint64_t middle_timer_index=0;
        thread_local Clock::time_point time1=Clock::now();
        int64_t local_load_total_ops = this->load_total_ops / producer_thread_num;
        if (index == producer_thread_num - 1)
        {
          local_load_total_ops += this->load_total_ops % producer_thread_num;
        }
        sleep(2);
        if (is_loading)
        {
          std::cout << "Loading: " << local_load_total_ops << std::endl;
          total=0;
          middle_task.clear();
          batch_num=0;
          timer_index=0;
          middle_timer_index=0;
          time1=Clock::now();
          if (async_test)
          {
            (*(DBList[index])).Init();
          }
          while (total <  local_load_total_ops)
          {
            batch_num = std::min(num_per_batch,  local_load_total_ops - total);
            wl->GenerateInsertTask(batch_num, middle_task);
            //GeneratePromise(middle_task,counter_,true);
            timer_index_=middle_timer_index;
            for(int i=0;i<batch_num;i++)
            {
              //timer_list[timer_index].SetStartTime(time1);
              timer_list[index][middle_timer_index].operation=middle_task[i].operation;
              middle_task[i].information->end_time=&timer_list[index][middle_timer_index].end_time;
              middle_task[i].information->total_complete_num=&total_complete_num;
              middle_timer_index++;
            }
             time1=Clock::now();
            for(int i=0;i<batch_num;i++)
            {
              timer_list[index][timer_index_].SetStartTime(time1);
              timer_index_++;
            }
            if (!async_test)
            {
              for (auto &t : middle_task)
              {
                TaskList.enqueue(std::move(t));
              }
            }
            else
            {
              (*(DBList[index])).DoTaskAsync(false, false, middle_task,counter_);
            }
            total += batch_num;
            middle_task.clear();
            if (!rlimt_.empty())
            {
              rlimt_[index]->Consume(batch_num);
            }
          }
          //if (should_clean_up && async_test)
          //{
          //  (*(DBList[0])).CleanUpDirectly();
          //}
        }
        else
        {
          total=0;
          middle_task.clear();
          batch_num=0;
          timer_index=0;
          middle_timer_index=0;
          time1=Clock::now();
          if (should_init_db && async_test)
          {
            (*(DBList[index])).Init();
          }
          //std::cout << "Transaction " << transaction_total_ops << std::endl;
          int64_t local_transaction_total_ops = this->transaction_total_ops / producer_thread_num;
          if (index == producer_thread_num - 1)
          {
            local_transaction_total_ops += this->transaction_total_ops % producer_thread_num;
          }
          while (total < local_transaction_total_ops)
          {
            batch_num = std::min(num_per_batch, local_transaction_total_ops - total);
            wl->GenerateTransactionTask(batch_num, middle_task);
            //GeneratePromise(middle_task,counter_,false);
            //Clock::time_point time1=Clock::now();
            timer_index_=middle_timer_index;
            for(int i=0;i<batch_num;i++)
            {
              //timer_list[timer_index].SetStartTime(time1);
              timer_list[index][middle_timer_index].operation=middle_task[i].operation;
              middle_task[i].information->end_time=&timer_list[index][middle_timer_index].end_time;
              middle_task[i].information->total_complete_num=&total_complete_num;
              middle_timer_index++;
            }
            Clock::time_point time1=Clock::now();
            for(int i=0;i<batch_num;i++)
            {
              timer_list[index][timer_index_].SetStartTime(time1);
              timer_index_++;
            }
            if (!async_test)
            {
              for (auto &t : middle_task)
              {
                TaskList.enqueue(std::move(t));
              }
              // std::cout<<"Finish task insert "<<TaskList.size()<<std::endl;
            }
            else
            {
              (*(DBList[index])).DoTaskAsync(false, false, middle_task,counter_);
            }
            total += batch_num;
            middle_task.clear();
            if (!rlimt_.empty())
            {
              rlimt_[index]->Consume(batch_num);
            }
          }
          //if (async_test)
          //{
          //  (*(DBList[0])).CleanUpDirectly();
          //}
        }
        if (!async_test)
        {
          for (int i = 0; i < thread_num; i++)
          {
            DB::Task t;
            t.operation = EXIT;
            TaskList.enqueue(t);
          }
        }
        std::cout<<"The producer "<<index<<" is OK "<<std::endl;
      }
    }
    // std::vector<DB*> DBWrapperList;
    using Clock = std::chrono::high_resolution_clock;
    CoreWorkload *wl;
    std::condition_variable cv_task;
    int64_t load_total_ops;
    int64_t transaction_total_ops;
    int64_t task_per_second;
    int thread_num;
    bool async_test;
    std::vector<utils::RateLimiter *>rlimt_ = {};
    // std::atomic<bool> should_stop=false;
    std::atomic<bool> stopFlag = false;
    std::mutex mtx;
    bool is_loading = false;
    std::thread workerThread;
    Measurements *measurements_;
    bool should_init_db = true;
    bool should_clean_up = false;
    AcknowledgedCounterGenerator* counter_;
    uint64_t timer_index=0;
    int producer_thread_num;
    std::vector<std::thread> producer_threads;
    std::atomic<int> index_=0;
    int64_t num_per_batch;
  };
}
#endif