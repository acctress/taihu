module;
#include <cassert>

export module taihu.support:source;
import std;

export namespace taihu::support
{
    struct SourceLocation
    {
        std::uint32_t raw {};

        [[nodiscard]] constexpr bool valid() const noexcept
        { return raw != 0; }

        auto operator<=> ( const SourceLocation& ) const = default;
    };

    [[nodiscard]] constexpr SourceLocation operator+ ( const SourceLocation location, const std::uint32_t offset ) noexcept
    { return { location.raw + offset }; }

    struct SourceRange
    {
        SourceLocation begin {};
        SourceLocation end {};
    };

    struct FileId
    {
        std::uint32_t index {};

        auto operator<=> ( const FileId& ) const = default;
    };

    struct LineColumn
    {
        std::uint32_t line {};
        std::uint32_t column {};
    };

    enum class LineEnding : std::uint8_t
    {
        lf,
        crlf,
        cr
    };

    struct FileStyle
    {
        bool       bom {};
        LineEnding ending {};
        bool       mixed {};
    };

    [[nodiscard]] inline std::string restore_style( const std::string_view text, const FileStyle style )
    {
        const std::string_view newline = style.ending == LineEnding::crlf ? "\r\n" : style.ending == LineEnding::cr ? "\r" : "\n";

        std::string out;
        out.reserve( text.size() + text.size() / 32 + 3 );
        if ( style.bom ) out += "\xEF\xBB\xBF";
        for ( const auto ch : text )
        {
            if ( ch == '\n' ) out += newline;
            else
                out += ch;
        }
        return out;
    }

    struct Decomposed
    {
        FileId        file {};
        std::uint32_t offset {};
    };

    struct CommandLine
    {
        FileId                     file {};
        std::vector< std::string > args;
        std::vector< SourceRange > ranges;
    };

    class SourceManager
    {
    public:
        FileId add_file( std::string path, std::string text );

        std::optional< FileId > load_file( const std::filesystem::path& path );

        [[nodiscard]] Decomposed decompose( SourceLocation location ) const;

        [[nodiscard]] std::string_view text( FileId file ) const;

        [[nodiscard]] std::string_view path( FileId file ) const;

        [[nodiscard]] FileStyle style( FileId file ) const;

        [[nodiscard]] SourceLocation location( FileId file, std::uint32_t offset ) const;

        [[nodiscard]] LineColumn line_column( SourceLocation location ) const;

        [[nodiscard]] std::string_view line_text( FileId file, std::uint32_t line ) const;

        [[nodiscard]] CommandLine add_cmd_line( std::span< const char* const > argv );

        [[nodiscard]] SourceLocation add_expansion( SourceLocation spelling, SourceRange expansion, std::uint32_t length );

        [[nodiscard]] bool is_expansion( SourceLocation location ) const;

        [[nodiscard]] SourceLocation spelling_location( SourceLocation location ) const;

        [[nodiscard]] SourceLocation file_location( SourceLocation location ) const;

        [[nodiscard]] SourceRange file_range( SourceRange range ) const;

    private:
        enum class Kind : std::uint8_t
        {
            file,
            expansion
        };

        struct Entry
        {
            Kind                                 kind {};
            std::string                          path;
            std::string                          text;
            std::uint32_t                        start {};
            FileStyle                            style {};
            mutable std::vector< std::uint32_t > line_starts;
            SourceLocation                       spelling {};
            SourceRange                          expansion {};
        };

        std::deque< Entry > m_entries;
        std::uint32_t       m_next { 1 };

        const Entry& entry( FileId file ) const;

        const Entry& entry_at( SourceLocation location ) const;

        static const std::vector< std::uint32_t >& lines( const Entry& entry );
    };

    FileId SourceManager::add_file( std::string path, std::string text )
    {
        FileStyle   style {};
        std::size_t read {};
        if ( text.starts_with( "\xEF\xBB\xBF" ) )
        {
            read      = 3;
            style.bom = true;
        }

        std::array< std::size_t, 3 > endings {};
        std::size_t                  write {};
        for ( ; read < text.size(); ++read )
        {
            const char c = text[ read ];
            if ( c != '\r' )
            {
                if ( c == '\n' ) ++endings[ std::to_underlying( LineEnding::lf ) ];
                text[ write++ ] = c;
                continue;
            }

            text[ write++ ] = '\n';
            if ( read + 1 < text.size() && text[ read + 1 ] == '\n' )
            {
                ++endings[ std::to_underlying( LineEnding::crlf ) ];
                ++read;
            }
            else
                ++endings[ std::to_underlying( LineEnding::cr ) ];
        }
        text.resize( write );

        style.ending = static_cast< LineEnding >( std::ranges::max_element( endings ) - endings.begin() );
        style.mixed  = std::ranges::count_if( endings,
                                              []( const std::size_t n )
                                              {
                                                 return n != 0;
                                             } )
                    > 1;

        const auto size = text.size();
        if ( size >= std::numeric_limits< std::uint32_t >::max() - m_next ) throw std::length_error( "source too large" );

        text.push_back( '\0' );

        const FileId id { static_cast< std::uint32_t >( m_entries.size() ) };
        m_entries.push_back(
            { .kind = Kind::file, .path = std::move( path ), .text = std::move( text ), .start = m_next, .style = style } );
        m_next += static_cast< std::uint32_t >( size ) + 1;
        return id;
    }

