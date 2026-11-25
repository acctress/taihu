#include <catch2/catch_test_macros.hpp>

#include "support.hh"

using namespace taihu;
using lex::Encoding;
using lex::TokenFlags;
using lex::TokenKind;
using support::DiagnosticId;
using test::Lexed;

namespace
{
    using Kinds = std::vector< TokenKind >;
}

TEST_CASE( "longest match splits x+++++y", "[lexer]" )
{
    const Lexed lexed { "x+++++y" };
    CHECK( lexed.kinds()
           == Kinds { TokenKind::identifier, TokenKind::plus_plus, TokenKind::plus_plus, TokenKind::plus, TokenKind::identifier,
                      TokenKind::eof } );
    CHECK( lexed.diags.all().empty() );
}

TEST_CASE( "1Ex is a single pp-number", "[lexer]" )
{
    const Lexed lexed { "1Ex" };
    CHECK( lexed.kinds() == Kinds { TokenKind::pp_number, TokenKind::eof } );
    CHECK( lexed.spelling( 0 ) == "1Ex" );
}

TEST_CASE( "every punctuator", "[lexer]" )
{
    using enum TokenKind;
    const std::vector< std::pair< std::string_view, TokenKind > > table {
        { "[", l_square },
        { "]", r_square },
        { "(", l_paren },
        { ")", r_paren },
        { "{", l_brace },
        { "}", r_brace },
        { ".", period },
        { "->", arrow },
        { "++", plus_plus },
        { "--", minus_minus },
        { "&", amp },
        { "*", star },
        { "+", plus },
        { "-", minus },
        { "~", tilde },
        { "!", exclaim },
        { "/", slash },
        { "%", percent },
        { "<<", less_less },
        { ">>", greater_greater },
        { "<", less },
        { ">", greater },
        { "<=", less_equal },
        { ">=", greater_equal },
        { "==", equal_equal },
        { "!=", exclaim_equal },
        { "^", caret },
        { "|", pipe },
        { "&&", amp_amp },
        { "||", pipe_pipe },
        { "?", question },
        { ":", colon },
        { ";", semi },
        { "...", ellipsis },
        { "=", equal },
        { "*=", star_equal },
        { "/=", slash_equal },
        { "%=", percent_equal },
        { "+=", plus_equal },
        { "-=", minus_equal },
        { "<<=", less_less_equal },
        { ">>=", greater_greater_equal },
        { "&=", amp_equal },
        { "^=", caret_equal },
        { "|=", pipe_equal },
        { ",", comma },
        { "#", hash },
        { "##", hash_hash },
    };

    for ( const auto& [ text, kind ] : table )
    {
        INFO( text );
        const Lexed lexed { text };
        CHECK( lexed.kinds() == Kinds { kind, eof } );
        CHECK( lexed.spelling( 0 ) == text );
    }
}

TEST_CASE( "digraphs", "[lexer]" )
{
    const Lexed lexed { "<: :> <% %> %: %:%:" };
    CHECK( lexed.kinds()
           == Kinds { TokenKind::l_square, TokenKind::r_square, TokenKind::l_brace, TokenKind::r_brace, TokenKind::hash,
                      TokenKind::hash_hash, TokenKind::eof } );
    CHECK( lexed.spelling( 5 ) == "%:%:" );
}

TEST_CASE( "a..b is two periods", "[lexer]" )
{
    const Lexed lexed { "a..b" };
    CHECK( lexed.kinds()
           == Kinds { TokenKind::identifier, TokenKind::period, TokenKind::period, TokenKind::identifier, TokenKind::eof } );
}

TEST_CASE( "%:% is hash then percent", "[lexer]" )
{
    const Lexed lexed { "%:%" };
    CHECK( lexed.kinds() == Kinds { TokenKind::hash, TokenKind::percent, TokenKind::eof } );
}

