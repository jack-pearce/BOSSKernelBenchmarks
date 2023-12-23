#include "dataGeneration.cpp"
#include "utilities.cpp"
#include <benchmark/benchmark.h>
#include <iostream>

using SpanArguments = boss::DefaultExpressionSystem::ExpressionSpanArguments;
using ComplexExpression = boss::DefaultExpressionSystem::ComplexExpression;
using ExpressionArguments = boss::ExpressionArguments;

void initStorageEngine_selectivity_sweep_uniform_dis(int dataSize) {
  static auto dataSet = std::string("selectivity_sweep_uniform_dis");

  if(latestDataSet == dataSet && latestDataSize == dataSize) {
    return;
  }

  resetStorageEngine();
  latestDataSet = dataSet;
  latestDataSize = dataSize;

  auto evalStorage = [](boss::Expression&& expression) mutable {
    return boss::evaluate("EvaluateInEngines"_("List"_(librariesToTest[0]), std::move(expression)));
  };

  auto checkForErrors = [](auto&& output) {
    auto* maybeComplexExpr = std::get_if<boss::ComplexExpression>(&output);
    if(maybeComplexExpr == nullptr) {
      return;
    }
    if(maybeComplexExpr->getHead() == "ErrorWhenEvaluatingExpression"_) {
      std::cout << "Error: " << output << std::endl;
    }
  };

  checkForErrors(evalStorage("CreateTable"_("UNIFORM_DIS"_)));

  SpanArguments keySpan, payloadSpan;
  keySpan.push_back(
      boss::Span<int64_t>(std::vector(generateUniformDistribution<int64_t>(dataSize, 1, 10000))));
  payloadSpan.push_back(
      boss::Span<int64_t>(std::vector(generateUniformDistribution<int64_t>(dataSize, 1, 10000))));

  ExpressionArguments keyColumn, payloadColumn, columns;
  keyColumn.emplace_back(ComplexExpression("List"_, {}, {}, std::move(keySpan)));
  payloadColumn.emplace_back(ComplexExpression("List"_, {}, {}, std::move(payloadSpan)));

  columns.emplace_back(ComplexExpression("key"_, {}, std::move(keyColumn), {}));
  columns.emplace_back(ComplexExpression("payload"_, {}, std::move(payloadColumn), {}));

  checkForErrors(evalStorage(
      "LoadDataTable"_("UNIFORM_DIS"_, ComplexExpression("Data"_, {}, std::move(columns), {}))));
}

void selectivity_sweep_uniform_dis_Benchmark(benchmark::State& state, int dataSize,
                                             const std::string& queryName) {
  initStorageEngine_selectivity_sweep_uniform_dis(dataSize);

  auto eval = [](auto&& expression) {
    boss::expressions::ExpressionSpanArguments spans;
    spans.emplace_back(boss::expressions::Span<std::string>(librariesToTest));
    return boss::evaluate("EvaluateInEngines"_(
        boss::ComplexExpression("List"_, {}, {}, std::move(spans)), std::move(expression)));
  };

  auto error_found = [](auto&& result, auto const& queryName) {
    if(!std::holds_alternative<boss::ComplexExpression>(result)) {
      return false;
    }
    if(std::get<boss::ComplexExpression>(result).getHead() == "Table"_) {
      return false;
    }
    if(std::get<boss::ComplexExpression>(result).getHead() == "List"_) {
      return false;
    }
    std::cout << queryName << " Error: "
              << (VERY_VERBOSE_QUERY_OUTPUT ? std::move(result)
                                            : utilities::injectDebugInfoToSpans(std::move(result)))
              << std::endl;
    return true;
  };

  int threshold = state.range(0);

  boss::Expression query =
      "Select"_("Project"_("UNIFORM_DIS"_, "As"_("key"_, "key"_, "payload"_, "payload"_)),
                "Where"_("Greater"_(threshold, "key"_)));

  if(VERBOSE_QUERY_OUTPUT) {
    auto result = eval(utilities::shallowCopy(std::get<boss::ComplexExpression>(query)));
    if(!error_found(result, queryName)) {
      std::cout << "BOSS " << queryName << " output = "
                << (VERY_VERBOSE_QUERY_OUTPUT
                        ? std::move(result)
                        : utilities::injectDebugInfoToSpans(std::move(result)))
                << std::endl;
    }
  }

  bool failed = false;

  for(int i = VERBOSE_QUERY_OUTPUT; i < BENCHMARK_NUM_WARMPUP_ITERATIONS; ++i) {
    auto result = eval(utilities::shallowCopy(std::get<boss::ComplexExpression>(query)));
    if(error_found(result, queryName)) {
      failed = true;
      break;
    }
  }

  vtune.startSampling(queryName + " - BOSS");
  for(auto _ : state) { // NOLINT
    if(!failed) {
      auto result = eval(utilities::shallowCopy(std::get<boss::ComplexExpression>(query)));
      if(error_found(result, queryName)) {
        failed = true;
      }
      benchmark::DoNotOptimize(result);
    }
  }
  vtune.stopSampling();
}

