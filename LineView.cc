////////////////////////////////////////////////////////////////////////////////
// VI-Simplified (vis) C++ Implementation                                     //
// Copyright (c) 07 Sep 2015 Paul J. Gartside                                 //
////////////////////////////////////////////////////////////////////////////////
// Permission is hereby granted, free of charge, to any person obtaining a    //
// copy of this software and associated documentation files (the "Software"), //
// to deal in the Software without restriction, including without  limitation //
// the rights to use, copy, modify, merge, publish, distribute, sublicense,   //
// and/or sell copies of the Software, and to permit persons to whom the      //
// Software is furnished to do so, subject to the following conditions:       //
//                                                                            //
// The above copyright notice and this permission notice shall be included in //
// all copies or substantial portions of the Software.                        //
//                                                                            //
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR //
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   //
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    //
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER //
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    //
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        //
// DEALINGS IN THE SOFTWARE.                                                  //
////////////////////////////////////////////////////////////////////////////////

#include <ctype.h>     // is(alnum|punct|space|print|lower...)
#include <string.h>
#include <unistd.h>    // write, ioctl[unix], read, chdir

#include "String.hh"
#include "Console.hh"
#include "MemLog.hh"
#include "FileBuf.hh"
#include "Utilities.hh"
#include "Cover_Array.hh"
#include "Key.hh"
#include "Vis.hh"
#include "View.hh"
#include "LineView.hh"

extern MemLog<MEM_LOG_BUF_SIZE> Log;

extern const uint16_t MAX_COLS;    // Arbitrary maximum char width of window
extern const unsigned BE_FILE   ;  // Buffer editor file
extern const unsigned SHELL_FILE;  // Command Shell file
extern const unsigned USER_FILE;   // First user file

static const unsigned top___border = 1;
static const unsigned bottomborder = 1;
static const unsigned left__border = 1;
static const unsigned right_border = 1;
static const unsigned NUM_ROWS     = 1;
static const unsigned WORKING_ROWS = 1;

struct ColonOp
{
  enum E
  {
    unknown,
    e,
    w
  };
};

struct LineView::Data
{
  LineView&   view;
  Vis&        vis;
  Key&        key;
  FileBuf&    fb; // reference to COLON_FILE
  LinesList&  reg;
  String      cover_key;
  Line        cover_buf;
  char*       cbuf;
  String&     sbuf;
  const char  banner_delim;

  unsigned nCols;     // number of rows in buffer view
  unsigned x;         // Top left x-position of buffer view in parent window
  unsigned y;         // Top left y-position of buffer view in parent window
  const
  unsigned prefix_len;
  unsigned topLine;   // top  of buffer view line number.
  unsigned leftChar;  // left of buffer view character number.
  unsigned crsCol;    // cursor column in buffer view. 0 <= m.crsCol < WorkingCols().

  unsigned v_st_line; // Visual start line number
  unsigned v_st_char; // Visual start char number on line
  unsigned v_fn_line; // Visual ending line number
  unsigned v_fn_char; // Visual ending char number on line

  bool inInsertMode;  // true if in insert  mode, else false
  bool inReplaceMode;
  bool inVisualMode;

  // Tab file name completion variables:
  View*      cv;
  FileBuf*   pfb;
  unsigned   file_index;
  ColonOp::E colon_op;
  String     partial_path;
  String     search__head;

  Data( LineView&   view
      , Vis&        vis
      , Key&        key
      , FileBuf&    fb
      , LinesList&  reg
      , char*       cbuf
      , String&     sbuf
      , const char  banner_delim );
  ~Data();
};

LineView::Data::Data( LineView&   view
                    , Vis&        vis
                    , Key&        key
                    , FileBuf&    fb
                    , LinesList&  reg
                    , char*       cbuf
                    , String&     sbuf
                    , const char  banner_delim )
  : view( view )
  , vis( vis )
  , key( key )
  , fb( fb )
  , reg( reg )
  , cover_key()
  , cover_buf()
  , cbuf( cbuf )
  , sbuf( sbuf )
  , banner_delim( banner_delim )
  , nCols( Console::Num_Cols() )
  , x( 0 )
  , y( 0 )
  , prefix_len( 2 )
  , topLine( 0 )
  , leftChar( 0 )
  , crsCol( 0 )
  , v_st_line( 0 )
  , v_st_char( 0 )
  , v_fn_line( 0 )
  , v_fn_char( 0 )
  , inInsertMode( false )
  , inReplaceMode( false )
  , inVisualMode( false )
  , cv( 0 )
  , pfb( 0 )
  , file_index( 0 )
  , colon_op( ColonOp::unknown )
  , partial_path()
  , search__head()
{
}

LineView::Data::~Data()
{
}

Style Get_Style( LineView::Data& m
               , const unsigned line
               , const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Style s = S_NORMAL;

  if( m.view.InVisualArea( line, pos ) )
  {
    s = S_RV_NORMAL;

    if     ( m.view.InStar    ( line, pos ) ) s = S_RV_STAR;
    else if( m.view.InDefine  ( line, pos ) ) s = S_RV_DEFINE;
    else if( m.view.InComment ( line, pos ) ) s = S_RV_COMMENT;
    else if( m.view.InConst   ( line, pos ) ) s = S_RV_CONST;
    else if( m.view.InControl ( line, pos ) ) s = S_RV_CONTROL;
    else if( m.view.InVarType ( line, pos ) ) s = S_RV_VARTYPE;
    else if( m.view.InNonAscii( line, pos ) ) s = S_RV_NONASCII;
  }
  else if( m.view.InStar    ( line, pos ) ) s = S_STAR;
  else if( m.view.InDefine  ( line, pos ) ) s = S_DEFINE;
  else if( m.view.InComment ( line, pos ) ) s = S_COMMENT;
  else if( m.view.InConst   ( line, pos ) ) s = S_CONST;
  else if( m.view.InControl ( line, pos ) ) s = S_CONTROL;
  else if( m.view.InVarType ( line, pos ) ) s = S_VARTYPE;
  else if( m.view.InNonAscii( line, pos ) ) s = S_NONASCII;

  return s;
}

void InsertAddChar( LineView::Data& m, const char C )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.fb.NumLines()==0 ) m.fb.PushLine();

  m.fb.InsertChar( m.view.CrsLine(), m.view.CrsChar(), C );

  if( m.view.WorkingCols() <= m.crsCol+1 )
  {
    // On last working column, need to scroll right:
    m.leftChar++;
  }
  else {
    m.crsCol += 1;
  }
  m.fb.UpdateCmd();
}

void InsertBackspace( LineView::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // If no lines in buffer, no backspacing to be done
  if( 0 < m.fb.NumLines() )
  {
    const unsigned CL = m.view.CrsLine(); // Cursor line
    const unsigned CP = m.view.CrsChar(); // Cursor position

    if( 0<CP )
    {
      m.fb.RemoveChar( CL, CP-1 );

      if( 0 < m.crsCol ) m.crsCol -= 1;
      else               m.leftChar -= 1;

      m.fb.UpdateCmd();
    }
  }
}

bool Do_i_normal( LineView::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  unsigned count = 0;
  for( char C=m.key.In(); C != ESC; C=m.key.In() )
  {
    if( IsEndOfLineDelim( C ) )
    {
      m.view.HandleReturn();
      return true;
    }
    else if( BS == C || DEL == C )
    {
      if( 0<count ) {
        InsertBackspace(m);
        count--;
      }
    }
    else {
      InsertAddChar( m, C );
      count++;
    }
  }
  return false;
}

void Reset_File_Name_Completion_Variables( LineView::Data& m )
{
  m.cv          = m.vis.CV();
  m.pfb         = 0;
  m.file_index  = 0;
  m.colon_op    = ColonOp::unknown;
  m.partial_path.clear();
  m.search__head.clear();
}

// in_out_fname goes in as         some/path/partial_file_name
// and if successful, comes out as some/path
// the relative path to the files
bool FindFileBuf( LineView::Data& m )
{
  // 1. seperate fname, in m.sbuf, into f_name_tail and f_name_head
  String f_name_head;
  String f_name_tail;
  GetFnameHeadAndTail( m.sbuf.c_str(), f_name_head, f_name_tail );

#ifndef WIN32
  if( f_name_tail == "~" ) f_name_tail = "$HOME";
#endif
  String f_full_path = f_name_tail;
  if( 0==f_full_path.len() ) f_full_path.push('.');

  if( FindFullFileNameRel2( m.cv->GetPathName(), f_full_path ) )
  {
    m.partial_path = f_name_tail;
    m.search__head = f_name_head;
    // f_full_path is now the full path to the directory
    // to search for matches to f_name_head
    unsigned file_index = 0;
    if( m.vis.HaveFile( f_full_path.c_str(), &file_index ) )
    {
      m.pfb = m.vis.GetFileBuf( file_index );
    }
    else {
      // This is not a memory leak.
      // m.pfb gets added to m.vis.m.files in Vis::Add_FileBuf_2_Lists_Create_Views()
      m.pfb = new(__FILE__,__LINE__)
              FileBuf( m.vis, f_full_path.c_str(), true, FT_UNKNOWN );
      m.pfb->ReadFile();
    }
    return true;
  }
  return false;
}

// Returns true if found tab filename, else false
bool Find_File_Name_Completion_Variables( LineView::Data& m )
{
  bool found_tab_fname = false;

  Line* lp = m.fb.GetLineP( m.view.CrsLine() );
  m.sbuf.clear();
  // Cant copy lp->c_str() to m.sbuf here because lp is not null terminated:
  for( unsigned i=0; i<lp->len(); i++ ) m.sbuf.push( lp->get(i) );
  m.sbuf.trim(); // Remove leading and trailing white space

  if     ( m.sbuf.has_at("e ",0) || m.sbuf=="e" ) m.colon_op = ColonOp::e;
  else if( m.sbuf.has_at("w ",0) || m.sbuf=="w" ) m.colon_op = ColonOp::w;

  if( ColonOp::e == m.colon_op
   || ColonOp::w == m.colon_op )
  {
    m.sbuf.shift(1); m.sbuf.trim_beg(); // Remove initial 'e' and space after 'e'
    if( FindFileBuf(m) )
    {
      // Have FileBuf, so add matching files names to tab_fnames
      for( unsigned k=0; !found_tab_fname && k<m.pfb->NumLines(); k++ )
      {
        Line l = m.pfb->GetLine( k );
        const char* fname = l.c_str( 0 );

        if( fname && 0==strncmp( fname, m.search__head.c_str(), m.search__head.len() ) )
        {
          found_tab_fname = true;
          m.file_index = k;
          m.sbuf = m.partial_path;
          if( 0<m.sbuf.len() )
          {
            m.sbuf.push('/'); // Dont append '/' if no m.partial_path
          }
          // Cant copy l.c_str() to m.sbuf here because l is not null terminated:
          for( unsigned i=0; i<l.len(); i++ ) m.sbuf.push( l.get(i) );
        }
      }
    }
    if( found_tab_fname )
    {
      // Removed "e" above, so add it back here:
      if( ColonOp::e == m.colon_op ) m.sbuf.insert( 0, "e ");
      else                           m.sbuf.insert( 0, "w ");

      lp = m.fb.RemoveLineP( m.view.CrsLine() );
      lp->clear();
      for( unsigned k=0; k<m.sbuf.len(); k++ )
      {
        lp->push(__FILE__, __LINE__, m.sbuf.get(k) );
      }
      m.fb.InsertLine( m.view.CrsLine(), lp );

      m.view.GoToCrsPos_NoWrite( m.view.CrsLine(), lp->len() );
    }
  }
  return found_tab_fname;
}

