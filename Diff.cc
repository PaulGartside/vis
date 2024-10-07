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
#include <string.h>    // strcmp

#include "MemLog.hh"
#include "String.hh"
#include "Key.hh"
#include "Vis.hh"
#include "View.hh"
#include "FileBuf.hh"
#include "Utilities.hh"
#include "Console.hh"
#include "Line.hh"
#include "Diff.hh"

extern MemLog<MEM_LOG_BUF_SIZE> Log;

enum Diff_Type
{
  DT_UNKN0WN,
  DT_SAME,
  DT_CHANGED,
  DT_INSERTED,
  DT_DELETED,
  DT_DIFF_FILES
};

// Diff or Comparison area
struct DiffArea
{
  unsigned ln_s;     // Beginning line number in short file
  unsigned ln_l;     // Beginning line number in long  file
  unsigned nlines_s; // Number of consecutive lines different in short file
  unsigned nlines_l; // Number of consecutive lines different in long  file

  DiffArea()
    : ln_s    ( 0 )
    , ln_l    ( 0 )
    , nlines_s( 0 )
    , nlines_l( 0 )
  {}
  DiffArea( const unsigned ln_s, const unsigned nlines_s
          , const unsigned ln_l, const unsigned nlines_l )
    : ln_s    ( ln_s     )
    , ln_l    ( ln_l     )
    , nlines_s( nlines_s )
    , nlines_l( nlines_l )
  {}
  void Print()
  {
    Log.Log("DiffArea: lines_s=(%u,%u) lines_l=(%u,%u)\n"
           , ln_s+1, fnl_s(), ln_l+1, fnl_l() );
  }
  unsigned fnl_s() const { return ln_s + nlines_s; }
  unsigned fnl_l() const { return ln_l + nlines_l; }
};

struct SameArea
{
  unsigned ln_s;   // Beginning line number in short file
  unsigned ln_l;   // Beginning line number in long  file
  unsigned nlines; // Number of consecutive lines the same
  unsigned nbytes; // Number of bytes in consecutive lines the same

  void Clear();
  void Init( unsigned _ln_s, unsigned _ln_l, unsigned _nbytes );
  void Inc( unsigned _nbytes );
  void Set( const SameArea& a );
};

//struct SameLineSec
//{
//  unsigned ch_s;   // Beginning char number in short line
//  unsigned ch_l;   // Beginning char number in long  line
//  unsigned nbytes; // Number of consecutive bytes the same
//
//  void Init( unsigned _ch_s, unsigned _ch_l );
//  void Set( const SameLineSec& a );
//};

typedef Array_t<Diff_Type> LineInfo;

struct SimLines // Similar lines
{
  unsigned ln_s;   // Line number in short comp area
  unsigned ln_l;   // Line number in long  comp area
  unsigned nbytes; // Number of bytes in common between lines
  LineInfo* li_s; // Line comparison info in short comp area
  LineInfo* li_l; // Line comparison info in long  comp area
};

struct Diff_Info
{
  Diff_Type diff_type; // Diff type of line this Diff_Info refers to
  unsigned  line_num;  // Line number in file to which this Diff_Info applies (view line)
  LineInfo* pLineInfo; // Only non-nullptr if diff_type is DT_CHANGED
};

const char* Diff_Type_2_Str( const Diff_Type dt )
{
  const char* s = "UNKN0WN";

  if     ( dt == DT_UNKN0WN  ) s = "DT_UNKN0WN";
  else if( dt == DT_SAME     ) s = "DT_SAME";
  else if( dt == DT_CHANGED  ) s = "DT_CHANGED";
  else if( dt == DT_INSERTED ) s = "DT_INSERTED";
  else if( dt == DT_DELETED  ) s = "DT_DELETED";

  return s;
}

struct Diff::Data
{
  Diff&      diff;
  Vis&       vis;
  View*      pvS;
  View*      pvL;
  FileBuf*   pfS;
  FileBuf*   pfL;
  Key&       key;
  LinesList& reg;
  double     mod_time_s;
  double     mod_time_l;
  unsigned   diff_ms;
  bool       printed_diff_ms;

  unsigned topLine;   // top  of buffer view line number.
  unsigned leftChar;  // left of buffer view character number.
  unsigned crsRow;    // cursor row    in buffer view. 0 <= crsRow < WorkingRows().
  unsigned crsCol;    // cursor column in buffer view. 0 <= crsCol < WorkingCols().

  unsigned v_st_line;  // Visual start line number
  unsigned v_st_char;  // Visual start char number on line
  unsigned v_fn_line;  // Visual ending line number
  unsigned v_fn_char;  // Visual ending char number on line

  bool sts_line_needs_update;
  bool  inVisualMode;  // true if in visual  mode, else false
  bool  inVisualBlock; // true if in visual block, else false
  bool undo_v;

  String cmd_line_msg;

  Array_t<SameArea> sameList;
  Array_t<DiffArea> diffList;

  Array_t<Diff_Info> DI_List_S;
  Array_t<Diff_Info> DI_List_L;
  unsigned DI_L_ins_idx;

  Array_t<SimLines> simiList;
  Array_t<LineInfo*> line_info_cache;

  const unsigned max_files_added_per_diff = 10;
        unsigned num_files_added_this_diff = 0;

  Data( Diff& diff, Vis& vis, Key& key, LinesList& reg );
  ~Data();
};

Diff::Data::Data( Diff& diff, Vis& vis, Key& key, LinesList& reg )
  : diff( diff )
  , vis( vis )
  , pvS( 0 )
  , pvL( 0 )
  , pfS( 0 )
  , pfL( 0 )
  , key( key )
  , reg( reg )
  , mod_time_s( 0 )
  , mod_time_l( 0 )
  , diff_ms( 0 )
  , printed_diff_ms( false )
  , topLine( 0 )
  , leftChar( 0 )
  , crsRow( 0 )
  , crsCol( 0 )
  , v_st_line( 0 )
  , v_st_char( 0 )
  , v_fn_line( 0 )
  , v_fn_char( 0 )
  , sts_line_needs_update( false )
  , inVisualMode ( false )
  , inVisualBlock( false )
  , undo_v( false )
  , cmd_line_msg()
  , sameList()
  , diffList()
  , DI_List_S()
  , DI_List_L()
  , DI_L_ins_idx(0)
  , simiList()
  , line_info_cache()
{
}

Diff::Data::~Data()
{
  for( unsigned k=0; k<DI_List_S.len(); k++ )
  {
    MemMark(__FILE__,__LINE__); delete DI_List_S[k].pLineInfo; DI_List_S[k].pLineInfo = 0;
  }
  for( unsigned k=0; k<DI_List_L.len(); k++ )
  {
    MemMark(__FILE__,__LINE__); delete DI_List_L[k].pLineInfo; DI_List_L[k].pLineInfo = 0;
  }
  for( unsigned k=0; k<simiList.len(); k++ )
  {
    MemMark(__FILE__,__LINE__); delete simiList[k].li_s; simiList[k].li_s = 0;
    MemMark(__FILE__,__LINE__); delete simiList[k].li_l; simiList[k].li_l = 0;
  }
  for( unsigned k=0; k<line_info_cache.len(); k++ )
  {
    MemMark(__FILE__,__LINE__); delete line_info_cache[k]; line_info_cache[k] = 0;
  }
}

SameArea Find_Max_Same( Diff::Data& m
                      , const DiffArea& ca, const unsigned count )
{
  Trace trace( __PRETTY_FUNCTION__ );
  SameArea max_same = { 0, 0, 0, 0 };

  for( unsigned _ln_s = ca.ln_s; _ln_s<ca.fnl_s()-max_same.nlines; _ln_s++ )
  {
    unsigned ln_s = _ln_s;
    SameArea cur_same = { 0, 0, 0, 0 };
    for( unsigned ln_l = ca.ln_l; ln_s<ca.fnl_s() && ln_l<ca.fnl_l(); ln_l++ )
    {
      const Line* ls = m.pfS->GetLineP( ln_s );
      const Line* ll = m.pfL->GetLineP( ln_l );

      if( ls->chksum() != ll->chksum() ) { cur_same.Clear(); ln_s = _ln_s; }
      else {
        if( 0 == max_same.nlines   // First line match
         || 0 == cur_same.nlines ) // First line match this outer loop
        {
          cur_same.Init( ln_s, ln_l, ls->len()+1 ); // Add one to account for line delimiter
        }
        else { // Continuation of cur_same
          cur_same.Inc( Min( ls->len()+1, ll->len()+1 ) ); // Add one to account for line delimiter
        }
        if( max_same.nbytes < cur_same.nbytes ) max_same.Set( cur_same );
        ln_s++;
      }
    }
    // This line makes the diff run faster:
    if( max_same.nlines ) _ln_s = Max( _ln_s, max_same.ln_s+max_same.nlines-1 );
  }
  return max_same;
}

void Popu_SameList( Diff::Data& m, const DiffArea& CA )
{
  Trace trace( __PRETTY_FUNCTION__ );
  m.sameList.clear();
  Array_t<DiffArea> compList;
                    compList.push( CA );
  unsigned count = 0;
  DiffArea ca;

  while( compList.pop( ca ) )
  {
    SameArea same = Find_Max_Same( m, ca, count++ );

    if( same.nlines && same.nbytes ) //< Dont count a single empty line as a same area
    {
      m.sameList.push( same );

      const unsigned SAME_FNL_S = same.ln_s+same.nlines; // Same finish line short
      const unsigned SAME_FNL_L = same.ln_l+same.nlines; // Same finish line long

      if( ( same.ln_s == ca.ln_s || same.ln_l == ca.ln_l )
       && SAME_FNL_S < ca.fnl_s()
       && SAME_FNL_L < ca.fnl_l() )
      {
        // Only one new DiffArea after same:
        DiffArea ca1( SAME_FNL_S , ca.fnl_s()-SAME_FNL_S
                    , SAME_FNL_L , ca.fnl_l()-SAME_FNL_L );
        compList.push( ca1 );
      }
      else if( ( SAME_FNL_S == ca.fnl_s() || SAME_FNL_L == ca.fnl_l() )
            && ca.ln_s < same.ln_s
            && ca.ln_l < same.ln_l )
      {
        // Only one new DiffArea before same:
        DiffArea ca1( ca.ln_s, same.ln_s-ca.ln_s
                    , ca.ln_l, same.ln_l-ca.ln_l );
        compList.push( ca1 );
      }
      else if( ca.ln_s < same.ln_s && SAME_FNL_S < ca.fnl_s()
            && ca.ln_l < same.ln_l && SAME_FNL_L < ca.fnl_l() )
      {
        // Two new DiffArea's, one before same, and one after same:
        DiffArea ca1( ca.ln_s, same.ln_s-ca.ln_s
                    , ca.ln_l, same.ln_l-ca.ln_l );
        DiffArea ca2( SAME_FNL_S, ca.fnl_s()-SAME_FNL_S
                    , SAME_FNL_L, ca.fnl_l()-SAME_FNL_L );
        compList.push( ca1 );
        compList.push( ca2 );
      }
    }
  }
}

void Sort_SameList( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned SLL = m.sameList.len();

  for( unsigned k=0; k<SLL; k++ )
  {
    for( unsigned j=SLL-1; k<j; j-- )
    {
      SameArea sa0 = m.sameList[ j-1 ];
      SameArea sa1 = m.sameList[ j   ];

      if( sa1.ln_l < sa0.ln_l )
      {
        m.sameList[ j-1 ] = sa1;
        m.sameList[ j   ] = sa0;
      }
    }
  }
}

void Popu_DiffList_Begin( Diff::Data& m, const DiffArea& CA )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.sameList.len() ) // Add DiffArea before first SameArea if needed:
  {
    const SameArea& sa = m.sameList[ 0 ];

    const unsigned nlines_s_da = sa.ln_s - CA.ln_s; // Num lines in short diff area
    const unsigned nlines_l_da = sa.ln_l - CA.ln_l; // Num lines in long  diff area

    if( nlines_s_da || nlines_l_da )
    {
      // DiffArea at beginning of DiffArea:
      DiffArea da( CA.ln_s, nlines_s_da, CA.ln_l, nlines_l_da );
      m.diffList.push( da );
    }
  }
}

void Popu_DiffList_End( Diff::Data& m, const DiffArea& CA )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned SLL = m.sameList.len();

  if( SLL ) // Add DiffArea after last SameArea if needed:
  {
    SameArea sa = m.sameList[ SLL-1 ];
    const unsigned sa_s_end = sa.ln_s + sa.nlines;
    const unsigned sa_l_end = sa.ln_l + sa.nlines;

    if( sa_s_end < CA.fnl_s()
     || sa_l_end < CA.fnl_l() ) // DiffArea at end of file:
    {
      // Number of lines of short and long equal to
      // start of SameArea short and long
      DiffArea da( sa_s_end, CA.fnl_s() - sa_s_end
                 , sa_l_end, CA.fnl_l() - sa_l_end );
      m.diffList.push( da );
    }
  }
  else // No SameArea, so whole DiffArea is a DiffArea:
  {
    DiffArea da( CA.ln_s, CA.nlines_s, CA.ln_l, CA.nlines_l );
    m.diffList.push( da );
  }
}

void Popu_DiffList( Diff::Data& m, const DiffArea& CA )
{
  Trace trace( __PRETTY_FUNCTION__ );
  m.diffList.clear();

  Popu_DiffList_Begin( m, CA );

  const unsigned SLL = m.sameList.len();

  for( unsigned k=1; k<SLL; k++ )
  {
    const SameArea& sa0 = m.sameList[ k-1 ];
    const SameArea& sa1 = m.sameList[ k   ];

    unsigned da_ln_s = sa0.ln_s+sa0.nlines;
    unsigned da_ln_l = sa0.ln_l+sa0.nlines;

    DiffArea da( da_ln_s, sa1.ln_s - da_ln_s
               , da_ln_l, sa1.ln_l - da_ln_l );

    m.diffList.push( da );
  }
  Popu_DiffList_End( m, CA );
}

void Sort_SimiList( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned SLL = m.simiList.len();

  for( unsigned k=0; k<SLL; k++ )
  {
    for( unsigned j=SLL-1; k<j; j-- )
    {
      SimLines sl0 = m.simiList[ j-1 ];
      SimLines sl1 = m.simiList[ j   ];

      if( sl1.ln_l < sl0.ln_l )
      {
        m.simiList[ j-1 ] = sl1;
        m.simiList[ j   ] = sl0;
      }
    }
  }
}

void Return_LineInfo( Diff::Data& m, LineInfo* lip )
{
  if( lip ) m.line_info_cache.push( lip );
}

void Clear_SimiList( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Return all the previously allocated LineInfo's:
  for( unsigned k=0; k<m.simiList.len(); k++ )
  {
    SimLines& siml = m.simiList[k];

    if( siml.li_s ) { Return_LineInfo( m, siml.li_s ); siml.li_s = 0; }
    if( siml.li_l ) { Return_LineInfo( m, siml.li_l ); siml.li_l = 0; }
  }
  m.simiList.clear();
}

LineInfo* Borrow_LineInfo( Diff::Data& m
                         , const char* _FILE_, const unsigned _LINE_ )
{
  Trace trace( __PRETTY_FUNCTION__ );

  LineInfo* lip = 0;

  if( m.line_info_cache.len() )
  {
    bool ok = m.line_info_cache.pop( lip );
    ASSERT( __LINE__, ok, "ok" );

    lip->clear();
  }
  else {
    lip = new(_FILE_,_LINE_) LineInfo;
  }
  return lip;
}

// Returns number of bytes that are the same between the two lines
// and fills in li_s and li_l
unsigned Compare_Lines( const Line* ls, LineInfo* li_s
                      , const Line* ll, LineInfo* li_l )
{
  Trace trace( __PRETTY_FUNCTION__ );
  if( 0==ls->len() && 0==ll->len() ) { return 1; }
  li_s->clear(); li_l->clear();
  const Line* pls = ls; LineInfo* pli_s = li_s;
  const Line* pll = ll; LineInfo* pli_l = li_l;
  if( ll->len() < ls->len() ) { pls = ll; pli_s = li_l;
                                pll = ls; pli_l = li_s; }
  const unsigned SLL = pls->len();
  const unsigned LLL = pll->len();

  pli_l->set_len( LLL );
  pli_s->set_len( LLL );

  unsigned num_same = 0;
  unsigned i_s = 0;
  unsigned i_l = 0;

  while( i_s < SLL && i_l < LLL )
  {
    const uint8_t cs = pls->get( i_s );
    const uint8_t cl = pll->get( i_l );

    if( cs == cl )
    {
      num_same++;
      (*pli_s)[ i_s++ ] = DT_SAME;
      (*pli_l)[ i_l++ ] = DT_SAME;
    }
    else {
      const unsigned remaining_s = SLL - i_s;
      const unsigned remaining_l = LLL - i_l;

      if( 0<remaining_s
       && 0<remaining_l && remaining_s == remaining_l )
      {
        (*pli_s)[ i_s++ ] = DT_CHANGED;
        (*pli_l)[ i_l++ ] = DT_CHANGED;
      }
      else if( remaining_s < remaining_l ) (*pli_l)[ i_l++ ] = DT_INSERTED;
      else if( remaining_l < remaining_s ) (*pli_s)[ i_s++ ] = DT_INSERTED;
    }
  }
  for( unsigned k=SLL; k<LLL; k++ ) (*pli_s)[k] = DT_DELETED;
  for( unsigned k=i_l; k<LLL; k++ ) (*pli_l)[k] = DT_INSERTED;

  return num_same;
}

SimLines Find_Lines_Most_Same( Diff::Data& m
                             , const DiffArea& ca, FileBuf* pfs, FileBuf* pfl )
{
  Trace trace( __PRETTY_FUNCTION__ );
  // LD = Length Difference between long area and short area
  const unsigned LD = ca.nlines_l - ca.nlines_s;

  SimLines most_same = { 0, 0, 0, 0, 0 };
  for( unsigned ln_s = ca.ln_s; ln_s<ca.fnl_s(); ln_s++ )
  {
    const unsigned ST_L = ca.ln_l+(ln_s-ca.ln_s);

    for( unsigned ln_l = ST_L; ln_l<ca.fnl_l() && ln_l<ST_L+LD+1; ln_l++ )
    {
      const Line* ls = pfs->GetLineP( ln_s ); // Line from short area
      const Line* ll = pfl->GetLineP( ln_l ); // Line from long  area

      LineInfo* li_s = Borrow_LineInfo( m, __FILE__,__LINE__);
      LineInfo* li_l = Borrow_LineInfo( m, __FILE__,__LINE__);
      const unsigned bytes_same = Compare_Lines( ls, li_s, ll, li_l );

      if( most_same.nbytes < bytes_same )
      {
        if( most_same.li_s ) { Return_LineInfo( m, most_same.li_s ); }
        if( most_same.li_l ) { Return_LineInfo( m, most_same.li_l ); }
        most_same.ln_s   = ln_s;
        most_same.ln_l   = ln_l;
        most_same.nbytes = bytes_same;
        most_same.li_s   = li_s; // Hand off li_s
        most_same.li_l   = li_l; // and      li_l
      }
      else {
        Return_LineInfo( m, li_s );
        Return_LineInfo( m, li_l );
      }
    }
  }
  if( 0==most_same.nbytes )
  {
    // This if() block ensures that each line in the short DiffArea is
    // matched to a line in the long DiffArea.  Each line in the short
    // DiffArea must be matched to a line in the long DiffArea or else
    // SimiList_2_DI_Lists wont work right.
    most_same.ln_s   = ca.ln_s;
    most_same.ln_l   = ca.ln_l;
    most_same.nbytes = 1;
  }
  return most_same;
}

void Popu_SimiList( Diff::Data& m
                  , const unsigned da_ln_s
                  , const unsigned da_ln_l
                  , const unsigned da_nlines_s
                  , const unsigned da_nlines_l
                  , FileBuf* pfs
                  , FileBuf* pfl )
{
  Trace trace( __PRETTY_FUNCTION__ );
  Clear_SimiList(m);

  if( da_nlines_s && da_nlines_l )
  {
    DiffArea ca( da_ln_s, da_nlines_s, da_ln_l, da_nlines_l );

    Array_t<DiffArea> compList;
                      compList.push( ca );

    while( compList.pop( ca ) )
    {
      SimLines siml = Find_Lines_Most_Same( m, ca, pfs, pfl );

      if( m.simiList.len() == da_nlines_s )
      {
        // Not putting siml into simiList, so delete any new'ed memory:
        MemMark(__FILE__,__LINE__); delete siml.li_s; siml.li_s = 0;
        MemMark(__FILE__,__LINE__); delete siml.li_l; siml.li_l = 0;
        return;
      }
      m.simiList.push( siml );
      if( ( siml.ln_s == ca.ln_s || siml.ln_l == ca.ln_l )
       && siml.ln_s+1 < ca.fnl_s()
       && siml.ln_l+1 < ca.fnl_l() )
      {
        // Only one new DiffArea after siml:
        DiffArea ca1( siml.ln_s+1, ca.fnl_s()-siml.ln_s-1
                    , siml.ln_l+1, ca.fnl_l()-siml.ln_l-1 );
        compList.push( ca1 );
      }
      else if( ( siml.ln_s+1 == ca.fnl_s() || siml.ln_l+1 == ca.fnl_l() )
            && ca.ln_s < siml.ln_s
            && ca.ln_l < siml.ln_l )
      {
        // Only one new DiffArea before siml:
        DiffArea ca1( ca.ln_s, siml.ln_s-ca.ln_s
                    , ca.ln_l, siml.ln_l-ca.ln_l );
        compList.push( ca1 );
      }
      else if( ca.ln_s < siml.ln_s && siml.ln_s+1 < ca.fnl_s()
            && ca.ln_l < siml.ln_l && siml.ln_l+1 < ca.fnl_l() )
      {
        // Two new DiffArea's, one before siml, and one after siml:
        DiffArea ca1( ca.ln_s, siml.ln_s-ca.ln_s
                    , ca.ln_l, siml.ln_l-ca.ln_l );
        DiffArea ca2( siml.ln_s+1, ca.fnl_s()-siml.ln_s-1
                    , siml.ln_l+1, ca.fnl_l()-siml.ln_l-1 );
        compList.push( ca1 );
        compList.push( ca2 );
      }
    }
  }
}

void Insert_DI_List( Diff::Data& m
                   , const Diff_Info& di
                   , Array_t<Diff_Info>& DI_List )
{
  bool ok = DI_List.insert( m.DI_L_ins_idx, di );
  ASSERT( __LINE__, ok, "ok" );
}

void SimiList_2_DI_Lists( Diff::Data& m
                        , const unsigned da_ln_s
                        , const unsigned da_ln_l
                        , const unsigned da_nlines_s
                        , const unsigned da_nlines_l
                        , Array_t<Diff_Info>& DI_List_s
                        , Array_t<Diff_Info>& DI_List_l )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Diff info short line number:
  unsigned dis_ln = da_ln_s ? da_ln_s-1 : 0;

  for( unsigned k=0; k<da_nlines_l; k++ )
  {
    Diff_Info dis = { DT_DELETED , dis_ln    };
    Diff_Info dil = { DT_INSERTED, da_ln_l+k };

    for( unsigned j=0; j<m.simiList.len(); j++ )
    {
      SimLines& siml = m.simiList[ j ];

      if( siml.ln_l == da_ln_l+k )
      {
        dis.diff_type = DT_CHANGED;
        dis.line_num  = siml.ln_s;
        dis.pLineInfo = siml.li_s; siml.li_s = 0; // Transfer ownership of LineInfo from siml to dis

        dil.diff_type = DT_CHANGED;
        dil.pLineInfo = siml.li_l; siml.li_l = 0; // Transfer ownership of LineInfo from siml to dil

        dis_ln = dis.line_num;
        break;
      }
    }
    // DI_List_s and DI_List_l now own LineInfo objects:
    Insert_DI_List( m, dis, DI_List_s );
    Insert_DI_List( m, dil, DI_List_l ); m.DI_L_ins_idx++;
  }
}

void Popu_DI_List_AddDiff_Common( Diff::Data& m
                                , const unsigned da_ln_s
                                , const unsigned da_ln_l
                                , const unsigned da_nlines_s
                                , const unsigned da_nlines_l
                                , Array_t<Diff_Info>& DI_List_s
                                , Array_t<Diff_Info>& DI_List_l
                                , FileBuf* pfs
                                , FileBuf* pfl )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Popu_SimiList( m, da_ln_s
                  , da_ln_l
                  , da_nlines_s
                  , da_nlines_l
                  , pfs
                  , pfl );
  Sort_SimiList(m);
//PrintSimiList();

  SimiList_2_DI_Lists( m, da_ln_s
                        , da_ln_l
                        , da_nlines_s
                        , da_nlines_l
                        , DI_List_s
                        , DI_List_l );
}

void Popu_DI_List_AddDiff( Diff::Data& m, const DiffArea& da )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( da.nlines_s < da.nlines_l )
  {
    Popu_DI_List_AddDiff_Common( m, da.ln_s
                                  , da.ln_l
                                  , da.nlines_s
                                  , da.nlines_l
                                  , m.DI_List_S
                                  , m.DI_List_L
                                  , m.pfS, m.pfL );
  }
  else if( da.nlines_l < da.nlines_s )
  {
    Popu_DI_List_AddDiff_Common( m, da.ln_l
                                  , da.ln_s
                                  , da.nlines_l
                                  , da.nlines_s
                                  , m.DI_List_L
                                  , m.DI_List_S
                                  , m.pfL, m.pfS );
  }
  else // da.nlines_s == da.nlines_l
  {
    for( unsigned k=0; k<da.nlines_l; k++ )
    {
      const Line* ls = m.pfS->GetLineP( da.ln_s+k );
      const Line* ll = m.pfL->GetLineP( da.ln_l+k );

      LineInfo* li_s = Borrow_LineInfo( m, __FILE__,__LINE__);
      LineInfo* li_l = Borrow_LineInfo( m, __FILE__,__LINE__);

      unsigned bytes_same = Compare_Lines( ls, li_s, ll, li_l );

      Diff_Info dis = { DT_CHANGED, da.ln_s+k, li_s };
      Diff_Info dil = { DT_CHANGED, da.ln_l+k, li_l };
      Insert_DI_List( m, dis, m.DI_List_S );
      Insert_DI_List( m, dil, m.DI_List_L ); m.DI_L_ins_idx++;
    }
  }
}

void Popu_DI_List_NoSameArea( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Should only be one DiffArea, which is the whole DiffArea:
  const unsigned DLL = m.diffList.len();
  ASSERT( __LINE__, DLL==1, "DLL==1" );

  Popu_DI_List_AddDiff( m, m.diffList[0] );
}

//void Clear_DI_List_CA( Diff::Data& m
//                     , const unsigned st_line
//                     , const unsigned fn_line
//                     , Array_t<Diff_Info>& DI_List )
//{
//  Trace trace( __PRETTY_FUNCTION__ );
//
//  bool     found_first = false;
//  unsigned first_to_remove = 0;
//  unsigned num___to_remove = 0;
//
//  // Since, Clear_DI_List_CA will only be called when DI_List is
//  // fully populated, the Diff_Info.line_num's will be at indexes
//  // greater than or equal to st_line
//  for( unsigned k=st_line; k<DI_List.len(); k++ )
//  {
//    Diff_Info& di = DI_List[k];
//
//    if( st_line <= di.line_num && di.line_num < fn_line )
//    {
//      if( di.pLineInfo )
//      {
//        // Return all the previously allocated LineInfo's:
//        Return_LineInfo( m, di.pLineInfo ); di.pLineInfo = 0;
//      }
//      if( !found_first )
//      {
//        found_first = true;
//        first_to_remove = k;
//      }
//      num___to_remove++;
//    }
//    else if( fn_line <= di.line_num )
//    {
//      // Past the range of line_num's we want to remove
//      break;
//    }
//  }
//  DI_List.remove_n( first_to_remove, num___to_remove );
//}

