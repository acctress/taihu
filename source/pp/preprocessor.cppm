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
        };

        struct Raw
        {
            lex::Token token;
            bool       from_file {};
        };

        support::SourceManager&                             m_sources;
        support::DiagnosticEngine&                          m_diags;
        support::Interner&                                  m_interner;
        lex::LexerOptions                                   m_options;
        std::vector< lex::Lexer >                           m_files;
        std::optional< lex::Token >                         m_peeked {};
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
        std::vector< lex::Token > rest_of_ln();
        Raw                       raw();
        bool                      enter( lex::Token& name );
        void                      directive();
        void                      dispatch();
        void                      define( const lex::Token& directive );
        void                      undef( const lex::Token& directive );
        void                      check_body( const Macro& macro );
        [[nodiscard]] bool        same_body( const Macro& a, const Macro& b ) const;
        [[nodiscard]] std::string cleaned( const lex::Token& token ) const;
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
    {
        push_file( main );
    }

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
            const auto token = m_files.back().next();
            if ( !token.is( lex::TokenKind::eof ) || m_files.size() == 1 ) return token;
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
        while ( !m_ctxs.empty() )
        {
            auto& [ tokens, pos, macro ] = m_ctxs.back();
            if ( pos < tokens.size() ) return { tokens[ pos++ ], false };

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

        constexpr auto pos = TokenFlags::at_line_start | TokenFlags::leading_space;
        if ( macro.body.empty() )
        {
            m_carry |= name.flags & pos;
            return true;
        }

        Ctx ctx { .macro = &macro };
        ctx.tokens.reserve( macro.body.size() );
        for ( const auto& body : macro.body )
        {
            auto token     = body;
            token.location = m_sources.add_expansion( body.location, name.range(), body.length );
            token.flags    = token.flags & ~TokenFlags::at_line_start;
            ctx.tokens.push_back( token );
        }

        auto& first = ctx.tokens.front();
        first.flags = ( first.flags & ~pos ) | name.flags & pos;

        macro.active = true;
        m_ctxs.push_back( std::move( ctx ) );

        return true;
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

        Macro macro { .name = name.range(), .body = rest_of_ln() };
        check_body( macro );

        if ( !macro.body.empty() && !macro.body.front().has( lex::TokenFlags::leading_space ) )
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

    void Preprocessor::check_body( const Macro& macro )
    {
        using support::DiagnosticId;

        for ( const auto& token : macro.body )
            if ( token.is( lex::TokenKind::identifier ) && ( token.symbol == m_va_args || token.symbol == m_va_opt ) )
                m_diags.report( DiagnosticId::va_args_outside_variadic, token.symbol->text ).primary( token.range(), "not allowed here" );

        if ( macro.body.empty() ) return;
        if ( macro.body.front().is( lex::TokenKind::hash_hash ) )
            m_diags.report( DiagnosticId::hash_hash_at_edge, "start" )
                .primary( macro.body.front().range(), "nothing to paste on the left" );
        if ( macro.body.size() > 1 && macro.body.back().is( lex::TokenKind::hash_hash ) )
            m_diags.report( DiagnosticId::hash_hash_at_edge, "end" ).primary( macro.body.back().range(), "nothing to paste on the right" );
    }

    bool Preprocessor::same_body( const Macro& a, const Macro& b ) const
    {
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

} // namespace taihu::pp