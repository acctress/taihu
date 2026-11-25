export module taihu.pp:preprocessor;
import std;
import taihu.support;
import taihu.lex;
import :macro;
import :expr;

export namespace taihu::pp
{
    class Preprocessor
    {
    public:
        static constexpr std::size_t max_include_depth = 200;

        Preprocessor( support::SourceManager& sources, support::DiagnosticEngine& diags, support::Interner& interner, support::FileId main,
                      lex::LexerOptions options = {} );

        void push_file( support::FileId file );
        void add_include_dir( std::filesystem::path dir );

        [[nodiscard]] lex::Token       next();
        [[nodiscard]] std::string_view spelling( const lex::Token& token ) const;

    private:
        struct Ctx
        {
            std::vector< lex::Token > tokens;
            std::size_t               pos {};
            Macro*                    macro {};
            bool                      barrier {};
        };

        struct Raw
        {
            lex::Token token;
            bool       from_file {};
        };

        struct File
        {
            lex::Lexer                  lexer;
            support::FileId             id;
            std::optional< lex::Token > saved {};
        };

        enum class Directive : std::uint8_t
        {
            define,
            undef,
            include,
            if_,
            ifdef,
            ifndef,
            elif,
            elifdef,
            elifndef,
            else_,
            endif,
            error,
            warning,
            pragma
        };

        struct Cond
        {
            support::SourceRange where {};
            std::string_view     name;
            std::size_t          depth {};
            bool                 taken {};
            bool                 seen_else {};
        };

        struct Item
        {
            enum class Role : std::uint8_t
            {
                token,
                paste,
                placemarker
            };

            lex::Token token;
            Role       role {};
        };

        using Args     = std::vector< std::vector< lex::Token > >;
        using Expanded = std::vector< std::optional< std::vector< lex::Token > > >;

        support::SourceManager&                                 m_sources;
        support::DiagnosticEngine&                              m_diags;
        support::Interner&                                      m_interner;
        lex::LexerOptions                                       m_options;
        std::vector< File >                                     m_files;
        std::vector< std::filesystem::path >                    m_include_dirs;
        std::optional< lex::Token >                             m_peeked {};
        std::optional< Raw >                                    m_unread {};
        std::vector< Ctx >                                      m_ctxs;
        std::unordered_map< const support::Symbol*, Macro >     m_macros;
        lex::TokenFlags                                         m_carry {};
        bool                                                    m_in_directive {};
        bool                                                    m_skipping {};
        std::vector< Cond >                                     m_conds;
        std::optional< support::FileId >                        m_builtin;
        std::unordered_map< const support::Symbol*, Directive > m_directives;
        const support::Symbol*                                  m_defined { nullptr };
        const support::Symbol*                                  m_file_macro { nullptr };
        const support::Symbol*                                  m_line_macro { nullptr };
        const support::Symbol*                                  m_va_args { nullptr };
        const support::Symbol*                                  m_va_opt { nullptr };

        lex::Token                next_file();
        lex::Token                lex_file();
        lex::Token                peek_file();
        lex::Token                stringify( const std::vector< lex::Token >& arg, const lex::Token& hash, support::SourceRange call );
        std::vector< lex::Token > rest_of_ln();
        std::vector< lex::Token > expand_arg( const std::vector< lex::Token >& arg );
        std::vector< lex::Token > sub( const Macro& macro, const Args& args, support::SourceRange call );
        std::vector< Item > build( const Macro& macro, const Args& args, std::size_t begin, std::size_t end, support::SourceRange call,
                                   Expanded& expanded );
        std::vector< Item > va_opt( const Macro& macro, const Args& args, std::size_t at, std::size_t close, support::SourceRange call,
                                    Expanded& expanded );
        [[nodiscard]] std::vector< lex::Token > scratch( std::string_view txt, support::SourceRange call ) const;
        [[nodiscard]] std::vector< Item >       paste( const std::vector< Item >& items, support::SourceRange call ) const;
        Raw                                     raw();
        bool                                    enter( lex::Token& name );
        bool                                    push( Macro& macro, const lex::Token& name, std::vector< lex::Token > tokens );
        bool                                    parse_params( Macro& macro, const lex::Token& open );
        void                                    directive();
        void                                    dispatch();

        void define( const lex::Token& directive );
        void undef( const lex::Token& directive );
        void include( const lex::Token& directive );
        void open_if( const lex::Token& directive, Directive kind );
        void next_branch( const lex::Token& directive, Directive kind );
        void endif( const lex::Token& directive );
        void message( const lex::Token& directive, Directive kind );
        void skip_group();
        void close_conds( std::size_t depth );
        void no_extra_tokens( const lex::Token& directive );
        bool condition( const lex::Token& directive, Directive kind );
        bool evaluate( const lex::Token& directive );

        [[nodiscard]] std::optional< Directive > directive_of( const lex::Token& token ) const;

        void                                                 check_body( const Macro& macro ) const;
        [[nodiscard]] bool                                   same_body( const Macro& a, const Macro& b ) const;
        [[nodiscard]] std::string                            cleaned( const lex::Token& token ) const;
        std::optional< Args >                                collect_args( const lex::Token& name, const Macro& macro, lex::Token& close );
        [[nodiscard]] std::optional< std::filesystem::path > find_include( std::string_view name, bool angled, support::FileId from ) const;
        [[nodiscard]] std::vector< std::filesystem::path >   search_dirs( bool angled, support::FileId from ) const;
    };