// Returns true if found tab filename, else false
bool Have_File_Name_Completion_Variables( LineView::Data& m )
{
  bool found_tab_fname = false;

  // Already have a FileBuf, just search for next matching filename:
  for( unsigned k=m.file_index+1
     ; !found_tab_fname && k<m.file_index+m.pfb->NumLines(); k++ )
  {
    Line l = m.pfb->GetLine( k % m.pfb->NumLines() );
    const char* fname = l.c_str( 0 );

    if( 0==strncmp( fname, m.search__head.c_str(), m.search__head.len() ) )
    {
      found_tab_fname = true;
      m.file_index = k;
      m.sbuf = m.partial_path;
      if( 0<m.sbuf.len() )
      {
        m.sbuf.push('/'); // Done append '/' if no m.partial_path
      }
      // Cant use m.sbuf.append here because Line is not NULL terminated:
      for( unsigned i=0; i<l.len(); i++ ) m.sbuf.push( l.get(i) );

      if( ColonOp::e == m.colon_op ) m.sbuf.insert( 0, "e ");
      else                           m.sbuf.insert( 0, "w ");

      Line* lp = m.fb.RemoveLineP( m.view.CrsLine() );
      lp->clear();
      for( unsigned k=0; k<m.sbuf.len(); k++ )
      {
        lp->push(__FILE__, __LINE__, m.sbuf.get(k) );
      }
      m.fb.InsertLine( m.view.CrsLine(), lp );

      m.view.GoToCrsPos_NoWrite( m.view.CrsLine(), lp->len() );
    }
  }
  return found_tab_fname;
}

void Do_i_tabs_HandleTab( LineView::Data& m, unsigned& count )
{
  Trace trace( __PRETTY_FUNCTION__ );

  bool found_tab_fname = false;

  if( 0 == m.pfb )
  {
    found_tab_fname = Find_File_Name_Completion_Variables(m);
  }
  else { // Put list of file names in tab_fnames:
    found_tab_fname = Have_File_Name_Completion_Variables(m);
  }
  if( found_tab_fname )
  {
    m.fb.UpdateCmd();

    count = m.fb.LineLen( m.view.CrsLine() );
  }
  else {
    // If we fall through, just treat tab like a space:
    InsertAddChar( m, ' ' );
  }
}

bool Do_i_tabs( LineView::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Reset_File_Name_Completion_Variables(m);

  unsigned count = 0;
  for( uint8_t C=m.key.In(); C != ESC; C=m.key.In() )
  {
    if( IsEndOfLineDelim( C ) )
    {
      m.view.HandleReturn();
      return true;
    }
    else if( '\t' == C && 0<count )
    {
      Do_i_tabs_HandleTab( m, count );
    }
    else {
      Reset_File_Name_Completion_Variables(m);

      if( BS == C || DEL == C )
      {
        if( 0<count ) {
          InsertBackspace(m);
          count--;
        }
      }
      else {
        InsertAddChar( m, C );
        count++;
      }
    }
  }
  return false;
}

void DisplayBanner( LineView::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned G_COL = m.x + left__border;

  if( m.inInsertMode )
  {
    Console::Set( m.y, G_COL, 'I', S_BANNER );
  }
  else if( m.inReplaceMode )
  {
    Console::Set( m.y, G_COL, 'R', S_CONST );
  }
  else if( m.inVisualMode )
  {
    Console::Set( m.y, G_COL, 'V', S_DEFINE );
  }
  else {
    Console::Set( m.y, G_COL, 'E', S_CONTROL );
  }
  Console::Set( m.y, G_COL+1, m.banner_delim, S_NORMAL );
  Console::Update();
  Console::Flush();
}

void DisplayBanner_PrintCursor( LineView::Data& m )
{
  DisplayBanner( m );

//m.view.GoToCrsPos_Write( m.view.CrsLine(), m.view.CrsChar() );
  m.view.PrintCursor();
}

void Replace_Crs_Char( LineView::Data& m, Style style )
{
  const unsigned LL = m.fb.LineLen( m.view.CrsLine() ); // Line length

  if( LL )
  {
    int byte = m.fb.Get( m.view.CrsLine(), m.view.CrsChar() );

    Console::Set( m.y
                , m.view.Col_Win_2_GL( m.crsCol )
                , byte, style );
  }
}

void Undo_v( LineView::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.fb.UpdateCmd();
}

void Do_v_Handle_gf( LineView::Data& m )
{
  if( m.v_st_line == m.v_fn_line )
  {
    const unsigned v_st_char = m.v_st_char < m.v_fn_char
                             ? m.v_st_char : m.v_fn_char;
    const unsigned v_fn_char = m.v_st_char < m.v_fn_char
                             ? m.v_fn_char : m.v_st_char;
    String fname;

    for( unsigned P = v_st_char; P<=v_fn_char; P++ )
    {
      fname.push( m.fb.Get( m.v_st_line, P  ) );
    }
    bool went_to_file = m.vis.GoToBuffer_Fname( fname );

    if( went_to_file )
    {
      // If we made it to buffer indicated by fname, no need to Undo_v() or
      // Remove_Banner() because the whole view pane will be redrawn
      m.inVisualMode = false;
    }
  }
}

void Swap_Visual_St_Fn_If_Needed( LineView::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.v_fn_line < m.v_st_line
   || (m.v_fn_line == m.v_st_line && m.v_fn_char < m.v_st_char) )
  {
    // Visual mode went backwards over multiple lines, or
    // Visual mode went backwards over one line
    Swap( m.v_st_line, m.v_fn_line );
    Swap( m.v_st_char, m.v_fn_char );
  }
}

void Do_v_Handle_gp( LineView::Data& m )
{
  if( m.v_st_line == m.v_fn_line )
  {
    Swap_Visual_St_Fn_If_Needed(m);

    String pattern;

    for( unsigned P = m.v_st_char; P<=m.v_fn_char; P++ )
    {
      pattern.push( m.fb.Get( m.v_st_line, P ) );
    }
    m.vis.Handle_Slash_GotPattern( pattern, false );

    m.inVisualMode = false;
    Undo_v(m);
    DisplayBanner_PrintCursor(m);
  }
}

// Returns true if still in visual mode, else false
//
void Do_v_Handle_g( LineView::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const char CC2 = m.key.In();

  if     ( CC2 == 'g' ) m.view.GoToTopOfFile();
  else if( CC2 == '0' ) m.view.GoToStartOfRow();
  else if( CC2 == '$' ) m.view.GoToEndOfRow();
  else if( CC2 == 'f' ) Do_v_Handle_gf(m);
  else if( CC2 == 'p' ) Do_v_Handle_gp(m);
}

void Do_y_v_st_fn( LineView::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Swap_Visual_St_Fn_If_Needed( m );

  for( unsigned L=m.v_st_line; L<=m.v_fn_line; L++ )
  {
    Line* nlp = m.vis.BorrowLine( __FILE__,__LINE__ );

    const unsigned LL = m.fb.LineLen( L );
    if( LL ) {
      const unsigned P_st = (L==m.v_st_line) ? m.v_st_char : 0;
      const unsigned P_fn = (L==m.v_fn_line) ? Min(LL-1,m.v_fn_char) : LL-1;

      for( unsigned P = P_st; P <= P_fn; P++ )
      {
        nlp->push(__FILE__,__LINE__, m.fb.Get( L, P ) );
      }
    }
    // m.reg will delete nlp
    m.reg.push( nlp );
  }
  m.vis.SetPasteMode( PM_ST_FN );
}

void Do_y_v( LineView::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.reg.clear();

  Do_y_v_st_fn(m);
}

void Do_Y_v_st_fn( LineView::Data& m )
{
  if( m.v_fn_line < m.v_st_line ) Swap( m.v_st_line, m.v_fn_line );

  for( unsigned L=m.v_st_line; L<=m.v_fn_line; L++ )
  {
    Line* nlp = m.vis.BorrowLine( __FILE__,__LINE__ );

    const unsigned LL = m.fb.LineLen(L);

    if( LL )
    {
      for( unsigned P = 0; P <= LL-1; P++ )
      {
        nlp->push(__FILE__,__LINE__, m.fb.Get( L, P ) );
      }
    }
    // m.reg will delete nlp
    m.reg.push( nlp );
  }
  m.vis.SetPasteMode( PM_LINE );
}

void Do_Y_v( LineView::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.reg.clear();

  Do_Y_v_st_fn(m);
}

void Do_x_range_pre( LineView::Data& m
                   , unsigned& st_line, unsigned& st_char
                   , unsigned& fn_line, unsigned& fn_char )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( fn_line < st_line
   || (fn_line == st_line && fn_char < st_char) )
  {
    Swap( st_line, fn_line );
    Swap( st_char, fn_char );
  }
  m.reg.clear();
}

void Do_x_range_post( LineView::Data& m
                    , const unsigned st_line
                    , const unsigned st_char )
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.vis.SetPasteMode( PM_ST_FN );

  // Try to put cursor at (st_line, st_char), but
  // make sure the cursor is in bounds after the deletion:
  const unsigned NUM_LINES = m.fb.NumLines();
  unsigned ncl = st_line;
  if( NUM_LINES <= ncl ) ncl = NUM_LINES-1;
  const unsigned NLL = m.fb.LineLen( ncl );
  unsigned ncc = 0;
  if( NLL ) ncc = NLL <= st_char ? NLL-1 : st_char;

  m.view.GoToCrsPos_NoWrite( ncl, ncc );

  m.inVisualMode = false;

  m.fb.UpdateCmd(); //<- No need to Undo_v() or Remove_Banner() because of this
}

