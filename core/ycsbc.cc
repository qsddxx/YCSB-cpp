//
//  ycsbc.cc
//  YCSB-cpp
//
//  Copyright (c) 2020 Youngjae Lee <ls4154.lee@gmail.com>.
//  Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>.
//

#include <cstring>
#include <ctime>

#include <string>
#include <iostream>
#include <vector>
#include <thread>
#include <future>
#include <chrono>
#include <iomanip>

#include "dbtaskpublisher.h"
#include "client.h"
#include "core_workload.h"
#include "db_factory.h"
#include "db_wrapper.h"
#include "measurements.h"
#include "utils/countdown_latch.h"
#include "utils/rate_limit.h"
#include "utils/timer.h"
#include "utils/utils.h"

void UsageMessage(const char *command);
bool StrStartWith(const char *str, const char *pre);
void ParseCommandLine(int argc, const char *argv[], ycsbc::utils::Properties &props);

void StatusThread(ycsbc::Measurements *measurements, ycsbc::utils::CountDownLatch *latch, int interval)
{
  using namespace std::chrono;
  time_point<system_clock> start = system_clock::now();
  bool done = false;
  while (1)
  {
    time_point<system_clock> now = system_clock::now();
    std::time_t now_c = system_clock::to_time_t(now);
    duration<double> elapsed_time = now - start;

    std::cout << std::put_time(std::localtime(&now_c), "%F %T") << ' '
              << static_cast<long long>(elapsed_time.count()) << " sec: ";

    std::cout << measurements->GetStatusMsg() << std::endl;

    if (done)
    {
      break;
    }
    done = latch->AwaitFor(interval);
  };
}

