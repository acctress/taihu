#include <catch2/catch_test_macros.hpp>

#include "support.hh"

using namespace taihu;
using lex::TokenFlags;
using support::DiagnosticId;
using test::Preprocessed;

TEST_CASE( "object-like macro", "[pp]" )
{
    const Preprocessed pp { "#define N 42\nint x = N;" };
    CHECK( pp.text() == "int x = 42 ;" );
    CHECK( pp.diags.all().empty() );
}

TEST_CASE( "nested macros expand fully", "[pp]" )
{ CHECK( Preprocessed { "#define W 2\n#define H 3\n#define AREA W * H\nAREA" }.text() == "2 * 3" ); }

TEST_CASE( "self-reference is painted", "[pp]" )
{
    const Preprocessed pp { "#define x x\nx" };
    CHECK( pp.text() == "x" );
    REQUIRE( pp.tokens.size() == 1 );
    CHECK( pp.tokens[ 0 ].has( TokenFlags::no_expand ) );
}

TEST_CASE( "mutual recursion stops", "[pp]" )
{
    CHECK( Preprocessed { "#define a b\n#define b a\na b" }.text() == "a b" );
    CHECK( Preprocessed { "#define x x + 1\nx" }.text() == "x + 1" );
}

TEST_CASE( "empty macro carries its spacing", "[pp]" )
{
    const Preprocessed pp { "#define e\na e b" };
    CHECK( pp.text() == "a b" );
    REQUIRE( pp.tokens.size() == 2 );
    CHECK( pp.tokens[ 1 ].has( TokenFlags::leading_space ) );
}

TEST_CASE( "redefinition", "[pp]" )
{
    CHECK( Preprocessed { "#define A 1  +  2\n#define A 1 + 2" }.reported( DiagnosticId::macro_redefined ) == 0 );
    CHECK( Preprocessed { "#define A 1+2\n#define A 1 + 2" }.reported( DiagnosticId::macro_redefined ) == 1 );
    CHECK( Preprocessed { "#define A 1\n#define A 2\nA" }.text() == "2" );
}

TEST_CASE( "directives", "[pp]" )
{
    CHECK( Preprocessed { "#define A 1\n#undef A\nA" }.text() == "A" );
    CHECK( Preprocessed { "#define X+1" }.reported( DiagnosticId::missing_space_after_macro_name ) == 1 );
    CHECK( Preprocessed { "a # define b" }.text() == "a # define b" );
    CHECK( Preprocessed { "#\nx" }.text() == "x" );
    CHECK( Preprocessed { "#foo" }.reported( DiagnosticId::unknown_directive ) == 1 );
    CHECK( Preprocessed { "#define" }.reported( DiagnosticId::missing_macro_name ) == 1 );
    CHECK( Preprocessed { "#define 1" }.reported( DiagnosticId::macro_name_not_identifier ) == 1 );
    CHECK( Preprocessed { "#undef A B" }.reported( DiagnosticId::extra_tokens_after_directive ) == 1 );
}

TEST_CASE( "a macro body cannot start a directive", "[pp]" )
{ CHECK( Preprocessed { "#define H #\nH define X 1\nX" }.text() == "# define X 1 X" ); }

TEST_CASE( "spec constraints on object-like macros", "[pp]" )
{
    CHECK( Preprocessed { "#define A 1\\\n0\n#define A 10" }.reported( DiagnosticId::macro_redefined ) == 0 );
    CHECK( Preprocessed { "#define\vA 1" }.reported( DiagnosticId::vertical_space_in_directive ) == 1 );
    CHECK( Preprocessed { "\f#define A 1" }.reported( DiagnosticId::vertical_space_in_directive ) == 0 );
    CHECK( Preprocessed { "a\fb" }.diags.all().empty() );
    CHECK( Preprocessed { "#define X __VA_ARGS__" }.reported( DiagnosticId::va_args_outside_variadic ) == 1 );
    CHECK( Preprocessed { "#define X __VA_OPT__(a)" }.reported( DiagnosticId::va_args_outside_variadic ) == 1 );
    CHECK( Preprocessed { "#define X ## a" }.reported( DiagnosticId::hash_hash_at_edge ) == 1 );
    CHECK( Preprocessed { "#define X a ##" }.reported( DiagnosticId::hash_hash_at_edge ) == 1 );
    CHECK( Preprocessed { "#define X ##" }.reported( DiagnosticId::hash_hash_at_edge ) == 1 );
}

TEST_CASE( "## pastes in object-like macros", "[pp][!shouldfail]" )
{ CHECK( Preprocessed { "#define XY x ## y\nXY" }.text() == "xy" ); }

