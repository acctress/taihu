export module taihu.pp:macro;
import std;
import taihu.support;
import taihu.lex;

export namespace taihu::pp
{
    struct Macro
    {
        support::SourceRange      name {};
        std::vector< lex::Token > body;
        bool                      active {};
    };
} // namespace taihu::pp