    Preprocessor::Preprocessor( support::SourceManager& sources, support::DiagnosticEngine& diags, support::Interner& interner,
                                const support::FileId main, const lex::LexerOptions options ) :
        m_sources { sources },
        m_diags { diags },
        m_interner { interner },
        m_options { options },
        m_va_args { interner.intern( "__VA_ARGS__" ) },
        m_va_opt { interner.intern( "__VA_OPT__" ) }
    {
        constexpr std::array< std::pair< std::string_view, Directive >, 14 > directives {
            {
             { "define", Directive::define },
             { "undef", Directive::undef },
             { "include", Directive::include },
             { "if", Directive::if_ },
             { "ifdef", Directive::ifdef },
             { "ifndef", Directive::ifndef },
             { "elif", Directive::elif },
             { "elifdef", Directive::elifdef },
             { "elifndef", Directive::elifndef },
             { "else", Directive::else_ },
             { "endif", Directive::endif },
             { "error", Directive::error },
             { "warning", Directive::warning },
             { "pragma", Directive::pragma },
             }
        };
        for ( const auto& [ text, kind ] : directives ) m_directives.emplace( interner.intern( text ), kind );

        push_file( main );
        m_builtin = sources.add_file( "<builtin>", "#define __STDC__ 1\n"
                                                   "#define __STDC_VERSION__ 202311L\n"
                                                   "#define __STDC_HOSTED__ 1\n"
                                                   "#define __STDC_UTF_16__ 1\n"
                                                   "#define __STDC_UTF_32__ 1\n"
                                                   "#define __FILE__ __FILE__\n"
                                                   "#define __LINE__ __LINE__\n" );
    }

    void Preprocessor::push_file( const support::FileId file )
    {
        if ( m_peeked ) m_files.back().saved = std::exchange( m_peeked, std::nullopt );
        m_files.push_back( {
            lex::Lexer { m_sources, file, m_diags, m_interner, m_options },
             file
        } );
    }

    void Preprocessor::add_include_dir( std::filesystem::path dir )
    { m_include_dirs.push_back( std::move( dir ) ); }

    lex::Token Preprocessor::next()
    {
        using lex::TokenFlags;
        using lex::TokenKind;

        if ( m_builtin ) push_file( *std::exchange( m_builtin, std::nullopt ) );

        for ( ;; )
        {
            auto [ token, ff ] = raw();

            if ( ff && token.is( TokenKind::hash ) && token.has( TokenFlags::at_line_start ) )
            {
                directive();
                continue;
            }

            if ( ff && token.is( TokenKind::eof ) ) close_conds( 0 );

            if ( token.is( TokenKind::identifier ) && enter( token ) ) continue;

            token.flags |= m_carry;
            m_carry      = TokenFlags::none;

            return token;
        }
    }

    std::string_view Preprocessor::spelling( const lex::Token& token ) const
    {
        const auto [ file, offset ] = m_sources.decompose( m_sources.spelling_location( token.location ) );
        return m_sources.text( file ).substr( offset, token.length );
    }

    lex::Token Preprocessor::next_file()
    {
        for ( ;; )
        {
            auto& t = m_files.back();
            if ( t.saved ) return *std::exchange( t.saved, std::nullopt );

            m_diags.mute( m_skipping );
            const auto token = t.lexer.next();
            m_diags.mute( false );

            if ( !token.is( lex::TokenKind::eof ) || m_files.size() == 1 ) return token;
            close_conds( m_files.size() - 1 );

            m_files.pop_back();
        }
    }

    lex::Token Preprocessor::lex_file()
    {
        lex::Token tkn;
        if ( m_peeked )
        {
            tkn = *m_peeked;
            m_peeked.reset();
        }
        else
            tkn = next_file();

        if ( m_in_directive && tkn.has( lex::TokenFlags::vertical_space ) )
            m_diags.report( support::DiagnosticId::vertical_space_in_directive )
                .primary( tkn.range(), "a vertical tab or form feed comes before this" );

        return tkn;
    }

    lex::Token Preprocessor::peek_file()
    {
        if ( !m_peeked ) m_peeked = next_file();
        return *m_peeked;
    }

    std::vector< lex::Token > Preprocessor::rest_of_ln()
    {
        std::vector< lex::Token > tokens;
        for ( ;; )
        {
            if ( const auto tk = peek_file(); tk.is( lex::TokenKind::eof ) || tk.has( lex::TokenFlags::at_line_start ) ) return tokens;
            tokens.push_back( lex_file() );
        }
    }

    Preprocessor::Raw Preprocessor::raw()
    {
        if ( m_unread ) return *std::exchange( m_unread, std::nullopt );
        while ( !m_ctxs.empty() )
        {
            auto& [ tokens, pos, macro, barrier ] = m_ctxs.back();
            if ( pos < tokens.size() ) return { tokens[ pos++ ], false };
            if ( barrier ) return { lex::Token {}, false };
            macro->active = false;
            m_ctxs.pop_back();
        }

        return { lex_file(), true };
    }

    bool Preprocessor::enter( lex::Token& name )
    {
        using lex::TokenFlags;

        if ( name.has( TokenFlags::no_expand ) ) return false;

        const auto iter = m_macros.find( name.symbol );
        if ( iter == m_macros.end() ) return false;

        auto& macro = iter->second;
        if ( macro.active )
        {
            name.flags |= TokenFlags::no_expand;
            return false;
        }

        if ( ( name.symbol == m_file_macro || name.symbol == m_line_macro ) && macro.body.size() == 1
             && macro.body.front().symbol == name.symbol )
        {
            const auto where = m_sources.file_location( name.location );
            const auto text  = name.symbol == m_line_macro ? std::to_string( m_sources.line_column( where ).line )
                                                           : std::format( "{:?}", m_sources.path( m_sources.decompose( where ).file ) );

            return push( macro, name, scratch( text, name.range() ) );
        }

        if ( !macro.function ) return push( macro, name, sub( macro, {}, name.range() ) );

        if ( const auto next = raw(); !next.token.is( lex::TokenKind::l_paren ) )
        {
            m_unread = next;
            return false;
        }

        lex::Token close;
        const auto args = collect_args( name, macro, close );
        if ( !args ) return true;

        return push( macro, name, sub( macro, *args, { name.location, close.range().end } ) );
    }

