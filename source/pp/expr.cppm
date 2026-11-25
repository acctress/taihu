export module taihu.pp:expr;
import std;
import taihu.support;
import taihu.lex;

export namespace taihu::pp
{
    struct Value
    {
        std::uint64_t bits {};
        bool          is_unsigned {};
    };

    using Spell = std::function< std::string( const lex::Token& ) >;

    class IfExpr
    {
    public:
        IfExpr( std::span< const lex::Token > tokens, support::SourceRange where, std::string_view directive,
                support::DiagnosticEngine& diags, Spell spell );

        [[nodiscard]] std::optional< Value > evaluate();

    private:
        std::span< const lex::Token > m_tokens;
        support::SourceRange          m_where;
        std::string_view              m_directive;
        support::DiagnosticEngine&    m_diags;
        Spell                         m_spell;
        std::size_t                   m_pos {};
        bool                          m_failed {};

        [[nodiscard]] const lex::Token*    peek() const;
        [[nodiscard]] support::SourceRange end() const;
        void                               fail( support::SourceRange range, std::string_view label );
        bool                               expect( lex::TokenKind kind, std::string_view text );

        Value cond( bool live );
        Value binary( int min, bool live );
        Value unary( bool live );
        Value primary( bool live );
        Value apply( const lex::Token& op, Value a, Value b, bool live );
        Value number( const lex::Token& token );
        Value character( const lex::Token& token ) const;
    };

    [[nodiscard]] constexpr int precedence( const lex::TokenKind kind ) noexcept
    {
        using enum lex::TokenKind;
        switch ( kind )
        {
            case pipe_pipe: return 1;
            case amp_amp: return 2;
            case pipe: return 3;
            case caret: return 4;
            case amp: return 5;
            case equal_equal:
            case exclaim_equal: return 6;
            case less:
            case greater:
            case less_equal:
            case greater_equal: return 7;
            case less_less:
            case greater_greater: return 8;
            case plus:
            case minus: return 9;
            case star:
            case slash:
            case percent: return 10;
            default: return 0;
        }
    }

    IfExpr::IfExpr( const std::span< const lex::Token > tokens, const support::SourceRange where, const std::string_view directive,
                    support::DiagnosticEngine& diags, Spell spell ) :
        m_tokens { tokens },
        m_where { where },
        m_directive { directive },
        m_diags { diags },
        m_spell { std::move( spell ) }
    {
    }

    std::optional< Value > IfExpr::evaluate()
    {
        const auto val = cond( true );
        if ( !m_failed && peek() ) fail( peek()->range(), "unexpected token after the expression" );
        if ( m_failed ) return std::nullopt;

        return val;
    }

    const lex::Token* IfExpr::peek() const
    { return m_pos < m_tokens.size() ? &m_tokens[ m_pos ] : nullptr; }

    support::SourceRange IfExpr::end() const
    { return m_tokens.empty() ? m_where : m_tokens.back().range(); }

    void IfExpr::fail( const support::SourceRange range, const std::string_view label )
    {
        if ( !m_failed ) m_diags.report( support::DiagnosticId::bad_if_expression, m_directive ).primary( range, std::string( label ) );
        m_failed = true;
    }

    bool IfExpr::expect( const lex::TokenKind kind, std::string_view text )
    {
        if ( const auto* token = peek(); token && token->is( kind ) )
        {
            ++m_pos;
            return true;
        }

        fail( peek() ? peek()->range() : end(), std::format( "expected `{}` here", text ) );
        return false;
    }

    Value IfExpr::cond( const bool live )
    {
        const auto tst = binary( 1, live );
        if ( m_failed || !peek() || !peek()->is( lex::TokenKind::question ) ) return tst;
        ++m_pos;

        const auto [ yes_bits, yes_is_unsigned ] = cond( live && tst.bits != 0 );
        if ( !expect( lex::TokenKind::colon, ":" ) ) return {};
        const auto [ no_bits, no_is_unsigned ] = cond( live && tst.bits == 0 );

        return { tst.bits != 0 ? yes_bits : no_bits, yes_is_unsigned || no_is_unsigned };
    }

    Value IfExpr::binary( int min, bool live )
    {
        auto lhs = unary( live );

        for ( ;; )
        {
            const auto* op = peek();
            if ( m_failed || !op ) return lhs;

            const auto prec = precedence( op->kind );
            if ( prec == 0 || prec < min ) return lhs;
            ++m_pos;

            if ( op->is( lex::TokenKind::amp_amp ) )
            {
                const auto [ rhs_bits, _ ] = binary( prec + 1, live && lhs.bits != 0 );
                lhs                        = { lhs.bits != 0 && rhs_bits != 0 ? 1u : 0u, false };
                continue;
            }

            if ( op->is( lex::TokenKind::pipe_pipe ) )
            {
                const auto [ rhs_bits, _ ] = binary( prec + 1, live && lhs.bits == 0 );
                lhs                        = { lhs.bits != 0 || rhs_bits != 0 ? 1u : 0u, false };
                continue;
            }

            lhs = apply( *op, lhs, binary( prec + 1, live ), live );
        }
    }

