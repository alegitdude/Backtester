#include <optional>

class IDataReader {
    public:
       auto readNextEvent() -> auto;

       auto getTimestamp() -> long long;
};
