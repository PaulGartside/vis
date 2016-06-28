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

#include "String.hh"
#include "MemLog.hh"
#include "FileBuf.hh"
#include "Cover_Array.hh"

extern MemLog<MEM_LOG_BUF_SIZE> Log;

static const unsigned seq_len = 7;
static uint8_t seq_inc[] = {  79, 101, 127, 139, 163, 181, 199 };
static uint8_t seq_mod[] = { 131, 151, 173, 191, 211, 229, 251 };
static uint8_t seq_val[seq_len];

void    Init_seq_val( const uint8_t seed, const String& key );
uint8_t Cover_Byte  ();

void Cover_Array( FileBuf& in
                , Line&    out
                , const uint8_t seed
                , const String& key )
{
  Init_seq_val( seed, key );

  out.clear();
  const unsigned NUM_LINES = in.NumLines();

  for( unsigned l=0; l<NUM_LINES; l++ )
  {
    const unsigned LL = in.LineLen( l );

    for( unsigned p=0; p<LL; p++ )
    {
      const uint8_t B = in.Get( l, p );
      const uint8_t C = Cover_Byte() ^ B;
      out.push(__FILE__,__LINE__, C );
    }
    if( l<NUM_LINES-1 || in.Has_LF_at_EOF() )
    {
      const uint8_t C = Cover_Byte() ^ '\n';
      out.push(__FILE__,__LINE__, C );
    }
  }
}

void Init_seq_val( const uint8_t seed
                 , const String& key )
{
  // Initialize seq_val:
  const unsigned key_len = key.len();
  const char*    key_ptr = key.c_str();

  for( unsigned k=0; k<seq_len; k++ )
  {
    seq_val[k] = (seed + seq_inc[k]) % seq_mod[k];
  }
  for( unsigned k=0; k<seq_len*key_len; k+=1 )
  {
    const unsigned k_m = k % seq_len;

    seq_val[ k_m ] ^= key_ptr[ k % key_len ];
    seq_val[ k_m ] %= seq_mod[ k_m ];
  }
}

uint8_t Cover_Byte()
{
  uint8_t cb = 0xAA;
  for( unsigned k=0; k<seq_len; k+=1 )
  {
    seq_val[k] = ( seq_val[k] + seq_inc[k] ) % seq_mod[k];

    cb ^= seq_val[k];
  }
  return cb;
}

