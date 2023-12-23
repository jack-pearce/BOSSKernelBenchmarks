#include <BOSS.hpp>
#include <ExpressionUtilities.hpp>

#include "config.hpp"
#include "dataGeneration.cpp"
#include "select.cpp"
#include "tpch.cpp"
#include "utilities.cpp"
#include <benchmark/benchmark.h>
#include <iostream>
#include <string>
#include <vector>

using namespace boss::utilities;

VTuneAPIInterface vtune{"BOSS"};

bool VERBOSE_QUERY_OUTPUT = false;
bool VERY_VERBOSE_QUERY_OUTPUT = false;
bool ENABLE_CONSTRAINTS = false;
int BENCHMARK_NUM_WARMPUP_ITERATIONS = 0;

std::vector<std::string> librariesToTest = {};
int latestDataSize = -1;
std::string latestDataSet;

int googleBenchmarkApplyParameterHelper = -1;

static void releaseBOSSEngines() {
  // make sure to release engines in reverse order of evaluation
  // (important for data ownership across engines)
  auto reversedLibraries = librariesToTest;
  std::reverse(reversedLibraries.begin(), reversedLibraries.end());
  boss::expressions::ExpressionSpanArguments spans;
  spans.emplace_back(boss::expressions::Span<std::string>(reversedLibraries));
  boss::evaluate("ReleaseEngines"_(boss::ComplexExpression("List"_, {}, {}, std::move(spans))));
}

void initAndRunBenchmarks(int argc, char** argv) {
  for(int i = 0; i < argc; ++i) {
    if(std::string("--library") == argv[i]) {
      if(++i < argc) {
        librariesToTest.emplace_back(argv[i]);
      }
    } else if(std::string("--benchmark-num-warmup-iterations") == argv[i]) {
      if(++i < argc) {
        BENCHMARK_NUM_WARMPUP_ITERATIONS = atoi(argv[i]);
      }
    } else if(std::string("--verbose-query-output") == argv[i] || std::string("-v") == argv[i]) {
      VERBOSE_QUERY_OUTPUT = true;
    } else if(std::string("--very-verbose-query-output") == argv[i] ||
              std::string("-vv") == argv[i]) {
      VERBOSE_QUERY_OUTPUT = true;
      VERY_VERBOSE_QUERY_OUTPUT = true;
    } else if(std::string("--enable-constraints") == argv[i]) {
      ENABLE_CONSTRAINTS = true;
    }
  }

  // register TPC-H benchmarks
  for(int dataSize : std::vector<int>{1, 10, 100, 1000}) {
    for(int query : std::vector<int>{TPCH_Q6}) {
      std::ostringstream testName;
      auto const& queryName = tpchQueryNames()[DATASETS::TPCH + query];
      testName << queryName << "/";
      testName << dataSize << "MB";
      benchmark::RegisterBenchmark(testName.str(), TPCH_Benchmark, DATASETS::TPCH + query, dataSize)
          ->MeasureProcessCPUTime()
          ->UseRealTime();
    }
  }

  // register TPC-H Q6 clustering benchmarks
  {
    int dataSize = 1000;
    googleBenchmarkApplyParameterHelper = dataSize;
    std::ostringstream testName;
    auto const& queryName =
        tpchQueryNames()[static_cast<int>(DATASETS::TPCH) + static_cast<int>(TPCH_Q6)];
    testName << queryName << "/";
    testName << dataSize << "MB";
    benchmark::RegisterBenchmark(testName.str(), tpch_q6_clustering_sweep_Benchmark, dataSize)
        ->MeasureProcessCPUTime()
        ->UseRealTime()
        ->Apply([](benchmark::internal::Benchmark* b) {
          std::string filepath = "../data/tpch_" +
                                 std::to_string(googleBenchmarkApplyParameterHelper) +
                                 "MB/lineitem.tbl";
          std::vector<int> thresholds =
              generateLogDistribution(30, 1, static_cast<double>(getNumberOfRowsInTable(filepath)));
          for(int value : thresholds) {
            b->Arg(value);
          }
        });
  }

  // register selectivity sweep benchmarks
  {
    int dataSize = 1 * 250 * 1000 * 1000;
    std::ostringstream testName;
    std::string queryName = "selectivity_sweep_uniform_dis";
    testName << queryName << ",";
    testName << dataSize << " tuples";
    benchmark::RegisterBenchmark(testName.str(), selectivity_sweep_uniform_dis_Benchmark, dataSize,
                                 queryName)
        ->MeasureProcessCPUTime()
        ->UseRealTime()
        ->Apply([](benchmark::internal::Benchmark* b) {
          std::vector<int> thresholds = generateLogDistribution(30, 1, 10001);
          for(int value : thresholds) {
            b->Arg(value);
          }
        });
  }

  // register randomness sweep benchmarks
  {
    int dataSize = 1 * 250 * 1000 * 1000;
    std::ostringstream testName;
    std::string queryName = "randomness_sweep_sorted_dis";
    testName << queryName << "/";
    testName << dataSize << " tuples";
    benchmark::RegisterBenchmark(testName.str(), randomness_sweep_sorted_dis_Benchmark, dataSize,
                                 queryName)
        ->MeasureProcessCPUTime()
        ->UseRealTime()
        ->Apply([](benchmark::internal::Benchmark* b) {
          std::vector<int> thresholds = generateLogDistribution(30, 1, 10000);
          for(int value : thresholds) {
            b->Arg(value);
          }
        });
  }

  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();

  releaseBOSSEngines();
}

int main(int argc, char** argv) {
  try {
    initAndRunBenchmarks(argc, argv);
  } catch(std::exception& e) {
    std::cerr << "caught exception in main: " << e.what() << std::endl;
    return EXIT_FAILURE;
  } catch(...) {
    std::cerr << "unhandled exception." << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}