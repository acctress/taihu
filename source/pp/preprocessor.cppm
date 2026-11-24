export module taihu.pp:preprocessor;
import std;
import taihu.support;
import taihu.lex;
import :macro;

export namespace taihu::pp
{
    class Preprocessor
    {
    public:
        Preprocessor( support::SourceManager& sources, support::DiagnosticEngine& diags, support::Interner& interner, support::FileId main,
                      lex::LexerOptions options = {} );

        void push_file( support::FileId file );

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

        using Args = std::vector< std::vector< lex::Token > >;

        support::SourceManager&                             m_sources;
        support::DiagnosticEngine&                          m_diags;
        support::Interner&                                  m_interner;
        lex::LexerOptions                                   m_options;
        std::vector< lex::Lexer >                           m_files;
        std::optional< lex::Token >                         m_peeked {};
        std::optional< Raw >                                m_unread {};
        std::vector< Ctx >                                  m_ctxs;
        std::unordered_map< const support::Symbol*, Macro > m_macros;
        lex::TokenFlags                                     m_carry {};
        bool                                                m_in_directive {};
        const support::Symbol*                              m_define { nullptr };
        const support::Symbol*                              m_undef { nullptr };
        const support::Symbol*                              m_va_args { nullptr };
        const support::Symbol*                              m_va_opt { nullptr };

        lex::Token                next_file();
        lex::Token                lex_file();
        lex::Token                peek_file();
        lex::Token                stringify( const std::vector< lex::Token >& arg, const lex::Token& hash, support::SourceRange call );
        std::vector< lex::Token > rest_of_ln();
        std::vector< lex::Token > expand_arg( const std::vector< lex::Token >& arg );
        std::vector< lex::Token > sub( const Macro& macro, const Args& args, support::SourceRange call );
        [[nodiscard]] std::vector< lex::Token > scratch( std::string_view txt, support::SourceRange call ) const;
        [[nodiscard]] std::vector< lex::Token > paste( const std::vector< Item >& items, support::SourceRange call ) const;
        Raw                                     raw();
        bool                                    enter( lex::Token& name );
        bool                                    push( Macro& macro, const lex::Token& name, std::vector< lex::Token > tokens );
        bool                                    parse_params( Macro& macro, const lex::Token& open );
        void                                    directive();
        void                                    dispatch();
        void                                    define( const lex::Token& directive );
        void                                    undef( const lex::Token& directive );
        void                                    check_body( const Macro& macro ) const;
        [[nodiscard]] bool                      same_body( const Macro& a, const Macro& b ) const;
        [[nodiscard]] std::string               cleaned( const lex::Token& token ) const;
        std::optional< Args >                   collect_args( const lex::Token& name, const Macro& macro, lex::Token& close );
    };

    Preprocessor::Preprocessor( support::SourceManager& sources, support::DiagnosticEngine& diags, support::Interner& interner,
                                const support::FileId main, const lex::LexerOptions options ) :
        m_sources { sources },
        m_diags { diags },
        m_interner { interner },
        m_options { options },
        m_define { interner.intern( "define" ) },
        m_undef { interner.intern( "undef" ) },
        m_va_args { interner.intern( "__VA_ARGS__" ) },
        m_va_opt { interner.intern( "__VA_OPT__" ) }
    { push_file( main ); }

    void Preprocessor::push_file( const support::FileId file )
    { m_files.emplace_back( m_sources, file, m_diags, m_interner, m_options ); }

    lex::Token Preprocessor::next()
    {
        using lex::TokenFlags;
        using lex::TokenKind;

        for ( ;; )
        {
            auto [ token, ff ] = raw();

            if ( ff && token.is( TokenKind::hash ) && token.has( TokenFlags::at_line_start ) )
            {
                directive();
                continue;
            }

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
            if ( const auto token = m_files.back().next(); !token.is( lex::TokenKind::eof ) || m_files.size() == 1 ) return token;
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

        if ( name.is( lex::TokenKind::identifier ) && name.symbol == m_define ) return define( name );
        if ( name.is( lex::TokenKind::identifier ) && name.symbol == m_undef ) return undef( name );

        m_diags.report( support::DiagnosticId::unknown_directive, spelling( name ) ).primary( name.range(), "not a directive" );

        static_cast< void >( rest_of_ln() );
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

    void Preprocessor::check_body( const Macro& macro ) const
    {
        using support::DiagnosticId;

        for ( const auto& token : macro.body )
            if ( token.is( lex::TokenKind::identifier ) && ( token.symbol == m_va_args || token.symbol == m_va_opt ) )
                m_diags.report( DiagnosticId::va_args_outside_variadic, token.symbol->text ).primary( token.range(), "not allowed here" );

        if ( macro.function )
            for ( std::size_t i {}; i < macro.body.size(); ++i )
                if ( macro.body[ i ].is( lex::TokenKind::hash ) && ( i + 1 == macro.body.size() || !macro.param( macro.body[ i + 1 ] ) ) )
                    m_diags.report( DiagnosticId::stringify_needs_parameter )
                        .primary( macro.body[ i ].range(), "not followed by a parameter" );

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
        using lex::TokenFlags;
        using lex::TokenKind;
        using Role = Item::Role;

        const auto& body   = macro.body;
        const auto  pasted = [ & ]( const std::size_t i )
        {
            return ( i > 0 && body[ i - 1 ].is( TokenKind::hash_hash ) )
                || ( i + 1 < body.size() && body[ i + 1 ].is( TokenKind::hash_hash ) );
        };

        std::vector< std::optional< std::vector< lex::Token > > > expanded( args.size() );
        std::vector< Item >                                       items;
        items.reserve( body.size() );

        for ( std::size_t i {}; i < body.size(); ++i )
        {
            const auto& token = body[ i ];

            if ( macro.function && token.is( TokenKind::hash ) && i + 1 < body.size() )
                if ( const auto idx = macro.param( body[ i + 1 ] ) )
                {
                    items.push_back( { stringify( args[ *idx ], token, call ) } );
                    ++i;
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
                auto& lead = items[ first ].token;
                lead.flags = ( lead.flags & ~( TokenFlags::at_line_start | TokenFlags::leading_space ) )
                           | ( token.flags & TokenFlags::leading_space );
                continue;
            }

            auto copy     = token;
            copy.location = m_sources.add_expansion( token.location, call, token.length );
            items.push_back( { copy, token.is( TokenKind::hash_hash ) ? Role::paste : Role::token } );
        }

        return paste( items, call );
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
            else if ( token.is( TokenKind::comma ) && depth == 0 )
            {
                args.emplace_back();
                continue;
            }

            args.back().push_back( token );
        }

        if ( macro.params.empty() && args.size() == 1 && args.front().empty() ) args.clear();
        if ( args.size() == macro.params.size() ) return args;

        const auto count = []( const std::size_t n )
        {
            return n == 1 ? std::string { "1 argument" } : std::format( "{} arguments", n );
        };

        m_diags.report( support::DiagnosticId::macro_argument_count, name.symbol->text, count( macro.params.size() ), count( args.size() ) )
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

    std::vector< lex::Token > Preprocessor::paste( const std::vector< Item >& items, const support::SourceRange call ) const
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

        std::vector< lex::Token > out;
        out.reserve( done.size() );
        for ( const auto& [ token, role ] : done )
            if ( role != Role::placemarker ) out.push_back( token );

        return out;
    }
} // namespace taihu::pp