#include <BOSS.hpp>
#include <ExpressionUtilities.hpp>

#include "ITTNotifySupport.hpp"
#include <benchmark/benchmark.h>
#include <iostream>
#include <string>
#include <vector>

using namespace boss::utilities;

namespace utilities {
static boss::ComplexExpression shallowCopy(boss::ComplexExpression const& e) {
  auto const& head = e.getHead();
  auto const& dynamics = e.getDynamicArguments();
  auto const& spans = e.getSpanArguments();
  boss::ExpressionArguments dynamicsCopy;
  std::transform(dynamics.begin(), dynamics.end(), std::back_inserter(dynamicsCopy),
                 [](auto const& arg) {
                   return std::visit(
                       boss::utilities::overload(
                           [&](boss::ComplexExpression const& expr) -> boss::Expression {
                             return shallowCopy(expr);
                           },
                           [](auto const& otherTypes) -> boss::Expression { return otherTypes; }),
                       arg);
                 });
  boss::expressions::ExpressionSpanArguments spansCopy;
  std::transform(spans.begin(), spans.end(), std::back_inserter(spansCopy), [](auto const& span) {
    return std::visit(
        [](auto const& typedSpan) -> boss::expressions::ExpressionSpanArgument {
          // just do a shallow copy of the span
          // the storage's span keeps the ownership
          // (since the storage will be alive until the query finishes)
          using SpanType = std::decay_t<decltype(typedSpan)>;
          using T = std::remove_const_t<typename SpanType::element_type>;
          if constexpr(std::is_same_v<T, bool>) {
            // this would still keep const spans for bools, need to fix later
            return SpanType(typedSpan.begin(), typedSpan.size(), []() {});
          } else {
            // force non-const value for now (otherwise expressions cannot be moved)
            auto* ptr = const_cast<T*>(typedSpan.begin()); // NOLINT
            return boss::Span<T>(ptr, typedSpan.size(), []() {});
          }
        },
        span);
  });
  return {head, {}, std::move(dynamicsCopy), std::move(spansCopy)};
}

static boss::Expression injectDebugInfoToSpans(boss::Expression&& expr) {
  return std::visit(
      boss::utilities::overload(
          [&](boss::ComplexExpression&& e) -> boss::Expression {
            auto [head, unused_, dynamics, spans] = std::move(e).decompose();
            boss::ExpressionArguments debugDynamics;
            debugDynamics.reserve(dynamics.size() + spans.size());
            std::transform(std::make_move_iterator(dynamics.begin()),
                           std::make_move_iterator(dynamics.end()),
                           std::back_inserter(debugDynamics), [](auto&& arg) {
                             return injectDebugInfoToSpans(std::forward<decltype(arg)>(arg));
                           });
            std::transform(
                std::make_move_iterator(spans.begin()), std::make_move_iterator(spans.end()),
                std::back_inserter(debugDynamics), [](auto&& span) {
                  return std::visit(
                      [](auto&& typedSpan) -> boss::Expression {
                        using Element = typename std::decay_t<decltype(typedSpan)>::element_type;
                        return boss::ComplexExpressionWithStaticArguments<std::string, int64_t>(
                            "Span"_, {typeid(Element).name(), typedSpan.size()}, {}, {});
                      },
                      std::forward<decltype(span)>(span));
                });
            return boss::ComplexExpression(std::move(head), {}, std::move(debugDynamics), {});
          },
          [](auto&& otherTypes) -> boss::Expression { return otherTypes; }),
      std::move(expr));
}
} // namespace utilities

static auto const vtune = VTuneAPIInterface{"BOSS"};

static bool VERBOSE_QUERY_OUTPUT = false;
static bool VERY_VERBOSE_QUERY_OUTPUT = false;
static bool ENABLE_CONSTRAINTS = false;
static int BENCHMARK_NUM_WARMPUP_ITERATIONS = 5;

enum TPCH_QUERIES { TPCH_Q1 = 1, TPCH_Q3 = 3, TPCH_Q6 = 6, TPCH_Q9 = 9, TPCH_Q18 = 18 };
enum DATASETS { TPCH = 0, CUSTOM = 100 };

static auto& librariesToTest() {
  static std::vector<std::string> libraries;
  return libraries;
}

static auto& lastDataSize() {
  static int dataSize;
  return dataSize;
}

static auto& lastDataSet() {
  static std::string dataset;
  return dataset;
}