void Do_x_range_single( LineView::Data& m
                      , const unsigned L
                      , const unsigned st_char
                      , const unsigned fn_char )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned OLL = m.fb.LineLen( L ); // Original line length

  if( 0<OLL )
  {
    Line* nlp = m.vis.BorrowLine( __FILE__,__LINE__ );

    const unsigned P_st = Min( st_char, OLL-1 );
    const unsigned P_fn = Min( fn_char, OLL-1 );

    unsigned LL = OLL;

    // Dont remove a single line, or else Q wont work right
    for( unsigned P = P_st; P_st < LL && P <= P_fn; P++ )
    {
      nlp->push(__FILE__,__LINE__, m.fb.RemoveChar( L, P_st ) );

      LL = m.fb.LineLen( L ); // Removed a char, so re-calculate LL
    }
    m.reg.push( nlp );
  }
}

void Do_x_range_multiple( LineView::Data& m
                        , const unsigned st_line
                        , const unsigned st_char
                        , const unsigned fn_line
                        , const unsigned fn_char )
{
  Trace trace( __PRETTY_FUNCTION__ );

  bool started_in_middle = false;
  bool ended___in_middle = false;

  unsigned n_fn_line = fn_line; // New finish line

  for( unsigned L = st_line; L<=n_fn_line; )
  {
    Line* nlp = m.vis.BorrowLine( __FILE__,__LINE__ );

    const unsigned OLL = m.fb.LineLen( L ); // Original line length

    const unsigned P_st = (L==  st_line) ? Min(st_char, OLL-1) : 0;
    const unsigned P_fn = (L==n_fn_line) ? Min(fn_char, OLL-1) : OLL-1;

    if(   st_line == L && 0    < P_st  ) started_in_middle = true;
    if( n_fn_line == L && P_fn < OLL-1 ) ended___in_middle = true;

    unsigned LL = OLL;

    for( unsigned P = P_st; P_st < LL && P <= P_fn; P++ )
    {
      nlp->push(__FILE__,__LINE__, m.fb.RemoveChar( L, P_st ) );

      LL = m.fb.LineLen( L ); // Removed a char, so re-calculate LL
    }
    if( 0 == P_st && OLL-1 == P_fn ) // Removed entire line
    {
      m.fb.RemoveLine( L );
      n_fn_line--;
    }
    else L++;

    m.reg.push( nlp );
  }
  if( started_in_middle && ended___in_middle )
  {
    Line* lp = m.fb.RemoveLineP( st_line+1 );
    m.fb.AppendLineToLine( st_line, lp );
  }
}

void Do_x_range( LineView::Data& m
               , unsigned st_line, unsigned st_char
               , unsigned fn_line, unsigned fn_char )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Do_x_range_pre( m, st_line, st_char, fn_line, fn_char );

  if( st_line == fn_line )
  {
    Do_x_range_single( m, st_line, st_char, fn_char );
  }
  else {
    Do_x_range_multiple( m, st_line, st_char, fn_line, fn_char );
  }
  Do_x_range_post( m, st_line, st_char );
}

void Do_x_v( LineView::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Do_x_range( m, m.v_st_line, m.v_st_char, m.v_fn_line, m.v_fn_char );

  DisplayBanner_PrintCursor(m);
}

void Do_D_v_line( LineView::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Swap_Visual_St_Fn_If_Needed(m);

  m.reg.clear();

  bool removed_line = false;
  // 1. If m.v_st_line==0, fn_line will go negative in the loop below,
  //    so use int's instead of unsigned's
  // 2. Dont remove all lines in file to avoid crashing
  int fn_line = m.v_fn_line;
  for( int L = m.v_st_line; 1 < m.fb.NumLines() && L<=fn_line; fn_line-- )
  {
    Line* lp = m.fb.RemoveLineP( L );
    m.reg.push( lp );

    // m.reg will delete nlp
    removed_line = true;
  }
  m.vis.SetPasteMode( PM_LINE );

  m.inVisualMode = false;
  DisplayBanner(m);
  // D'ed lines will be removed, so no need to Undo_v()

  if( removed_line )
  {
    // Figure out and move to new cursor position:
    const unsigned NUM_LINES = m.fb.NumLines();
    const unsigned OCL       = m.view.CrsLine(); // Old cursor line

    unsigned ncl = m.v_st_line;
    if( NUM_LINES-1 < ncl ) ncl = m.v_st_line ? m.v_st_line-1 : 0;

    const unsigned NCLL = m.fb.LineLen( ncl );
    unsigned ncc = 0;
    if( NCLL ) ncc = m.v_st_char < NCLL ? m.v_st_char : NCLL-1;

    m.view.GoToCrsPos_NoWrite( ncl, ncc );

    m.fb.UpdateCmd();
  }
}

void Do_D_v( LineView::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Do_D_v_line(m);
}

void InsertBackspace_vb( LineView::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned OCL = m.view.CrsLine();  // Old cursor line
  const unsigned OCP = m.view.CrsChar();  // Old cursor position

  if( 0<OCP )
  {
    const unsigned N_REG_LINES = m.reg.len();

    for( unsigned k=0; k<N_REG_LINES; k++ )
    {
      m.fb.RemoveChar( OCL+k, OCP-1 );
    }
    m.view.GoToCrsPos_NoWrite( OCL, OCP-1 );
  }
}

void InsertAddChar_vb( LineView::Data& m, const char c )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned OCL = m.view.CrsLine();  // Old cursor line
  const unsigned OCP = m.view.CrsChar();  // Old cursor position

  const unsigned N_REG_LINES = m.reg.len();

  for( unsigned k=0; k<N_REG_LINES; k++ )
  {
    const unsigned LL = m.fb.LineLen( OCL+k );

    if( LL < OCP )
    {
      // Fill in line with white space up to OCP:
      for( unsigned i=0; i<(OCP-LL); i++ )
      {
        // Insert at end of line so undo will be atomic:
        const unsigned NLL = m.fb.LineLen( OCL+k ); // New line length
        m.fb.InsertChar( OCL+k, NLL, ' ' );
      }
    }
    m.fb.InsertChar( OCL+k, OCP, c );
  }
  m.view.GoToCrsPos_NoWrite( OCL, OCP+1 );
}

bool Do_s_v_cursor_at_end_of_line( LineView::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned LL = m.fb.LineLen( m.view.CrsLine() );

  return 0<LL ? m.view.CrsChar() == LL-1 : false;
}

void Do_s_v( LineView::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Need to know if cursor is at end of line before Do_x_v() is called:
  const bool CURSOR_AT_END_OF_LINE = Do_s_v_cursor_at_end_of_line(m);

  Do_x_v(m);

  if( CURSOR_AT_END_OF_LINE ) m.view.Do_a();
  else                        m.view.Do_i();
}

void Do_Tilda_v_st_fn( LineView::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  for( unsigned L = m.v_st_line; L<=m.v_fn_line; L++ )
  {
    const unsigned LL = m.fb.LineLen( L );
    const unsigned P_st = (L==m.v_st_line) ? m.v_st_char : 0;
    const unsigned P_fn = (L==m.v_fn_line) ? m.v_fn_char : LL-1;

    for( unsigned P = P_st; P <= P_fn; P++ )
    {
      char c = m.fb.Get( L, P );
      bool changed = false;
      if     ( isupper( c ) ) { c = tolower( c ); changed = true; }
      else if( islower( c ) ) { c = toupper( c ); changed = true; }
      if( changed ) m.fb.Set( L, P, c );
    }
  }
}

void Do_Tilda_v( LineView::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Swap_Visual_St_Fn_If_Needed( m );

  Do_Tilda_v_st_fn(m);

  m.inVisualMode = false;
  DisplayBanner(m);
  Undo_v(m); //<- This will cause the tilda'ed characters to be redrawn
}

void ReplaceAddReturn( LineView::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );
  // The lines in fb do not end with '\n's.
  // When the file is written, '\n's are added to the ends of the lines.
  Line new_line(__FILE__, __LINE__);
  const unsigned OLL = m.fb.LineLen( m.view.CrsLine() );
  const unsigned OCP = m.view.CrsChar();

  for( unsigned k=OCP; k<OLL; k++ )
  {
    const uint8_t C = m.fb.RemoveChar( m.view.CrsLine(), OCP );
    bool ok = new_line.push(__FILE__,__LINE__, C );
    ASSERT( __LINE__, ok, "ok" );
  }
  // Truncate the rest of the old line:
  // Add the new line:
  const unsigned new_line_num = m.view.CrsLine()+1;
  m.fb.InsertLine( new_line_num, new_line );
  m.crsCol = 0;
  m.leftChar = 0;
  m.topLine++;

  m.fb.UpdateCmd();
}

void ReplaceAddChars( LineView::Data& m, const char C )
{
  Trace trace( __PRETTY_FUNCTION__ );
  if( m.fb.NumLines()==0 ) m.fb.PushLine();

  const unsigned CL = m.view.CrsLine();
  const unsigned CP = m.view.CrsChar();
  const unsigned LL = m.fb.LineLen( CL );
  const unsigned EOL = LL ? LL-1 : 0;

  if( EOL < CP )
  {
    // Extend line out to where cursor is:
    for( unsigned k=LL; k<CP; k++ ) m.fb.PushChar( CL, ' ' );
  }
  // Put char back in file buffer
  const bool continue_last_update = false;
  if( CP < LL ) m.fb.Set( CL, CP, C, continue_last_update );
  else {
    m.fb.PushChar( CL, C );
  }
  if( m.crsCol < m.view.WorkingCols()-1 )
  {
    m.crsCol++;
  }
  else {
    m.leftChar++;
  }
  m.fb.UpdateCmd();
}

// Returns true if found next word, else false
//
bool GoToNextWord_GetPosition( LineView::Data& m, CrsPos& ncp )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned NUM_LINES = m.fb.NumLines();
  if( 0==NUM_LINES ) return false;

  bool found_space = false;
  bool found_word  = false;
  const unsigned OCL = m.view.CrsLine(); // Old cursor line
  const unsigned OCP = m.view.CrsChar(); // Old cursor position

  IsWord_Func isWord = IsWord_Ident;

  // Find white space, and then find non-white space
  for( unsigned l=OCL; (!found_space || !found_word) && l<NUM_LINES; l++ )
  {
    const unsigned LL = m.fb.LineLen( l );
    if( LL==0 || OCL<l )
    {
      found_space = true;
      // Once we have encountered a space, word is anything non-space.
      // An empty line is considered to be a space.
      isWord = NotSpace;
    }
    const unsigned START_C = OCL==l ? OCP : 0;

    for( unsigned p=START_C; (!found_space || !found_word) && p<LL; p++ )
    {
      ncp.crsLine = l;
      ncp.crsChar = p;

      const int C = m.fb.Get( l, p );

      if( found_space  )
      {
        if( isWord( C ) ) found_word = true;
      }
      else {
        if( !isWord( C ) ) found_space = true;
      }
      // Once we have encountered a space, word is anything non-space
      if( IsSpace( C ) ) isWord = NotSpace;
    }
  }
  return found_space && found_word;
}