    Decomposed SourceManager::decompose( const SourceLocation location ) const
    {
        assert( location.valid() );

        const auto iter = std::prev( std::ranges::upper_bound( m_entries, location.raw, {}, &Entry::start ) );
        return { .file = FileId { static_cast< std::uint32_t >( iter - m_entries.begin() ) }, .offset = location.raw - iter->start };
    }

    std::optional< FileId > SourceManager::load_file( const std::filesystem::path& path )
    {
        std::ifstream in { path, std::ios::binary };
        if ( !in ) return std::nullopt;

        std::string text { std::istreambuf_iterator { in }, {} };
        return add_file( path.string(), std::move( text ) );
    }

    std::string_view SourceManager::text( const FileId file ) const
    {
        const auto& e = entry( file );
        if ( e.text.empty() ) return {};
        return { e.text.data(), e.text.size() - 1 };
    }

    std::string_view SourceManager::path( const FileId file ) const
    { return entry( file ).path; }

    FileStyle SourceManager::style( const FileId file ) const
    { return entry( file ).style; }

    SourceLocation SourceManager::location( const FileId file, const std::uint32_t offset ) const
    {
        const auto& e = entry( file );
        assert( offset < e.text.size() );
        return { e.start + offset };
    }

    LineColumn SourceManager::line_column( const SourceLocation location ) const
    {
        const auto [ file, offset ] = decompose( location );
        const auto& ls              = lines( entry( file ) );
        const auto  line            = std::ranges::upper_bound( ls, offset ) - ls.begin();
        return { static_cast< std::uint32_t >( line ), offset - ls[ line - 1 ] + 1 };
    }

    std::string_view SourceManager::line_text( const FileId file, const std::uint32_t line ) const
    {
        const auto& ls    = lines( entry( file ) );
        const auto  src   = text( file );
        const auto  start = ls[ line - 1 ];
        const auto  end   = line < ls.size() ? ls[ line ] - 1 : src.size();
        return src.substr( start, end - start );
    }

    const SourceManager::Entry& SourceManager::entry( const FileId file ) const
    {
        assert( file.index < m_entries.size() );
        return m_entries[ file.index ];
    }

    const std::vector< std::uint32_t >& SourceManager::lines( const Entry& entry )
    {
        if ( !entry.line_starts.empty() ) return entry.line_starts;

        entry.line_starts.push_back( 0 );
        for ( std::uint32_t i {}; i + 1 < entry.text.size(); ++i )
            if ( entry.text[ i ] == '\n' ) entry.line_starts.push_back( i + 1 );

        return entry.line_starts;
    }

    CommandLine SourceManager::add_cmd_line( const std::span< const char* const > argv )
    {
        CommandLine                                              result;
        std::string                                              text;
        std::vector< std::pair< std::uint32_t, std::uint32_t > > offsets;

        for ( const auto [ i, raw ] : std::views::enumerate( argv ) )
        {
            if ( i > 0 ) text += ' ';

            const auto             program = i == 0 ? std::filesystem::path { raw }.stem().string() : std::string {};
            const std::string_view arg     = i == 0 ? std::string_view { program } : std::string_view { raw };
            const auto             begin   = static_cast< std::uint32_t >( text.size() );

            if ( arg.contains( ' ' ) )
            {
                text += '"';
                text += arg;
                text += '"';
            }
            else
                text += arg;

            offsets.emplace_back( begin, static_cast< std::uint32_t >( text.size() ) );
            result.args.emplace_back( arg );
        }

        result.file = add_file( "<cli>", std::move( text ) );
        result.ranges.reserve( offsets.size() );

        for ( const auto [ begin, end ] : offsets )
            result.ranges.push_back( { location( result.file, begin ), location( result.file, end ) } );

        return result;
    }

    const SourceManager::Entry& SourceManager::entry_at( const SourceLocation location ) const
    { return entry( decompose( location ).file ); }

    SourceLocation SourceManager::add_expansion( const SourceLocation spelling, const SourceRange expansion, const std::uint32_t length )
    {
        assert( spelling.valid() && expansion.begin.valid() );

        const auto size = std::max< std::uint32_t >( length, 1 );
        if ( size >= std::numeric_limits< std::uint32_t >::max() - m_next ) throw std::length_error( "too many macro expansions" );

        const SourceLocation start { m_next };
        m_entries.push_back( { .kind = Kind::expansion, .start = m_next, .spelling = spelling, .expansion = expansion } );
        m_next += size;
        return start;
    }

    bool SourceManager::is_expansion( const SourceLocation location ) const
    { return entry_at( location ).kind == Kind::expansion; }

    SourceLocation SourceManager::spelling_location( SourceLocation location ) const
    {
        for ( ;; )
        {
            const auto& e = entry_at( location );
            if ( e.kind == Kind::file ) return location;
            location = e.spelling + ( location.raw - e.start );
        }
    }

    SourceLocation SourceManager::file_location( SourceLocation location ) const
    {
        for ( ;; )
        {
            const auto& e = entry_at( location );
            if ( e.kind == Kind::file ) return location;
            location = e.expansion.begin;
        }
    }

    SourceRange SourceManager::file_range( const SourceRange range ) const
    {
        const auto begin = file_location( range.begin );
        if ( !range.end.valid() ) return { begin, range.end };
        if ( range.end <= range.begin ) return { begin, begin };

        SourceLocation last { range.end.raw - 1 };
        for ( ;; )
        {
            const auto& e = entry_at( last );
            if ( e.kind == Kind::file ) break;
            last = { e.expansion.end.raw - 1 };
        }
        return { begin, last + 1 };
    }
} // namespace taihu::support
