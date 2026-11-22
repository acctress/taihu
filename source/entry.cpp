#include <xiao/version.hh>

import std;
import taihu.support;

std::int32_t main() {
    std::println("taihu {} (xiao {})", TAIHU_VERSION, xiao::version);

    constexpr taihu::support::SourceLocation none{};
    constexpr taihu::support::SourceLocation loc{42};

    static_assert(!none.valid() && loc.valid() && none < loc);
}