    void Preprocessor::directive()
    {
        m_in_directive = true;
        dispatch();
        m_in_directive = false;
    }

    void Preprocessor::dispatch()
    {
        const auto name = peek_file();
        if ( name.is( lex::TokenKind::eof ) || name.has( lex::TokenFlags::at_line_start ) ) return;

        lex_file();

        const auto kind = directive_of( name );
        if ( !kind )
        {
            m_diags.report( support::DiagnosticId::unknown_directive, spelling( name ) ).primary( name.range(), "not a directive" );
            static_cast< void >( rest_of_ln() );
            return;
        }

        switch ( *kind )
        {
            case Directive::define: return define( name );
            case Directive::undef: return undef( name );
            case Directive::include: return include( name );
            case Directive::if_:
            case Directive::ifdef:
            case Directive::ifndef: return open_if( name, *kind );
            case Directive::elif:
            case Directive::elifdef:
            case Directive::elifndef:
            case Directive::else_: return next_branch( name, *kind );
            case Directive::endif: return endif( name );
            case Directive::error:
            case Directive::warning: return message( name, *kind );
            case Directive::pragma: static_cast< void >( rest_of_ln() ); return;
        }
    }

    void Preprocessor::define( const lex::Token& directive )
    {
        using support::DiagnosticId;

        const auto name = peek_file();
        if ( name.is( lex::TokenKind::eof ) || name.has( lex::TokenFlags::at_line_start ) )
        {
            m_diags.report( DiagnosticId::missing_macro_name, "define" ).primary( directive.range(), "expected a name after this" );

            return;
        }

        lex_file();

        if ( !name.is( lex::TokenKind::identifier ) )
        {
            m_diags.report( DiagnosticId::macro_name_not_identifier ).primary( name.range(), "not an identifier" );

            static_cast< void >( rest_of_ln() );
            return;
        }

        Macro macro { .name = name.range() };
        if ( const auto open = peek_file(); open.is( lex::TokenKind::l_paren ) && !open.has( lex::TokenFlags::at_line_start )
                                            && !open.has( lex::TokenFlags::leading_space ) )
        {
            lex_file();

            macro.function = true;
            if ( !parse_params( macro, open ) )
            {
                static_cast< void >( rest_of_ln() );
                return;
            }
        }

        macro.body = rest_of_ln();
        check_body( macro );

        if ( !macro.function && !macro.body.empty() && !macro.body.front().has( lex::TokenFlags::leading_space ) )
            m_diags.report( DiagnosticId::missing_space_after_macro_name )
                .primary( macro.body.front().range(), "add a space before this" )
                .suggest( support::Applicability::certain, "add a space",
                          {
                              { { name.range().end, name.range().end }, " " }
            } );

        if ( const auto iter = m_macros.find( name.symbol ); iter != m_macros.end() )
        {
            if ( !same_body( iter->second, macro ) )
                m_diags.report( DiagnosticId::macro_redefined, name.symbol->text )
                    .primary( name.range(), "redefined here" )
                    .secondary( iter->second.name, "first defined here" );

            iter->second = std::move( macro );
            return;
        }

        m_macros.emplace( name.symbol, std::move( macro ) );
    }

    void Preprocessor::undef( const lex::Token& directive )
    {
        using support::DiagnosticId;

        const auto name = peek_file();
        if ( name.is( lex::TokenKind::eof ) || name.has( lex::TokenFlags::at_line_start ) )
        {
            m_diags.report( DiagnosticId::missing_macro_name, "undef" ).primary( directive.range(), "expected a name after this" );

            return;
        }

        lex_file();

        if ( !name.is( lex::TokenKind::identifier ) )
        {
            m_diags.report( DiagnosticId::macro_name_not_identifier ).primary( name.range(), "not an identifier" );

            static_cast< void >( rest_of_ln() );
            return;
        }

        m_macros.erase( name.symbol );

        if ( const auto ext = rest_of_ln(); !ext.empty() )
            m_diags.report( DiagnosticId::extra_tokens_after_directive, "undef" )
                .primary( { ext.front().location, ext.back().range().end }, "`#undef` takes one name" );
    }