void RateLimitThread(std::string rate_file, std::vector<ycsbc::utils::RateLimiter *> rate_limiters,
                     ycsbc::utils::CountDownLatch *latch)
{
  std::ifstream ifs;
  ifs.open(rate_file);

  if (!ifs.is_open())
  {
    ycsbc::utils::Exception("failed to open: " + rate_file);
  }

  int64_t num_threads = rate_limiters.size();

  int64_t last_time = 0;
  while (!ifs.eof())
  {
    int64_t next_time;
    int64_t next_rate;
    ifs >> next_time >> next_rate;

    if (next_time <= last_time)
    {
      ycsbc::utils::Exception("invalid rate file");
    }

    bool done = latch->AwaitFor(next_time - last_time);
    if (done)
    {
      break;
    }
    last_time = next_time;
    for (auto x : rate_limiters)
    {
      x->SetRate(next_rate / num_threads);
    }
  }
}
void Read_Write_Proportion_Thread(std::string rate_file, ycsbc::CoreWorkload* cw, ycsbc::utils::CountDownLatch *latch)
{
  std::ifstream ifs;
  ifs.open(rate_file);
  std::cout<<"Rate file: "<<rate_file<<std::endl;

  if (!ifs.is_open())
  {
    ycsbc::utils::Exception("failed to open: " + rate_file);
  }

  int64_t last_time = 0;
  int64_t next_time;
  double next_read_rate;
  double next_update_rate;
  while (ifs >> next_time >> next_read_rate>>next_update_rate)
  {
    
    //ifs >> next_time >> next_read_rate>>next_update_rate;

    if (next_time <= last_time)
    {
      ycsbc::utils::Exception("invalid rate file");
    }
    std::cout << "Read success - Time: " << next_time 
              << ", Read rate: " << next_read_rate 
              << ", Update rate: " << next_update_rate << std::endl;

    bool done = latch->AwaitFor(next_time - last_time);
    if (done)
    {
      break;
    }
    last_time = next_time;
    cw->UpdateOperationProportions(next_read_rate,next_update_rate,0,0,0);
  }
}
int main(const int argc, const char *argv[])
{
  bool async_test = false;
  int num_executor_thread = 1;
  int producer_num=2;
  int num_per_batch=20;
  ycsbc::utils::Properties props;
  ParseCommandLine(argc, argv, props);
  producer_num = std::stoi(props.GetProperty("producer_num", "0"));
  num_per_batch = std::stoi(props.GetProperty("batch_num", "0"));

  const bool do_load = (props.GetProperty("doload", "false") == "true");
  const bool do_transaction = (props.GetProperty("dotransaction", "false") == "true");
  if (!do_load && !do_transaction)
  {
    std::cerr << "No operation to do" << std::endl;
    exit(1);
  }

  const int num_threads = stoi(props.GetProperty("threadcount", "1"));
  std::cout<<"Num thread: "<<num_threads<<std::endl;

  ycsbc::Measurements *measurements = ycsbc::CreateMeasurements(&props);
  if (measurements == nullptr)
  {
    std::cerr << "Unknown measurements name" << std::endl;
    exit(1);
  }

  std::vector<ycsbc::DB *> dbs;
  for (int i = 0; i < num_threads; i++)
  {
    ycsbc::DB *db = ycsbc::DBFactory::CreateDB(&props, measurements);
    if (db == nullptr)
    {
      std::cerr << "Unknown database name " << props["dbname"] << std::endl;
      exit(1);
    }
    dbs.push_back(db);
  }
  for (auto &d : dbs)
  {
    (*d).SetAsyncTest(async_test);
    (*d).SetMeasurements(measurements);
  }

  ycsbc::CoreWorkload wl;
  wl.Init(props);
  int load_total_ops_ = stoi(props[ycsbc::CoreWorkload::RECORD_COUNT_PROPERTY]);
  int64_t ops_limit_ = std::stoi(props.GetProperty("limit.ops", "0"));
  std::cout<<"Limit speed: "<<ops_limit_<<std::endl;
  int transaction_total_ops = stoi(props[ycsbc::CoreWorkload::OPERATION_COUNT_PROPERTY]);
  ycsbc::DBTaskPublisher task_publisher(load_total_ops_, transaction_total_ops, &wl, ops_limit_, num_threads, async_test, num_executor_thread, measurements,wl.GetCounterGenerator(),num_per_batch,producer_num);
  task_publisher.SetDB(&dbs);

  // print status periodically
  const bool show_status = (props.GetProperty("status", "false") == "true");
  const int status_interval = std::stoi(props.GetProperty("status.interval", "10"));

  // load phase
  std::string b="";
    b.clear();
    std::cout<<"Should the load begin?: "<<std::endl;;
    std::cin>>b;
  if (do_load)
  {
    task_publisher.Clear();
    const int total_ops = stoi(props[ycsbc::CoreWorkload::RECORD_COUNT_PROPERTY]);

    ycsbc::utils::CountDownLatch latch(num_threads);
    ycsbc::utils::Timer<double> timer;

    timer.Start();
    std::future<void> status_future;
    if (show_status)
    {
      //status_future = std::async(std::launch::async, StatusThread,
                                 //measurements, &latch, status_interval);
    }
    std::vector<std::future<void>> client_threads;
    const int64_t ops_limit = std::stoi(props.GetProperty("limit.ops", "0"));
    std::vector<ycsbc::utils::RateLimiter *> rate_limiters;
    std::string rate_file = props.GetProperty("limit.file", "");
    task_publisher.SetTimerList(total_ops);
    if (ops_limit > 0 || rate_file != "")
    {
      for(int j=0;j<producer_num;j++)
      {
        auto rlim = new ycsbc::utils::RateLimiter(ops_limit, ops_limit);
        task_publisher.SetRateLimiter(rlim);
        rate_limiters.emplace_back(rlim);
      }
      task_publisher.BeginLoading(true, !do_transaction);
    }
    else
    {
      task_publisher.SetRateLimiter(nullptr);
      task_publisher.BeginLoading(true, !do_transaction);
    }
    if (!async_test)
    {
      for (int i = 0; i < num_threads; ++i)
      {
        client_threads.emplace_back(std::async(std::launch::async, ycsbc::DoTaskSync, dbs[i], true, !do_transaction, &task_publisher, i,wl.GetCounterGenerator(),true,producer_num));
      }
    }
    else
    {
    }
    assert((int)client_threads.size() == num_threads);

    int sum = 0;
    while (task_publisher.total_complete_num.load() != total_ops)
    {
      //measurements->SetTaskNum(task_publisher.TaskList.size());
      usleep(1000000);
    }
    task_publisher.FinishReport(status_interval);
    sum = task_publisher.total_complete_num.load();
    latch.NotifyAll();
    double runtime = timer.End();

    if (show_status)
    {
      //status_future.wait();
    }
    if(async_test&&!do_transaction)
    {
      task_publisher.CleanUpDirectly();
    }

    std::cout << "Load runtime(sec): " << runtime << std::endl;
    std::cout << "Load operations(ops): " << sum << std::endl;
    std::cout << "Load throughput(ops/sec): " << sum / runtime << std::endl;
    task_publisher.Clear();
  }

  measurements->Reset();
  std::this_thread::sleep_for(std::chrono::seconds(stoi(props.GetProperty("sleepafterload", "0"))));
  std::string a="";
    a.clear();
    std::cout<<"Should the transaction begin?: "<<std::endl;;
    std::cin>>a;
  

  // transaction phase
  if (do_transaction)
  {
    // initial ops per second, unlimited if <= 0
    const int64_t ops_limit = std::stoi(props.GetProperty("limit.ops", "0"));
    // rate file path for dynamic rate limiting, format "time_stamp_sec new_ops_per_second" per line
    std::string rate_file = props.GetProperty("limit.file", "");
    std::string proportion_file=props.GetProperty("proportion.file","");
    std::cout<<"Proportion file: "<<proportion_file<<std::endl;

    const int total_ops = stoi(props[ycsbc::CoreWorkload::OPERATION_COUNT_PROPERTY]);

    ycsbc::utils::CountDownLatch latch(1);
    ycsbc::utils::Timer<double> timer;

    timer.Start();
    std::future<void> status_future;
    if (show_status)
    {
      //status_future = std::async(std::launch::async, StatusThread,
                                 //measurements, &latch, status_interval);
    }
    std::vector<std::future<void>> client_threads;
    std::vector<ycsbc::utils::RateLimiter *> rate_limiters;
    task_publisher.SetTimerList(total_ops);
    if (ops_limit > 0 || rate_file != "")
    {
      for(int j=0;j<producer_num;j++)
      {
        auto rlim = new ycsbc::utils::RateLimiter(ops_limit, ops_limit);
        task_publisher.SetRateLimiter(rlim);
        rate_limiters.emplace_back(rlim);
      }
      task_publisher.BeginTransaction(!do_load, true);
    }
    else
    {
      task_publisher.SetRateLimiter(nullptr);
      task_publisher.BeginTransaction(!do_load, true);
    }
    if (!async_test)
    {
      for (int i = 0; i < num_threads; ++i)
      {
        client_threads.emplace_back(std::async(std::launch::async, ycsbc::DoTaskSync, dbs[i], !do_load, true, &task_publisher, i,wl.GetCounterGenerator(),false,producer_num));
      }
    }

    std::future<void> rlim_future;
    if (rate_file != "")
    {
      rlim_future = std::async(std::launch::async, RateLimitThread, rate_file, rate_limiters, &latch);
    }
    if(proportion_file!="")
    {
      rlim_future = std::async(std::launch::async, Read_Write_Proportion_Thread, proportion_file, &wl, &latch);
    }

    assert((int)client_threads.size() == num_threads);

    int sum = 0;
    while (task_publisher.total_complete_num.load() != total_ops)
    {
      //measurements->SetTaskNum(task_publisher.TaskList.size());
      usleep(100000);
    }
    task_publisher.FinishReport(status_interval);
    sum = task_publisher.total_complete_num.load();
    latch.NotifyAll();
    double runtime = timer.End();

    if (show_status)
    {
      //status_future.wait();
    }

    std::cout << "Run runtime(sec): " << runtime << std::endl;
    std::cout << "Run operations(ops): " << sum << std::endl;
    std::cout << "Run throughput(ops/sec): " << sum / runtime << std::endl;
    task_publisher.Clear();
    if(async_test)
    {
      task_publisher.CleanUpDirectly();
    }
  }

  for (int i = 0; i < num_threads; i++)
  {
    delete dbs[i];
  }
  return 0;
}