// Returns true if the two lines, line_s and line_l, in the two files
// being compared, are the names of files that differ
//bool Popu_DI_List_Have_Diff_Files( Diff::Data& m
//                                 , const unsigned line_s
//                                 , const unsigned line_l )
//{
//  bool files_differ = false;
//
//  if( m.pfS->IsDir() && m.pfL->IsDir() )
//  {
//    // fname_s and fname_l are head names
//    String fname_s = m.pfS->GetLine( line_s ).toString();
//    String fname_l = m.pfL->GetLine( line_l ).toString();
//
//    if( (fname_s != "..") && !fname_s.ends_with( DirDelimStr() )
//     && (fname_l != "..") && !fname_l.ends_with( DirDelimStr() ) )
//    {
//      // fname_s and fname_l should now be full path names,
//      // tail and head, of regular files
//      fname_s.insert( 0, m.pfS->GetDirName() );
//      fname_l.insert( 0, m.pfL->GetDirName() );
//
//      FileBuf* pfb_s = m.vis.GetFileBuf( fname_s );
//      FileBuf* pfb_l = m.vis.GetFileBuf( fname_l );
//
//      if( (0 != pfb_s) && (0 != pfb_l) )
//      {
//        // Fast: Compare files already cached in memory:
//        files_differ = !Files_Are_Same( *pfb_s, *pfb_l );
//      }
//      else {
//        // Slow: Compare the files in NVM:
//        files_differ = !Files_Are_Same( fname_s.c_str(), fname_l.c_str() );
//      }
//    }
//  }
//  return files_differ;
//}

// Returns true if the two lines, line_s and line_l, in the two files
// being compared, are the names of files that differ
//bool Popu_DI_List_Have_Diff_Files( Diff::Data& m
//                                 , const unsigned line_s
//                                 , const unsigned line_l )
//{
//  bool files_differ = false;
//
//  if( m.pfS->IsDir() && m.pfL->IsDir() )
//  {
//    // fname_s and fname_l are head names
//    String fname_s = m.pfS->GetLineP( line_s )->toString();
//    String fname_l = m.pfL->GetLineP( line_l )->toString();
//
//    if( (fname_s != "..") && !fname_s.ends_with( DirDelimStr() )
//     && (fname_l != "..") && !fname_l.ends_with( DirDelimStr() ) )
//    {
//      // After prepending the directory names, fname_s and fname_l
//      // should be full path names, tail and head, of regular files
//      fname_s.insert( 0, m.pfS->GetDirName() );
//      fname_l.insert( 0, m.pfL->GetDirName() );
//
//      // Add files if they have not been added:
//      m.vis.NotHaveFileAddFile( fname_s );
//      m.vis.NotHaveFileAddFile( fname_l );
//
//      FileBuf* pfb_s = m.vis.GetFileBuf( fname_s );
//      FileBuf* pfb_l = m.vis.GetFileBuf( fname_l );
//
//      if( (0 != pfb_s) && (0 != pfb_l) )
//      {
//        // Fast: Compare files already cached in memory:
//        files_differ = !Files_Are_Same( *pfb_s, *pfb_l );
//      }
//      else {
//        // Slow: Compare the files in NVM:
//        files_differ = !Files_Are_Same( fname_s.c_str(), fname_l.c_str() );
//      }
//    }
//  }
//  return files_differ;
//}

// Returns true if the two lines, line_s and line_l, in the two files
// being compared, are the names of files that differ
bool Popu_DI_List_Have_Diff_Files( Diff::Data& m
                                 , const unsigned line_s
                                 , const unsigned line_l )
{
  bool files_differ = false;

  if( m.pfS->IsDir() && m.pfL->IsDir() )
  {
    // fname_s and fname_l are head names
    String fname_s = m.pfS->GetLine( line_s ).toString();
    String fname_l = m.pfL->GetLine( line_l ).toString();

    if( (fname_s != "..") && !fname_s.ends_with( DirDelimStr() )
     && (fname_l != "..") && !fname_l.ends_with( DirDelimStr() ) )
    {
      // fname_s and fname_l should now be full path names,
      // tail and head, of regular files
      fname_s.insert( 0, m.pfS->GetDirName() );
      fname_l.insert( 0, m.pfL->GetDirName() );

      FileBuf* pfb_s = m.vis.GetFileBuf( fname_s );
      FileBuf* pfb_l = m.vis.GetFileBuf( fname_l );

      // If one side is in ram, read in the other side:
      if     ( (0 == pfb_s) && (0 != pfb_l) ) m.vis.NotHaveFileAddFile( fname_s );
      else if( (0 != pfb_s) && (0 == pfb_l) ) m.vis.NotHaveFileAddFile( fname_l );
      else if( (0 == pfb_s) && (0 == pfb_l) )
      {
        // Adding files is slow because of all the new'ing, so limit
        // the number of files that can be added per diff:
        if( m.num_files_added_this_diff < m.max_files_added_per_diff )
        {
          bool added_s = m.vis.NotHaveFileAddFile( fname_s );
          bool added_l = m.vis.NotHaveFileAddFile( fname_l );

          if( added_s ) m.num_files_added_this_diff++;
          if( added_l ) m.num_files_added_this_diff++;
        }
      }
      pfb_s = m.vis.GetFileBuf( fname_s );
      pfb_l = m.vis.GetFileBuf( fname_l );

      if( (0 == pfb_s) || (0 == pfb_l) )
      {
        // Slow: Compare the files in NVM:
        files_differ = !Files_Are_Same( fname_s.c_str(), fname_l.c_str() );
      }
      else {
        // Fast: Compare files already cached in memory:
        files_differ = !Files_Are_Same( *pfb_s, *pfb_l );
      }
    }
  }
  return files_differ;
}

void Popu_DI_List_AddSame( Diff::Data& m, const SameArea& sa )
{
  Trace trace( __PRETTY_FUNCTION__ );

  for( unsigned k=0; k<sa.nlines; k++ )
  {
    const Diff_Type DT = Popu_DI_List_Have_Diff_Files( m, sa.ln_s+k, sa.ln_l+k )
                       ? DT_DIFF_FILES
                       : DT_SAME;

    Diff_Info dis = { DT, sa.ln_s+k };
    Diff_Info dil = { DT, sa.ln_l+k };

    Insert_DI_List( m, dis, m.DI_List_S );
    Insert_DI_List( m, dil, m.DI_List_L ); m.DI_L_ins_idx++;
  }
}

void Popu_DI_List_NoDiffArea( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Should only be one SameArea, which is the whole DiffArea:
  const unsigned SLL = m.sameList.len();
  ASSERT( __LINE__, SLL==1, "SLL==1" );

  Popu_DI_List_AddSame( m, m.sameList[0] );
}

void Popu_DI_List_DiffAndSame( Diff::Data& m, const DiffArea& CA )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned SLL = m.sameList.len();
  const unsigned DLL = m.diffList.len();

  DiffArea da = m.diffList[ 0 ];

  if( CA.ln_s==da.ln_s && CA.ln_l==da.ln_l )
  {
    // Start with DiffArea, and then alternate between SameArea and DiffArea.
    // There should be at least as many DiffArea's as SameArea's.
    ASSERT( __LINE__, SLL<=DLL, "SLL<=DLL" );

    for( unsigned k=0; k<SLL; k++ )
    {
      DiffArea da = m.diffList[ k ]; Popu_DI_List_AddDiff( m, da );
      SameArea sa = m.sameList[ k ]; Popu_DI_List_AddSame( m, sa );
    }
    if( SLL < DLL )
    {
      ASSERT( __LINE__, SLL+1==DLL, "SLL+1==DLL" );
      DiffArea da = m.diffList[ DLL-1 ]; Popu_DI_List_AddDiff( m, da );
    }
  }
  else {
    // Start with SameArea, and then alternate between DiffArea and SameArea.
    // There should be at least as many SameArea's as DiffArea's.
    ASSERT( __LINE__, DLL<=SLL, "DLL<=SLL" );

    for( unsigned k=0; k<DLL; k++ )
    {
      SameArea sa = m.sameList[ k ]; Popu_DI_List_AddSame( m, sa );
      DiffArea da = m.diffList[ k ]; Popu_DI_List_AddDiff( m, da );
    }
    if( DLL < SLL )
    {
      ASSERT( __LINE__, DLL+1==SLL, "DLL+1==SLL" );
      SameArea sa = m.sameList[ SLL-1 ]; Popu_DI_List_AddSame( m, sa );
    }
  }
}

void Popu_DI_List( Diff::Data& m, const DiffArea& CA )
{
  Trace trace( __PRETTY_FUNCTION__ );

//Clear_DI_List_CA( m, CA.ln_s, CA.fnl_s(), m.DI_List_S );
//Clear_DI_List_CA( m, CA.ln_l, CA.fnl_l(), m.DI_List_L );

  const unsigned SLL = m.sameList.len();
  const unsigned DLL = m.diffList.len();

  if     ( SLL == 0 ) Popu_DI_List_NoSameArea( m );
  else if( DLL == 0 ) Popu_DI_List_NoDiffArea( m );
  else                Popu_DI_List_DiffAndSame( m, CA );
}

void RunDiff( Diff::Data& m, const DiffArea& CA )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const double t1_s = GetTimeSeconds();

  Popu_SameList( m, CA );
  Sort_SameList( m );
//PrintSameList();
  Popu_DiffList( m, CA );
//PrintDiffList();
  Popu_DI_List( m, CA );
//PrintDI_List( CA );

  const double t2_s = GetTimeSeconds();
  m.diff_ms = (t2_s - t1_s)*1000 + 0.5;
  m.printed_diff_ms = false;
}

unsigned NumLines( Diff::Data& m )
{
  // DI_List_L and DI_List_S should be the same length
  return m.DI_List_L.len();
}

unsigned CrsLine( Diff::Data& m )
{
  return m.topLine  + m.crsRow;
}
unsigned CrsChar( Diff::Data& m )
{
  return m.leftChar + m.crsCol;
}

bool Do_n_Search_for_Same( Diff::Data& m
                         , unsigned& dl
                         , const Array_t<Diff_Info>& DI_List )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = NumLines(m);
  const unsigned dl_st = dl;

  // Search forward for DT_SAME
  bool found = false;

  if( 1 < NUM_LINES )
  {
    while( !found && dl<NUM_LINES )
    {
      const Diff_Type DT = DI_List[dl].diff_type;

      if( DT == DT_SAME )
      {
        found = true;
      }
      else dl++;
    }
    if( !found )
    {
      // Wrap around back to top and search again:
      dl = 0;
      while( !found && dl<dl_st )
      {
        const Diff_Type DT = DI_List[dl].diff_type;

        if( DT == DT_SAME )
        {
          found = true;
        }
        else dl++;
      }
    }
  }
  return found;
}

// Look for difference based on Diff_Info:
bool Do_n_Search_for_Diff_DT( Diff::Data& m
                            , unsigned& dl
                            , const Array_t<Diff_Info>& DI_List )
{
  bool found_diff = false;

  const unsigned NUM_LINES = NumLines(m);
  const unsigned dl_st = dl;

  while( !found_diff && dl<NUM_LINES )
  {
    const Diff_Type DT = DI_List[dl].diff_type;

    if( DT == DT_CHANGED
     || DT == DT_INSERTED
     || DT == DT_DELETED
     || DT == DT_DIFF_FILES )
    {
      found_diff = true;
    }
    else dl++;
  }
  if( !found_diff )
  {
    // Wrap around back to top and search again:
    dl = 0;
    while( !found_diff && dl<dl_st )
    {
      const Diff_Type DT = DI_List[dl].diff_type;

      if( DT == DT_CHANGED
       || DT == DT_INSERTED
       || DT == DT_DELETED
       || DT == DT_DIFF_FILES )
      {
        found_diff = true;
      }
      else dl++;
    }
  }
  return found_diff;
}

bool
Line_Has_Leading_or_Trailing_WS_Diff( unsigned& dl
                                    , const unsigned k
                                    , const Array_t<Diff_Info>& DI_List
                                    , const Array_t<Diff_Info>& DI_List_o
                                    , const FileBuf* pF_m
                                    , const FileBuf* pF_o )
{
  bool L_T_WS_diff = false;

  const Diff_Info& Di_m = DI_List[ k ];
  const Diff_Info& Di_o = DI_List_o[ k ];

  if( Di_m.diff_type == DT_SAME
   && Di_o.diff_type == DT_SAME )
  {
    const Line* lm = pF_m->GetLineP( Di_m.line_num ); // Line from my    view
    const Line* lo = pF_o->GetLineP( Di_o.line_num ); // Line from other view

    if( lm->len() != lo->len() )
    {
      L_T_WS_diff = true;
      dl = k;
    }
  }
  return L_T_WS_diff;
}

// Look for difference in white space at beginning or ending of lines:
bool Do_n_Search_for_Diff_WhiteSpace( Diff::Data& m
                                    , unsigned& dl
                                    , const Array_t<Diff_Info>& DI_List )
{
  bool found_diff = false;

  const unsigned NUM_LINES = NumLines(m);

  Array_t<Diff_Info>& DI_List_o = (&DI_List == &m.DI_List_S) ? m.DI_List_L : m.DI_List_S;
  FileBuf* pF_m = (&DI_List == &m.DI_List_S) ? m.pfS : m.pfL;
  FileBuf* pF_o = (&DI_List == &m.DI_List_S) ? m.pfL : m.pfS;

  // If the current line has a difference in white space at beginning or end, start
  // searching on next line so the current line number is not automatically returned.
  bool curr_line_has_LT_WS_diff
    = Line_Has_Leading_or_Trailing_WS_Diff( dl, dl
                                          , DI_List, DI_List_o
                                          , pF_m, pF_o );
  const unsigned dl_st = curr_line_has_LT_WS_diff
                       ? (dl + 1) % NUM_LINES
                       : dl;

  // Search from dl_st to end for lines of different length:
  for( unsigned k=dl_st; !found_diff && k<NUM_LINES; k++ )
  {
    found_diff = Line_Has_Leading_or_Trailing_WS_Diff( dl, k
                                                     , DI_List, DI_List_o
                                                     , pF_m, pF_o );
  }
  if( !found_diff )
  {
    // Search from top to dl_st for lines of different length:
    for( unsigned k=0; !found_diff && k<dl_st; k++ )
    {
      found_diff = Line_Has_Leading_or_Trailing_WS_Diff( dl, k
                                                       , DI_List, DI_List_o
                                                       , pF_m, pF_o );
    }
  }
  return found_diff;
}

bool Do_n_Search_for_Diff( Diff::Data& m
                         , unsigned& dl
                         , const Array_t<Diff_Info>& DI_List )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned dl_st = dl;

  // Search forward for non-DT_SAME
  bool found_diff = false;

  if( 1 < NumLines(m) )
  {
    found_diff = Do_n_Search_for_Diff_DT( m, dl, DI_List );

    if( !found_diff )
    {
      dl = dl_st;
      found_diff = Do_n_Search_for_Diff_WhiteSpace( m, dl, DI_List );
    }
  }
  return found_diff;
}

unsigned Do_n_Find_Crs_Pos( Diff::Data& m
                          , const unsigned NCL
                          , const Array_t<Diff_Info>& DI_List )
{
  unsigned NCP = 0;

  const Diff_Type DT_new = DI_List[ NCL ].diff_type;

  if( DT_new == DT_CHANGED )
  {
    LineInfo* pLI_s = m.DI_List_S[ NCL ].pLineInfo;
    LineInfo* pLI_l = m.DI_List_L[ NCL ].pLineInfo;

    for( unsigned k=0; 0 != pLI_s && k<pLI_s->len()
                    && 0 != pLI_l && k<pLI_l->len(); k++ )
    {
      Diff_Type dt_s = (*pLI_s)[ k ];
      Diff_Type dt_l = (*pLI_l)[ k ];

      if( dt_s != DT_SAME
       || dt_l != DT_SAME )
      {
        NCP = k;
        break;
      }
    }
  }
  return NCP;
}

unsigned WorkingRows( View* pV )
{
  return pV->WinRows() -5 ;
}

unsigned WorkingCols( View* pV )
{
  return pV->WinCols() -2 ;
}

unsigned BotLine( Diff::Data& m, View* pV )
{
  return m.topLine + WorkingRows( pV )-1;
}

unsigned RightChar( Diff::Data& m, View* pV )
{
  return m.leftChar + WorkingCols( pV )-1;
}

unsigned ViewLine( Diff::Data& m, const View* pV, const unsigned diff_line )
{
  Trace trace( __PRETTY_FUNCTION__ );

  return ( pV == m.pvS ) ? m.DI_List_S[ diff_line ].line_num
                         : m.DI_List_L[ diff_line ].line_num;
}

bool InVisualBlock( Diff::Data& m, const unsigned DL, const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  return ( m.v_st_line <= DL  && DL  <= m.v_fn_line
        && m.v_st_char <= pos && pos <= m.v_fn_char ) // bot rite
      || ( m.v_st_line <= DL  && DL  <= m.v_fn_line
        && m.v_fn_char <= pos && pos <= m.v_st_char ) // bot left
      || ( m.v_fn_line <= DL  && DL  <= m.v_st_line
        && m.v_st_char <= pos && pos <= m.v_fn_char ) // top rite
      || ( m.v_fn_line <= DL  && DL  <= m.v_st_line
        && m.v_fn_char <= pos && pos <= m.v_st_char );// top left
}

bool InVisualStFn( Diff::Data& m, const unsigned DL, const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.inVisualMode ) return false;

  if( m.v_st_line == DL && DL == m.v_fn_line )
  {
    return (m.v_st_char <= pos && pos <= m.v_fn_char)
        || (m.v_fn_char <= pos && pos <= m.v_st_char);
  }
  else if( (m.v_st_line < DL && DL < m.v_fn_line)
        || (m.v_fn_line < DL && DL < m.v_st_line) )
  {
    return true;
  }
  else if( m.v_st_line == DL && DL < m.v_fn_line )
  {
    return m.v_st_char <= pos;
  }
  else if( m.v_fn_line == DL && DL < m.v_st_line )
  {
    return m.v_fn_char <= pos;
  }
  else if( m.v_st_line < DL && DL == m.v_fn_line )
  {
    return pos <= m.v_fn_char;
  }
  else if( m.v_fn_line < DL && DL == m.v_st_line )
  {
    return pos <= m.v_st_char;
  }
  return false;
}

bool InVisualArea( Diff::Data& m
                 , View* pV
                 , const unsigned DL
                 , const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Only one diff view, current view, can be in visual mode.
  if( m.vis.CV() == pV && m.inVisualMode )
  {
    if( m.inVisualBlock ) return InVisualBlock( m, DL, pos );
    else                  return InVisualStFn ( m, DL, pos );
  }
  return false;
}

Style Get_Style( Diff::Data& m
               , View* pV
               , const unsigned DL
               , const unsigned VL
               , const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Style S = S_EMPTY;

  FileBuf* pfb = pV->GetFB();

  if( VL < pfb->NumLines() && pos < pfb->LineLen( VL ) )
  {
    S = S_NORMAL;

    if     ( InVisualArea( m, pV, DL, pos ) ) S = S_RV_VISUAL;
    else if( pV->InStar         ( VL, pos ) ) S = S_STAR;
    else if( pV->InStarInF      ( VL, pos ) ) S = S_STAR_IN_F;
    else if( pV->InDefine       ( VL, pos ) ) S = S_DEFINE;
    else if( pV->InComment      ( VL, pos ) ) S = S_COMMENT;
    else if( pV->InConst        ( VL, pos ) ) S = S_CONST;
    else if( pV->InControl      ( VL, pos ) ) S = S_CONTROL;
    else if( pV->InVarType      ( VL, pos ) ) S = S_VARTYPE;
  }
  return S;
}

// Translates zero based file line number to zero based global row
unsigned Line_2_GL( Diff::Data& m, View* pV, const unsigned file_line )
{
  return pV->Y() + 1 + file_line - m.topLine;
}

// Translates zero based file line char position to zero based global column
unsigned Char_2_GL( Diff::Data& m, View* pV, const unsigned line_char )
{
  return pV->X() + 1 + line_char - m.leftChar;
}

void GoToCrsPos_Write_VisualBlock( Diff::Data& m
                                 , const int OCL
                                 , const int OCP
                                 , const int NCL
                                 , const int NCP )
{
  Trace trace( __PRETTY_FUNCTION__ );
  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();
  // m.v_fn_line == NCL && m.v_fn_char == NCP, so dont need to include
  // m.v_fn_line       and m.v_fn_char in Min and Max calls below:
  const int vis_box_left = Min( m.v_st_char, Min( OCP, NCP ) );
  const int vis_box_rite = Max( m.v_st_char, Max( OCP, NCP ) );
  const int vis_box_top  = Min( m.v_st_line, Min( OCL, NCL ) );
  const int vis_box_bot  = Max( m.v_st_line, Max( OCL, NCL ) );

  const int draw_box_left = Max( m.leftChar     , vis_box_left );
  const int draw_box_rite = Min( RightChar(m,pV), vis_box_rite );
  const int draw_box_top  = Max( m.topLine      , vis_box_top  );
  const int draw_box_bot  = Min( BotLine(m,pV)  , vis_box_bot  );

  for( int DL=draw_box_top; DL<=draw_box_bot; DL++ )
  {
    const int VL = ViewLine( m, pV, DL ); // View line number

    const int LL = pfb->LineLen( VL );

    for( int k=draw_box_left; k<LL && k<=draw_box_rite; k++ )
    {
      const char  C = pfb->Get( VL, k );
      const Style S = Get_Style( m, pV, DL, VL, k );

      Console::Set( Line_2_GL( m, pV, DL ), Char_2_GL( m, pV, k ), C, S );
    }
  }
  m.crsRow = NCL - m.topLine;
  m.crsCol = NCP - m.leftChar;
  Console::Update();
  m.diff.PrintCursor( pV ); // Does Console::Update()
  m.sts_line_needs_update = true;
}

Diff_Type DiffType( Diff::Data& m, const View* pV, const unsigned diff_line )
{
  Trace trace( __PRETTY_FUNCTION__ );

  return ( pV == m.pvS ) ? m.DI_List_S[ diff_line ].diff_type
                         : m.DI_List_L[ diff_line ].diff_type;
}

// Cursor is moving forward
// Write out from (OCL,OCP) up to but not including (NCL,NCP)
void GoToCrsPos_WV_Forward( Diff::Data& m
                          , const unsigned OCL, const unsigned OCP
                          , const unsigned NCL, const unsigned NCP )
{
  Trace trace( __PRETTY_FUNCTION__ );
  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();
  // Convert OCL and NCL, which are diff lines, to view lines:
  const unsigned OCLv = ViewLine( m, pV, OCL );
  const unsigned NCLv = ViewLine( m, pV, NCL );

  if( OCL == NCL ) // Only one line:
  {
    for( unsigned k=OCP; k<NCP; k++ )
    {
      const char  C = pfb->Get( OCLv, k );
      const Style S = Get_Style(m,pV,OCL,OCLv,k);
      Console::Set( Line_2_GL( m, pV, OCL ), Char_2_GL( m, pV, k ), C, S );
    }
  }
  else { // Multiple lines
    // Write out first line:
    const unsigned FIRST_LINE_DIFF_TYPE = DiffType( m, pV, OCL );
    if( FIRST_LINE_DIFF_TYPE != DT_DELETED )
    {
      const unsigned OCLL = pfb->LineLen( OCLv ); // Old cursor line length
      const unsigned END_FIRST_LINE = Min( RightChar( m, pV )+1, OCLL );
      for( unsigned k=OCP; k<END_FIRST_LINE; k++ )
      {
        const char  C = pfb->Get( OCLv, k );
        const Style S = Get_Style(m,pV,OCL,OCLv,k);
        Console::Set( Line_2_GL( m, pV, OCL ), Char_2_GL( m, pV, k ), C, S );
      }
    }
    // Write out intermediate lines:
    for( unsigned l=OCL+1; l<NCL; l++ )
    {
      const unsigned LINE_DIFF_TYPE = DiffType( m, pV, l );
      if( LINE_DIFF_TYPE != DT_DELETED )
      {
        // Convert OCL, which is diff line, to view line
        const unsigned Vl = ViewLine( m, pV, l );
        const unsigned LL = pfb->LineLen( Vl ); // Line length
        const unsigned END_OF_LINE = Min( RightChar( m, pV )+1, LL );
        for( unsigned k=m.leftChar; k<END_OF_LINE; k++ )
        {
          const char  C = pfb->Get( Vl, k );
          const Style S = Get_Style(m,pV,l,Vl,k);
          Console::Set( Line_2_GL( m, pV, l ), Char_2_GL( m, pV, k ), C, S );
        }
      }
    }
    // Write out last line:
    const unsigned LAST_LINE_DIFF_TYPE = DiffType( m, pV, NCL );
    if( LAST_LINE_DIFF_TYPE != DT_DELETED )
    {
      // Print from beginning of next line to new cursor position:
      const unsigned NCLL = pfb->LineLen( NCLv ); // Line length
      const unsigned END_LAST_LINE = Min( NCLL, NCP );
      for( unsigned k=m.leftChar; k<END_LAST_LINE; k++ )
      {
        const char  C = pfb->Get( NCLv, k );
        const Style S = Get_Style(m,pV,NCL,NCLv,k);
        Console::Set( Line_2_GL( m, pV, NCL ), Char_2_GL( m, pV, k ), C, S );
      }
    }
  }
}

// Cursor is moving backwards
// Write out from (OCL,OCP) back to but not including (NCL,NCP)
void GoToCrsPos_WV_Backward( Diff::Data& m
                           , const unsigned OCL, const unsigned OCP
                           , const unsigned NCL, const unsigned NCP )
{
  Trace trace( __PRETTY_FUNCTION__ );
  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();
  // Convert OCL and NCL, which are diff lines, to view lines:
  const unsigned OCLv = ViewLine( m, pV, OCL );
  const unsigned NCLv = ViewLine( m, pV, NCL );

  if( OCL == NCL ) // Only one line:
  {
    for( unsigned k=OCP; NCP<k; k-- )
    {
      const char  C = pfb->Get( OCLv, k );
      const Style S = Get_Style(m,pV,OCL,OCLv,k);
      Console::Set( Line_2_GL( m, pV, OCL ), Char_2_GL( m, pV, k ), C, S );
    }
  }
  else { // Multiple lines
    // Write out first line:
    const int FIRST_LINE_DIFF_TYPE = DiffType( m, pV, OCL );
    if( FIRST_LINE_DIFF_TYPE != DT_DELETED )
    {
      const unsigned OCLL = pfb->LineLen( OCLv ); // Old cursor line length
      const unsigned RIGHT_MOST_POS = Min( OCP, 0<OCLL ? OCLL-1 : 0 );
      for( unsigned k=RIGHT_MOST_POS; m.leftChar<k; k-- )
      {
        const char  C = pfb->Get( OCLv, k );
        const Style S = Get_Style(m,pV,OCL,OCLv,k);
        Console::Set( Line_2_GL( m, pV, OCL ), Char_2_GL( m, pV, k ), C, S );
      }
      if( m.leftChar < OCLL ) {
        const char  C = pfb->Get( OCLv, m.leftChar );
        const Style S = Get_Style(m,pV,OCL,OCLv,m.leftChar);
        Console::Set( Line_2_GL( m, pV, OCL ), Char_2_GL( m, pV, m.leftChar ), C, S );
      }
    }
    // Write out intermediate lines:
    for( unsigned l=OCL-1; NCL<l; l-- )
    {
      const int LINE_DIFF_TYPE = DiffType( m, pV, l );
      if( LINE_DIFF_TYPE != DT_DELETED )
      {
        // Convert l, which is diff line, to view line:
        const unsigned Vl = ViewLine( m, pV, l );
        const unsigned LL = pfb->LineLen( Vl ); // Line length
        const unsigned END_OF_LINE = Min( RightChar( m, pV ), 0<LL ? LL-1 : 0 );
        for( unsigned k=END_OF_LINE; m.leftChar<k; k-- )
        {
          const char  C = pfb->Get( Vl, k );
          const Style S = Get_Style(m,pV,l,Vl,k);
          Console::Set( Line_2_GL( m, pV, l ), Char_2_GL( m, pV, k ), C, S );
        }
      }
    }
    // Write out last line:
    const int LAST_LINE_DIFF_TYPE = DiffType( m, pV, NCL );
    if( LAST_LINE_DIFF_TYPE != DT_DELETED )
    {
      // Print from end of last line to new cursor position:
      const unsigned NCLL = pfb->LineLen( NCLv ); // New cursor line length
      const unsigned END_LAST_LINE = Min( RightChar( m, pV ), 0<NCLL ? NCLL-1 : 0 );
      for( unsigned k=END_LAST_LINE; NCP<k; k-- )
      {
        const char  C = pfb->Get( NCLv, k );
        const Style S = Get_Style(m,pV,NCL,NCLv,k);
        Console::Set( Line_2_GL( m, pV, NCL ), Char_2_GL( m, pV, k ), C, S );
      }
      if( NCP < NCLL ) {
        const char  C = pfb->Get( NCLv, NCP );
        const Style S = Get_Style(m,pV,NCL,NCLv,NCP);
        Console::Set( Line_2_GL( m, pV, NCL ), Char_2_GL( m, pV, NCP ), C, S );
      }
    }
  }
}

