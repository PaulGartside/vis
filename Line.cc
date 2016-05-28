
#include "Utilities.hh"
#include "Line.hh"

const unsigned
Line::m_primes[] = {  43, 101, 149, 193, 241, 293, 353, 409, 461
                   , 521, 587, 641, 691, 757, 823, 881, 947 };

const unsigned Line::m_num_primes = sizeof( m_primes )/sizeof( unsigned );

Line::Line()
  : m_data()
  , m_chksum_valid( false )
  , m_chksum( 0 )
{
}

Line::Line( const char* _FILE_, const unsigned _LINE_, unsigned size )
  : m_data( _FILE_, _LINE_, size )
  , m_chksum_valid( false )
  , m_chksum( 0 )
{
}

Line::Line( const char* _FILE_, const unsigned _LINE_, unsigned size, uint8_t fill )
  : m_data( _FILE_, _LINE_, size, fill )
  , m_chksum_valid( false )
  , m_chksum( 0 )
{
}

void Line::clear()
{
  m_chksum_valid = false;

  m_data.clear();
}

bool Line::copy( const Line& a )
{
  m_chksum_valid = false;

  return m_data.copy( a.m_data );
}

uint8_t Line::get( const unsigned i ) const
{
  return m_data[i];
}

void Line::set( const unsigned i, const uint8_t C )
{
  m_chksum_valid = false;

  m_data[i] = C;
}

//const char* Line::c_str( const unsigned i ) const
//{
//  const unsigned LEN = len();
//  if( i < LEN )
//  {
//    if( LEN < sz() )
//    {
//      // Do the user of c_str() a favor and make sure m_data is
//      // null terminated so strcmp() will work right, else the
//      // user would have to use strncmp().
//      m_data[LEN] = 0;
//    }
//    return RCast<const char*>( m_data.get(i) );
//  }
//  return 0;
//}

const char* Line::c_str( const unsigned i ) const
{
  if( i < len() )
  {
    return RCast<const char*>( m_data.get(i) );
  }
  return 0;
}

bool Line::insert( const char* _FILE_, const unsigned _LINE_
                 , unsigned i, uint8_t t )
{
  m_chksum_valid = false;

  return m_data.insert( _FILE_, _LINE_, i, t );
}

bool Line::push( const char* _FILE_, const unsigned _LINE_, uint8_t t )
{
  m_chksum_valid = false;

  return m_data.push( _FILE_, _LINE_, t );
}

bool Line::remove( const unsigned i )
{
  m_chksum_valid = false;

  return m_data.remove( i );
}

bool Line::remove( const unsigned i, uint8_t& t )
{
  m_chksum_valid = false;

  return m_data.remove( i, t );
}

bool Line::pop( uint8_t& t )
{
  m_chksum_valid = false;

  return m_data.pop( t );
}
bool Line::pop()
{
  m_chksum_valid = false;

  return m_data.pop();
}

bool Line::append( const char* _FILE_, const unsigned _LINE_, const Line& a )
{
  m_chksum_valid = false;

  return m_data.append( _FILE_, _LINE_, a.m_data );
}

unsigned Line::chksum()
{
  if( !m_chksum_valid )
  {
    m_chksum = calc_chksum();

    m_chksum_valid = true;
  }
  return m_chksum;
}

int Line::calc_chksum() const
{
  unsigned chk_sum = 0;

  int start = 0;
  int finish = 0<m_data.len() ? m_data.len()-1 : -1;

  start  = skip_white_beg( start );
  finish = skip_white_end( start, finish );

  for( int i=start; i<=finish; i++ )
  {
    chk_sum ^= m_primes[(i-start)%m_num_primes] ^ m_data[ i ];
    chk_sum = ((chk_sum << 13)&0xFFFFE000)
            | ((chk_sum >> 19)&0x00001FFF);
  }
  return chk_sum;
}

int Line::skip_white_beg( int start ) const
{
  const unsigned LEN = m_data.len();

  if( 0<LEN )
  for( uint8_t C = m_data[ start ]
     ; start<LEN && (' '==C || '\t'==C || '\r'==C); )
  {
    start++;
    if( start<LEN ) C = m_data[ start ];
  }
  return start;
}

int Line::skip_white_end( const int start
                        ,       int finish ) const
{
  if( -1<finish )
  for( uint8_t C = m_data[ finish ]
     ; start<=finish && (' '==C || '\t'==C || '\r'==C); )
  {
    finish--;
    if( start<=finish ) C = m_data[ finish ];
  }
  return finish;
}

