//
//  db.h
//  YCSB-cpp
//
//  Copyright (c) 2020 Youngjae Lee <ls4154.lee@gmail.com>.
//  Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>.
//

#ifndef YCSB_C_DB_H_
#define YCSB_C_DB_H_

#include "utils/properties.h"
#include "utils/timer.h"
#include "utils/rate_limit.h"
#include "operation.h"
#include "measurements.h"
#include "acknowledged_counter_generator.h"
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
#include <iostream>
namespace ycsbc {

///
/// Database interface layer.
/// per-thread DB instance.
///
//class Measurements;
/*
enum Operation {
  INSERT = 0,
  READ,
  UPDATE,
  SCAN,
  READMODIFYWRITE,
  DELETE,
  INSERT_FAILED,
  READ_FAILED,
  UPDATE_FAILED,
  SCAN_FAILED,
  READMODIFYWRITE_FAILED,
  DELETE_FAILED,
  MAXOPTYPE,
  EXIT
};
*/
struct Information
{
  using Clock = std::chrono::high_resolution_clock;
  Clock::time_point* end_time;
  AcknowledgedCounterGenerator* counter;
  std::atomic<int>* total_complete_num;
  bool is_loading;
};
class Task;
class DB {
 public:
 using Clock = std::chrono::high_resolution_clock;
 struct Field {
    std::string name;
    std::string value;
  };
  enum Status {
    kOK = 0,
    kError,
    kNotFound,
    kNotImplemented
  };
  class Task
  {
    public:
    using Clock = std::chrono::high_resolution_clock;
    Operation operation;
    std::string* table;
    std::shared_ptr<std::string> key;
    std::shared_ptr<std::vector<std::string>> fields;
    std::shared_ptr<std::vector<Field>> result;
    std::shared_ptr<std::vector<std::vector<Field>>> scan_result;
    int record_count;
    std::shared_ptr<std::vector<Field>> values;
    std::shared_ptr<utils::Timer<uint64_t, std::nano>> timer_;
    std::shared_ptr<folly::Promise<Clock::time_point>> promise;
    std::shared_ptr<Information> information;
    uint64_t key_num;
    Clock::time_point* end_time;
  };
  ///
  /// Initializes any state for accessing this DB.
  ///
  virtual void Init() { }
  ///
  /// Clears any state for accessing this DB.
  ///
  virtual void Cleanup() { }
  ///
  /// Reads a record from the database.
  /// Field/value pairs from the result are stored in a vector.
  ///
  /// @param table The name of the table.
  /// @param key The key of the record to read.
  /// @param fields The list of fields to read, or NULL for all of them.
  /// @param result A vector of field/value pairs for the result.
  /// @return Zero on success, or a non-zero error code on error/record-miss.
  ///
  virtual Status Read(const std::string &table, const std::string &key,
                   const std::vector<std::string> *fields,
                   std::vector<Field> &result) = 0;
  virtual Status Read(const std::string &table, std::shared_ptr<std::string> key,
                   const std::shared_ptr<std::vector<std::string>> fields,
                   std::shared_ptr<std::vector<Field>> result,std::shared_ptr<Information> information){return kOK;};
  ///
  /// Performs a range scan for a set of records in the database.
  /// Field/value pairs from the result are stored in a vector.
  ///
  /// @param table The name of the table.
  /// @param key The key of the first record to read.
  /// @param record_count The number of records to read.
  /// @param fields The list of fields to read, or NULL for all of them.
  /// @param result A vector of vector, where each vector contains field/value
  ///        pairs for one record
  /// @return Zero on success, or a non-zero error code on error.
  ///
  virtual Status Scan(const std::string &table, const std::string &key,
                   int record_count, const std::vector<std::string> *fields,
                   std::vector<std::vector<Field>> &result) = 0;
  virtual Status Scan(const std::string &table, std::shared_ptr<std::string> key,
                   int record_count, const std::shared_ptr<std::vector<std::string>> fields,
                   std::shared_ptr<std::vector<std::vector<Field>>> result,std::shared_ptr<Information> information){return kOK;};
  ///
  /// Updates a record in the database.
  /// Field/value pairs in the specified vector are written to the record,
  /// overwriting any existing values with the same field names.
  ///
  /// @param table The name of the table.
  /// @param key The key of the record to write.
  /// @param values A vector of field/value pairs to update in the record.
  /// @return Zero on success, a non-zero error code on error.
  ///
  virtual Status Update(const std::string &table, const std::string &key,
                     std::vector<Field> &values) = 0;
  virtual Status Update(const std::string &table, std::shared_ptr<std::string> key,
                     std::shared_ptr<std::vector<Field>>values,std::shared_ptr<Information> information){return kOK;};
  ///
  /// Inserts a record into the database.
  /// Field/value pairs in the specified vector are written into the record.
  ///
  /// @param table The name of the table.
  /// @param key The key of the record to insert.
  /// @param values A vector of field/value pairs to insert in the record.
  /// @return Zero on success, a non-zero error code on error.
  ///
  virtual Status Insert(const std::string &table, const std::string &key,
                     std::vector<Field> &values) = 0;
  virtual Status Insert(const std::string &table, std::shared_ptr<std::string>key,
                     std::shared_ptr<std::vector<Field>> values,std::shared_ptr<Information> information){return kOK;};
  ///
  /// Deletes a record from the database.
  ///
  /// @param table The name of the table.
  /// @param key The key of the record to delete.
  /// @return Zero on success, a non-zero error code on error.
  ///
  virtual Status Delete(const std::string &table, const std::string &key) = 0;
  virtual Status Delete(const std::string &table, std::shared_ptr<std::string> key,std::shared_ptr<Information> information){return kOK;};