void GoToCrsPos_Write_Visual( Diff::Data& m
                            , const unsigned OCL, const unsigned OCP
                            , const unsigned NCL, const unsigned NCP )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // (old cursor pos) < (new cursor pos)
  const bool OCP_LT_NCP = OCL < NCL || (OCL == NCL && OCP < NCP);

  if( OCP_LT_NCP ) // Cursor moved forward
  {
    GoToCrsPos_WV_Forward( m, OCL, OCP, NCL, NCP );
  }
  else // NCP_LT_OCP // Cursor moved backward
  {
    GoToCrsPos_WV_Backward( m, OCL, OCP, NCL, NCP );
  }
  m.crsRow = NCL - m.topLine;
  m.crsCol = NCP - m.leftChar;
  Console::Update();
  m.diff.PrintCursor( m.vis.CV() );
  m.sts_line_needs_update = true;
}

void Diff::GoToCrsPos_NoWrite( const unsigned ncp_crsLine
                             , const unsigned ncp_crsChar )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = m.vis.CV();

  // These moves refer to View of buffer:
  const bool MOVE_DOWN  = BotLine( m, pV )   < ncp_crsLine;
  const bool MOVE_RIGHT = RightChar( m, pV ) < ncp_crsChar;
  const bool MOVE_UP    = ncp_crsLine     < m.topLine;
  const bool MOVE_LEFT  = ncp_crsChar     < m.leftChar;

  if     ( MOVE_DOWN ) m.topLine = ncp_crsLine - WorkingRows( pV ) + 1;
  else if( MOVE_UP   ) m.topLine = ncp_crsLine;
  m.crsRow  = ncp_crsLine - m.topLine;

  if     ( MOVE_RIGHT ) m.leftChar = ncp_crsChar - WorkingCols( pV ) + 1;
  else if( MOVE_LEFT  ) m.leftChar = ncp_crsChar;
  m.crsCol   = ncp_crsChar - m.leftChar;
}

void GoToCrsPos_Write( Diff::Data& m
                     , const unsigned ncp_crsLine
                     , const unsigned ncp_crsChar )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = m.vis.CV();

  const unsigned OCL = CrsLine(m);
  const unsigned OCP = CrsChar(m);
  const unsigned NCL = ncp_crsLine;
  const unsigned NCP = ncp_crsChar;

  if( OCL == NCL && OCP == NCP )
  {
    // Not moving to new cursor line so just put cursor back where is was
    m.diff.PrintCursor( pV );
  }
  else {
    if( m.inVisualMode )
    {
      m.v_fn_line = NCL;
      m.v_fn_char = NCP;
    }
    // These moves refer to View of buffer:
    const bool MOVE_DOWN  = BotLine( m, pV )   < NCL;
    const bool MOVE_RIGHT = RightChar( m, pV ) < NCP;
    const bool MOVE_UP    = NCL < m.topLine;
    const bool MOVE_LEFT  = NCP < m.leftChar;

    bool redraw = MOVE_DOWN || MOVE_RIGHT || MOVE_UP || MOVE_LEFT;

    if( redraw )
    {
      if     ( MOVE_DOWN ) m.topLine = NCL - WorkingRows( pV ) + 1;
      else if( MOVE_UP   ) m.topLine = NCL;

      if     ( MOVE_RIGHT ) m.leftChar = NCP - WorkingCols( pV ) + 1;
      else if( MOVE_LEFT  ) m.leftChar = NCP;

      // crsRow and crsCol must be set to new values before calling CalcNewCrsByte
      m.crsRow = NCL - m.topLine;
      m.crsCol = NCP - m.leftChar;

      m.diff.Update();
    }
    else if( m.inVisualMode )
    {
      if( m.inVisualBlock ) GoToCrsPos_Write_VisualBlock( m, OCL, OCP, NCL, NCP );
      else                  GoToCrsPos_Write_Visual     ( m, OCL, OCP, NCL, NCP );
    }
    else {
      // crsRow and crsCol must be set to new values before calling CalcNewCrsByte and PrintCursor
      m.crsRow = NCL - m.topLine;
      m.crsCol = NCP - m.leftChar;

      m.diff.PrintCursor( pV );  // Put cursor into position.

      m.sts_line_needs_update = true;
    }
  }
}

void Do_n_Diff( Diff::Data& m, const bool write )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0 < NumLines(m) )
  {
    m.diff.Set_Cmd_Line_Msg("Searching down for diff");

    unsigned dl = CrsLine(m); // Diff line, changed by search methods below

    View* pV = m.vis.CV();

    Array_t<Diff_Info>& DI_List = (pV == m.pvS) ? m.DI_List_S : m.DI_List_L;

    const Diff_Type DT = DI_List[dl].diff_type; // Current diff type

    bool found_same = true;

    if( DT == DT_CHANGED
     || DT == DT_INSERTED
     || DT == DT_DELETED
     || DT == DT_DIFF_FILES )
    {
      // If currently on a diff, search for same before searching for diff
      found_same = Do_n_Search_for_Same( m, dl, DI_List );
    }
    if( found_same )
    {
      bool found_diff = Do_n_Search_for_Diff( m, dl, DI_List );

      unsigned NCL, NCP;
      if( found_diff )
      {
        NCL = dl;
        NCP = Do_n_Find_Crs_Pos( m, NCL, DI_List );
      }
      else // Could not find a difference.
      {    // Check if one file ends in LF and the other does not:
        if( m.pfS->Has_LF_at_EOF() != m.pfL->Has_LF_at_EOF() )
        {
          found_diff = true;
          NCL = DI_List.len() - 1;
          NCP = pV->GetFB()->LineLen( DI_List[ NCL ].line_num );
        }
      }
      if( found_diff )
      {
        if( write ) GoToCrsPos_Write( m, NCL, NCP );
        else        m.diff.GoToCrsPos_NoWrite( NCL, NCP );
      }
    }
  }
}

bool Has_Context( Diff::Data& m )
{
  return 0 != m.topLine
      || 0 != m.leftChar
      || 0 != m.crsRow
      || 0 != m.crsCol ;
}

void Copy_ViewContext_2_DiffContext( Diff::Data& m )
{
  View* pV = m.vis.CV();

  // View context -> diff context
  const unsigned diff_topLine = m.diff.DiffLine( pV, pV->GetTopLine() );
  const unsigned diff_crsLine = m.diff.DiffLine( pV, pV->CrsLine() );
  const unsigned diff_crsRow  = diff_crsLine - diff_topLine;

  m.topLine  = diff_topLine;
  m.leftChar = pV->GetLeftChar();
  m.crsRow   = diff_crsRow;
  m.crsCol   = pV->GetCrsCol();
}

void Find_Context( Diff::Data& m )
{
  if( !Has_Context( m ) )
  {
    View* pV = m.vis.CV();

    if( pV->Has_Context() )
    {
      Copy_ViewContext_2_DiffContext(m);
    }
    else {
      Do_n_Diff( m, false );
      m.diff.MoveCurrLineCenter( false );
    }
  }
}

void Diff::Copy_DiffContext_2_Remaining_ViewContext()
{
  View* shrt_view = GetViewShort();
  View* long_view = GetViewLong ();

  View* cV = m.vis.CV();
  View* remaining_view = cV == long_view ? shrt_view : long_view;

  remaining_view->Set_Context( GetTopLine( remaining_view )
                             , GetLeftChar()
                             , GetCrsRow()
                             , GetCrsCol() );
}

void Clear_DI_List( Diff::Data& m, Array_t<Diff_Info>& DI_List )
{
  Trace trace( __PRETTY_FUNCTION__ );

  for( unsigned k=0; k<DI_List.len(); k++ )
  {
    Diff_Info& di = DI_List[k];

    if( di.pLineInfo )
    {
      // Return all the previously allocated LineInfo's:
      Return_LineInfo( m, di.pLineInfo ); di.pLineInfo = 0;
    }
  }
  DI_List.clear();
}

void Diff::ClearDiff()
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.sameList.clear();
  m.diffList.clear();

  Clear_DI_List( m, m.DI_List_S );
  Clear_DI_List( m, m.DI_List_L );
  m.DI_L_ins_idx = 0;

  Clear_SimiList(m);

  // Reset some other variables:
  m.topLine  = 0;
  m.leftChar = 0;
  m.crsRow   = 0;
  m.crsCol   = 0;
  m.sts_line_needs_update = false;
  m.inVisualMode = false;
  m.v_st_line = 0;
  m.v_st_char = 0;
  m.v_fn_line = 0;
  m.v_fn_char = 0;
  m.pvS = 0;
  m.pvL = 0;
  m.pfS = 0;
  m.pfL = 0;
  m.num_files_added_this_diff = 0;
}

bool DiffSameAsPrev( Diff::Data& m, View* const pv0, View* const pv1 )
{
  Trace trace( __PRETTY_FUNCTION__ );

        bool DATES_SAME_AS_BEFORE = false;

  const bool FILES_SAME_AS_BEFORE = m.pfS && m.pfL &&
                                  (
                                    ( pv0->GetFB() == m.pfS && pv1->GetFB() == m.pfL )
                                 || ( pv0->GetFB() == m.pfL && pv1->GetFB() == m.pfS )
                                  );
  if( FILES_SAME_AS_BEFORE )
  {
    DATES_SAME_AS_BEFORE =
    (
      ( m.mod_time_s == m.pfS->GetModTime() && m.mod_time_l == m.pfL->GetModTime() )
   || ( m.mod_time_l == m.pfS->GetModTime() && m.mod_time_s == m.pfL->GetModTime() )
    );
  }
  return FILES_SAME_AS_BEFORE
      && DATES_SAME_AS_BEFORE;
}

void Set_ShortLong_ViewfileMod_Vars( Diff::Data& m
                                   , View* const pv0, View* const pv1 )
{
  const unsigned nLines_0 = pv0->GetFB()->NumLines();
  const unsigned nLines_1 = pv1->GetFB()->NumLines();

  m.pvS = nLines_0 < nLines_1 ? pv0 : pv1; // Short view
  m.pvL = nLines_0 < nLines_1 ? pv1 : pv0; // Long  view
  m.pfS = m.pvS->GetFB();
  m.pfL = m.pvL->GetFB();
  m.mod_time_s = m.pfS->GetModTime();
  m.mod_time_l = m.pfL->GetModTime();
}

unsigned Row_Win_2_GL( Diff::Data& m, View* pV, const unsigned win_row )
{
  return pV->Y() + 1 + win_row;
}

unsigned Col_Win_2_GL( Diff::Data& m, View* pV, const unsigned win_col )
{
  return pV->X() + 1 + win_col;
}

unsigned LineLen( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = m.vis.CV();

  const unsigned diff_line = CrsLine(m);

  Diff_Info& rDI = ( pV == m.pvS ) ? m.DI_List_S[ diff_line ]
                                   : m.DI_List_L[ diff_line ];
  if( DT_UNKN0WN == rDI.diff_type
   || DT_DELETED == rDI.diff_type )
  {
    return 0;
  }
  const unsigned view_line = rDI.line_num;

  return pV->GetFB()->LineLen( view_line );
}

//// Return the diff line of the view line on the short side
//unsigned DiffLine_S( Diff::Data& m, const unsigned view_line )
//{
//  Trace trace( __PRETTY_FUNCTION__ );
//
//  const unsigned LEN = m.DI_List_S.len();
//
//  if( 0 < m.pvS->GetFB()->NumLines() )
//  {
//    // Diff line is greater or equal to view line,
//    // so start at view line number and search forward
//    bool ok = true;
//    for( unsigned k=view_line; k<LEN && ok; k++ )
//    {
//      Diff_Info di = m.DI_List_S[ k ];
//
//      if( DT_SAME     == di.diff_type
//       || DT_CHANGED  == di.diff_type
//       || DT_INSERTED == di.diff_type )
//      {
//        if( view_line == di.line_num ) return k;
//      }
//    }
//    ASSERT( __LINE__, 0, "view_line : %u : not found", view_line );
//  }
//  return 0;
//}

// Return the diff line of the view line on the short side
//unsigned DiffLine_S( Diff::Data& m, const unsigned view_line )
//{
//  Trace trace( __PRETTY_FUNCTION__ );
//  unsigned diff_line = 0;
//  const unsigned NUM_LINES_VS = m.pvS->GetFB()->NumLines();
//
//  if( 0 < NUM_LINES_VS )
//  {
//    const unsigned DI_LEN = m.DI_List_S.len();
//
//    if( NUM_LINES_VS <= view_line ) diff_line = DI_LEN-1;
//    else {
//      // Diff line is greater or equal to view line,
//      // so start at view line number and search forward
//      unsigned k = view_line;
//      Diff_Info di = m.DI_List_S[ view_line ];
//      k += view_line - di.line_num;
//      bool found = false;
//      for( ; !found && k<DI_LEN; k += view_line - di.line_num )
//      {
//        di = m.DI_List_S[ k ];
//        if( DT_SAME       == di.diff_type
//         || DT_CHANGED    == di.diff_type
//         || DT_INSERTED   == di.diff_type
//         || DT_DIFF_FILES == di.diff_type )
//        {
//          if( view_line == di.line_num )
//          {
//            found = true;
//            diff_line = k;
//          }
//        }
//      }
//      if( !found ) {
//        ASSERT( __LINE__, 0, "view_line : %u : not found", view_line );
//      }
//    }
//  }
//  return diff_line;
//}

// Return the diff line of the view line on the short side
unsigned DiffLine_S( Diff::Data& m, const unsigned view_line )
{
  Trace trace( __PRETTY_FUNCTION__ );
  unsigned diff_line = 0;
  const unsigned NUM_LINES_VS = m.pvS->GetFB()->NumLines();

  if( 0 < NUM_LINES_VS )
  {
    const unsigned DI_LEN = m.DI_List_S.len();

    if( NUM_LINES_VS <= view_line ) diff_line = DI_LEN-1;
    else {
      // Diff line is greater or equal to view line,
      // so start at view line number and search forward
      unsigned k = view_line;
      Diff_Info di = m.DI_List_S[ view_line ];
      k += view_line - di.line_num;
      bool found = false;

      for( ; !found && k<DI_LEN; k += view_line - di.line_num )
      {
        di = m.DI_List_S[ k ];

        if( view_line == di.line_num )
        {
          found = true;
          diff_line = k;
        }
      }
      if( !found ) {
        ASSERT( __LINE__, 0, "view_line : %u : not found", view_line );
      }
    }
  }
  return diff_line;
}

//// Return the diff line of the view line on the long side
//unsigned DiffLine_L( Diff::Data& m, const unsigned view_line )
//{
//  Trace trace( __PRETTY_FUNCTION__ );
//
//  const unsigned LEN = m.DI_List_L.len();
//
//  if( 0 < m.pvL->GetFB()->NumLines() )
//  {
//    // Diff line is greater or equal to view line,
//    // so start at view line number and search forward
//    bool ok = true;
//    for( unsigned k=view_line; k<LEN && ok; k++ )
//    {
//      Diff_Info di = m.DI_List_L[ k ];
//
//      if( DT_SAME     == di.diff_type
//       || DT_CHANGED  == di.diff_type
//       || DT_INSERTED == di.diff_type )
//      {
//        if( view_line == di.line_num ) return k;
//      }
//    }
//    ASSERT( __LINE__, 0, "view_line : %u : not found", view_line );
//  }
//  return 0;
//}

//// Return the diff line of the view line on the long side
//unsigned DiffLine_L( Diff::Data& m, const unsigned view_line )
//{
//  Trace trace( __PRETTY_FUNCTION__ );
//  unsigned diff_line = 0;
//  const unsigned NUM_LINES_VL = m.pvL->GetFB()->NumLines();
//
//  if( 0 < NUM_LINES_VL )
//  {
//    const unsigned DI_LEN = m.DI_List_L.len();
//
//    if( NUM_LINES_VL <= view_line ) diff_line = DI_LEN-1;
//    else {
//      // Diff line is greater or equal to view line,
//      // so start at view line number and search forward
//      unsigned k = view_line;
//      Diff_Info di = m.DI_List_L[ view_line ];
//      k += view_line - di.line_num;
//      bool found = false;
//      for( ; !found && k<DI_LEN; k += view_line - di.line_num )
//      {
//        di = m.DI_List_L[ k ];
//        if( DT_SAME       == di.diff_type
//         || DT_CHANGED    == di.diff_type
//         || DT_INSERTED   == di.diff_type
//         || DT_DIFF_FILES == di.diff_type )
//        {
//          if( view_line == di.line_num )
//          {
//            found = true;
//            diff_line = k;
//          }
//        }
//      }
//      if( !found ) {
//        ASSERT( __LINE__, 0, "view_line : %u : not found", view_line );
//      }
//    }
//  }
//  return diff_line;
//}

// Return the diff line of the view line on the long side
unsigned DiffLine_L( Diff::Data& m, const unsigned view_line )
{
  Trace trace( __PRETTY_FUNCTION__ );
  unsigned diff_line = 0;
  const unsigned NUM_LINES_VL = m.pvL->GetFB()->NumLines();

  if( 0 < NUM_LINES_VL )
  {
    const unsigned DI_LEN = m.DI_List_L.len();

    if( NUM_LINES_VL <= view_line ) diff_line = DI_LEN-1;
    else {
      // Diff line is greater or equal to view line,
      // so start at view line number and search forward
      unsigned k = view_line;
      Diff_Info di = m.DI_List_L[ view_line ];
      k += view_line - di.line_num;
      bool found = false;

      for( ; !found && k<DI_LEN; k += view_line - di.line_num )
      {
        di = m.DI_List_L[ k ];

        if( view_line == di.line_num )
        {
          found = true;
          diff_line = k;
        }
      }
      if( !found ) {
        ASSERT( __LINE__, 0, "view_line : %u : not found", view_line );
      }
    }
  }
  return diff_line;
}

//unsigned DiffLine( Diff::Data& m, const View* pV, const unsigned view_line )
//{
//  Trace trace( __PRETTY_FUNCTION__ );
//
//  return ( pV == m.pvS ) ? DiffLine_S( m, view_line )
//                         : DiffLine_L( m, view_line );
//}

Style DiffStyle( const Style S )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // If S is already a DIFF style, just return it
  Style diff_s = S;

  if     ( S == S_NORMAL   ) diff_s = S_DIFF_NORMAL   ;
  else if( S == S_STAR     ) diff_s = S_DIFF_STAR     ;
  else if( S == S_STAR_IN_F) diff_s = S_DIFF_STAR_IN_F;
  else if( S == S_COMMENT  ) diff_s = S_DIFF_COMMENT  ;
  else if( S == S_DEFINE   ) diff_s = S_DIFF_DEFINE   ;
  else if( S == S_CONST    ) diff_s = S_DIFF_CONST    ;
  else if( S == S_CONTROL  ) diff_s = S_DIFF_CONTROL  ;
  else if( S == S_VARTYPE  ) diff_s = S_DIFF_VARTYPE  ;
  else if( S == S_VISUAL   ) diff_s = S_DIFF_VISUAL   ;

  return diff_s;
}

void PrintWorkingView_DT_UNKN0WN( Diff::Data& m
                                , View* pV
                                , const unsigned WC
                                , const unsigned G_ROW )
{
  Trace trace( __PRETTY_FUNCTION__ );

  for( unsigned col=0; col<WC; col++ )
  {
    Console::Set( G_ROW, Col_Win_2_GL( m, pV, col ), '~', S_DIFF_DEL );
  }
}

void PrintWorkingView_DT_DELETED( Diff::Data& m
                                , View* pV
                                , const unsigned WC
                                , const unsigned G_ROW )
{
  Trace trace( __PRETTY_FUNCTION__ );

  for( unsigned col=0; col<WC; col++ )
  {
    Console::Set( G_ROW, Col_Win_2_GL( m, pV, col ), '-', S_DIFF_DEL );
  }
}

void PrintWorkingView_DT_CHANGED( Diff::Data& m
                                , View* pV
                                , const unsigned WC
                                , const unsigned G_ROW
                                , const unsigned dl )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned vl = ViewLine( m, pV, dl ); //(vl=view line)
  const unsigned LL = pV->GetFB()->LineLen( vl );
  Diff_Info di = (pV == m.pvS) ? m.DI_List_S[ dl ] : m.DI_List_L[ dl ];
  unsigned col = 0;

  if( di.pLineInfo )
  {
    const unsigned LIL = di.pLineInfo->len();
    unsigned cp = m.leftChar; // char position
    for( unsigned i=m.leftChar; cp<LL && i<LIL && col<WC; i++, col++ )
    {
      Diff_Type dt = (*di.pLineInfo)[i];

      if( DT_SAME == dt )
      {
        Style s    = Get_Style( m, pV, dl, vl, cp );
        int   byte = pV->GetFB()->Get( vl, cp );
        pV->PrintWorkingView_Set( LL, G_ROW, col, cp, byte, s );
        cp++;
      }
      else if( DT_CHANGED == dt || DT_INSERTED == dt )
      {
        Style s    = Get_Style( m, pV, dl, vl, cp ); s = DiffStyle( s );
        int   byte = pV->GetFB()->Get( vl, cp );
        pV->PrintWorkingView_Set( LL, G_ROW, col, cp, byte, s );
        cp++;
      }
      else if( DT_DELETED == dt )
      {
        Console::Set( G_ROW, Col_Win_2_GL( m, pV, col ), '-', S_DIFF_DEL );
      }
      else //( DT_UNKN0WN  == dt )
      {
        Console::Set( G_ROW, Col_Win_2_GL( m, pV, col ), '~', S_DIFF_DEL );
      }
    }
    for( ; col<WC; col++ )
    {
      Console::Set( G_ROW, Col_Win_2_GL( m, pV, col ), ' ', S_EMPTY );
    }
  }
  else {
    for( unsigned i=m.leftChar; i<LL && col<WC; i++, col++ )
    {
      Style s = Get_Style( m, pV, dl, vl, i );
            s = DiffStyle( s );
      int byte = pV->GetFB()->Get( vl, i );
      pV->PrintWorkingView_Set( LL, G_ROW, col, i, byte, s );
    }
    for( ; col<WC; col++ )
    {
      Console::Set( G_ROW, Col_Win_2_GL( m, pV, col ), ' ', S_DIFF_NORMAL );
    }
  }
}

void PrintWorkingView_DT_DIFF_FILES( Diff::Data& m
                                   , View* pV
                                   , const unsigned WC
                                   , const unsigned G_ROW
                                   , const unsigned dl )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned vl = ViewLine( m, pV, dl ); //(vl=view line)
  const unsigned LL = pV->GetFB()->LineLen( vl );
  unsigned col = 0;

  for( unsigned i=m.leftChar; i<LL && col<WC; i++, col++ )
  {
    uint8_t c = pV->GetFB()->Get( vl, i );
    Style   s = Get_Style( m, pV, dl, vl, i );

    pV->PrintWorkingView_Set( LL, G_ROW, col, i, c, s );
  }
  for( ; col<WC; col++ )
  {
    Console::Set( G_ROW, Col_Win_2_GL( m, pV, col ), ' '
                , col%2==0 ? S_NORMAL : S_DIFF_NORMAL );
  }
}

void PrintWorkingView_DT_INSERTED_SAME( Diff::Data& m
                                      , View* pV
                                      , const unsigned WC
                                      , const unsigned G_ROW
                                      , const unsigned dl
                                      , const Diff_Type DT )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned vl = ViewLine( m, pV, dl ); //(vl=view line)
  const unsigned LL = pV->GetFB()->LineLen( vl );
  unsigned col = 0;

  for( unsigned i=m.leftChar; i<LL && col<WC; i++, col++ )
  {
    uint8_t c = pV->GetFB()->Get( vl, i );
    Style   s = Get_Style( m, pV, dl, vl, i );

    if( DT == DT_INSERTED ) s = DiffStyle( s );
    pV->PrintWorkingView_Set( LL, G_ROW, col, i, c, s );
  }
  for( ; col<WC; col++ )
  {
    Console::Set( G_ROW, Col_Win_2_GL( m, pV, col ), ' '
                , DT==DT_SAME ? S_EMPTY : S_DIFF_NORMAL );
  }
}

void PrintWorkingView_EOF( Diff::Data& m
                         , View* pV
                         , const unsigned WR
                         , const unsigned WC
                         ,       unsigned row )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Not enough lines to display, fill in with ~
  for( ; row < WR; row++ )
  {
    const unsigned G_ROW = Row_Win_2_GL( m, pV, row );

    Console::Set( G_ROW, Col_Win_2_GL( m, pV, 0 ), '~', S_EOF );

    for( unsigned col=1; col<WC; col++ )
    {
      Console::Set( G_ROW, Col_Win_2_GL( m, pV, col ), ' ', S_EOF );
    }
  }
}

void PrintWorkingView( Diff::Data& m, View* pV )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = NumLines(m);
  const unsigned WR        = WorkingRows( pV );
  const unsigned WC        = WorkingCols( pV );

  unsigned row = 0; // (dl=diff line)
  for( unsigned dl=m.topLine; dl<NUM_LINES && row<WR; dl++, row++ )
  {
    unsigned col=0;
    const unsigned G_ROW = Row_Win_2_GL( m, pV, row );
    const Diff_Type DT = DiffType( m, pV, dl );

    if( DT == DT_UNKN0WN )
    {
      PrintWorkingView_DT_UNKN0WN( m, pV, WC, G_ROW );
    }
    else if( DT == DT_DELETED )
    {
      PrintWorkingView_DT_DELETED( m, pV, WC, G_ROW );
    }
    else if( DT == DT_CHANGED )
    {
      PrintWorkingView_DT_CHANGED( m, pV, WC, G_ROW, dl );
    }
    else if( DT == DT_DIFF_FILES )
    {
      PrintWorkingView_DT_DIFF_FILES( m, pV, WC, G_ROW, dl );
    }
    else // DT == DT_INSERTED || DT == DT_SAME
    {
      PrintWorkingView_DT_INSERTED_SAME( m, pV, WC, G_ROW, dl, DT );
    }
  }
  PrintWorkingView_EOF( m, pV, WR, WC, row );
}