// Return true if new cursor position found, else false
bool GoToPrevWord_GetPosition( LineView::Data& m, CrsPos& ncp )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned NUM_LINES = m.fb.NumLines();
  if( 0==NUM_LINES ) return false;

  const int      OCL = m.view.CrsLine(); // Old cursor line
  const unsigned LL  = m.fb.LineLen( OCL );

  if( LL < m.view.CrsChar() ) // Since cursor is now allowed past EOL,
  {                           // it may need to be moved back:
    if( LL && !IsSpace( m.fb.Get( OCL, LL-1 ) ) )
    {
      // Backed up to non-white space, which is previous word, so return true
      ncp.crsLine = OCL;
      ncp.crsChar = LL-1;
      return true;
    }
    else {
      m.view.GoToCrsPos_NoWrite( OCL, LL ? LL-1 : 0 );
    }
  }
  bool found_space = false;
  bool found_word  = false;
  const unsigned OCP = m.view.CrsChar(); // Old cursor position

  IsWord_Func isWord = NotSpace;

  // Find word to non-word transition
  for( int l=OCL; (!found_space || !found_word) && -1<l; l-- )
  {
    const int LL = m.fb.LineLen( l );
    if( LL==0 || l<OCL )
    {
      // Once we have encountered a space, word is anything non-space.
      // An empty line is considered to be a space.
      isWord = NotSpace;
    }
    const unsigned START_C = OCL==l ? OCP-1 : LL-1;

    for( int p=START_C; (!found_space || !found_word) && -1<p; p-- )
    {
      ncp.crsLine = l;
      ncp.crsChar = p;

      const int C = m.fb.Get( l, p );

      if( found_word  )
      {
        if( !isWord( C ) || p==0 ) found_space = true;
      }
      else {
        if( isWord( C ) ) {
          found_word = true;
          if( 0==p ) found_space = true;
        }
      }
      // Once we have encountered a space, word is anything non-space
      if( IsIdent( C ) ) isWord = IsWord_Ident;
    }
    if( found_space && found_word )
    {
      if( ncp.crsChar && ncp.crsChar < LL-1 ) ncp.crsChar++;
    }
  }
  return found_space && found_word;
}

// Returns true if found end of word, else false
// 1. If at end of word, or end of non-word, move to next char
// 2. If on white space, skip past white space
// 3. If on word, go to end of word
// 4. If on non-white-non-word, go to end of non-white-non-word
bool GoToEndOfWord_GetPosition( LineView::Data& m, CrsPos& ncp )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned NUM_LINES = m.fb.NumLines();
  if( 0==NUM_LINES ) return false;

  const int      CL = m.view.CrsLine(); // Cursor line
  const unsigned LL = m.fb.LineLen( CL );
        unsigned CP = m.view.CrsChar(); // Cursor position

  // At end of line, or line too short:
  if( (LL-1) <= CP || LL < 2 ) return false;

  int CC = m.fb.Get( CL, CP );   // Current char
  int NC = m.fb.Get( CL, CP+1 ); // Next char

  // 1. If at end of word, or end of non-word, move to next char
  if( (IsWord_Ident   ( CC ) && !IsWord_Ident   ( NC ))
   || (IsWord_NonIdent( CC ) && !IsWord_NonIdent( NC )) ) CP++;

  // 2. If on white space, skip past white space
  if( IsSpace( m.fb.Get(CL, CP) ) )
  {
    for( ; CP<LL && IsSpace( m.fb.Get(CL, CP) ); CP++ ) ;
    if( LL <= CP ) return false; // Did not find non-white space
  }
  // At this point (CL,CP) should be non-white space
  CC = m.fb.Get( CL, CP );  // Current char

  ncp.crsLine = CL;

  if( IsWord_Ident( CC ) ) // On identity
  {
    // 3. If on word space, go to end of word space
    for( ; CP<LL && IsWord_Ident( m.fb.Get(CL, CP) ); CP++ )
    {
      ncp.crsChar = CP;
    }
  }
  else if( IsWord_NonIdent( CC ) )// On Non-identity, non-white space
  {
    // 4. If on non-white-non-word, go to end of non-white-non-word
    for( ; CP<LL && IsWord_NonIdent( m.fb.Get(CL, CP) ); CP++ )
    {
      ncp.crsChar = CP;
    }
  }
  else { // Should never get here:
    return false;
  }
  return true;
}

void GoToOppositeBracket_Forward( LineView::Data& m
                                , const char ST_C
                                , const char FN_C )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned NUM_LINES = m.fb.NumLines();
  const unsigned CL = m.view.CrsLine();
  const unsigned CC = m.view.CrsChar();

  // Search forward
  unsigned level = 0;
  bool     found = false;

  for( unsigned l=CL; !found && l<NUM_LINES; l++ )
  {
    const unsigned LL = m.fb.LineLen( l );

    for( unsigned p=(CL==l)?(CC+1):0; !found && p<LL; p++ )
    {
      const char C = m.fb.Get( l, p );

      if     ( C==ST_C ) level++;
      else if( C==FN_C )
      {
        if( 0 < level ) level--;
        else {
          found = true;

          m.view.GoToCrsPos_Write( l, p );
        }
      }
    }
  }
}

void GoToOppositeBracket_Backward( LineView::Data& m
                                 , const char ST_C
                                 , const char FN_C )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const int CL = m.view.CrsLine();
  const int CC = m.view.CrsChar();

  // Search forward
  unsigned level = 0;
  bool     found = false;

  for( int l=CL; !found && 0<=l; l-- )
  {
    const unsigned LL = m.fb.LineLen( l );

    for( int p=(CL==l)?(CC-1):(LL-1); !found && 0<=p; p-- )
    {
      const char C = m.fb.Get( l, p );

      if     ( C==ST_C ) level++;
      else if( C==FN_C )
      {
        if( 0 < level ) level--;
        else {
          found = true;

          m.view.GoToCrsPos_Write( l, p );
        }
      }
    }
  }
}

// Cursor is moving forward
// Write out from (OCL,OCP) up to but not including (NCL,NCP)
void GoToCrsPos_WV_Forward( LineView::Data& m
                          , const unsigned OCP
                          , const unsigned NCP )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned CL = m.view.CrsLine();

  for( unsigned k=OCP; k<NCP; k++ )
  {
    int byte = m.fb.Get( CL, k );

    Console::Set( m.y
                , m.view.Char_2_GL( k ), byte, Get_Style(m,CL,k) );
  }
}

// Cursor is moving backwards
// Write out from (OCL,OCP) back to but not including (NCL,NCP)
void GoToCrsPos_WV_Backward( LineView::Data& m
                           , const unsigned OCP
                           , const unsigned NCP )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned CL = m.view.CrsLine();

  const unsigned LL = m.fb.LineLen( CL ); // Line length
  if( LL ) {
    const unsigned START = Min( OCP, LL-1 );
    for( unsigned k=START; NCP<k; k-- )
    {
      int byte = m.fb.Get( CL, k );
      Console::Set( m.y
                  , m.view.Char_2_GL( k ), byte, Get_Style(m,CL,k) );
    }
  }
}

void GoToCrsPos_Write_Visual( LineView::Data& m
                            , const unsigned OCP
                            , const unsigned NCP )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // (old cursor pos) < (new cursor pos)
  const bool OCP_LT_NCP = OCP < NCP;

  if( OCP_LT_NCP ) // Cursor moved forward
  {
    GoToCrsPos_WV_Forward( m, OCP, NCP );
  }
  else // NCP_LT_OCP // Cursor moved backward
  {
    GoToCrsPos_WV_Backward( m, OCP, NCP );
  }
  m.crsCol = NCP - m.leftChar;
  Console::Update();
  m.view.PrintCursor();
}

static bool RV_Style( const Style s )
{
  return s == S_RV_NORMAL
      || s == S_RV_STAR
      || s == S_RV_DEFINE
      || s == S_RV_COMMENT
      || s == S_RV_CONST
      || s == S_RV_CONTROL
      || s == S_RV_VARTYPE;
}

static Style RV_Style_2_NonRV( const Style RVS )
{
  Style s = S_NORMAL;

  if     ( RVS == S_RV_STAR    ) s = S_STAR   ;
  else if( RVS == S_RV_DEFINE  ) s = S_DEFINE ;
  else if( RVS == S_RV_COMMENT ) s = S_COMMENT;
  else if( RVS == S_RV_CONST   ) s = S_CONST  ;
  else if( RVS == S_RV_CONTROL ) s = S_CONTROL;
  else if( RVS == S_RV_VARTYPE ) s = S_VARTYPE;

  return s;
}

bool Do_n_FindNextPattern( LineView::Data& m, CrsPos& ncp )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = m.fb.NumLines();

  const unsigned OCL = m.view.CrsLine();
  const unsigned OCC = m.view.CrsChar();

  unsigned st_l = OCL;
  unsigned st_c = OCC;

  bool found_next_star = false;

  // Move past current star:
  const unsigned LL = m.fb.LineLen( OCL );

  for( ; st_c<LL && m.view.InStar(OCL,st_c); st_c++ ) ;

  // Go down to next line
  if( LL <= st_c ) { st_c=0; st_l++; }

  // Search for first star position past current position
  for( unsigned l=st_l; !found_next_star && l<NUM_LINES; l++ )
  {
    const unsigned LL = m.fb.LineLen( l );

    for( unsigned p=st_c
       ; !found_next_star && p<LL
       ; p++ )
    {
      if( m.view.InStar(l,p) )
      {
        found_next_star = true;
        ncp.crsLine = l;
        ncp.crsChar = p;
      }
    }
    // After first line, always start at beginning of line
    st_c = 0;
  }
  // Near end of file and did not find any patterns, so go to first pattern in file
  if( !found_next_star )
  {
    for( unsigned l=0; !found_next_star && l<=OCL; l++ )
    {
      const unsigned LL = m.fb.LineLen( l );
      const unsigned END_C = (OCL==l) ? Min( OCC, LL ) : LL;

      for( unsigned p=0; !found_next_star && p<END_C; p++ )
      {
        if( m.view.InStar(l,p) )
        {
          found_next_star = true;
          ncp.crsLine = l;
          ncp.crsChar = p;
        }
      }
    }
  }
  return found_next_star;
}

