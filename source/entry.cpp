#include <xiao/version.hh>

import std;
import taihu.support;
import taihu.lex;
import taihu.pp;

using namespace taihu::support;

namespace
{
    struct Options
    {
        bool                       dump_tokens {};
        bool                       preprocess_only {};
        bool                       no_line_markers {};
        std::vector< std::string > defines;
        std::vector< std::string > inputs;
    };

    std::string predefines( const std::vector< std::string >& defines )
    {
        std::string text;
        for ( const std::string_view define : defines )
        {
            if ( const auto equals = define.find( '=' ); equals == std::string_view::npos )
                std::format_to( std::back_inserter( text ), "#define {} 1\n", define );
            else
                std::format_to( std::back_inserter( text ), "#define {} {}\n", define.substr( 0, equals ), define.substr( equals + 1 ) );
        }
        return text;
    }

    void dump_token( const SourceManager& sources, const taihu::pp::Preprocessor& preprocessor, const taihu::lex::Token& token )
    {
        using taihu::lex::TokenFlags;

        const auto [ line, column ] = sources.line_column( sources.file_location( token.location ) );

        std::string line_out = std::format( "{} '{}' {}:{}", token.kind, preprocessor.spelling( token ), line, column );
        if ( token.has( TokenFlags::at_line_start ) ) line_out += " [bol]";
        if ( token.has( TokenFlags::leading_space ) ) line_out += " [space]";
        if ( token.has( TokenFlags::needs_cleaning ) ) line_out += " [clean]";
        if ( token.has( TokenFlags::no_expand ) ) line_out += " [painted]";
        if ( sources.is_expansion( token.location ) ) line_out += " [macro]";
        if ( token.encoding != taihu::lex::Encoding::none ) std::format_to( std::back_inserter( line_out ), " [{}]", token.encoding );
        std::println( "{}", line_out );
    }

    void preprocess( SourceManager& sources, DiagnosticEngine& diags, Interner& interner, const FileId file,
                     const std::optional< FileId > defines, const Options& options )
    {
        taihu::pp::Preprocessor preprocessor { sources, diags, interner, file };
        if ( defines ) preprocessor.push_file( *defines );

        taihu::pp::Printer printer { sources, preprocessor,
                                     { .line_markers = !options.no_line_markers, .color = color_enabled( stdout ) } };
        for ( auto token = preprocessor.next(); !token.is( taihu::lex::TokenKind::eof ); token = preprocessor.next() )
        {
            if ( options.dump_tokens ) dump_token( sources, preprocessor, token );
            if ( options.preprocess_only ) printer.add( token );
        }

        if ( options.preprocess_only ) std::print( "{}", printer.finish() );
    }
} // namespace

std::int32_t main( const int argc, const char* const * argv )
{
    use_utf8_console();

    Options options;
    Parser  parser { "taihu", "a C23 compiler built on xiao" };
    parser.version( TAIHU_VERSION )
        .flag( "--dt", options.dump_tokens, "print each file's tokens after preprocessing and stop" )
        .flag( "-E", options.preprocess_only, "print each file after preprocessing and stop" )
        .flag( "-P", options.no_line_markers, "leave line markers out of the -E output" )
        .values( "-D", options.defines, "name[=value]", "define a macro" )
        .positionals( options.inputs, "files", "C source files" );

    SourceManager            sources;
    DiagnosticEngine         diags { sources };
    Interner                 interner;
    const DiagnosticRenderer renderer { sources, theme_classic, color_enabled( stderr ) };

    switch ( parser.parse( { argv, static_cast< std::size_t >( argc ) }, sources, diags ) )
    {
        case ParseResult::help: std::print( "{}", parser.help() ); return 0;
        case ParseResult::version: std::println( "taihu {} (xiao {})", parser.version_text(), xiao::version ); return 0;
        case ParseResult::error: std::print( stderr, "{}", renderer.render_all( diags ) ); return 2;
        case ParseResult::run: break;
    }

    std::optional< FileId > defines;
    if ( !options.defines.empty() ) defines = sources.add_file( "<cli>", predefines( options.defines ) );

    for ( const auto& input : options.inputs )
    {
        const auto file = sources.load_file( input );
        if ( !file )
        {
            diags.report( DiagnosticId::cannot_open_file, input ).help( "check that the path is right and the file is readable" );
            continue;
        }

        preprocess( sources, diags, interner, *file, defines, options );
    }

    if ( !diags.all().empty() ) std::print( stderr, "{}", renderer.render_all( diags ) );
    return diags.error_count() > 0 ? 1 : 0;
}