TEST_CASE( "C23 digit separators and u8 character constants", "[lexer]" )
{
    const Lexed lexed { "1'000'000 0x'FF 1'' 3.14'15e+'2 u8'a' u8\"s\" u8 'b'" };
    CHECK( lexed.kinds()
           == Kinds { TokenKind::pp_number, TokenKind::pp_number, TokenKind::pp_number, TokenKind::char_constant, TokenKind::pp_number,
                      TokenKind::char_constant, TokenKind::string_literal, TokenKind::identifier, TokenKind::char_constant,
                      TokenKind::eof } );

    CHECK( lexed.spelling( 0 ) == "1'000'000" );
    CHECK( lexed.spelling( 1 ) == "0x'FF" );
    CHECK( lexed.spelling( 2 ) == "1" );
    CHECK( lexed.spelling( 3 ) == "''" );
    CHECK( lexed.spelling( 4 ) == "3.14'15e+'2" );
    CHECK( lexed.spelling( 5 ) == "u8'a'" );
    CHECK( lexed.spelling( 7 ) == "u8" );

    CHECK( lexed.tokens[ 5 ].encoding == Encoding::utf8 );
    CHECK( lexed.tokens[ 6 ].encoding == Encoding::utf8 );
    CHECK( lexed.tokens[ 8 ].encoding == Encoding::none );

    CHECK( lexed.reported( DiagnosticId::empty_char_constant ) == 1 );
    CHECK( lexed.diags.all().size() == 1 );
}

TEST_CASE( "line splice inside a token", "[lexer]" )
{
    const Lexed lexed { "re\\\nturn" };
    REQUIRE( lexed.kinds() == Kinds { TokenKind::identifier, TokenKind::eof } );
    CHECK( lexed.tokens[ 0 ].symbol->text == "return" );
    CHECK( lexed.tokens[ 0 ].has( TokenFlags::needs_cleaning ) );
    CHECK( lexed.spelling( 0 ) == "re\\\nturn" );
}

TEST_CASE( "a UCN and its UTF-8 spelling share a symbol", "[lexer]" )
{
    const Lexed lexed { "caf\\u00E9 caf\xC3\xA9" };
    REQUIRE( lexed.kinds() == Kinds { TokenKind::identifier, TokenKind::identifier, TokenKind::eof } );
    CHECK( lexed.tokens[ 0 ].symbol == lexed.tokens[ 1 ].symbol );
    CHECK( lexed.tokens[ 0 ].symbol->text == "caf\xC3\xA9" );
    CHECK( lexed.tokens[ 0 ].has( TokenFlags::needs_cleaning ) );
    CHECK_FALSE( lexed.tokens[ 1 ].has( TokenFlags::needs_cleaning ) );
    CHECK( lexed.diags.all().empty() );
}

TEST_CASE( "bad UCNs", "[lexer]" )
{
    SECTION( "basic character" )
    {
        const Lexed lexed { "\\u0041" };
        CHECK( lexed.reported( DiagnosticId::invalid_universal_character ) == 1 );
        CHECK( lexed.diags.all().size() == 1 );
    }

    SECTION( "too few digits" )
    {
        const Lexed lexed { "\\u12" };
        CHECK( lexed.reported( DiagnosticId::incomplete_universal_character ) == 1 );
        CHECK( lexed.diags.all().size() == 1 );
    }

    SECTION( "surrogate" )
    {
        const Lexed lexed { "\\uD800" };
        CHECK( lexed.reported( DiagnosticId::invalid_universal_character ) == 1 );
        CHECK( lexed.diags.all().size() == 1 );
    }
}

TEST_CASE( "literal prefixes", "[lexer]" )
{
    const Lexed lexed { R"("a" u8"a" u"a" U"a" L"a" 'a' u8'a' u'a' U'a' L'a')" };
    REQUIRE( lexed.tokens.size() == 11 );

    const std::array encodings { Encoding::none, Encoding::utf8, Encoding::utf16, Encoding::utf32, Encoding::wide };
    for ( std::size_t i {}; i < 10; ++i )
    {
        INFO( lexed.spelling( i ) );
        CHECK( lexed.tokens[ i ].kind == ( i < 5 ? TokenKind::string_literal : TokenKind::char_constant ) );
        CHECK( lexed.tokens[ i ].encoding == encodings[ i % 5 ] );
    }
    CHECK( lexed.diags.all().empty() );
}

