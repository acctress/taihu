export module taihu.pp:macro;
import std;
import taihu.support;
import taihu.lex;

export namespace taihu::pp
{
    struct Macro
    {
        support::SourceRange                  name {};
        std::vector< lex::Token >             body;
        std::vector< const support::Symbol* > params;
        bool                                  function {};
        bool                                  active {};
        bool                                  variadic {};

        [[nodiscard]] std::size_t named() const noexcept
        { return params.size() - ( variadic ? 1 : 0 ); }

        [[nodiscard]] std::optional< std::size_t > va_opt_end( const std::size_t at ) const
        {
            if ( at + 1 >= body.size() || !body[ at + 1 ].is( lex::TokenKind::l_paren ) ) return std::nullopt;

            std::size_t depth {};
            for ( auto i { at + 1 }; i < body.size(); ++i )
            {
                if ( body[ i ].is( lex::TokenKind::l_paren ) ) ++depth;
                else if ( body[ i ].is( lex::TokenKind::r_paren ) && --depth == 0 )
                    return i;
            }

            return std::nullopt;
        }

        [[nodiscard]] std::optional< std::size_t > param( const lex::Token& token ) const
        {
            if ( !token.is( lex::TokenKind::identifier ) ) return std::nullopt;
            const auto iter = std::ranges::find( params, token.symbol );
            if ( iter == params.end() ) return std::nullopt;
            return static_cast< std::size_t >( iter - params.begin() );
        }
    };
} // namespace taihu::pp