    Value IfExpr::unary( const bool live )
    {
        const auto* token = peek();
        if ( !token )
        {
            fail( end(), "expected a value here" );
            return {};
        }

        switch ( token->kind )
        {
            case lex::TokenKind::plus: ++m_pos; return unary( live );
            case lex::TokenKind::minus:
            {
                ++m_pos;
                const auto [ bits, is_unsigned ] = unary( live );
                return { 0 - bits, is_unsigned };
            }

            case lex::TokenKind::tilde:
            {
                ++m_pos;
                const auto [ bits, is_unsigned ] = unary( live );
                return { ~bits, is_unsigned };
            }

            case lex::TokenKind::exclaim:
            {
                ++m_pos;
                const auto [ bits, _ ] = unary( live );
                return { bits == 0 ? 1u : 0u, false };
            }

            default: return primary( live );
        }
    }

    Value IfExpr::primary( const bool live )
    {
        const auto& token = *peek();
        ++m_pos;

        switch ( token.kind )
        {
            case lex::TokenKind::l_paren:
            {
                const auto val = cond( live );
                expect( lex::TokenKind::r_paren, ")" );
                return val;
            }

            case lex::TokenKind::pp_number: return number( token );
            case lex::TokenKind::char_constant: return character( token );
            case lex::TokenKind::identifier: return { m_spell( token ) == "true" ? 1u : 0u, false };
            case lex::TokenKind::string_literal: fail( token.range(), "strings are not allowed here" ); return {};
            default: fail( token.range(), "expected a value here" ); return {};
        }
    }

    Value IfExpr::apply( const lex::Token& op, const Value a, const Value b, const bool live )
    {
        using enum lex::TokenKind;

        const bool unsigned_b = a.is_unsigned || b.is_unsigned;
        const auto sa         = static_cast< std::int64_t >( a.bits );
        const auto sb         = static_cast< std::int64_t >( b.bits );

        switch ( op.kind )
        {
            case star: return { a.bits * b.bits, unsigned_b };
            case plus: return { a.bits + b.bits, unsigned_b };
            case minus: return { a.bits - b.bits, unsigned_b };
            case slash:
            case percent:
            {
                if ( b.bits == 0 )
                {
                    if ( live )
                        m_diags.report( support::DiagnosticId::if_division_by_zero, m_directive ).primary( op.range(), "divides by zero" );
                    return { 0, unsigned_b };
                }
                if ( unsigned_b ) return { op.is( slash ) ? a.bits / b.bits : a.bits % b.bits, true };
                if ( sb == -1 ) return { op.is( slash ) ? 0 - a.bits : 0, false };
                return { static_cast< std::uint64_t >( op.is( slash ) ? sa / sb : sa % sb ), false };
            }
            case less_less:
            case greater_greater:
            {
                const auto cnt = !b.is_unsigned && sb < 0 ? std::uint64_t { 64 } : b.bits;
                if ( cnt >= 64 )
                {
                    const bool fill = op.is( greater_greater ) && !a.is_unsigned && sa < 0;
                    return { fill ? ~std::uint64_t {} : 0, a.is_unsigned };
                }
                if ( op.is( less_less ) ) return { a.bits << cnt, a.is_unsigned };
                return { a.is_unsigned ? a.bits >> cnt : static_cast< std::uint64_t >( sa >> cnt ), a.is_unsigned };
            }
            case less: return { ( unsigned_b ? a.bits < b.bits : sa < sb ) ? 1u : 0u, false };
            case greater: return { ( unsigned_b ? a.bits > b.bits : sa > sb ) ? 1u : 0u, false };
            case less_equal: return { ( unsigned_b ? a.bits <= b.bits : sa <= sb ) ? 1u : 0u, false };
            case greater_equal: return { ( unsigned_b ? a.bits >= b.bits : sa >= sb ) ? 1u : 0u, false };
            case equal_equal: return { a.bits == b.bits ? 1u : 0u, false };
            case exclaim_equal: return { a.bits != b.bits ? 1u : 0u, false };
            case amp: return { a.bits & b.bits, unsigned_b };
            case caret: return { a.bits ^ b.bits, unsigned_b };
            case pipe: return { a.bits | b.bits, unsigned_b };
            default: return {};
        }
    }

