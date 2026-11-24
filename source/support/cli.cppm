export module taihu.support:cli;
import std;
import :source;
import :enums;
import :diagnostics;

namespace taihu::support
{
    std::size_t edit_distance( const std::string_view a, const std::string_view b )
    {
        std::vector< std::size_t > previous( b.size() + 1 );
        std::vector< std::size_t > current( b.size() + 1 );
        std::iota( previous.begin(), previous.end(), std::size_t {} );

        for ( std::size_t i { 1 }; i <= a.size(); ++i )
        {
            current[ 0 ] = i;
            for ( std::size_t j { 1 }; j <= b.size(); ++j )
                current[ j ]
                    = std::min( { previous[ j ] + 1, current[ j - 1 ] + 1, previous[ j - 1 ] + ( a[ i - 1 ] == b[ j - 1 ] ? 0u : 1u ) } );
            std::swap( previous, current );
        }

        return previous[ b.size() ];
    }

    SourceRange sub_range( const SourceRange range, const std::string_view arg, const std::size_t offset, const std::size_t length )
    {
        const auto quote = arg.contains( ' ' ) ? 1u : 0u;
        const auto begin = range.begin + static_cast< std::uint32_t >( quote + offset );
        return { begin, begin + static_cast< std::uint32_t >( std::max< std::size_t >( length, 1 ) ) };
    }
} // namespace taihu::support

export namespace taihu::support
{
    template < typename T >
    concept CLIValue = std::same_as< T, std::string >
                    || ( std::integral< T > && !std::same_as< T, bool > && !std::same_as< T, char > ) || NamedEnum< T >;

    enum class ParseResult : std::uint8_t
    {
        run,
        help,
        version,
        error
    };

    enum class Arity : std::uint8_t
    {
        flag,
        single,
        multiple
    };

    struct OptionSpec
    {
        std::string_view                          name;
        std::string_view                          value_name;
        std::string_view                          help;
        Arity                                     arity {};
        std::function< bool( std::string_view ) > store;
    };

    template < CLIValue V >
    [[nodiscard]] std::optional< V > parse_value( const std::string_view text )
    {
        if constexpr ( std::same_as< V, std::string > ) return std::string { text };
        else if constexpr ( std::integral< V > )
        {
            V value {};

            const auto [ end, err ] = std::from_chars( text.data(), text.data() + text.size(), value );
            if ( err != std::errc {} || end != text.data() + text.size() ) return std::nullopt;

            return value;
        }
        else
            return enum_from_name< V >( text );
    }

    class Parser
    {
    public:
        Parser( std::string_view name, std::string_view about );

        Parser& version( std::string_view text );

        Parser& flag( std::string_view name, bool& target, std::string_view help );

        template < CLIValue V >
        Parser& value( std::string_view name, V& target, std::string_view value_name, std::string_view help );

        template < CLIValue V >
        Parser& values( std::string_view name, std::vector< V >& target, std::string_view value_name, std::string_view help );

        template < CLIValue V >
        Parser& positionals( std::vector< V >& target, std::string_view value_name, std::string_view help, bool required = true );

        [[nodiscard]] ParseResult parse( std::span< const char* const > argv, SourceManager& sources, DiagnosticEngine& diags );

        [[nodiscard]] std::string help() const;

        [[nodiscard]] std::string_view version_text() const noexcept
        { return m_version; }

    private:
        std::string_view          m_name;
        std::string_view          m_about;
        std::string_view          m_version;
        std::vector< OptionSpec > m_options;
        OptionSpec                m_positional;
        bool                      m_positional_required {};
        std::size_t               m_positional_count {};

        [[nodiscard]] const OptionSpec* find( std::string_view name ) const;
        [[nodiscard]] const OptionSpec* find_joined( std::string_view argument ) const;

        static void store_value( const OptionSpec& spec, std::string_view value, SourceRange range, DiagnosticEngine& diags );
        void        store_positional( std::string_view arg, SourceRange range, DiagnosticEngine& diags );
        void        report_unknown( std::string_view arg, SourceRange range, DiagnosticEngine& diags ) const;
    };

    Parser::Parser( const std::string_view name, const std::string_view about ) : m_name { name }, m_about { about } { }

    Parser& Parser::version( const std::string_view text )
    {
        m_version = text;
        return *this;
    }

