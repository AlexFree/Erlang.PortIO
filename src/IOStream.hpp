/*

*/

#ifndef __IOSTREAM_HPP__
#define __IOSTREAM_HPP__
//-------------------------------------------------------------------------------------------------
#include <fcntl.h>
#include <stdio.h>
#include <io.h>

#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>

#include "Defines.hpp"
//-------------------------------------------------------------------------------------------------
namespace IOStream
{
	struct ErrorInfo
	{
		public: bool WasError;
		public: int ReturnValue;
		public: int ErrorCode;
		
		public: ErrorInfo(bool wasError = false, int returnValue = 0, int errorCode = 0):
			WasError(wasError),
			ReturnValue(returnValue),
			ErrorCode(errorCode)
		{
		}
	};
	
	class Stream
	{
		public: enum FileDescriptor
		{
			StdIn,
			StdOut,
			StdErr,
		};
		
		public: enum Mode
		{
			Text,
			Binary,
		};
		
		public: static int SetMode(Stream::FileDescriptor fd, Stream::Mode mode)
		{
			int m = (mode == Stream::Text ? _O_TEXT : _O_BINARY);
			int f = (fd == Stream::StdIn ? _fileno(stdin) : (fd == Stream::StdOut ? _fileno(stdout) : _fileno(stderr)));
			return _setmode(f, m);
		}
		
		private: static boost::mutex& GetReadMutex(void)
		{
			static boost::mutex m;
			return m;
		}
		
		private: static boost::mutex& GetWriteMutex(void)
		{
			static boost::mutex m;
			return m;
		}
		
		public: static UInt16 Read2(byte* pBuf, ErrorInfo* pErrorInfo = NULL)
		{
			boost::mutex::scoped_lock lock(GetReadMutex());
			if(ReadImpl(pBuf, 2, pErrorInfo) != 2)
				return 0;
			UInt16 len = (UInt16(pBuf[0]) << 8) | (UInt16(pBuf[1]) << 0);
			return (UInt16)ReadImpl(pBuf, len, pErrorInfo);
		}
		
		private: static size_t ReadImpl(byte* pBuf, size_t len, ErrorInfo* pErrorInfo)
		{
			size_t got = 0;
			do {
				int count = _read(0, pBuf + got, (unsigned)(len - got));
				if(count <= 0) {
					if(pErrorInfo)
						*pErrorInfo = ErrorInfo(count < 0, count, errno);
					return 0;
				}
				got += (size_t)count;
			} while(got < len);
			return got;
		}
		
		public: static UInt16 Write2(const byte* pBuf, UInt16 len, ErrorInfo* pErrorInfo = NULL)
		{
			boost::mutex::scoped_lock lock(GetWriteMutex());
			byte blen[2] = { byte((len >> 8) & 0xff), byte((len >> 0) & 0xff) };
			if(WriteImpl(blen, 2, pErrorInfo) != 2)
				return 0;
			return (UInt16)WriteImpl(pBuf, len, pErrorInfo);
		}
		
		private: static size_t WriteImpl(const byte* pBuf, size_t len, ErrorInfo* pErrorInfo)
		{
			size_t wrote = 0;
			do {
				int count = _write(1, pBuf + wrote, (unsigned)(len - wrote));
				if(count <= 0) {
					if(pErrorInfo)
						*pErrorInfo = ErrorInfo(count < 0, count, errno);
					return 0;
				}
				wrote += (size_t)count;
			} while(wrote < len);
			return wrote;
		}
	};
	
