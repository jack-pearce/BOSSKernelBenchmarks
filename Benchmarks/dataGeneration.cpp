#ifndef DATAGENERATION_CPP
#define DATAGENERATION_CPP

#include "utilities.cpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <set>
#include <vector>
#include <cassert>

template <typename T>
std::vector<T> generateLogDistribution(int numPoints, double minValue, double maxValue) {
  std::vector<T> points;

  for(auto i = 0; i < numPoints; ++i) {
    auto t = i / static_cast<double>(numPoints - 1); // Normalized parameter between 0 and 1
    auto value =
        std::pow(10.0, t * (std::log10(maxValue) - std::log10(minValue)) + std::log10(minValue));
    points.push_back(static_cast<T>(value));
  }

  auto unique_end = std::unique(points.begin(), points.end());
  points.erase(unique_end, points.end());

  if(points[points.size() - 1] != static_cast<int>(maxValue)) {
    points[points.size() - 1] = static_cast<int>(maxValue);
  }

  return points;
}

template <typename T>
std::vector<T> generateUniformDistribution(size_t n, T lowerBound, T upperBound) {
  static_assert(std::is_integral<T>::value, "Must be an integer type");

#if False
  std::cout << "Generating data in memory... ";
  std::cout.flush();
#endif

  unsigned int seed = 1;
  std::mt19937 gen(seed);

  std::uniform_int_distribution<T> distribution(lowerBound, upperBound);

  std::vector<T> data;
  data.reserve(n);

  for(size_t i = 0; i < n; ++i) {
    data.push_back(distribution(gen));
  }

#if False
  std::cout << "Complete" << std::endl;
#endif
  return data;
}

template <typename T>
std::vector<T> generatePartiallySortedOneToOneHundred(int n, int numRepeats,
                                                      float percentageRandom) {
  static_assert(std::is_integral<T>::value, "Must be an integer type");
  assert(percentageRandom >= 0.0 && percentageRandom <= 100.0);

  if (static_cast<int>(percentageRandom) == 100) {
    return generateUniformDistribution<T>(n, 1, 100);
  }

#if False
  std::cout << "Generating data in memory... ";
  std::cout.flush();
#endif

  std::vector<T> data;
  data.reserve(n);

  int sectionSize = 100 * numRepeats;
  int sections = n / (sectionSize);
  int elementsToShufflePerSection =
      static_cast<int>(0.5 * (percentageRandom / 100.0) * static_cast<float>(sectionSize));

#if False
  std::cout << sectionSize << " section size" << std::endl;
  std::cout << sections << " sections" << std::endl;
  std::cout << elementsToShufflePerSection << " pairs to shuffle per section" << std::endl;
#endif

  unsigned int seed = 1;
  std::mt19937 gen(seed);

  bool increasing = true;
  T lowerBound = 1;
  T upperBound = 100;

  T value;
  auto index = 0;
  for(auto i = 0; i < sections; ++i) {
    value = increasing ? lowerBound : upperBound;
    for(auto j = 0; j < 100; ++j) {
      for(auto k = 0; k < numRepeats; ++k) {
        data.push_back(value);
        ++index;
      }
      value = increasing ? value + 1 : value - 1;
    }
    increasing = !increasing;

    std::set<int> selectedIndexes = {};
    int index1, index2;
    for(auto swapCount = 0; swapCount < elementsToShufflePerSection; ++swapCount) {
      std::uniform_int_distribution<int> dis(1, sectionSize);

      index1 = dis(gen);
      while(selectedIndexes.count(index1) > 0) {
        index1 = dis(gen);
      }

      index2 = dis(gen);
      while(selectedIndexes.count(index2) > 0) {
        index2 = dis(gen);
      }

      selectedIndexes.insert(index1);
      selectedIndexes.insert(index2);
      std::swap(data[index - index1], data[index - index2]);

#if False
      std::cout << "Swapped indexes: " << index1 << ", " << index2 << std::endl;
#endif
    }

#if False
    for(int x = 0; x < 100; ++x) {
      for(int y = 0; y < numRepeats; ++y) {
        std::cout << data[index - sectionSize + (x * numRepeats) + y] << std::endl;
      }
    }
#endif
  }
#if False
  std::cout << "Complete" << std::endl;
#endif
  return data;
}

std::vector<uint32_t> generateClusteringOrder(int n, int spreadInCluster) {
  uint32_t numBuckets = 1 + n - spreadInCluster;
  std::vector<std::vector<uint32_t>> buckets(numBuckets, std::vector<uint32_t>());

  uint32_t seed = 1;
  std::mt19937 gen(seed);

  for(int i = 0; i < n; i++) {
    std::uniform_int_distribution<uint32_t> distribution(std::max(0, 1 + i - spreadInCluster),
                                                         std::min(i, n - spreadInCluster));
    buckets[distribution(gen)].push_back(i);
  }

  std::vector<uint32_t> result;
  for(uint32_t i = 0; i < numBuckets; i++) {
    std::shuffle(buckets[i].begin(), buckets[i].end(), gen);
    result.insert(result.end(), buckets[i].begin(), buckets[i].end());
  }

  return result;
}

template <typename T>
boss::Span<T> applyClusteringOrderToSpan(const boss::Span<T>& orderedSpan,
                                         boss::Span<T>&& clusteredSpan,
                                         const std::vector<uint32_t>& positions, int n) {
  for(auto i = 0; i < n; i++) {
    clusteredSpan[i] = orderedSpan[positions[i]];
  }
  return clusteredSpan;
}

#endif // DATAGENERATION_CPP