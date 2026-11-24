#include <xiao/version.hh>

import std;
import taihu.support;
import taihu.lex;

using namespace taihu::support;
using enum taihu::lex::TokenKind;
using enum taihu::lex::Encoding;

std::int32_t main()
{
    use_utf8_console();
    std::println( "taihu {} (xiao {})", TAIHU_VERSION, xiao::version );

    struct Expected
    {
        std::string_view      spelling;
        taihu::lex::TokenKind kind;
        taihu::lex::Encoding  encoding;
    };

    constexpr Expected expected[] {
        { "1'000'000", pp_number, none },    { "0x'FF", pp_number, none },       { "1", pp_number, none },
        { "''", char_constant, none },       { "3.14'15e+'2", pp_number, none }, { "u8'a'", char_constant, utf8 },
        { "u8\"s\"", string_literal, utf8 }, { "u8", identifier, none },         { "'b'", char_constant, none },
    };

    SourceManager     sources;
    Interner          interner;
    DiagnosticEngine  diags { sources };
    const auto        file = sources.add_file( "c23.c", "1'000'000 0x'FF 1'' 3.14'15e+'2 u8'a' u8\"s\" u8 'b'" );
    taihu::lex::Lexer lexer { sources, file, diags, interner };

    bool pass = true;
    for ( const auto& want : expected )
    {
        const auto token    = lexer.next();
        const auto offset   = sources.decompose( token.location ).offset;
        const auto spelling = sources.text( file ).substr( offset, token.length );
        const bool ok       = spelling == want.spelling && token.kind == want.kind && token.encoding == want.encoding;
        pass                = pass && ok;
        std::println( "{} {:<12} {:<14} {}", ok ? "pass" : "FAIL", spelling, token.kind, token.encoding );
    }

    pass = pass && lexer.next().is( eof ) && diags.error_count() == 1;
    std::println( "{}", pass ? "all passed" : "FAILED" );

    const DiagnosticRenderer renderer { sources, theme_classic, color_enabled( stdout ) };
    std::print( "\n{}", renderer.render_all( diags ) );

    return pass ? 0 : 1;
}
