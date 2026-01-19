//
//  db_wrapper.h
//  YCSB-cpp
//
//  Copyright (c) 2020 Youngjae Lee <ls4154.lee@gmail.com>.
//

#ifndef YCSB_C_DB_WRAPPER_H_
#define YCSB_C_DB_WRAPPER_H_

#include <string>
#include <vector>
#include <folly/concurrency/UnboundedQueue.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "db.h"
#include "dbtaskpublisher.h"
#include "measurements.h"
#include "utils/timer.h"
#include "utils/utils.h"
#include "utils/rate_limit.h"

namespace ycsbc {

class DBWrapper : public DB {
 public:
  DBWrapper(DB *db, Measurements *measurements) : db_(db) {measurements_=measurements;}
  ~DBWrapper() {
    delete db_;
  }
  void Init() {
    db_->Init();
  }
  void Cleanup() {
    db_->Cleanup();
  }
  void SetAsyncTest(bool async_test_) override
  {
    async_test=async_test_;
    db_->SetAsyncTest(async_test_);
  }
  void SetMeasurements(Measurements* m) override
  {
    measurements_=m;
    db_->SetMeasurements(m);
  }
  void CleanUpDirectly()override
  {
    db_->CleanUpDirectly();
  }
  Status Read(const std::string &table, const std::string &key,
              const std::vector<std::string> *fields, std::vector<Field> &result) {
    timer_.Start();
    Status s = db_->Read(table, key, fields, result);
    uint64_t elapsed = timer_.End();
    if (s == kOK) {
      measurements_->Report(READ, elapsed);
    } else {
      measurements_->Report(READ_FAILED, elapsed);
    }
    return s;
  }
  Status Read(const std::string &table, std::shared_ptr<std::string> key,
              const std::shared_ptr<std::vector<std::string>> fields, std::shared_ptr<std::vector<Field>> result,std::shared_ptr<Information> information) override{
    Status s = db_->Read(table, key, fields, result,information);
    return s;
  }
  Status Scan(const std::string &table, const std::string &key, int record_count,
              const std::vector<std::string> *fields, std::vector<std::vector<Field>> &result) {
    timer_.Start();
    Status s = db_->Scan(table, key, record_count, fields, result);
    uint64_t elapsed = timer_.End();
    if (s == kOK) {
      measurements_->Report(SCAN, elapsed);
    } else {
      measurements_->Report(SCAN_FAILED, elapsed);
    }
    return s;
  }
  Status Scan(const std::string &table, std::shared_ptr<std::string>key, int record_count,
              const std::shared_ptr<std::vector<std::string>> fields, std::shared_ptr<std::vector<std::vector<Field>>> result,std::shared_ptr<Information> information) override{
    Status s = db_->Scan(table, key, record_count, fields, result,information);
    return s;
  }
  Status Update(const std::string &table, const std::string &key, std::vector<Field> &values) {
    Status s = db_->Update(table, key, values);
    return s;
  }
  Status Insert(const std::string &table, const std::string &key, std::vector<Field> &values) {
    Status s = db_->Insert(table, key, values);
    return s;
  }
  Status Update(const std::string &table, std::shared_ptr<std::string> key, std::shared_ptr<std::vector<Field>> values,std::shared_ptr<Information> information) override {
    Status s = db_->Update(table, key, values,information);
    return s;
  }
  Status Insert(const std::string &table, std::shared_ptr<std::string> key, std::shared_ptr<std::vector<Field>>values,std::shared_ptr<Information> information) override {
    Status s = db_->Insert(table, key, values,information);
    return s;
  }
  Status Delete(const std::string &table, const std::string &key) {
    timer_.Start();
    Status s = db_->Delete(table, key);
    uint64_t elapsed = timer_.End();
    if (s == kOK) {
      measurements_->Report(DELETE, elapsed);
    } else {
      measurements_->Report(DELETE_FAILED, elapsed);
    }
    return s;
  }
  Status Delete(const std::string &table, std::shared_ptr<std::string> key,std::shared_ptr<Information> information) override
  {
    Status s=db_->Delete(table,key,information);
    return s;
  }
  void SetTaskPublisheer(DBTaskPublisher* d)
  {
    //task_publisher=d;
  }
 private:
  DB *db_;
  //Measurements *measurements_;
  utils::Timer<uint64_t, std::nano> timer_;
};


} // ycsbc

#endif
