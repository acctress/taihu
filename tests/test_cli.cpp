#include <catch2/catch_test_macros.hpp>

#include "support.hh"

using namespace taihu;
using support::DiagnosticId;
using support::ParseResult;
using test::Parsed;

namespace
{
    using Strings = std::vector< std::string >;
}

TEST_CASE( "a flag sets its field", "[cli]" )
{
    const Parsed parsed { "--dump-tokens", "main.c" };
    CHECK( parsed.result == ParseResult::run );
    CHECK( parsed.dump_tokens );
    CHECK( parsed.inputs == Strings { "main.c" } );
}

TEST_CASE( "a flag left out stays unset", "[cli]" )
{
    const Parsed parsed { "main.c" };
    CHECK( parsed.result == ParseResult::run );
    CHECK_FALSE( parsed.dump_tokens );
}

TEST_CASE( "a value as the next argument, joined, or after =", "[cli]" )
{
    SECTION( "separate" )
    {
        const Parsed parsed { "-o", "x.exe", "main.c" };
        CHECK( parsed.result == ParseResult::run );
        CHECK( parsed.output == "x.exe" );
    }

    SECTION( "joined" )
    {
        const Parsed parsed { "-ox.exe", "main.c" };
        CHECK( parsed.result == ParseResult::run );
        CHECK( parsed.output == "x.exe" );
    }

    SECTION( "equals" )
    {
        const Parsed parsed { "-o=x.exe", "main.c" };
        CHECK( parsed.result == ParseResult::run );
        CHECK( parsed.output == "x.exe" );
    }
}

TEST_CASE( "a repeated option collects every value", "[cli]" )
{
    const Parsed parsed { "-I", "a", "-Ib", "-I=c", "main.c" };
    CHECK( parsed.result == ParseResult::run );
    CHECK( parsed.includes == Strings { "a", "b", "c" } );
    CHECK( parsed.inputs == Strings { "main.c" } );
}

TEST_CASE( "an integer value", "[cli]" )
{
    const Parsed parsed { "-j", "8", "main.c" };
    CHECK( parsed.result == ParseResult::run );
    CHECK( parsed.jobs == 8 );
}

TEST_CASE( "everything after -- is positional", "[cli]" )
{
    const Parsed parsed { "main.c", "--", "-cuh.c" };
    CHECK( parsed.result == ParseResult::run );
    CHECK( parsed.inputs == Strings { "main.c", "-cuh.c" } );
}

TEST_CASE( "a lone - is positional", "[cli]" )
{
    const Parsed parsed { "-" };
    CHECK( parsed.result == ParseResult::run );
    CHECK( parsed.inputs == Strings { "-" } );
}

TEST_CASE( "a typo gets a suggestion", "[cli]" )
{
    const Parsed parsed { "--dump-tokns", "main.c" };
    CHECK( parsed.result == ParseResult::error );
    REQUIRE( parsed.reported( DiagnosticId::unknown_option ) == 1 );

    const auto& diagnostic = parsed.first( DiagnosticId::unknown_option );
    REQUIRE( diagnostic.suggestions.size() == 1 );
    REQUIRE( diagnostic.suggestions[ 0 ].edits.size() == 1 );
    CHECK( diagnostic.suggestions[ 0 ].edits[ 0 ].replacement == "--dump-tokens" );
}

TEST_CASE( "a far-off name gets no suggestion", "[cli]" )
{
    const Parsed parsed { "--frobnicate-everything", "main.c" };
    CHECK( parsed.result == ParseResult::error );
    REQUIRE( parsed.reported( DiagnosticId::unknown_option ) == 1 );
    CHECK( parsed.first( DiagnosticId::unknown_option ).suggestions.empty() );
}

TEST_CASE( "a value option at the end is missing its value", "[cli]" )
{
    const Parsed parsed { "main.c", "-o" };
    CHECK( parsed.result == ParseResult::error );
    CHECK( parsed.reported( DiagnosticId::missing_value ) == 1 );
    CHECK( parsed.diags.all().size() == 1 );
}

TEST_CASE( "a value that does not parse is invalid", "[cli]" )
{
    const Parsed parsed { "-j", "abc", "main.c" };
    CHECK( parsed.result == ParseResult::error );
    CHECK( parsed.reported( DiagnosticId::invalid_value ) == 1 );
    CHECK( parsed.diags.all().size() == 1 );
}

TEST_CASE( "no inputs", "[cli]" )
{
    const Parsed parsed { "--dump-tokens" };
    CHECK( parsed.result == ParseResult::error );
    CHECK( parsed.reported( DiagnosticId::missing_input ) == 1 );
}

TEST_CASE( "help and version", "[cli]" )
{
    CHECK( Parsed { "-h" }.result == ParseResult::help );
    CHECK( Parsed { "--help" }.result == ParseResult::help );
    CHECK( Parsed { "--version" }.result == ParseResult::version );
    CHECK( Parsed { "-V" }.result == ParseResult::version );
}
