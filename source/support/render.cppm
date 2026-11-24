export module taihu.support:render;
import std;
import :source;
import :enums;
import :style;
import :diagnostics;

namespace taihu::support
{
    constexpr std::uint32_t tab_width = 4;

    std::uint32_t display_column( const std::string_view line, const std::uint32_t byte )
    {
        std::uint32_t column {};
        for ( std::uint32_t i {}; i < byte && i < line.size(); ++i )
        {
            const auto c = static_cast< unsigned char >( line[ i ] );
            if ( c == '\t' ) column += tab_width - column % tab_width;
            else if ( ( c & 0xC0 ) != 0x80 )
                ++column;
        }
        return column;
    }

    std::uint32_t display_width( const std::string_view line )
    { return display_column( line, static_cast< std::uint32_t >( line.size() ) ); }

    std::string expand_tabs( const std::string_view line )
    {
        std::string   out;
        std::uint32_t column {};
        for ( const auto ch : line )
        {
            if ( ch == '\t' )
            {
                const auto spaces = tab_width - column % tab_width;
                out.append( spaces, ' ' );
                column += spaces;
                continue;
            }
            out += ch;
            if ( ( static_cast< unsigned char >( ch ) & 0xC0 ) != 0x80 ) ++column;
        }
        return out;
    }

    std::uint32_t digits( std::uint32_t n )
    {
        std::uint32_t count { 1 };
        while ( n >= 10 )
        {
            n /= 10;
            ++count;
        }
        return count;
    }

    struct Mark
    {
        FileId           file {};
        std::uint32_t    line {};
        std::uint32_t    column {};
        std::uint32_t    start {};
        std::uint32_t    end_line {};
        std::uint32_t    end {};
        std::string_view text;
        bool             primary {};

        [[nodiscard]] bool multiline() const noexcept
        { return end_line > line; }
    };

    struct LineEdit
    {
        std::uint32_t    begin {};
        std::uint32_t    end {};
        std::string_view replacement;
    };
} // namespace taihu::support

export namespace taihu::support
{
    class DiagnosticRenderer
    {
    public:
        DiagnosticRenderer( const SourceManager& sources, const Theme& theme, const bool color ) :
            m_sources { sources },
            m_theme { theme },
            m_painter { color }
        {
        }

        [[nodiscard]] std::string render( const Diagnostic& diagnostic ) const
        {
            const auto& info     = diagnostic_catalog[ static_cast< std::size_t >( diagnostic.id ) ];
            const auto& severity = severity_style( diagnostic.severity );
            const auto  marks    = resolve( diagnostic );
            const auto* primary  = find_primary( marks );

            std::uint32_t max_line = 1;
            for ( const auto& mark : marks ) max_line = std::max( max_line, mark.end_line );
            for ( const auto& suggestion : diagnostic.suggestions )
                for ( const auto& edit : suggestion.edits )
                    if ( edit.range.begin.valid() ) max_line = std::max( max_line, m_sources.line_column( edit.range.begin ).line );
            const auto width = digits( max_line );
            const auto pad   = std::string( width, ' ' );

            std::string out;
            paint_into( out, enum_name( diagnostic.severity ), severity );
            paint_into( out, ": ", m_theme.message );
            paint_into( out, diagnostic.message, m_theme.message );
            if ( !info.code.empty() ) out += "  " + paint( std::format( "[{}]", info.code ), m_theme.code );
            out += '\n';

            if ( primary )
                out += std::format( "{}{} {}:{}:{}\n", pad, paint( "-->", m_theme.gutter ), m_sources.path( primary->file ), primary->line,
                                    primary->column );

            if ( !marks.empty() )
            {
                std::vector< FileId > files;
                if ( primary ) files.push_back( primary->file );
                for ( const auto& mark : marks )
                    if ( std::ranges::find( files, mark.file ) == files.end() ) files.push_back( mark.file );

                for ( const auto file : files )
                {
                    if ( !primary || file != primary->file )
                    {
                        const auto& first = *std::ranges::find( marks, file, &Mark::file );
                        out += std::format( "{}{} {}:{}:{}\n", pad, paint( ":::", m_theme.gutter ), m_sources.path( file ), first.line,
                                            first.column );
                    }
                    render_file( out, marks, file, width, severity );
                }
            }

            const auto footer = [ & ]( const std::string_view kind, const Style& style, const std::string_view text )
            {
                out += std::format( "{} {} {}: {}\n", pad, paint( "=", m_theme.gutter ), paint( kind, style ), text );
            };

            for ( const auto& help : diagnostic.help ) footer( "help", m_theme.help, help );
            for ( const auto& note : diagnostic.notes ) footer( "note", m_theme.note, note );
            if ( !info.spec.empty() ) footer( "spec", m_theme.spec, paint( info.spec, m_theme.spec ) );

            for ( const auto& suggestion : diagnostic.suggestions ) render_suggestion( out, suggestion, width );

            return out;
        }