    void Preprocessor::include( const lex::Token& directive )
    {
        using lex::TokenFlags;
        using lex::TokenKind;
        using support::DiagnosticId;

        const auto           from = m_files.back().id;
        std::string          name;
        bool                 angled {};
        support::SourceRange where {};

        if ( const auto header = m_files.back().lexer.next_header_name() )
        {
            const auto text = spelling( *header );
            angled          = text.front() == '<';
            name            = text.substr( 1, text.size() - 2 );
            where           = header->range();

            if ( const auto ext = rest_of_ln(); !ext.empty() )
                m_diags.report( DiagnosticId::extra_tokens_after_directive, "include" )
                    .primary( { ext.front().location, ext.back().range().end }, "`#include` takes one file name" );
        }
        else
        {
            const auto line = rest_of_ln();
            if ( line.empty() )
            {
                m_diags.report( DiagnosticId::bad_include ).primary( directive.range(), "expected a file name after this" );
                return;
            }

            where = { line.front().location, line.back().range().end };

            if ( const auto tokens = expand_arg( line );
                 tokens.size() == 1 && tokens.front().is( TokenKind::string_literal ) && tokens.front().encoding == lex::Encoding::none )
            {
                const auto text = cleaned( tokens.front() );
                name            = text.substr( 1, text.size() - 2 );
            }
            else if ( tokens.size() >= 2 && tokens.front().is( TokenKind::less ) && tokens.back().is( TokenKind::greater ) )
            {
                angled = true;
                for ( std::size_t i { 1 }; i + 1 < tokens.size(); ++i )
                {
                    if ( i > 1 && tokens[ i ].has( TokenFlags::leading_space ) ) name += ' ';
                    name += cleaned( tokens[ i ] );
                }
            }
            else
            {
                m_diags.report( DiagnosticId::bad_include ).primary( where, "this is not \"file\" or <file>" );
                return;
            }
        }

        if ( name.empty() )
        {
            m_diags.report( DiagnosticId::bad_include ).primary( where, "the file name is empty" );
            return;
        }

        if ( m_files.size() >= max_include_depth )
        {
            m_diags.report( DiagnosticId::include_too_deep, max_include_depth ).primary( where, "included here" );
            return;
        }

        const auto path = find_include( name, angled, from );
        if ( !path )
        {
            std::string searched;
            for ( const auto& dir : search_dirs( angled, from ) )
                std::format_to( std::back_inserter( searched ), "{}`{}`", searched.empty() ? "" : ", ", dir.generic_string() );

            auto report = m_diags.report( DiagnosticId::include_not_found, name );
            report.primary( where, "not found" );
            if ( searched.empty() ) report.help( "no folders to search; add one with `-I <dir>`" );
            else
                report.help( std::format( "searched {}", searched ) );
            return;
        }

        const auto file = m_sources.load_file( *path );
        if ( !file )
        {
            m_diags.report( DiagnosticId::cannot_open_file, path->generic_string() ).primary( where, "included here" );
            return;
        }

        push_file( *file );
    }

    void Preprocessor::endif( const lex::Token& directive )
    {
        if ( m_conds.empty() )
            m_diags.report( support::DiagnosticId::unmatched_conditional, spelling( directive ) )
                .primary( directive.range(), "no `#if` if open" );
        else
            m_conds.pop_back();

        no_extra_tokens( directive );
    }

    void Preprocessor::check_body( const Macro& macro ) const
    {
        using support::DiagnosticId;

        const auto& body      = macro.body;
        const auto  is_va_opt = [ & ]( const lex::Token& token )
        {
            return token.is( lex::TokenKind::identifier ) && token.symbol == m_va_opt;
        };

        if ( !macro.variadic )
            for ( const auto& token : body )
                if ( token.is( lex::TokenKind::identifier ) && ( token.symbol == m_va_args || token.symbol == m_va_opt ) )
                    m_diags.report( DiagnosticId::va_args_outside_variadic, token.symbol->text )
                        .primary( token.range(), "not allowed here" );

        if ( macro.variadic )
            for ( std::size_t i {}; i < body.size(); ++i )
            {
                if ( !is_va_opt( body[ i ] ) ) continue;

                const auto close = macro.va_opt_end( i );
                if ( !close )
                {
                    m_diags.report( DiagnosticId::va_opt_needs_parens )
                        .primary( body[ i ].range(), "expected `(` and a matching `)` afer this" );
                    continue;
                }

                for ( auto j { i + 2 }; j < *close; ++j )
                    if ( is_va_opt( body[ j ] ) ) m_diags.report( DiagnosticId::nested_va_opt ).primary( body[ j ].range(), "nested here" );

                if ( *close > i + 2 && body[ i + 2 ].is( lex::TokenKind::hash_hash ) )
                    m_diags.report( DiagnosticId::hash_hash_at_edge, "start" )
                        .primary( body[ i + 2 ].range(), "nothing to paste on the left" );

                if ( *close > i + 3 && body[ *close - 1 ].is( lex::TokenKind::hash_hash ) )
                    m_diags.report( DiagnosticId::hash_hash_at_edge, "end" )
                        .primary( body[ *close - 1 ].range(), "nothing to paste on the right" );

                i = *close;
            }

        if ( macro.function )
            for ( std::size_t i {}; i < body.size(); ++i )
                if ( body[ i ].is( lex::TokenKind::hash )
                     && ( i + 1 == body.size() || ( !macro.param( body[ i + 1 ] ) && !( macro.variadic && is_va_opt( body[ i + 1 ] ) ) ) ) )
                    m_diags.report( DiagnosticId::stringify_needs_parameter ).primary( body[ i ].range(), "not followed by a parameter" );

        if ( macro.body.empty() ) return;
        if ( macro.body.front().is( lex::TokenKind::hash_hash ) )
            m_diags.report( DiagnosticId::hash_hash_at_edge, "start" )
                .primary( macro.body.front().range(), "nothing to paste on the left" );
        if ( macro.body.size() > 1 && macro.body.back().is( lex::TokenKind::hash_hash ) )
            m_diags.report( DiagnosticId::hash_hash_at_edge, "end" ).primary( macro.body.back().range(), "nothing to paste on the right" );
    }

    bool Preprocessor::same_body( const Macro& a, const Macro& b ) const
    {
        if ( a.function != b.function || a.params != b.params ) return false;
        if ( a.body.size() != b.body.size() ) return false;

        for ( std::size_t i {}; i < a.body.size(); ++i )
        {
            const auto& x = a.body[ i ];
            const auto& y = b.body[ i ];
            if ( x.kind != y.kind ) return false;
            if ( x.is( lex::TokenKind::identifier ) ? x.symbol != y.symbol : cleaned( x ) != cleaned( y ) ) return false;
            if ( i > 0 && x.has( lex::TokenFlags::leading_space ) != y.has( lex::TokenFlags::leading_space ) ) return false;
        }
        return true;
    }

