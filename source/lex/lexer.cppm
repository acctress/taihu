export module taihu.lex:lexer;
import std;
import taihu.support;
import :token;

namespace taihu::lex
{
    [[nodiscard]] constexpr char trigraph( const char ch )
    {
        switch ( ch )
        {
            case '=': return '#';
            case '(': return '[';
            case '/': return '\\';
            case ')': return ']';
            case '\'': return '^';
            case '<': return '{';
            case '!': return '|';
            case '>': return '}';
            case '-': return '~';
            default: return '\0';
        }
    }

    [[nodiscard]] constexpr bool is_digit( const char c )
    { return c >= '0' && c <= '9'; }

    [[nodiscard]] constexpr bool is_nondigit( const char c )
    { return ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || c == '_'; }

    [[nodiscard]] constexpr bool is_ident_start( const char c )
    { return ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || c == '_' || static_cast< unsigned char >( c ) >= 0x80; }

    [[nodiscard]] constexpr bool is_ident_continue( const char c )
    { return is_ident_start( c ) || is_digit( c ); }

    [[nodiscard]] constexpr int hex_value( const char c )
    {
        if ( c >= '0' && c <= '9' ) return c - '0';
        if ( c >= 'a' && c <= 'f' ) return c - 'a' + 10;
        if ( c >= 'A' && c <= 'F' ) return c - 'A' + 10;
        return -1;
    }

    [[nodiscard]] constexpr bool ucn_allowed( const std::uint32_t code )
    {
        if ( code < 0xA0 ) return false;
        if ( code >= 0xD800 && code <= 0xDFFF ) return false;
        return code <= 0x10FFFF;
    }

    void append_utf8( std::string& out, std::uint32_t code )
    {
        if ( !ucn_allowed( code ) ) code = 0xFFFD;
        if ( code < 0x80 ) out += static_cast< char >( code );
        else if ( code < 0x800 )
        {
            out += static_cast< char >( 0xC0 | ( code >> 6 ) );
            out += static_cast< char >( 0x80 | ( code & 0x3F ) );
        }
        else if ( code < 0x10000 )
        {
            out += static_cast< char >( 0xE0 | ( code >> 12 ) );
            out += static_cast< char >( 0x80 | ( ( code >> 6 ) & 0x3F ) );
            out += static_cast< char >( 0x80 | ( code & 0x3F ) );
        }
        else
        {
            out += static_cast< char >( 0xF0 | ( code >> 18 ) );
            out += static_cast< char >( 0x80 | ( ( code >> 12 ) & 0x3F ) );
            out += static_cast< char >( 0x80 | ( ( code >> 6 ) & 0x3F ) );
            out += static_cast< char >( 0x80 | ( code & 0x3F ) );
        }
    }
} // namespace taihu::lex

export namespace taihu::lex
{
    struct LexerOptions
    {
        bool trigraphs { false };
    };

    class Lexer
    {
    public:
        Lexer( const support::SourceManager& sources, support::FileId file, support::DiagnosticEngine& diagnostics,
               support::Interner& interner, LexerOptions options = {} );

        [[nodiscard]] Token next();

        [[nodiscard]] std::optional< Token > next_header_name();

    private:
        struct Chr
        {
            char          value {};
            std::uint32_t size {};
        };

        const support::SourceManager& m_sources;
        support::DiagnosticEngine&    m_diagnostics;
        support::Interner&            m_interner;
        support::FileId               m_file;
        LexerOptions                  m_options;
        const char*                   m_begin {};
        const char*                   m_cur {};
        const char*                   m_end {};
        TokenFlags                    m_pending { TokenFlags::at_line_start };
        bool                          m_cleaning {};

        [[nodiscard]] Chr                     peek( const char* at ) const;
        [[nodiscard]] support::SourceLocation location( const char* at ) const;
        Token                                 make( TokenKind kind, const char* start );
        char                                  advance();
        void                                  skip_ws();
        void                                  skip_line_comment();
        void                                  skip_block_comment();

