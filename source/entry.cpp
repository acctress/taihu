#include <xiao/version.hh>
#include <cstdint>
#include <print>

std::int32_t main( )
{
    std::println( "taihu 0.0.1 (xiao {})", xiao::version );
}