    Value IfExpr::number( const lex::Token& token )
    {
        auto text = m_spell( token );
        std::erase( text, '\'' );

        const bool hex = text.size() > 1 && text[ 0 ] == '0' && ( text[ 1 ] == 'x' || text[ 1 ] == 'X' );
        const bool bin = text.size() > 1 && text[ 0 ] == '0' && ( text[ 1 ] == 'b' || text[ 1 ] == 'B' );
        if ( text.contains( '.' ) || ( hex && text.find_first_of( "pP" ) != std::string::npos )
             || ( !hex && text.find_first_of( "eE" ) != std::string::npos ) )
        {
            fail( token.range(), "floating point numbers are not allowed here" );
            return {};
        }

        const int   base  = hex ? 16 : bin ? 2 : text.size() > 1 && text[ 0 ] == '0' ? 8 : 10;
        std::size_t i     = hex || bin ? 2 : 0;
        const auto  start = i;

        std::uint64_t value {};
        bool          overflow {};
        for ( ; i < text.size(); ++i )
        {
            const char c     = text[ i ];
            const int  digit = c >= '0' && c <= '9' ? c - '0'
                             : c >= 'a' && c <= 'f' ? c - 'a' + 10
                             : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                                    : 99;
            if ( digit >= base ) break;
            if ( value > ( std::numeric_limits< std::uint64_t >::max() - static_cast< std::uint64_t >( digit ) ) / base ) overflow = true;
            value = value * base + static_cast< std::uint64_t >( digit );
        }

        auto suffix = text.substr( i );
        std::ranges::transform( suffix, suffix.begin(),
                                []( const char c )
                                {
                                    return static_cast< char >( std::tolower( c ) );
                                } );

        if ( constexpr std::array< std::string_view, 11 > suffixes { "", "u", "l", "ul", "lu", "ll", "ull", "llu", "wb", "uwb", "wbu" };
             ( i == start && base != 10 && base != 8 ) || !std::ranges::contains( suffixes, suffix ) )
        {
            fail( token.range(), "not a valid integer constant" );
            return {};
        }
        if ( overflow )
        {
            fail( token.range(), "this number is too large" );
            return {};
        }

        return { value, suffix.contains( 'u' ) || value > static_cast< std::uint64_t >( std::numeric_limits< std::int64_t >::max() ) };
    }

    Value IfExpr::character( const lex::Token& token ) const
    {
        const auto text = m_spell( token );
        const auto open = text.find( '\'' );
        const auto body = std::string_view { text }.substr( open + 1, text.size() - open - 2 );
        const bool wide = token.encoding != lex::Encoding::none;

        std::vector< std::uint32_t > units;
        for ( std::size_t i {}; i < body.size(); )
        {
            if ( const auto c = static_cast< unsigned char >( body[ i ] ); c != '\\' )
            {
                if ( !wide || c < 0x80 )
                {
                    units.push_back( c );
                    ++i;
                    continue;
                }

                const int     length = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : 2;
                std::uint32_t point  = c & ( 0x3F >> ( length - 1 ) );
                for ( int k = 1; k < length && i + k < body.size(); ++k ) point = ( point << 6 ) | ( body[ i + k ] & 0x3F );
                units.push_back( point );
                i += static_cast< std::size_t >( length );
                continue;
            }

            const char e  = i + 1 < body.size() ? body[ i + 1 ] : '\\';
            i            += 2;
            switch ( e )
            {
                case 'a': units.push_back( 7 ); break;
                case 'b': units.push_back( 8 ); break;
                case 'f': units.push_back( 12 ); break;
                case 'n': units.push_back( 10 ); break;
                case 'r': units.push_back( 13 ); break;
                case 't': units.push_back( 9 ); break;
                case 'v': units.push_back( 11 ); break;
                case 'x':
                case 'u':
                case 'U':
                {
                    std::uint32_t point {};
                    std::size_t   limit = e == 'x' ? body.size() : i + ( e == 'u' ? 4 : 8 );
                    for ( ; i < body.size() && i < limit && std::isxdigit( static_cast< unsigned char >( body[ i ] ) ); ++i )
                        point = point * 16
                              + static_cast< std::uint32_t >( std::isdigit( static_cast< unsigned char >( body[ i ] ) )
                                                                  ? body[ i ] - '0'
                                                                  : std::tolower( body[ i ] ) - 'a' + 10 );
                    units.push_back( point );
                    break;
                }
                default:
                    if ( e >= '0' && e <= '7' )
                    {
                        std::uint32_t point = static_cast< std::uint32_t >( e - '0' );
                        for ( int k = 0; k < 2 && i < body.size() && body[ i ] >= '0' && body[ i ] <= '7'; ++k, ++i )
                            point = point * 8 + static_cast< std::uint32_t >( body[ i ] - '0' );
                        units.push_back( point );
                    }
                    else
                        units.push_back( static_cast< unsigned char >( e ) );
            }
        }

        if ( wide ) return { units.empty() ? 0 : units.front(), false };
        if ( units.size() == 1 )
            return { static_cast< std::uint64_t >( static_cast< std::int64_t >( static_cast< signed char >( units.front() ) ) ), false };

        std::uint64_t value {};
        for ( const auto unit : units ) value = ( value << 8 ) | ( unit & 0xFF );
        return { value, false };
    }
} // namespace taihu::pp