        Token                     lex_identifier( const char* start );
        Token                     lex_number( const char* start );
        Token                     lex_punctuation( const char* start );
        Token                     lex_quoted( const char* start, char quote, Encoding encoding );
        void                      report_unclosed( const char* start, const char* open, char quote );
        std::uint32_t             lex_ucn( std::string_view where );
        [[nodiscard]] std::string clean( const char* at, const char* end, bool decode_ucns ) const;

        [[nodiscard]] char current() const
        { return peek( m_cur ).value; }

        [[nodiscard]] char ahead( std::uint32_t n ) const
        {
            const char* at = m_cur;
            for ( ; n > 0; --n ) at += peek( at ).size;
            return peek( at ).value;
        }

        bool accept( const char c )
        {
            if ( current() != c ) return false;
            advance();

            return true;
        }
    };

    [[nodiscard]] std::string clean_spelling( const std::string_view raw, const LexerOptions options = {} )
    {
        std::string out;

        for ( std::size_t i {}; i < raw.size(); )
        {
            if ( raw[ i ] == '\\' && i + 1 < raw.size() && raw[ i + 1 ] == '\n' )
            {
                i += 2;
                continue;
            }

            if ( options.trigraphs && raw.substr( i, 2 ) == "??" && i + 2 < raw.size() )
                if ( const char t = trigraph( raw[ i + 2 ] ) )
                {
                    if ( t == '\\' && i + 3 < raw.size() && raw[ i + 3 ] == '\n' )
                    {
                        i += 4;
                        continue;
                    }

                    out += t;
                    i   += 3;
                    continue;
                }

            out += raw[ i++ ];
        }

        return out;
    }

    Lexer::Lexer( const support::SourceManager& sources, const support::FileId file, support::DiagnosticEngine& diagnostics,
                  support::Interner& interner, const LexerOptions options ) :
        m_sources { sources },
        m_diagnostics { diagnostics },
        m_interner { interner },
        m_file { file },
        m_options { options }
    {
        const auto text = sources.text( file );
        m_begin = m_cur = text.data();
        m_end           = text.data() + text.size();
    }

    Token Lexer::next()
    {
        skip_ws();
        m_cleaning = false;

        if ( m_cur >= m_end ) return make( TokenKind::eof, m_cur );

        const auto start = m_cur;
        const auto c     = current();

        if ( c == '"' || c == '\'' ) return lex_quoted( start, c, Encoding::none );

        if ( c == 'u' && ahead( 1 ) == '8' && ( ahead( 2 ) == '"' || ahead( 2 ) == '\'' ) )
        {
            advance();
            advance();

            return lex_quoted( start, current(), Encoding::utf8 );
        }

        if ( ( c == 'u' || c == 'U' || c == 'L' ) && ( ahead( 1 ) == '"' || ahead( 1 ) == '\'' ) )
        {
            advance();
            const auto encoding = c == 'u' ? Encoding::utf16 : c == 'U' ? Encoding::utf32 : Encoding::wide;
            return lex_quoted( start, current(), encoding );
        }

        if ( is_ident_start( c ) || ( c == '\\' && ( ahead( 1 ) == 'u' || ahead( 1 ) == 'U' ) ) ) return lex_identifier( start );
        if ( is_digit( c ) || ( c == '.' && is_digit( ahead( 1 ) ) ) ) return lex_number( start );

        return lex_punctuation( start );
    }

    Token Lexer::lex_identifier( const char* start )
    {
        for ( ;; )
        {
            if ( is_ident_continue( current() ) ) advance();
            else if ( current() == '\\' && ( ahead( 1 ) == 'u' || ahead( 1 ) == 'U' ) )
                lex_ucn( "an identifier" );
            else
                break;
        }

        auto token = make( TokenKind::identifier, start );
        if ( token.has( TokenFlags::needs_cleaning ) ) token.symbol = m_interner.intern( clean( start, m_cur, true ) );
        else
            token.symbol = m_interner.intern( std::string_view { start, m_cur } );
        return token;
    }

