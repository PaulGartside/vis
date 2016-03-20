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

#include <string.h>    // memcpy, memset
#include "MemLog.hh"
#include "Utilities.hh"
#include "FileBuf.hh"
#include "Highlight_Base.hh"

extern MemLog<MEM_LOG_BUF_SIZE> Log;

Highlight_Base::Highlight_Base( FileBuf& rfb )
  : fb( rfb )
{
}

void Highlight_Base::Hi_FindKey( HiKeyVal* HiPairs )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned NUM_LINES = fb.NumLines();

  for( unsigned l=0; l<NUM_LINES; l++ )
  {
    const Line&    lr = fb.GetLine( l );
    const Line&    sr = fb.GetStyle( l );
    const unsigned LL = lr.len();

    for( unsigned p=0; p<LL; p++ )
    {
      bool key_search = !sr.get(p) && line_start_or_non_ident( lr, LL, p );

      for( unsigned h=0; key_search && HiPairs[h].key; h++ )
      {
        bool matches = true;
        const char*    key     = HiPairs[h].key;
        const uint8_t  HI_TYPE = HiPairs[h].val;
        const unsigned KEY_LEN = strlen( key );

        for( unsigned k=0; matches && (p+k)<LL && k<KEY_LEN; k++ )
        {
          if( sr.get(p+k) || key[k] != lr.get(p+k) ) matches = false;
          else {
            if( k+1 == KEY_LEN ) // Found pattern
            {
              matches = line_end_or_non_ident( lr, LL, p+k );
              if( matches ) {
                for( unsigned m=p; m<p+KEY_LEN; m++ ) fb.SetSyntaxStyle( l, m, HI_TYPE );
                // Increment p one less than KEY_LEN, because p
                // will be incremented again by the for loop
                p += KEY_LEN-1;
                // Set key_search to false here to break out of h for loop
                key_search = false;
              }
            }
          }
        }
      }
    }
  }
}

void Highlight_Base::Hi_FindKey_In_Range( HiKeyVal* HiPairs
                                        , const CrsPos   st
                                        , const unsigned fn )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned NUM_LINES = fb.NumLines();

  for( unsigned l=st.crsLine; l<=fn && l<NUM_LINES; l++ )
  {
    const Line&    lr = fb.GetLine( l );
    const Line&    sr = fb.GetStyle( l );
    const unsigned LL = lr.len();

    const unsigned st_pos = st.crsLine==l ? st.crsChar : 0;
    const unsigned fn_pos = 0<LL ? LL-1 : 0;

    for( unsigned p=st_pos; p<=fn_pos && p<LL; p++ )
    {
      bool key_st = !sr.get(p) && line_start_or_non_ident( lr, LL, p );

      for( unsigned h=0; key_st && HiPairs[h].key; h++ )
      {
        bool matches = true;
        const char*    key     = HiPairs[h].key;
        const uint8_t  HI_TYPE = HiPairs[h].val;
        const unsigned KEY_LEN = strlen( key );

        for( unsigned k=0; matches && (p+k)<LL && k<KEY_LEN; k++ )
        {
          if( sr.get(p+k) || key[k] != lr.get(p+k) ) matches = false;
          else {
            if( k+1 == KEY_LEN ) // Found pattern
            {
              matches = line_end_or_non_ident( lr, LL, p+k );
              if( matches ) {
                for( unsigned m=p; m<p+KEY_LEN; m++ ) fb.SetSyntaxStyle( l, m, HI_TYPE );
                // Increment p one less than KEY_LEN, because p
                // will be incremented again by the for loop
                p += KEY_LEN-1;
                // Set key_st to false here to break out of h for loop
                key_st = false;
              }
            }
          }
        }
      }
    }
  }
}

