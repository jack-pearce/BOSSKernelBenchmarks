#ifndef BOSSBENCHMARKS_CONFIG_HPP
#define BOSSBENCHMARKS_CONFIG_HPP

#include "ITTNotifySupport.hpp"

extern VTuneAPIInterface vtune;

extern bool VERBOSE_QUERY_OUTPUT;
extern bool VERY_VERBOSE_QUERY_OUTPUT;
extern bool ENABLE_CONSTRAINTS;
extern int BENCHMARK_NUM_WARMPUP_ITERATIONS;

extern std::vector<std::string> librariesToTest;
extern int latestDataSize; // Scale factor for TPCH and num of elements for custom
extern std::string latestDataSet;

extern int googleBenchmarkApplyParameterHelper;

enum DATASETS { TPCH = 0, CUSTOM = 100 };

#endif // BOSSBENCHMARKS_CONFIG_HPP
