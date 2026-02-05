#ifndef YCSB_C_ELASTIC_DB_H_
#define YCSB_C_ELASTIC_DB_H_

#include <string>
#include <mutex>

#include "core/db.h"
#include "utils/properties.h"

#include <rocksdb/elastic_lsm.h>

namespace ycsbc {

class ElasticDB : public DB {
 public:
  ElasticDB() {}
  ~ElasticDB() {}

  void Init();

  void Cleanup();

  void CleanUpDirectly()override
  {
    const std::lock_guard<std::mutex> lock(mu_);
    if (elastic_db_ && --ref_cnt_ == 0) {
      delete elastic_db_;
      elastic_db_ = nullptr;
    }
  }
  Status Read(const std::string &table, const std::string &key,
              const std::vector<std::string> *fields, std::vector<Field> &result) {
    //return (this->*(method_read_))(table, key, fields, result);
    return kOK;
  }
  Status Read(const std::string &table, std::shared_ptr<std::string> key,
              const std::shared_ptr<std::vector<std::string>> fields, std::shared_ptr<std::vector<Field>> result,std::shared_ptr<Information> information) override{
    return (this->*(method_read_))(table, key, fields, result,information);
  }
  Status Scan(const std::string &table, const std::string &key, int len,
              const std::vector<std::string>* fields, std::vector<std::vector<Field>> &result) {
    //return (this->*(method_scan_))(table, key, len, fields, result);
    return kOK;
  }
  Status Scan(const std::string &table, std::shared_ptr<std::string> key, int len,
              const std::shared_ptr<std::vector<std::string>> fields, std::shared_ptr<std::vector<std::vector<Field>>> result,std::shared_ptr<Information> information) override{
    return (this->*(method_scan_))(table, key, len, fields, result,information);
  }

  Status Update(const std::string &table, const std::string &key, std::vector<Field> &values) {
    //return (this->*(method_update_))(table, key, values);
    return kOK;
  }

  Status Insert(const std::string &table, const std::string &key, std::vector<Field> &values) {
    //return (this->*(method_insert_))(table, key, values);
    return kOK;
  }

  Status Delete(const std::string &table, const std::string &key) {
    //return (this->*(method_delete_))(table, key);
    return kOK;
  }
  Status Update(const std::string &table, std::shared_ptr<std::string> key, std::shared_ptr<std::vector<Field>> values,std::shared_ptr<Information> information) override {
    return (this->*(method_update_))(table, key, values,information);
  }

  Status Insert(const std::string &table, std::shared_ptr<std::string> key, std::shared_ptr<std::vector<Field>> values,std::shared_ptr<Information> information) override {
    return (this->*(method_insert_))(table, key, values,information);
  }

  Status Delete(const std::string &table, std::shared_ptr<std::string> key,std::shared_ptr<Information> information) override {
    return (this->*(method_delete_))(table, key,information);
  }


 private:
  enum RocksFormat {
    kSingleRow,
  };
  RocksFormat format_;

  void GetOptions(const utils::Properties &props, rocksdb::Options *opt,
                  std::vector<rocksdb::ColumnFamilyDescriptor> *cf_descs);
  void GetElasticOptions(const utils::Properties &props, rocksdb::ElasticLSMOptions *elastic_options);
  static void SerializeRow(const std::vector<Field> &values, std::string &data);
  static void DeserializeRowFilter(std::vector<Field> &values, const char *p, const char *lim,
                                   const std::vector<std::string> &fields);
  static void DeserializeRowFilter(std::vector<Field> &values, const std::string &data,
                                   const std::vector<std::string> &fields);
  static void DeserializeRow(std::vector<Field> &values, const char *p, const char *lim);
  static void DeserializeRow(std::vector<Field> &values, const std::string &data);

  Status ReadSingle(const std::string &table, std::shared_ptr<std::string> key,
                    const std::shared_ptr<std::vector<std::string>> fields, std::shared_ptr<std::vector<Field>> result,std::shared_ptr<Information> information);
  Status ScanSingle(const std::string &table, std::shared_ptr<std::string> key, int len,
                                 const std::shared_ptr<std::vector<std::string>> fields,
                                 std::shared_ptr<std::vector<std::vector<Field>>> result,std::shared_ptr<Information> information);
  Status UpdateSingle(const std::string &table, std::shared_ptr<std::string> key,
                                   std::shared_ptr<std::vector<Field>> values,std::shared_ptr<Information> information);
  Status MergeSingle(const std::string &table, std::shared_ptr<std::string> key,
                                  std::shared_ptr<std::vector<Field>> values,std::shared_ptr<Information> information);
  Status InsertSingle(const std::string &table, std::shared_ptr<std::string> key,
                                   std::shared_ptr<std::vector<Field>> values,std::shared_ptr<Information> information);
  Status DeleteSingle(const std::string &table, std::shared_ptr<std::string> key,std::shared_ptr<Information> information);

  Status (ElasticDB::*method_read_)(const std::string &, std::shared_ptr<std:: string>,
                                    const std::shared_ptr<std::vector<std::string>> , std::shared_ptr<std::vector<Field>>,std::shared_ptr<Information>);
  Status (ElasticDB::*method_scan_)(const std::string &, std::shared_ptr<std::string>,
                                    int, const std::shared_ptr<std::vector<std::string>> ,
                                    std::shared_ptr<std::vector<std::vector<Field>>>,std::shared_ptr<Information>);
  Status (ElasticDB::*method_update_)(const std::string &, std::shared_ptr<std::string>,
                                      std::shared_ptr<std::vector<Field>>,std::shared_ptr<Information>);
  Status (ElasticDB::*method_insert_)(const std::string &, std::shared_ptr<std::string>,
                                      std::shared_ptr<std::vector<Field>>,std::shared_ptr<Information>);
  Status (ElasticDB::*method_delete_)(const std::string &, std::shared_ptr<std::string>,std::shared_ptr<Information>);

  int fieldcount_;

  static std::vector<rocksdb::ColumnFamilyHandle *> cf_handles_;
  static rocksdb::ElasticLSM *elastic_db_;
  static int ref_cnt_;
  static std::mutex mu_;
  rocksdb::ReadOptions read_options_;
  rocksdb::WriteOptions write_options_;
};

DB *NewElasticDB();

} // ycsbc

#endif // YCSB_C_ELASTIC_DB_H_

