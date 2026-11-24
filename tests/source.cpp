#include <catch2/catch_test_macros.hpp>

#include "support.hh"

using namespace taihu;
using support::SourceLocation;
using support::SourceRange;

namespace
{
    constexpr std::string_view text = "#define N 42\n#define M N\nint x = N + M;\n";

    struct Expanded : test::Buffer
    {
        SourceLocation body;
        SourceRange    use;
        SourceLocation token;

        Expanded() : Buffer { text }
        {
            body  = at( "42" );
            use   = { at( "N +" ), at( "N +" ) + 1 };
            token = sources.add_expansion( body, use, 2 );
        }

        [[nodiscard]] SourceLocation at( const std::string_view needle ) const
        { return sources.location( file, static_cast< std::uint32_t >( text.find( needle ) ) ); }
    };
} // namespace

TEST_CASE( "file locations are not expansions", "[source]" )
{
    const Expanded expanded;
    CHECK_FALSE( expanded.sources.is_expansion( expanded.body ) );
    CHECK( expanded.sources.spelling_location( expanded.body ) == expanded.body );
    CHECK( expanded.sources.file_location( expanded.body ) == expanded.body );
}

TEST_CASE( "an expansion maps to its spelling and its use", "[source]" )
{
    const Expanded expanded;
    const auto&    sources = expanded.sources;

    CHECK( sources.is_expansion( expanded.token ) );
    CHECK( sources.spelling_location( expanded.token ) == expanded.body );
    CHECK( sources.spelling_location( expanded.token + 1 ) == expanded.body + 1 );
    CHECK( sources.file_location( expanded.token ) == expanded.use.begin );
    CHECK( sources.file_location( expanded.token + 1 ) == expanded.use.begin );
    CHECK( sources.text( sources.decompose( expanded.token ).file ).empty() );
}

TEST_CASE( "nested expansions", "[source]" )
{
    Expanded   expanded;
    auto&      sources = expanded.sources;
    const auto m_body  = expanded.at( "N\nint" );
    const auto m_use   = expanded.at( "M;" );

    const auto n_in_m = sources.add_expansion( m_body, { m_use, m_use + 1 }, 1 );
    const auto value  = sources.add_expansion( expanded.body, { n_in_m, n_in_m + 1 }, 2 );

    CHECK( sources.spelling_location( value ) == expanded.body );
    CHECK( sources.spelling_location( n_in_m ) == m_body );
    CHECK( sources.file_location( value ) == m_use );

    const auto [ begin, end ] = sources.file_range( { value, value + 2 } );
    CHECK(  begin == m_use );
    CHECK( end == m_use + 1 );
}

TEST_CASE( "file_range of a file range is unchanged", "[source]" )
{
    const Expanded    expanded;
    const SourceRange range { expanded.body, expanded.body + 2 };
    const auto        [ begin, end ] = expanded.sources.file_range( range );
    CHECK(  begin == range.begin );
    CHECK( end == range.end );
}

TEST_CASE( "file_range across an expansion covers the macro name", "[source]" )
{
    const Expanded expanded;
    const auto     [ begin, end ] = expanded.sources.file_range( { expanded.token, expanded.token + 2 } );
    CHECK(  begin == expanded.use.begin );
    CHECK( end == expanded.use.end );
}

TEST_CASE( "the renderer points at the use of a macro", "[source]" )
{
    Expanded expanded;
    expanded.diags.report( support::DiagnosticId::empty_char_constant ).primary( { expanded.token, expanded.token + 2 }, "here" );

    const support::DiagnosticRenderer renderer { expanded.sources, support::theme_classic, false };
    const auto                        out = renderer.render_all( expanded.diags );
    CHECK( out.contains( "<test>:3:9" ) );
    CHECK( out.contains( "int x = N + M;" ) );
}
