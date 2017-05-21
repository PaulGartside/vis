
#include <string>

#include "Utilities.hh"
#include "Line.hh"

using std::string;

const unsigned m_primes[] = {  43, 101, 149, 193, 241, 293, 353, 409, 461
                            , 521, 587, 641, 691, 757, 823, 881, 947 };

const unsigned m_num_primes = sizeof( m_primes )/sizeof( unsigned );

struct Line::Data
{
  Data();
  Data( const unsigned cap );
  Data( const unsigned len, const uint8_t fill );

  string s;

  bool     chksum_valid;
  unsigned chksum;
};

Line::Data::Data()
  : s()
  , chksum_valid( false )
  , chksum( 0 )
{
}

Line::Data::Data( const unsigned cap )
  : s()
  , chksum_valid( false )
  , chksum( 0 )
{
  s.reserve( cap );
}

Line::Data::Data( const unsigned len
                , const uint8_t  fill )
  : chksum_valid( false )
  , chksum( 0 )
{
  s.reserve( len );

  for( unsigned k=0; k<len; k++ ) s.append( 1, fill );
}

int skip_white_beg( Line::Data& m, int start )
{
  const unsigned LEN = m.s.length();

  if( 0<LEN )
  {
    for( uint8_t C = m.s[ start ]
       ; start<LEN && (' '==C || '\t'==C || '\r'==C); )
    {
      start++;
      if( start<LEN ) C = m.s[ start ];
    }
  }
  return start;
}

int skip_white_end( Line::Data& m
                  , const int start
                  ,       int finish )
{
  if( -1<finish )
  {
    for( uint8_t C = m.s[ finish ]
       ; start<=finish && (' '==C || '\t'==C || '\r'==C); )
    {
      finish--;
      if( start<=finish ) C = m.s[ finish ];
    }
  }
  return finish;
}

int calc_chksum( Line::Data& m )
{
  unsigned chk_sum = 0;

  int start = 0;
  int finish = 0<m.s.length() ? m.s.length()-1 : -1;

  start  = skip_white_beg( m, start );
  finish = skip_white_end( m, start, finish );

  for( int i=start; i<=finish; i++ )
  {
    chk_sum ^= m_primes[(i-start)%m_num_primes] ^ m.s[ i ];
    chk_sum = ((chk_sum << 13)&0xFFFFE000)
            | ((chk_sum >> 19)&0x00001FFF);
  }
  return chk_sum;
}

Line::Line()
  : m( *new(__FILE__,__LINE__) Data() )
{
}

Line::Line( unsigned cap )
  : m( *new(__FILE__,__LINE__) Data( cap ) )
{
}

Line::Line( const unsigned len, const uint8_t  fill )
  : m( *new(__FILE__, __LINE__) Data( len, fill ) )
{
}

Line::Line( const Line& a )
  : m( *new(__FILE__,__LINE__) Data( a.len() ) )
{
  copy( a );
}

Line::~Line()
{
  MemMark(__FILE__,__LINE__); delete &m;
}

void Line::clear()
{
  m.chksum_valid = false;

  m.s.clear();
}

unsigned Line::len() const { return m.s.length(); }
unsigned Line::cap() const { return m.s.capacity(); }

bool Line::set_len( const unsigned new_len )
{
  if( !inc_cap( new_len ) ) return false;

  // Fill in new values with zero:
  for( unsigned k=m.s.length(); k<new_len; k++ ) m.s.push_back( 0 );

  // If s is too long, remove values from end:
//for( unsigned k=m.s.length(); new_len<k; k-- ) m.s.pop_back();
  for( unsigned k=m.s.length(); new_len<k; k-- )
  {
    m.s.erase( m.s.begin() + m.s.length()-1 );
  }
  return true;
}

bool Line::inc_cap( unsigned new_cap )
{
  bool ok = true;

  if( m.s.capacity() < new_cap )
  {
    m.s.reserve( new_cap );
  }
  return ok;
}

bool Line::copy( const Line& a )
{
  if( this == &a ) return true;

  m.chksum_valid = false;
  m.s = a.m.s;

  return true;
}

uint8_t Line::get( const unsigned i ) const
{
  return m.s[i];
}

void Line::set( const unsigned i, const uint8_t C )
{
  m.chksum_valid = false;

  m.s[i] = C;
}

const char* Line::c_str( const unsigned i ) const
{
  if( i < m.s.length() )
  {
    return RCast<const char*>( &m.s[i] );
  }
  return 0;
}

bool Line::insert( const unsigned i
                 , const uint8_t  t )
{
  if( i<=m.s.length() )
  {
    m.s.insert( i, 1, t );
    return true;
  }
  return false;
}

bool Line::push( uint8_t t )
{
  m.s.append( 1, t );
  return true;
}

bool Line::remove( const unsigned i )
{
  if( i<m.s.length() )
  {
    m.chksum_valid = false;

    m.s.erase( m.s.begin() + i );

    return true;
  }
  return false;
}

bool Line::remove( const unsigned i, uint8_t& t )
{
  if( i<m.s.length() )
  {
    t = m.s[i];
  }
  return remove( i );
}

bool Line::pop( uint8_t& t )
{
  return 0<m.s.length() ? remove( m.s.length()-1, t ) : false;
}

bool Line::pop()
{
  return 0<m.s.length() ? remove( m.s.length()-1 ) : false;
}

bool Line::append( const Line& a )
{
  m.chksum_valid = false;

  if( this == &a ) return false;

  m.s.append( a.m.s );

  return true;
}

bool Line::ends_with( const uint8_t C )
{
  if( 0 < m.s.length() )
  {
    return C == m.s[ m.s.length()-1 ];
  }
  return false;
}

unsigned Line::chksum() const
{
  if( !m.chksum_valid )
  {
    m.chksum = calc_chksum(m);

    m.chksum_valid = true;
  }
  return m.chksum;
}

