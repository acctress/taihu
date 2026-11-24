export module taihu.support:enums;
import std;

export namespace taihu::support
{
    template < typename E >
    struct EnumNames;

    template < typename E >
    concept NamedEnum = std::is_enum_v< E > && requires {
        { EnumNames< E >::names[ 0 ] } -> std::convertible_to< std::string_view >;
    };

    template < NamedEnum E >
    [[nodiscard]] constexpr std::string_view enum_name( const E value ) noexcept
    {
        constexpr auto& names = EnumNames< E >::names;
        const auto      index = static_cast< std::size_t >( std::to_underlying( value ) );
        return index < std::size( names ) ? std::string_view { names[ index ] } : std::string_view { "?" };
    }

    template < NamedEnum E >
    [[nodiscard]] constexpr std::optional< E > enum_from_name( const std::string_view name ) noexcept
    {
        constexpr auto& names = EnumNames< E >::names;
        for ( std::size_t i {}; i < std::size( names ); ++i )
            if ( names[ i ] == name ) return static_cast< E >( i );
        return std::nullopt;
    }
} // namespace taihu::support

export template < taihu::support::NamedEnum E >
struct std::formatter< E, char > : std::formatter< std::string_view >
{
    auto format( const E value, auto& ctx ) const
    { return std::formatter< std::string_view >::format( taihu::support::enum_name( value ), ctx ); }
};
