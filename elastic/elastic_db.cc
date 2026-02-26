#include "elastic_db.h"

#include "core/core_workload.h"
#include "core/db_factory.h"
#include "utils/utils.h"

#include <rocksdb/cache.h>
#include <rocksdb/filter_policy.h>
#include <rocksdb/merge_operator.h>
#include <rocksdb/status.h>
#include <rocksdb/utilities/options_util.h>
#include <rocksdb/write_batch.h>

namespace {
  const std::string MAX_BACKGROUND_THREADS = "elastic.max_background_threads";
  const std::string MAX_BACKGROUND_THREADS_DEFAULT = "16";

  const std::string MIN_TP_THREADS = "elastic.min_tp_threads";
  const std::string MIN_TP_THREADS_DEFAULT = "2";

  const std::string MIN_AP_THREADS = "elastic.min_ap_threads";
  const std::string MIN_AP_THREADS_DEFAULT = "2";

  const std::string MIN_COMPACTION_THREADS = "elastic.min_compaction_threads";
  const std::string MIN_COMPACTION_THREADS_DEFAULT = "2";

  const std::string MAX_TP_TASK_QUEUE = "elastic.max_tp_task_queue";
  const std::string MAX_TP_TASK_QUEUE_DEFAULT = "131072";

  const std::string MAX_COMPACTION_NUM = "elastic.max_compaction_num";
  const std::string MAX_COMPACTION_NUM_DEFAULT = "32";

  const std::string COMPACTION_MORSEL_SIZE = "elastic.compaction_morsel_size";
  const std::string COMPACTION_MORSEL_SIZE_DEFAULT = "2000";

  const std::string TP_MORSEL_SIZE = "elastic.tp_morsel_size";
  const std::string TP_MORSEL_SIZE_DEFAULT = "2000";

  const std::string PROP_NAME = "elastic.dbname";
  const std::string PROP_NAME_DEFAULT = "/tmp/elastic_testdb";

  const std::string PROP_FORMAT = "elastic.format";
  const std::string PROP_FORMAT_DEFAULT = "single";

  const std::string PROP_MERGEUPDATE = "elastic.mergeupdate";
  const std::string PROP_MERGEUPDATE_DEFAULT = "false";

  const std::string PROP_DESTROY = "elastic.destroy";
  const std::string PROP_DESTROY_DEFAULT = "false";

  const std::string PROP_COMPRESSION = "elastic.compression";
  const std::string PROP_COMPRESSION_DEFAULT = "no";

  const std::string PROP_MAX_BG_JOBS = "elastic.max_background_jobs";
  const std::string PROP_MAX_BG_JOBS_DEFAULT = "0";

  const std::string PROP_TARGET_FILE_SIZE_BASE = "elastic.target_file_size_base";
  const std::string PROP_TARGET_FILE_SIZE_BASE_DEFAULT = "0";

  const std::string PROP_TARGET_FILE_SIZE_MULT = "elastic.target_file_size_multiplier";
  const std::string PROP_TARGET_FILE_SIZE_MULT_DEFAULT = "0";

  const std::string PROP_MAX_BYTES_FOR_LEVEL_BASE = "elastic.max_bytes_for_level_base";
  const std::string PROP_MAX_BYTES_FOR_LEVEL_BASE_DEFAULT = "0";

  const std::string PROP_WRITE_BUFFER_SIZE = "elastic.write_buffer_size";
  const std::string PROP_WRITE_BUFFER_SIZE_DEFAULT = "0";

  const std::string PROP_MAX_WRITE_BUFFER = "elastic.max_write_buffer_number";
  const std::string PROP_MAX_WRITE_BUFFER_DEFAULT = "0";

  const std::string PROP_COMPACTION_PRI = "elastic.compaction_pri";
  const std::string PROP_COMPACTION_PRI_DEFAULT = "-1";

  const std::string PROP_MAX_OPEN_FILES = "elastic.max_open_files";
  const std::string PROP_MAX_OPEN_FILES_DEFAULT = "-1";