    std::string Preprocessor::cleaned( const lex::Token& token ) const
    {
        const auto text = spelling( token );
        return token.has( lex::TokenFlags::needs_cleaning ) ? lex::clean_spelling( text, m_options ) : std::string { text };
    }

    bool Preprocessor::push( Macro& macro, const lex::Token& name, std::vector< lex::Token > tokens )
    {
        using lex::TokenFlags;

        constexpr auto pos = TokenFlags::at_line_start | TokenFlags::leading_space;
        if ( tokens.empty() )
        {
            m_carry |= name.flags & pos;
            return true;
        }

        for ( auto& token : tokens )
            if ( token.has( TokenFlags::at_line_start ) )
                token.flags = ( token.flags & ~TokenFlags::at_line_start ) | TokenFlags::leading_space;

        auto& first = tokens.front();
        first.flags = ( first.flags & ~pos ) | ( name.flags & pos );

        macro.active = true;
        m_ctxs.push_back( { .tokens = std::move( tokens ), .macro = &macro } );
        return true;
    }

    std::vector< lex::Token > Preprocessor::expand_arg( const std::vector< lex::Token >& arg )
    {
        const auto carry = std::exchange( m_carry, lex::TokenFlags::none );
        m_ctxs.push_back( { .tokens = arg, .barrier = true } );

        std::vector< lex::Token > out;
        for ( ;; )
        {
            auto [ token, ff ] = raw();
            if ( token.is( lex::TokenKind::eof ) ) break;
            if ( token.is( lex::TokenKind::identifier ) && enter( token ) ) continue;

            token.flags |= m_carry;
            m_carry      = lex::TokenFlags::none;
            out.push_back( token );
        }

        m_ctxs.pop_back();
        m_carry = carry;
        return out;
    }

    std::vector< lex::Token > Preprocessor::sub( const Macro& macro, const Args& args, support::SourceRange call )
    {
        Expanded   expanded( args.size() );
        const auto items = paste( build( macro, args, 0, macro.body.size(), call, expanded ), call );

        std::vector< lex::Token > out;
        out.reserve( items.size() );
        for ( const auto& [ token, role ] : items )
            if ( role != Item::Role::placemarker ) out.push_back( token );
        return out;
    }

    bool Preprocessor::parse_params( Macro& macro, const lex::Token& open )
    {
        using lex::TokenKind;
        using support::DiagnosticId;

        const auto on_line = []( const lex::Token& token )
        {
            return !token.is( TokenKind::eof ) && !token.has( lex::TokenFlags::at_line_start );
        };

        if ( const auto close = peek_file(); on_line( close ) && close.is( TokenKind::r_paren ) )
        {
            lex_file();
            return true;
        }

        auto last = open;
        for ( ;; )
        {
            const auto param = peek_file();
            if ( !on_line( param ) )
            {
                m_diags.report( DiagnosticId::expected_parameter ).primary( last.range(), "expected a parameter name after this" );
                return false;
            }

            if ( param.is( TokenKind::ellipsis ) )
            {
                lex_file();

                macro.variadic = true;
                macro.params.push_back( m_va_args );

                if ( const auto close = peek_file(); on_line( close ) && close.is( TokenKind::r_paren ) )
                {
                    lex_file();
                    return true;
                }

                m_diags.report( DiagnosticId::unclosed_parameter_list ).primary( param.range(), "`...` must be the last parameter" );
                return false;
            }

            if ( !param.is( TokenKind::identifier ) )
            {
                m_diags.report( DiagnosticId::expected_parameter ).primary( param.range(), "not an identifier" );
                return false;
            }

            lex_file();

            if ( param.symbol == m_va_args || param.symbol == m_va_opt )
                m_diags.report( DiagnosticId::va_args_outside_variadic, param.symbol->text ).primary( param.range(), "not allowed here" );
            else if ( std::ranges::contains( macro.params, param.symbol ) )
                m_diags.report( DiagnosticId::duplicate_parameter, param.symbol->text ).primary( param.range(), "declared again here" );
            else
                macro.params.push_back( param.symbol );

            const auto sep = peek_file();
            if ( on_line( sep ) && sep.is( TokenKind::comma ) )
            {
                last = lex_file();
                continue;
            }

            if ( on_line( sep ) && sep.is( TokenKind::r_paren ) )
            {
                lex_file();
                return true;
            }

            m_diags.report( DiagnosticId::unclosed_parameter_list )
                .primary( on_line( sep ) ? sep.range() : param.range(),
                          on_line( sep ) ? "expected `,` or `)` here" : "the list ends here without a `)`" );

            return false;
        }
    }