	class RWBinary
	{
		private: template<typename T> struct RWHelper
		{
			public: template<unsigned> static const byte* ReadNumber(const byte*, T&);
			public: template<unsigned> static const byte* ReadString(const byte*, T*, size_t);
			public: template<unsigned> static byte* WriteNumber(byte*, const T&);
			public: template<unsigned> static byte* WriteString(byte*, const T*, size_t*);
			
			public: template<> static const byte* ReadNumber<8>(const byte* p, T& v)
			{
				v = (T(p[0]) << 0);
				return &p[1];
			}
		
			public: template<> static const byte* ReadNumber<16>(const byte* p, T& v)
			{
				v = (T(p[0]) << 8) | (T(p[1]) << 0);
				return &p[2];
			}
			
			public: template<> static const byte* ReadNumber<32>(const byte* p, T& v)
			{
				v = (T(p[0]) << 24) | (T(p[1]) << 16) | (T(p[2]) << 8) | (T(p[3]) << 0);
				return &p[4];
			}
			
			public: template<> static const byte* ReadNumber<64>(const byte* p, T& v)
			{
				UInt64 v64;
				v64 = (UInt64(p[0]) << 56) | (UInt64(p[1]) << 48) | (UInt64(p[2]) << 40) | (UInt64(p[3]) << 32) | 
							(UInt64(p[4]) << 24) | (UInt64(p[5]) << 16) | (UInt64(p[6]) << 8) | (UInt64(p[7]) << 0);
				v = *(T*)(&v64);
				return &p[8];
			}
		
			public: template<> static const byte* ReadString<8>(const byte* p, T* str, size_t count) // ASCII
			{
				memcpy((char*)str, (char*)p, count);
				return &p[count];
			}
			
			public: template<> static const byte* ReadString<16>(const byte* p, T* str, size_t count) // Unicode
			{
				wmemcpy((wchar_t*)str, (wchar_t*)p, count);
				return &p[2*count];
			}
			
			public: template<> static byte* WriteNumber<8>(byte* p, const T& v)
			{
				p[0] = (UInt8(v >> 0) & 0xff);
				return &p[1];
			}
			
			public: template<> static byte* WriteNumber<16>(byte* p, const T& v)
			{
				p[0] = (UInt8(v >> 8) & 0xff);
				p[1] = (UInt8(v >> 0) & 0xff);
				return &p[2];
			}
			
			public: template<> static byte* WriteNumber<32>(byte* p, const T& v)
			{
				p[0] = (UInt8(v >> 24) & 0xff);
				p[1] = (UInt8(v >> 16) & 0xff);
				p[2] = (UInt8(v >> 8) & 0xff);
				p[3] = (UInt8(v >> 0) & 0xff);
				return &p[4];
			}
			
			public: template<> static byte* WriteNumber<64>(byte* p, const T& v)
			{
				UInt64 v64 = *(UInt64*)(&v);
				p[0] = (UInt8(v64 >> 56) & 0xff);
				p[1] = (UInt8(v64 >> 48) & 0xff);
				p[2] = (UInt8(v64 >> 40) & 0xff);
				p[3] = (UInt8(v64 >> 32) & 0xff);
				p[4] = (UInt8(v64 >> 24) & 0xff);
				p[5] = (UInt8(v64 >> 16) & 0xff);
				p[6] = (UInt8(v64 >> 8) & 0xff);
				p[7] = (UInt8(v64 >> 0) & 0xff);
				return &p[8];
			}
		
			public: template<> static byte* WriteString<8>(byte* p, const T* str, size_t* pCount) // ASCII
			{
				*pCount = (str ? strlen((const char*)str) : 0 );
				memcpy((char*)p, (char*)str, *pCount);
				return &p[*pCount];
			}
			
			public: template<> static byte* WriteString<16>(byte* p, const T* str, size_t* pCount) // Unicode
			{
				*pCount = (str ? wcslen((wchar_t*)str) : 0);
				wmemcpy((wchar_t*)p, (wchar_t*)str, *pCount);
				return &p[2*(*pCount)];
			}
		};
		
		public: template<typename T> static const byte* Read(const byte* p, T& v)
		{
			return (p ? RWHelper<T>::ReadNumber<sizeof(T)*8>(p, v) : p);
		}
		
		public: template<typename T> static const byte* Read(const byte* p, T* str, size_t count)
		{
			return ((p && str) ? RWHelper<T>::ReadString<sizeof(T)*8>(p, str, count) : p);
		}
		
		public: template<typename T> static byte* Write(byte* p, const T& v)
		{
			return (p ? RWHelper<T>::WriteNumber<sizeof(T)*8>(p, v) : p);
		}
		
		public: template<typename T> static byte* Write(byte* p, const T* str, size_t* pCount)
		{
			size_t temp;
			return ((p && str) ? RWHelper<T>::WriteString<sizeof(T)*8>(p, str, (pCount ? pCount : &temp)) : p);
		}
	};

}
//-------------------------------------------------------------------------------------------------
#endif /* __IOSTREAM_HPP__ */
