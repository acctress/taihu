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

    class SourceManager
    {
    public:
        FileId add_file( const std::string& path, std::string text );

        std::optional< FileId > load_file( const std::filesystem::path& path );

        [[nodiscard]] std::pair< FileId, std::uint32_t > decompose( SourceLocation location ) const;

        [[nodiscard]] std::string_view text( FileId file ) const;

        [[nodiscard]] std::string_view path( FileId file ) const;

        [[nodiscard]] SourceLocation location( FileId file, std::uint32_t offset ) const;

        [[nodiscard]] LineColumn line_column( SourceLocation location ) const;

        [[nodiscard]] std::string_view line_text( FileId file, std::uint32_t line ) const;

    private:
        enum class Kind : std::uint8_t
        {
            file
        };

        struct Entry
        {
            Kind                                 kind {};
            std::string                          path;
            std::string                          text;
            std::uint32_t                        start {};
            mutable std::vector< std::uint32_t > line_starts;
        };

        std::deque< Entry > m_entries;
        std::uint32_t       m_next { 1 };

        const Entry& entry( FileId file ) const;

        const std::vector< std::uint32_t >& lines( const Entry& entry ) const;
    };

    FileId SourceManager::add_file( const std::string& path, std::string text )
    {
        if ( text.starts_with( "\xEF\xBB\xBF" ) ) text.erase( 0, 3 );

        std::string out {};
        out.reserve( text.size() + 1 );

        for ( auto i { 0uz }; i < text.size(); ++i )
        {
            if ( text[ i ] != '\r' )
            {
                out += text[ i ];
                continue;
            }
            out += '\n';
            if ( i + 1 < text.size() && text[ i + 1 ] == '\n' ) ++i;
        }

        const auto sz = out.size();
        if ( sz >= std::numeric_limits< std::uint32_t >::max() - m_next ) throw std::length_error( "source too large" );

        out += '\0';

        const FileId id { static_cast< std::uint32_t >( m_entries.size() ) };

        m_entries.push_back( Entry { Kind::file, path, std::move( out ), m_next, {} } );
        m_next += static_cast< std::uint32_t >( sz ) + 1;
        return id;
    }

    std::pair< FileId, std::uint32_t > SourceManager::decompose( SourceLocation location ) const { }

    std::optional< FileId > SourceManager::load_file( const std::filesystem::path& path ) { }

    std::string_view SourceManager::text( FileId file ) const { }

    std::string_view SourceManager::path( FileId file ) const { }

    SourceLocation SourceManager::location( FileId file, std::uint32_t offset ) const { }

    LineColumn SourceManager::line_column( SourceLocation location ) const { }

    std::string_view SourceManager::line_text( FileId file, std::uint32_t line ) const { }

    const SourceManager::Entry& SourceManager::entry( FileId file ) const { }

    const std::vector< std::uint32_t >& SourceManager::lines( const Entry& entry ) const { }
} // namespace taihu::support