    std::optional< Preprocessor::Args > Preprocessor::collect_args( const lex::Token& name, const Macro& macro, lex::Token& close )
    {
        using lex::TokenKind;

        Args        args( 1 );
        std::size_t depth {};

        for ( ;; )
        {
            const auto  next  = raw();
            const auto& token = next.token;

            if ( token.is( TokenKind::eof ) )
            {
                m_unread = next;
                m_diags.report( support::DiagnosticId::unterminated_macro_call, name.symbol->text )
                    .primary( name.range(), "this call is never closed" );
                return std::nullopt;
            }

            if ( token.is( TokenKind::l_paren ) ) ++depth;
            else if ( token.is( TokenKind::r_paren ) )
            {
                if ( depth == 0 )
                {
                    close = token;
                    break;
                }
                --depth;
            }
            else if ( token.is( TokenKind::comma ) && depth == 0 && !( macro.variadic && args.size() == macro.params.size() ) )
            {
                args.emplace_back();
                continue;
            }

            args.back().push_back( token );
        }

        if ( !macro.variadic && macro.params.empty() && args.size() == 1 && args.front().empty() ) args.clear();
        if ( macro.variadic && args.size() == macro.named() ) args.emplace_back();
        if ( args.size() == macro.params.size() ) return args;

        const auto count = []( const std::size_t n )
        {
            return n == 1 ? std::string { "1 argument" } : std::format( "{} arguments", n );
        };
        const auto takes = macro.variadic ? "at least " + count( macro.named() ) : count( macro.params.size() );

        m_diags.report( support::DiagnosticId::macro_argument_count, name.symbol->text, takes, count( args.size() ) )
            .primary( name.range(), std::format( "given {} here", count( args.size() ) ) )
            .secondary( macro.name, "defined here" );

        return std::nullopt;
    }

    lex::Token Preprocessor::stringify( const std::vector< lex::Token >& arg, const lex::Token& hash, support::SourceRange call )
    {
        using lex::TokenFlags;
        using lex::TokenKind;

        std::string text = "\"";
        for ( std::size_t i {}; i < arg.size(); ++i )
        {
            const auto& token = arg[ i ];
            if ( i > 0 && token.has( TokenFlags::leading_space | TokenFlags::at_line_start ) ) text += ' ';

            const auto spelling = cleaned( token );
            if ( !token.is( TokenKind::string_literal ) && !token.is( TokenKind::char_constant ) )
            {
                text += spelling;
                continue;
            }

            for ( const char c : spelling )
            {
                if ( c == '"' || c == '\\' ) text += '\\';
                text += c;
            }
        }

        text += '"';

        const auto tokens = scratch( text, call );
        if ( tokens.size() != 1 || !tokens.front().is( TokenKind::string_literal ) )
            m_diags.report( support::DiagnosticId::invalid_stringify )
                .primary( call, "in this expansion" )
                .secondary( hash.range(), "stringified here" );

        auto result = tokens.front();
        result.flags
            = ( result.flags & ~( TokenFlags::at_line_start | TokenFlags::leading_space ) ) | ( hash.flags & TokenFlags::leading_space );

        return result;
    }

    std::vector< lex::Token > Preprocessor::scratch( const std::string_view txt, const support::SourceRange call ) const
    {
        const auto                file = m_sources.add_file( "<scratch>", std::string { txt } );
        support::DiagnosticEngine diags { m_sources };
        lex::Lexer                lexer { m_sources, file, diags, m_interner, m_options };

        std::vector< lex::Token > tokens;
        for ( auto token = lexer.next(); !token.is( lex::TokenKind::eof ); token = lexer.next() )
        {
            token.location = m_sources.add_expansion( token.location, call, token.length );
            tokens.push_back( token );
        }

        return tokens;
    }

    std::vector< Preprocessor::Item > Preprocessor::paste( const std::vector< Item >& items, const support::SourceRange call ) const
    {
        using lex::TokenFlags;
        using Role = Item::Role;

        constexpr auto pos = TokenFlags::at_line_start | TokenFlags::leading_space;

        std::vector< Item > done;
        done.reserve( items.size() );

        for ( std::size_t i {}; i < items.size(); ++i )
        {
            if ( items[ i ].role != Role::paste || done.empty() || i + 1 == items.size() )
            {
                done.push_back( items[ i ] );
                continue;
            }

            auto&       lhs = done.back();
            const auto& rhs = items[ ++i ];

            if ( rhs.role == Role::placemarker ) continue;
            if ( lhs.role == Role::placemarker )
            {
                const auto space = lhs.token.flags & TokenFlags::leading_space;
                lhs              = rhs;
                lhs.token.flags  = lhs.token.flags & ~pos | space;
                continue;
            }

            const auto left  = cleaned( lhs.token );
            const auto right = cleaned( rhs.token );

            if ( auto tokens = scratch( left + right, call ); tokens.size() == 1 )
            {
                tokens.front().flags = tokens.front().flags & ~pos | lhs.token.flags & TokenFlags::leading_space;
                lhs.token            = tokens.front();
                continue;
            }

            m_diags.report( support::DiagnosticId::invalid_paste, left, right ).primary( call, "pasted in this expansion" );
            done.push_back( rhs );
        }

        return done;
    }

