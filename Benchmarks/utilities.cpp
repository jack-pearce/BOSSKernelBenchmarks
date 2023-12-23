#ifndef UTILITIES_CPP
#define UTILITIES_CPP

#include <BOSS.hpp>
#include <ExpressionUtilities.hpp>
#include <fstream>
#include <iostream>

#include "config.hpp"

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

void resetStorageEngine() {
  auto evalStorage = [](boss::Expression&& expression) mutable {
    return boss::evaluate(
        "EvaluateInEngines"_("List"_(librariesToTest[0]), std::move(expression)));
  };

  if(latestDataSet == "TPCH") {
    evalStorage("DropTable"_("REGION"_));
    evalStorage("DropTable"_("NATION"_));
    evalStorage("DropTable"_("PART"_));
    evalStorage("DropTable"_("SUPPLIER"_));
    evalStorage("DropTable"_("PARTSUPP"_));
    evalStorage("DropTable"_("CUSTOMER"_));
    evalStorage("DropTable"_("ORDERS"_));
    evalStorage("DropTable"_("LINEITEM"_));
  } else if(latestDataSet == "selectivity_sweep_uniform_dis") {
    evalStorage("DropTable"_("UNIFORM_DIS"_));
  } else if(latestDataSet == "randomness_sweep_sorted_dis") {
    evalStorage("DropTable"_("PARTIALLY_SORTED_DIS"_));
  } else if(latestDataSet == "tpch_q6_clustering_sweep") {
    evalStorage("DropTable"_("LINEITEM"_));
    evalStorage("DropTable"_("LINEITEM_CLUSTERED"_));
  }
}

size_t getNumberOfRowsInTable(std::string& filepath) {
  std::ifstream file(filepath);
  if (!file.is_open()) {
    std::cerr << "Error: Unable to open file " << filepath << std::endl;
    return 0;
  }

  size_t rowCount = 0;
  std::string line;
  while (std::getline(file, line)) {
    ++rowCount;
  }

  file.close();
  return rowCount;
}

#endif // UTILITIES_CPP