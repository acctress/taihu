export module taihu.lex:token;
import std;
import taihu.support;

export namespace taihu::lex
{
    enum class TokenKind : std::uint8_t
    {
        eof,
        identifier,
        pp_number,
        char_constant,
        string_literal,
        header_name,
        other,
        l_square,
        r_square,
        l_paren,
        r_paren,
        l_brace,
        r_brace,
        period,
        arrow,
        plus_plus,
        minus_minus,
        amp,
        star,
        plus,
        minus,
        tilde,
        exclaim,
        slash,
        percent,
        less_less,
        greater_greater,
        less,
        greater,
        less_equal,
        greater_equal,
        equal_equal,
        exclaim_equal,
        caret,
        pipe,
        amp_amp,
        pipe_pipe,
        question,
        colon,
        semi,
        ellipsis,
        equal,
        star_equal,
        slash_equal,
        percent_equal,
        plus_equal,
        minus_equal,
        less_less_equal,
        greater_greater_equal,
        amp_equal,
        caret_equal,
        pipe_equal,
        comma,
        hash,
        hash_hash,
        count
    };

    enum class Encoding : std::uint8_t
    {
        none,
        utf8,
        utf16,
        utf32,
        wide
    };

    enum class TokenFlags : std::uint8_t
    {
        none           = 0,
        at_line_start  = 1 << 0,
        leading_space  = 1 << 1,
        needs_cleaning = 1 << 2,
        no_expand      = 1 << 3,
        vertical_space = 1 << 4,
    };

    [[nodiscard]] constexpr TokenFlags operator| ( const TokenFlags a, const TokenFlags b ) noexcept
    { return static_cast< TokenFlags >( std::to_underlying( a ) | std::to_underlying( b ) ); }

    [[nodiscard]] constexpr TokenFlags operator& ( const TokenFlags a, const TokenFlags b ) noexcept
    { return static_cast< TokenFlags >( std::to_underlying( a ) & std::to_underlying( b ) ); }

    [[nodiscard]] constexpr TokenFlags operator~( const TokenFlags a ) noexcept
    { return static_cast< TokenFlags >( static_cast< std::uint8_t >( ~std::to_underlying( a ) ) ); }

    constexpr TokenFlags& operator|= ( TokenFlags& a, const TokenFlags b ) noexcept
    { return a = a | b; }

    struct Token
    {
        TokenKind               kind {};
        TokenFlags              flags {};
        Encoding                encoding {};
        std::uint32_t           length {};
        support::SourceLocation location {};
        const support::Symbol*  symbol {};

        [[nodiscard]] bool is( const TokenKind k ) const noexcept
        { return kind == k; }

        [[nodiscard]] bool has( const TokenFlags flag ) const noexcept
        { return ( flags & flag ) != TokenFlags::none; }

        [[nodiscard]] support::SourceRange range() const noexcept
        { return { location, location + length }; }
    };

    static_assert( sizeof( Token ) <= 24 );
} // namespace taihu::lex

export namespace taihu::support
{
    template <>
    struct EnumNames< lex::TokenKind >
    {
        static constexpr std::array names {
            "eof",
            "identifier",
            "pp_number",
            "char_constant",
            "string_literal",
            "header_name",
            "other",
            "l_square",
            "r_square",
            "l_paren",
            "r_paren",
            "l_brace",
            "r_brace",
            "period",
            "arrow",
            "plus_plus",
            "minus_minus",
            "amp",
            "star",
            "plus",
            "minus",
            "tilde",
            "exclaim",
            "slash",
            "percent",
            "less_less",
            "greater_greater",
            "less",
            "greater",
            "less_equal",
            "greater_equal",
            "equal_equal",
            "exclaim_equal",
            "caret",
            "pipe",
            "amp_amp",
            "pipe_pipe",
            "question",
            "colon",
            "semi",
            "ellipsis",
            "equal",
            "star_equal",
            "slash_equal",
            "percent_equal",
            "plus_equal",
            "minus_equal",
            "less_less_equal",
            "greater_greater_equal",
            "amp_equal",
            "caret_equal",
            "pipe_equal",
            "comma",
            "hash",
            "hash_hash",
        };
    };

    template <>
    struct EnumNames< lex::Encoding >
    {
        static constexpr std::array names { "none", "u8", "u", "U", "L" };
    };

    static_assert( EnumNames< lex::TokenKind >::names.size() == static_cast< std::size_t >( lex::TokenKind::count ) );
    static_assert( enum_name( lex::TokenKind::hash_hash ) == "hash_hash" );
    static_assert( enum_name( lex::TokenKind::l_square ) == "l_square" );
} // namespace taihu::support