TEST_CASE( "expanded tokens point at the body and the use", "[pp]" )
{
    const Preprocessed pp { "#define N 42\nN" };
    REQUIRE( pp.tokens.size() == 1 );
    const auto& sources = pp.sources;
    const auto  token   = pp.tokens[ 0 ];
    CHECK( sources.is_expansion( token.location ) );
    CHECK( sources.line_column( sources.spelling_location( token.location ) ).line == 1 );
    CHECK( sources.line_column( sources.file_location( token.location ) ).line == 2 );
    CHECK( token.has( TokenFlags::at_line_start ) );
}

TEST_CASE( "printer starts with a line marker and keeps indentation", "[pp][printer]" )
{ CHECK( Preprocessed { "#define N 42\n\n    int x = N;\n" }.printed() == "# 3 \"<test>\"\n    int x = 42;\n" ); }

TEST_CASE( "printer keeps short gaps as blank lines", "[pp][printer]" )
{ CHECK( Preprocessed { "a\n#define X\n\nb" }.printed() == "# 1 \"<test>\"\na\n\n\nb\n" ); }

TEST_CASE( "printer uses a line marker for long gaps", "[pp][printer]" )
{ CHECK( Preprocessed { "a\n\n\n\n\n\n\n\n\n\nb" }.printed() == "# 1 \"<test>\"\na\n# 11 \"<test>\"\nb\n" ); }

TEST_CASE( "printer without line markers keeps at most one blank line", "[pp][printer]" )
{ CHECK( Preprocessed { "a\n\n\n\nb\nc" }.printed( { .line_markers = false } ) == "a\n\nb\nc\n" ); }

TEST_CASE( "printer drops the space an empty macro leaves at the start of a line", "[pp][printer]" )
{ CHECK( Preprocessed { "#define static\n  static int x;" }.printed( { .line_markers = false } ) == "  int x;\n" ); }

TEST_CASE( "printer colours expanded and painted tokens", "[pp][printer]" )
{
    const auto out = Preprocessed { "#define N 42\n#define p p\nN p q" }.printed( { .line_markers = false, .color = true } );
    CHECK( out.contains( "\x1b[38;2;185;170;255m42\x1b[0m" ) );
    CHECK( out.contains( "\x1b[4;38;2;255;212;150mp\x1b[0m" ) );
    CHECK( out.ends_with( " q\n" ) );
    CHECK( Preprocessed { "#define N 42\nN" }.printed( { .line_markers = false } ) == "42\n" );
}

TEST_CASE( "function-like macro", "[pp][function]" )
{
    const Preprocessed pp { "#define max(a, b) ((a) > (b) ? (a) : (b))\nmax(1, x + 2)" };
    CHECK( pp.text() == "( ( 1 ) > ( x + 2 ) ? ( 1 ) : ( x + 2 ) )" );
    CHECK( pp.diags.all().empty() );
}

TEST_CASE( "a name without ( after it is not a call", "[pp][function]" )
{
    CHECK( Preprocessed { "#define f(x) [x]\nint f;" }.text() == "int f ;" );
    CHECK( Preprocessed { "#define f(x) [x]\nf\n(1)" }.text() == "[ 1 ]" );
    CHECK( Preprocessed { "#define f(x) [x]\nf\n#define g 1\ng" }.text() == "f 1" );
    CHECK( Preprocessed { "#define f(x) [x]\nf" }.text() == "f" );
}

TEST_CASE( "a space before ( makes an object-like macro", "[pp][function]" )
{
    CHECK( Preprocessed { "#define f (x)\nf" }.text() == "( x )" );
    CHECK( Preprocessed { "#define f/**/(x)\nf" }.text() == "( x )" );
}

TEST_CASE( "arguments are expanded before they are substituted", "[pp][function]" )
{
    CHECK( Preprocessed { "#define ONE 1\n#define id(x) x\nid(ONE)" }.text() == "1" );
    CHECK( Preprocessed { "#define id(x) x\nid(id(id(2)))" }.text() == "2" );
    CHECK( Preprocessed { "#define f(x) x\n#define g f\ng(3)" }.text() == "3" );
}

TEST_CASE( "commas inside parentheses do not split arguments", "[pp][function]" )
{
    CHECK( Preprocessed { "#define first(a, b) a\nfirst((1, 2), 3)" }.text() == "( 1 , 2 )" );
    CHECK( Preprocessed { "#define one(a) a\none({1, 2})" }.reported( DiagnosticId::macro_argument_count ) == 1 );
}