bool Do_N_FindPrevPattern( LineView::Data& m, CrsPos& ncp )
{
  Trace trace( __PRETTY_FUNCTION__ );
  m.view.MoveInBounds();

  const unsigned NUM_LINES = m.fb.NumLines();

  const unsigned OCL = m.view.CrsLine();
  const unsigned OCC = m.view.CrsChar();

  bool found_prev_star = false;

  // Search for first star position before current position
  for( int l=OCL; !found_prev_star && 0<=l; l-- )
  {
    const int LL = m.fb.LineLen( l );

    int p=LL-1;
    if( OCL==l ) p = OCC ? OCC-1 : 0;

    for( ; 0<p && !found_prev_star; p-- )
    {
      for( ; 0<=p && m.view.InStar(l,p); p-- )
      {
        found_prev_star = true;
        ncp.crsLine = l;
        ncp.crsChar = p;
      }
    }
  }
  // Near beginning of file and did not find any patterns, so go to last pattern in file
  if( !found_prev_star )
  {
    for( int l=NUM_LINES-1; !found_prev_star && OCL<l; l-- )
    {
      const unsigned LL = m.fb.LineLen( l );

      int p=LL-1;
      if( OCL==l ) p = OCC ? OCC-1 : 0;

      for( ; 0<p && !found_prev_star; p-- )
      {
        for( ; 0<=p && m.view.InStar(l,p); p-- )
        {
          found_prev_star = true;
          ncp.crsLine = l;
          ncp.crsChar = p;
        }
      }
    }
  }
  return found_prev_star;
}

bool Do_dw_get_fn( LineView::Data& m
                 , const unsigned  st_line, const unsigned  st_char
                 ,       unsigned& fn_line,       unsigned& fn_char )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned LL = m.fb.LineLen( st_line );
  const uint8_t  C  = m.fb.Get( st_line, st_char );

  if( IsSpace( C )         // On white space
   || ( st_char < LLM1(LL) // On non-white space before white space
     && IsSpace( m.fb.Get( st_line, st_char+1 ) ) ) )
  {
    // w:
    CrsPos ncp_w = { 0, 0 };
    bool ok = GoToNextWord_GetPosition( m, ncp_w );
    if( ok && 0 < ncp_w.crsChar ) ncp_w.crsChar--;
    if( ok && st_line == ncp_w.crsLine
           && st_char <= ncp_w.crsChar )
    {
      fn_line = ncp_w.crsLine;
      fn_char = ncp_w.crsChar;
      return true;
    }
  }
  // if not on white space, and
  // not on non-white space before white space,
  // or fell through, try e:
  CrsPos ncp_e = { 0, 0 };
  bool ok = GoToEndOfWord_GetPosition( m, ncp_e );

  if( ok && st_line == ncp_e.crsLine
         && st_char <= ncp_e.crsChar )
  {
    fn_line = ncp_e.crsLine;
    fn_char = ncp_e.crsChar;
    return true;
  }
  return false;
}

void Do_dd_Normal( LineView::Data& m, const unsigned ONL )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned OCL = m.view.CrsLine();           // Old cursor line
  const unsigned OCP = m.view.CrsChar();           // Old cursor position
  const unsigned OLL = m.fb.LineLen( OCL ); // Old line length

  const bool DELETING_LAST_LINE = OCL == ONL-1;

  const unsigned NCL = DELETING_LAST_LINE ? OCL-1 : OCL; // New cursor line
  const unsigned NLL = DELETING_LAST_LINE ? m.fb.LineLen( NCL )
                                          : m.fb.LineLen( NCL + 1 );
  const unsigned NCP = Min( OCP, 0<NLL ? NLL-1 : 0 );

  // Remove line from FileBuf and save in paste register:
  Line* lp = m.fb.RemoveLineP( OCL );
  if( lp ) {
    // m.reg will own nlp
    m.reg.clear();
    m.reg.push( lp );
    m.vis.SetPasteMode( PM_LINE );
  }
  m.view.GoToCrsPos_NoWrite( NCL, NCP );

  m.fb.UpdateCmd();
}

void Do_dd_BufferEditor( LineView::Data& m, const unsigned ONL )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned OCL = m.view.CrsLine(); // Old cursor line

  // Can only delete one of the user files out of buffer editor
  if( USER_FILE <= OCL )
  {
    Line* lp = m.fb.GetLineP( OCL );

    const char* fname = lp->c_str( 0 );

    if( !m.vis.File_Is_Displayed( fname ) )
    {
      m.vis.ReleaseFileName( fname );

      Do_dd_Normal( m, ONL );
    }
  }
}

void Do_p_or_P_st_fn( LineView::Data& m, Paste_Pos paste_pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned N_REG_LINES = m.reg.len();

  for( unsigned k=0; k<N_REG_LINES; k++ )
  {
    const unsigned NLL = m.reg[k]->len();  // New line length
    const unsigned OCL = m.view.CrsLine();               // Old cursor line

    if( 0 == k ) // Add to current line
    {
      m.view.MoveInBounds();
      const unsigned OLL = m.fb.LineLen( OCL );
      const unsigned OCP = m.view.CrsChar();               // Old cursor position

      // If line we are pasting to is zero length, dont paste a space forward
      const unsigned forward = OLL ? ( paste_pos==PP_After ? 1 : 0 ) : 0;

      for( unsigned i=0; i<NLL; i++ )
      {
        uint8_t C = m.reg[k]->get(i);

        m.fb.InsertChar( OCL, OCP+i+forward, C );
      }
      if( 1 < N_REG_LINES && OCP+forward < OLL ) // Move rest of first line onto new line below
      {
        m.fb.InsertLine( OCL+1 );
        for( unsigned i=0; i<(OLL-OCP-forward); i++ )
        {
          uint8_t C = m.fb.RemoveChar( OCL, OCP + NLL+forward );
          m.fb.PushChar( OCL+1, C );
        }
      }
    }
    else if( N_REG_LINES-1 == k )
    {
      // Insert a new line if at end of file:
      if( m.fb.NumLines() == OCL+k ) m.fb.InsertLine( OCL+k );

      for( unsigned i=0; i<NLL; i++ )
      {
        uint8_t C = m.reg[k]->get(i);

        m.fb.InsertChar( OCL+k, i, C );
      }
    }
    else {
      // Put reg on line below:
      m.fb.InsertLine( OCL+k, *(m.reg[k]) );
    }
  }
  // Update current view after other views, so that the cursor will be put back in place
  m.fb.UpdateCmd();
}

void Do_P_line( LineView::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned OCL = m.view.CrsLine();  // Old cursor line

  const unsigned NUM_LINES = m.reg.len();

  for( unsigned k=0; k<NUM_LINES; k++ )
  {
    // Put reg on line above:
    m.fb.InsertLine( OCL+k, *(m.reg[k]) );
  }
  m.fb.UpdateCmd();
}

LineView::LineView( Vis& vis
                , Key& key
                , FileBuf& fb
                , LinesList& reg
                , char* cbuf
                , String& sbuf
                , const char banner_delim )
  : m( *new(__FILE__, __LINE__) Data( *this
                                    , vis
                                    , key
                                    , fb
                                    , reg
                                    , cbuf
                                    , sbuf
                                    , banner_delim ) )
{
}

LineView::~LineView()
{
  delete &m;
}

FileBuf* LineView::GetFB() const { return &m.fb; }

unsigned LineView::WinCols() const { return m.nCols; }
unsigned LineView::X() const { return m.x; }
unsigned LineView::Y() const { return m.y; }

unsigned LineView::GetTopLine () const { return m.topLine ; }
unsigned LineView::GetLeftChar() const { return m.leftChar; }
unsigned LineView::GetCrsRow  () const { return 0; }
unsigned LineView::GetCrsCol  () const { return m.crsCol  ; }

Line* LineView::GetCrsLine()
{
  return m.fb.GetLineP( m.topLine );
}

void LineView::SetTopLine ( const unsigned val )
{
  m.topLine  = val;
}

void LineView::SetLeftChar( const unsigned val )
{
  m.leftChar = val;
}

void LineView::SetCrsRow( unsigned val )
{
}

void LineView::SetCrsCol  ( const unsigned val )
{
  m.crsCol   = val;
}

unsigned LineView::WorkingCols() const
{
  return m.nCols-  left__border-right_border - m.prefix_len;
}

unsigned LineView::CrsLine() const
{
  return m.topLine;
}

unsigned LineView::BotLine() const
{
  return m.topLine;
}

unsigned LineView::CrsChar() const
{
  return m.leftChar + m.crsCol;
}

unsigned LineView::RightChar() const
{
  return m.leftChar + WorkingCols()-1;
}

// Translates zero based working view column to zero based global column
unsigned LineView::Col_Win_2_GL( const unsigned win_col ) const
{
  return m.x + left__border + m.prefix_len + win_col;
}

// Translates zero based file line char position to zero based global column
unsigned LineView::Char_2_GL( const unsigned line_char ) const
{
  return m.x + left__border + m.prefix_len - m.leftChar + line_char;
}

void LineView::SetContext( const unsigned num_cols
                         , const unsigned x
                         , const unsigned y )
{
  m.nCols    = num_cols;
  m.x        = x;
  m.y        = y;
//m.topLine  = LLM1( m.fb.NumLines() );
  m.leftChar = 0;
}

bool LineView::GetInsertMode() const { return m.inInsertMode; }
void LineView::SetInsertMode( const bool val ) { m.inInsertMode = val; }
bool LineView::GetReplaceMode() const { return m.inReplaceMode; }
void LineView::SetReplaceMode( const bool val ) { m.inReplaceMode = val; }

void LineView::GoUp()
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned NUM_LINES = m.fb.NumLines();
  if( 0==NUM_LINES ) return;

  const unsigned OCL = CrsLine(); // Old cursor line
  if( OCL == 0 ) return;

  const unsigned NCL = OCL-1; // New cursor line

  const unsigned OCP = CrsChar(); // Old cursor position
        unsigned NCP = OCP;

  GoToCrsPos_Write( NCL, NCP );
}

