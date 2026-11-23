#include <xiao/version.hh>

import std;
import taihu.support;

std::int32_t main()
{
    std::println( "taihu {} (xiao {})", TAIHU_VERSION, xiao::version );

    taihu::support::SourceManager source_man;
    auto f = source_man.add_file( "a.c", "\xEF\xBB\xBFint y;\r\nreturn 0;\r" );
}
