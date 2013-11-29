
#ifndef __DEFINES_HPP__
#define __DEFINES_HPP__

#include <boost/Integer.hpp>


typedef boost::int_t<8>::least   Int8;
typedef boost::int_t<16>::least  Int16;
typedef boost::int_t<32>::least  Int32;
typedef boost::uint_t<8>::least  UInt8;
typedef boost::uint_t<16>::least UInt16;
typedef boost::uint_t<32>::least UInt32;

#ifdef _LONGLONG
typedef long long Int64;
typedef unsigned long long UInt64;
#endif /* _LONGLONG */

typedef Int8          SByte;
typedef Int16         SShort;
typedef Int32         SInt;
typedef signed long   SLong;
typedef UInt8         UByte;
typedef UInt16        UShort;
typedef UInt32        UInt;
typedef unsigned long ULong;
typedef UInt8         byte;

#define MAX_MESSAGE_LENGTH		UInt16(-1)

#endif /* __DEFINES_HPP__ */