TEST_CASE( "empty arguments", "[pp][function]" )
{
    CHECK( Preprocessed { "#define f(a, b) [a|b]\nf(,)" }.text() == "[ | ]" );
    CHECK( Preprocessed { "#define f(a) [a]\nf()" }.text() == "[ ]" );
    CHECK( Preprocessed { "#define z() 0\nz()" }.text() == "0" );
    CHECK( Preprocessed { "#define e(x)\na e(1) b" }.text() == "a b" );
}

TEST_CASE( "calls with the wrong number of arguments", "[pp][function]" )
{
    CHECK( Preprocessed { "#define z() 0\nz(1)" }.reported( DiagnosticId::macro_argument_count ) == 1 );
    CHECK( Preprocessed { "#define f(a, b) a\nf(1)" }.reported( DiagnosticId::macro_argument_count ) == 1 );
    CHECK( Preprocessed { "#define f(a) a\nf(1, 2)" }.reported( DiagnosticId::macro_argument_count ) == 1 );
    CHECK( Preprocessed { "#define f(a) a\nf(1" }.reported( DiagnosticId::unterminated_macro_call ) == 1 );
    CHECK( Preprocessed { "#define f(a) a\n#define g(a) a\ng(f(1)" }.reported( DiagnosticId::unterminated_macro_call ) == 1 );
}

TEST_CASE( "parameter list errors", "[pp][function]" )
{
    CHECK( Preprocessed { "#define f(" }.reported( DiagnosticId::expected_parameter ) == 1 );
    CHECK( Preprocessed { "#define f(1) x" }.reported( DiagnosticId::expected_parameter ) == 1 );
    CHECK( Preprocessed { "#define f(a,) x" }.reported( DiagnosticId::expected_parameter ) == 1 );
    CHECK( Preprocessed { "#define f(a, a) a" }.reported( DiagnosticId::duplicate_parameter ) == 1 );
    CHECK( Preprocessed { "#define f(a b) a" }.reported( DiagnosticId::unclosed_parameter_list ) == 1 );
    CHECK( Preprocessed { "#define f(a" }.reported( DiagnosticId::unclosed_parameter_list ) == 1 );
    CHECK( Preprocessed { "#define f(__VA_ARGS__) 1" }.reported( DiagnosticId::va_args_outside_variadic ) == 1 );
    CHECK( Preprocessed { "#define f(1) x\nf(2)" }.text() == "f ( 2 )" );
}

TEST_CASE( "redefinition compares parameters", "[pp][function]" )
{
    CHECK( Preprocessed { "#define f(a) a\n#define f(a) a" }.reported( DiagnosticId::macro_redefined ) == 0 );
    CHECK( Preprocessed { "#define f(a) a\n#define f(b) b" }.reported( DiagnosticId::macro_redefined ) == 1 );
    CHECK( Preprocessed { "#define f(a) a\n#define f a" }.reported( DiagnosticId::macro_redefined ) == 1 );
}

TEST_CASE( "C23 6.10.5.5 EXAMPLE 3 without # and ##", "[pp][function]" )
{
    const Preprocessed pp { "#define x 3\n"
                            "#define f(a) f(x * (a))\n"
                            "#undef x\n"
                            "#define x 2\n"
                            "#define g f\n"
                            "#define z z[0]\n"
                            "#define h g(~\n"
                            "#define m(a) a(w)\n"
                            "#define w 0,1\n"
                            "#define t(a) a\n"
                            "f(y+1) + f(f(z)) % t(t(g)(0) + t)(1);\n"
                            "g(x+(3,4)-w) | h 5) & m\n"
                            "\n"
                            "      (f)^m(m);\n" };
    const Preprocessed expected { "f(2 * (y+1)) + f(2 * (f(2 * (z[0])))) % f(2 * (0)) + t(1);\n"
                                  "f(2 * (2+(3,4)-0,1)) | f(2 * (~ 5)) & f(2 * (0,1))^m(0,1);\n" };
    CHECK( pp.text() == expected.text() );
    CHECK( pp.diags.all().empty() );
}

TEST_CASE( "C23 6.10.5.4 EXAMPLE gives one of its two answers", "[pp][function]" )
{
    const auto text = Preprocessed { "#define f(a) a*g\n#define g(a) f(a)\nf(2)(9)" }.text();
    CHECK( ( text == "2 * 9 * g" || text == "2 * f ( 9 )" ) );
}

TEST_CASE( "printer keeps a call that spans lines on one line", "[pp][printer][function]" )
{ CHECK( Preprocessed { "#define add(a, b) a + b\nint x = add(1,\n    2);" }.printed( { .line_markers = false } ) == "int x = 1 + 2;\n" ); }