    Parser& Parser::flag( const std::string_view name, bool& target, const std::string_view help )
    {
        m_options.push_back( {
            .name  = name,
            .help  = help,
            .arity = Arity::flag,
            .store =
                [ &target ]( std::string_view )
            {
                target = true;
                return true;
            },
        } );
        return *this;
    }

    template < CLIValue V >
    Parser& Parser::value( const std::string_view name, V& target, const std::string_view value_name, const std::string_view help )
    {
        m_options.push_back( {
            .name       = name,
            .value_name = value_name,
            .help       = help,
            .arity      = Arity::single,
            .store =
                [ &target ]( const std::string_view text )
            {
                auto parsed = parse_value< V >( text );
                if ( !parsed ) return false;
                target = std::move( *parsed );
                return true;
            },
        } );
        return *this;
    }

    template < CLIValue V >
    Parser& Parser::values( const std::string_view name, std::vector< V >& target, const std::string_view value_name,
                            const std::string_view help )
    {
        m_options.push_back( {
            .name       = name,
            .value_name = value_name,
            .help       = help,
            .arity      = Arity::multiple,
            .store =
                [ &target ]( const std::string_view text )
            {
                auto parsed = parse_value< V >( text );
                if ( !parsed ) return false;
                target.push_back( std::move( *parsed ) );
                return true;
            },
        } );
        return *this;
    }

    template < CLIValue V >
    Parser& Parser::positionals( std::vector< V >& target, const std::string_view value_name, const std::string_view help,
                                 const bool required )
    {
        m_positional = {
            .name       = {},
            .value_name = value_name,
            .help       = help,
            .arity      = Arity::multiple,
            .store =
                [ &target ]( const std::string_view text )
            {
                auto parsed = parse_value< V >( text );
                if ( !parsed ) return false;
                target.push_back( std::move( *parsed ) );
                return true;
            },
        };
        m_positional_required = required;
        return *this;
    }

    ParseResult Parser::parse( std::span< const char* const > argv, SourceManager& sources, DiagnosticEngine& diags )
    {
        const auto line   = sources.add_cmd_line( argv );
        const auto before = diags.error_count();

        m_positional_count = 0;
        bool only_pos      = false;
        for ( std::size_t i { 1 }; i < line.args.size(); ++i )
        {
            const std::string_view arg   = line.args[ i ];
            const auto             range = line.ranges[ i ];

            if ( only_pos || arg == "-" || !arg.starts_with( '-' ) )
            {
                store_positional( arg, range, diags );
                continue;
            }

            if ( arg == "--" )
            {
                only_pos = true;
                continue;
            }

            if ( arg == "-h" || arg == "--help" ) return ParseResult::help;
            if ( !m_version.empty() && ( arg == "-V" || arg == "--version" ) ) return ParseResult::version;

            if ( const auto* spec = find( arg ) )
            {
                if ( spec->arity == Arity::flag )
                {
                    spec->store( {} );
                    continue;
                }

                if ( i + 1 >= line.args.size() )
                {
                    diags.report( DiagnosticId::missing_value, spec->name, spec->value_name )
                        .primary( range, std::format( "expected <{}> after this", spec->value_name ) );
                    continue;
                }

                ++i;
                store_value( *spec, line.args[ i ], line.ranges[ i ], diags );
                continue;
            }

            if ( const auto equals = arg.find( '=' ); equals != std::string_view::npos )
                if ( const auto* spec = find( arg.substr( 0, equals ) ) )
                {
                    const auto value       = arg.substr( equals + 1 );
                    const auto value_range = sub_range( range, arg, equals + 1, value.size() );

                    if ( spec->arity == Arity::flag )
                        diags.report( DiagnosticId::invalid_value, value, "value", spec->name )
                            .primary( value_range, "this value" )
                            .note( std::format( "`{}` is a flag and takes no value", spec->name ) )
                            .suggest( Applicability::certain, std::format( "write just `{}`", spec->name ),
                                      {
                                          { range, std::string { spec->name } }
                        } );
                    else
                        store_value( *spec, value, value_range, diags );
                    continue;
                }

            if ( const auto* spec = find_joined( arg ) )
            {
                const auto value = arg.substr( spec->name.size() );
                store_value( *spec, value, sub_range( range, arg, spec->name.size(), value.size() ), diags );
                continue;
            }

            report_unknown( arg, range, diags );
        }

        if ( m_positional_required && m_positional.store && m_positional_count == 0 )
            diags.report( DiagnosticId::missing_input )
                .help( std::format( "give one or more <{}>, for example `{} main.c`", m_positional.value_name, m_name ) );

        return diags.error_count() > before ? ParseResult::error : ParseResult::run;
    }