void ParseCommandLine(int argc, const char *argv[], ycsbc::utils::Properties &props)
{
  int argindex = 1;
  while (argindex < argc && StrStartWith(argv[argindex], "-"))
  {
    if (strcmp(argv[argindex], "-load") == 0)
    {
      props.SetProperty("doload", "true");
      argindex++;
    }
    else if (strcmp(argv[argindex], "-run") == 0 || strcmp(argv[argindex], "-t") == 0)
    {
      props.SetProperty("dotransaction", "true");
      argindex++;
    }
    else if (strcmp(argv[argindex], "-threads") == 0)
    {
      argindex++;
      if (argindex >= argc)
      {
        UsageMessage(argv[0]);
        std::cerr << "Missing argument value for -threads" << std::endl;
        exit(0);
      }
      props.SetProperty("threadcount", argv[argindex]);
      argindex++;
    }
    else if (strcmp(argv[argindex], "-db") == 0)
    {
      argindex++;
      if (argindex >= argc)
      {
        UsageMessage(argv[0]);
        std::cerr << "Missing argument value for -db" << std::endl;
        exit(0);
      }
      props.SetProperty("dbname", argv[argindex]);
      argindex++;
    }
    else if (strcmp(argv[argindex], "-P") == 0)
    {
      argindex++;
      if (argindex >= argc)
      {
        UsageMessage(argv[0]);
        std::cerr << "Missing argument value for -P" << std::endl;
        exit(0);
      }
      std::string filename(argv[argindex]);
      std::ifstream input(argv[argindex]);
      try
      {
        props.Load(input);
      }
      catch (const std::string &message)
      {
        std::cerr << message << std::endl;
        exit(0);
      }
      input.close();
      argindex++;
    }
    else if (strcmp(argv[argindex], "-p") == 0)
    {
      argindex++;
      if (argindex >= argc)
      {
        UsageMessage(argv[0]);
        std::cerr << "Missing argument value for -p" << std::endl;
        exit(0);
      }
      std::string prop(argv[argindex]);
      size_t eq = prop.find('=');
      if (eq == std::string::npos)
      {
        std::cerr << "Argument '-p' expected to be in key=value format "
                     "(e.g., -p operationcount=99999)"
                  << std::endl;
        exit(0);
      }
      props.SetProperty(ycsbc::utils::Trim(prop.substr(0, eq)),
                        ycsbc::utils::Trim(prop.substr(eq + 1)));
      argindex++;
    }
    else if (strcmp(argv[argindex], "-s") == 0)
    {
      props.SetProperty("status", "true");
      argindex++;
    }
    else
    {
      UsageMessage(argv[0]);
      std::cerr << "Unknown option '" << argv[argindex] << "'" << std::endl;
      exit(0);
    }
  }

  if (argindex == 1 || argindex != argc)
  {
    UsageMessage(argv[0]);
    exit(0);
  }
}

void UsageMessage(const char *command)
{
  std::cout << "Usage: " << command << " [options]\n"
                                       "Options:\n"
                                       "  -load: run the loading phase of the workload\n"
                                       "  -t: run the transactions phase of the workload\n"
                                       "  -run: same as -t\n"
                                       "  -threads n: execute using n threads (default: 1)\n"
                                       "  -db dbname: specify the name of the DB to use (default: basic)\n"
                                       "  -P propertyfile: load properties from the given file. Multiple files can\n"
                                       "                   be specified, and will be processed in the order specified\n"
                                       "  -p name=value: specify a property to be passed to the DB and workloads\n"
                                       "                 multiple properties can be specified, and override any\n"
                                       "                 values in the propertyfile\n"
                                       "  -s: print status every 10 seconds (use status.interval prop to override)"
            << std::endl;
}

inline bool StrStartWith(const char *str, const char *pre)
{
  return strncmp(str, pre, strlen(pre)) == 0;
}