void PrintStsLine( Diff::Data& m, View* pV )
{
  Trace trace( __PRETTY_FUNCTION__ );
  char buf1[  16]; buf1[0] = 0;
  char buf2[1024]; buf2[0] = 0;

  Array_t<Diff_Info>& DI_List = (pV == m.pvS) ? m.DI_List_S : m.DI_List_L;
  FileBuf* pfb = pV->GetFB();
  const unsigned CLd = CrsLine(m);               // Line position diff
  const unsigned CLv = DI_List[ CLd ].line_num;  // Line position view
  const unsigned CC = CrsChar(m);                // Char position
  const unsigned LL = 0 < NumLines(m)
                  ? ( 0 < pfb->NumLines() ? pfb->LineLen( CLv ) : 0 )
                  : 0;
  const unsigned WC = WorkingCols( pV );

  // When inserting text at the end of a line, CrsChar(m) == LL
  if( LL && CC < LL ) // Print current char info:
  {
    const int c = pfb->Get( CLv, CC );

    if( (  0 <= c && c <=  8 )
     || ( 11 <= c && c <= 12 )
     || ( 14 <= c && c <= 31 )
     || (127 <= c && c <= 255 ) ) // Non-printable char:
    {
      sprintf( buf1, "%u", c );
    }
    else if(  9 == c ) sprintf( buf1, "%u,\\t", c );
    else if( 13 == c ) sprintf( buf1, "%u,\\r", c );
    else               sprintf( buf1, "%u,%c", c, c );
  }
  const unsigned fileSize = pfb->GetSize();
  const unsigned  crsByte = pfb->GetCursorByte( CLv, CC );
  char percent = SCast<char>(100*double(crsByte)/double(fileSize) + 0.5);
  // Screen width so far
  char* p = buf2;

  p += sprintf( buf2, "Pos=(%u,%u)  (%i%%, %u/%u)  Char=(%s)  "
                    , CLv+1, CC+1, percent, crsByte, pfb->GetSize(), buf1 );
  unsigned SW = p - buf2; // Screen width so far

  if( SW < WC )
  {
    for( unsigned k=SW; k<WC; k++ ) *p++ = ' ';
  }
  else if( WC < SW )
  {
    p = buf2 + WC; // Truncate extra part
  }
  *p = 0;

  Console::SetS( pV->Sts__Line_Row(), pV->Col_Win_2_GL( 0 ), buf2, S_STATUS );
}

void PrintCmdLine( Diff::Data& m, View* pV )
{
  // Prints "--INSERT--" banner, and/or clears command line
  Trace trace( __PRETTY_FUNCTION__ );

  unsigned i=0;
  // Draw insert banner if needed
  if( pV->GetInsertMode() )
  {
    i=10; // Strlen of "--INSERT--"
    Console::SetS( pV->Cmd__Line_Row(), pV->Col_Win_2_GL( 0 ), "--INSERT--", S_BANNER );
  }
  const unsigned WC = WorkingCols( pV );

  for( ; i<WC-7; i++ )
  {
    Console::Set( pV->Cmd__Line_Row(), pV->Col_Win_2_GL( i ), ' ', S_NORMAL );
  }
  Console::SetS( pV->Cmd__Line_Row(), pV->Col_Win_2_GL( WC-8 ), "--DIFF--", S_BANNER );
}

void DisplayBanner( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = m.vis.CV();

  // Command line row in window:
  const unsigned WIN_ROW = WorkingRows( pV ) + 2;
  const unsigned WIN_COL = 0;

  const unsigned G_ROW = Row_Win_2_GL( m, pV, WIN_ROW );
  const unsigned G_COL = Col_Win_2_GL( m, pV, WIN_COL );

  if( pV->GetInsertMode() )
  {
    Console::SetS( G_ROW, G_COL, "--INSERT --", S_BANNER );
  }
  else if( pV->GetReplaceMode() )
  {
    Console::SetS( G_ROW, G_COL, "--REPLACE--", S_BANNER );
  }
  else if( m.inVisualMode )
  {
    Console::SetS( G_ROW, G_COL, "--VISUAL --", S_BANNER );
  }
  Console::Update();
  m.diff.PrintCursor( pV ); // Put cursor back in position.
}

void Remove_Banner( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = m.vis.CV();

  const unsigned WC = WorkingCols( pV );
  const unsigned N = Min( WC, 11 );

  // Command line row in window:
  const unsigned WIN_ROW = WorkingRows( pV ) + 2;

  // Clear command line:
  for( unsigned k=0; k<N; k++ )
  {
    Console::Set( Row_Win_2_GL( m, pV, WIN_ROW )
                , Col_Win_2_GL( m, pV, k )
                , ' '
                , S_NORMAL );
  }
  Console::Update();
  m.diff.PrintCursor( pV ); // Put cursor back in position.
}

void PrintSameList( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  for( unsigned k=0; k<m.sameList.len(); k++ )
  {
    SameArea same = m.sameList[k];
    Log.Log( "Same: (%s):(%u-%u), (%s):(%u-%u), nlines=%u, nbytes=%u\n"
           , m.pfS->GetPathName(), same.ln_s+1, same.ln_s+same.nlines
           , m.pfL->GetPathName(), same.ln_l+1, same.ln_l+same.nlines
           , same.nlines
           , same.nbytes );
  }
}

void PrintDiffList( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  for( unsigned k=0; k<m.diffList.len(); k++ )
  {
    DiffArea da = m.diffList[k];
    Log.Log( "Diff: (%s):(%u-%u), (%s):(%u-%u)\n"
           , m.pfS->GetPathName(), da.ln_s+1, da.ln_s+da.nlines_s
           , m.pfL->GetPathName(), da.ln_l+1, da.ln_l+da.nlines_l );
  }
}

void PrintDI_List( Diff::Data& m, const DiffArea& CA )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned DILL = m.DI_List_S.len();

  for( unsigned k=CA.ln_s; k<DILL; k++ )
  {
    Diff_Info dis = m.DI_List_S[k];
    Diff_Info dil = m.DI_List_L[k];

    Log.Log("DIS (%u:%s), DIL (%u,%s)\n"
           , dis.line_num+1, Diff_Type_2_Str( dis.diff_type )
           , dil.line_num+1, Diff_Type_2_Str( dil.diff_type ) );

    if( CA.fnl_s() <= dis.line_num ) break;
  }
}

void PrintSimiList( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned SLL = m.simiList.len();

  for( unsigned k=0; k<SLL; k++ )
  {
    SimLines sl = m.simiList[k];

    Log.Log("SimLines: ln_s=%u, ln_l=%u, nbytes=%u\n"
           , sl.ln_s+1, sl.ln_l+1, sl.nbytes );
  }
}

void GoToOppositeBracket_Forward( Diff::Data& m
                                , const char ST_C, const char FN_C )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = m.vis.CV();

  const unsigned NUM_LINES = pV->GetFB()->NumLines();

  // Convert from diff line (CrsLine(m)), to view line:
  const unsigned CL = ViewLine( m, pV, CrsLine(m) );
  const unsigned CC = CrsChar(m);

  // Search forward
  unsigned level = 0;
  bool     found = false;

  for( unsigned vl=CL; !found && vl<NUM_LINES; vl++ )
  {
    const unsigned LL = pV->GetFB()->LineLen( vl );

    for( unsigned p=(CL==vl)?(CC+1):0; !found && p<LL; p++ )
    {
      const char C = pV->GetFB()->Get( vl, p );

      if     ( C==ST_C ) level++;
      else if( C==FN_C )
      {
        if( 0 < level ) level--;
        else {
          found = true;

          // Convert from view line back to diff line:
          const unsigned dl = m.diff.DiffLine(pV, vl);

          GoToCrsPos_Write( m, dl, p );
        }
      }
    }
  }
}

void GoToOppositeBracket_Backward( Diff::Data& m
                                 , const char ST_C, const char FN_C )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = m.vis.CV();

  // Convert from diff line (CrsLine(m)), to view line:
  const int CL = ViewLine( m, pV, CrsLine(m) );
  const int CC = CrsChar(m);

  // Search forward
  unsigned level = 0;
  bool     found = false;

  for( int vl=CL; !found && 0<=vl; vl-- )
  {
    const unsigned LL = pV->GetFB()->LineLen( vl );

    for( int p=(CL==vl)?(CC-1):(LL-1); !found && 0<=p; p-- )
    {
      const char C = pV->GetFB()->Get( vl, p );

      if     ( C==ST_C ) level++;
      else if( C==FN_C )
      {
        if( 0 < level ) level--;
        else {
          found = true;

          // Convert from view line back to dif line:
          const unsigned dl = m.diff.DiffLine(pV, vl);

          GoToCrsPos_Write( m, dl, p );
        }
      }
    }
  }
}

// Returns true if found next word, else false
//
bool GoToNextWord_GetPosition( Diff::Data& m, CrsPos& ncp )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = m.vis.CV();

  const unsigned NUM_LINES = pV->GetFB()->NumLines();
  if( 0==NUM_LINES ) return false;

  bool found_space = false;
  bool found_word  = false;

  // Convert from diff line (CrsLine(m)), to view line:
  const unsigned OCL = ViewLine( m, pV, CrsLine(m) ); //< Old cursor view line
  const unsigned OCP = CrsChar(m);                    //< Old cursor position

  IsWord_Func isWord = IsWord_Ident;

  // Find white space, and then find non-white space
  for( unsigned vl=OCL; (!found_space || !found_word) && vl<NUM_LINES; vl++ )
  {
    const unsigned LL = pV->GetFB()->LineLen( vl );
    if( LL==0 || OCL<vl )
    {
      found_space = true;
      // Once we have encountered a space, word is anything non-space.
      // An empty line is considered to be a space.
      isWord = NotSpace;
    }
    const unsigned START_C = OCL==vl ? OCP : 0;

    for( unsigned p=START_C; (!found_space || !found_word) && p<LL; p++ )
    {
      const int C = pV->GetFB()->Get( vl, p );

      if( found_space  )
      {
        if( isWord( C ) ) found_word = true;
      }
      else {
        if( !isWord( C ) ) found_space = true;
      }
      // Once we have encountered a space, word is anything non-space
      if( IsSpace( C ) ) isWord = NotSpace;

      if( found_space && found_word )
      {
        // Convert from view line back to diff line:
        const unsigned dl = m.diff.DiffLine( pV, vl );

        ncp.crsLine = dl;
        ncp.crsChar = p;
      }
    }
  }
  return found_space && found_word;
}

// Return true if new cursor position found, else false
bool GoToPrevWord_GetPosition( Diff::Data& m, CrsPos& ncp )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = m.vis.CV();

  const unsigned NUM_LINES = pV->GetFB()->NumLines();
  if( 0==NUM_LINES ) return false;

  // Convert from diff line (CrsLine(m)), to view line:
  const int      OCL = ViewLine( m, pV, CrsLine(m) );
  const unsigned LL  = pV->GetFB()->LineLen( OCL );

  if( LL < CrsChar(m) ) // Since cursor is now allowed past EOL,
  {                    // it may need to be moved back:
    if( LL && !IsSpace( pV->GetFB()->Get( OCL, LL-1 ) ) )
    {
      // Backed up to non-white space, which is previous word, so return true
      // Convert from view line back to diff line:
      ncp.crsLine = CrsLine(m); //< diff line
      ncp.crsChar = LL-1;
      return true;
    }
    else {
      m.diff.GoToCrsPos_NoWrite( CrsLine(m), LL ? LL-1 : 0 );
    }
  }
  bool found_space = false;
  bool found_word  = false;
  const unsigned OCP = CrsChar(m); // Old cursor position

  IsWord_Func isWord = NotSpace;

  // Find word to non-word transition
  for( int vl=OCL; (!found_space || !found_word) && -1<vl; vl-- )
  {
    const int LL = pV->GetFB()->LineLen( vl );
    if( LL==0 || vl<OCL )
    {
      // Once we have encountered a space, word is anything non-space.
      // An empty line is considered to be a space.
      isWord = NotSpace;
    }
    const unsigned START_C = OCL==vl ? OCP-1 : LL-1;

    for( int p=START_C; (!found_space || !found_word) && -1<p; p-- )
    {
      const int C = pV->GetFB()->Get( vl, p);

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

      if( found_space && found_word )
      {
        // Convert from view line back to diff line:
        const unsigned dl = m.diff.DiffLine( pV, vl );

        ncp.crsLine = dl;
        ncp.crsChar = p;
      }
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
bool GoToEndOfWord_GetPosition( Diff::Data& m, CrsPos& ncp )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  const unsigned NUM_LINES = pfb->NumLines();
  if( 0==NUM_LINES ) return false;

  // Convert from diff line (CrsLine(m)), to view line:
  const unsigned CL = ViewLine( m, pV, CrsLine(m) );
  const unsigned LL = pfb->LineLen( CL );
        unsigned CP = CrsChar(m); // Cursor position

  // At end of line, or line too short:
  if( (LL-1) <= CP || LL < 2 ) return false;

  int CC = pfb->Get( CL, CP );   // Current char
  int NC = pfb->Get( CL, CP+1 ); // Next char

  // 1. If at end of word, or end of non-word, move to next char
  if( (IsWord_Ident   ( CC ) && !IsWord_Ident   ( NC ))
   || (IsWord_NonIdent( CC ) && !IsWord_NonIdent( NC )) ) CP++;

  // 2. If on white space, skip past white space
  if( IsSpace( pfb->Get(CL, CP) ) )
  {
    for( ; CP<LL && IsSpace( pfb->Get(CL, CP) ); CP++ ) ;
    if( LL <= CP ) return false; // Did not find non-white space
  }
  // At this point (CL,CP) should be non-white space
  CC = pfb->Get( CL, CP );  // Current char

  ncp.crsLine = CrsLine(m); // Diff line

  if( IsWord_Ident( CC ) ) // On identity
  {
    // 3. If on word space, go to end of word space
    for( ; CP<LL && IsWord_Ident( pfb->Get(CL, CP) ); CP++ )
    {
      ncp.crsChar = CP;
    }
  }
  else if( IsWord_NonIdent( CC ) )// On Non-identity, non-white space
  {
    // 4. If on non-white-non-word, go to end of non-white-non-word
    for( ; CP<LL && IsWord_NonIdent( pfb->Get(CL, CP) ); CP++ )
    {
      ncp.crsChar = CP;
    }
  }
  else { // Should never get here:
    return false;
  }
  return true;
}

bool Do_n_FindNextPattern( Diff::Data& m, CrsPos& ncp )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  const unsigned NUM_LINES = pfb->NumLines();

  const unsigned OCL = CrsLine(m); // Diff line
  const unsigned OCC = CrsChar(m);

  const unsigned OCLv = ViewLine( m, pV, OCL ); // View line

  unsigned st_l = OCLv;
  unsigned st_c = OCC;

  bool found_next_star = false;

  // Move past current star:
  const unsigned LL = pfb->LineLen( OCLv );

  pfb->Check_4_New_Regex();
  pfb->Find_Regexs_4_Line( OCL );

  // Move past current pattern:
  for( ; st_c<LL && pV->InStarOrStarInF(OCLv,st_c); st_c++ ) ;

  // If at end of current line, go down to next line
  if( LL <= st_c ) { st_c=0; st_l++; }

  // Search for first pattern position past current position
  for( unsigned l=st_l; !found_next_star && l<NUM_LINES; l++ )
  {
    pfb->Find_Regexs_4_Line( l );

    const unsigned LL = pfb->LineLen( l );

    for( unsigned p=st_c; !found_next_star && p<LL; p++ )
    {
      if( pV->InStarOrStarInF(l,p) )
      {
        found_next_star = true;
        // Convert from view line back to diff line:
        const unsigned dl = m.diff.DiffLine( pV, l );
        ncp.crsLine = dl;
        ncp.crsChar = p;
      }
    }
    // After first line, always start at beginning of line
    st_c = 0;
  }
  // Near end of file and did not find any patterns, so go to first pattern in file
  if( !found_next_star )
  {
    for( unsigned l=0; !found_next_star && l<=OCLv; l++ )
    {
      pfb->Find_Regexs_4_Line( l );

      const unsigned LL = pfb->LineLen( l );
      const unsigned END_C = (OCLv==l) ? Min( OCC, LL ) : LL;

      for( unsigned p=0; !found_next_star && p<END_C; p++ )
      {
        if( pV->InStarOrStarInF(l,p) )
        {
          found_next_star = true;
          // Convert from view line back to diff line:
          const unsigned dl = m.diff.DiffLine( pV, l );
          ncp.crsLine = dl;
          ncp.crsChar = p;
        }
      }
    }
  }
  return found_next_star;
}

void Do_n_Pattern( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = m.vis.CV();

  const unsigned NUM_LINES = pV->GetFB()->NumLines();

  if( 0 < NUM_LINES )
  {
    String msg("/");
    m.diff.Set_Cmd_Line_Msg( msg += m.vis.GetRegex() );

    CrsPos ncp = { 0, 0 }; // Next cursor position

    if( Do_n_FindNextPattern( m, ncp ) )
    {
      GoToCrsPos_Write( m, ncp.crsLine, ncp.crsChar );
    }
  }
}

// If past end of line, move back to end of line.
// Returns true if moved, false otherwise.
//
void MoveInBounds_Line( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = m.vis.CV();

  const unsigned DL  = CrsLine(m);  // Diff line
  const unsigned VL  = ViewLine( m, pV, DL );      // View line
  const unsigned LL  = pV->GetFB()->LineLen( VL );
  const unsigned EOL = LL ? LL-1 : 0;

  if( EOL < CrsChar(m) ) // Since cursor is now allowed past EOL,
  {                      // it may need to be moved back:
    m.diff.GoToCrsPos_NoWrite( DL, EOL );
  }
}

bool Do_N_FindPrevPattern( Diff::Data& m, CrsPos& ncp )
{
  Trace trace( __PRETTY_FUNCTION__ );
  MoveInBounds_Line(m);

  View* pV = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  const unsigned NUM_LINES = pfb->NumLines();

  const unsigned OCL = CrsLine(m);
  const unsigned OCC = CrsChar(m);

  const unsigned OCLv = ViewLine( m, pV, OCL ); // View line

  pfb->Check_4_New_Regex();

  bool found_prev_star = false;

  // Search for first star position before current position
  for( int l=OCLv; !found_prev_star && 0<=l; l-- )
  {
    pfb->Find_Regexs_4_Line( l );

    const int LL = pfb->LineLen( l );

    int p=LL-1;
    if( OCLv==l ) p = OCC ? OCC-1 : 0;

    for( ; 0<p && !found_prev_star; p-- )
    {
      for( ; 0<=p && pV->InStarOrStarInF(l,p); p-- )
      {
        found_prev_star = true;
        // Convert from view line back to diff line:
        const unsigned dl = m.diff.DiffLine( pV, l );
        ncp.crsLine = dl;
        ncp.crsChar = p;
      }
    }
  }
  // Near beginning of file and did not find any patterns, so go to last pattern in file
  if( !found_prev_star )
  {
    for( int l=NUM_LINES-1; !found_prev_star && OCLv<l; l-- )
    {
      pfb->Find_Regexs_4_Line( l );

      const unsigned LL = pfb->LineLen( l );

      int p=LL-1;
      if( OCLv==l ) p = OCC ? OCC-1 : 0;

      for( ; 0<p && !found_prev_star; p-- )
      {
        for( ; 0<=p && pV->InStarOrStarInF(l,p); p-- )
        {
          found_prev_star = true;
          // Convert from view line back to diff line:
          const unsigned dl = m.diff.DiffLine( pV, l );
          ncp.crsLine = dl;
          ncp.crsChar = p;
        }
      }
    }
  }
  return found_prev_star;
}

void Do_N_Pattern( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = m.vis.CV();

  const unsigned NUM_LINES = pV->GetFB()->NumLines();

  if( 0 < NUM_LINES )
  {
    String msg("/");
    m.diff.Set_Cmd_Line_Msg( msg += m.vis.GetRegex() );

    CrsPos ncp = { 0, 0 }; // Next cursor position

    if( Do_N_FindPrevPattern( m, ncp ) )
    {
      GoToCrsPos_Write( m, ncp.crsLine, ncp.crsChar );
    }
  }
}

bool Do_N_Search_for_Same( Diff::Data& m
                         , int& dl
                         , const Array_t<Diff_Info>& DI_List )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const int NUM_LINES = NumLines(m);
  const int dl_st = dl;

  // Search backwards for DT_SAME
  bool found = false;

  if( 1 < NUM_LINES )
  {
    while( !found && 0<=dl )
    {
      const Diff_Type DT = DI_List[dl].diff_type;

      if( DT == DT_SAME )
      {
        found = true;
      }
      else dl--;
    }
    if( !found )
    {
      // Wrap around back to bottom and search again:
      dl = NUM_LINES-1;
      while( !found && dl_st<dl )
      {
        const Diff_Type DT = DI_List[dl].diff_type;

        if( DT == DT_SAME )
        {
          found = true;
        }
        else dl--;
      }
    }
  }
  return found;
}

// Look for difference based on Diff_Info:
bool Do_N_Search_for_Diff_DT( Diff::Data& m
                            , int& dl
                            , const Array_t<Diff_Info>& DI_List )
{
  bool found_diff = false;

  const unsigned NUM_LINES = NumLines(m);
  const unsigned dl_st = dl;

  while( !found_diff && 0<=dl )
  {
    const Diff_Type DT = DI_List[dl].diff_type;

    if( DT == DT_CHANGED
     || DT == DT_INSERTED
     || DT == DT_DELETED
     || DT == DT_DIFF_FILES )
    {
      found_diff = true;
    }
    else dl--;
  }
  if( !found_diff )
  {
    // Wrap around back to bottom and search again:
    dl = NUM_LINES-1;
    while( !found_diff && dl_st<dl )
    {
      const Diff_Type DT = DI_List[dl].diff_type;

      if( DT == DT_CHANGED
       || DT == DT_INSERTED
       || DT == DT_DELETED
       || DT == DT_DIFF_FILES )
      {
        found_diff = true;
      }
      else dl--;
    }
  }
  return found_diff;
}

bool
Line_Has_Leading_or_Trailing_WS_Diff( int& dl
                                    , const int k
                                    , const Array_t<Diff_Info>& DI_List
                                    , const Array_t<Diff_Info>& DI_List_o
                                    , const FileBuf* pF_m
                                    , const FileBuf* pF_o )
{
  bool L_T_WS_diff = false;

  const Diff_Info& Di_m = DI_List[ k ];
  const Diff_Info& Di_o = DI_List_o[ k ];

  if( Di_m.diff_type == DT_SAME
   && Di_o.diff_type == DT_SAME )
  {
    const Line* lm = pF_m->GetLineP( Di_m.line_num ); // Line from my    view
    const Line* lo = pF_o->GetLineP( Di_o.line_num ); // Line from other view

    if( lm->len() != lo->len() )
    {
      L_T_WS_diff = true;
      dl = k;
    }
  }
  return L_T_WS_diff;
}

// Look for difference in white space at beginning or ending of lines:
bool Do_N_Search_for_Diff_WhiteSpace( Diff::Data& m
                                    , int& dl
                                    , const Array_t<Diff_Info>& DI_List )
{
  bool found_diff = false;

  const unsigned NUM_LINES = NumLines(m);

  Array_t<Diff_Info>& DI_List_o = (&DI_List == &m.DI_List_S) ? m.DI_List_L : m.DI_List_S;
  FileBuf* pF_m = (&DI_List == &m.DI_List_S) ? m.pfS : m.pfL;
  FileBuf* pF_o = (&DI_List == &m.DI_List_S) ? m.pfL : m.pfS;

  // If the current line has a difference in white space at beginning or end, start
  // searching on next line so the current line number is not automatically returned.
  bool curr_line_has_LT_WS_diff
    = Line_Has_Leading_or_Trailing_WS_Diff( dl, dl
                                          , DI_List, DI_List_o
                                          , pF_m, pF_o );
  const unsigned dl_st = curr_line_has_LT_WS_diff
                       ? ( 0 < dl ? (dl - 1) % NUM_LINES
                                  : NUM_LINES-1 )
                       : dl;

  // Search from dl_st to end for lines of different length:
  for( int k=dl_st; !found_diff && 0<=k; k-- )
  {
    found_diff = Line_Has_Leading_or_Trailing_WS_Diff( dl, k
                                                     , DI_List, DI_List_o
                                                     , pF_m, pF_o );
  }
  if( !found_diff )
  {
    // Search from top to dl_st for lines of different length:
    for( int k=NUM_LINES-1; !found_diff && dl_st<k; k-- )
    {
      found_diff = Line_Has_Leading_or_Trailing_WS_Diff( dl, k
                                                       , DI_List, DI_List_o
                                                       , pF_m, pF_o );
    }
  }
  return found_diff;
}


bool Do_N_Search_for_Diff( Diff::Data& m
                         , int& dl
                         , const Array_t<Diff_Info>& DI_List )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned dl_st = dl;

  // Search backwards for non-DT_SAME
  bool found_diff = false;

  if( 1 < NumLines(m) )
  {
    found_diff = Do_N_Search_for_Diff_DT( m, dl, DI_List );

    if( !found_diff )
    {
      dl = dl_st;
      found_diff = Do_N_Search_for_Diff_WhiteSpace( m, dl, DI_List );
    }
  }
  return found_diff;
}

void Do_N_Diff( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0 < NumLines(m) )
  {
    m.diff.Set_Cmd_Line_Msg("Searching up for diff");

    int dl = CrsLine(m); // Diff line, changed by search methods below

    View* pV = m.vis.CV();

    Array_t<Diff_Info>& DI_List = (pV == m.pvS) ? m.DI_List_S : m.DI_List_L;

    const Diff_Type DT = DI_List[dl].diff_type; // Current diff type

    bool found_same = true;

    if( DT == DT_CHANGED
     || DT == DT_INSERTED
     || DT == DT_DELETED
     || DT == DT_DIFF_FILES )
    {
      // If currently on a diff, search for same before searching for diff
      found_same = Do_N_Search_for_Same( m, dl, DI_List );
    }
    if( found_same )
    {
      bool found_diff = Do_N_Search_for_Diff( m, dl, DI_List );

      if( found_diff )
      {
        const unsigned NCL = dl;
        const unsigned NCP = Do_n_Find_Crs_Pos( m, NCL, DI_List );

        GoToCrsPos_Write( m, NCL, NCP );
      }
    }
  }
}

// Since a line was just inserted, increment line numbers of all lines
// following, and increment line number of inserted line if needed.
void Patch_Diff_Info_Inserted_Inc( Diff::Data& m
                                 , const unsigned DPL
                                 , const bool ON_DELETED_VIEW_LINE_ZERO
                                 , Array_t<Diff_Info>& cDI_List )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // If started inserting into empty first line in file, dont increment
  // Diff_Info line_num, because DELETED first line starts at zero:
  unsigned inc_st = DPL;
  if( ON_DELETED_VIEW_LINE_ZERO )
  {
    inc_st = DPL+1;
    // Since we just inserted into DELETED_VIEW_LINE_ZERO,
    // current line is line zero.
    // Move increment start down to first non-DELETED line after current line.
    for( unsigned k=DPL+1; k<cDI_List.len(); k++ )
    {
      Diff_Info& di = cDI_List[ k ];
      if( DT_DELETED == di.diff_type )
      {
        inc_st = k+1;
      }
      else break;
    }
  }
  // Added a view line, so increment all following view line numbers:
  for( unsigned k=inc_st; k<cDI_List.len(); k++ )
  {
    cDI_List[ k ].line_num++;
  }
}