TEST_CASE( "an unclosed string still lexes the next line", "[lexer]" )
{
    const Lexed lexed { "\"abc\nint" };
    REQUIRE( lexed.kinds() == Kinds { TokenKind::string_literal, TokenKind::identifier, TokenKind::eof } );
    CHECK( lexed.spelling( 0 ) == "\"abc" );
    CHECK( lexed.spelling( 1 ) == "int" );
    CHECK( lexed.tokens[ 1 ].has( TokenFlags::at_line_start ) );
    CHECK( lexed.reported( DiagnosticId::missing_terminating_quote ) == 1 );
    CHECK( lexed.diags.all().size() == 1 );
}

TEST_CASE( "empty character constant", "[lexer]" )
{
    const Lexed lexed { "''" };
    CHECK( lexed.kinds() == Kinds { TokenKind::char_constant, TokenKind::eof } );
    CHECK( lexed.reported( DiagnosticId::empty_char_constant ) == 1 );
}

TEST_CASE( "unclosed comment", "[lexer]" )
{
    const Lexed lexed { "a /* never closed" };
    CHECK( lexed.kinds() == Kinds { TokenKind::identifier, TokenKind::eof } );
    CHECK( lexed.reported( DiagnosticId::unterminated_comment ) == 1 );
    CHECK( lexed.diags.all().size() == 1 );
}

TEST_CASE( "a comment counts as leading space", "[lexer]" )
{
    const Lexed lexed { "a/**/b" };
    REQUIRE( lexed.kinds() == Kinds { TokenKind::identifier, TokenKind::identifier, TokenKind::eof } );
    CHECK_FALSE( lexed.tokens[ 0 ].has( TokenFlags::leading_space ) );
    CHECK( lexed.tokens[ 1 ].has( TokenFlags::leading_space ) );
    CHECK_FALSE( lexed.tokens[ 1 ].has( TokenFlags::at_line_start ) );
}

TEST_CASE( "a newline resets leading space", "[lexer]" )
{
    const Lexed lexed { "a \nb c" };
    REQUIRE( lexed.kinds() == Kinds { TokenKind::identifier, TokenKind::identifier, TokenKind::identifier, TokenKind::eof } );
    CHECK( lexed.tokens[ 0 ].has( TokenFlags::at_line_start ) );
    CHECK( lexed.tokens[ 1 ].has( TokenFlags::at_line_start ) );
    CHECK_FALSE( lexed.tokens[ 1 ].has( TokenFlags::leading_space ) );
    CHECK( lexed.tokens[ 2 ].has( TokenFlags::leading_space ) );
    CHECK_FALSE( lexed.tokens[ 2 ].has( TokenFlags::at_line_start ) );
}

TEST_CASE( "next_header_name", "[lexer]" )
{
    test::Buffer buffer { "<stdio.h> \"x.h\" <broken" };
    lex::Lexer   lexer { buffer.sources, buffer.file, buffer.diags, buffer.interner };

    const auto angled = lexer.next_header_name();
    REQUIRE( angled );
    CHECK( angled->kind == TokenKind::header_name );
    CHECK( buffer.spelling( *angled ) == "<stdio.h>" );

    const auto quoted = lexer.next_header_name();
    REQUIRE( quoted );
    CHECK( buffer.spelling( *quoted ) == "\"x.h\"" );
    CHECK( quoted->has( TokenFlags::leading_space ) );

    CHECK_FALSE( lexer.next_header_name() );

    const auto less = lexer.next();
    CHECK( less.kind == TokenKind::less );
    CHECK( less.has( TokenFlags::leading_space ) );

    const auto name = lexer.next();
    CHECK( name.kind == TokenKind::identifier );
    CHECK( buffer.spelling( name ) == "broken" );

    CHECK( lexer.next().kind == TokenKind::eof );
    CHECK( buffer.diags.all().empty() );
}

TEST_CASE( "trigraphs are off by default", "[lexer]" )
{
    const Lexed lexed { "?\?=" };
    CHECK( lexed.kinds() == Kinds { TokenKind::question, TokenKind::question, TokenKind::equal, TokenKind::eof } );
    CHECK( lexed.diags.all().empty() );
}