  const std::string PROP_L0_COMPACTION_TRIGGER = "elastic.level0_file_num_compaction_trigger";
  const std::string PROP_L0_COMPACTION_TRIGGER_DEFAULT = "0";

  const std::string PROP_L0_SLOWDOWN_TRIGGER = "elastic.level0_slowdown_writes_trigger";
  const std::string PROP_L0_SLOWDOWN_TRIGGER_DEFAULT = "0";

  const std::string PROP_L0_STOP_TRIGGER = "elastic.level0_stop_writes_trigger";
  const std::string PROP_L0_STOP_TRIGGER_DEFAULT = "0";

  const std::string PROP_USE_DIRECT_WRITE = "elastic.use_direct_io_for_flush_compaction";
  const std::string PROP_USE_DIRECT_WRITE_DEFAULT = "false";

  const std::string PROP_USE_DIRECT_READ = "elastic.use_direct_reads";
  const std::string PROP_USE_DIRECT_READ_DEFAULT = "false";

  const std::string PROP_USE_MMAP_WRITE = "elastic.allow_mmap_writes";
  const std::string PROP_USE_MMAP_WRITE_DEFAULT = "false";

  const std::string PROP_USE_MMAP_READ = "elastic.allow_mmap_reads";
  const std::string PROP_USE_MMAP_READ_DEFAULT = "false";

  const std::string PROP_CACHE_SIZE = "elastic.cache_size";
  const std::string PROP_CACHE_SIZE_DEFAULT = "0";

  const std::string PROP_COMPRESSED_CACHE_SIZE = "elastic.compressed_cache_size";
  const std::string PROP_COMPRESSED_CACHE_SIZE_DEFAULT = "0";

  const std::string PROP_BLOOM_BITS = "elastic.bloom_bits";
  const std::string PROP_BLOOM_BITS_DEFAULT = "0";

  const std::string PROP_INCREASE_PARALLELISM = "elastic.increase_parallelism";
  const std::string PROP_INCREASE_PARALLELISM_DEFAULT = "false";

  const std::string PROP_OPTIMIZE_LEVELCOMP = "elastic.optimize_level_style_compaction";
  const std::string PROP_OPTIMIZE_LEVELCOMP_DEFAULT = "false";

  const std::string PROP_OPTIONS_FILE = "elastic.optionsfile";
  const std::string PROP_OPTIONS_FILE_DEFAULT = "";

  const std::string PROP_ENV_URI = "elastic.env_uri";
  const std::string PROP_ENV_URI_DEFAULT = "";

  const std::string PROP_FS_URI = "elastic.fs_uri";
  const std::string PROP_FS_URI_DEFAULT = "";

  static std::shared_ptr<rocksdb::Env> env_guard;
  static std::shared_ptr<rocksdb::Cache> block_cache;
#if ROCKSDB_MAJOR < 8
  static std::shared_ptr<rocksdb::Cache> block_cache_compressed;
#endif
} // anonymous

