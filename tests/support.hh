#pragma once

import std;
import taihu.support;
import taihu.lex;

namespace taihu::test
{
    struct Buffer
    {
        support::SourceManager   sources;
        support::Interner        interner;
        support::DiagnosticEngine diags { sources };
        support::FileId          file;

        explicit Buffer( const std::string_view text ) : file { sources.add_file( "<test>", std::string { text } ) } { }

        Buffer( const Buffer& )             = delete;
        Buffer& operator= ( const Buffer& ) = delete;
        Buffer( Buffer&& )                  = delete;
        Buffer& operator= ( Buffer&& )      = delete;

        [[nodiscard]] std::string_view spelling( const lex::Token& token ) const
        {
            const auto [ owner, offset ] = sources.decompose( token.location );
            return sources.text( owner ).substr( offset, token.length );
        }

        [[nodiscard]] std::size_t reported( const support::DiagnosticId id ) const
        { return static_cast< std::size_t >( std::ranges::count( diags.all(), id, &support::Diagnostic::id ) ); }
    };

    struct Lexed : Buffer
    {
        std::vector< lex::Token > tokens;

        explicit Lexed( const std::string_view text, const lex::LexerOptions options = {} ) : Buffer { text }
        {
            lex::Lexer lexer { sources, file, diags, interner, options };
            for ( ;; )
            {
                tokens.push_back( lexer.next() );
                if ( tokens.back().is( lex::TokenKind::eof ) ) break;
            }
        }

        using Buffer::spelling;

        [[nodiscard]] std::string_view spelling( const std::size_t i ) const
        { return spelling( tokens.at( i ) ); }

        [[nodiscard]] std::vector< lex::TokenKind > kinds() const
        {
            return tokens | std::views::transform( &lex::Token::kind ) | std::ranges::to< std::vector >();
        }
    };

    struct Parsed
    {
        bool                       dump_tokens {};
        std::string                output;
        int                        jobs {};
        std::vector< std::string > includes;
        std::vector< std::string > inputs;

        support::SourceManager    sources;
        support::DiagnosticEngine diags { sources };
        support::Parser           parser { "taihu", "a C23 compiler built on xiao" };
        support::ParseResult      result {};

        explicit Parsed( const std::initializer_list< const char* > args )
        {
            parser.version( "0.0.0" )
                .flag( "--dump-tokens", dump_tokens, "print tokens" )
                .value( "-o", output, "file", "output file" )
                .value( "-j", jobs, "n", "job count" )
                .values( "-I", includes, "dir", "include directory" )
                .positionals( inputs, "files", "C source files" );

            std::vector< const char* > argv { "taihu" };
            argv.append_range( args );
            result = parser.parse( argv, sources, diags );
        }

        Parsed( const Parsed& )             = delete;
        Parsed& operator= ( const Parsed& ) = delete;
        Parsed( Parsed&& )                  = delete;
        Parsed& operator= ( Parsed&& )      = delete;

        [[nodiscard]] std::size_t reported( const support::DiagnosticId id ) const
        { return static_cast< std::size_t >( std::ranges::count( diags.all(), id, &support::Diagnostic::id ) ); }

        [[nodiscard]] const support::Diagnostic& first( const support::DiagnosticId id ) const
        { return *std::ranges::find( diags.all(), id, &support::Diagnostic::id ); }
    };
} // namespace taihu::test