static void releaseBOSSEngines() {
  // make sure to release engines in reverse order of evaluation
  // (important for data ownership across engines)
  auto reversedLibraries = librariesToTest();
  std::reverse(reversedLibraries.begin(), reversedLibraries.end());
  boss::expressions::ExpressionSpanArguments spans;
  spans.emplace_back(boss::expressions::Span<std::string>(reversedLibraries));
  boss::evaluate("ReleaseEngines"_(boss::ComplexExpression("List"_, {}, {}, std::move(spans))));
}

static void resetStorageEngine() {
  auto evalStorage = [](boss::Expression&& expression) mutable {
    return boss::evaluate(
        "EvaluateInEngines"_("List"_(librariesToTest()[0]), std::move(expression)));
  };

  if(lastDataSet() == "TPCH") {
    evalStorage("DropTable"_("REGION"_));
    evalStorage("DropTable"_("NATION"_));
    evalStorage("DropTable"_("PART"_));
    evalStorage("DropTable"_("SUPPLIER"_));
    evalStorage("DropTable"_("PARTSUPP"_));
    evalStorage("DropTable"_("CUSTOMER"_));
    evalStorage("DropTable"_("ORDERS"_));
    evalStorage("DropTable"_("LINEITEM"_));
    return;
  }
}

static void initStorageEngine_TPCH(int dataSize) {
  static auto dataSet = std::string("TPCH");

  if(lastDataSet() == dataSet && lastDataSize() == dataSize) {
    return;
  }

  resetStorageEngine();
  lastDataSet() = dataSet;
  lastDataSize() = dataSize;

  auto evalStorage = [](boss::Expression&& expression) mutable {
    return boss::evaluate(
        "EvaluateInEngines"_("List"_(librariesToTest()[0]), std::move(expression)));
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

#if FALSE
  checkForErrors(eval("Set"_("LoadToMemoryMappedFiles"_, !DISABLE_MMAP_CACHE)));
  checkForErrors(eval("Set"_("UseAutoDictionaryEncoding"_, !DISABLE_AUTO_DICTIONARY_ENCODING)));
  checkForErrors(eval("Set"_("AllStringColumnsAsIntegers"_, ALL_STRINGS_AS_INTEGERS)));
#endif

  checkForErrors(evalStorage("CreateTable"_(
      "LINEITEM"_, "l_orderkey"_, "l_partkey"_, "l_suppkey"_, "l_linenumber"_, "l_quantity"_,
      "l_extendedprice"_, "l_discount"_, "l_tax"_, "l_returnflag"_, "l_linestatus"_, "l_shipdate"_,
      "l_commitdate"_, "l_receiptdate"_, "l_shipinstruct"_, "l_shipmode"_, "l_comment"_)));

#if FALSE
  checkForErrors(evalStorage("CreateTable"_("REGION"_, "r_regionkey"_, "r_name"_, "r_comment"_)));

  checkForErrors(evalStorage(
      "CreateTable"_("NATION"_, "n_nationkey"_, "n_name"_, "n_regionkey"_, "n_comment"_)));

  checkForErrors(
      evalStorage("CreateTable"_("PART"_, "p_partkey"_, "p_name"_, "p_mfgr"_, "p_brand"_, "p_type"_,
                                 "p_size"_, "p_container"_, "p_retailprice"_, "p_comment"_)));

  checkForErrors(
      evalStorage("CreateTable"_("SUPPLIER"_, "s_suppkey"_, "s_name"_, "s_address"_, "s_nationkey"_,
                                 "s_phone"_, "s_acctbal"_, "s_comment"_)));

  checkForErrors(evalStorage("CreateTable"_("PARTSUPP"_, "ps_partkey"_, "ps_suppkey"_,
                                            "ps_availqty"_, "ps_supplycost"_, "ps_comment"_)));

  checkForErrors(
      evalStorage("CreateTable"_("CUSTOMER"_, "c_custkey"_, "c_name"_, "c_address"_, "c_nationkey"_,
                                 "c_phone"_, "c_acctbal"_, "c_mktsegment"_, "c_comment"_)));

  checkForErrors(evalStorage("CreateTable"_(
      "ORDERS"_, "o_orderkey"_, "o_custkey"_, "o_orderstatus"_, "o_totalprice"_, "o_orderdate"_,
      "o_orderpriority"_, "o_clerk"_, "o_shippriority"_, "o_comment"_)));
#endif

  auto filenamesAndTables = std::vector<std::pair<std::string, boss::Symbol>>{
      {"lineitem", "LINEITEM"_} /*, {"region", "REGION"_},     {"nation", "NATION"_},
       {"part", "PART"_},         {"supplier", "SUPPLIER"_}, {"partsupp", "PARTSUPP"_},
       {"customer", "CUSTOMER"_}, {"orders", "ORDERS"_}*/
  };

  for(auto const& [filename, table] : filenamesAndTables) {
    std::string path = "../data/tpch_" + std::to_string(dataSize) + "MB/" + filename + ".tbl";
    checkForErrors(evalStorage("Load"_(table, path)));
  }

  if(ENABLE_CONSTRAINTS) {
    // primary key constraints
    checkForErrors(evalStorage("AddConstraint"_("PART"_, "PrimaryKey"_("p_partkey"_))));
    checkForErrors(evalStorage("AddConstraint"_("SUPPLIER"_, "PrimaryKey"_("s_suppkey"_))));
    checkForErrors(
        evalStorage("AddConstraint"_("PARTSUPP"_, "PrimaryKey"_("ps_partkey"_, "ps_suppkey"_))));
    checkForErrors(evalStorage("AddConstraint"_("CUSTOMER"_, "PrimaryKey"_("c_custkey"_))));
    checkForErrors(evalStorage("AddConstraint"_("ORDERS"_, "PrimaryKey"_("o_orderkey"_))));
    checkForErrors(
        evalStorage("AddConstraint"_("LINEITEM"_, "PrimaryKey"_("l_orderkey"_, "l_linenumber"_))));
    checkForErrors(evalStorage("AddConstraint"_("NATION"_, "PrimaryKey"_("n_nationkey"_))));
    checkForErrors(evalStorage("AddConstraint"_("REGION"_, "PrimaryKey"_("r_regionkey"_))));

    // foreign key constraints
    checkForErrors(
        evalStorage("AddConstraint"_("SUPPLIER"_, "ForeignKey"_("NATION"_, "s_nationkey"_))));
    checkForErrors(
        evalStorage("AddConstraint"_("PARTSUPP"_, "ForeignKey"_("PART"_, "ps_partkey"_))));
    checkForErrors(
        evalStorage("AddConstraint"_("PARTSUPP"_, "ForeignKey"_("SUPPLIER"_, "ps_suppkey"_))));
    checkForErrors(
        evalStorage("AddConstraint"_("CUSTOMER"_, "ForeignKey"_("NATION"_, "c_nationkey"_))));
    checkForErrors(
        evalStorage("AddConstraint"_("ORDERS"_, "ForeignKey"_("CUSTOMER"_, "o_custkey"_))));
    checkForErrors(
        evalStorage("AddConstraint"_("LINEITEM"_, "ForeignKey"_("ORDERS"_, "l_orderkey"_))));
    checkForErrors(evalStorage(
        "AddConstraint"_("LINEITEM"_, "ForeignKey"_("PARTSUPP"_, "l_partkey"_, "l_suppkey"_))));
    checkForErrors(
        evalStorage("AddConstraint"_("NATION"_, "ForeignKey"_("REGION"_, "n_regionkey"_))));
  }
}

static auto& queryNames() {
  static std::map<int, std::string> names;
  if(names.empty()) {
    // TPC-H
    names.try_emplace(DATASETS::TPCH + TPCH_Q1, "TPC-H_Q1");
    names.try_emplace(DATASETS::TPCH + TPCH_Q3, "TPC-H_Q3");
    names.try_emplace(DATASETS::TPCH + TPCH_Q6, "TPC-H_Q6");
    names.try_emplace(DATASETS::TPCH + TPCH_Q9, "TPC-H_Q9");
    names.try_emplace(DATASETS::TPCH + TPCH_Q18, "TPC-H_Q18");

    // Custom
  }
  return names;
}

static auto& queries() {
  // Queries are Expressions and therefore cannot be just a table i.e. a Symbol
  static std::map<int, boss::Expression> queries;
  if(queries.empty()) {
    // TPC-H
    queries.try_emplace(
        DATASETS::TPCH + TPCH_Q1,
        "Order"_(
            "Group"_(
                "Project"_(
                    "Project"_(
                        "Project"_(
                            "Select"_(
                                "Project"_("LINEITEM"_,
                                           "As"_("l_quantity"_, "l_quantity"_, "l_discount"_,
                                                 "l_discount"_, "l_shipdate"_, "l_shipdate"_,
                                                 "l_extendedprice"_, "l_extendedprice"_,
                                                 "l_returnflag"_, "l_returnflag"_, "l_linestatus"_,
                                                 "l_linestatus"_, "l_tax"_, "l_tax"_)),
                                "Where"_("Greater"_("DateObject"_("1998-08-31"), "l_shipdate"_))),
                            "As"_("l_returnflag"_, "l_returnflag"_, "l_linestatus"_,
                                  "l_linestatus"_, "l_quantity"_, "l_quantity"_, "l_extendedprice"_,
                                  "l_extendedprice"_, "l_discount"_, "l_discount"_, "calc1"_,
                                  "Minus"_(1.0, "l_discount"_), "calc2"_, "Plus"_("l_tax"_, 1.0))),
                        "As"_("l_returnflag"_, "l_returnflag"_, "l_linestatus"_, "l_linestatus"_,
                              "l_quantity"_, "l_quantity"_, "l_extendedprice"_, "l_extendedprice"_,
                              "l_discount"_, "l_discount"_, "disc_price"_,
                              "Times"_("l_extendedprice"_, "calc1"_), "calc2"_, "calc2"_)),
                    "As"_("l_returnflag"_, "l_returnflag"_, "l_linestatus"_, "l_linestatus"_,
                          "l_quantity"_, "l_quantity"_, "l_extendedprice"_, "l_extendedprice"_,
                          "l_discount"_, "l_discount"_, "disc_price"_, "disc_price"_, "calc"_,
                          "Times"_("disc_price"_, "calc2"_))),
                "By"_("l_returnflag"_, "l_linestatus"_),
                "As"_("sum_qty"_, "Sum"_("l_quantity"_), "sum_base_price"_,
                      "Sum"_("l_extendedprice"_), "sum_disc_price"_, "Sum"_("disc_price"_),
                      "sum_charges"_, "Sum"_("calc"_), "avg_qty"_, "Avg"_("l_quantity"_),
                      "avg_price"_, "Avg"_("l_extendedprice"_), "avg_disc"_, "Avg"_("l_discount"_),
                      "count_order"_, "Count"_("*"_))),
            "By"_("l_returnflag"_, "l_linestatus"_)));

    queries.try_emplace(
        DATASETS::TPCH + TPCH_Q3,
        "Top"_(
            "Group"_(
                "Project"_(
                    "Join"_(
                        "Project"_(
                            "Join"_(
                                "Project"_(
                                    "Select"_("Project"_("CUSTOMER"_,
                                                         "As"_("c_custkey"_, "c_custkey"_,
                                                               "c_mktsegment"_, "c_mktsegment"_)),
                                              "Where"_(
                                                  "StringContainsQ"_("c_mktsegment"_, "BUILDING"))),
                                    "As"_("c_custkey"_, "c_custkey"_, "c_mktsegment"_,
                                          "c_mktsegment"_)),
                                "Select"_(
                                    "Project"_("ORDERS"_,
                                               "As"_("o_orderkey"_, "o_orderkey"_, "o_orderdate"_,
                                                     "o_orderdate"_, "o_custkey"_, "o_custkey"_,
                                                     "o_shippriority"_, "o_shippriority"_)),
                                    "Where"_(
                                        "Greater"_("DateObject"_("1995-03-15"), "o_orderdate"_))),
                                "Where"_("Equal"_("c_custkey"_, "o_custkey"_))),
                            "As"_("o_orderkey"_, "o_orderkey"_, "o_orderdate"_, "o_orderdate"_,
                                  "o_custkey"_, "o_custkey"_, "o_shippriority"_,
                                  "o_shippriority"_)),
                        "Project"_(
                            "Select"_(
                                "Project"_("LINEITEM"_,
                                           "As"_("l_orderkey"_, "l_orderkey"_, "l_discount"_,
                                                 "l_discount"_, "l_shipdate"_, "l_shipdate"_,
                                                 "l_extendedprice"_, "l_extendedprice"_)),
                                "Where"_("Greater"_("l_shipdate"_, "DateObject"_("1993-03-15")))),
                            "As"_("l_orderkey"_, "l_orderkey"_, "l_discount"_, "l_discount"_,
                                  "l_extendedprice"_, "l_extendedprice"_)),
                        "Where"_("Equal"_("o_orderkey"_, "l_orderkey"_))),
                    "As"_("expr1009"_, "Times"_("l_extendedprice"_, "Minus"_(1.0, "l_discount"_)),
                          "l_extendedprice"_, "l_extendedprice"_, "l_orderkey"_, "l_orderkey"_,
                          "o_orderdate"_, "o_orderdate"_, "o_shippriority"_, "o_shippriority"_)),
                "By"_("l_orderkey"_, "o_orderdate"_, "o_shippriority"_),
                "As"_("revenue"_, "Sum"_("expr1009"_))),
            "By"_("revenue"_, "desc"_, "o_orderdate"_), 10));

    queries.try_emplace(
        DATASETS::TPCH + TPCH_Q6,
        "Group"_(
            "Project"_(
                "Select"_("Project"_("LINEITEM"_, "As"_("l_quantity"_, "l_quantity"_, "l_discount"_,
                                                        "l_discount"_, "l_shipdate"_, "l_shipdate"_,
                                                        "l_extendedprice"_, "l_extendedprice"_)),
                          "Where"_("And"_("Greater"_(24, "l_quantity"_),      // NOLINT
                                          "Greater"_("l_discount"_, 0.0499),  // NOLINT
                                          "Greater"_(0.07001, "l_discount"_), // NOLINT
                                          "Greater"_("DateObject"_("1995-01-01"), "l_shipdate"_),
                                          "Greater"_("l_shipdate"_, "DateObject"_("1993-12-31"))))),
                "As"_("revenue"_, "Times"_("l_extendedprice"_, "l_discount"_))),
            "Sum"_("revenue"_)));

    queries.try_emplace(
        DATASETS::TPCH + TPCH_Q9,
        "Order"_(
            "Group"_(
                "Project"_(
                    "Join"_(
                        "Project"_("ORDERS"_, "As"_("o_orderkey"_, "o_orderkey"_, "o_orderdate"_,
                                                    "o_orderdate"_)),
                        "Project"_(
                            "Join"_(
                                "Project"_(
                                    "Join"_(
                                        "Project"_(
                                            "Select"_(
                                                "Project"_("PART"_,
                                                           "As"_("p_partkey"_, "p_partkey"_,
                                                                 "p_retailprice"_,
                                                                 "p_retailprice"_)),
                                                "Where"_("And"_("Greater"_("p_retailprice"_,
                                                                           1006.05), // NOLINT
                                                                "Greater"_(1080.1,   // NOLINT
                                                                           "p_retailprice"_)))),
                                            "As"_("p_partkey"_, "p_partkey"_, "p_retailprice"_,
                                                  "p_retailprice"_)),
                                        "Project"_(
                                            "Join"_(
                                                "Project"_(
                                                    "Join"_(
                                                        "Project"_("NATION"_,
                                                                   "As"_("n_name"_, "n_name"_,
                                                                         "n_nationkey"_,
                                                                         "n_nationkey"_)),
                                                        "Project"_("SUPPLIER"_,
                                                                   "As"_("s_suppkey"_, "s_suppkey"_,
                                                                         "s_nationkey"_,
                                                                         "s_nationkey"_)),
                                                        "Where"_("Equal"_("n_nationkey"_,
                                                                          "s_nationkey"_))),
                                                    "As"_("n_name"_, "n_name"_, "s_suppkey"_,
                                                          "s_suppkey"_)),
                                                "Project"_("PARTSUPP"_,
                                                           "As"_("ps_partkey"_, "ps_partkey"_,
                                                                 "ps_suppkey"_, "ps_suppkey"_,
                                                                 "ps_supplycost"_,
                                                                 "ps_supplycost"_)),
                                                "Where"_("Equal"_("s_suppkey"_, "ps_suppkey"_))),
                                            "As"_("n_name"_, "n_name"_, "ps_partkey"_,
                                                  "ps_partkey"_, "ps_suppkey"_, "ps_suppkey"_,
                                                  "ps_supplycost"_, "ps_supplycost"_)),
                                        "Where"_("Equal"_("p_partkey"_, "ps_partkey"_))),
                                    "As"_("n_name"_, "n_name"_, "ps_partkey"_, "ps_partkey"_,
                                          "ps_suppkey"_, "ps_suppkey"_, "ps_supplycost"_,
                                          "ps_supplycost"_)),
                                "Project"_("LINEITEM"_,
                                           "As"_("l_partkey"_, "l_partkey"_, "l_suppkey"_,
                                                 "l_suppkey"_, "l_orderkey"_, "l_orderkey"_,
                                                 "l_extendedprice"_, "l_extendedprice"_,
                                                 "l_discount"_, "l_discount"_, "l_quantity"_,
                                                 "l_quantity"_)),
                                "Where"_("Equal"_("List"_("ps_partkey"_, "ps_suppkey"_),
                                                  "List"_("l_partkey"_, "l_suppkey"_)))),
                            "As"_("n_name"_, "n_name"_, "ps_supplycost"_, "ps_supplycost"_,
                                  "l_orderkey"_, "l_orderkey"_, "l_extendedprice"_,
                                  "l_extendedprice"_, "l_discount"_, "l_discount"_, "l_quantity"_,
                                  "l_quantity"_)),
                        "Where"_("Equal"_("o_orderkey"_, "l_orderkey"_))),
                    "As"_("nation"_, "n_name"_, "o_year"_, "Year"_("o_orderdate"_), "amount"_,
                          "Minus"_("Times"_("l_extendedprice"_, "Minus"_(1.0, "l_discount"_)),
                                   "Times"_("ps_supplycost"_, "l_quantity"_)))),
                "By"_("nation"_, "o_year"_), "Sum"_("amount"_)),
            "By"_("nation"_, "o_year"_, "desc"_)));

    queries.try_emplace(
        DATASETS::TPCH + TPCH_Q18,
        "Top"_(
            "Group"_(
                "Project"_(
                    "Join"_(
                        /* Following our join order heuristics, LINEITEM should be on the probe side
                         * and not the build side. However, because Velox engine does not support an
                         * aggregated relation on the probe side, we need to put the
                         * aggregated relation on the build side.
                         */
                        "Select"_(
                            "Group"_("Project"_("LINEITEM"_, "As"_("l_orderkey"_, "l_orderkey"_,
                                                                   "l_quantity"_, "l_quantity"_)),
                                     "By"_("l_orderkey"_),
                                     "As"_("sum_l_quantity"_, "Sum"_("l_quantity"_))),
                            "Where"_("Greater"_("sum_l_quantity"_, 300))), // NOLINT
                        "Project"_(
                            "Join"_("Project"_("CUSTOMER"_, "As"_("c_custkey"_, "c_custkey"_)),
                                    "Project"_("ORDERS"_,
                                               "As"_("o_orderkey"_, "o_orderkey"_, "o_custkey"_,
                                                     "o_custkey"_, "o_orderdate"_, "o_orderdate"_,
                                                     "o_totalprice"_, "o_totalprice"_)),
                                    "Where"_("Equal"_("c_custkey"_, "o_custkey"_))),
                            "As"_("o_orderkey"_, "o_orderkey"_, "o_custkey"_, "o_custkey"_,
                                  "o_orderdate"_, "o_orderdate"_, "o_totalprice"_,
                                  "o_totalprice"_)),
                        "Where"_("Equal"_("l_orderkey"_, "o_orderkey"_))),
                    "As"_("o_orderkey"_, "o_orderkey"_, "o_orderdate"_, "o_orderdate"_,
                          "o_totalprice"_, "o_totalprice"_, "o_custkey"_, "o_custkey"_,
                          "sum_l_quantity"_, "sum_l_quantity"_)),
                "By"_("o_custkey"_, "o_orderkey"_, "o_orderdate"_, "o_totalprice"_),
                "Sum"_("sum_l_quantity"_)),
            "By"_("o_totalprice"_, "desc"_, "o_orderdate"_), 100));

    // Custom
  }
  return queries;
}

static void TPCH_Benchmark(benchmark::State& state, int queryIdx, int dataSize) {
  initStorageEngine_TPCH(dataSize);

  auto eval = [](auto&& expression) {
    boss::expressions::ExpressionSpanArguments spans;
    spans.emplace_back(boss::expressions::Span<std::string>(librariesToTest()));
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

  auto const& queryName = queryNames().find(queryIdx)->second;
  auto const& query = queries().find(queryIdx)->second;

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

void initAndRunBenchmarks(int argc, char** argv) {
  for(int i = 0; i < argc; ++i) {
    if(std::string("--library") == argv[i]) {
      if(++i < argc) {
        librariesToTest().emplace_back(argv[i]);
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
  for(int dataSize : std::vector<int>{1 , 10, 100, 1000}) {
    for(int query : std::vector<int>{TPCH_Q6}) {
      std::ostringstream testName;
      auto const& queryName = queryNames()[DATASETS::TPCH + query];
      testName << queryName << "/";
      testName << dataSize << "MB";
      benchmark::RegisterBenchmark(testName.str(), TPCH_Benchmark, DATASETS::TPCH + query,
                                   dataSize);
    }
  }

  // register custom benchmarks

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