    Token Lexer::lex_number( const char* start )
    {
        for ( ;; )
        {
            if ( const char c = current(); ( c == 'e' || c == 'E' || c == 'p' || c == 'P' ) && ( ahead( 1 ) == '+' || ahead( 1 ) == '-' ) )
            {
                advance();
                advance();
            }
            //! pp-number ' digit, pp-number ' nondigit
            else if ( c == '\'' && ( is_digit( ahead( 1 ) ) || is_nondigit( ahead( 1 ) ) ) )
            {
                advance();
                advance();
            }
            else if ( is_ident_continue( c ) || c == '.' )
                advance();
            else if ( c == '\\' && ( ahead( 1 ) == 'u' || ahead( 1 ) == 'U' ) )
                lex_ucn( "a number" );
            else
                break;
        }

        return make( TokenKind::pp_number, start );
    }

    Token Lexer::lex_punctuation( const char* start )
    {
        using enum TokenKind;

        const auto either = [ & ]( const char next, const TokenKind with, const TokenKind without )
        {
            return make( accept( next ) ? with : without, start );
        };

        switch ( advance() )
        {
            case '[': return make( l_square, start );
            case ']': return make( r_square, start );
            case '(': return make( l_paren, start );
            case ')': return make( r_paren, start );
            case '{': return make( l_brace, start );
            case '}': return make( r_brace, start );
            case '?': return make( question, start );
            case ';': return make( semi, start );
            case ',': return make( comma, start );
            case '~': return make( tilde, start );
            case '.':
                if ( current() == '.' && ahead( 1 ) == '.' )
                {
                    advance();
                    advance();
                    return make( ellipsis, start );
                }
                return make( period, start );
            case '-':
                if ( accept( '>' ) ) return make( arrow, start );
                if ( accept( '-' ) ) return make( minus_minus, start );
                return either( '=', minus_equal, minus );
            case '+':
                if ( accept( '+' ) ) return make( plus_plus, start );
                return either( '=', plus_equal, plus );
            case '&':
                if ( accept( '&' ) ) return make( amp_amp, start );
                return either( '=', amp_equal, amp );
            case '|':
                if ( accept( '|' ) ) return make( pipe_pipe, start );
                return either( '=', pipe_equal, pipe );
            case '*': return either( '=', star_equal, star );
            case '/': return either( '=', slash_equal, slash );
            case '!': return either( '=', exclaim_equal, exclaim );
            case '=': return either( '=', equal_equal, equal );
            case '^': return either( '=', caret_equal, caret );
            case '#': return either( '#', hash_hash, hash );
            case ':': return either( '>', r_square, colon );
            case '<':
                if ( accept( ':' ) ) return make( l_square, start );
                if ( accept( '%' ) ) return make( l_brace, start );
                if ( accept( '<' ) ) return either( '=', less_less_equal, less_less );
                return either( '=', less_equal, less );
            case '>':
                if ( accept( '>' ) ) return either( '=', greater_greater_equal, greater_greater );
                return either( '=', greater_equal, greater );
            case '%':
                if ( accept( '>' ) ) return make( r_brace, start );
                if ( accept( ':' ) )
                {
                    if ( current() == '%' && ahead( 1 ) == ':' )
                    {
                        advance();
                        advance();
                        return make( hash_hash, start );
                    }
                    return make( hash, start );
                }
                return either( '=', percent_equal, percent );
            default: return make( other, start );
        }
    }

    Token Lexer::lex_quoted( const char* start, const char quote, const Encoding encoding )
    {
        const auto kind = quote == '"' ? TokenKind::string_literal : TokenKind::char_constant;
        const auto open = m_cur;
        advance();

        bool closed = false;
        bool empty  = true;
        while ( m_cur < m_end )
        {
            const char c = current();
            if ( c == '\n' ) break;
            advance();
            if ( c == quote )
            {
                closed = true;
                break;
            }
            empty = false;
            if ( c == '\\' && m_cur < m_end && current() != '\n' ) advance();
        }

        if ( !closed ) report_unclosed( start, open, quote );
        else if ( empty && kind == TokenKind::char_constant )
            m_diagnostics.report( support::DiagnosticId::empty_char_constant )
                .primary( { location( start ), location( m_cur ) }, "there is nothing between the quotes" )
                .help( "write `'\\''` for a quote character, or `'\\0'` for the null character" );

        auto token     = make( kind, start );
        token.encoding = encoding;
        return token;
    }