//| Action | ThisSide | OtherSide | Action
//--------------------------------------------------------------------------------
//| Insert | DELETED  | INSERTED  | Compare sides, if same set both to SAME, else set both to CHANGED
//|        | -------- | ANY OTHER | Add line to both sides, and set this side to INSERTED and other side to DELETED
void Diff::Patch_Diff_Info_Inserted( View* pV
                                   , const unsigned DPL
                                   , const bool ON_DELETED_VIEW_LINE_ZERO )
{
  Trace trace( __PRETTY_FUNCTION__ );
  Array_t<Diff_Info>& cDI_List = (pV == m.pvS) ? m.DI_List_S : m.DI_List_L; // Current
  Array_t<Diff_Info>& oDI_List = (pV == m.pvS) ? m.DI_List_L : m.DI_List_S; // Other

  const unsigned DI_Len = cDI_List.len();

  if( DI_Len <= DPL )
  {
    // Inserting onto end of Diff_Info lists:
    Diff_Info dic = { DT_INSERTED, cDI_List[ DI_Len-1 ].line_num+1 };
    Diff_Info dio = { DT_DELETED , oDI_List[ DI_Len-1 ].line_num   };

    bool ok1 = cDI_List.insert( DI_Len, dic ); ASSERT( __LINE__, ok1, "ok1" );
    bool ok2 = oDI_List.insert( DI_Len, dio ); ASSERT( __LINE__, ok2, "ok2" );
  }
  else { // Inserting into beginning or middle of Diff_Info lists:
    Diff_Info& cDI = cDI_List[ DPL ];
    Diff_Info& oDI = oDI_List[ DPL ];

    if( DT_DELETED == cDI.diff_type )
    {
      Patch_Diff_Info_Inserted_Inc( m, DPL, ON_DELETED_VIEW_LINE_ZERO, cDI_List );

      Diff_Info& sDI = m.DI_List_S[ DPL ]; // Short   Diff_Info
      Diff_Info& lDI = m.DI_List_L[ DPL ]; // Long    Diff_Info

      const Line* ls = m.pfS->GetLineP( sDI.line_num ); // Line from short view
      const Line* ll = m.pfL->GetLineP( lDI.line_num ); // Line from long  view

      if( ls->chksum() == ll->chksum() ) // Lines are now equal
      {
        cDI.diff_type = DT_SAME;
        oDI.diff_type = DT_SAME;
      }
      else { // Lines are different
        if( !sDI.pLineInfo ) sDI.pLineInfo = Borrow_LineInfo(m,__FILE__,__LINE__);
        if( !lDI.pLineInfo ) lDI.pLineInfo = Borrow_LineInfo(m,__FILE__,__LINE__);

        Compare_Lines( ls, sDI.pLineInfo, ll, lDI.pLineInfo );

        cDI.diff_type = DT_CHANGED;
        oDI.diff_type = DT_CHANGED;
      }
    }
    else {
      unsigned dio_line = DT_DELETED==oDI.diff_type
                        ? oDI.line_num                            // Use current  line number
                        : (0<oDI.line_num ? oDI.line_num-1 : 0 ); // Use previous line number
      // Current side gets current  line number
      Diff_Info dic = { DT_INSERTED, cDI.line_num };
      Diff_Info dio = { DT_DELETED , dio_line };

      bool ok1 = cDI_List.insert( DPL, dic ); ASSERT( __LINE__, ok1, "ok1" );
      bool ok2 = oDI_List.insert( DPL, dio ); ASSERT( __LINE__, ok2, "ok2" );

      // Added a view line, so increment all following view line numbers:
      for( unsigned k=DPL+1; k<cDI_List.len(); k++ )
      {
        cDI_List[ k ].line_num++;
      }
    }
  }
}

//| Action | ThisSide | OtherSide | Action
//--------------------------------------------------------------------------------
//| Delete | SAME     | SAME      | Change this side to DELETED and other side to INSERTED
//|        | CHANGED  | CHANGED   | Change this side to DELETED and other side to INSERTED
//|        | INSERTED | DELETED   | Remove line on both sides
//|        | DELETED  | --------- | Do nothing
void Diff::Patch_Diff_Info_Deleted( View* pV, const unsigned DPL )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Array_t<Diff_Info>& cDI_List = (pV == m.pvS) ? m.DI_List_S : m.DI_List_L; // Current
  Array_t<Diff_Info>& oDI_List = (pV == m.pvS) ? m.DI_List_L : m.DI_List_S; // Other

  Diff_Info& cDI = cDI_List[ DPL ];
  Diff_Info& oDI = oDI_List[ DPL ];

  if( DT_SAME == cDI.diff_type )
  {
    cDI.diff_type = DT_DELETED;
    oDI.diff_type = DT_INSERTED;
  }
  else if( DT_CHANGED == cDI.diff_type )
  {
    cDI.diff_type = DT_DELETED;
    oDI.diff_type = DT_INSERTED;

    Return_LineInfo( m, cDI.pLineInfo ); cDI.pLineInfo = 0;
    Return_LineInfo( m, oDI.pLineInfo ); oDI.pLineInfo = 0;
  }
  else if( DT_INSERTED == cDI.diff_type )
  {
    bool ok1 = cDI_List.remove( DPL );
    bool ok2 = oDI_List.remove( DPL );

    ASSERT( __LINE__, ok1, "ok1" );
    ASSERT( __LINE__, ok2, "ok2" );
  }
  // Removed a view line, so decrement current and all following view line numbers:
  for( unsigned k=DPL; k<cDI_List.len(); k++ )
  {
    cDI_List[ k ].line_num--;
  }
}

//| Action | ThisSide | OtherSide | Action
//--------------------------------------------------------------------------------
//| Change | SAME     | SAME      | Change this side and other side to CHANGED
//|        | CHANGED  | CHANGED   | Compare sides, if same change both to SAME, else leave both CHANGED
//|        | INSERTED | DELETED   | Dont change anything
void Diff::Patch_Diff_Info_Changed( View* pV, const unsigned DPL )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Array_t<Diff_Info>& cDI_List = (pV == m.pvS) ? m.DI_List_S : m.DI_List_L; // Current
  Array_t<Diff_Info>& oDI_List = (pV == m.pvS) ? m.DI_List_L : m.DI_List_S; // Other

  Diff_Info& cDI = cDI_List [ DPL ]; // Current Diff_Info
  Diff_Info& oDI = oDI_List [ DPL ]; // Other   Diff_Info

  Diff_Info& sDI = m.DI_List_S[ DPL ]; // Short   Diff_Info
  Diff_Info& lDI = m.DI_List_L[ DPL ]; // Long    Diff_Info

  const Line* ls = m.pfS->GetLineP( sDI.line_num ); // Line from short view
  const Line* ll = m.pfL->GetLineP( lDI.line_num ); // Line from long  view

  if( DT_SAME == cDI.diff_type )
  {
    if( !sDI.pLineInfo ) sDI.pLineInfo = Borrow_LineInfo(m,__FILE__,__LINE__);
    if( !lDI.pLineInfo ) lDI.pLineInfo = Borrow_LineInfo(m,__FILE__,__LINE__);

    Compare_Lines( ls, sDI.pLineInfo, ll, lDI.pLineInfo );

    cDI.diff_type = DT_CHANGED;
    oDI.diff_type = DT_CHANGED;
  }
  else if( DT_CHANGED == cDI.diff_type )
  {
    if( ls->chksum() == ll->chksum() ) // Lines are now equal
    {
      cDI.diff_type = DT_SAME;
      oDI.diff_type = DT_SAME;

      Return_LineInfo( m, cDI.pLineInfo ); cDI.pLineInfo = 0;
      Return_LineInfo( m, oDI.pLineInfo ); oDI.pLineInfo = 0;
    }
    else { // Lines are still different
      if( !sDI.pLineInfo ) sDI.pLineInfo = Borrow_LineInfo(m,__FILE__,__LINE__);
      if( !lDI.pLineInfo ) lDI.pLineInfo = Borrow_LineInfo(m,__FILE__,__LINE__);

      Compare_Lines( ls, sDI.pLineInfo, ll, lDI.pLineInfo );
    }
  }
}

void InsertAddChar( Diff::Data& m, const char c )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  if( pfb->NumLines()==0 ) pfb->PushLine();

  const unsigned DL = CrsLine(m); // Diff line number

  Array_t<Diff_Info>& cDI_List = (pV == m.pvS) ? m.DI_List_S : m.DI_List_L; // Current
  Diff_Info& cDI = cDI_List[ DL ];

  const unsigned VL = ViewLine( m, pV, DL ); // View line number

  if( DT_DELETED == cDI.diff_type )
  {
    m.crsCol = 0;
    pfb->InsertLine( VL+1 );
    pfb->InsertChar( VL+1, 0, c );
    m.diff.Patch_Diff_Info_Inserted( pV, DL );
  }
  else {
    pfb->InsertChar( VL, CrsChar(m), c );
    m.diff.Patch_Diff_Info_Changed( pV, DL );
  }
  if( WorkingCols( pV ) <= m.crsCol+1 )
  {
    // On last working column, need to scroll right:
    m.leftChar++;
  }
  else {
    m.crsCol += 1;
  }
  m.diff.Update();
}

void InsertAddReturn( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  // The lines in fb do not end with '\n's.
  // When the file is written, '\n's are added to the ends of the lines.
  Line new_line;
  const unsigned DL = CrsLine(m);          // Diff line number
  const unsigned VL = ViewLine( m, pV, DL ); // View line number
  const unsigned OLL = pfb->LineLen( VL ); // Old line length
  const unsigned OCP = CrsChar(m);          // Old cursor position

  for( unsigned k=OCP; k<OLL; k++ )
  {
    const uint8_t C = pfb->RemoveChar( VL, OCP );
    bool ok = new_line.push( C );
    ASSERT( __LINE__, ok, "ok" );
  }
  // Truncate the rest of the old line:
  // Add the new line:
  const unsigned new_line_num = VL+1;
  pfb->InsertLine( new_line_num, new_line );
  m.crsCol = 0;
  m.leftChar = 0;
  if( DL < BotLine( m, pV ) ) m.crsRow++;
  else {
    // If we were on the bottom working line, scroll screen down
    // one line so that the cursor line is not below the screen.
    m.topLine++;
  }
  m.diff.Patch_Diff_Info_Changed( pV, DL );
  m.diff.Patch_Diff_Info_Inserted( pV, DL+1 );
  m.diff.Update();
}

void InsertBackspace_RmC( Diff::Data& m
                        , const unsigned DL
                        , const unsigned OCP )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  const unsigned VL = ViewLine( m, pV, DL ); // View line number

  pfb->RemoveChar( VL, OCP-1 );

  if( 0 < m.crsCol ) m.crsCol -= 1;
  else               m.leftChar -= 1;

  m.diff.Patch_Diff_Info_Changed( pV, DL );
  m.diff.Update();
}

void InsertBackspace_RmNL( Diff::Data& m, const unsigned DL )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  const unsigned VL = ViewLine( m, pV, DL ); // View line number

  // Cursor Line Position is zero, so:
  // 1. Save previous line, end of line + 1 position
  CrsPos ncp = { DL-1, pfb->LineLen( VL-1 ) };

  // 2. Remove the line
  Line lr;
  pfb->RemoveLine( VL, lr );

  // 3. Append rest of line to previous line
  pfb->AppendLineToLine( VL-1, lr );

  // 4. Put cursor at the old previous line end of line + 1 position
  const bool MOVE_UP    = ncp.crsLine < m.topLine;
  const bool MOVE_RIGHT = RightChar( m, pV ) < ncp.crsChar;

  if( MOVE_UP ) m.topLine = ncp.crsLine;
                m.crsRow = ncp.crsLine - m.topLine;

  if( MOVE_RIGHT ) m.leftChar = ncp.crsChar - WorkingCols( pV ) + 1;
                   m.crsCol = ncp.crsChar - m.leftChar;

  // 5. Removed a line, so update to re-draw window view
  m.diff.Patch_Diff_Info_Deleted( pV, DL );
  m.diff.Patch_Diff_Info_Changed( pV, DL-1 );
  m.diff.Update();
}

void InsertBackspace( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  // If no lines in buffer, no backspacing to be done
  if( 0==pfb->NumLines() ) return;

  const unsigned DL = CrsLine(m);  // Diff line

  const unsigned OCP = CrsChar(m); // Old cursor position

  if( OCP ) InsertBackspace_RmC ( m, DL, OCP );
  else      InsertBackspace_RmNL( m, DL );
}

void Swap_Visual_St_Fn_If_Needed( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.inVisualBlock )
  {
    if( m.v_fn_line < m.v_st_line ) Swap( m.v_st_line, m.v_fn_line );
    if( m.v_fn_char < m.v_st_char ) Swap( m.v_st_char, m.v_fn_char );
  }
  else {
    if( m.v_fn_line < m.v_st_line
     || (m.v_fn_line == m.v_st_line && m.v_fn_char < m.v_st_char) )
    {
      // Visual mode went backwards over multiple lines, or
      // Visual mode went backwards over one line
      Swap( m.v_st_line, m.v_fn_line );
      Swap( m.v_st_char, m.v_fn_char );
    }
  }
}

void Do_y_v_block( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  Swap_Visual_St_Fn_If_Needed(m);

  for( unsigned DL=m.v_st_line; DL<=m.v_fn_line; DL++ )
  {
    Line* nlp = m.vis.BorrowLine( __FILE__,__LINE__ );

    const unsigned VL = ViewLine( m, pV, DL );
    const unsigned LL = pfb->LineLen( VL );

    for( unsigned P = m.v_st_char; P<LL && P <= m.v_fn_char; P++ )
    {
      nlp->push( pfb->Get( VL, P ) );
    }
    // m.reg will delete nlp
    m.reg.push( nlp );
  }
  m.vis.SetPasteMode( PM_BLOCK );

  // Try to put cursor at (m.v_st_line, m.v_st_char), but
  // make sure the cursor is in bounds after the deletion:
  const unsigned NUM_LINES = NumLines(m);
  unsigned ncl = m.v_st_line;
  if( NUM_LINES <= ncl ) ncl = NUM_LINES-1;
  const unsigned NLL = pfb->LineLen( ViewLine( m, pV, ncl ) );
  unsigned ncc = 0;
  if( 0<NLL ) ncc = NLL <= m.v_st_char ? NLL-1 : m.v_st_char;

  m.diff.GoToCrsPos_NoWrite( ncl, ncc );
}

void Do_y_v_st_fn( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  Swap_Visual_St_Fn_If_Needed( m );

  for( unsigned L=m.v_st_line; L<=m.v_fn_line; L++ )
  {
    const unsigned LINE_DIFF_TYPE = DiffType( m, pV, L );
    if( LINE_DIFF_TYPE != DT_DELETED )
    {
      Line* nlp = m.vis.BorrowLine( __FILE__,__LINE__ );

      // Convert L, which is diff line, to view line
      const unsigned VL = ViewLine( m, pV, L );
      const unsigned LL = pfb->LineLen( VL );

      if( 0<LL )
      {
        const unsigned P_st = (L==m.v_st_line) ? m.v_st_char : 0;
        const unsigned P_fn = (L==m.v_fn_line) ? Min(LL-1,m.v_fn_char) : LL-1;

        for( unsigned P = P_st; P <= P_fn; P++ )
        {
          nlp->push( pfb->Get( VL, P ) );
        }
      }
      // m.reg will delete nlp
      m.reg.push( nlp );
    }
  }
  m.vis.SetPasteMode( PM_ST_FN );
}

void Do_y_v( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.reg.clear();

  if( m.inVisualBlock ) Do_y_v_block(m);
  else                  Do_y_v_st_fn(m);

  m.inVisualMode = false;
}

void Do_Y_v_st_fn( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  if( m.v_fn_line < m.v_st_line ) Swap( m.v_st_line, m.v_fn_line );

  for( unsigned L=m.v_st_line; L<=m.v_fn_line; L++ )
  {
    const unsigned LINE_DIFF_TYPE = DiffType( m, pV, L );
    if( LINE_DIFF_TYPE != DT_DELETED )
    {
      Line* nlp = m.vis.BorrowLine( __FILE__,__LINE__ );

      // Convert L, which is diff line, to view line
      const unsigned VL = ViewLine( m, pV, L );
      const unsigned LL = pfb->LineLen( VL );

      if( 0<LL )
      {
        for( unsigned P = 0; P <= LL-1; P++ )
        {
          nlp->push(  pfb->Get( VL, P ) );
        }
      }
      // m.reg will delete nlp
      m.reg.push( nlp );
    }
  }
  m.vis.SetPasteMode( PM_LINE );
}

void Do_Y_v( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.reg.clear();

  if( m.inVisualBlock ) Do_y_v_block(m);
  else                  Do_Y_v_st_fn(m);

  m.inVisualMode = false;
}

void Do_D_v_find_new_crs_pos( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  Array_t<Diff_Info>& cDI_List = (pV == m.pvS) ? m.DI_List_S : m.DI_List_L; // Current diff info list

  // Figure out new cursor position:
  unsigned ncld = m.v_fn_line+1;
  if( cDI_List.len()-1 < ncld ) ncld = cDI_List.len()-1;

  const unsigned nclv = ViewLine( m, pV, ncld );
  const unsigned NCLL = pfb->LineLen( nclv );

  unsigned ncc = 0;
  if( NCLL ) ncc = m.v_st_char < NCLL ? m.v_st_char : NCLL-1;

  m.diff.GoToCrsPos_NoWrite( ncld, ncc );
}

void Do_D_v( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  m.reg.clear();
  Swap_Visual_St_Fn_If_Needed(m);

  bool removed_line = false;
  Array_t<Diff_Info>& cDI_List = (pV == m.pvS) ? m.DI_List_S : m.DI_List_L; // Current diff info list
  Array_t<Diff_Info>& oDI_List = (pV == m.pvS) ? m.DI_List_L : m.DI_List_S; // Other   diff info list
  const unsigned VL = ViewLine( m, pV, m.v_st_line ); // View line

  // To avoid crashing, dont remove all lines in file
  for( unsigned DL = m.v_st_line; 1 < pfb->NumLines() && DL<=m.v_fn_line; DL++ )
  {
    const Diff_Type cDT = cDI_List[DL].diff_type; // Current diff type
    const Diff_Type oDT = oDI_List[DL].diff_type; // Other   diff type

    if( cDT == DT_SAME
     || cDT == DT_CHANGED
     || cDT == DT_INSERTED )
    {
      Line* lp = pfb->RemoveLineP( VL );
      m.reg.push( lp ); // m.reg will delete lp

      m.diff.Patch_Diff_Info_Deleted( pV, DL );

      removed_line = true;
      // If line on other side is DT_DELETED, a diff line will be removed
      // from both sides, so decrement DL to stay on same DL, decrement
      // m.v_fn_line because it just moved up a line
      if( oDT == DT_DELETED ) { DL--; m.v_fn_line--; }
    }
  }
  m.vis.SetPasteMode( PM_LINE );

  // Deleted lines will be removed, so no need to Undo_v()
  m.inVisualMode = false;

  if( removed_line )
  {
    Do_D_v_find_new_crs_pos(m);
    if( !m.diff.ReDiff() ) m.diff.Update();
  }
}

void Do_x_range_pre( Diff::Data& m
                   , unsigned& st_line, unsigned& st_char
                   , unsigned& fn_line, unsigned& fn_char )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.inVisualBlock )
  {
    if( fn_line < st_line ) Swap( st_line, fn_line );
    if( fn_char < st_char ) Swap( st_char, fn_char );
  }
  else {
    if( fn_line < st_line
     || (fn_line == st_line && fn_char < st_char) )
    {
      // Visual mode went backwards over multiple lines, or
      // Visual mode went backwards over one line
      Swap( st_line, fn_line );
      Swap( st_char, fn_char );
    }
  }
  m.reg.clear();
}

void Do_x_range_post( Diff::Data& m
                    , const unsigned st_line
                    , const unsigned st_char )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.inVisualBlock ) m.vis.SetPasteMode( PM_BLOCK );
  else                  m.vis.SetPasteMode( PM_ST_FN );

  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  Array_t<Diff_Info>& cDI_List = (pV == m.pvS) ? m.DI_List_S : m.DI_List_L; // Current diff info list

  // Make sure the cursor is in bounds after the deletion:
  unsigned ncld = st_line;
  if( cDI_List.len()-1 < ncld ) ncld = cDI_List.len()-1;

  const unsigned nclv = ViewLine( m, pV, ncld ); // New cursor line view
  const unsigned NCLL = pfb->LineLen( nclv );

  unsigned ncc = 0;
  if( 0<NCLL ) ncc = st_char < NCLL ? st_char : NCLL-1;

  m.diff.GoToCrsPos_NoWrite( ncld, ncc );

  m.inVisualMode = false;

  if( !m.diff.ReDiff() ) m.diff.Update(); //<- No need to Undo_v() or Remove_Banner() because of this
}

void Do_x_range_block( Diff::Data& m
                     , unsigned st_line, unsigned st_char
                     , unsigned fn_line, unsigned fn_char )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  Do_x_range_pre( m, st_line, st_char, fn_line, fn_char );

  for( int DL = st_line; DL<=fn_line; DL++ )
  {
    const int VL = ViewLine( m, pV, DL ); // View line

    Line* nlp = m.vis.BorrowLine( __FILE__,__LINE__ );

    const int LL = pfb->LineLen( VL );

    for( int P = st_char; P<LL && P <= fn_char; P++ )
    {
      nlp->push( pfb->RemoveChar( VL, st_char ) );
    }
    m.reg.push( nlp );
  }
  Do_x_range_post( m, st_line, st_char );
}

void Do_x_range_single( Diff::Data& m
                      , const unsigned DL
                      , const unsigned st_char
                      , const unsigned fn_char )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  const unsigned VL = ViewLine( m, pV, DL ); // View line

  Line* nlp = m.vis.BorrowLine( __FILE__,__LINE__ );

  unsigned LL = pfb->LineLen( VL );

  // Dont remove a single line, or else Q wont work right
  bool removed_char = false;

  for( unsigned P = st_char; st_char < LL && P <= fn_char; P++ )
  {
    nlp->push( pfb->RemoveChar( VL, st_char ) );
    LL = pfb->LineLen( VL ); // Removed a char, so re-set LL
    removed_char = true;
  }
  if( removed_char ) m.diff.Patch_Diff_Info_Changed( pV, DL );

  m.reg.push( nlp );
}

void Do_x_range_multiple( Diff::Data& m
                        , const unsigned st_line
                        , const unsigned st_char
                        , const unsigned fn_line
                        , const unsigned fn_char )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  Array_t<Diff_Info>& cDI_List = (pV == m.pvS) ? m.DI_List_S : m.DI_List_L; // Current diff info list
  Array_t<Diff_Info>& oDI_List = (pV == m.pvS) ? m.DI_List_L : m.DI_List_S; // Other   diff info list

  bool started_in_middle = false;
  bool ended___in_middle = false;

  unsigned n_fn_line = fn_line; // New finish line

  for( unsigned DL = st_line; DL<=n_fn_line; DL++ )
  {
    const Diff_Type cDT = cDI_List[DL].diff_type; // Current diff type
    const Diff_Type oDT = oDI_List[DL].diff_type; // Other diff type

    if( cDT != DT_SAME       // If cDT is UNKN0WN or DELETED,
     && cDT != DT_CHANGED    // nothing to do so continue
     && cDT != DT_INSERTED ) continue;

    const unsigned VL  = ViewLine( m, pV, DL ); // View line
    const unsigned OLL = pfb->LineLen( VL ); // Original line length

    Line* nlp = m.vis.BorrowLine( __FILE__,__LINE__ );

    const unsigned P_st = (DL==  st_line) ? Min(st_char,OLL-1) : 0;
    const unsigned P_fn = (DL==n_fn_line) ? Min(fn_char,OLL-1) : OLL-1;

    if(   st_line == DL && 0    < P_st  ) started_in_middle = true;
    if( n_fn_line == DL && P_fn < OLL-1 ) ended___in_middle = true;

    bool removed_char = false;
    unsigned LL = OLL;
    for( unsigned P = P_st; P_st < LL && P <= P_fn; P++ )
    {
      nlp->push( pfb->RemoveChar( VL, P_st ) );
      LL = pfb->LineLen( VL ); // Removed a char, so re-calculate LL
      removed_char = true;
    }
    if( 0 == P_st && OLL-1 == P_fn )
    {
      pfb->RemoveLine( VL );
      m.diff.Patch_Diff_Info_Deleted( pV, DL );
      // If line on other side is DT_DELETED, a diff line will be removed
      // from both sides, so decrement DL to stay on same DL, decrement
      // n_fn_line because it just moved up a line
      if( oDT == DT_DELETED ) { DL--; n_fn_line--; }
    }
    else {
      if( removed_char ) m.diff.Patch_Diff_Info_Changed( pV, DL );
    }
    m.reg.push( nlp );
  }
  if( started_in_middle && ended___in_middle )
  {
    const unsigned v_st_line  = ViewLine( m, pV, st_line ); // View line start
    const unsigned v_fn_line  = ViewLine( m, pV, fn_line ); // View line finish

    Line lr;
    pfb->RemoveLine( v_fn_line, lr );
    pfb->AppendLineToLine( v_st_line, lr );

    m.diff.Patch_Diff_Info_Deleted( pV, fn_line );
    m.diff.Patch_Diff_Info_Changed( pV, st_line );
  }
}

void Do_x_range( Diff::Data& m
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

void Do_x_v( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.inVisualBlock )
  {
    Do_x_range_block( m, m.v_st_line, m.v_st_char, m.v_fn_line, m.v_fn_char );
  }
  else {
    Do_x_range( m, m.v_st_line, m.v_st_char, m.v_fn_line, m.v_fn_char );
  }
}

// Returns true if character was removed
void InsertBackspace_vb( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  const unsigned DL = CrsLine( m );          // Diff line number
  const unsigned VL = ViewLine( m, pV, DL ); // View line number
  const unsigned CP = CrsChar( m );          // Cursor position

  if( 0<CP )
  {
    const unsigned N_REG_LINES = m.reg.len();

    for( unsigned k=0; k<N_REG_LINES; k++ )
    {
      pfb->RemoveChar( VL+k, CP-1 );

      m.diff.Patch_Diff_Info_Changed( pV, DL+k );
    }
    m.diff.GoToCrsPos_NoWrite( DL, CP-1 );
  }
}

void InsertAddChar_vb( Diff::Data& m, const char c )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  const unsigned DL = CrsLine( m );          // Diff line number
  const unsigned VL = ViewLine( m, pV, DL ); // View line number
  const unsigned CP = CrsChar( m );          // Cursor position

  const unsigned N_REG_LINES = m.reg.len();

  for( unsigned k=0; k<N_REG_LINES; k++ )
  {
    const unsigned LL = pfb->LineLen( VL+k );

    if( LL < CP )
    {
      // Fill in line with white space up to CP:
      for( unsigned i=0; i<(CP-LL); i++ )
      {
        // Insert at end of line so undo will be atomic:
        const unsigned NLL = pfb->LineLen( VL+k ); // New line length
        pfb->InsertChar( VL+k, NLL, ' ' );
      }
    }
    pfb->InsertChar( VL+k, CP, c );

    m.diff.Patch_Diff_Info_Changed( pV, DL+k );
  }
  m.diff.GoToCrsPos_NoWrite( DL, CP+1 );
}

