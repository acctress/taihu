#include <xiao/version.hh>

import std;

std::int32_t main() {
    std::println("taihu {} (xiao {})", TAIHU_VERSION, xiao::version);
}
