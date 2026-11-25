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
        cannot_open_file,
        missing_macro_name,
        macro_name_not_identifier,
        macro_redefined,
        missing_space_after_macro_name,
        unknown_directive,
        extra_tokens_after_directive,
        vertical_space_in_directive,
        va_args_outside_variadic,
        hash_hash_at_edge,
        expected_parameter,
        duplicate_parameter,
        unclosed_parameter_list,
        unterminated_macro_call,
        macro_argument_count,
        stringify_needs_parameter,
        invalid_stringify,
        invalid_paste,
        va_opt_needs_parens,
        nested_va_opt,
        include_not_found,
        bad_include,
        include_too_deep,
        bad_if_expression,
        if_division_by_zero,
        unterminated_conditional,
        unmatched_conditional,
        else_after_else,
        error_directive,
        warning_directive,
        defined_needs_name,
        count
    };

    template <>
    struct EnumNames< DiagnosticId >
    {
        static constexpr std::array names {
            "unterminated_comment",
            "missing_terminating_quote",
            "incomplete_universal_character",
            "invalid_universal_character",
            "empty_char_constant",
            "unknown_option",
            "missing_value",
            "invalid_value",
            "missing_input",
            "cannot_open_file",
            "missing_macro_name",
            "macro_name_not_identifier",
            "macro_redefined",
            "missing_space_after_macro_name",
            "unknown_directive",
            "extra_tokens_after_directive",
            "vertical_space_in_directive",
            "va_args_outside_variadic",
            "hash_hash_at_edge",
            "expected_parameter",
            "duplicate_parameter",
            "unclosed_parameter_list",
            "unterminated_macro_call",
            "macro_argument_count",
            "stringify_needs_parameter",
            "invalid_stringify",
            "invalid_paste",
            "va_opt_needs_parens",
            "nested_va_opt",
            "include_not_found",
            "bad_include",
            "include_too_deep",
            "bad_if_expression",
            "if_division_by_zero",
            "unterminated_conditional",
            "unmatched_conditional",
            "else_after_else",
            "error_directive",
            "warning_directive",
            "defined_needs_name",
        };
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
         { DiagnosticId::cannot_open_file, Severity::error, "E0104", "", "cannot open `{}`", "" },
         { DiagnosticId::missing_macro_name, Severity::error, "E0200", "", "`#{}` needs a macro name", "C23 §6.10.5" },
         { DiagnosticId::macro_name_not_identifier, Severity::error, "E0201", "", "a macro name must be an identifier", "C23 §6.10.5" },
         { DiagnosticId::macro_redefined, Severity::error, "E0202", "", "`{}` is redefined with a different replacement list",
              "C23 §6.10.5p2" },
         { DiagnosticId::missing_space_after_macro_name, Severity::error, "E0203", "",
              "an object-like macro needs white space after its name", "C23 §6.10.5p3" },
         { DiagnosticId::unknown_directive, Severity::error, "E0204", "", "unknown directive `#{}`", "C23 §6.10.1p13" },
         { DiagnosticId::extra_tokens_after_directive, Severity::error, "E0205", "", "extra tokens after `#{}`", "C23 §6.10.1" },
         { DiagnosticId::vertical_space_in_directive, Severity::error, "E0206", "",
              "only spaces and tabs may separate the tokens of a directive", "C23 §6.10.1p8" },
         { DiagnosticId::va_args_outside_variadic, Severity::error, "E0207", "",
              "`{}` can only appear in a function-like macro that takes `...`", "C23 §6.10.5p5" },
         { DiagnosticId::hash_hash_at_edge, Severity::error, "E0208", "", "`##` cannot be at the {} of a replacement list",
              "C23 §6.10.5.3p1" },
         { DiagnosticId::expected_parameter, Severity::error, "E0209", "", "expected a parameter name", "C23 §6.10.5p10" },
         { DiagnosticId::duplicate_parameter, Severity::error, "E0210", "", "`{}` is already a parameter of this macro",
              "C23 §6.10.5p6" },
         { DiagnosticId::unclosed_parameter_list, Severity::error, "E0211", "", "expected `,` or `)` in the parameter list",
              "C23 §6.10.5p10" },
         { DiagnosticId::unterminated_macro_call, Severity::error, "E0212", "", "the call to `{}` has no closing `)`", "C23 §6.10.5p4" },
         { DiagnosticId::macro_argument_count, Severity::error, "E0213", "", "`{}` takes {} but was given {}", "C23 §6.10.5p4" },
         { DiagnosticId::stringify_needs_parameter, Severity::error, "E0214", "",
              "`#` must be followed by a parameter in a function-like macro", "C23 §6.10.5.2p1" },
         { DiagnosticId::invalid_stringify, Severity::error, "E0215", "", "`#` does not make a valid string literal here",
              "C23 §6.10.5.2p4" },
         { DiagnosticId::invalid_paste, Severity::error, "E0216", "", "pasting `{}` and `{}` does not give a valid token",
              "C23 §6.10.5.3p3" },
         { DiagnosticId::va_opt_needs_parens, Severity::error, "E0217", "", "`__VA_OPT__` must be followed by `(` and a matching `)`",
              "C23 §6.10.5.1p3" },
         { DiagnosticId::nested_va_opt, Severity::error, "E0218", "", "`__VA_OPT__` cannot appear inside another `__VA_OPT__`",
              "C23 §6.10.5.1p3" },
         { DiagnosticId::include_not_found, Severity::error, "E0219", "", "cannot find `{}`", "C23 §6.10.3p1" },
         { DiagnosticId::bad_include, Severity::error, "E0220", "", "`#include` expects \"file\" or <file>", "C23 §6.10.3p4" },
         { DiagnosticId::include_too_deep, Severity::error, "E0221", "", "`#include` is nested more than {} files deep",
              "C23 §6.10.3p6" },
         { DiagnosticId::bad_if_expression, Severity::error, "E0222", "", "invalid expression in `#{}`", "C23 §6.10.2p3" },
         { DiagnosticId::if_division_by_zero, Severity::error, "E0223", "", "division by zero in `#{}`", "C23 §6.6p4" },
         { DiagnosticId::unterminated_conditional, Severity::error, "E0224", "", "`#{}` is never closed by `#endif`", "C23 §6.10.2p17" },
         { DiagnosticId::unmatched_conditional, Severity::error, "E0225", "", "`#{}` without a matching `#if`", "C23 §6.10.2p17" },
         { DiagnosticId::else_after_else, Severity::error, "E0226", "", "`#{}` after `#else`", "C23 §6.10.2p17" },
         { DiagnosticId::error_directive, Severity::error, "E0227", "", "{}", "C23 §6.10.7" },
         { DiagnosticId::warning_directive, Severity::warning, "W0228", "", "{}", "C23 §6.10.7" },
         { DiagnosticId::defined_needs_name, Severity::error, "E0229", "", "`defined` must be followed by a macro name",
              "C23 §6.10.2p1" },
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

        void mute( const bool v ) noexcept
        { m_muted = v; }

    private:
        friend class DiagnosticBuilder;

        const SourceManager&      m_sources;
        std::vector< Diagnostic > m_diagnostics;
        std::size_t               m_errors {};
        bool                      m_muted {};

        void emit( Diagnostic&& diagnostic )
        {
            if ( m_muted ) return;
            if ( diagnostic.severity >= Severity::error ) ++m_errors;
            m_diagnostics.emplace_back( std::move( diagnostic ) );
        }
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