void Do_i_vb( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  pV->SetInsertMode( true );
  DisplayBanner( m );

  unsigned count = 0;
  for( char c=m.key.In(); c != ESC; c=m.key.In() )
  {
    if( IsEndOfLineDelim( c ) )
    {
      ; // Ignore end of line delimiters
    }
    else if( BS  == c || DEL == c )
    {
      if( 0<count )
      {
        InsertBackspace_vb( m );
        count--;
        m.diff.Update();
      }
    }
    else {
      InsertAddChar_vb( m, c );
      count++;
      m.diff.Update();
    }
  }
  Remove_Banner(m);
  pV->SetInsertMode( false );
}

void Do_a_vb( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  const unsigned DL = CrsLine( m );          // Diff line number
  const unsigned VL = ViewLine( m, pV, DL ); // View line number
  const unsigned LL = pfb->LineLen( VL );

  if( 0==LL ) { Do_i_vb(m); return; }

  const bool CURSOR_AT_EOL = ( CrsChar( m ) == LL-1 );
  if( CURSOR_AT_EOL )
  {
    m.diff.GoToCrsPos_NoWrite( DL, LL );
  }
  const bool CURSOR_AT_RIGHT_COL = ( m.crsCol == WorkingCols( pV )-1 );

  if( CURSOR_AT_RIGHT_COL )
  {
    // Only need to scroll window right, and then enter insert i:
    m.leftChar++; //< This increments CrsChar( m )
  }
  else if( !CURSOR_AT_EOL ) // If cursor was at EOL, already moved cursor forward
  {
    // Only need to move cursor right, and then enter insert i:
    m.crsCol += 1; //< This increments CrsChar( m )
  }
  m.diff.Update();

  Do_i_vb(m);
}

bool Do_s_v_cursor_at_end_of_line( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  const unsigned DL = CrsLine( m );  // Diff line
  const unsigned VL = ViewLine( m, pV, DL );
  const unsigned LL = pfb->LineLen( VL );

  if( m.inVisualBlock )
  {
    return 0<LL ? LL-1 <= CrsChar(m)
                : 0    <  CrsChar(m);
  }
  return 0<LL ? CrsChar(m) == LL-1 : false;
}

void Do_s_v( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Need to know if cursor is at end of line before Do_x_v() is called:
  const bool CURSOR_AT_END_OF_LINE = Do_s_v_cursor_at_end_of_line(m);

  Do_x_v(m);

  if( m.inVisualBlock )
  {
    if( CURSOR_AT_END_OF_LINE ) Do_a_vb( m );
    else                        Do_i_vb( m );
  }
  else {
    if( CURSOR_AT_END_OF_LINE ) m.diff.Do_a();
    else                        m.diff.Do_i();
  }
  m.inVisualMode = false;
}

void Do_Tilda_v_block( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  for( unsigned L = m.v_st_line; L<=m.v_fn_line; L++ )
  {
    const unsigned LL = pfb->LineLen( L );

    for( unsigned P = m.v_st_char; P<LL && P <= m.v_fn_char; P++ )
    {
      char C = pfb->Get( L, P );
      bool changed = false;
      if     ( isupper( C ) ) { C = tolower( C ); changed = true; }
      else if( islower( C ) ) { C = toupper( C ); changed = true; }
      if( changed ) pfb->Set( L, P, C );
    }
  }
}

void Do_Tilda_v_st_fn( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  for( unsigned DL = m.v_st_line; DL<=m.v_fn_line; DL++ )
  {
    const unsigned VL = ViewLine( m, pV, DL );
    const unsigned LL = pfb->LineLen( VL );
    const unsigned P_st = (DL==m.v_st_line) ? m.v_st_char : 0;
    const unsigned P_fn = (DL==m.v_fn_line) ? m.v_fn_char : (0<LL?LL-1:0);

    bool changed_line = false;

    for( unsigned P = P_st; P <= P_fn; P++ )
    {
      char C = pfb->Get( VL, P );
      bool changed = false;
      if     ( isupper( C ) ) { C = tolower( C ); changed = true; }
      else if( islower( C ) ) { C = toupper( C ); changed = true; }

      if( changed )
      {
        pfb->Set( VL, P, C );
        changed_line = true;
      }
    }
    if( changed_line )
    {
      m.diff.Patch_Diff_Info_Changed( pV, DL );
    }
  }
}

void Do_Tilda_v( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Swap_Visual_St_Fn_If_Needed( m );

  if( m.inVisualBlock ) Do_Tilda_v_block(m);
  else                  Do_Tilda_v_st_fn(m);

  m.inVisualMode = false;
}

bool Diff::On_Deleted_View_Line_Zero( const unsigned DL )
{
  Trace trace( __PRETTY_FUNCTION__ );

  bool ODVL0 = false; // On Deleted View Line Zero

  View* pV = m.vis.CV();
  Array_t<Diff_Info>& cDI_List = (pV == m.pvS) ? m.DI_List_S : m.DI_List_L; // Current
  Diff_Info& cDI = cDI_List[ DL ];

  if( DT_DELETED == cDI.diff_type )
  {
    ODVL0 = true;

    for( unsigned k=0; ODVL0 && k<DL; k++ )
    {
      if( DT_DELETED != cDI_List[ k ].diff_type ) ODVL0 = false;
    }
  }
  return ODVL0;
}

void Do_p_line( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  const unsigned DL = CrsLine(m);          // Diff line
  const unsigned VL = ViewLine( m, pV, DL ); // View line

  const unsigned NUM_LINES_TO_INSERT = m.reg.len();

  Array_t<Diff_Info>& cDI_List = (pV == m.pvS) ? m.DI_List_S : m.DI_List_L; // Current diff info list
  Diff_Info& cDI = cDI_List[ DL ];

  // If cursor is on a deleted diff line, start inserting lines into that deleted diff line
  // If cursor is NOT on a deleted diff line, start inserting lines below diff cursor line
  const bool ON_DELETED = DT_DELETED == cDI.diff_type;
        bool ODVL0 = m.diff.On_Deleted_View_Line_Zero( DL );
  const unsigned DL_START = ON_DELETED ? DL : DL+1;
  const unsigned VL_START = ODVL0      ? VL : VL+1;

  for( unsigned k=0; k<NUM_LINES_TO_INSERT; k++ )
  {
    // In FileBuf: Put reg on line below:
    pfb->InsertLine( VL_START+k, *(m.reg[k]) );

    m.diff.Patch_Diff_Info_Inserted( pV, DL_START+k, ODVL0 );
    ODVL0 = false;
  }
  if( !m.diff.ReDiff() ) m.diff.Update();
}

void Do_p_or_P_st_fn_FirstLine( Diff::Data& m
                              , Paste_Pos      paste_pos
                              , const unsigned k
                              , const unsigned ODL
                              , const unsigned OVL
                              , const bool     ON_DELETED )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  const unsigned NUM_LINES = m.reg.len();

  const unsigned NLL = m.reg[ k ]->len();  // New line length
  const unsigned VL  = ViewLine( m, pV, ODL+k ); // View line

  if( ON_DELETED )
  {
    const bool ODVL0 = m.diff.On_Deleted_View_Line_Zero( ODL );

    // In FileBuf: Put reg on line below:
    pfb->InsertLine( ODVL0 ? VL : VL+1, *(m.reg[0]) );

    m.diff.Patch_Diff_Info_Inserted( pV, ODL+k, ODVL0 );
  }
  else {
    MoveInBounds_Line(m);
    const unsigned LL = pfb->LineLen( VL );
    const unsigned CP = CrsChar(m);         // Cursor position

    // If line we are pasting to is zero length, dont paste a space forward
    const unsigned forward = 0<LL ? ( paste_pos==PP_After ? 1 : 0 ) : 0;

    for( unsigned i=0; i<NLL; i++ )
    {
      char C = m.reg[k]->get(i);

      pfb->InsertChar( VL, CP+i+forward, C );
    }
    m.diff.Patch_Diff_Info_Changed( pV, ODL+k );

    // Move rest of first line onto new line below
    if( 1 < NUM_LINES && CP+forward < LL )
    {
      pfb->InsertLine( VL+1 );
      for( unsigned i=0; i<(LL-CP-forward); i++ )
      {
        char C = pfb->RemoveChar( VL, CP + NLL+forward );
        pfb->PushChar( VL+1, C );
      }
      m.diff.Patch_Diff_Info_Inserted( pV, ODL+k+1, false ); //< Always false since we are starting on line below
    }
  }
}

void Do_p_or_P_st_fn_LastLine( Diff::Data& m
                             , const unsigned k
                             , const unsigned ODL
                             , const unsigned OVL
                             , const bool     ON_DELETED )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  const unsigned VL  = ViewLine( m, pV, ODL+k ); // View line
  const unsigned NLL = m.reg[ k ]->len();  // New line length

  if( ON_DELETED )
  {
    pfb->InsertLine( VL+1, *(m.reg[k]) );
    m.diff.Patch_Diff_Info_Inserted( pV, ODL+k, false );
  }
  else {
    for( unsigned i=0; i<NLL; i++ )
    {
      char C = m.reg[k]->get(i);
      pfb->InsertChar( VL, i, C );
    }
    m.diff.Patch_Diff_Info_Changed( pV, ODL+k );
  }
}

void Do_p_or_P_st_fn_IntermediatLine( Diff::Data& m
                                    , const unsigned k
                                    , const unsigned ODL
                                    , const unsigned OVL
                                    , const bool     ON_DELETED )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  const unsigned NUM_LINES = m.reg.len();

  const unsigned NLL = m.reg[ k ]->len(); // New line length
  const unsigned VL  = ViewLine( m, pV, ODL+k );    // View line

  if( ON_DELETED )
  {
    // In FileBuf: Put reg on line below:
    pfb->InsertLine( VL+1, *( m.reg[k] ) );

    m.diff.Patch_Diff_Info_Inserted( pV, ODL+k, false );
  }
  else {
    MoveInBounds_Line(m);
    const unsigned LL = pfb->LineLen( VL );

    for( unsigned i=0; i<NLL; i++ )
    {
      char C = m.reg[k]->get(i);

      pfb->InsertChar( VL, i, C );
    }
    m.diff.Patch_Diff_Info_Changed( pV, ODL+k );

    // Move rest of first line onto new line below
    if( 1 < NUM_LINES && 0 < LL )
    {
      pfb->InsertLine( VL+1 );
      for( unsigned i=0; i<LL; i++ )
      {
        char C = pfb->RemoveChar( VL, NLL );
        pfb->PushChar( VL+1, C );
      }
      m.diff.Patch_Diff_Info_Inserted( pV, ODL+k+1, false ); //< Always false since we are starting on line below
    }
  }
}

void Do_p_or_P_st_fn( Diff::Data& m, Paste_Pos paste_pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  const unsigned NUM_LINES = m.reg.len();
  const unsigned ODL       = CrsLine(m);           // Original Diff line
  const unsigned OVL       = ViewLine( m, pV, ODL ); // Original View line

  Array_t<Diff_Info>& cDI_List = (pV == m.pvS) ? m.DI_List_S : m.DI_List_L; // Current

  for( unsigned k=0; k<NUM_LINES; k++ )
  {
    Diff_Info& cDI = cDI_List[ ODL+k ];

    const bool ON_DELETED = DT_DELETED == cDI.diff_type;

    if( 0 == k ) // Add to current line
    {
      Do_p_or_P_st_fn_FirstLine( m, paste_pos, k, ODL, OVL, ON_DELETED );
    }
    else if( NUM_LINES-1 == k ) // Last line
    {
      Do_p_or_P_st_fn_LastLine( m, k, ODL, OVL, ON_DELETED );
    }
    else { // Intermediate line
      Do_p_or_P_st_fn_IntermediatLine( m, k, ODL, OVL, ON_DELETED );
    }
  }
  if( !m.diff.ReDiff() ) m.diff.Update();
}

void Do_p_block_Change_Line( Diff::Data& m
                           , const unsigned k
                           , const unsigned DL
                           , const unsigned VL
                           , const unsigned ISP )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  const unsigned LL_k = pfb->LineLen( VL+k );

  if( LL_k < ISP )
  {
    // Fill in line with white space up to ISP:
    for( unsigned i=0; i<(ISP-LL_k); i++ )
    {
      // Insert at end of line so undo will be atomic:
      const unsigned NLL = pfb->LineLen( VL+k ); // New line length
      pfb->InsertChar( VL+k, NLL, ' ' );
    }
  }
  Line* reg_line = m.reg[k];
  const unsigned RLL = reg_line->len();

  for( unsigned i=0; i<RLL; i++ )
  {
    char C = reg_line->get(i);

    pfb->InsertChar( VL+k, ISP+i, C );
  }
  m.diff.Patch_Diff_Info_Changed( pV, DL+k );
}

void Do_p_block_Insert_Line( Diff::Data& m
                           , const unsigned k
                           , const unsigned DL
                           , const unsigned VL
                           , const unsigned ISP )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  pfb->InsertLine( VL+k );

  const unsigned LL_k = pfb->LineLen( VL+k );

  if( LL_k < ISP )
  {
    // Fill in line with white space up to ISP:
    for( unsigned i=0; i<(ISP-LL_k); i++ )
    {
      // Insert at end of line so undo will be atomic:
      const unsigned NLL = pfb->LineLen( VL+k ); // New line length
      pfb->InsertChar( VL+k, NLL, ' ' );
    }
  }
  Line* reg_line = m.reg[k];
  const unsigned RLL = reg_line->len();

  for( unsigned i=0; i<RLL; i++ )
  {
    char C = reg_line->get(i);

    pfb->InsertChar( VL+k, ISP+i, C );
  }
  const bool ODVL0 = m.diff.On_Deleted_View_Line_Zero( DL+k );

  m.diff.Patch_Diff_Info_Inserted( pV, DL+k, ODVL0 );
}

void Do_p_block( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  Array_t<Diff_Info>& cDI_List = (pV == m.pvS) ? m.DI_List_S : m.DI_List_L; // Current

  const unsigned DL = CrsLine(m);          // Diff line
  const unsigned CP = CrsChar(m);          // Cursor position
  const unsigned VL = ViewLine( m, pV, DL ); // View line
  const bool ON_DELETED = DT_DELETED == cDI_List[ DL ].diff_type;
  const unsigned LL = ON_DELETED ? 0 : pfb->LineLen( VL ); // Line length
  const unsigned ISP = 0<CP ? CP+1    // Insert position
                     : ( 0<LL ? 1:0 );// If at beginning of line,
                                      // and LL is zero insert at 0,
                                      // else insert at 1
  const unsigned N_REG_LINES = m.reg.len();

  for( unsigned k=0; k<N_REG_LINES; k++ )
  {
    if( VL+k < pfb->NumLines()
     && DT_DELETED != cDI_List[ DL+k ].diff_type )
    {
      Do_p_block_Change_Line( m, k, DL, VL, ISP );
    }
    else {
      Do_p_block_Insert_Line( m, k, DL, 0<VL?VL+1:0, ISP );
    }
  }
  if( !m.diff.ReDiff() ) m.diff.Update();
}

void Do_P_line( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const int DL = CrsLine(m); // Diff line

  // Move to line above, and then do 'p':
  if( 0<DL ) m.diff.GoToCrsPos_NoWrite( DL-1, CrsChar(m) );

  Do_p_line(m);
}

void Do_P_block( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  Array_t<Diff_Info>& cDI_List = (pV == m.pvS) ? m.DI_List_S : m.DI_List_L; // Current

  const unsigned DL = CrsLine(m);          // Diff line
  const unsigned CP = CrsChar(m);          // Cursor position
  const unsigned VL = ViewLine( m, pV, DL ); // View line
  const bool     ON_DELETED = DT_DELETED == cDI_List[ DL ].diff_type;
  const unsigned LL = ON_DELETED ? 0 : pfb->LineLen( VL ); // Line length
  const unsigned ISP = 0<CP ? CP : 0;     // Insert position

  const unsigned N_REG_LINES = m.reg.len();

  for( unsigned k=0; k<N_REG_LINES; k++ )
  {
    if( VL+k < pfb->NumLines()
     && DT_DELETED != cDI_List[ DL+k ].diff_type )
    {
      Do_p_block_Change_Line( m, k, DL, VL, ISP );
    }
    else {
      Do_p_block_Insert_Line( m, k, DL, 0<VL?VL+1:0, ISP );
    }
  }
  if( !m.diff.ReDiff() ) m.diff.Update();
}

void Replace_Crs_Char( Diff::Data& m, Style style )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  // Convert CL, which is diff line, to view line:
  const unsigned CLv = ViewLine( m, pV, CrsLine(m) );

  const unsigned LL = pfb->LineLen( CLv ); // Line length
  if( LL )
  {
    int byte = pfb->Get( CLv, CrsChar(m) );

    Console::Set( Row_Win_2_GL( m, pV, CrsLine(m)-m.topLine )
                , Col_Win_2_GL( m, pV, CrsChar(m)-m.leftChar )
                , byte, style );
  }
}

void Do_v_Handle_gf( Diff::Data& m )
{
  if( m.v_st_line == m.v_fn_line )
  {
    View*    pV  = m.vis.CV();
    FileBuf* pfb = pV->GetFB();

    Swap_Visual_St_Fn_If_Needed(m);

    const int VL = ViewLine( m, pV, m.v_st_line );

    String fname;

    for( unsigned P = m.v_st_char; P<=m.v_fn_char; P++ )
    {
      fname.push( pfb->Get( VL, P  ) );
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

void Do_v_Handle_gp( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.v_st_line == m.v_fn_line )
  {
    View*    pV  = m.vis.CV();
    FileBuf* pfb = pV->GetFB();

    Swap_Visual_St_Fn_If_Needed(m);

    const int VL = ViewLine( m, pV, m.v_st_line );

    String pattern;

    for( int P = m.v_st_char; P<=m.v_fn_char; P++ )
    {
      pattern.push( pfb->Get( VL, P ) );
    }
    m.vis.Handle_Slash_GotPattern( pattern, false );

    m.inVisualMode = false;
  }
}

void Do_v_Handle_g( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const char CC2 = m.key.In();

  if     ( CC2 == 'g' ) m.diff.GoToTopOfFile();
  else if( CC2 == '0' ) m.diff.GoToStartOfRow();
  else if( CC2 == '$' ) m.diff.GoToEndOfRow();
  else if( CC2 == 'f' ) Do_v_Handle_gf(m);
  else if( CC2 == 'p' ) Do_v_Handle_gp(m);
}

// This one works better when IN visual mode:
void PageDown_v( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = NumLines(m);

  if( 0<NUM_LINES )
  {
    const unsigned OCLd = CrsLine(m); // Old cursor line diff

    unsigned NCLd = OCLd + WorkingRows( m.vis.CV() ) - 1; // New cursor line diff

    // Dont let cursor go past the end of the file:
    if( NUM_LINES-1 < NCLd ) NCLd = NUM_LINES-1;

    GoToCrsPos_Write( m, NCLd, 0 );
  }
}

// This one works better when IN visual mode:
void PageUp_v( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = NumLines(m);

  if( 0<NUM_LINES )
  {
    const unsigned OCLd = CrsLine(m); // Old cursor line diff

    int NCLd = OCLd - WorkingRows( m.vis.CV() ) + 1; // New cursor line diff

    // Check for underflow:
    if( NCLd < 0 ) NCLd = 0;

    GoToCrsPos_Write( m, NCLd, 0 );
  }
}

void Undo_v( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.undo_v )
  {
    m.diff.Update();
  }
}

bool Do_visualMode( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  MoveInBounds_Line(m);
  m.inVisualMode = true;
  m.undo_v     = true;
  DisplayBanner(m);

  m.v_st_line = CrsLine(m);  m.v_fn_line = m.v_st_line;
  m.v_st_char = CrsChar(m);  m.v_fn_char = m.v_st_char;

  // Write current byte in visual:
  Replace_Crs_Char( m, S_RV_VISUAL );

  while( m.inVisualMode )
  {
    const char C=m.key.In();

    if     ( C == 'l' ) m.diff.GoRight();
    else if( C == 'h' ) m.diff.GoLeft();
    else if( C == 'j' ) m.diff.GoDown();
    else if( C == 'k' ) m.diff.GoUp();
    else if( C == 'H' ) m.diff.GoToTopLineInView();
    else if( C == 'L' ) m.diff.GoToBotLineInView();
    else if( C == 'M' ) m.diff.GoToMidLineInView();
    else if( C == 'n' ) m.diff.Do_n();
    else if( C == 'N' ) m.diff.Do_N();
    else if( C == '0' ) m.diff.GoToBegOfLine();
    else if( C == '$' ) m.diff.GoToEndOfLine();
    else if( C == 'g' )        Do_v_Handle_g(m);
    else if( C == 'G' ) m.diff.GoToEndOfFile();
    else if( C == 'F' )        PageDown_v(m);
    else if( C == 'B' )        PageUp_v(m);
    else if( C == 'b' ) m.diff.GoToPrevWord();
    else if( C == 'w' ) m.diff.GoToNextWord();
    else if( C == 'e' ) m.diff.GoToEndOfWord();
    else if( C == '%' ) m.diff.GoToOppositeBracket();
    else if( C == 'z' ) m.vis.Handle_z();
    else if( C == 'f' ) m.vis.Handle_f();
    else if( C == ';' ) m.vis.Handle_SemiColon();
    else if( C == 'y' ) { Do_y_v(m); goto EXIT_VISUAL; }
    else if( C == 'Y' ) { Do_Y_v(m); goto EXIT_VISUAL; }
    else if( C == 'x'
          || C == 'd' ) { Do_x_v(m);     return true; }
    else if( C == 'D' ) { Do_D_v(m);     return true; }
    else if( C == 's' ) { Do_s_v(m);     return true; }
    else if( C == '~' ) { Do_Tilda_v(m); return true; }
    else if( C == ESC ) { goto EXIT_VISUAL; }
  }
  return false;

EXIT_VISUAL:
  m.inVisualMode = false;
  Undo_v(m);
  Remove_Banner(m);
  return false;
}

void ReplaceAddReturn( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();
  // The lines in fb do not end with '\n's.
  // When the file is written, '\n's are added to the ends of the lines.
  Line new_line;
  const unsigned ODL = CrsLine(m);
  const unsigned OVL = ViewLine( m, pV, ODL ); // View line number
  const unsigned OLL = pfb->LineLen( OVL );
  const unsigned OCP = CrsChar(m);

  for( unsigned k=OCP; k<OLL; k++ )
  {
    const uint8_t C = pfb->RemoveChar( OVL, OCP );
    bool ok = new_line.push( C );
    ASSERT( __LINE__, ok, "ok" );
  }
  // Truncate the rest of the old line:
  // Add the new line:
  const unsigned new_line_num = OVL+1;
  pfb->InsertLine( new_line_num, new_line );

  m.diff.GoToCrsPos_NoWrite( ODL+1, 0 );

  m.diff.Patch_Diff_Info_Changed( pV, ODL );
  m.diff.Patch_Diff_Info_Inserted( pV, ODL+1 );
  m.diff.Update();
}

void ReplaceAddChar_ON_DELETED( Diff::Data& m
                              , const char C
                              , const unsigned DL
                              , const unsigned VL )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  Array_t<Diff_Info>& cDI_List = (pV == m.pvS) ? m.DI_List_S : m.DI_List_L; // Current

  const bool ODVL0 = m.diff.On_Deleted_View_Line_Zero( DL );

  Line* nlp = m.vis.BorrowLine(__FILE__,__LINE__);
  nlp->push( C );
  pfb->InsertLine( ODVL0 ? VL : VL+1, nlp );
  m.diff.Patch_Diff_Info_Inserted( pV, DL, ODVL0 );
}

void ReplaceAddChar( Diff::Data& m, const char C )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  if( pfb->NumLines()==0 ) pfb->PushLine();

  const unsigned DL = CrsLine(m);
  const unsigned VL = ViewLine( m, pV, DL ); // View line number

  Array_t<Diff_Info>& cDI_List = (pV == m.pvS) ? m.DI_List_S : m.DI_List_L; // Current
  Diff_Info& cDI = cDI_List[ DL ];

  const bool ON_DELETED = DT_DELETED == cDI.diff_type;
  if( ON_DELETED )
  {
    ReplaceAddChar_ON_DELETED( m, C, DL, VL );
  }
  else {
    const unsigned CP = CrsChar(m);
    const unsigned LL = pfb->LineLen( VL );
    const unsigned EOL = 0<LL ? LL-1 : 0;

    if( EOL < CP )
    {
      // Extend line out to where cursor is:
      for( unsigned k=LL; k<CP; k++ ) pfb->PushChar( VL, ' ' );
    }
    // Put char back in file buffer
    const bool continue_last_update = false;
    if( CP < LL ) pfb->Set( VL, CP, C, continue_last_update );
    else {
      pfb->PushChar( VL, C );
    }
    m.diff.Patch_Diff_Info_Changed( pV, DL );
  }
  if( m.crsCol < WorkingCols( pV )-1 )
  {
    m.crsCol++;
  }
  else {
    m.leftChar++;
  }
  m.diff.Update();
}

void RepositionViews( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );
  // If a window re-size has taken place, and the window has gotten
  // smaller, change top line and left char if needed, so that the
  // cursor is in the buffer when it is re-drawn
  View* pV = m.vis.CV();

  if( WorkingRows( pV ) <= m.crsRow )
  {
    m.topLine += ( m.crsRow - WorkingRows( pV ) + 1 );
    m.crsRow  -= ( m.crsRow - WorkingRows( pV ) + 1 );
  }
  if( WorkingCols( pV ) <= m.crsCol )
  {
    m.leftChar += ( m.crsCol - WorkingCols( pV ) + 1 );
    m.crsCol   -= ( m.crsCol - WorkingCols( pV ) + 1 );
  }
}

// st_line_d and fn_line_d are in terms of diff line
bool Do_dw_get_fn( Diff::Data& m
                 , const unsigned  st_line_d, const unsigned  st_char
                 ,       unsigned& fn_line_d,       unsigned& fn_char )
{
  Trace trace( __PRETTY_FUNCTION__ );
  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  const unsigned st_line_v = ViewLine( m, pV, st_line_d );
  const unsigned LL        = pfb->LineLen( st_line_v );
  const uint8_t  C         = pfb->Get( st_line_v, st_char );

  if( IsSpace( C )      // On white space
    || ( st_char < LLM1(LL) // On non-white space before white space
       && IsSpace( pfb->Get( st_line_v, st_char+1 ) ) ) )
  {
    // w:
    CrsPos ncp_w = { 0, 0 };
    bool ok = GoToNextWord_GetPosition( m, ncp_w );

    if( ok && 0 < ncp_w.crsChar ) ncp_w.crsChar--;
    if( ok && st_line_d == ncp_w.crsLine
           && st_char   <= ncp_w.crsChar )
    {
      fn_line_d = ncp_w.crsLine;
      fn_char   = ncp_w.crsChar;
      return true;
    }
  }
  // if not on white space, and
  // not on non-white space before white space,
  // or fell through, try e:
  CrsPos ncp_e = { 0, 0 };
  bool ok = GoToEndOfWord_GetPosition( m, ncp_e );

  if( ok && st_line_d == ncp_e.crsLine
         && st_char   <= ncp_e.crsChar )
  {
    fn_line_d = ncp_e.crsLine;
    fn_char   = ncp_e.crsChar;
    return true;
  }
  return false;
}

bool GetFileName_UnderCursor( Diff::Data& m, String& fname )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();
  bool     got_filename = false;

  const unsigned DL = CrsLine(m);            // Diff line number
  const unsigned VL = ViewLine( m, pV, DL ); // View line number
  const unsigned LL = pfb->LineLen( VL );

  if( 0 < LL )
  {
    MoveInBounds_Line(m);
    const int CP = CrsChar(m);
    char C = pfb->Get( VL, CP );

    if( IsFileNameChar( C ) )
    {
      // Get the file name:
      got_filename = true;

      fname.push( C );

      // Search backwards, until white space is found:
      for( int k=CP-1; -1<k; k-- )
      {
        C = pfb->Get( VL, k );

        if( !IsFileNameChar( C ) ) break;
        else fname.insert( 0, C );
      }
      // Search forwards, until white space is found:
      for( unsigned k=CP+1; k<LL; k++ )
      {
        C = pfb->Get( VL, k );

        if( !IsFileNameChar( C ) ) break;
        else fname.push( C );
      }
      EnvKeys2Vals( fname );
    }
  }
  return got_filename;
}