        [[nodiscard]] std::string render_all( const DiagnosticEngine& engine ) const
        {
            std::string out;
            for ( const auto& diagnostic : engine.all() )
            {
                out += render( diagnostic );
                out += '\n';
            }

            std::size_t warnings {};
            for ( const auto& diagnostic : engine.all() )
                if ( diagnostic.severity == Severity::warning ) ++warnings;

            const auto errors = engine.error_count();
            if ( errors == 0 && warnings == 0 ) return out;

            std::vector< std::string > parts;
            if ( errors ) parts.push_back( paint( std::format( "{} error{}", errors, errors == 1 ? "" : "s" ), m_theme.error ) );
            if ( warnings ) parts.push_back( paint( std::format( "{} warning{}", warnings, warnings == 1 ? "" : "s" ), m_theme.warning ) );

            out += parts.size() == 2 ? std::format( "{} and {} generated\n", parts[ 0 ], parts[ 1 ] )
                                     : std::format( "{} generated\n", parts[ 0 ] );
            return out;
        }

    private:
        const SourceManager& m_sources;
        const Theme&         m_theme;
        Painter              m_painter;

        [[nodiscard]] std::string paint( const std::string_view text, const Style& style ) const
        { return m_painter.paint( text, style ); }

        void paint_into( std::string& out, const std::string_view text, const Style& style ) const
        { m_painter.paint_into( out, text, style ); }

        [[nodiscard]] const Style& severity_style( const Severity severity ) const noexcept
        {
            switch ( severity )
            {
                case Severity::note: return m_theme.note;
                case Severity::warning: return m_theme.warning;
                case Severity::error:
                case Severity::fatal: return m_theme.error;
            }
            return m_theme.error;
        }

        [[nodiscard]] static const Mark* find_primary( const std::vector< Mark >& marks ) noexcept
        {
            for ( const auto& mark : marks )
                if ( mark.primary ) return &mark;
            return marks.empty() ? nullptr : &marks.front();
        }

        [[nodiscard]] std::string gutter( const std::uint32_t width ) const
        { return paint( std::format( "{:>{}} |", "", width ), m_theme.gutter ); }

        [[nodiscard]] std::string numbered_gutter( const std::uint32_t width, const std::uint32_t line ) const
        { return paint( std::format( "{:>{}} |", line, width ), m_theme.gutter ); }

        [[nodiscard]] std::vector< Mark > resolve( const Diagnostic& diagnostic ) const
        {
            std::vector< Mark > marks;
            for ( const auto& label : diagnostic.labels )
            {
                if ( !label.range.begin.valid() ) continue;

                const auto file             = m_sources.decompose( label.range.begin ).file;
                const auto [ line, column ] = m_sources.line_column( label.range.begin );
                const auto text             = m_sources.line_text( file, line );

                Mark mark { file, line, column, display_column( text, column - 1 ), line, 0, label.text, label.primary };
                mark.end = mark.start + 1;

                if ( label.range.end.valid() && label.range.end > label.range.begin )
                {
                    const auto end_file = m_sources.decompose( label.range.end ).file;
                    auto       end_pos  = m_sources.line_column( label.range.end );

                    if ( end_file != file ) mark.end = display_width( text );
                    else if ( end_pos.line == line )
                        mark.end = display_column( text, end_pos.column - 1 );
                    else if ( end_pos.column == 1 && end_pos.line - 1 == line )
                        mark.end = display_width( text );
                    else
                    {
                        mark.end_line = end_pos.line;
                        mark.end      = display_column( m_sources.line_text( file, end_pos.line ), end_pos.column - 1 );
                        if ( mark.end == 0 )
                        {
                            --mark.end_line;
                            mark.end = display_width( m_sources.line_text( file, mark.end_line ) );
                        }
                    }
                }

                if ( !mark.multiline() ) mark.end = std::max( mark.end, mark.start + 1 );
                marks.push_back( mark );
            }
            return marks;
        }