  virtual ~DB() { }

  void SetProps(utils::Properties *props) {
    props_ = props;
  }
  virtual void CleanUpDirectly(){};
  void DoTaskSync(bool init_db, bool cleanup_db,folly::UnboundedQueue<Task,false,false,false>* TaskList,AcknowledgedCounterGenerator* counter,bool is_loading,int thread_num)
  {
    thread_local int exit_num=0;
    using Clock = std::chrono::high_resolution_clock;
    bool should_stop=false;
    try
    {
      if(init_db)
      {
        Init();
      }
      int total=0;
      //Task task_;
      //Status s;
      while(true)
      {  
        //while(!TaskList->try_dequeue(task_));
        auto task_=TaskList->dequeue();
        //auto start=Clock::now();
        switch(task_.operation)
        {
          case READ:
            Read(*task_.table, (task_.key), task_.fields, (task_.result),task_.information);
            //task_.promise->setValue(Clock::now());
            *(task_.information->end_time)=Clock::now();
            task_.information->total_complete_num->fetch_add(1);
            //measurements_->Report(READ,task_.timer_->GetElpased(Clock::now(),start));
            break;
          case SCAN:
            Scan(*task_.table, (task_.key),task_.record_count, task_.fields, (task_.scan_result),task_.information);
            //task_.promise->setValue(Clock::now());
            *(task_.information->end_time)=Clock::now();
            task_.information->total_complete_num->fetch_add(1);
            //measurements_->Report(SCAN,task_.timer_->GetElpased(Clock::now(),start));
            break;
          case INSERT:
            Insert(*task_.table,(task_.key),(task_.values),task_.information);
            *(task_.information->end_time)=Clock::now();
            task_.information->total_complete_num->fetch_add(1);
            if(!is_loading)
            {
              counter->Acknowledge(task_.key_num);
            }
            //task_.promise->setValue(Clock::now());
            //*(task_.information->end_time)=Clock::now();
            //task_.information->total_complete_num->fetch_add(1);
            //measurements_->Report(INSERT,task_.timer_->GetElpased(Clock::now(),start));
            break;
          case UPDATE:
            Update(*task_.table,(task_.key),(task_.values),task_.information);
            //task_.promise->setValue(Clock::now());
            *(task_.information->end_time)=Clock::now();
            task_.information->total_complete_num->fetch_add(1);
            //measurements_->Report(UPDATE,task_.timer_->GetElpased(Clock::now(),start));
            break;
          case READMODIFYWRITE:
            Read(*task_.table, (task_.key), task_.fields, (task_.result),task_.information);
            task_.promise->setValue(Clock::now());
            Update(*task_.table,(task_.key),(task_.values),task_.information);
            task_.promise->setValue(Clock::now());
            break;
          case EXIT:
            exit_num++;
            if(exit_num==thread_num)
            {
              should_stop=true;
            }
            break;
          default:
          //std::cout<<"Case Default"<<std::endl;
          break;
        }
        if(should_stop)
        {
          break;
        }
        else
        {
          total++;
        }
      }
      if(cleanup_db)
      {
        Cleanup();
      }
    }catch (const utils::Exception &e) {
      std::cerr << "Caught exception: " << e.what() << std::endl;
      exit(1);
    }
  }
  void DoTaskAsync(bool init_db, bool cleanup_db,std::vector<Task>& task_list,AcknowledgedCounterGenerator* counter)
  {
    bool should_stop=false;
    if(init_db)
    {
      Init();
    }
    int total=0;
    //Status s;
    for(auto& task_:task_list)
    {
      switch(task_.operation)
      {
        case READ:
          Read(*task_.table, (task_.key), task_.fields, (task_.result),task_.information);
          break;
        case SCAN:
          Scan(*task_.table, (task_.key),task_.record_count, task_.fields, (task_.scan_result),task_.information);
          break;
        case INSERT:
          Insert(*task_.table,(task_.key),(task_.values),task_.information);
          break;
        case UPDATE:
          Update(*task_.table,(task_.key),(task_.values),task_.information);
          break;
        case READMODIFYWRITE:
          Read(*task_.table, (task_.key), task_.fields, (task_.result),task_.information);
          Update(*task_.table,(task_.key),(task_.values),task_.information);
          break;
        case EXIT:
          should_stop=true;
          break;
        default:
        break;
      }
      if(should_stop)
      {
        break;
      }
      else
      {
        total++;
      }
    }
    if(cleanup_db)
    {
      Cleanup();
    }
  }
  virtual void SetAsyncTest(bool async_test_)
  {
    async_test=async_test_;
  }
  virtual void SetMeasurements(Measurements* m)
  {
    measurements_=m;
  }
 protected:
  
  utils::Properties *props_;
  //DBTaskPublisher* task_publisher;
  Measurements* measurements_;
  bool async_test=true;
};
} // ycsbc

#endif // YCSB_C_DB_H_