bool GetBufferIndex( Diff::Data& m
                   , const char* file_path
                   , unsigned* file_index )
{
  // 1. Search for file_path in buffer list
  if( m.vis.HaveFile( file_path, file_index ) )
  {
    return true;
  }
  // 2. See if file exists, and if so, add a file buffer
  if( FileExists( file_path ) )
  {
    // pfb gets added to m.vis.m.files in Add_FileBuf_2_Lists_Create_Views()
    FileBuf* pfb = new(__FILE__,__LINE__)
                   FileBuf( m.vis, file_path, true, FT_UNKNOWN );
    pfb->ReadFile();

    if( m.vis.HaveFile( file_path, file_index ) )
    {
      return true;
    }
  }
  return false;
}

void Print_L( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );
  char fname_L[512];
  sprintf( fname_L, "%s.diff", m.pfL->GetPathName() );
  FILE* fpL = fopen( fname_L, "wb" );

  for( unsigned k=0; fpL && k<m.DI_List_L.len(); k++ )
  {
    Diff_Info di = m.DI_List_L[k];
    const unsigned LN = di.line_num;
    if( DT_UNKN0WN  == di.diff_type )
    {
      fprintf( fpL, "U:-----------------------------");
    }
    else if( DT_SAME     == di.diff_type )
    {
      fprintf( fpL, "S:%u:", LN );
      const unsigned LL = m.pfL->LineLen( LN );
      for( unsigned i=0; i<LL; i++ ) fprintf( fpL, "%c", m.pfL->Get( LN, i ) );
    }
    else if( DT_CHANGED  == di.diff_type )
    {
      fprintf( fpL, "C:%u:", LN );
      const unsigned LL = m.pfL->LineLen( LN );
      for( unsigned i=0; i<LL; i++ ) fprintf( fpL, "%c", m.pfL->Get( LN, i ) );
    }
    else if( DT_INSERTED == di.diff_type )
    {
      fprintf( fpL, "I:%u:", LN );
      const unsigned LL = m.pfL->LineLen( LN );
      for( unsigned i=0; i<LL; i++ ) fprintf( fpL, "%c", m.pfL->Get( LN, i ) );
    }
    else if( DT_DELETED  == di.diff_type )
    {
      fprintf( fpL, "D:-----------------------------");
    }
    fprintf( fpL, "\n" );
  }
  fclose( fpL );
}

void Print_S( Diff::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );
  char fname_S[512];
  sprintf( fname_S, "%s.diff", m.pfS->GetPathName() );
  FILE* fpS = fopen( fname_S, "wb" );

  for( unsigned k=0; fpS && k<m.DI_List_L.len(); k++ )
  {
    Diff_Info di = m.DI_List_S[k];
    const unsigned LN = di.line_num;
    if( DT_UNKN0WN  == di.diff_type )
    {
      fprintf( fpS, "U:-----------------------------");
    }
    else if( DT_SAME     == di.diff_type )
    {
      fprintf( fpS, "S:%u:", LN );
      const unsigned LL = m.pfS->LineLen( LN );
      for( unsigned i=0; i<LL; i++ ) fprintf( fpS, "%c", m.pfS->Get( LN, i ) );
    }
    else if( DT_CHANGED  == di.diff_type )
    {
      fprintf( fpS, "C:%u:", LN );
      const unsigned LL = m.pfS->LineLen( LN );
      for( unsigned i=0; i<LL; i++ ) fprintf( fpS, "%c", m.pfS->Get( LN, i ) );
    }
    else if( DT_INSERTED == di.diff_type )
    {
      fprintf( fpS, "I:%u:", LN );
      const unsigned LL = m.pfS->LineLen( LN );
      for( unsigned i=0; i<LL; i++ ) fprintf( fpS, "%c", m.pfS->Get( LN, i ) );
    }
    else if( DT_DELETED  == di.diff_type )
    {
      fprintf( fpS, "D:-----------------------------");
    }
    fprintf( fpS, "\n" );
  }
  fclose( fpS );
}

void SameArea::Clear()
{
  ln_s   = 0;
  ln_l   = 0;
  nlines = 0;
  nbytes = 0;
}

void SameArea::Init( unsigned _ln_s, unsigned _ln_l, unsigned _nbytes )
{
  ln_s   = _ln_s;
  ln_l   = _ln_l;
  nlines = 1;
  nbytes = _nbytes;
}

void SameArea::Inc( unsigned _nbytes )
{
  nlines += 1;
  nbytes += _nbytes;
}

void SameArea::Set( const SameArea& a )
{
  ln_s   = a.ln_s;
  ln_l   = a.ln_l;
  nlines = a.nlines;
  nbytes = a.nbytes;
}

//void SameLineSec::Init( unsigned _ch_s, unsigned _ch_l )
//{
//  ch_s   = _ch_s;
//  ch_l   = _ch_l;
//  nbytes = 1;
//}

//void SameLineSec::Set( const SameLineSec& a )
//{
//  ch_s   = a.ch_s;
//  ch_l   = a.ch_l;
//  nbytes = a.nbytes;
//}

Diff::Diff( Vis& vis
          , Key& key
          , LinesList& reg )
  : m( *new(__FILE__, __LINE__) Data( *this, vis, key, reg ) )
{
}

Diff::~Diff()
{
  delete &m;
}

// Returns true if diff took place, else false
//
bool Diff::Run( View* const pv0, View* const pv1 )
{
  Trace trace( __PRETTY_FUNCTION__ );

  bool ran_diff = false;
  // Each buffer must be displaying a different file to do diff:
  if( pv0->GetFB() != pv1->GetFB() )
  {
    if( !DiffSameAsPrev( m, pv0, pv1 ) )
    {
      ClearDiff(); //< Start over with clean slate

      Set_ShortLong_ViewfileMod_Vars( m, pv0, pv1 );

      // All lines in both files:
      DiffArea CA( 0, m.pfS->NumLines(), 0, m.pfL->NumLines() );
      RunDiff( m, CA );

      Find_Context(m);
    }
    ran_diff = true;
  }
  return ran_diff;
}

void Diff::Update()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Update long view:
  m.pfL->Find_Styles( ViewLine( m, m.pvL, m.topLine ) + WorkingRows( m.pvL ) );
  m.pfL->Find_Regexs( ViewLine( m, m.pvL, m.topLine ), WorkingRows( m.pvL ) );

  RepositionViews( m );

  m.pvL->Print_Borders();
  PrintWorkingView( m, m.pvL );
  PrintStsLine( m, m.pvL );
  m.pvL->PrintFileLine();
  PrintCmdLine( m, m.pvL );

  // Update short view:
  m.pfS->Find_Styles( ViewLine( m, m.pvS, m.topLine ) + WorkingRows( m.pvS ) );
  m.pfS->Find_Regexs( ViewLine( m, m.pvS, m.topLine ), WorkingRows( m.pvS ) );

  m.pvS->Print_Borders();
  PrintWorkingView( m, m.pvS );
  PrintStsLine( m, m.pvS );
  m.pvS->PrintFileLine();
  PrintCmdLine( m, m.pvS );

  if( ! m.printed_diff_ms )
  {
    m.vis.CmdLineMessage( "Diff took: %u ms", m.diff_ms );
    m.printed_diff_ms = true;
  }
  Console::Update();

  PrintCursor( m.vis.CV() );
}

View* Diff::GetViewShort() const
{
  return m.pvS;
}

View* Diff::GetViewLong() const
{
  return m.pvL;
}

unsigned Diff::GetTopLine( View* const pV ) const
{
  if( pV == m.pvS || pV == m.pvL )
  {
    return ViewLine( m, pV, m.topLine );
  }
  return 0;
}

unsigned Diff::GetLeftChar() const
{
  return m.leftChar;
}

unsigned Diff::GetCrsRow() const
{
  return m.crsRow;
}

unsigned Diff::GetCrsCol() const
{
  return m.crsCol;
}

unsigned Diff::DiffLine( const View* pV, const unsigned view_line )
{
//return DiffLine( m, pV, view_line );
  Trace trace( __PRETTY_FUNCTION__ );

  return ( pV == m.pvS ) ? DiffLine_S( m, view_line )
                         : DiffLine_L( m, view_line );
}

void Diff::PageDown()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = NumLines(m);
  if( 0==NUM_LINES ) return;

  View* pV = m.vis.CV();

  // new diff top line:
  const unsigned newTopLine = m.topLine + WorkingRows( pV ) - 1;
  // Subtracting 1 above leaves one line in common between the 2 pages.

  if( newTopLine < NUM_LINES )
  {
    m.crsCol = 0;
    m.topLine = newTopLine;

    // Dont let cursor go past the end of the file:
    if( NUM_LINES <= CrsLine(m) )
    {
      // This line places the cursor at the top of the screen, which I prefer:
      m.crsRow = 0;
    }
    Update();
  }
}

void Diff::PageUp()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Dont scroll if we are at the top of the file:
  if( m.topLine )
  {
    //Leave crsRow unchanged.
    m.crsCol = 0;

    View* pV = m.vis.CV();

    // Dont scroll past the top of the file:
    if( m.topLine < WorkingRows( pV ) - 1 )
    {
      m.topLine = 0;
    }
    else {
      m.topLine -= WorkingRows( pV ) - 1;
    }
    Update();
  }
}

//void Diff::GoDown()
//{
//  Trace trace( __PRETTY_FUNCTION__ );
//
//  const unsigned NUM_LINES = NumLines(m);
//  const unsigned OCL       = CrsLine(m); // Old cursor line
//  const unsigned NCL       = OCL+1;     // New cursor line
//
//  if( 0 < NUM_LINES && NCL < NUM_LINES )
//  {
//    const unsigned OCP = CrsChar(m); // Old cursor position
//          unsigned NCP = OCP;
//
//    GoToCrsPos_Write( m, NCL, NCP );
//  }
//}

void Diff::GoDown( const unsigned num )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = NumLines(m);
  const unsigned OCL       = CrsLine(m); // Old cursor line

  if( 0 < NUM_LINES && OCL < NUM_LINES-1 )
  {
    unsigned NCL = OCL+num; // New cursor line

    if( NUM_LINES-1 < NCL ) NCL = NUM_LINES-1;

    GoToCrsPos_Write( m, NCL, CrsChar(m) );
  }
}

//void Diff::GoUp()
//{
//  Trace trace( __PRETTY_FUNCTION__ );
//
//  const unsigned NUM_LINES = NumLines(m);
//  const unsigned OCL       = CrsLine(m); // Old cursor line
//  const unsigned NCL       = OCL-1; // New cursor line
//
//  if( 0 < NUM_LINES && 0 < OCL )
//  {
//    const unsigned OCP = CrsChar(m); // Old cursor position
//          unsigned NCP = OCP;
//
//    GoToCrsPos_Write( m, NCL, NCP );
//  }
//}

void Diff::GoUp( const int num )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = NumLines(m);
  const int      OCL       = CrsLine(m); // Old cursor line

  if( 0 < NUM_LINES && 0 < OCL )
  {
    int NCL = OCL-num; // New cursor line

    if( NCL < 0 ) NCL = 0;

    GoToCrsPos_Write( m, NCL, CrsChar(m) );
  }
}

//void Diff::GoRight()
//{
//  Trace trace( __PRETTY_FUNCTION__ );
//
//  const unsigned LL = LineLen(m);
//  const unsigned CP = CrsChar(m); // Cursor position
//
//  if( 0<NumLines(m) && 0<LL && CP<LL-1 )
//  {
//    const unsigned CL = CrsLine(m); // Cursor line
//
//    GoToCrsPos_Write( m, CL, CP+1 );
//  }
//}

void Diff::GoRight( const unsigned num )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0<NumLines(m) )
  {
    const unsigned LL  = LineLen(m);
    const unsigned OCP = CrsChar(m); // Old cursor position

    if( 0<LL && OCP < LL-1 )
    {
      unsigned NCP = OCP+num; // New cursor position

      if( LL-1 < NCP ) NCP = LL-1;

      GoToCrsPos_Write( m, CrsLine(m), NCP );
    }
  }
}

//void Diff::GoLeft()
//{
//  Trace trace( __PRETTY_FUNCTION__ );
//
//  const unsigned CP = CrsChar(m); // Cursor position
//
//  if( 0<NumLines(m) && 0<CP )
//  {
//    const unsigned CL = CrsLine(m); // Cursor line
//
//    GoToCrsPos_Write( m, CL, CP-1 );
//  }
//}

void Diff::GoLeft( const int num )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned OCP = CrsChar(m); // Old cursor position

  if( 0 < NumLines(m) && 0 < OCP )
  {
    int NCP = OCP-num; // New cursor position

    if( NCP < 0 ) NCP = 0;

    GoToCrsPos_Write( m, CrsLine(m), NCP );
  }
}

void Diff::GoToBegOfLine()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0<NumLines(m) )
  {
    const unsigned CL = CrsLine(m); // Cursor line

    GoToCrsPos_Write( m, CL, 0 );
  }
}

void Diff::GoToEndOfLine()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0<NumLines(m) )
  {
    const unsigned LL = LineLen(m);

    const unsigned OCL = CrsLine(m); // Old cursor line

    GoToCrsPos_Write( m, OCL, 0<LL ? LL-1 : 0 );
  }
}

void Diff::GoToBegOfNextLine()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = NumLines(m);

  if( 0<NUM_LINES )
  {
    const unsigned OCL = CrsLine(m); // Old cursor line

    if( OCL < (NUM_LINES-1) )
    {
      // Before last line, so can go down
      GoToCrsPos_Write( m, OCL+1, 0 );
    }
  }
}

void Diff::GoToEndOfNextLine()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = NumLines(m); // Diff

  if( 0<NUM_LINES )
  {
    const unsigned OCL = CrsLine(m); // Old cursor diff line

    if( OCL < (NUM_LINES-1) )
    {
      // Before last line, so can go down
      View*    pV  = m.vis.CV();
      FileBuf* pfb = pV->GetFB();
      const unsigned VL = ViewLine( m, pV, OCL+1 ); // View line
      const unsigned LL = pfb->LineLen( VL );

      GoToCrsPos_Write( m, OCL+1, LLM1(LL) );
    }
  }
}

void Diff::GoToLine( const unsigned user_line_num )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  // Internal line number is 1 less than user line number:
  const unsigned NCLv = user_line_num - 1; // New cursor view line number

  if( pfb->NumLines() <= NCLv )
  {
    // Cant move to NCLv so just put cursor back where is was
    PrintCursor( pV );
  }
  else {
    const unsigned NCLd = m.diff.DiffLine( pV, NCLv );

    GoToCrsPos_Write( m, NCLd, 0 );
  }
}

void Diff::GoToTopLineInView()
{
  Trace trace( __PRETTY_FUNCTION__ );

  GoToCrsPos_Write( m, m.topLine, CrsChar(m) );
}

void Diff::GoToMidLineInView()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = m.vis.CV();

  const unsigned NUM_LINES = NumLines(m);

  // Default: Last line in file is not in view
  unsigned crsLine = m.topLine + WorkingRows( pV )/2;

  if( NUM_LINES-1 < BotLine( m, pV ) )
  {
    // Last line in file above bottom of view
    crsLine = m.topLine + (NUM_LINES-1 - m.topLine)/2;
  }
  GoToCrsPos_Write( m, crsLine, 0 );
}

void Diff::GoToBotLineInView()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = m.vis.CV();

  const unsigned NUM_LINES = NumLines(m);

  unsigned bottom_line_in_view = m.topLine + WorkingRows( pV )-1;

  bottom_line_in_view = Min( NUM_LINES-1, bottom_line_in_view );

  GoToCrsPos_Write( m, bottom_line_in_view, CrsChar(m) );
}

void Diff::GoToTopOfFile()
{
  Trace trace( __PRETTY_FUNCTION__ );

  GoToCrsPos_Write( m, 0, 0 );
}

void Diff::GoToEndOfFile()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = NumLines(m);

  if( 0<NUM_LINES )
  {
    GoToCrsPos_Write( m, NUM_LINES-1, 0 );
  }
}

void Diff::GoToStartOfRow()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0<NumLines(m) )
  {
    const int OCL = CrsLine(m); // Old cursor line

    GoToCrsPos_Write( m, OCL, m.leftChar );
  }
}

void Diff::GoToEndOfRow()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0<NumLines(m) )
  {
    View*    pV  = m.vis.CV();
    FileBuf* pfb = pV->GetFB();

    const int DL = CrsLine(m);            // Diff line
    const int VL = ViewLine( m, pV, DL ); // View line

    const int LL = pfb->LineLen( VL );
    if( 0 < LL )
    {
      const int NCP = Min( LL-1, m.leftChar + WorkingCols( pV ) - 1 );

      GoToCrsPos_Write( m, DL, NCP );
    }
  }
}

void Diff::GoToOppositeBracket()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = m.vis.CV();

  MoveInBounds_Line(m);

  const unsigned NUM_LINES = pV->GetFB()->NumLines();
  const unsigned CL        = ViewLine( m, pV, CrsLine(m) ); //< View line
  const unsigned CC        = CrsChar(m);
  const unsigned LL        = LineLen(m);

  if( 0==NUM_LINES || 0==LL ) return;

  const char C = pV->GetFB()->Get( CL, CC );

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

void Diff::GoToLeftSquigglyBracket()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = m.vis.CV();

  MoveInBounds_Line(m);

  const char  start_char = '}';
  const char finish_char = '{';
  GoToOppositeBracket_Backward( m, start_char, finish_char );
}

void Diff::GoToRightSquigglyBracket()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = m.vis.CV();

  MoveInBounds_Line(m);

  const char  start_char = '{';
  const char finish_char = '}';
  GoToOppositeBracket_Forward( m, start_char, finish_char );
}

void Diff::GoToNextWord()
{
  Trace trace( __PRETTY_FUNCTION__ );
  CrsPos ncp = { 0, 0 };

  if( GoToNextWord_GetPosition( m, ncp ) )
  {
    GoToCrsPos_Write( m, ncp.crsLine, ncp.crsChar );
  }
}

void Diff::GoToPrevWord()
{
  Trace trace( __PRETTY_FUNCTION__ );
  CrsPos ncp = { 0, 0 };

  if( GoToPrevWord_GetPosition( m, ncp ) )
  {
    GoToCrsPos_Write( m, ncp.crsLine, ncp.crsChar );
  }
}

void Diff::GoToEndOfWord()
{
  Trace trace( __PRETTY_FUNCTION__ );
  CrsPos ncp = { 0, 0 };

  if( GoToEndOfWord_GetPosition( m, ncp ) )
  {
    GoToCrsPos_Write( m, ncp.crsLine, ncp.crsChar );
  }
}

void Diff::Do_n()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0<m.vis.GetRegexLen() ) Do_n_Pattern(m);
  else                        Do_n_Diff(m,true);
}

void Diff::Do_N()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.vis.GetRegexLen() ) Do_N_Pattern(m);
  else                      Do_N_Diff(m);
}

void Diff::Do_f( const char FAST_CHAR )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = NumLines(m);

  if( 0< NUM_LINES )
  {
    View* pV = m.vis.CV();
    FileBuf* pfb = pV->GetFB();

    const unsigned DL  = CrsLine(m);            // Diff line
    const unsigned VL  = ViewLine( m, pV, DL ); // View line
    const unsigned LL  = pfb->LineLen( VL );
    const unsigned OCP = CrsChar(m);            // Old cursor position

    if( OCP < LL-1 )
    {
      unsigned NCP = 0;
      bool found_char = false;
      for( unsigned p=OCP+1; !found_char && p<LL; p++ )
      {
        const char C = pfb->Get( VL, p );

        if( C == FAST_CHAR )
        {
          NCP = p;
          found_char = true;
        }
      }
      if( found_char )
      {
        GoToCrsPos_Write( m, DL, NCP );
      }
    }
  }
}

void Diff::MoveCurrLineToTop()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0<m.crsRow )
  {
    m.topLine += m.crsRow;
    m.crsRow = 0;
    Update();
  }
}

void Diff::MoveCurrLineCenter( const bool write )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = m.vis.CV();

  const unsigned center = SCast<unsigned>( 0.5*WorkingRows(pV) + 0.5 );

  const unsigned OCL = CrsLine(m); // Old cursor line

  if( 0 < OCL && OCL < center && 0 < m.topLine )
  {
    // Cursor line cannot be moved to center, but can be moved closer to center
    // CrsLine(m) does not change:
    m.crsRow += m.topLine;
    m.topLine = 0;

    if( write ) Update();
  }
  else if( center < OCL
        && center != m.crsRow )
  {
    m.topLine += m.crsRow - center;
    m.crsRow = center;

    if( write ) Update();
  }
}

void Diff::MoveCurrLineToBottom()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0 < m.topLine )
  {
    View* pV = m.vis.CV();

    const unsigned WR  = WorkingRows( pV );
    const unsigned OCL = CrsLine(m); // Old cursor line

    if( WR-1 <= OCL )
    {
      m.topLine -= WR - m.crsRow - 1;
      m.crsRow = WR-1;
      Update();
    }
    else {
      // Cursor line cannot be moved to bottom, but can be moved closer to bottom
      // CrsLine(m) does not change:
      m.crsRow += m.topLine;
      m.topLine = 0;
      Update();
    }
  }
}

void Diff::Do_i()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  pV->SetInsertMode( true );
  DisplayBanner(m);

  if( 0 == pfb->NumLines() ) pfb->PushLine();

  const unsigned DL = CrsLine(m); // Diff line number
  const unsigned VL = ViewLine( m, pV, DL ); // View line number
  const unsigned LL = pfb->LineLen( VL ); // Line length

  if( LL < CrsChar(m) ) // Since cursor is now allowed past EOL,
  {                    // it may need to be moved back:
    // For user friendlyness, move cursor to new position immediately:
    GoToCrsPos_Write( m, DL, LL );
  }
  unsigned count = 0;
  for( char c=m.key.In(); c != ESC; c=m.key.In() )
  {
    if( BS == c || DEL == c )
    {
      if( 0<count )
      {
        InsertBackspace(m);
        count--;
      }
    }
    else if( IsEndOfLineDelim( c ) )
    {
      InsertAddReturn( m );
      count++;
    }
    else {
      InsertAddChar( m, c );
      count++;
    }
  }
  pV->SetInsertMode( false );

  // Move cursor back one space:
  if( m.crsCol ) m.crsCol--;

  // Always update when leaving insert mode because banner will need to be removed:
  Update();
}

void Diff::Do_a()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  if( 0<pfb->NumLines() )
  {
    const unsigned DL = CrsLine(m);
    const unsigned VL = ViewLine( m, pV, DL ); // View line number
    const unsigned LL = pfb->LineLen( VL );

    if( 0<LL ) {
      const bool CURSOR_AT_EOL = ( CrsChar(m) == LL-1 );
      if( CURSOR_AT_EOL )
      {
        m.diff.GoToCrsPos_NoWrite( DL, LL );
      }
      const bool CURSOR_AT_RIGHT_COL = ( m.crsCol == WorkingCols( pV )-1 );

      if( CURSOR_AT_RIGHT_COL )
      {
        // Only need to scroll window right, and then enter insert i:
        m.leftChar++; //< This increments CrsChar(m)
      }
      else if( !CURSOR_AT_EOL ) // If cursor was at EOL, already moved cursor forward
      {
        // Only need to move cursor right, and then enter insert i:
        m.crsCol += 1; //< This increments CrsChar(m)
      }
      Update();
    }
  }
  Do_i();
}

void Diff::Do_A()
{
  Trace trace( __PRETTY_FUNCTION__ );

  GoToEndOfLine();

  Do_a();
}

void Diff::Do_o()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  const unsigned NL = pfb->NumLines();
  const unsigned DL = CrsLine(m);

  Array_t<Diff_Info>& cDI_List = (pV == m.pvS) ? m.DI_List_S : m.DI_List_L; // Current

  const bool ON_DELETED = DT_DELETED == cDI_List[ DL ].diff_type;

  // If no lines or on a deleted line, just Do_i()
  if( 0<NL && !ON_DELETED )
  {
    const unsigned VL = ViewLine( m, pV, DL ); // View line

    pfb->InsertLine( VL+1 );
    m.crsCol   = 0;
    m.leftChar = 0;
    if( DL < BotLine( m, pV ) ) m.crsRow++;
    else {
      // If we were on the bottom working line, scroll screen down
      // one line so that the cursor line is not below the screen.
      m.topLine++;
    }
    m.diff.Patch_Diff_Info_Inserted( pV, DL+1, false );

    Update();
  }
  Do_i();
}

// Wrapper around Do_o approach:
void Diff::Do_O()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned DL = CrsLine(m);

  if( 0<DL )
  {
    // Not on top line, so just back up and then Do_o:
    m.diff.GoToCrsPos_NoWrite( DL-1, CrsChar(m) );
    Do_o();
  }
  else {
    // On top line, so cannot move up a line and then Do_o,
    // so use some custom code:
    View*    pV  = m.vis.CV();
    FileBuf* pfb = pV->GetFB();

    pfb->InsertLine( 0 );
    m.diff.Patch_Diff_Info_Inserted( pV, 0, true );

    Update();
    Do_i();
  }
}

void Diff::Do_x()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  // If there is nothing to 'x', just return:
  if( 0<pfb->NumLines() )
  {
    const unsigned DL = CrsLine(m); // Diff line number
    const unsigned VL = ViewLine( m, pV, DL ); // View line number
    const unsigned LL = pfb->LineLen( VL );

    // If nothing on line, just return:
    if( 0<LL )
    {
      // If past end of line, move to end of line:
      if( LL-1 < CrsChar(m) )
      {
        GoToCrsPos_Write( m, DL, LL-1 );
      }
      const uint8_t C = pfb->RemoveChar( VL, CrsChar(m) );

      // Put char x'ed into register:
      Line* nlp = m.vis.BorrowLine(__FILE__,__LINE__);
      nlp->push( C );
      m.reg.clear();
      m.reg.push( nlp );
      m.vis.SetPasteMode( PM_ST_FN );

      const unsigned NLL = pfb->LineLen( VL ); // New line length

      // Reposition the cursor:
      if( NLL <= m.leftChar+m.crsCol )
      {
        // The char x'ed is the last char on the line, so move the cursor
        //   back one space.  Above, a char was removed from the line,
        //   but crsCol has not changed, so the last char is now NLL.
        // If cursor is not at beginning of line, move it back one more space.
        if( m.crsCol ) m.crsCol--;
      }
      m.diff.Patch_Diff_Info_Changed( pV, DL );
      Update();
    }
  }
}

void Diff::Do_s()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  const unsigned DL  = CrsLine(m);          // Diff line
  const unsigned VL  = ViewLine( m, pV, DL ); // View line
  const unsigned LL  = pfb->LineLen( VL );
  const unsigned EOL = LL ? LL-1 : 0;
  const unsigned CP  = CrsChar(m);

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