        void render_file( std::string& out, const std::vector< Mark >& marks, const FileId file, const std::uint32_t width,
                          const Style& severity ) const
        {
            const auto mark_style = [ & ]( const Mark& mark ) -> const Style&
            {
                return mark.primary ? severity : m_theme.secondary;
            };

            const Mark* span = nullptr;
            for ( const auto& mark : marks )
                if ( mark.file == file && mark.multiline() && !span ) span = &mark;

            std::vector< std::uint32_t > lines;
            for ( const auto& mark : marks )
            {
                if ( mark.file != file ) continue;
                lines.push_back( mark.line );
                if ( &mark != span ) continue;
                lines.push_back( mark.end_line );
                if ( mark.end_line - mark.line <= 4 )
                    for ( auto l = mark.line + 1; l < mark.end_line; ++l ) lines.push_back( l );
                else
                {
                    lines.push_back( mark.line + 1 );
                    lines.push_back( mark.end_line - 1 );
                }
            }
            std::ranges::sort( lines );
            lines.erase( std::ranges::unique( lines ).begin(), lines.end() );

            const auto margin = [ & ]( const std::uint32_t line ) -> std::string
            {
                if ( !span ) return "";
                if ( line > span->line && line <= span->end_line ) return paint( "|", mark_style( *span ) ) + " ";
                return "  ";
            };

            const auto source_line = [ & ]( const std::uint32_t line )
            {
                out += std::format( "{} {}{}\n", numbered_gutter( width, line ), margin( line ),
                                    expand_tabs( m_sources.line_text( file, line ) ) );
            };

            out += gutter( width ) + '\n';

            std::uint32_t previous {};
            for ( const auto line : lines )
            {
                if ( previous != 0 && line == previous + 2 ) source_line( previous + 1 );
                else if ( previous != 0 && line > previous + 2 )
                    out += std::format( "{}{}\n", paint( std::format( "{:>{}}", "...", width + 2 ), m_theme.gutter ),
                                        span && line > span->line && line <= span->end_line ? " " + margin( line ) : "" );
                previous = line;

                source_line( line );

                std::vector< const Mark* > on_line;
                for ( const auto& mark : marks )
                    if ( mark.file == file && mark.line == line && &mark != span ) on_line.push_back( &mark );

                if ( !on_line.empty() ) render_marks( out, on_line, width, margin( line ), mark_style );

                if ( span && line == span->line )
                    out += std::format( "{}  {}\n", gutter( width ),
                                        paint( std::string( span->start + 1, '_' ) + "^", mark_style( *span ) ) );

                if ( span && line == span->end_line )
                {
                    const auto caret  = std::max< std::uint32_t >( span->end, 1 ) - 1;
                    out              += std::format( "{} {}{}", gutter( width ), paint( "|", mark_style( *span ) ),
                                                     paint( std::string( caret + 1, '_' ) + "^", mark_style( *span ) ) );
                    if ( !span->text.empty() )
                    {
                        out += ' ';
                        paint_into( out, span->text, mark_style( *span ) );
                    }
                    out += '\n';
                }
            }

            out += gutter( width ) + '\n';
        }

