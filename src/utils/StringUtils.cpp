#include <algorithm> 
#include <cctype>  
#include <string>

namespace backtester {

namespace stringUtils{

std::string ToLower(const std::string &s) {
    std::string str = s;
    std::transform(str.begin(), str.end(), str.begin(),
                [](unsigned char c){ return std::tolower(c); });
    return str;
}  

}
}