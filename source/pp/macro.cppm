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

        [[nodiscard]] std::optional< std::size_t > param( const lex::Token& token ) const
        {
            if ( !token.is( lex::TokenKind::identifier ) ) return std::nullopt;
            const auto iter = std::ranges::find( params, token.symbol );
            if ( iter == params.end() ) return std::nullopt;
            return static_cast< std::size_t >( iter - params.begin() );
        }
    };
} // namespace taihu::pp