    void Lexer::report_unclosed( const char* start, const char* open, const char quote )
    {
        const auto what = quote == '"' ? std::string_view { "string literal" } : std::string_view { "character constant" };

        auto rest = std::string_view { open + 1, m_cur };
        while ( !rest.empty() && ( rest.back() == ' ' || rest.back() == '\t' ) ) rest.remove_suffix( 1 );

        const auto content = rest.find_last_not_of( ";,)]}" );
        const auto keep    = content == std::string_view::npos ? std::size_t { 0 } : content + 1;
        const auto tail    = rest.substr( keep );
        const auto insert  = location( rest.data() + keep );

        auto message = tail.empty() ? std::format( "close the {} at the end of the line", what )
                                    : std::format( "close the {} before the `{}`", what, tail );

        m_diagnostics.report( support::DiagnosticId::missing_terminating_quote, quote, what )
            .primary(
                {
                    location( open ), location( open ) + 1
        },
                std::format( "this {} is never closed", what ) )
            .secondary( { location( start ), location( m_cur ) }, "it runs to the end of the line" )
            .suggest( support::Applicability::likely, std::move( message ), { { { insert, insert }, std::string( 1, quote ) } } );
    }

    std::optional< Token > Lexer::next_header_name()
    {
        const auto saved_cur      = m_cur;
        const auto saved_pending  = m_pending;
        const auto saved_cleaning = m_cleaning;

        skip_ws();
        m_cleaning = false;

        const auto start = m_cur;
        if ( const char open = current(); open == '<' || open == '"' )
        {
            const char close = open == '<' ? '>' : '"';
            advance();
            while ( m_cur < m_end && current() != '\n' && current() != close ) advance();
            if ( current() == close )
            {
                advance();
                return make( TokenKind::header_name, start );
            }
        }

        m_cur      = saved_cur;
        m_pending  = saved_pending;
        m_cleaning = saved_cleaning;
        return std::nullopt;
    }

    std::uint32_t Lexer::lex_ucn( const std::string_view where )
    {
        const auto start = m_cur;
        advance();
        const char kind   = advance();
        const int  digits = kind == 'u' ? 4 : 8;

        std::uint32_t code {};
        int           read {};
        for ( ; read < digits; ++read )
        {
            const int value = hex_value( current() );
            if ( value < 0 ) break;
            code = code << 4 | static_cast< std::uint32_t >( value );
            advance();
        }

        m_cleaning = true;
        const support::SourceRange range { location( start ), location( m_cur ) };

        if ( read < digits )
        {
            m_diagnostics.report( support::DiagnosticId::incomplete_universal_character, kind, digits, read )
                .primary( range, read == 0 ? "no hexadecimal digits here" : "too few hexadecimal digits" )
                .help( kind == 'u' ? "`\\u` takes exactly 4 hexadecimal digits, like `\\u00E9`"
                                   : "`\\U` takes exactly 8 hexadecimal digits, like `\\U0001F600`" );
            return 0xFFFD;
        }

        if ( !ucn_allowed( code ) )
        {
            const auto spelled = clean( start, m_cur, false );
            auto       builder = m_diagnostics.report( support::DiagnosticId::invalid_universal_character, spelled, where );
            builder.primary( range, std::format( "this is U+{:04X}", code ) );

            const bool control = code < 0x20 || ( code >= 0x7F && code < 0xA0 );
            if ( !control && code < 0x7F && is_ident_continue( static_cast< char >( code ) ) )
                builder.suggest( support::Applicability::certain, std::format( "write `{}` directly", static_cast< char >( code ) ),
                                 {
                                     { range, std::string( 1, static_cast< char >( code ) ) }
                } );
            else if ( !control && code < 0x7F )
                builder.note( std::format( "`{}` is in the basic character set, which `\\u` can only name inside character constants "
                                           "and string literals",
                                           static_cast< char >( code ) ) );
            else if ( control )
                builder.note( "control characters can only be named inside character constants and string literals" );
            else if ( code >= 0xD800 && code <= 0xDFFF )
                builder.note( "U+D800 to U+DFFF are UTF-16 surrogate halves, not characters" );
            else
                builder.note( "the largest Unicode code point is U+10FFFF" );
            return 0xFFFD;
        }

        return code;
    }