    std::vector< Preprocessor::Item > Preprocessor::build( const Macro& macro, const Args& args, std::size_t begin, std::size_t end,
                                                           support::SourceRange call, Expanded& expanded )
    {
        using lex::TokenFlags;
        using lex::TokenKind;
        using Role = Item::Role;

        const auto& body   = macro.body;
        const auto  pasted = [ & ]( const std::size_t i )
        {
            return ( i > begin && body[ i - 1 ].is( TokenKind::hash_hash ) ) || ( i + 1 < end && body[ i + 1 ].is( TokenKind::hash_hash ) );
        };
        const auto is_va_opt = [ & ]( const lex::Token& token )
        {
            return macro.variadic && token.is( TokenKind::identifier ) && token.symbol == m_va_opt;
        };
        const auto lead_with = []( std::vector< Item >& items, const std::size_t first, const lex::Token& from )
        {
            if ( first == items.size() ) return;
            auto& lead = items[ first ].token;
            lead.flags
                = ( lead.flags & ~( TokenFlags::at_line_start | TokenFlags::leading_space ) ) | ( from.flags & TokenFlags::leading_space );
        };

        std::vector< Item > items;
        items.reserve( end - begin );

        for ( auto i = begin; i < end; ++i )
        {
            const auto& token = body[ i ];

            if ( macro.function && token.is( TokenKind::hash ) && i + 1 < end )
            {
                if ( const auto idx = macro.param( body[ i + 1 ] ) )
                {
                    items.push_back( { stringify( args[ *idx ], token, call ) } );
                    ++i;
                    continue;
                }

                if ( is_va_opt( body[ i + 1 ] ) )
                    if ( const auto close = macro.va_opt_end( i + 1 ); close && *close < end )
                    {
                        std::vector< lex::Token > tokens;
                        for ( const auto& [ piece, role ] : va_opt( macro, args, i + 1, *close, call, expanded ) )
                            if ( role != Role::placemarker ) tokens.push_back( piece );
                        items.push_back( { stringify( tokens, token, call ) } );
                        i = *close;
                        continue;
                    }
            }

            if ( is_va_opt( token ) )
                if ( const auto close = macro.va_opt_end( i ); close && *close < end )
                {
                    const auto first = items.size();
                    items.append_range( va_opt( macro, args, i, *close, call, expanded ) );
                    lead_with( items, first, token );
                    i = *close;
                    continue;
                }

            if ( const auto idx = macro.param( token ) )
            {
                const auto raw_arg = pasted( i );
                if ( !raw_arg && !expanded[ *idx ] ) expanded[ *idx ] = expand_arg( args[ *idx ] );
                const auto& arg = raw_arg ? args[ *idx ] : *expanded[ *idx ];

                if ( arg.empty() )
                {
                    if ( raw_arg ) items.push_back( { token, Role::placemarker } );
                    continue;
                }

                const auto first = items.size();
                for ( const auto& piece : arg ) items.push_back( { piece } );
                lead_with( items, first, token );
                continue;
            }

            auto copy     = token;
            copy.location = m_sources.add_expansion( token.location, call, token.length );
            items.push_back( { copy, token.is( TokenKind::hash_hash ) ? Role::paste : Role::token } );
        }

        return items;
    }

    std::vector< Preprocessor::Item > Preprocessor::va_opt( const Macro& macro, const Args& args, const std::size_t at,
                                                            const std::size_t close, const support::SourceRange call, Expanded& expanded )
    {
        const auto va = macro.params.size() - 1;
        if ( !expanded[ va ] ) expanded[ va ] = expand_arg( args[ va ] );
        if ( expanded[ va ]->empty() )
            return {
                { macro.body[ at ], Item::Role::placemarker }
            };

        auto inner = paste( build( macro, args, at + 2, close, call, expanded ), call );
        if ( inner.empty() )
            return {
                { macro.body[ at ], Item::Role::placemarker }
            };
        return inner;
    }

    std::optional< std::filesystem::path > Preprocessor::find_include( const std::string_view name, const bool angled,
                                                                       const support::FileId from ) const
    {
        std::filesystem::path relative { name };
        std::error_code             error;

        if ( relative.is_absolute() )
        {
            if ( std::filesystem::is_regular_file( relative, error ) ) return relative;
            return std::nullopt;
        }

        for ( const auto& dir : search_dirs( angled, from ) )
            if ( const auto candidate = ( dir / relative ).lexically_normal(); std::filesystem::is_regular_file( candidate, error ) )
                return std::filesystem::path { candidate.generic_string() };

        return std::nullopt;
    }

    std::vector< std::filesystem::path > Preprocessor::search_dirs( const bool angled, const support::FileId from ) const
    {
        std::vector< std::filesystem::path > dirs;
        if ( !angled )
        {
            auto here = std::filesystem::path { m_sources.path( from ) }.parent_path();
            dirs.push_back( here.empty() ? std::filesystem::path { "." } : std::move( here ) );
        }

        dirs.append_range( m_include_dirs );
        return dirs;
    }

    void Preprocessor::open_if( const lex::Token& directive, Directive kind )
    {
        const auto depth = m_files.size();
        const auto value = condition( directive, kind );

        m_conds.push_back( { .where = directive.range(), .name = spelling( directive ), .depth = depth, .taken = value } );

        if ( !value ) skip_group();
    }

    void Preprocessor::next_branch( const lex::Token& directive, Directive kind )
    {
        using support::DiagnosticId;
        if ( m_conds.empty() )
        {
            m_diags.report( DiagnosticId::unmatched_conditional, spelling( directive ) ).primary( directive.range(), "no `#if` is open" );
            static_cast< void >( rest_of_ln() );
            return;
        }

        auto& cond = m_conds.back();
        if ( cond.seen_else )
            m_diags.report( DiagnosticId::else_after_else, spelling( directive ) ).primary( directive.range(), "this comes after `#else`" );

        if ( kind == Directive::else_ )
        {
            cond.seen_else = true;
            no_extra_tokens( directive );
        }
        else
            static_cast< void >( rest_of_ln() );

        skip_group();
    }

    void Preprocessor::message( const lex::Token& directive, Directive kind )
    {
        const auto  line = rest_of_ln();
        std::string text;
        for ( const auto& token : line )
        {
            if ( !text.empty() && token.has( lex::TokenFlags::leading_space ) ) text += ' ';
            text += cleaned( token );
        }

        const auto range = line.empty() ? directive.range() : support::SourceRange { directive.location, line.back().range().end };
        const auto id    = kind == Directive::error ? support::DiagnosticId::error_directive : support::DiagnosticId::warning_directive;
        m_diags.report( id, text.empty() ? std::format( "#{}", spelling( directive ) ) : text ).primary( range );
    }

