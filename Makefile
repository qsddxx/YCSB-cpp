PROJECT = ycsb-cpp
TARGET = ycsb
BUILD_DIR = build

GPERFTOOLS_ROOT = /home/liyixian/gperftools
GPERFTOOLS_INC = $(GPERFTOOLS_ROOT)/src
GPERFTOOLS_LIB = $(GPERFTOOLS_ROOT)/build
# ========== 强制链接的静态库 ==========
GFLAGS_STATIC_LIB = /usr/lib/x86_64-linux-gnu/libgflags.a
#GLOG_STATIC_LIB = /home/liyixian/glog-0.7.1/build/libglog.a
UNWIND_STATIC_LIB = /usr/lib/x86_64-linux-gnu/libunwind.a
FMT_STATIC_LIB = /home/liyixian/fmt-11.2.0/build/libfmt.a

# 验证静态库存在性
$(foreach lib, $(GFLAGS_STATIC_LIB) $(GLOG_STATIC_LIB) $(UNWIND_STATIC_LIB) $(FMT_STATIC_LIB), \
  $(if $(wildcard $(lib)),, $(info 警告: 静态库不存在：$(lib))))

# RocksDB 路径
ROCKSDB_HOME ?= /home/liyixian/rocksdb-10.7.5
ROCKSDB_LIB_DIR = $(ROCKSDB_HOME)/build_
ROCKSDB_INC_DIR = $(ROCKSDB_HOME)/include

# ========== 编译器设置 - 移除所有 PIE 相关选项 ==========
CXX = g++
CXX_STANDARD = 20
# 注意：移除了 -fPIE
CXXFLAGS = -std=c++$(CXX_STANDARD) -Wall -Werror -I.
CXXFLAGS += -I$(GPERFTOOLS_INC)
CXXFLAGS += -I$(ROCKSDB_INC_DIR) -I/usr/local/include -I$(HDRHISTOGRAM_DIR)/include
LDFLAGS = 
# 注意：移除了 -pie
LDFLAGS += -Wl,--no-as-needed -no-pie -fno-pie
LDFLAGS += -Wl,-rpath=/home/liyixian/gperftools/build
LDFLAGS += -L$(GPERFTOOLS_LIB)
LDFLAGS += -L/usr/local/lib -L/usr/lib/x86_64-linux-gnu

# SSE4.2 检测
UNAME_M := $(shell uname -m)
ifeq ($(filter x86_64 amd64 i386 i686,$(UNAME_M)),)
    SUPPORT_SSE42 = FALSE
    $(warning CPU architecture $(UNAME_M) does not support SSE4.2, skipping)
else
    SUPPORT_SSE42 = TRUE
    CXXFLAGS += -msse4.2
endif

# 构建选项
BIND_ROCKSDB ?= ON
USE_FOLLY ?= ON
WITH_ZLIB ?= ON
WITH_LZ4 ?= ON
WITH_SNAPPY ?= ON
WITH_ZSTD ?= ON
WITH_BZ2 ?= ON
HDRMEASUREMENT = 1