void LineView::GoDown()
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned NUM_LINES = m.fb.NumLines();
  if( 0==NUM_LINES ) return;

  const unsigned OCL = CrsLine(); // Old cursor line
  if( OCL == NUM_LINES-1 ) return;

  const unsigned NCL = OCL+1; // New cursor line

  const unsigned OCP = CrsChar(); // Old cursor position
        unsigned NCP = OCP;

  GoToCrsPos_Write( NCL, NCP );
}

void LineView::GoLeft()
{
  Trace trace( __PRETTY_FUNCTION__ );
  if( 0==m.fb.NumLines() ) return;

  const unsigned CL = CrsLine(); // Cursor line
  const unsigned CP = CrsChar(); // Cursor position

  if( CP == 0 ) return;

  GoToCrsPos_Write( CL, CP-1 );
}

void LineView::GoRight()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0<m.fb.NumLines() )
  {
    const unsigned CL = CrsLine(); // Cursor line
    const unsigned LL = m.fb.LineLen( CL );

    if( 0<LL )
    {
      const unsigned CP = CrsChar(); // Cursor position

      if( CP < LL-1 )
      {
        GoToCrsPos_Write( CL, CP+1 );
      }
    }
  }
}

void LineView::GoToBegOfLine()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0==m.fb.NumLines() ) return;

  const unsigned OCL = CrsLine(); // Old cursor line

  GoToCrsPos_Write( OCL, 0 );
}

void LineView::GoToEndOfLine()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0==m.fb.NumLines() ) return;

  const unsigned LL = m.fb.LineLen( CrsLine() );

  const unsigned OCL = CrsLine(); // Old cursor line

  GoToCrsPos_Write( OCL, LL ? LL-1 : 0 );
}

void LineView::GoToBegOfNextLine()
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned NUM_LINES = m.fb.NumLines();
  if( 0==NUM_LINES ) return;

  const unsigned OCL = CrsLine(); // Old cursor line
  if( (NUM_LINES-1) <= OCL ) return; // On last line, so cant go down

  GoToCrsPos_Write( OCL+1, 0 );
}

void LineView::GoToLine( const unsigned user_line_num )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Internal line number is 1 less than user line number:
  const unsigned NCL = user_line_num - 1; // New cursor line number

  if( m.fb.NumLines() <= NCL )
  {
    PrintCursor();
  }
  else {
    GoToCrsPos_Write( NCL, 0 );
  }
}

void LineView::GoToTopOfFile()
{
  Trace trace( __PRETTY_FUNCTION__ );

  GoToCrsPos_Write( 0, 0 );
}

void LineView::GoToEndOfFile()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = m.fb.NumLines();

  GoToCrsPos_Write( NUM_LINES-1, 0 );
}

void LineView::GoToNextWord()
{
  Trace trace( __PRETTY_FUNCTION__ );
  CrsPos ncp = { 0, 0 };

  if( GoToNextWord_GetPosition( m, ncp ) )
  {
    GoToCrsPos_Write( ncp.crsLine, ncp.crsChar );
  }
}

void LineView::GoToPrevWord()
{
  Trace trace( __PRETTY_FUNCTION__ );

  CrsPos ncp = { 0, 0 };

  if( GoToPrevWord_GetPosition( m, ncp ) )
  {
    GoToCrsPos_Write( ncp.crsLine, ncp.crsChar );
  }
}

void LineView::GoToEndOfWord()
{
  Trace trace( __PRETTY_FUNCTION__ );

  CrsPos ncp = { 0, 0 };

  if( GoToEndOfWord_GetPosition( m, ncp ) )
  {
    GoToCrsPos_Write( ncp.crsLine, ncp.crsChar );
  }
}

void LineView::GoToStartOfRow()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0==m.fb.NumLines() ) return;

  const unsigned OCL = CrsLine(); // Old cursor line

  GoToCrsPos_Write( OCL, m.leftChar );
}

void LineView::GoToEndOfRow()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0==m.fb.NumLines() ) return;

  const unsigned OCL = CrsLine(); // Old cursor line

  const unsigned LL = m.fb.LineLen( OCL );
  if( 0==LL ) return;

  const unsigned NCP = Min( LL-1, m.leftChar + WorkingCols()-1 );

  GoToCrsPos_Write( OCL, NCP );
}

void LineView::GoToOppositeBracket()
{
  Trace trace( __PRETTY_FUNCTION__ );
  MoveInBounds();
  const unsigned NUM_LINES = m.fb.NumLines();
  const unsigned CL = CrsLine();
  const unsigned CC = CrsChar();
  const unsigned LL = m.fb.LineLen( CL );

  if( 0==NUM_LINES || 0==LL ) return;

  const char C = m.fb.Get( CL, CC );

  if( C=='{' || C=='[' || C=='(' )
  {
    char finish_char = 0;
    if     ( C=='{' ) finish_char = '}';
    else if( C=='[' ) finish_char = ']';
    else if( C=='(' ) finish_char = ')';
    else              ASSERT( __LINE__, 0, "Un-handled case" );

    GoToOppositeBracket_Forward( m, C, finish_char );
  }
  else if( C=='}' || C==']' || C==')' )
  {
    char finish_char = 0;
    if     ( C=='}' ) finish_char = '{';
    else if( C==']' ) finish_char = '[';
    else if( C==')' ) finish_char = '(';
    else              ASSERT( __LINE__, 0, "Un-handled case" );

    GoToOppositeBracket_Backward( m, C, finish_char );
  }
}

void LineView::GoToCrsPos_NoWrite( const unsigned ncp_crsLine
                                 , const unsigned ncp_crsChar )
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.topLine = ncp_crsLine;

  // These moves refer to View of buffer:
  const bool MOVE_RIGHT = RightChar() < ncp_crsChar;
  const bool MOVE_LEFT  = ncp_crsChar < m.leftChar;

  if     ( MOVE_RIGHT ) m.leftChar = ncp_crsChar - WorkingCols()+1;
  else if( MOVE_LEFT  ) m.leftChar = ncp_crsChar;

  m.crsCol = ncp_crsChar - m.leftChar;
}

void LineView::GoToCrsPos_Write( const unsigned ncp_crsLine
                               , const unsigned ncp_crsChar )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned OCP = CrsChar();
  const unsigned NCL = ncp_crsLine;
  const unsigned NCP = ncp_crsChar;

  if( m.topLine == NCL && OCP == NCP )
  {
    PrintCursor();  // Put cursor back into position.
    return;
  }
  if( m.inVisualMode )
  {
    m.v_fn_line = NCL;
    m.v_fn_char = NCP;
  }
  // These moves refer to View of buffer:
  const bool MOVE_UP_DN = NCL != m.topLine;
  const bool MOVE_RIGHT = RightChar() < NCP;
  const bool MOVE_LEFT  = NCP < m.leftChar;

  const bool redraw = MOVE_UP_DN || MOVE_RIGHT || MOVE_LEFT;

  if( redraw )
  {
    m.topLine = NCL;

    if     ( MOVE_RIGHT ) m.leftChar = NCP - WorkingCols() + 1;
    else if( MOVE_LEFT  ) m.leftChar = NCP;

    // m.crsRow and m.crsCol must be set to new values before calling CalcNewCrsByte
    m.crsCol = NCP - m.leftChar;

    Update();
  }
  else if( m.inVisualMode )
  {
    GoToCrsPos_Write_Visual( m, OCP, NCP );
  }
  else {
    m.crsCol = NCP - m.leftChar;

    PrintCursor();  // Put cursor into position.
  }
}

// If past end of line, move back to end of line.
// Returns true if moved, false otherwise.
//
bool LineView::MoveInBounds()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned CL  = CrsLine();
  const unsigned LL  = m.fb.LineLen( CL );
  const unsigned EOL = LL ? LL-1 : 0;

  if( EOL < CrsChar() ) // Since cursor is now allowed past EOL,
  {                      // it may need to be moved back:
    GoToCrsPos_NoWrite( CL, EOL );
    return true;
  }
  return false;
}

// Returns true if end of line delimiter was entered, else false
bool LineView::Do_i()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0 == m.fb.NumLines() ) m.fb.PushLine();

  m.inInsertMode = true;
  Update(); //< Clear any possible message left on command line

  const unsigned LL = m.fb.LineLen( CrsLine() );  // Line length

  // For user friendlyness, move cursor to new position immediately:
  // Since cursor is now allowed past EOL, it may need to be moved back:
  GoToCrsPos_Write( CrsLine(), LL < CrsChar() ? LL : CrsChar() );

  const bool CURSOR_AT_EOL = CrsChar() == LL;

  const bool EOL_DELIM_ENTERED = CURSOR_AT_EOL ? Do_i_tabs( m )
                                               : Do_i_normal( m );
  if( !EOL_DELIM_ENTERED )
  {
    // Move cursor back one space:
    if( 0 < m.crsCol ) m.crsCol--;

    m.inInsertMode = false;
    DisplayBanner_PrintCursor(m);
  }
  return EOL_DELIM_ENTERED;
}

bool LineView::Do_a()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.fb.NumLines()==0 ) return Do_i();

  const unsigned CL = CrsLine();
  const unsigned LL = m.fb.LineLen( CL );
  if( 0==LL ) return Do_i();

  const bool CURSOR_AT_EOL = ( CrsChar() == LL-1 );
  if( CURSOR_AT_EOL )
  {
    GoToCrsPos_NoWrite( CL, LL );
  }
  const bool CURSOR_AT_RIGHT_COL = ( m.crsCol == WorkingCols()-1 );

  if( CURSOR_AT_RIGHT_COL )
  {
    // Only need to scroll window right, and then enter insert i:
    m.leftChar++; //< This increments CrsChar()
  }
  else if( !CURSOR_AT_EOL ) // If cursor was at EOL, already moved cursor forward
  {
    // Only need to move cursor right, and then enter insert i:
    m.crsCol += 1; //< This increments CrsChar()
  }
  m.fb.UpdateCmd();

  return Do_i();
}

bool LineView::Do_A()
{
  Trace trace( __PRETTY_FUNCTION__ );

  GoToEndOfLine();

  return Do_a();
}

bool LineView::Do_o()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned ONL = m.fb.NumLines();
  const unsigned OCL = CrsLine();

  // Add the new line:
  const unsigned NCL = ( 0 != ONL ) ? OCL+1 : 0;
  m.fb.InsertLine( NCL );
  m.crsCol = 0;
  m.leftChar = 0;
  if     ( 0==ONL ) ; // Do nothing
  else {
    // If we were on the bottom working line, scroll screen down
    // one line so that the cursor line is not below the screen.
    m.topLine++;
  }
  m.fb.UpdateCmd();

  return Do_i();
}

