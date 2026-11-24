module;

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <cstdio>
#include <cstdlib>

#if defined( _WIN32 )
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

export module taihu.support:style;
import std;

export namespace taihu::support
{
    struct Color
    {
        enum class Kind : std::uint8_t
        {
            none,
            ansi,
            rgb
        };

        Kind         kind {};
        std::uint8_t r {};
        std::uint8_t g {};
        std::uint8_t b {};

        [[nodiscard]] static constexpr Color ansi( const std::uint8_t code ) noexcept
        { return { Kind::ansi, code, 0, 0 }; }

        [[nodiscard]] static constexpr Color rgb( const std::uint8_t red, const std::uint8_t green, const std::uint8_t blue ) noexcept
        { return { Kind::rgb, red, green, blue }; }
    };

    namespace ansi
    {
        inline constexpr Color black          = Color::ansi( 30 );
        inline constexpr Color red            = Color::ansi( 31 );
        inline constexpr Color green          = Color::ansi( 32 );
        inline constexpr Color yellow         = Color::ansi( 33 );
        inline constexpr Color blue           = Color::ansi( 34 );
        inline constexpr Color magenta        = Color::ansi( 35 );
        inline constexpr Color cyan           = Color::ansi( 36 );
        inline constexpr Color white          = Color::ansi( 37 );
        inline constexpr Color bright_black   = Color::ansi( 90 );
        inline constexpr Color bright_red     = Color::ansi( 91 );
        inline constexpr Color bright_green   = Color::ansi( 92 );
        inline constexpr Color bright_yellow  = Color::ansi( 93 );
        inline constexpr Color bright_blue    = Color::ansi( 94 );
        inline constexpr Color bright_magenta = Color::ansi( 95 );
        inline constexpr Color bright_cyan    = Color::ansi( 96 );
        inline constexpr Color bright_white   = Color::ansi( 97 );
    } // namespace ansi

    struct Style
    {
        Color fg {};
        bool  bold {};
        bool  dim {};
        bool  italic {};
        bool  underline {};

        [[nodiscard]] constexpr bool plain() const noexcept
        { return fg.kind == Color::Kind::none && !bold && !dim && !italic && !underline; }
    };

    class Painter
    {
    public:
        explicit Painter( const bool enabled ) noexcept : m_enabled { enabled } { }

        [[nodiscard]] bool enabled() const noexcept
        { return m_enabled; }

        [[nodiscard]] std::string paint( const std::string_view text, const Style& style ) const
        {
            std::string out;
            paint_into( out, text, style );
            return out;
        }

        void paint_into( std::string& out, const std::string_view text, const Style& style ) const
        {
            if ( !m_enabled || style.plain() || text.empty() )
            {
                out += text;
                return;
            }

            out.reserve( out.size() + text.size() + 24 );
            out += "\x1b[";

            auto       separate = false;
            const auto add      = [ & ]( const std::string_view code )
            {
                if ( separate ) out += ';';
                out      += code;
                separate  = true;
            };

            if ( style.bold ) add( "1" );
            if ( style.dim ) add( "2" );
            if ( style.italic ) add( "3" );
            if ( style.underline ) add( "4" );

            switch ( style.fg.kind )
            {
                case Color::Kind::none: break;
                case Color::Kind::ansi:
                    add( "" );
                    std::format_to( std::back_inserter( out ), "{}", style.fg.r );
                    break;
                case Color::Kind::rgb:
                    add( "38;2;" );
                    std::format_to( std::back_inserter( out ), "{};{};{}", style.fg.r, style.fg.g, style.fg.b );
                    break;
            }

            out += 'm';
            out += text;
            out += "\x1b[0m";
        }

    private:
        bool m_enabled {};
    };

    struct Theme
    {
        std::string_view name;
        Style            error;
        Style            warning;
        Style            note;
        Style            help;
        Style            message;
        Style            code;
        Style            gutter;
        Style            secondary;
        Style            label;
        Style            spec;
    };

    inline constexpr Theme theme_classic {
        .name      = "classic",
        .error     = { .fg = Color::rgb( 255, 140, 150 ), .bold = true },
        .warning   = { .fg = Color::rgb( 255, 212, 150 ), .bold = true },
        .note      = { .fg = Color::rgb( 150, 210, 245 ), .bold = true },
        .help      = { .fg = Color::rgb( 170, 230, 175 ), .bold = true },
        .message   = { .bold = true },
        .code      = { .fg = Color::rgb( 170, 165, 195 ) },
        .gutter    = { .fg = Color::rgb( 185, 170, 255 ) },
        .secondary = { .fg = Color::rgb( 185, 170, 255 ), .bold = true },
        .label     = {},
        .spec      = { .fg = Color::rgb( 170, 165, 195 ), .italic = true },
    };

    [[nodiscard]] inline std::optional< std::string > environment( const char* name )
    {
#if defined( _WIN32 )
        char*       raw {};
        std::size_t length {};
        if ( _dupenv_s( &raw, &length, name ) != 0 || !raw ) return std::nullopt;
        const std::unique_ptr< char, decltype( &std::free ) > value { raw, &std::free };
        return std::string { value.get() };
#else
        const char* value = std::getenv( name );
        return value ? std::optional< std::string > { value } : std::nullopt;
#endif
    }

    inline void use_utf8_console() noexcept
    {
#if defined( _WIN32 )
        SetConsoleOutputCP( CP_UTF8 );
#endif
    }

    [[nodiscard]] inline bool color_enabled( std::FILE* stream )
    {
        if ( environment( "NO_COLOR" ) ) return false;
        if ( environment( "FORCE_COLOR" ) ) return true;

#if defined( _WIN32 )
        const auto fd = _fileno( stream );
        if ( fd < 0 || !_isatty( fd ) ) return false;

        const auto handle = reinterpret_cast< HANDLE >( _get_osfhandle( fd ) );
        DWORD      mode {};
        if ( !GetConsoleMode( handle, &mode ) ) return false;
        return SetConsoleMode( handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING ) != 0;
#else
        const auto term = environment( "TERM" );
        return isatty( fileno( stream ) ) && term != "dumb";
#endif
    }
} // namespace taihu::support