void Diff::Do_D()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  const unsigned NUM_LINES = pfb->NumLines();
  const unsigned DL = CrsLine(m);  // Old cursor line
  const unsigned VL = ViewLine( m, pV, DL ); // View line
  const unsigned CP = CrsChar(m);  // Old cursor position
  const unsigned LL = pfb->LineLen( VL );  // Old line length

  // If there is nothing to 'D', just return:
  if( 0<NUM_LINES && 0<LL && CP<LL )
  {
    Line* lpd = m.vis.BorrowLine( __FILE__,__LINE__ );

    for( unsigned k=CP; k<LL; k++ )
    {
      uint8_t c = pfb->RemoveChar( VL, CP );
      lpd->push( c );
    }
    m.reg.clear();
    m.reg.push( lpd );
    m.vis.SetPasteMode( PM_ST_FN );

    // If cursor is not at beginning of line, move it back one space.
    if( 0<m.crsCol ) m.crsCol--;

    m.diff.Patch_Diff_Info_Changed( pV, DL );
    if( !m.diff.ReDiff() ) m.diff.Update();
  }
}

void Diff::Do_J()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  const int DL  = CrsLine(m); // Diff line
  const int VL  = ViewLine( m, pV, DL ); // View line

  if( VL < pfb->NumLines()-1 )
  {
    Array_t<Diff_Info> cDI_List = (pV == m.pvS) ? m.DI_List_S : m.DI_List_L; // Current diff info list
    const Diff_Type cDT = cDI_List[DL].diff_type; // Current diff type

    if( 0 < VL
     && ( cDT == DT_SAME
       || cDT == DT_CHANGED
       || cDT == DT_INSERTED ) )
    {
      const int DLp = m.diff.DiffLine( pV, VL+1 ); // Diff line for VL+1

      Line* lp = pfb->RemoveLineP( VL+1 );
      m.diff.Patch_Diff_Info_Deleted( pV, DLp );

      pfb->AppendLineToLine( VL, lp );
      m.diff.Patch_Diff_Info_Changed( pV, DL );

      if( !m.diff.ReDiff() ) m.diff.Update();
    }
  }
}

void Diff::Do_dd()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  const unsigned NVL = pfb->NumLines();   // Number of view lines

  // If there is nothing to 'dd', just return:
  if( 1 < NVL )
  {
    const unsigned DL = CrsLine(m); // Old Diff line

    // Cant delete a deleted or unknown line
    const Diff_Type DT = DiffType( m, pV, DL );
    if( DT != DT_UNKN0WN && DT != DT_DELETED )
    {
      const unsigned VL = ViewLine( m, pV, DL );  // View line

      // Remove line from FileBuf and save in paste register:
      Line* lp = pfb->RemoveLineP( VL );
      if( lp ) {
        // m.reg will own lp
        m.reg.clear();
        m.reg.push( lp );

        m.vis.SetPasteMode( PM_LINE );
      }
      m.diff.Patch_Diff_Info_Deleted( pV, DL );

      // Figure out where to put cursor after deletion:
      const bool DELETED_LAST_LINE = VL == NVL-1;

      unsigned ncld = DL;
      // Deleting last line of file, so move to line above:
      if( DELETED_LAST_LINE ) ncld--;
      else {
        // If cursor is now sitting on a deleted line, move to line below:
        const Diff_Type DTN = DiffType( m, pV, DL );
        if( DTN == DT_DELETED ) ncld++;
      }
      m.diff.GoToCrsPos_NoWrite( ncld, CrsChar(m) );

      if( !m.diff.ReDiff() ) m.diff.Update();
    }
  }
}

// If nothing was deleted, return 0.
// If last char on line was deleted, return 2,
// Else return 1.
int Diff::Do_dw()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  // If there is nothing to 'yw', just return:
  if( 0 < pfb->NumLines() )
  {
    const unsigned  DL = CrsLine(m); // Diff line
    const Diff_Type DT = DiffType( m, pV, DL );

    if( DT == DT_SAME
     || DT == DT_CHANGED
     || DT == DT_INSERTED )
    {
      const unsigned st_line_v = ViewLine( m, pV, DL ); // View line
      const unsigned st_char   = CrsChar( m );

      const unsigned LL = pfb->LineLen( st_line_v );

      // If past end of line, nothing to do
      if( st_char < LL )
      {
        // Determine fn_line_d, fn_char:
        unsigned fn_line_d = 0;
        unsigned fn_char = 0;

        if( Do_dw_get_fn( m, DL, st_char, fn_line_d, fn_char ) )
        {
          Do_x_range( m, DL, st_char, fn_line_d, fn_char );

          bool deleted_last_char = fn_char == LL-1;

          return deleted_last_char ? 2 : 1;
        }
      }
    }
  }
  return 0;
}

void Diff::Do_cw()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned result = Do_dw();

  if     ( result==1 ) Do_i();
  else if( result==2 ) Do_a();
}

void Diff::Do_yy()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  // If there is nothing to 'yy', just return:
  if( 0<pfb->NumLines() )
  {
    const unsigned DL = CrsLine(m);  // Diff line

    // Cant yank a deleted or unknown line
    const Diff_Type DT = DiffType( m, pV, DL );
    if( DT != DT_UNKN0WN && DT != DT_DELETED )
    {
      const unsigned VL = ViewLine( m, pV, DL ); // View Cursor line

      const Line* lp = pfb->GetLineP( VL );

      m.reg.clear();
      m.reg.push( m.vis.BorrowLine( __FILE__,__LINE__, *lp ) );

      m.vis.SetPasteMode( PM_LINE );
    }
  }
}

void Diff::Do_yw()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  // If there is nothing to 'yw', just return:
  if( !pfb->NumLines() ) return;

  const unsigned  DL = CrsLine(m); // Diff line
  const Diff_Type DT = DiffType( m, pV, DL );

  if( DT == DT_SAME
   || DT == DT_CHANGED
   || DT == DT_INSERTED )
  {
    const unsigned st_line_v = ViewLine( m, pV, DL ); // View line
    const unsigned st_char   = CrsChar( m );

    // Determine fn_line_d, fn_char:
    unsigned fn_line_d = 0;
    unsigned fn_char   = 0;

    if( Do_dw_get_fn( m, DL, st_char, fn_line_d, fn_char ) )
    {
      m.reg.clear();
      m.reg.push( m.vis.BorrowLine( __FILE__,__LINE__ ) );

      // DL and fn_line_d should be the same
      for( unsigned k=st_char; k<=fn_char; k++ )
      {
        m.reg[0]->push( pfb->Get( st_line_v, k ) );
      }
      m.vis.SetPasteMode( PM_ST_FN );
    }
  }
}

void Diff::Do_p()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const Paste_Mode PM = m.vis.GetPasteMode();

  if     ( PM_ST_FN == PM ) return Do_p_or_P_st_fn( m, PP_After );
  else if( PM_BLOCK == PM ) return Do_p_block(m);
  else /*( PM_LINE  == PM*/ return Do_p_line(m);
}

void Diff::Do_P()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const Paste_Mode PM = m.vis.GetPasteMode();

  if     ( PM_ST_FN == PM ) return Do_p_or_P_st_fn( m, PP_Before );
  else if( PM_BLOCK == PM ) return Do_P_block(m);
  else /*( PM_LINE  == PM*/ return Do_P_line(m);
}

bool Diff::Do_v()
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.inVisualBlock = false;

  return Do_visualMode(m);
}

bool Diff::Do_V()
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.inVisualBlock = true;

  return Do_visualMode(m);
}

void Diff::Do_r()
{
}

void Diff::Do_R()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  pV->SetReplaceMode( true );
  DisplayBanner(m);

  if( 0 == pfb->NumLines() ) pfb->PushLine();

  unsigned count = 0;
  for( char C=m.key.In(); C != ESC; C=m.key.In() )
  {
    if( BS == C || DEL == C )
    {
      if( 0<count )
      {
        InsertBackspace(m);
        count--;
      }
    }
    else if( IsEndOfLineDelim( C ) )
    {
      ReplaceAddReturn(m);
      count++;
    }
    else {
      ReplaceAddChar( m, C );
      count++;
    }
  }
  Remove_Banner(m);
  pV->SetReplaceMode( false );

  // Move cursor back one space:
  if( 0<m.crsCol ) m.crsCol--;  // Move cursor back one space.

  Update();
}

void Diff::Do_Tilda()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  if( 0==pfb->NumLines() ) return;

  const unsigned DL = CrsLine(m);          // Diff line
  const unsigned VL = ViewLine( m, pV, DL ); // View line
  const unsigned CP = CrsChar(m);          // Cursor position
  const unsigned LL = pfb->LineLen( VL );

  if( 0<LL && CP<LL )
  {
    char C = pfb->Get( VL, CP );
    bool changed = false;
    if     ( isupper( C ) ) { C = tolower( C ); changed = true; }
    else if( islower( C ) ) { C = toupper( C ); changed = true; }

    const bool CONT_LAST_UPDATE = true;
    if( m.crsCol < Min( LL-1, WorkingCols( pV )-1 ) )
    {
      if( changed ) pfb->Set( VL, CP, C, CONT_LAST_UPDATE );
      // Need to move cursor right:
      m.crsCol++;
    }
    else if( RightChar( m, pV ) < LL-1 )
    {
      // Need to scroll window right:
      if( changed ) pfb->Set( VL, CP, C, CONT_LAST_UPDATE );
      m.leftChar++;
    }
    else // RightChar() == LL-1
    {
      // At end of line so cant move or scroll right:
      if( changed ) pfb->Set( VL, CP, C, CONT_LAST_UPDATE );
    }
    if( changed ) m.diff.Patch_Diff_Info_Changed( pV, DL );
    Update();
  }
}

void Diff::Do_u()
{
  View* pV = m.vis.CV();

  pV->GetFB()->Undo( *pV );
}

void Diff::Do_U()
{
  View* pV = m.vis.CV();

  pV->GetFB()->UndoAll( *pV );
}

String Diff::Do_Star_GetNewPattern()
{
  Trace trace( __PRETTY_FUNCTION__ );
  String pattern;
  View* pV  = m.vis.CV();
  FileBuf* pfb = pV->GetFB();

  if( pfb->NumLines() == 0 ) return pattern;

  const unsigned CL = CrsLine(m);
  // Convert CL, which is diff line, to view line:
  const unsigned CLv = ViewLine( m, pV, CrsLine(m) );
  const unsigned LL = pfb->LineLen( CLv );

  if( 0<LL )
  {
    MoveInBounds_Line(m);
    const unsigned CC = CrsChar(m);

    const int c = pfb->Get( CLv,  CC );

    if( isalnum( c ) || c=='_' )
    {
      pattern.push( c );

      // Search forward:
      for( unsigned k=CC+1; k<LL; k++ )
      {
        const int c = pfb->Get( CLv, k );
        if( isalnum( c ) || c=='_' ) pattern.push( c );
        else                         break;
      }
      // Search backward:
      for( int k=CC-1; 0<=k; k-- )
      {
        const int c = pfb->Get( CLv, k );
        if( isalnum( c ) || c=='_' ) pattern.insert( 0, c );
        else                         break;
      }
    }
    else {
      if( isgraph( c ) ) pattern.push( c );
    }
    if( 0<pattern.len() )
    {
      pattern.insert( 0, "\\b" );
      pattern.append(    "\\b" );
    }
  }
  return pattern;
}

//void Diff::GoToFile()
//{
//  Trace trace( __PRETTY_FUNCTION__ );
//
//  // 1. Get fname underneath the cursor:
//  String fname;
//  bool ok = GetFileName_UnderCursor( m, fname );
//
//  if( ok ) m.vis.GoToBuffer_Fname( fname );
//}

void Diff::GoToFile()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = m.vis.CV();

  Array_t<Diff_Info> cDI_List = (pV == m.pvS) ? m.DI_List_S : m.DI_List_L;
  Array_t<Diff_Info> oDI_List = (pV == m.pvS) ? m.DI_List_L : m.DI_List_S;

  const Diff_Type cDT = cDI_List[ CrsLine(m) ].diff_type; // Current diff type
  const Diff_Type oDT = oDI_List[ CrsLine(m) ].diff_type; // Other   diff type

  String fname;
  if( GetFileName_UnderCursor( m, fname ) )
  {
    bool did_diff = false;
    // Special case, look at two file in diff mode:
    View* cV = (pV == m.pvS) ? m.pvS : m.pvL; // Current view
    View* oV = (pV == m.pvS) ? m.pvL : m.pvS; // Other   view

    String cPath = fname; // Current side file to diff full fname
    String oPath = fname; // Other   side file to diff full fname
    if( FindFullFileNameRel2( cV->GetFB()->GetDirName(), cPath )
     && FindFullFileNameRel2( oV->GetFB()->GetDirName(), oPath ) )
    {
      unsigned c_file_idx = 0; // Current side index of file to diff
      unsigned o_file_idx = 0; // Other   side index of file to diff
      if( GetBufferIndex( m, cPath.c_str(), &c_file_idx )
       && GetBufferIndex( m, oPath.c_str(), &o_file_idx ) )
      {
        FileBuf* c_file_buf = m.vis.GetFileBuf( c_file_idx );
        FileBuf* o_file_buf = m.vis.GetFileBuf( o_file_idx );
        // Files with same name and different contents
        // or directories with same name but different paths
        if( (cDT == DT_DIFF_FILES && oDT == DT_DIFF_FILES)
         || (cV->GetFB()->IsDir() && oV->GetFB()->IsDir()
          && 0==strcmp(c_file_buf->GetFileName(),o_file_buf->GetFileName())
          && 0!=strcmp(c_file_buf->GetDirName(),o_file_buf->GetDirName()) ) )
        {
          // Save current view context for when we come back
          const unsigned cV_vl_cl = ViewLine( m, cV, CrsLine(m) );
          const unsigned cV_vl_tl = ViewLine( m, cV, m.topLine );
          cV->SetTopLine( cV_vl_tl );
          cV->SetCrsRow( cV_vl_cl - cV_vl_tl );
          cV->SetLeftChar( m.leftChar );
          cV->SetCrsCol  ( m.crsCol );

          // Save other view context for when we come back
          const unsigned oV_vl_cl = ViewLine( m, oV, CrsLine(m) );
          const unsigned oV_vl_tl = ViewLine( m, oV, m.topLine );
          oV->SetTopLine( oV_vl_tl );
          oV->SetCrsRow( oV_vl_cl - oV_vl_tl );
          oV->SetLeftChar( m.leftChar );
          oV->SetCrsCol  ( m.crsCol );

          did_diff = m.vis.Diff_By_File_Indexes( cV, c_file_idx, oV, o_file_idx );
        }
      }
    }
    if( !did_diff ) {
      // Normal case, dropping out of diff mode to look at file:
      m.vis.GoToBuffer_Fname( fname );
    }
  }
}

void Diff::PrintCursor( View* pV )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Console::Move_2_Row_Col( Row_Win_2_GL( m, pV, m.crsRow )
                         , Col_Win_2_GL( m, pV, m.crsCol ) );
  Console::Flush();
}

bool Diff::Update_Status_Lines()
{
  Trace trace( __PRETTY_FUNCTION__ );

  bool updated_a_sts_line = false;

  if( m.sts_line_needs_update )
  {
    // Update status line:
    PrintStsLine( m, m.pvS );
    PrintStsLine( m, m.pvL );
    Console::Update();

    m.sts_line_needs_update = false;
    updated_a_sts_line = true;
  }
  return updated_a_sts_line;
}

bool ReDiff_GetDiffSt_Search_4_Same( Diff::Data& m
                                   , unsigned& DL_st )
{
  const bool in_short = m.vis.CV() == m.pvS;
  Array_t<Diff_Info>& cDI_List = in_short ? m.DI_List_S : m.DI_List_L; // Current

  // Search up for SAME
  bool found = false;
  int L = DL_st;
  for( ; !found && 0<=L; L-- )
  {
    Diff_Info& di = cDI_List[ L ];
    if( DT_SAME == di.diff_type )
    {
      found = true;
      DL_st = L+1; // Diff area starts on first diff after first same
    }
  }
  if( !found && L < 0 )
  {
    found = true;
    DL_st = 0; // Diff area starts at beginning of file
  }
  return found;
}

bool ReDiff_GetSt_Search_4_Diff_Then_Same( Diff::Data& m
                                         , unsigned& DL_st )
{
  const bool in_short = m.vis.CV() == m.pvS;
  Array_t<Diff_Info>& cDI_List = in_short ? m.DI_List_S : m.DI_List_L; // Current

  // Search up for CHANGED, INSERTED or DELETED and then for SAME
  bool found = false;
  int L = DL_st;
  for( ; !found && 0<=L; L-- )
  {
    Diff_Info& di = cDI_List[ L ];
    if( DT_CHANGED  == di.diff_type
     || DT_INSERTED == di.diff_type
     || DT_DELETED  == di.diff_type )
    {
      found = true;
    }
  }
  if( found )
  {
    found = false;
    for( ; !found && 0<=L; L-- )
    {
      Diff_Info& di = cDI_List[ L ];
      if( DT_SAME == di.diff_type )
      {
        found = true;
        DL_st = L+1; // Diff area starts on first diff after first same
      }
    }
  }
  if( !found && L < 0 )
  {
    found = true;
    DL_st = 0; // Diff area starts at beginning of file
  }
  return found;
}

bool ReDiff_GetDiffFn_Search_4_Same( Diff::Data& m
                                   , unsigned& DL_fn )
{
  const bool in_short = m.vis.CV() == m.pvS;
  Array_t<Diff_Info>& cDI_List = in_short ? m.DI_List_S : m.DI_List_L; // Current

  // Search down for SAME
  bool found = false;
  unsigned L = DL_fn;
  for( ; !found && L<cDI_List.len(); L++ )
  {
    Diff_Info& di = cDI_List[ L ];
    if( DT_SAME == di.diff_type )
    {
      found = true;
      DL_fn = L;
    }
  }
  if( !found && cDI_List.len() < L )
  {
    found = true;
    DL_fn = cDI_List.len(); // Diff area ends at end of file
  }
  return found;
}

bool ReDiff_GetFn_Search_4_Diff_Then_Same( Diff::Data& m
                                         , unsigned& DL_fn )
{
  const bool in_short = m.vis.CV() == m.pvS;
  Array_t<Diff_Info>& cDI_List = in_short ? m.DI_List_S : m.DI_List_L; // Current

  // Search down for CHANGED, INSERTED or DELETED and then for SAME
  bool found = false;
  unsigned L = DL_fn;
  for( ; !found && L<cDI_List.len(); L++ )
  {
    Diff_Info& di = cDI_List[ L ];
    if( DT_CHANGED  == di.diff_type
     || DT_INSERTED == di.diff_type
     || DT_DELETED  == di.diff_type )
    {
      found = true;
    }
  }
  if( found )
  {
    found = false;
    for( ; !found && L<cDI_List.len(); L++ )
    {
      Diff_Info& di = cDI_List[ L ];
      if( DT_SAME == di.diff_type )
      {
        found = true;
        DL_fn = L;
      }
    }
  }
  if( !found && cDI_List.len() < L )
  {
    found = true;
    DL_fn = cDI_List.len(); // Diff area ends at end of file
  }
  return found;
}

bool ReDiff_FindDiffSt( Diff::Data& m
                      , unsigned& DL_st )
{
  bool found_diff_st = false;

  const bool in_short = m.vis.CV() == m.pvS;
  Array_t<Diff_Info>& cDI_List = in_short ? m.DI_List_S : m.DI_List_L; // Current
  Diff_Info& cDI_st = cDI_List[ DL_st ];

  if( DT_SAME == cDI_st.diff_type )
  {
    found_diff_st = ReDiff_GetSt_Search_4_Diff_Then_Same( m, DL_st );
  }
  else if( DT_CHANGED  == cDI_st.diff_type
        || DT_INSERTED == cDI_st.diff_type
        || DT_DELETED  == cDI_st.diff_type )
  {
    found_diff_st = ReDiff_GetDiffSt_Search_4_Same( m, DL_st );
  }
  return found_diff_st;
}

bool ReDiff_FindDiffFn( Diff::Data& m
                      , unsigned& DL_fn )
{
  bool found_diff_fn = false;

  const bool in_short = m.vis.CV() == m.pvS;
  Array_t<Diff_Info>& cDI_List = in_short ? m.DI_List_S : m.DI_List_L; // Current
  Diff_Info& cDI_fn = cDI_List[ DL_fn ];

  if( DT_SAME == cDI_fn.diff_type )
  {
    found_diff_fn = ReDiff_GetFn_Search_4_Diff_Then_Same( m, DL_fn );
  }
  else if( DT_CHANGED  == cDI_fn.diff_type
        || DT_INSERTED == cDI_fn.diff_type
        || DT_DELETED  == cDI_fn.diff_type )
  {
    found_diff_fn = ReDiff_GetDiffFn_Search_4_Same( m, DL_fn );
  }
  else {
  }
  return found_diff_fn;
}

DiffArea DL_st_fn_2_DiffArea( Diff::Data& m
                            , const unsigned DL_st
                            , const unsigned DL_fn )
{
  DiffArea da;

  da.ln_s = ( DT_DELETED == m.DI_List_S[ DL_st ].diff_type )
          ? m.DI_List_S[DL_st].line_num+1
          : m.DI_List_S[DL_st].line_num;

  da.ln_l = ( DT_DELETED == m.DI_List_L[ DL_st ].diff_type )
          ? m.DI_List_L[DL_st].line_num+1
          : m.DI_List_L[DL_st].line_num;

  if( DL_fn < m.DI_List_L.len() )
  {
    da.nlines_s = m.DI_List_S[DL_fn].line_num - da.ln_s;
    da.nlines_l = m.DI_List_L[DL_fn].line_num - da.ln_l;
  }
  else {
    // Need the extra -1 here to avoid a crash.
    // Not sure why it is needed.
  //da.nlines_s = m.pfS->NumLines() - da.ln_s - 1;
  //da.nlines_l = m.pfL->NumLines() - da.ln_l - 1;
    da.nlines_s = m.pfS->NumLines() - da.ln_s;
    da.nlines_l = m.pfL->NumLines() - da.ln_l;
  }
  return da;
}

bool ReDiff_GetDiffArea( Diff::Data& m
                       , const unsigned DL_st
                       , const unsigned DL_fn
                       , DiffArea& da )
{
  // Diff area if from l_DL_st up to but not including l_DL_fn
  unsigned l_DL_st = DL_st; // local diff line start
  unsigned l_DL_fn = DL_fn; // local diff line finish

  bool found_st = ReDiff_FindDiffSt( m, l_DL_st );
  bool found_fn = false;

  if( found_st )
  {
    found_fn = l_DL_fn < m.DI_List_L.len()
             ? ReDiff_FindDiffFn( m, l_DL_fn )
             : true;
  }
  bool found_diff_area = found_st && found_fn;

  if( found_diff_area )
  {
    da = DL_st_fn_2_DiffArea( m, l_DL_st, l_DL_fn );
  }
  else {
    const unsigned DL = CrsLine(m); // Diff line number

    unsigned l_DL_st_2 = DL; // local diff line start
    unsigned l_DL_fn_2 = DL; // local diff line finish

    found_diff_area = ReDiff_FindDiffSt( m, l_DL_st_2 )
                   && ReDiff_FindDiffFn( m, l_DL_fn_2 );

    if( found_diff_area )
    {
      da = DL_st_fn_2_DiffArea( m, l_DL_st_2, l_DL_fn_2 );
    }
  }
  return found_diff_area;
}

//unsigned Remove_From_DI_Lists( Diff::Data& m, DiffArea da )
//{
//  unsigned DI_lists_insert_idx = 0;
//
//  unsigned DI_list_s_remove_st = DiffLine_S( m, da.ln_s );
//  unsigned DI_list_l_remove_st = DiffLine_L( m, da.ln_l );
//  unsigned DI_list_remove_st = Min( DI_list_s_remove_st
//                                  , DI_list_l_remove_st );
//  DI_lists_insert_idx = DI_list_remove_st;
//
//  unsigned DI_list_s_remove_fn = DiffLine_S( m, da.fnl_s() );
//  unsigned DI_list_l_remove_fn = DiffLine_L( m, da.fnl_l() );
//  unsigned DI_list_remove_fn = Max( DI_list_s_remove_fn
//                                  , DI_list_l_remove_fn );
////Log.Log("(DI_list_remove_st,DI_list_remove_fn) = ("
////       + (DI_list_remove_st+1)+","+(DI_list_remove_fn+1) +")");
//
//  for( unsigned k=DI_list_remove_st; k<DI_list_remove_fn; k++ )
//  {
//    m.DI_List_S.remove( DI_lists_insert_idx );
//    m.DI_List_L.remove( DI_lists_insert_idx );
//  }
//  return DI_lists_insert_idx;
//}

unsigned Remove_From_DI_Lists( Diff::Data& m, DiffArea da )
{
  unsigned DI_lists_insert_idx = 0;

  unsigned DI_list_s_remove_st = DiffLine_S( m, da.ln_s );
  unsigned DI_list_l_remove_st = DiffLine_L( m, da.ln_l );
  unsigned DI_list_remove_st = Min( DI_list_s_remove_st
                                  , DI_list_l_remove_st );
  DI_lists_insert_idx = DI_list_remove_st;

  unsigned DI_list_s_remove_fn = m.pvS->GetFB()->NumLines() <= da.fnl_s()
                               ? m.DI_List_S.len()
                               : DiffLine_S( m, da.fnl_s() );

  unsigned DI_list_l_remove_fn = m.pvL->GetFB()->NumLines() <= da.fnl_l()
                               ? m.DI_List_L.len()
                               : DiffLine_L( m, da.fnl_l() );

  unsigned DI_list_remove_fn = Max( DI_list_s_remove_fn
                                  , DI_list_l_remove_fn );
//Log.Log("(DI_list_remove_st,DI_list_remove_fn) = ("
//       + (DI_list_remove_st+1)+","+(DI_list_remove_fn+1) +")");

  for( unsigned k=DI_list_remove_st; k<DI_list_remove_fn; k++ )
  {
    m.DI_List_S.remove( DI_lists_insert_idx );
    m.DI_List_L.remove( DI_lists_insert_idx );
  }
  return DI_lists_insert_idx;
}

// Returns success or failure
bool Diff::ReDiff()
{
  bool ok = false;
  const unsigned DL = CrsLine(m); // Diff line number
  const unsigned NUM_DLs = m.DI_List_L.len();
  const unsigned SIDE_BAND = 50;
  unsigned DL_st = DL < SIDE_BAND ? 0 : DL - SIDE_BAND;
  unsigned DL_fn = NUM_DLs;
  if( SIDE_BAND < NUM_DLs )
  {
    DL_fn = NUM_DLs-SIDE_BAND < DL ? NUM_DLs : DL + SIDE_BAND;
  }
  DiffArea da;
  const bool found_diff_area = ReDiff_GetDiffArea( m, DL_st, DL_fn, da );

  if( !found_diff_area )
  {
    m.vis.CmdLineMessage("rediff: DiffArea not found");
    PrintCursor( m.vis.CV() );
  }
  else {
    ok = true;
  //Log.Log("ReDiff DiffArea:"); da.Print();

    m.DI_L_ins_idx = Remove_From_DI_Lists( m, da );

    RunDiff( m, da );
  //Update();
    m.vis.UpdateViews( false );
  }
  return ok;
}

void Diff::Set_Cmd_Line_Msg( const String& msg )
{
  m.cmd_line_msg = msg;
}

void Diff::DisplayMapping()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = m.vis.CV();

  const char* mapping = "--MAPPING--";
  const int   mapping_len = strlen( mapping );

  // Command line row in window:
  const unsigned WIN_ROW = WorkingRows( pV ) + 2;
  const unsigned WIN_COL = WorkingCols( pV ) - mapping_len;

  const unsigned G_ROW = Row_Win_2_GL( m, pV, WIN_ROW );
  const unsigned G_COL = Col_Win_2_GL( m, pV, WIN_COL );

  Console::SetS( G_ROW, G_COL, mapping, S_BANNER );

  Console::Update();
  PrintCursor( pV ); // Put cursor back in position.
}