    void Parser::store_value( const OptionSpec& spec, const std::string_view value, const SourceRange range, DiagnosticEngine& diags )
    {
        if ( spec.store( value ) ) return;
        diags.report( DiagnosticId::invalid_value, value, spec.value_name, spec.name )
            .primary( range, std::format( "expected <{}>", spec.value_name ) );
    }

    void Parser::store_positional( const std::string_view arg, const SourceRange range, DiagnosticEngine& diags )
    {
        if ( !m_positional.store )
        {
            report_unknown( arg, range, diags );
            return;
        }

        if ( m_positional.store( arg ) ) ++m_positional_count;
        else
            diags.report( DiagnosticId::invalid_value, arg, m_positional.value_name, m_name )
                .primary( range, std::format( "expected <{}>", m_positional.value_name ) );
    }

    void Parser::report_unknown( const std::string_view arg, const SourceRange range, DiagnosticEngine& diags ) const
    {
        const auto name = arg.substr( 0, arg.find( '=' ) );

        std::string_view best {};
        std::size_t      best_distance = std::numeric_limits< std::size_t >::max();
        const auto       consider      = [ & ]( const std::string_view candidate )
        {
            if ( const auto distance = edit_distance( name, candidate ); distance < best_distance )
            {
                best          = candidate;
                best_distance = distance;
            }
        };

        for ( const auto& spec : m_options ) consider( spec.name );
        consider( "--help" );
        if ( !m_version.empty() ) consider( "--version" );

        auto builder = diags.report( DiagnosticId::unknown_option, name );
        builder.primary( range, std::format( "not a {} option", m_name ) );

        if ( !best.empty() && best_distance <= std::max< std::size_t >( 2, name.size() / 3 ) )
        {
            auto replacement = std::string { best };
            if ( name.size() < arg.size() ) replacement += arg.substr( name.size() );
            builder.suggest( Applicability::certain, std::format( "did you mean `{}`?", best ),
                             {
                                 { range, std::move( replacement ) }
            } );
        }
        else
            builder.help( std::format( "run `{} --help` to see every option", m_name ) );
    }

    std::string Parser::help() const
    {
        const auto left_of = []( const OptionSpec& spec )
        {
            return spec.arity == Arity::flag ? std::string { spec.name } : std::format( "{} <{}>", spec.name, spec.value_name );
        };

        std::vector< std::pair< std::string, std::string_view > > rows;
        for ( const auto& spec : m_options ) rows.emplace_back( left_of( spec ), spec.help );
        rows.emplace_back( "-h, --help", "print this help" );
        if ( !m_version.empty() ) rows.emplace_back( "-V, --version", "print the version" );

        std::size_t width {};
        for ( const auto& left : rows | std::views::keys ) width = std::max( width, left.size() );
        const auto positional = std::format( "<{}>", m_positional.value_name );
        if ( m_positional.store ) width = std::max( width, positional.size() );

        std::string out;
        if ( m_version.empty() ) std::format_to( std::back_inserter( out ), "{}, {}\n\n", m_name, m_about );
        else
            std::format_to( std::back_inserter( out ), "{} {}, {}\n\n", m_name, m_version, m_about );

        std::format_to( std::back_inserter( out ), "usage: {} [options]", m_name );
        if ( m_positional.store ) std::format_to( std::back_inserter( out ), " {}...", positional );
        out += "\n\noptions:\n";

        for ( const auto& [ left, text ] : rows ) std::format_to( std::back_inserter( out ), "  {:<{}}  {}\n", left, width, text );

        if ( m_positional.store )
            std::format_to( std::back_inserter( out ), "\narguments:\n  {:<{}}  {}\n", positional, width, m_positional.help );

        return out;
    }

    const OptionSpec* Parser::find( const std::string_view name ) const
    {
        const auto iter = std::ranges::find( m_options, name, &OptionSpec::name );
        return iter == m_options.end() ? nullptr : &*iter;
    }

    const OptionSpec* Parser::find_joined( const std::string_view argument ) const
    {
        const OptionSpec* best = nullptr;
        for ( const auto& spec : m_options )
            if ( spec.arity != Arity::flag && argument.size() > spec.name.size() && argument.starts_with( spec.name )
                 && ( !best || spec.name.size() > best->name.size() ) )
                best = &spec;
        return best;
    }
} // namespace taihu::support