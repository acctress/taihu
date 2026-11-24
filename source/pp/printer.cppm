export module taihu.pp:printer;
import std;
import taihu.support;
import taihu.lex;
import :preprocessor;

export namespace taihu::pp
{
    struct PrintOptions
    {
        bool line_markers { true };
        bool color {};
    };

    class Printer
    {
    public:
        Printer( const support::SourceManager& sources, const Preprocessor& preprocessor, PrintOptions options = {} );

        void add( const lex::Token& token );

        [[nodiscard]] std::string finish();

    private:
        const support::SourceManager& m_sources;
        const Preprocessor&           m_preprocessor;
        PrintOptions                  m_options;
        support::Painter              m_painter;
        std::string                   m_out;
        support::FileId               m_file {};
        std::uint32_t                 m_line {};
        bool                          m_started {};

        void move_to( support::FileId file, std::uint32_t line );
        void marker( support::FileId file, std::uint32_t line );
        void indent( support::FileId file, std::uint32_t line, std::uint32_t column );
    };

    inline constexpr support::Style marker_style { .fg = support::Color::rgb( 170, 165, 195 ), .italic = true };
    inline constexpr support::Style expanded_style { .fg = support::Color::rgb( 185, 170, 255 ) };
    inline constexpr support::Style painted_style { .fg = support::Color::rgb( 255, 212, 150 ), .underline = true };

    Printer::Printer( const support::SourceManager& sources, const Preprocessor& preprocessor, const PrintOptions options ) :
        m_sources { sources },
        m_preprocessor { preprocessor },
        m_options { options },
        m_painter { options.color }
    {
    }

    void Printer::add( const lex::Token& token )
    {
        using lex::TokenFlags;

        const auto where            = m_sources.file_location( token.location );
        const auto [ file, offset ] = m_sources.decompose( where );
        const auto [ line, column ] = m_sources.line_column( where );

        if ( !m_started || token.has( TokenFlags::at_line_start ) )
        {
            move_to( file, line );
            indent( file, line, column );
        }
        else if ( token.has( TokenFlags::leading_space ) )
            m_out += ' ';

        const auto text     = m_preprocessor.spelling( token );
        const auto spelling = token.has( TokenFlags::needs_cleaning ) ? lex::clean_spelling( text ) : std::string { text };

        if ( token.has( TokenFlags::no_expand ) ) m_painter.paint_into( m_out, spelling, painted_style );
        else if ( m_sources.is_expansion( token.location ) )
            m_painter.paint_into( m_out, spelling, expanded_style );
        else
            m_out += spelling;
    }

    std::string Printer::finish()
    {
        if ( m_started ) m_out += '\n';
        m_started = false;
        return std::exchange( m_out, {} );
    }

    void Printer::move_to( const support::FileId file, const std::uint32_t line )
    {
        if ( !m_started || file != m_file )
        {
            if ( m_started ) m_out += '\n';
            if ( m_options.line_markers ) marker( file, line );
            m_started = true;
            m_file    = file;
            m_line    = line;
            return;
        }

        if ( const auto gap = line > m_line ? line - m_line : 1u; m_options.line_markers && gap > 8 )
        {
            m_out += '\n';
            marker( file, line );
        }
        else
            m_out.append( m_options.line_markers ? gap : std::min( gap, 2u ), '\n' );

        m_line = line;
    }

    void Printer::marker( const support::FileId file, const std::uint32_t line )
    {
        m_painter.paint_into( m_out, std::format( "# {} {:?}", line, m_sources.path( file ) ), marker_style );
        m_out += '\n';
    }

    void Printer::indent( const support::FileId file, const std::uint32_t line, const std::uint32_t column )
    {
        const auto text   = m_sources.line_text( file, line ).substr( 0, column - 1 );
        const auto blank  = text.find_first_not_of( " \t" );
        m_out            += text.substr( 0, blank );
    }
} // namespace taihu::pp