    void Preprocessor::skip_group()
    {
        using lex::TokenFlags;
        using lex::TokenKind;

        const auto  prec = m_conds.size();
        std::size_t depth {};

        m_in_directive = false;
        m_skipping     = true;
        for ( ;; )
        {
            const auto token = lex_file();
            if ( token.is( TokenKind::eof ) || m_conds.size() < prec )
            {
                m_peeked = token;
                break;
            }

            if ( !token.is( TokenKind::hash ) || !token.has( TokenFlags::at_line_start ) ) continue;

            const auto name = peek_file();
            if ( name.has( TokenFlags::at_line_start ) ) continue;

            const auto kind = directive_of( name );
            if ( !kind ) continue;

            if ( *kind == Directive::if_ || *kind == Directive::ifdef || *kind == Directive::ifndef )
            {
                ++depth;
                continue;
            }

            if ( depth > 0 )
            {
                if ( *kind == Directive::endif ) --depth;
                continue;
            }

            if ( *kind != Directive::endif && *kind != Directive::else_ && *kind != Directive::elif && *kind != Directive::elifdef
                 && *kind != Directive::elifndef )
                continue;

            lex_file();
            m_skipping     = false;
            m_in_directive = true;

            if ( *kind == Directive::endif )
            {
                endif( name );
                return;
            }

            auto& cond = m_conds.back();
            if ( cond.seen_else )
                m_diags.report( support::DiagnosticId::else_after_else, spelling( name ) )
                    .primary( name.range(), "this comes after `#else`" );

            if ( *kind == Directive::else_ )
            {
                cond.seen_else = true;
                no_extra_tokens( name );
                if ( !cond.taken )
                {
                    cond.taken = true;
                    return;
                }
            }
            else if ( cond.taken )
                static_cast< void >( rest_of_ln() );
            else if ( condition( name, *kind ) )
            {
                cond.taken = true;
                return;
            }

            m_in_directive = false;
            m_skipping     = true;
        }

        m_skipping = false;
    }

    void Preprocessor::close_conds( const std::size_t depth )
    {
        while ( !m_conds.empty() && m_conds.back().depth > depth )
        {
            const auto& cond = m_conds.back();

            m_diags.report( support::DiagnosticId::unterminated_conditional, cond.name ).primary( cond.where, "never closed by `#endif`" );
            m_conds.pop_back();
        }
    }

    void Preprocessor::no_extra_tokens( const lex::Token& directive )
    {
        if ( const auto ext = rest_of_ln(); !ext.empty() )
            m_diags.report( support::DiagnosticId::extra_tokens_after_directive, spelling( directive ) )
                .primary( { ext.front().location, ext.back().range().end }, "not expected here" );
    }

    bool Preprocessor::condition( const lex::Token& directive, Directive kind )
    {
        using support::DiagnosticId;

        if ( kind == Directive::if_ || kind == Directive::elif ) return evaluate( directive );

        const auto name = peek_file();
        if ( name.is( lex::TokenKind::eof ) || name.has( lex::TokenFlags::at_line_start ) )
        {
            m_diags.report( DiagnosticId::missing_macro_name, spelling( directive ) )
                .primary( directive.range(), "expected a name after this" );
            return false;
        }
        lex_file();

        if ( !name.is( lex::TokenKind::identifier ) )
        {
            m_diags.report( DiagnosticId::macro_name_not_identifier ).primary( name.range(), "not an identifier" );
            static_cast< void >( rest_of_ln() );
            return false;
        }

        no_extra_tokens( directive );

        const bool defined = m_macros.contains( name.symbol );
        return kind == Directive::ifdef || kind == Directive::elifdef ? defined : !defined;
    }

    bool Preprocessor::evaluate( const lex::Token& directive )
    {
        using lex::TokenKind;

        const auto line = rest_of_ln();
        if ( line.empty() )
        {
            m_diags.report( support::DiagnosticId::bad_if_expression, spelling( directive ) )
                .primary( directive.range(), "expected an expression after this" );
            return false;
        }

        std::vector< lex::Token > replaced;
        for ( std::size_t i {}; i < line.size(); ++i )
        {
            const auto& token = line[ i ];
            if ( !token.is( TokenKind::identifier ) || token.symbol != m_defined )
            {
                replaced.push_back( token );
                continue;
            }

            const bool paren = i + 1 < line.size() && line[ i + 1 ].is( TokenKind::l_paren );
            const auto at    = i + ( paren ? 2 : 1 );
            const auto last  = paren ? at + 1 : at;
            if ( at >= line.size() || !line[ at ].is( TokenKind::identifier )
                 || ( paren && ( last >= line.size() || !line[ last ].is( TokenKind::r_paren ) ) ) )
            {
                m_diags.report( support::DiagnosticId::defined_needs_name ).primary( token.range(), "expected a macro name after this" );
                return false;
            }

            auto value
                = scratch( m_macros.contains( line[ at ].symbol ) ? "1" : "0", { token.location, line[ last ].range().end } ).front();
            value.flags = token.flags;
            replaced.push_back( value );
            i = last;
        }

        const auto tokens = expand_arg( replaced );
        IfExpr     expression {
            tokens,
                { line.front().location, line.back().range().end },
            spelling( directive ),
            m_diags,
            [ this ]( const lex::Token& token )
            {
                return cleaned( token );
                  }
        };

        const auto value = expression.evaluate();
        return value && value->bits != 0;
    }

    std::optional< Preprocessor::Directive > Preprocessor::directive_of( const lex::Token& token ) const
    {
        if ( !token.is( lex::TokenKind::identifier ) ) return std::nullopt;

        const auto iter = m_directives.find( token.symbol );
        if ( iter == m_directives.end() ) return std::nullopt;

        return iter->second;
    }
} // namespace taihu::pp