    std::string Lexer::clean( const char* at, const char* end, const bool decode_ucns ) const
    {
        std::string out;
        while ( at < end )
        {
            const auto c  = peek( at );
            at           += c.size;

            if ( decode_ucns && c.value == '\\' && at < end )
            {
                const auto kind = peek( at );
                if ( kind.value == 'u' || kind.value == 'U' )
                {
                    const int     digits = kind.value == 'u' ? 4 : 8;
                    const char*   scan   = at + kind.size;
                    std::uint32_t code {};
                    int           read {};
                    for ( ; read < digits && scan < end; ++read )
                    {
                        const auto h     = peek( scan );
                        const int  value = hex_value( h.value );
                        if ( value < 0 ) break;
                        code  = code << 4 | static_cast< std::uint32_t >( value );
                        scan += h.size;
                    }

                    if ( read == digits )
                    {
                        append_utf8( out, code );
                        at = scan;
                        continue;
                    }
                }
            }

            out += c.value;
        }
        return out;
    }

    Lexer::Chr Lexer::peek( const char* at ) const
    {
        std::uint32_t size {};
        for ( ;; )
        {
            if ( at[ 0 ] == '\\' && at[ 1 ] == '\n' )
            {
                at   += 2;
                size += 2;
                continue;
            }
            if ( m_options.trigraphs && at[ 0 ] == '?' && at[ 1 ] == '?' )
            {
                if ( const char t = trigraph( at[ 2 ] ) )
                {
                    if ( t == '\\' && at[ 3 ] == '\n' )
                    {
                        at   += 4;
                        size += 4;
                        continue;
                    }

                    return { t, size + 3 };
                }
            }

            return { at[ 0 ], size + 1 };
        }
    }

    support::SourceLocation Lexer::location( const char* at ) const
    { return m_sources.location( m_file, static_cast< std::uint32_t >( at - m_begin ) ); }

    Token Lexer::make( const TokenKind kind, const char* start )
    {
        Token token {
            .kind     = kind,
            .flags    = m_pending,
            .length   = static_cast< std::uint32_t >( m_cur - start ),
            .location = location( start ),
        };

        if ( m_cleaning ) token.flags |= TokenFlags::needs_cleaning;

        m_pending  = TokenFlags::none;
        m_cleaning = false;

        return token;
    }

    char Lexer::advance()
    {
        if ( m_cur >= m_end ) return '\0';
        const auto [ value, size ] = peek( m_cur );
        if ( size > 1 ) m_cleaning = true;
        m_cur = std::min( m_cur + size, m_end );
        return value;
    }

    void Lexer::skip_ws()
    {
        while ( m_cur < m_end )
        {
            switch ( const auto [ value, size ] = peek( m_cur ); value )
            {
                case ' ':
                case '\t':
                    advance();
                    m_pending |= TokenFlags::leading_space;
                    continue;
                case '\v':
                case '\f':
                    advance();
                    m_pending |= TokenFlags::leading_space | TokenFlags::vertical_space;
                    continue;
                case '\n':
                    advance();
                    m_pending = TokenFlags::at_line_start;
                    continue;
                case '/':
                {
                    if ( const auto after = peek( m_cur + size ).value; after == '/' ) skip_line_comment();
                    else if ( after == '*' )
                        skip_block_comment();
                    else
                        return;
                    m_pending |= TokenFlags::leading_space;
                    continue;
                }
                default: return;
            }
        }
    }

    void Lexer::skip_line_comment()
    {
        advance();
        advance();
        while ( m_cur < m_end && peek( m_cur ).value != '\n' ) advance();
    }

    void Lexer::skip_block_comment()
    {
        const auto start = m_cur;
        advance();
        advance();

        while ( m_cur < m_end )
            if ( advance() == '*' && peek( m_cur ).value == '/' )
            {
                advance();
                return;
            }

        m_diagnostics.report( support::DiagnosticId::unterminated_comment )
            .primary( { location( start ), location( start ) + 2 }, "the comment starts here" )
            .secondary( { location( start ), location( m_end ) }, "so everything up to the end of the file is part of it" )
            .note( "comments do not nest, and end at the first `*/`" );
    }
} // namespace taihu::lex