void initStorageEngine_randomness_sweep_sorted_diss(int dataSize, float percentageRandom) {
  static auto dataSet = std::string("randomness_sweep_sorted_dis");

  resetStorageEngine();
  latestDataSet = dataSet;
  latestDataSize = dataSize;

  auto evalStorage = [](boss::Expression&& expression) mutable {
    return boss::evaluate("EvaluateInEngines"_("List"_(librariesToTest[0]), std::move(expression)));
  };

  auto checkForErrors = [](auto&& output) {
    auto* maybeComplexExpr = std::get_if<boss::ComplexExpression>(&output);
    if(maybeComplexExpr == nullptr) {
      return;
    }
    if(maybeComplexExpr->getHead() == "ErrorWhenEvaluatingExpression"_) {
      std::cout << "Error: " << output << std::endl;
    }
  };

  checkForErrors(evalStorage("CreateTable"_("PARTIALLY_SORTED_DIS"_)));

  SpanArguments keySpan, payloadSpan;
  keySpan.push_back(
      boss::Span<int64_t>(std::vector(generatePartiallySortedOneToOneHundred<int64_t>(dataSize, 10, percentageRandom))));
  payloadSpan.push_back(
      boss::Span<int64_t>(std::vector(generateUniformDistribution<int64_t>(dataSize, 1, 10000))));

  ExpressionArguments keyColumn, payloadColumn, columns;
  keyColumn.emplace_back(ComplexExpression("List"_, {}, {}, std::move(keySpan)));
  payloadColumn.emplace_back(ComplexExpression("List"_, {}, {}, std::move(payloadSpan)));

  columns.emplace_back(ComplexExpression("key"_, {}, std::move(keyColumn), {}));
  columns.emplace_back(ComplexExpression("payload"_, {}, std::move(payloadColumn), {}));

  checkForErrors(evalStorage(
      "LoadDataTable"_("PARTIALLY_SORTED_DIS"_, ComplexExpression("Data"_, {}, std::move(columns), {}))));
}

void randomness_sweep_sorted_dis_Benchmark(benchmark::State& state, int dataSize,
                                             const std::string& queryName) {
  float percentageRandom = state.range(0) / 100.0;
  initStorageEngine_randomness_sweep_sorted_diss(dataSize, percentageRandom);

  auto eval = [](auto&& expression) {
    boss::expressions::ExpressionSpanArguments spans;
    spans.emplace_back(boss::expressions::Span<std::string>(librariesToTest));
    return boss::evaluate("EvaluateInEngines"_(
        boss::ComplexExpression("List"_, {}, {}, std::move(spans)), std::move(expression)));
  };

  auto error_found = [](auto&& result, auto const& queryName) {
    if(!std::holds_alternative<boss::ComplexExpression>(result)) {
      return false;
    }
    if(std::get<boss::ComplexExpression>(result).getHead() == "Table"_) {
      return false;
    }
    if(std::get<boss::ComplexExpression>(result).getHead() == "List"_) {
      return false;
    }
    std::cout << queryName << " Error: "
              << (VERY_VERBOSE_QUERY_OUTPUT ? std::move(result)
                                            : utilities::injectDebugInfoToSpans(std::move(result)))
              << std::endl;
    return true;
  };

  boss::Expression query =
      "Select"_("Project"_("PARTIALLY_SORTED_DIS"_, "As"_("key"_, "key"_, "payload"_, "payload"_)),
                "Where"_("Greater"_(51, "key"_)));

  if(VERBOSE_QUERY_OUTPUT) {
    auto result = eval(utilities::shallowCopy(std::get<boss::ComplexExpression>(query)));
    if(!error_found(result, queryName)) {
      std::cout << "BOSS " << queryName << " output = "
                << (VERY_VERBOSE_QUERY_OUTPUT
                        ? std::move(result)
                        : utilities::injectDebugInfoToSpans(std::move(result)))
                << std::endl;
    }
  }

  bool failed = false;

  for(int i = VERBOSE_QUERY_OUTPUT; i < BENCHMARK_NUM_WARMPUP_ITERATIONS; ++i) {
    auto result = eval(utilities::shallowCopy(std::get<boss::ComplexExpression>(query)));
    if(error_found(result, queryName)) {
      failed = true;
      break;
    }
  }

  vtune.startSampling(queryName + " - BOSS");
  for(auto _ : state) { // NOLINT
    if(!failed) {
      auto result = eval(utilities::shallowCopy(std::get<boss::ComplexExpression>(query)));
      if(error_found(result, queryName)) {
        failed = true;
      }
      benchmark::DoNotOptimize(result);
    }
  }
  vtune.stopSampling();
}