namespace ycsbc {

std::vector<rocksdb::ColumnFamilyHandle *> ElasticDB::cf_handles_;
rocksdb::ElasticLSM *ElasticDB::elastic_db_ = nullptr;
int ElasticDB::ref_cnt_ = 0;
std::mutex ElasticDB::mu_;
void ElasticDB::Init() {
// merge operator disabled by default due to link error
#ifdef USE_MERGEUPDATE
  class YCSBUpdateMerge : public rocksdb::AssociativeMergeOperator {
   public:
    virtual bool Merge(const rocksdb::Slice &key, const rocksdb::Slice *existing_value,
                       const rocksdb::Slice &value, std::string *new_value,
                       rocksdb::Logger *logger) const override {
      assert(existing_value);

      std::vector<Field> values;
      const char *p = existing_value->data();
      const char *lim = p + existing_value->size();
      DeserializeRow(values, p, lim);

      std::vector<Field> new_values;
      p = value.data();
      lim = p + value.size();
      DeserializeRow(new_values, p, lim);

      for (Field &new_field : new_values) {
        bool found = false;
        for (Field &field : values) {
          if (field.name == new_field.name) {
            found = true;
            field.value = new_field.value;
            break;
          }
        }
        if (!found) {
          values.push_back(new_field);
        }
      }

      SerializeRow(values, *new_value);
      return true;
    }

    virtual const char *Name() const override {
      return "YCSBUpdateMerge";
    }
  };
#endif

  const utils::Properties &props = *props_;
  const std::string format = props.GetProperty(PROP_FORMAT, PROP_FORMAT_DEFAULT);
  if (format == "single") {
    format_ = kSingleRow;
    method_read_ = &ElasticDB::ReadSingle;
    method_scan_ = &ElasticDB::ScanSingle;
    method_update_ = &ElasticDB::UpdateSingle;
    method_insert_ = &ElasticDB::InsertSingle;
    method_delete_ = &ElasticDB::DeleteSingle;
#ifdef USE_MERGEUPDATE
    if (props.GetProperty(PROP_MERGEUPDATE, PROP_MERGEUPDATE_DEFAULT) == "true") {
      method_update_ = &ElasticDB::MergeSingle;
    }
#endif
  } else {
    throw utils::Exception("unknown format");
  }
  fieldcount_ = std::stoi(props.GetProperty(CoreWorkload::FIELD_COUNT_PROPERTY,
                                            CoreWorkload::FIELD_COUNT_DEFAULT));

  const std::lock_guard<std::mutex> lock(mu_);
  ref_cnt_++;
  async_test = true;
  if (elastic_db_) {
    return;
  }

  const std::string &elastic_db_path = props.GetProperty(PROP_NAME, PROP_NAME_DEFAULT);
  if (elastic_db_path == "") {
    throw utils::Exception("ElasticLSM db path is missing");
  }

  rocksdb::Options opt;
  opt.create_if_missing = true;
  std::vector<rocksdb::ColumnFamilyDescriptor> cf_descs;
  GetOptions(props, &opt, &cf_descs);
#ifdef USE_MERGEUPDATE
  opt.merge_operator.reset(new YCSBUpdateMerge);
#endif

  rocksdb::Status s;
  if (props.GetProperty(PROP_DESTROY, PROP_DESTROY_DEFAULT) == "true") {
    s = rocksdb::DestroyDB(elastic_db_path, opt);
    if (!s.ok()) {
      throw utils::Exception(std::string("ElasticLSM DestroyDB: ") + s.ToString());
    }
  }

  rocksdb::ElasticLSMOptions elastic_options;
  GetElasticOptions(props, &elastic_options);

  if (cf_descs.empty()) {
    s = rocksdb::ElasticLSM::Open(opt, elastic_options, elastic_db_path, &elastic_db_);
  } else {
    s = rocksdb::ElasticLSM::Open(
        opt, elastic_options, elastic_db_path, cf_descs,
        &cf_handles_, &elastic_db_);
  }
  if (!s.ok()) {
    throw utils::Exception(std::string("ElasticLSM Open: ") + s.ToString());
  }
}

void ElasticDB::Cleanup() { 
  const std::lock_guard<std::mutex> lock(mu_);
  if (--ref_cnt_) {
    return;
  }
  for (size_t i = 0; i < cf_handles_.size(); i++) {
    if (cf_handles_[i] != nullptr) {
      elastic_db_->DestroyColumnFamilyHandle(cf_handles_[i]);
      cf_handles_[i] = nullptr;
    }
  }
  delete elastic_db_;
}

void ElasticDB::GetOptions(const utils::Properties &props, rocksdb::Options *opt,
                           std::vector<rocksdb::ColumnFamilyDescriptor> *cf_descs) {
  std::string env_uri = props.GetProperty(PROP_ENV_URI, PROP_ENV_URI_DEFAULT);
  std::string fs_uri = props.GetProperty(PROP_FS_URI, PROP_FS_URI_DEFAULT);
  rocksdb::Env* env =  rocksdb::Env::Default();;
  if (!env_uri.empty() || !fs_uri.empty()) {
    rocksdb::Status s = rocksdb::Env::CreateFromUri(rocksdb::ConfigOptions(),
                                                    env_uri, fs_uri, &env, &env_guard);
    if (!s.ok()) {
      throw utils::Exception(std::string("RocksDB CreateFromUri: ") + s.ToString());
    }
    opt->env = env;
  }

  const std::string options_file = props.GetProperty(PROP_OPTIONS_FILE, PROP_OPTIONS_FILE_DEFAULT);
  if (options_file != "") {
    rocksdb::ConfigOptions config_options;
    config_options.ignore_unknown_options = false;
    config_options.input_strings_escaped = true;
    config_options.env = env;
    rocksdb::Status s = rocksdb::LoadOptionsFromFile(config_options, options_file, opt, cf_descs);
    if (!s.ok()) {
      throw utils::Exception(std::string("RocksDB LoadOptionsFromFile: ") + s.ToString());
    }
  } else {
    const std::string compression_type = props.GetProperty(PROP_COMPRESSION,
                                                           PROP_COMPRESSION_DEFAULT);
    if (compression_type == "no") {
      opt->compression = rocksdb::kNoCompression;
    } else if (compression_type == "snappy") {
      opt->compression = rocksdb::kSnappyCompression;
    } else if (compression_type == "zlib") {
      opt->compression = rocksdb::kZlibCompression;
    } else if (compression_type == "bzip2") {
      opt->compression = rocksdb::kBZip2Compression;
    } else if (compression_type == "lz4") {
      opt->compression = rocksdb::kLZ4Compression;
    } else if (compression_type == "lz4hc") {
      opt->compression = rocksdb::kLZ4HCCompression;
    } else if (compression_type == "xpress") {
      opt->compression = rocksdb::kXpressCompression;
    } else if (compression_type == "zstd") {
      opt->compression = rocksdb::kZSTD;
    } else {
      throw utils::Exception("Unknown compression type");
    }

    int val = std::stoi(props.GetProperty(PROP_MAX_BG_JOBS, PROP_MAX_BG_JOBS_DEFAULT));
    if (val != 0) {
      opt->max_background_jobs = val;
    }
    val = std::stoi(props.GetProperty(PROP_TARGET_FILE_SIZE_BASE, PROP_TARGET_FILE_SIZE_BASE_DEFAULT));
    if (val != 0) {
      opt->target_file_size_base = val;
    }
    val = std::stoi(props.GetProperty(PROP_TARGET_FILE_SIZE_MULT, PROP_TARGET_FILE_SIZE_MULT_DEFAULT));
    if (val != 0) {
      opt->target_file_size_multiplier = val;
    }
    val = std::stoi(props.GetProperty(PROP_MAX_BYTES_FOR_LEVEL_BASE, PROP_MAX_BYTES_FOR_LEVEL_BASE_DEFAULT));
    if (val != 0) {
      opt->max_bytes_for_level_base = val;
    }
    val = std::stoi(props.GetProperty(PROP_WRITE_BUFFER_SIZE, PROP_WRITE_BUFFER_SIZE_DEFAULT));
    if (val != 0) {
      opt->write_buffer_size = val;
    }
    val = std::stoi(props.GetProperty(PROP_MAX_WRITE_BUFFER, PROP_MAX_WRITE_BUFFER_DEFAULT));
    if (val != 0) {
      opt->max_write_buffer_number = val;
    }
    val = std::stoi(props.GetProperty(PROP_COMPACTION_PRI, PROP_COMPACTION_PRI_DEFAULT));
    if (val != -1) {
      opt->compaction_pri = static_cast<rocksdb::CompactionPri>(val);
    }
    val = std::stoi(props.GetProperty(PROP_MAX_OPEN_FILES, PROP_MAX_OPEN_FILES_DEFAULT));
    if (val != 0) {
      opt->max_open_files = val;
    }

    val = std::stoi(props.GetProperty(PROP_L0_COMPACTION_TRIGGER, PROP_L0_COMPACTION_TRIGGER_DEFAULT));
    if (val != 0) {
      opt->level0_file_num_compaction_trigger = val;
    }
    val = std::stoi(props.GetProperty(PROP_L0_SLOWDOWN_TRIGGER, PROP_L0_SLOWDOWN_TRIGGER_DEFAULT));
    if (val != 0) {
      opt->level0_slowdown_writes_trigger = val;
    }
    val = std::stoi(props.GetProperty(PROP_L0_STOP_TRIGGER, PROP_L0_STOP_TRIGGER_DEFAULT));
    if (val != 0) {
      opt->level0_stop_writes_trigger = val;
    }

    if (props.GetProperty(PROP_USE_DIRECT_WRITE, PROP_USE_DIRECT_WRITE_DEFAULT) == "true") {
      opt->use_direct_io_for_flush_and_compaction = true;
    }
    if (props.GetProperty(PROP_USE_DIRECT_READ, PROP_USE_DIRECT_READ_DEFAULT) == "true") {
      opt->use_direct_reads = true;
    }
    if (props.GetProperty(PROP_USE_MMAP_WRITE, PROP_USE_MMAP_WRITE_DEFAULT) == "true") {
      opt->allow_mmap_writes = true;
    }
    if (props.GetProperty(PROP_USE_MMAP_READ, PROP_USE_MMAP_READ_DEFAULT) == "true") {
      opt->allow_mmap_reads = true;
    }

    rocksdb::BlockBasedTableOptions table_options;
    size_t cache_size = std::stoul(props.GetProperty(PROP_CACHE_SIZE, PROP_CACHE_SIZE_DEFAULT));
    if (cache_size > 0) {
      block_cache = rocksdb::NewLRUCache(cache_size);
      table_options.block_cache = block_cache;
    }
#if ROCKSDB_MAJOR < 8
    size_t compressed_cache_size = std::stoul(props.GetProperty(PROP_COMPRESSED_CACHE_SIZE,
                                                                PROP_COMPRESSED_CACHE_SIZE_DEFAULT));
    if (compressed_cache_size > 0) {
      block_cache_compressed = rocksdb::NewLRUCache(compressed_cache_size);
      table_options.block_cache_compressed = block_cache_compressed;
    }
#endif
    int bloom_bits = std::stoul(props.GetProperty(PROP_BLOOM_BITS, PROP_BLOOM_BITS_DEFAULT));
    if (bloom_bits > 0) {
      table_options.filter_policy.reset(rocksdb::NewBloomFilterPolicy(bloom_bits));
    }
    opt->table_factory.reset(rocksdb::NewBlockBasedTableFactory(table_options));

    if (props.GetProperty(PROP_INCREASE_PARALLELISM, PROP_INCREASE_PARALLELISM_DEFAULT) == "true") {
      opt->IncreaseParallelism();
    }
    if (props.GetProperty(PROP_OPTIMIZE_LEVELCOMP, PROP_OPTIMIZE_LEVELCOMP_DEFAULT) == "true") {
      opt->OptimizeLevelStyleCompaction();
    }
  }
}

void ElasticDB::GetElasticOptions(const utils::Properties &props, rocksdb::ElasticLSMOptions *elastic_options)
{
  elastic_options->max_background_threads = std::stoi(
      props.GetProperty(MAX_BACKGROUND_THREADS, MAX_BACKGROUND_THREADS_DEFAULT));
  elastic_options->min_tp_threads = std::stoi(
      props.GetProperty(MIN_TP_THREADS, MIN_TP_THREADS_DEFAULT));
  elastic_options->min_ap_threads = std::stoi(
      props.GetProperty(MIN_AP_THREADS, MIN_AP_THREADS_DEFAULT));
  elastic_options->min_compaction_threads = std::stoi(
      props.GetProperty(MIN_COMPACTION_THREADS, MIN_COMPACTION_THREADS_DEFAULT));
  elastic_options->max_tp_task_queue = std::stoi(
      props.GetProperty(MAX_TP_TASK_QUEUE, MAX_TP_TASK_QUEUE_DEFAULT));
  elastic_options->max_compaction_num = std::stoi(
      props.GetProperty(MAX_COMPACTION_NUM, MAX_COMPACTION_NUM_DEFAULT));
  elastic_options->compaction_morsel_size = std::stoi(
      props.GetProperty(COMPACTION_MORSEL_SIZE, COMPACTION_MORSEL_SIZE_DEFAULT));
  elastic_options->tp_morsel_size = std::stoi(
      props.GetProperty(TP_MORSEL_SIZE, TP_MORSEL_SIZE_DEFAULT));
}

void ElasticDB::SerializeRow(const std::vector<Field> &values, std::string &data) {
  for (const Field &field : values) {
    uint32_t len = field.name.size();
    data.append(reinterpret_cast<char *>(&len), sizeof(uint32_t));
    data.append(field.name.data(), field.name.size());
    len = field.value.size();
    data.append(reinterpret_cast<char *>(&len), sizeof(uint32_t));
    data.append(field.value.data(), field.value.size());
  }
}

void ElasticDB::DeserializeRowFilter(std::vector<Field> &values, const char *p, const char *lim,
                                     const std::vector<std::string> &fields) {
  std::vector<std::string>::const_iterator filter_iter = fields.begin();
  while (p != lim && filter_iter != fields.end()) {
    assert(p < lim);
    uint32_t len = *reinterpret_cast<const uint32_t *>(p);
    p += sizeof(uint32_t);
    std::string field(p, static_cast<const size_t>(len));
    p += len;
    len = *reinterpret_cast<const uint32_t *>(p);
    p += sizeof(uint32_t);
    std::string value(p, static_cast<const size_t>(len));
    p += len;
    if (*filter_iter == field) {
      values.push_back({field, value});
      filter_iter++;
    }
  }
  assert(values.size() == fields.size());
}

void ElasticDB::DeserializeRowFilter(std::vector<Field> &values, const std::string &data,
                                     const std::vector<std::string> &fields) {
  const char *p = data.data();
  const char *lim = p + data.size();
  DeserializeRowFilter(values, p, lim, fields);
}

void ElasticDB::DeserializeRow(std::vector<Field> &values, const char *p, const char *lim) {
  while (p != lim) {
    assert(p < lim);
    uint32_t len = *reinterpret_cast<const uint32_t *>(p);
    p += sizeof(uint32_t);
    std::string field(p, static_cast<const size_t>(len));
    p += len;
    len = *reinterpret_cast<const uint32_t *>(p);
    p += sizeof(uint32_t);
    std::string value(p, static_cast<const size_t>(len));
    p += len;
    values.push_back({field, value});
  }
}

void ElasticDB::DeserializeRow(std::vector<Field> &values, const std::string &data) {
  const char *p = data.data();
  const char *lim = p + data.size();
  DeserializeRow(values, p, lim);
}

DB::Status ElasticDB::ReadSingle(const std::string &table, std::shared_ptr<std::string> key,
                                 const std::shared_ptr<std::vector<std::string>> fields,
                                 std::shared_ptr<std::vector<Field>> result,std::shared_ptr<Information> information) {
  if(async_test)
  {
    auto callback = new std::function<void()>(
      [information, key, fields, result, this]() {
        if (fields != nullptr) {
          DeserializeRowFilter(*result, information->answer, *fields);
        } else {
          DeserializeRow(*result, information->answer);
          assert((*result).size() == static_cast<size_t>(fieldcount_));
        }
        *information->end_time = Clock::now();
        information->total_complete_num->fetch_add(1);
      });
    rocksdb::Status s = elastic_db_->Get(read_options_, *key, &information->answer, callback);
    if (s.IsNotFound()) {
      return kNotFound;
    } else if (!s.ok()) {
      throw utils::Exception(std::string("Elastic Get: ") + s.ToString());
    }
    return kOK;
  }
  else
  {
    return kOK;
  }
}
DB::Status ElasticDB::ScanSingle(const std::string &table, std::shared_ptr<std::string> key, int len,
                                 const std::shared_ptr<std::vector<std::string>> fields,
                                 std::shared_ptr<std::vector<std::vector<Field>>> result,std::shared_ptr<Information> information) {
  if(async_test)
  {
    auto func = new std::function<void(rocksdb::Iterator *)>(
      [key, len, fields, result, this](rocksdb::Iterator *db_iter){
        db_iter->Seek(*key);
        for (int i = 0; db_iter->Valid() && i < len; i++) {
          std::string data = db_iter->value().ToString();
          result->push_back(std::vector<Field>());
          std::vector<Field> &values = result->back();
          if (fields != nullptr) {
            DeserializeRowFilter(values, data, *fields);
          } else {
            DeserializeRow(values, data);
            assert(values.size() == static_cast<size_t>(fieldcount_));
          }
          db_iter->Next();
        }
      }
    );
    auto callback = new std::function<void()>(
      [information]() {
        *information->end_time = Clock::now();
        information->total_complete_num->fetch_add(1);
      });
    rocksdb::Status s = elastic_db_->Scan(read_options_, func, callback);
    return kOK;
  }
  else
  {
    return kOK;
  }
}

DB::Status ElasticDB::UpdateSingle(const std::string &table, std::shared_ptr<std::string> key,
                                   std::shared_ptr<std::vector<Field>> values,std::shared_ptr<Information> information) {
  if(async_test)
  {
    auto midcallback = new std::function<bool()>(
      [information, values, this]() {
        std::vector<Field> current_values;
        DeserializeRow(current_values, information->answer);
        assert(current_values.size() == static_cast<size_t>(fieldcount_));
        for (Field &new_field : (*values)) {
          bool found MAYBE_UNUSED = false;
          for (Field &cur_field : current_values) {
            if (cur_field.name == new_field.name) {
              found = true;
              cur_field.value = new_field.value;
              break;
            }
          }
          assert(found);
        }
        information->answer.clear();
        SerializeRow(current_values, information->answer);
        return true;
      });
    auto callbackend = new std::function<void()>(
        [information, key]() {
          *information->end_time = Clock::now();
          information->total_complete_num->fetch_add(1);
        });
    rocksdb::Status s = elastic_db_->Update(read_options_, write_options_, *key, &information->answer, midcallback, callbackend);
    return kOK;
  }
  else
  {
    return kOK;
  }
}

DB::Status ElasticDB::MergeSingle(const std::string &table, std::shared_ptr<std::string> key,
                                  std::shared_ptr<std::vector<Field>> values,std::shared_ptr<Information> information) {
  if(async_test)
  {
    return kOK;
  }
  else
  {
    return kOK;
  }
}

DB::Status ElasticDB::InsertSingle(const std::string &table, std::shared_ptr<std::string> key,
                                   std::shared_ptr<std::vector<Field>> values,std::shared_ptr<Information> information) {
  
  if(async_test)
  {
    information->answer.clear();
    SerializeRow(*values, information->answer);
    auto callback = new std::function<void()>(
      [information, key]() {
        *information->end_time = Clock::now();
        information->total_complete_num->fetch_add(1);
      });
    elastic_db_->Put(write_options_, *key, information->answer, callback);
    return kOK;
  }
  else
  {
    return kOK;
  }
}

DB::Status ElasticDB::DeleteSingle(const std::string &table, std::shared_ptr<std::string> key,std::shared_ptr<Information> information) {
  if(async_test)
  {
    auto callback = new std::function<void()>(
      [information, key]() {
        *information->end_time = Clock::now();
        information->total_complete_num->fetch_add(1);
      });
    rocksdb::Status s = elastic_db_->Delete(write_options_, *key, callback);
    if (!s.ok()) {
      throw utils::Exception(std::string("RocksDB Delete: ") + s.ToString());
    }
    return kOK;
  }
  else
  {
    return kOK;
  }
}

DB *NewElasticDB() {
  return new ElasticDB;
}

const bool registered = DBFactory::RegisterDB("elastic", NewElasticDB);
} // ycsbc
