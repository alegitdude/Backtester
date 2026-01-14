#include "CsvReader.h"

namespace backtester {

bool CsvReader::Open(const std::string& filename) {
    file_.open(filename, std::ios::in);

    if (!file_.is_open()) return false;

}

}