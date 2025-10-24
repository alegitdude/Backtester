#include <cstdint>
#include <limits>
#include <databento/record.hpp>

using namespace databento;

struct PriceLevel {
  int64_t price{kUndefPrice};
  uint32_t size{0};
  uint32_t count{0};

  bool IsEmpty() const { return price == kUndefPrice; }
  operator bool() const { return !IsEmpty(); }
};