// Returns true if something was changed, else false
//
bool LineView::Do_v()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0 == m.fb.NumLines() ) m.fb.PushLine();

  m.inVisualMode = true;
  DisplayBanner(m);

  const unsigned LL = m.fb.LineLen( CrsLine() );  // Line length

  // For user friendlyness, move cursor to new position immediately:
  // Since cursor is now allowed past EOL, it may need to be moved back:
  GoToCrsPos_Write( CrsLine(), LL < CrsChar() ? LL : CrsChar() );

  m.v_st_line = CrsLine();  m.v_fn_line = m.v_st_line;
  m.v_st_char = CrsChar();  m.v_fn_char = m.v_st_char;

  // Write current byte in visual:
  Replace_Crs_Char( m, S_VISUAL );

  while( m.inVisualMode )
  {
    const char C=m.key.In();

    if     ( C == 'l' ) GoRight();
    else if( C == 'h' ) GoLeft();
    else if( C == '0' ) GoToBegOfLine();
    else if( C == '$' ) GoToEndOfLine();
    else if( C == 'g' ) Do_v_Handle_g(m);
    else if( C == 'f' ) m.vis.L_Handle_f();
    else if( C == ';' ) m.vis.L_Handle_SemiColon();
    else if( C == 'y' ) { Do_y_v(m); goto EXIT_VISUAL; }
    else if( C == 'Y' ) { Do_Y_v(m); goto EXIT_VISUAL; }
    else if( C == 'x'
          || C == 'd' ) { Do_x_v(m); return true; }
    else if( C == 'D' ) { Do_D_v(m); return true; }
    else if( C == 's' ) { Do_s_v(m); return true; }
    else if( C == '~' ) { Do_Tilda_v(m); return true; }
    else if( C == ESC ) { goto EXIT_VISUAL; }
  }
  return false;

EXIT_VISUAL:
  m.inVisualMode = false;
  DisplayBanner(m);
  Undo_v(m);
  return false;
}

void LineView::Do_x()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // If there is nothing to 'x', just return:
  if( !m.fb.NumLines() ) return;

  const unsigned CL = CrsLine();
  const unsigned LL = m.fb.LineLen( CL );

  // If nothing on line, just return:
  if( !LL ) return;

  // If past end of line, move to end of line:
  if( LL-1 < CrsChar() )
  {
    GoToCrsPos_Write( CL, LL-1 );
  }
  const uint8_t C = m.fb.RemoveChar( CL, CrsChar() );

  // Put char x'ed into register:
  Line* nlp = m.vis.BorrowLine( __FILE__,__LINE__ );
  nlp->push(__FILE__,__LINE__, C );
  m.reg.clear();
  m.reg.push( nlp );
  m.vis.SetPasteMode( PM_ST_FN );

  const unsigned NLL = m.fb.LineLen( CL ); // New line length

  // Reposition the cursor:
  if( NLL <= m.leftChar+m.crsCol )
  {
    // The char x'ed is the last char on the line, so move the cursor
    //   back one space.  Above, a char was removed from the line,
    //   but m.crsCol has not changed, so the last char is now NLL.
    // If cursor is not at beginning of line, move it back one more space.
    if( m.crsCol ) m.crsCol--;
  }
  m.fb.UpdateCmd();
}

void LineView::Do_s()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned CL  = CrsLine();
  const unsigned LL  = m.fb.LineLen( CL );
  const unsigned EOL = LL ? LL-1 : 0;
  const unsigned CP  = CrsChar();

  if( CP < EOL )
  {
    Do_x();
    Do_i();
  }
  else // EOL <= CP
  {
    Do_x();
    Do_a();
  }
}

void LineView::Do_cw()
{
  const unsigned result = Do_dw();

  if     ( result==1 ) Do_i();
  else if( result==2 ) Do_a();
}

// If nothing was deleted, return 0.
// If last char on line was deleted, return 2,
// Else return 1.
int LineView::Do_dw()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = m.fb.NumLines();

  if( 0< NUM_LINES )
  {
    const unsigned st_line = CrsLine();
    const unsigned st_char = CrsChar();

    const unsigned LL = m.fb.LineLen( st_line );

    // If past end of line, nothing to do
    if( st_char < LL )
    {
      // Determine fn_line, fn_char:
      unsigned fn_line = 0;
      unsigned fn_char = 0;

      if( Do_dw_get_fn( m, st_line, st_char, fn_line, fn_char ) )
      {
        Do_x_range( m, st_line, st_char, fn_line, fn_char );

        bool deleted_last_char = fn_char == LL-1;

        return deleted_last_char ? 2 : 1;
      }
    }
  }
  return 0;
}

void LineView::Do_D()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = m.fb.NumLines();
  const unsigned OCL = CrsLine();  // Old cursor line
  const unsigned OCP = CrsChar();  // Old cursor position
  const unsigned OLL = m.fb.LineLen( OCL );  // Old line length

  // If there is nothing to 'D', just return:
  if( !NUM_LINES || !OLL || OLL-1 < OCP ) return;

  Line* lpd = m.vis.BorrowLine( __FILE__,__LINE__ );

  for( unsigned k=OCP; k<OLL; k++ )
  {
    uint8_t c = m.fb.RemoveChar( OCL, OCP );
    lpd->push(__FILE__,__LINE__, c );
  }
  m.reg.clear();
  m.reg.push( lpd );
  m.vis.SetPasteMode( PM_ST_FN );

  // If cursor is not at beginning of line, move it back one space.
  if( m.crsCol ) m.crsCol--;

  m.fb.UpdateCmd();
}

void LineView::Do_f( const char FAST_CHAR )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = m.fb.NumLines();

  if( 0< NUM_LINES )
  {
    const unsigned OCL = CrsLine();           // Old cursor line
    const unsigned LL  = m.fb.LineLen( OCL ); // Line length
    const unsigned OCP = CrsChar();           // Old cursor position

    if( OCP < LLM1(LL) )
    {
      unsigned NCP = 0;
      bool found_char = false;
      for( unsigned p=OCP+1; !found_char && p<LL; p++ )
      {
        const char C = m.fb.Get( OCL, p );

        if( C == FAST_CHAR )
        {
          NCP = p;
          found_char = true;
        }
      }
      if( found_char )
      {
        GoToCrsPos_Write( OCL, NCP );
      }
    }
  }
}

// Go to next pattern
void LineView::Do_n()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = m.fb.NumLines();

  if( 0 < NUM_LINES )
  {
    CrsPos ncp = { 0, 0 }; // Next cursor position

    if( Do_n_FindNextPattern( m, ncp ) )
    {
      GoToCrsPos_Write( ncp.crsLine, ncp.crsChar );
    }
    else {
      // Pattern not found, so put cursor back in view:
      PrintCursor();
    }
  }
}

// Go to previous pattern
void LineView::Do_N()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = m.fb.NumLines();

  if( 0 < NUM_LINES )
  {
    CrsPos ncp = { 0, 0 }; // Next cursor position

    if( Do_N_FindPrevPattern( m, ncp ) )
    {
      GoToCrsPos_Write( ncp.crsLine, ncp.crsChar );
    }
    else {
      // Pattern not found, so put cursor back in view:
      PrintCursor();
    }
  }
}

void LineView::Do_dd()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned ONL = m.fb.NumLines(); // Old number of lines

  // If there is nothing to 'dd', just return:
  if( 1 < ONL )
  {
    Do_dd_Normal( m, ONL );
  }
}

void LineView::Do_yy()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // If there is nothing to 'yy', just return:
  if( !m.fb.NumLines() ) return;

  Line l = m.fb.GetLine( CrsLine() );

  m.reg.clear();
  m.reg.push( m.vis.BorrowLine( __FILE__,__LINE__, l ) );

  m.vis.SetPasteMode( PM_LINE );
}

void LineView::Do_yw()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // If there is nothing to 'yw', just return:
  if( !m.fb.NumLines() ) return;

  const unsigned st_line = CrsLine();
  const unsigned st_char = CrsChar();

  // Determine fn_line, fn_char:
  unsigned fn_line = 0;
  unsigned fn_char = 0;

  if( Do_dw_get_fn( m, st_line, st_char, fn_line, fn_char ) )
  {
    m.reg.clear();
    m.reg.push( m.vis.BorrowLine( __FILE__,__LINE__ ) );

    // st_line and fn_line should be the same
    for( unsigned k=st_char; k<=fn_char; k++ )
    {
      m.reg[0]->push(__FILE__,__LINE__, m.fb.Get( st_line, k ) );
    }
    m.vis.SetPasteMode( PM_ST_FN );
  }
}

void LineView::Do_p()
{
  Trace trace( __PRETTY_FUNCTION__ );

  Do_p_or_P_st_fn( m, PP_After );
}

void LineView::Do_P()
{
  Trace trace( __PRETTY_FUNCTION__ );

  Do_p_or_P_st_fn( m, PP_Before );
}

// Returns true if end of line delimiter was entered, else false
bool LineView::Do_R()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.fb.NumLines()==0 ) m.fb.PushLine();

  m.inReplaceMode = true;
  DisplayBanner_PrintCursor(m);

//unsigned count = 0;
  for( char C=m.key.In(); C != ESC; C=m.key.In() )
  {
    if( IsEndOfLineDelim( C ) )
    {
      m.view.HandleReturn();
      return true;
    }
    else if( BS == C || DEL == C )
    {
      m.fb.Undo( m.view );
    //if( count ) InsertBackspace(m);
    }
    else ReplaceAddChars( m, C );

  //if( BS == C || DEL == C ) { if( count ) count--; }
  //else count++;
  }
  // Move cursor back one space:
  if( 0 < m.crsCol ) m.crsCol--;

  m.inReplaceMode = false;
  DisplayBanner_PrintCursor(m);

  return false;
}

void LineView::Do_J()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = m.fb.NumLines(); // Number of lines
  const unsigned CL        = CrsLine();       // Cursor line

  const bool ON_LAST_LINE = ( CL == NUM_LINES-1 );

  if( ON_LAST_LINE || NUM_LINES < 2 ) return;

  GoToEndOfLine();

  Line* lp = m.fb.RemoveLineP( CL+1 );
  m.fb.AppendLineToLine( CL  , lp );

  // Update() is less efficient than only updating part of the screen,
  //   but it makes the code simpler.
  m.fb.UpdateCmd();
}

