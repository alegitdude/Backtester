#include <string>

namespace backtester {

  namespace stringUtils {
    std::string ToLower(const std::string& s);
  }

  inline bool AreEqual(std::string_view a, std::string_view b) {
    return a.size() == b.size() &&
      std::equal(a.begin(), a.end(), b.begin(),
        [](unsigned char x, unsigned char y) {
          return std::tolower(x) == std::tolower(y);
        });
  }

}