# Copilot instructions for YCSB-cpp (ycsb-elestic fork)

These notes highlight repository-specific architecture, build/run developer flows, and common patterns that an AI coding agent should follow when making edits.

1. Big-picture architecture
  - The executable `ycsb` is built from `core/*.cc` (see `CMakeLists.txt` using `file(GLOB_RECURSE YCSB_CORE_SRC "core/*.cc")`).
  - Runtime components:
    - `core/ycsbc.cc` is the entrypoint: parses CLI properties and wires workload, DB instances and measurements.
    - `core/db.h` defines the `DB` interface (sync and async flavors) and `DB::Task` structure.
    - `core/db_factory.h` + per-backend files (e.g. `leveldb/leveldb_db.cc`) implement DB backends and register them with `DBFactory::RegisterDB("name", NewXDB)`.
    - `core/dbtaskpublisher.h` orchestrates task generation, rate limiting, and measurement reporting (uses Folly `Promise`/`Future` and `CPUThreadPoolExecutor`).

2. How data and control flow work (quick summary)
  - `CoreWorkload` generates tasks (insert/transaction) that `DBTaskPublisher` batches.
  - `DBTaskPublisher` either enqueues tasks on a `folly::UnboundedQueue` (sync worker threads consume them) or calls `DB::DoTaskAsync(...)` for in-process async execution.
  - Each task gets a `folly::Promise` whose future is scheduled on `executor_` to record end-time and report to `Measurements` (see `GeneratePromise` in `core/dbtaskpublisher.h`).

3. Build / test / run workflows (explicit examples)
  - CMake build (recommended):
    - git submodule update --init
    - mkdir build && cd build
    - cmake -DBIND_LEVELDB=1 -DBIND_ROCKSDB=1 -DWITH_SNAPPY=1 ..
    - make
  - Makefile build (POSIX):
    - make BIND_LEVELDB=1
    - or set EXTRA_CXXFLAGS / EXTRA_LDFLAGS as in `README.md` if libs are in non-standard locations.
  - Run examples (from `README.md`):
    - ./ycsb -load -db leveldb -P workloads/workloada -P leveldb/leveldb.properties -s
    - ./ycsb -run -db leveldb -P workloads/workloada -p threadcount=4 -s

4. Project-specific conventions & patterns to preserve
  - DB registration: backends expose `DB *NewXDB()` and register via `DBFactory::RegisterDB("name", NewXDB);` (see `leveldb/leveldb_db.cc`).
  - Property-driven configuration: runtime flags come from `-P` property files and `-p name=value`. Refer to `core/ycsbc.cc` for property names used (e.g., `limit.ops`, `limit.file`, `status.interval`).
  - Two execution models: async vs sync. `DB::SetAsyncTest(bool)` toggles behavior; `DBTaskPublisher` has branches for `async_test` which change how tasks are executed and cleaned up. Be careful editing code that assumes one model.
  - Measurement path: tasks create `folly::Promise` and use `executor_` to call `Measurements::Report(...)`. Keep promise/future lifetimes intact when modifying task flow.
  - Resource sharing: many DB backends use static singletons and ref-counting (see `LeveldbDB::Init()` / `Cleanup()`), so be careful with destructor/cleanup changes.

5. Integration points, dependencies and where to look
  - CMake glue and options: `CMakeLists.txt` (controls BIND_* options and links to gflags, folly, RocksDB, LevelDB, liburing, etc.).
  - Per-backend config and properties: `leveldb/leveldb.properties`, `rocksdb/rocksdb.properties`, etc.
  - Workloads are in `workloads/` (e.g., `workloada`) and `CoreWorkload` is under `core/core_workload.*` — these shape the generated tasks.
  - Measurements / histogram: `HdrHistogram_c` is vendored as a subdirectory and linked; measurements code is under `core/measurements.*`.

6. Common edits the agent may be asked to do (and pitfalls)
  - Adding a new DB backend: implement `DB` methods, provide `NewXDB()` and register in the same pattern as `leveldb/leveldb_db.cc`; add CMake option `BIND_X` and follow `CMakeLists.txt` search/link patterns.
  - Changing task/measurement timing: modify `core/dbtaskpublisher.h` and `core/ycsbc.cc` together; preserve the Promise -> Future -> executor -> Measurements chain.
  - Changing CLI flags or properties: update `core/ycsbc.cc` parsing and add default values to per-db `*.properties` where appropriate.

7. Where to add tests / validations
  - There are no unit tests in-tree; prefer adding small integration harnesses under `test/` that exercise `DB` backends using sample `workloads/` props and assert measurement invariants.

If anything above is unclear or you want more detail on a section (for example: exact property names, DB registration internals, or the async flow in `DB::Task`), tell me which part to expand. I can iterate the file with concrete examples and links to exact lines in the code.