        template < typename StyleOf >
        void render_marks( std::string& out, std::vector< const Mark* > on_line, const std::uint32_t width, const std::string_view margin,
                           const StyleOf& mark_style ) const
        {
            std::ranges::sort( on_line, {}, &Mark::start );

            std::uint32_t right {};
            for ( const auto* mark : on_line ) right = std::max( right, mark->end );

            std::vector< char >        chars( right, ' ' );
            std::vector< const Mark* > owner( right, nullptr );
            for ( const bool primary_pass : { false, true } )
                for ( const auto* mark : on_line )
                    if ( mark->primary == primary_pass )
                        for ( auto c = mark->start; c < mark->end; ++c )
                        {
                            chars[ c ] = mark->primary ? '^' : '-';
                            owner[ c ] = mark;
                        }

            std::string underline;
            for ( std::uint32_t c {}; c < right; )
            {
                auto run = c;
                while ( run < right && chars[ run ] == chars[ c ] && owner[ run ] == owner[ c ] ) ++run;
                const std::string piece( run - c, chars[ c ] );
                underline += owner[ c ] ? paint( piece, mark_style( *owner[ c ] ) ) : piece;
                c          = run;
            }

            std::vector< const Mark* > labelled;
            for ( const auto* mark : on_line )
                if ( !mark->text.empty() ) labelled.push_back( mark );
            std::ranges::sort( labelled, []( const Mark* a, const Mark* b )
                               { return a->start != b->start ? a->start > b->start : a->end > b->end; } );

            const Mark* inline_mark = nullptr;
            if ( !labelled.empty() && labelled.front()->end == right ) inline_mark = labelled.front();

            out += std::format( "{} {}{}", gutter( width ), margin, underline );
            if ( inline_mark )
            {
                out += ' ';
                paint_into( out, inline_mark->text, mark_style( *inline_mark ) );
            }
            out += '\n';

            std::vector< const Mark* > hanging;
            for ( const auto* mark : labelled )
                if ( mark != inline_mark ) hanging.push_back( mark );
            if ( hanging.empty() ) return;

            const auto row = [ & ]( const std::size_t from, const Mark* text_mark )
            {
                std::string   cells;
                std::uint32_t column {};
                for ( auto i = hanging.size(); i-- > from; )
                {
                    const auto* mark = hanging[ i ];
                    if ( mark->start < column ) continue;
                    cells.append( mark->start - column, ' ' );
                    cells  += paint( "|", mark_style( *mark ) );
                    column  = mark->start + 1;
                }
                if ( text_mark )
                {
                    cells.append( text_mark->start - std::min( column, text_mark->start ), ' ' );
                    cells += paint( text_mark->text, mark_style( *text_mark ) );
                }
                return std::format( "{} {}{}\n", gutter( width ), margin, cells );
            };

            out += row( 0, nullptr );
            for ( std::size_t i {}; i < hanging.size(); ++i ) out += row( i + 1, hanging[ i ] );
        }

        void render_suggestion( std::string& out, const Suggestion& suggestion, const std::uint32_t width ) const
        {
            out += std::format( "{}: {}\n", paint( "help", m_theme.help ), suggestion.message );

            std::map< std::pair< std::uint32_t, std::uint32_t >, std::vector< LineEdit > > by_line;
            for ( const auto& edit : suggestion.edits )
            {
                if ( !edit.range.begin.valid() ) continue;
                const auto end_loc = edit.range.end.valid() ? edit.range.end : edit.range.begin;
                const auto file    = m_sources.decompose( edit.range.begin ).file;
                const auto begin   = m_sources.line_column( edit.range.begin );
                const auto end     = m_sources.line_column( end_loc );
                if ( end.line != begin.line || m_sources.decompose( end_loc ).file != file ) continue;
                by_line[ { file.index, begin.line } ].push_back(
                    { begin.column - 1, std::max( end.column, begin.column ) - 1, edit.replacement } );
            }
            if ( by_line.empty() ) return;

            out += gutter( width ) + '\n';
            for ( auto& [ key, edits ] : by_line )
            {
                const auto [ file_index, line ] = key;
                const auto original             = m_sources.line_text( FileId { file_index }, line );
                std::ranges::sort( edits, {}, &LineEdit::begin );

                struct Piece
                {
                    std::uint32_t begin;
                    std::uint32_t end;
                    char          marker;
                };

                std::string          edited;
                std::vector< Piece > pieces;
                std::uint32_t        position {};
                for ( const auto& edit : edits )
                {
                    if ( edit.begin < position ) continue;
                    edited        += original.substr( position, edit.begin - position );
                    const auto at  = static_cast< std::uint32_t >( edited.size() );
                    if ( edit.replacement.empty() )
                    {
                        edited += original.substr( edit.begin, edit.end - edit.begin );
                        pieces.push_back( { at, static_cast< std::uint32_t >( edited.size() ), '-' } );
                    }
                    else
                    {
                        edited += edit.replacement;
                        pieces.push_back( { at, static_cast< std::uint32_t >( edited.size() ), edit.end > edit.begin ? '~' : '+' } );
                    }
                    position = edit.end;
                }
                edited += original.substr( position );

                std::string   markers;
                std::uint32_t column {};
                for ( const auto& piece : pieces )
                {
                    const auto from = display_column( edited, piece.begin );
                    const auto to   = std::max( display_column( edited, piece.end ), from + 1 );
                    markers.append( from - std::min( column, from ), ' ' );
                    markers += paint( std::string( to - from, piece.marker ), piece.marker == '-' ? m_theme.error : m_theme.help );
                    column   = to;
                }

                out += std::format( "{} {}\n", numbered_gutter( width, line ), expand_tabs( edited ) );
                out += std::format( "{} {}\n", gutter( width ), markers );
            }
        }
    };
} // namespace taihu::support
