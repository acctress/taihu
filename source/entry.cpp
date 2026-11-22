#include <xiao/version.hh>
#include <cstdint>
#include <print>

std::int32_t main( )
{
    std::println( "taihu {} (xiao {})", TAIHU_VERSION, xiao::version );
}
