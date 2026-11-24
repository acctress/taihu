export module taihu.support:diagnostics;
import std;
import :source;
import :enums;

export namespace taihu::support
{
    enum class Severity : std::uint8_t
    {
        note,
        warning,
        error,
        fatal
    };

    template <>
    struct EnumNames< Severity >
    {
        static constexpr std::array names { "note", "warning", "error", "fatal error" };
    };

    enum class DiagnosticId : std::uint16_t
    {
        unterminated_comment,
        missing_terminating_quote,
        incomplete_universal_character,
        invalid_universal_character,
        empty_char_constant,
        unknown_option,
        missing_value,
        invalid_value,
        missing_input,
        count
    };

    template <>
    struct EnumNames< DiagnosticId >
    {
        static constexpr std::array names { "unterminated_comment",
                                            "missing_terminating_quote",
                                            "incomplete_universal_character",
                                            "invalid_universal_character",
                                            "empty_char_constant",
                                            "unknown_option",
                                            "missing_value",
                                            "invalid_value",
                                            "missing_input" };
    };

    static_assert( EnumNames< DiagnosticId >::names.size() == static_cast< std::size_t >( DiagnosticId::count ) );

    struct DiagnosticInfo
    {
        DiagnosticId     id;
        Severity         severity;
        std::string_view code;
        std::string_view group;
        std::string_view message;
        std::string_view spec;
    };

    struct Label
    {
        SourceRange range {};
        std::string text;
        bool        primary {};
    };

    struct Edit
    {
        SourceRange range {};
        std::string replacement;
    };

    enum class Applicability : std::uint8_t
    {
        certain,
        likely,
        placeholder
    };

    template <>
    struct EnumNames< Applicability >
    {
        static constexpr std::array names { "certain", "likely", "placeholder" };
    };

    struct Suggestion
    {
        Applicability       applicability {};
        std::string         message;
        std::vector< Edit > edits;
    };

    struct Diagnostic
    {
        DiagnosticId               id {};
        Severity                   severity {};
        std::string                message;
        std::vector< Label >       labels;
        std::vector< std::string > help;
        std::vector< std::string > notes;
        std::vector< Suggestion >  suggestions;
    };

    inline constexpr std::array< DiagnosticInfo, static_cast< std::size_t >( DiagnosticId::count ) > diagnostic_catalog {
        {
         { DiagnosticId::unterminated_comment, Severity::error, "E0001", "", "this comment is never closed", "C23 §6.4.9" },
         { DiagnosticId::missing_terminating_quote, Severity::error, "E0002", "", "missing closing `{}` for this {}",
              "C23 §6.4.4.5, §6.4.5" },
         { DiagnosticId::incomplete_universal_character, Severity::error, "E0003", "", "`\\{}` needs {} hexadecimal digits but has {}",
              "C23 §6.4.3" },
         { DiagnosticId::invalid_universal_character, Severity::error, "E0004", "", "`{}` is not allowed in {}", "C23 §6.4.3" },
         { DiagnosticId::empty_char_constant, Severity::error, "E0005", "", "this character constant is empty", "C23 §6.4.4.5" },
         { DiagnosticId::unknown_option, Severity::error, "E0100", "", "unknown option `{}`", "" },
         { DiagnosticId::missing_value, Severity::error, "E0101", "", "`{}` needs a value: <{}>", "" },
         { DiagnosticId::invalid_value, Severity::error, "E0102", "", "`{}` is not a valid <{}> for `{}`", "" },
         { DiagnosticId::missing_input, Severity::error, "E0103", "", "no input files", "" },
         }
    };

    static_assert(
        []
        {
            for ( std::size_t i {}; i < diagnostic_catalog.size(); ++i )
                if ( static_cast< std::size_t >( diagnostic_catalog[ i ].id ) != i ) return false;
            return true;
        }() );

    class DiagnosticEngine;

    class DiagnosticBuilder
    {
    public:
        DiagnosticBuilder( DiagnosticEngine& engine, Diagnostic diagnostic );
        ~DiagnosticBuilder();

        DiagnosticBuilder( const DiagnosticBuilder& )             = delete;
        DiagnosticBuilder& operator= ( const DiagnosticBuilder& ) = delete;

        DiagnosticBuilder& primary( SourceRange range, std::string text = {} );
        DiagnosticBuilder& secondary( SourceRange range, std::string text );
        DiagnosticBuilder& help( std::string text );
        DiagnosticBuilder& note( std::string text );
        DiagnosticBuilder& suggest( Applicability applicability, std::string message, std::vector< Edit > edits );

    private:
        DiagnosticEngine& m_engine;
        Diagnostic        m_diagnostic;
    };

    class DiagnosticEngine
    {
    public:
        explicit DiagnosticEngine( const SourceManager& sources ) : m_sources { sources } { }

        template < typename... Args >
        DiagnosticBuilder report( const DiagnosticId id, Args&&... args )
        {
            const auto& info = diagnostic_catalog[ static_cast< std::size_t >( id ) ];
            auto        text = std::vformat( info.message, std::make_format_args( args... ) );
            return DiagnosticBuilder {
                *this,
                { .id = id, .severity = info.severity, .message = std::move( text ) }
            };
        }

        [[nodiscard]] std::size_t error_count() const noexcept
        { return m_errors; }

        [[nodiscard]] std::span< const Diagnostic > all() const noexcept
        { return m_diagnostics; }

        [[nodiscard]] const SourceManager& sources() const noexcept
        { return m_sources; }

    private:
        friend class DiagnosticBuilder;

        void emit( Diagnostic&& diagnostic )
        {
            if ( diagnostic.severity >= Severity::error ) ++m_errors;
            m_diagnostics.emplace_back( std::move( diagnostic ) );
        }

        const SourceManager&      m_sources;
        std::vector< Diagnostic > m_diagnostics;
        std::size_t               m_errors {};
    };

    DiagnosticBuilder::DiagnosticBuilder( DiagnosticEngine& engine, Diagnostic diagnostic ) :
        m_engine { engine },
        m_diagnostic { std::move( diagnostic ) }
    {
    }

    DiagnosticBuilder::~DiagnosticBuilder()
    { m_engine.emit( std::move( m_diagnostic ) ); }

    DiagnosticBuilder& DiagnosticBuilder::primary( const SourceRange range, std::string text )
    {
        m_diagnostic.labels.emplace_back( range, std::move( text ), true );
        return *this;
    }

    DiagnosticBuilder& DiagnosticBuilder::secondary( const SourceRange range, std::string text )
    {
        m_diagnostic.labels.emplace_back( range, std::move( text ), false );
        return *this;
    }

    DiagnosticBuilder& DiagnosticBuilder::help( std::string text )
    {
        m_diagnostic.help.emplace_back( std::move( text ) );
        return *this;
    }

    DiagnosticBuilder& DiagnosticBuilder::note( std::string text )
    {
        m_diagnostic.notes.emplace_back( std::move( text ) );
        return *this;
    }

    DiagnosticBuilder& DiagnosticBuilder::suggest( const Applicability applicability, std::string message, std::vector< Edit > edits )
    {
        m_diagnostic.suggestions.emplace_back( applicability, std::move( message ), std::move( edits ) );
        return *this;
    }
} // namespace taihu::support