# 源文件
YCSB_CORE_SRC = $(wildcard core/*.cc)
ifeq ($(BIND_ROCKSDB),ON)
    YCSB_ROCKSDB_SRC = $(wildcard rocksdb/*.cc)
    YCSB_SRC += $(YCSB_ROCKSDB_SRC)
    WITH_ZLIB = ON
    WITH_BZ2 = ON
endif
YCSB_SRC += $(YCSB_CORE_SRC)
OBJ_FILES = $(patsubst %.cc,$(BUILD_DIR)/%.o,$(YCSB_SRC))

# 初始 LIBS
LIBS = -lprofiler -ltcmalloc -lpthread

# RocksDB 配置
ifeq ($(BIND_ROCKSDB),ON)
    ifeq ($(wildcard $(ROCKSDB_LIB_DIR)/librocksdb.a),)
        ROCKSDB_LIB = $(ROCKSDB_LIB_DIR)/librocksdb.so
    else
        ROCKSDB_LIB = $(ROCKSDB_LIB_DIR)/librocksdb.a
    endif
    LIBS += $(ROCKSDB_LIB) -luring -ldl
    $(info Using RocksDB: lib=$(ROCKSDB_LIB_DIR), include=$(ROCKSDB_INC_DIR))
endif

# 压缩库
ifeq ($(WITH_ZLIB),ON)
    LIBS += -lz
endif
ifeq ($(WITH_SNAPPY),ON)
    LIBS += -lsnappy
endif
ifeq ($(WITH_LZ4),ON)
    LIBS += -llz4
endif
ifeq ($(WITH_ZSTD),ON)
    LIBS += -lzstd
endif
ifeq ($(WITH_BZ2),ON)
    LIBS += -lbz2
endif

# Folly 及依赖链接
ifeq ($(USE_FOLLY),ON)
    LIBS += -levent -lboost_context -lboost_thread -lboost_system -lfolly
    LIBS += $(GFLAGS_STATIC_LIB) -lglog $(UNWIND_STATIC_LIB) -llzma
    LIBS += $(FMT_STATIC_LIB) -ldouble-conversion
    CXXFLAGS += -fexceptions -lfolly -lfmt -lunwind -ldl -lglog -lgflags -lpthread -ldouble-conversion
    LDFLAGS += $(EXTRA_LDFLAGS) -lfolly -lfmt -lunwind -ldl -lglog -lgflags -lpthread -ldouble-conversion
endif

# HdrHistogram 配置
HDRHISTOGRAM_DIR = HdrHistogram_c
HDRHISTOGRAM_SRC = $(wildcard $(HDRHISTOGRAM_DIR)/src/*.c)
HDRHISTOGRAM_OBJ = $(patsubst %.c,$(BUILD_DIR)/%.o,$(HDRHISTOGRAM_SRC))
LIBS += -L$(BUILD_DIR)/$(HDRHISTOGRAM_DIR)/src -lhdr_histogram_static

# 宏定义
ifeq ($(HDRMEASUREMENT),1)
    CXXFLAGS += -DHDRMEASUREMENT
endif

# 默认目标
all: $(BUILD_DIR)/$(TARGET)

# 生成可执行文件
$(BUILD_DIR)/$(TARGET): $(OBJ_FILES) $(BUILD_DIR)/$(HDRHISTOGRAM_DIR)/src/libhdr_histogram_static.a
	@mkdir -p $(dir $@)
	$(CXX) $(OBJ_FILES) -o $@ $(LDFLAGS) $(LIBS)
	@echo "Build completed: $@"

# C++ 目标文件编译
$(BUILD_DIR)/%.o: %.cc
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# HdrHistogram C 文件编译 - 移除了 -fPIE
$(BUILD_DIR)/$(HDRHISTOGRAM_DIR)/src/%.o: $(HDRHISTOGRAM_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) -I$(HDRHISTOGRAM_DIR)/include -c $< -o $@

# 构建 HdrHistogram 静态库
$(BUILD_DIR)/$(HDRHISTOGRAM_DIR)/src/libhdr_histogram_static.a: $(HDRHISTOGRAM_OBJ)
	@mkdir -p $(dir $@)
	ar rcs $@ $(HDRHISTOGRAM_OBJ)

clean:
	rm -rf $(BUILD_DIR)
	@echo "Clean completed"

debug:
	@echo "CXXFLAGS: $(CXXFLAGS)"
	@echo "LDFLAGS: $(LDFLAGS)"
	@echo "LIBS: $(LIBS)"

# 确保目录存在
$(shell mkdir -p $(BUILD_DIR)/core $(BUILD_DIR)/rocksdb $(BUILD_DIR)/$(HDRHISTOGRAM_DIR)/src)

# 显示构建选项
print-options:
	@echo "Build Options:"
	@echo "  BIND_ROCKSDB: $(BIND_ROCKSDB)"
	@echo "  ROCKSDB_HOME: $(ROCKSDB_HOME)"
	@echo "  USE_FOLLY: $(USE_FOLLY)"
	@echo "  SUPPORT_SSE42: $(SUPPORT_SSE42)"