void LineView::Do_Tilda()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0==m.fb.NumLines() ) return;

  const unsigned OCL = CrsLine(); // Old cursor line
  const unsigned OCP = CrsChar(); // Old cursor position
  const unsigned LL  = m.fb.LineLen( OCL );

  if( !LL || LL-1 < OCP ) return;

  char c = m.fb.Get( CrsLine(), CrsChar() );
  bool changed = false;
  if     ( isupper( c ) ) { c = tolower( c ); changed = true; }
  else if( islower( c ) ) { c = toupper( c ); changed = true; }

  if( m.crsCol < Min( LL-1, WorkingCols()-1 ) )
  {
    if( changed ) m.fb.Set( CrsLine(), CrsChar(), c );
    // Need to move cursor right:
    m.crsCol++;
  }
  else if( RightChar() < LL-1 )
  {
    // Need to scroll window right:
    if( changed ) m.fb.Set( CrsLine(), CrsChar(), c );
    m.leftChar++;
  }
  else // RightChar() == LL-1
  {
    // At end of line so cant move or scroll right:
    if( changed ) m.fb.Set( CrsLine(), CrsChar(), c );
  }
  m.fb.UpdateCmd();
}

//void LineView::Do_u()
//{
//  Trace trace( __PRETTY_FUNCTION__ );
//
//  m.fb.Undo( m.view );
//}

//void LineView::Do_U()
//{
//  Trace trace( __PRETTY_FUNCTION__ );
//
//  m.fb.UndoAll( m.view );
//}

bool LineView::InVisualArea( const unsigned line, const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.inVisualMode )
  {
    return InVisualStFn( line, pos );
  }
  return false;
}

bool LineView::InVisualStFn( const unsigned line, const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.v_st_line == line && line == m.v_fn_line )
  {
    return (m.v_st_char <= pos && pos <= m.v_fn_char)
        || (m.v_fn_char <= pos && pos <= m.v_st_char);
  }
  else if( (m.v_st_line < line && line < m.v_fn_line)
        || (m.v_fn_line < line && line < m.v_st_line) )
  {
    return true;
  }
  else if( m.v_st_line == line && line < m.v_fn_line )
  {
    return m.v_st_char <= pos;
  }
  else if( m.v_fn_line == line && line < m.v_st_line )
  {
    return m.v_fn_char <= pos;
  }
  else if( m.v_st_line < line && line == m.v_fn_line )
  {
    return pos <= m.v_fn_char;
  }
  else if( m.v_fn_line < line && line == m.v_st_line )
  {
    return pos <= m.v_st_char;
  }
  return false;
}

bool LineView::InComment( const unsigned line, const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  return m.fb.HasStyle( line, pos, HI_COMMENT );
}

bool LineView::InDefine( const unsigned line, const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  return m.fb.HasStyle( line, pos, HI_DEFINE );
}

bool LineView::InConst( const unsigned line, const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  return m.fb.HasStyle( line, pos, HI_CONST );
}

bool LineView::InControl( const unsigned line, const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  return m.fb.HasStyle( line, pos, HI_CONTROL );
}

bool LineView::InVarType( const unsigned line, const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  return m.fb.HasStyle( line, pos, HI_VARTYPE );
}

bool LineView::InStar( const unsigned line, const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  return m.fb.HasStyle( line, pos, HI_STAR );
}

bool LineView::InNonAscii( const unsigned line, const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  return m.fb.HasStyle( line, pos, HI_NONASCII );
}

// 1. Re-position window if needed
// 2. Draw borders
// 4. Re-draw working window
// 5. Re-draw status line
// 6. Re-draw file-name line
// 7. Draw command line
// 8. Put cursor back in window
//
void LineView::Update()
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.fb.Find_Styles( m.topLine + WORKING_ROWS );
  m.fb.ClearStars();
  m.fb.Find_Stars();

  RepositionView();
  DisplayBanner( m );
  PrintWorkingView();

  Console::Update();

  PrintCursor();

  Console::Flush();
}

void LineView::RepositionView()
{
  Trace trace( __PRETTY_FUNCTION__ );
  // If a window re-size has taken place, and the window has gotten
  // smaller, change top line and left char if needed, so that the
  // cursor is in the buffer when it is re-drawn
  if( WorkingCols() <= m.crsCol )
  {
    m.leftChar += ( m.crsCol - WorkingCols() + 1 );
    m.crsCol   -= ( m.crsCol - WorkingCols() + 1 );
  }
}

void LineView::PrintWorkingView()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned WC    = WorkingCols();

  // Dont allow line wrap:
  const unsigned k     = m.topLine;
  const unsigned LL    = m.fb.LineLen( k );
  const unsigned G_ROW = m.y;

  unsigned col=0;
  for( unsigned i=m.leftChar; i<LL && col<WC; i++, col++ )
  {
    Style s    = Get_Style( m, k, i );
    int   byte = m.fb.Get( k, i );

    PrintWorkingView_Set( LL, G_ROW, col, i, byte, s );
  }
  for( ; col<WC; col++ )
  {
    Console::Set( G_ROW, Col_Win_2_GL( col ), ' ', S_EMPTY );
  }
}

void LineView::PrintWorkingView_Set( const unsigned LL
                                  , const unsigned G_ROW
                                  , const unsigned col
                                  , const unsigned i
                                  , const unsigned byte
                                  , const Style    s )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( '\r' == byte && i==(LL-1) )
  {
    // For readability, display carriage return at end of line as a space
    Console::Set( G_ROW, Col_Win_2_GL( col ), ' ', S_NORMAL );
  }
  else {
    Console::Set( G_ROW, Col_Win_2_GL( col ), byte, s );
  }
}

// Moves cursor into position on screen:
//
void LineView::PrintCursor()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Either one of these should work:
  Console::Move_2_Row_Col( m.y, Col_Win_2_GL( m.crsCol ) );
  Console::Flush();
}

bool LineView::Has_Context()
{
  return 0 != m.topLine
      || 0 != m.leftChar
      || 0 != m.crsCol ;
}

void LineView::Clear_Context()
{
  m.topLine  = 0;
  m.leftChar = 0;
  m.crsCol   = 0;
}

void LineView::Check_Context()
{
  const unsigned NUM_LINES = m.fb.NumLines();

  if( 0 == NUM_LINES )
  {
    Clear_Context();
  }
  else {
    bool changed = false;
    unsigned CL = CrsLine();

    if( NUM_LINES <= CrsLine() )
    {
      CL = NUM_LINES-1;
      changed = true;
    }
    const unsigned LL = m.fb.LineLen( CL );
    unsigned CP = CrsChar();
    if( LL <= CP )
    {
      CP = LLM1(LL);
      changed = true;
    }
    if( changed )
    {
      GoToCrsPos_NoWrite( CL, CP );
    }
  }
}

const char* LineView::GetPathName()
{
  return m.fb.GetPathName();
}

bool LineView::HandleReturn()
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.inInsertMode = false;

  const unsigned CL = m.topLine;
  const unsigned LL = m.fb.LineLen( CL ); // Current line length

  // 1. Remove current colon command into and copy it into cbuf:
  Line* const lp = m.fb.RemoveLineP( CL );
  for( unsigned k=0; k<LL; k++ ) m.cbuf[k] = lp->get( k );
  m.cbuf[LL] = 0;

  // 2. Remove last line if it is blank:
  int NL = m.fb.NumLines(); // Number of colon file lines
  if( 0<NL && 0 == m.fb.LineLen( NL-1 ) )
  {
    m.fb.RemoveLine( NL-1 ); NL--;
  }

  // 3. Remove any other lines in colon file that match current colon command:
  for( int ln=0; ln<NL; ln++ )
  {
    Line* t_lp = m.fb.GetLineP( ln );
    const unsigned t_ln_L = m.fb.LineLen( ln ); // Current line length
    // Lines are not NULL terminated, so use strncmp with line length:
    if( 0==strncmp( m.cbuf, t_lp->c_str(0), t_ln_L ) )
    {
      m.fb.RemoveLine( ln ); NL--; ln--;
    }
  }

  // 4. Add current colon command to end of colon file:
  m.fb.PushLine( lp ); NL++;

  if( 0 < LL )
  {
    m.fb.PushLine(); NL++;
  }
  GoToCrsPos_NoWrite( NL-1, 0 );

  m.fb.UpdateCmd();

  return true;
}

void LineView::Cover()
{
  m.vis.NoDiff();

  View* cv = m.vis.CV();
  FileBuf* pfb = cv->GetFB();

  if( pfb->IsDir() )
  {
    cv->PrintCursor();
  }
  else {
    const uint8_t seed = pfb->GetSize() % 256;

    Cover_Array( *pfb, m.cover_buf, seed, m.cover_key );

    // Fill in m.cover_buf from old file data:
    // Clear old file:
    pfb->ClearChanged();
    pfb->ClearLines();
    // Read in covered file:
    pfb->ReadArray( m.cover_buf );

    // Make sure all windows have proper change status in borders
    m.vis.Update_Change_Statuses();

    // Reset view position:
    cv->Clear_Context();

    cv->Update();
  }
}

void LineView::CoverKey()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* cv = m.vis.CV();

  const char* msg = "Enter cover key:";
  const unsigned msg_len = strlen( msg );

  const unsigned G_COL = m.x + left__border;
  Console::SetS( m.y, G_COL, msg, S_NORMAL );
  Console::Move_2_Row_Col( m.y, G_COL + msg_len );
  Console::Update();
  Console::Flush();

  const unsigned WC = m.nCols - left__border - right_border;
  char* p = m.cbuf;

  for( uint8_t C=m.key.In(); !IsEndOfLineDelim( C ); C=m.key.In() )
  {
    if( BS != C && DEL != C )
    {
      // Normal
      *p++ = C;

      const unsigned local_COL = Min( msg_len+p-m.cbuf, WC-1 );

      Console::Set( m.y, m.x + local_COL, '*', S_NORMAL );

      const bool c_written = Console::Update();
      if( !c_written )
      {
        // If C was written, the cursor moves over automatically.
        // If C was not written, the cursor must be moved over manually.
        // Cursor is not written if C entered is same as what is already displayed.
        Console::Move_2_Row_Col( m.y, m.x + local_COL+1 );
      }
    }
    else { // Backspace or Delete key
      if( m.cbuf < p )
      {
        const unsigned local_COL = Min( msg_len+p-m.cbuf, WC-1 );

        Console::Set( m.y, m.x + local_COL, ' ', S_NORMAL );
        // Display space:
        Console::Update();
        // Move back onto new space:
        Console::Move_2_Row_Col( m.y, m.x + local_COL );
        p--;
      }
    }
    Console::Flush();
  }
  *p++ = 0;

  m.cover_key = m.cbuf;

  cv->PrintCursor();
}

