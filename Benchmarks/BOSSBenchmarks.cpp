#include <BOSS.h>
#include <ExpressionUtilities.hpp>
#include <Serialization.hpp>

// #include "ITTNotifySupport.hpp"
#include <benchmark/benchmark.h>
#include <iostream>
#include <string>
#include <vector>

using namespace boss::utilities;

namespace {
int64_t operator""_i64(char c) { return static_cast<int64_t>(c); };
} // namespace

static std::vector<std::string> librariesToTest{};

// static auto const vtune = VTuneAPIInterface{"BOSS"};
static void tpch_q6(benchmark::State& state, const std::string& engineLibrary) {

  auto eval = [&engineLibrary](boss::Expression&& expression) mutable {
    return boss::evaluate("EvaluateInEngines"_("List"_(engineLibrary), std::move(expression)));
  };

  auto lineitem =
      "Table"_("L_ORDERKEY"_("List"_(1, 1, 2, 3)),                                 // NOLINT
               "L_PARTKEY"_("List"_(1, 2, 3, 4)),                                  // NOLINT
               "L_SUPPKEY"_("List"_(1, 2, 3, 4)),                                  // NOLINT
               "L_RETURNFLAG"_("List"_("N", "N", "A", "A")),                       // NOLINT
               "L_LINESTATUS"_("List"_("O", "O", "F", "F")),                       // NOLINT
               "L_RETURNFLAG_INT"_("List"_('N'_i64, 'N'_i64, 'A'_i64, 'A'_i64)),   // NOLINT
               "L_LINESTATUS_INT"_("List"_('O'_i64, 'O'_i64, 'F'_i64, 'F'_i64)),   // NOLINT
               "L_QUANTITY"_("List"_(17, 21, 8, 5)),                               // NOLINT
               "L_EXTENDEDPRICE"_("List"_(17954.55, 34850.16, 7712.48, 25284.00)), // NOLINT
               "L_DISCOUNT"_("List"_(0.10, 0.05, 0.06, 0.06)),                     // NOLINT
               "L_TAX"_("List"_(0.02, 0.06, 0.02, 0.06)),                          // NOLINT
               "L_SHIPDATE"_("List"_("DateObject"_("1992-03-13"), "DateObject"_("1994-04-12"),
                                     "DateObject"_("1996-02-28"), "DateObject"_("1994-12-31"))));

  //  vtune.startSampling("DummyBenchmark");
  for(auto _ : state) { // NOLINT
    auto output = eval("Group"_(
        "Project"_(
            "Select"_("Project"_(lineitem.clone(boss::expressions::CloneReason::FOR_TESTING),
                                 "As"_("L_QUANTITY"_, "L_QUANTITY"_, "L_DISCOUNT"_, "L_DISCOUNT"_,
                                       "L_SHIPDATE"_, "L_SHIPDATE"_, "L_EXTENDEDPRICE"_,
                                       "L_EXTENDEDPRICE"_)),
                      "Where"_("And"_("Greater"_(24, "L_QUANTITY"_),      // NOLINT
                                      "Greater"_("L_DISCOUNT"_, 0.0499),  // NOLINT
                                      "Greater"_(0.07001, "L_DISCOUNT"_), // NOLINT
                                      "Greater"_("DateObject"_("1995-01-01"), "L_SHIPDATE"_),
                                      "Greater"_("L_SHIPDATE"_, "DateObject"_("1993-12-31"))))),
            "As"_("revenue"_, "Times"_("L_EXTENDEDPRICE"_, "L_DISCOUNT"_))),
        "Sum"_("revenue"_)));
    benchmark::DoNotOptimize(output);
  }
  //  vtune.stopSampling();

  if (state.thread_index() == 0) {
    std::cout << "Result: ";
    auto output = eval("Group"_(
        "Project"_(
            "Select"_("Project"_(lineitem.clone(boss::expressions::CloneReason::FOR_TESTING),
                                 "As"_("L_QUANTITY"_, "L_QUANTITY"_, "L_DISCOUNT"_, "L_DISCOUNT"_,
                                       "L_SHIPDATE"_, "L_SHIPDATE"_, "L_EXTENDEDPRICE"_,
                                       "L_EXTENDEDPRICE"_)),
                      "Where"_("And"_("Greater"_(24, "L_QUANTITY"_),      // NOLINT
                                      "Greater"_("L_DISCOUNT"_, 0.0499),  // NOLINT
                                      "Greater"_(0.07001, "L_DISCOUNT"_), // NOLINT
                                      "Greater"_("DateObject"_("1995-01-01"), "L_SHIPDATE"_),
                                      "Greater"_("L_SHIPDATE"_, "DateObject"_("1993-12-31"))))),
            "As"_("revenue"_, "Times"_("L_EXTENDEDPRICE"_, "L_DISCOUNT"_))),
        "Sum"_("revenue"_)));
    std::cout << output << std::endl;
  }
}

void initAndRunBenchmarks(int argc, char** argv) {
  for(int i = 0; i < argc; ++i) {
    if(std::string("--library") == argv[i]) {
      if(++i < argc) {
        librariesToTest.emplace_back(argv[i]);
      }
    }
  }

  for(auto& library : librariesToTest) {
    benchmark::RegisterBenchmark("TPC-H_Q6_" + library, tpch_q6, library);
  }

  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
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