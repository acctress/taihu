export module taihu.support:interner;
import std;

export namespace taihu::support
{
    struct Symbol
    {
        std::string_view text;
    };

    class Interner
    {
    public:
        [[nodiscard]] const Symbol* intern( std::string_view text );

        [[nodiscard]] std::size_t size() const noexcept
        { return m_lookup.size(); }

    private:
        std::deque< std::string >                             m_strings;
        std::deque< Symbol >                                  m_symbols;
        std::unordered_map< std::string_view, const Symbol* > m_lookup;
    };

    const Symbol* Interner::intern( const std::string_view text )
    {
        if ( const auto iter = m_lookup.find( text ); iter != m_lookup.end() ) return iter->second;

        const auto& stored = m_strings.emplace_back( text );
        const auto& symbol = m_symbols.emplace_back( stored );
        m_lookup.emplace( stored, &symbol );
        return &symbol;
    }
} // namespace taihu::support