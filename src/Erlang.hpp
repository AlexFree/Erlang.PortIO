/*

*/

#ifndef __ERLANG_HPP__
#define __ERLANG_HPP__
//-------------------------------------------------------------------------------------------------
#include <stdexcept>
#include <vector>
#include <memory>
#include <limits>
#include <string.h>
#include <crtdbg.h>
#include <math.h>

#include "IOStream.hpp"
//-------------------------------------------------------------------------------------------------
using namespace IOStream;
//-------------------------------------------------------------------------------------------------
namespace Erlang
{
	enum ETFTag // External Term Format Tag
	{
		ERL_VERSION = 131,
		SMALL_TUPLE_EXT = 104,
		LARGE_TUPLE_EXT = 105,
		NEW_FLOAT_EXT = 70,
		SMALL_BIG_EXT = 110,
		LARGE_BIG_EXT = 111,
		SMALL_INTEGER_EXT = 97,
		INTEGER_EXT = 98,
		NIL_EXT = 106,
		STRING_EXT = 107,
		LIST_EXT = 108,
		BINARY_EXT = 109,
		ATOM_EXT = 100,
		SMALL_ATOM_EXT = 115,
		ATOM_CACHE_REF = 82,
		REFERENCE_EXT = 101,
		NEW_REFERENCE_EXT = 114,
	};
	
	class RawData
	{
		private: byte* pBuffer_;
		private: size_t Size_;
		private: ETFTag TermTag_;

		protected: RawData(ETFTag termTag, const byte* pBuffer, size_t size):
			TermTag_(termTag),
			pBuffer_(NULL),
			Size_(0)
		{
			pBuffer_ = new byte[size];
			Size_ = size;
			memcpy(pBuffer_, pBuffer, Size_);
		}
		
		public: RawData(const RawData& rhs):
			pBuffer_(NULL),
			Size_(0)
		{
			operator =(rhs);
		}
		
		public: virtual ~RawData(void)
		{
			if(pBuffer_)
				delete[] pBuffer_;
		}
		
		public: bool operator ==(const RawData& rhs) const
		{
			return (Size_ != rhs.Size_ ? false : !memcmp(pBuffer_, rhs.pBuffer_, Size_));
		}
		
		public: bool operator !=(const RawData& rhs) const
		{
			return !(*this == rhs);
		}
		
		public: RawData& operator =(const RawData& rhs)
		{
			if(&rhs != this) {
				byte* p = new byte[rhs.Size_];
				if(pBuffer_)
					delete[] pBuffer_;
				pBuffer_ = p;
				Size_ = rhs.Size_;
				memcpy(pBuffer_, rhs.pBuffer_, Size_);
			}
			return *this;
		}
		
		public: operator const byte*(void) const
		{
			return pBuffer_;
		}
		
		public: size_t Size(void) const
		{
			return Size_;
		}

		public: ETFTag TermTag(void) const
		{
			return TermTag_;
		}
	};
	
	class Binary: public RawData
	{
		friend class ETFReader; // friend cReference cETFReader::ReadReference(void);

		private: Binary(const byte* pBuffer, size_t size):
			RawData(BINARY_EXT, pBuffer, size)
		{
		}
	};
	
	class Reference: public RawData
	{
		friend class ETFReader; // friend cReference cETFReader::ReadReference(void);

		private: Reference(const byte* pBuffer, size_t size):
			RawData(NEW_REFERENCE_EXT, pBuffer, size)
		{
			if(!pBuffer || !size)
				throw std::invalid_argument("Zero Buffer Argument");
		}
	};

	class ETFReader // External Term Format Reader
	{
		private: byte* Ptr_;
		private: const byte* pBuffer_;
		private: size_t Size_;
		
		public: ETFReader(const byte* pBuf, size_t size):
			Ptr_(NULL),
			pBuffer_(NULL),
			Size_(0)
		{
			if(!size)
				return;
			if(!pBuf)
				throw std::invalid_argument("Zero Buffer Argument");
			if(*pBuf != ERL_VERSION)
				throw std::invalid_argument("Invalid Version (Current is 131)");
			
			byte* p = new byte[size];
			Size_ = size;
			memcpy(p, pBuf, Size_);
			pBuffer_ = Ptr_ = p;
			++pBuffer_; // Omit Version Number
		}
		
		public: ETFReader(const ETFReader& rhs):
			Ptr_(NULL),
			pBuffer_(NULL),
			Size_(0)
		{
			operator =(rhs);
		}
		
		public: ~ETFReader(void)
		{
			if(Ptr_)
				delete[] Ptr_;
		}
		
		public: ETFReader& operator =(const ETFReader& rhs)
		{
			if(this != &rhs) {
				byte* p = new byte[rhs.Size_];
				if(Ptr_)
					delete[] Ptr_;
				pBuffer_ = Ptr_ = p;
				Size_ = rhs.Size_;
				memcpy(Ptr_, rhs.Ptr_, Size_);
				pBuffer_ += (rhs.pBuffer_ - rhs.Ptr_);
			}
			return *this;
		}
		
		private: size_t RestSize(void) const
		{
			return (Size_ - (pBuffer_ - Ptr_));
		}
		
		public: operator bool(void) const
		{
			return RestSize() > 0;
		}
		
		public: template<typename T> std::vector<T> ToVector(void) const
		{
			return std::vector<T>(&Ptr_[0], &Ptr_[Size_]);
		}
		
		public: UInt8 GetNextTag(void) const
		{
			UInt8 tag = 0;
			const byte* pPos = pBuffer_;
			if(RestSize() < sizeof(tag))
				return 0;
			RWBinary::Read(pPos, tag);
			return tag;
		}
		
		public: UInt32 ReadTuple(void)
		{
			UInt8 tag = 0;
			UInt8 smallTuple = 0;
			UInt32 largeTuple = 0;
			const byte* pPos = pBuffer_;
			size_t count = RestSize();
			
			if(count < sizeof(tag))
				throw std::out_of_range("Out of Buffer Range");
			count -= sizeof(tag);
			pPos = RWBinary::Read(pPos, tag);
			if(!(tag == SMALL_TUPLE_EXT || tag == LARGE_TUPLE_EXT))
				throw std::runtime_error("Invalid Operation");
			if(	(tag == SMALL_TUPLE_EXT && count < sizeof(smallTuple)) || 
					(tag == LARGE_TUPLE_EXT && count < sizeof(largeTuple)))
				throw std::out_of_range("Out of Buffer Range");
			
			pBuffer_ = (tag == SMALL_TUPLE_EXT ? RWBinary::Read(pPos, smallTuple) : RWBinary::Read(pPos, largeTuple));
			return (tag == SMALL_TUPLE_EXT ? smallTuple : largeTuple);
		}
		
		// T is the signed/unsigned number: 8bit, 16bit, 32bit, 64bit.
		// A float is stored as 8 bytes in big-endian IEEE format.
		// IEEE float format is used in minor version 1 of the external format.
		public: template<typename T> T ReadNumber(void)
		{
			UInt8 tag = 0;
			const byte* pPos = pBuffer_;
			size_t count = RestSize();
			
			if(count < sizeof(tag))
				throw std::out_of_range("Out of Buffer Range");
			count -= sizeof(tag);
			pPos = RWBinary::Read(pPos, tag);
			
			if(tag == SMALL_BIG_EXT || tag == LARGE_BIG_EXT) {
				T value = T();
				T maxT = (*std::numeric_limits<T>::max)();
				bool hasTSign = !((T)(-1) > 0);
				UInt8 sign = 0;
				UInt8 size8 = 0;
				UInt32 size32 = 0;
				
				if((tag == SMALL_BIG_EXT && count < sizeof(size8)) || (tag == LARGE_BIG_EXT && count < sizeof(size32)))
					throw std::out_of_range("Out of Buffer Range");
				count -= (tag == SMALL_BIG_EXT ? sizeof(size8) : sizeof(size32));
				pPos = (tag == SMALL_BIG_EXT ? RWBinary::Read(pPos, size8) : RWBinary::Read(pPos, size32));
				if(count < sizeof(sign))
					throw std::out_of_range("Out of Buffer Range");
				count -= sizeof(sign);
				pPos = RWBinary::Read(pPos, sign);
				if(sign == 1 && !hasTSign)
					throw std::bad_cast("Cast Negative Integer to Unsigned");
				
				UInt32 size = (tag == SMALL_BIG_EXT ? size8 : size32);
				for(UInt32 i = 0; i < size; ++i) {
					UInt8 digit = 0;
					if(count < sizeof(digit))
						throw std::out_of_range("Out of Buffer Range");
					count -= sizeof(digit);
					pPos = RWBinary::Read(pPos, digit);
					long double frame = digit*pow(256.0, (double)i);
					if(errno == ERANGE || !frame)
						throw std::overflow_error("pow Overflow");
					if(frame > maxT || value > maxT - frame) // ERROR: as frame can be negative value
						throw std::overflow_error("Overflow Integer");
					value = (T)value + (T)frame; // Can't use value += ... because of warning for type < int
				}
				
				pBuffer_ = pPos;
				return (sign == 1 ? (T)(-1)*value : value);
			}
			else if(tag == SMALL_INTEGER_EXT || tag == INTEGER_EXT) {
				UInt8 value8 = 0;
				UInt32 value32 = 0;
				if(	(tag == SMALL_INTEGER_EXT && count < sizeof(value8)) || 
						(tag == INTEGER_EXT && count < sizeof(value32)))
					throw std::out_of_range("Out of Buffer Range");
				if(tag == INTEGER_EXT && sizeof(T) < sizeof(value32))
					throw std::bad_cast("Cast Big Integer to Small Integer");
				
				pBuffer_ = (tag == SMALL_INTEGER_EXT ? RWBinary::Read(pPos, value8) : RWBinary::Read(pPos, value32));
				return (tag == SMALL_INTEGER_EXT ? (T)value8 : (T)value32);
			}
			else if(tag == NEW_FLOAT_EXT) {
				UInt64 value = 0;
				if(count < sizeof(value))
					throw std::out_of_range("Out of Buffer Range");
				if(sizeof(T) < sizeof(value))
					throw std::bad_cast("Cast Big Type to Small Type");
				pBuffer_ = RWBinary::Read(pPos, value);
				return *(T*)(&value);
			}
			else
				throw std::invalid_argument("Invalid Operation");
		}
		
		public: void ReadNil(void)
		{
			UInt8 tag = 0;
			const byte* pPos = pBuffer_;
			size_t count = RestSize();
			
			if(count < sizeof(tag))
				throw std::out_of_range("Out of Buffer Range");
			count -= sizeof(tag);
			pPos = RWBinary::Read(pPos, tag);
			if(tag != NIL_EXT)
				throw std::runtime_error("Invalid Operation");
			
			pBuffer_ = pPos;
		}
		
		public: UInt8* ReadASCII(void)
		{
			UInt8 tag = 0;
			UInt16 size = 0;
			UInt8* str = NULL;
			const byte* pPos = pBuffer_;
			size_t count = RestSize();
			
			if(sizeof(count) < sizeof(size))
				throw std::bad_cast("Huge Size of Array");
			if(count < sizeof(tag))
				throw std::out_of_range("Out of Buffer Range");
			count -= sizeof(tag);
			pPos = RWBinary::Read(pPos, tag);
			if(tag == NIL_EXT) {
				pBuffer_ = pPos;
				str = new UInt8[1];
				str[0] = '\0';
				return str;
			}
			if(tag != STRING_EXT)
				throw std::runtime_error("Invalid Operation");
			if(count < sizeof(size))
				throw std::out_of_range("Out of Buffer Range");
			count -= sizeof(size);
			pPos = RWBinary::Read(pPos, size);
			if(!size)
				throw std::length_error("Invalid Null String Size");
			if(count < size)
				throw std::out_of_range("Out of Buffer Range");
			
			str = new UInt8[size + 1];
			pPos = RWBinary::Read(pPos, str, size);
			str[size] = '\0';
			
			pBuffer_ = pPos;
			return str;
		}
		
		public: UInt16* ReadUnicode(void)
		{
			UInt8 tag = 0;
			UInt32 size = 0;
			UInt16* str = NULL;
			UInt16 c = 0;
			UInt16 maxUInt16 = (*std::numeric_limits<UInt16>::max)();
			const byte* pPos = pBuffer_;
			size_t count = RestSize();
			
			if(sizeof(count) < sizeof(size))
				throw std::bad_cast("Huge Size of Array");
			if(count < sizeof(tag))
				throw std::out_of_range("Out of Buffer Range");
			count -= sizeof(tag);
			pPos = RWBinary::Read(pPos, tag);
			if(tag == NIL_EXT) {
				pBuffer_ = pPos;
				str = new UInt16[1];
				str[0] = L'\0';
				return str;
			}
			if(tag != LIST_EXT)
				throw std::runtime_error("Invalid Operation");
			if(count < sizeof(size))
				throw std::out_of_range("Out of Buffer Range");
			count -= sizeof(size);
			pPos = RWBinary::Read(pPos, size);
			if(!size)
				throw std::length_error("Invalid Null String Size");
			
			// Read String
			str = new UInt16[size + 1];
			for(UInt32 i = 0; i < size; ++i) {
				if(count < sizeof(tag)) {
					delete[] str;
					throw std::out_of_range("Out of Buffer Range");
				}
				count -= sizeof(tag);
				pPos = RWBinary::Read(pPos, tag);
				if(tag == SMALL_INTEGER_EXT || tag == INTEGER_EXT) {
					Int8 value8 = 0;
					Int32 value32 = 0;
					if(	(tag == SMALL_INTEGER_EXT && count < sizeof(value8)) || 
							(tag == INTEGER_EXT && count < sizeof(value32))) {
						delete[] str;
						throw std::out_of_range("Out of Buffer Range");
					}
					count -= (tag == SMALL_INTEGER_EXT ? sizeof(value8) : sizeof(value32));
					pPos = (tag == SMALL_INTEGER_EXT ? RWBinary::Read(pPos, value8) : RWBinary::Read(pPos, value32));
					if(value32 > maxUInt16) {
						delete[] str;
						throw std::bad_cast("Cast Big Integer to Small Integer");
					}
					c = (UInt16)(tag == SMALL_INTEGER_EXT ? value8 : value32);
				}
				else {
					delete[] str;
					throw std::runtime_error("Invalid Operation");
				}
				str[i] = c;
			}
			
			// Read Tail - Nil
			if(count < sizeof(tag)) {
				delete[] str;
				throw std::out_of_range("Out of Buffer Range");
			}
			count -= sizeof(tag);
			pPos = RWBinary::Read(pPos, tag);
			if(tag != NIL_EXT) {
				delete[] str;
				throw std::runtime_error("Invalid Operation");
			}
			
			str[size] = 0;
			pBuffer_ = pPos;
			return str;
		}
		
		public: UInt32 ReadList(void)
		{
			UInt8 tag = 0;
			UInt32 value = 0;
			const byte* pPos = pBuffer_;
			size_t count = RestSize();
			
			if(count < sizeof(tag))
				throw std::out_of_range("Out of Buffer Range");
			count -= sizeof(tag);
			pPos = RWBinary::Read(pPos, tag);
			if(tag != LIST_EXT)
				throw std::runtime_error("Invalid Operation");
			if(count < sizeof(value))
				throw std::out_of_range("Out of Buffer Range");
			
			pBuffer_ = RWBinary::Read(pPos, value);
			return value;
		}
		
		public: UInt8* ReadAtom(void)
		{
			UInt8 tag = 0;
			UInt16 size = 0, size16 = 0;
			UInt8 size8 = 0;
			UInt8* str = NULL;
			const byte* pPos = pBuffer_;
			size_t count = RestSize();
			
			if(sizeof(count) < sizeof(size))
				throw std::bad_cast("Huge Size of Array");
			if(count < sizeof(tag))
				throw std::out_of_range("Out of Buffer Range");
			count -= sizeof(tag);
			pPos = RWBinary::Read(pPos, tag);
			if(!(tag == SMALL_ATOM_EXT || tag == ATOM_EXT))
				throw std::runtime_error("Invalid Operation");
			if((tag == SMALL_ATOM_EXT && count < sizeof(size8)) || (tag == ATOM_EXT && count < sizeof(size16)))
				throw std::out_of_range("Out of Buffer Range");
			count -= (tag == SMALL_ATOM_EXT ? sizeof(size8) : sizeof(size16));
			pPos = (tag == SMALL_ATOM_EXT ? RWBinary::Read(pPos, size8) : RWBinary::Read(pPos, size16));
			size = (tag == SMALL_ATOM_EXT ? size8 : size16);
			if(!size || size > 255)
				throw std::length_error("Invalid String Size");
			if(count < size)
				throw std::out_of_range("Out of Buffer Range");
			
			str = new UInt8[size + 1];
			pPos = RWBinary::Read(pPos, str, size);
			str[size] = '\0';
			
			pBuffer_ = pPos;
			return str;
		}
		
		public: Reference ReadReference(void)
		{
 			UInt8 tag = 0;
 			UInt8 tag2 = 0;
			UInt16 len = 0;
			UInt16 size = 0, size16 = 0;
			UInt8 size8 = 0;
			const byte* pPos = pBuffer_;
			size_t count = RestSize();
			
			if(count < sizeof(tag))
				throw std::out_of_range("Out of Buffer Range");
			count -= sizeof(tag);
			pPos = RWBinary::Read(pPos, tag);
			if(!(tag == REFERENCE_EXT || tag == NEW_REFERENCE_EXT))
				throw std::runtime_error("Invalid Operation");
			
			// Read Len (2 bytes) for NEW_REFERENCE_EXT
			if(tag == NEW_REFERENCE_EXT) {
				if(count < sizeof(len))
					throw std::out_of_range("Out of Buffer Range");
				count -= sizeof(len);
				pPos = RWBinary::Read(pPos, len);
			}
			
			// Move N-bytes atom - Node name of reference
			if(count < sizeof(tag2))
				throw std::out_of_range("Out of Buffer Range");
			count -= sizeof(tag2);
			pPos = RWBinary::Read(pPos, tag2);
			if(tag2 == ATOM_CACHE_REF) {
				if(count < sizeof(UInt8))
					throw std::out_of_range("Out of Buffer Range");
				count -= sizeof(UInt8);
				pPos += sizeof(UInt8);
			}
			else if(tag2 == ATOM_EXT || tag2 == SMALL_ATOM_EXT) {
				if((tag == SMALL_ATOM_EXT && count < sizeof(size8)) || (tag == ATOM_EXT && count < sizeof(size16)))
					throw std::out_of_range("Out of Buffer Range");
				count -= (tag == SMALL_ATOM_EXT ? sizeof(size8) : sizeof(size16));
				pPos = (tag == SMALL_ATOM_EXT ? RWBinary::Read(pPos, size8) : RWBinary::Read(pPos, size16));
				size = (tag == SMALL_ATOM_EXT ? size8 : size16);
				if(!size || size > 255)
					throw std::length_error("Invalid String Size");
				if(count < size)
					throw std::out_of_range("Out of Buffer Range");
				pPos += (size_t)(size);
				count -= size;
			}
			else
				throw std::runtime_error("Invalid Operation");
			
			// Move 4 bytes (ID) for REFERENCE_EXT
			if(tag == REFERENCE_EXT) {
				if(count < sizeof(UInt32))
					throw std::out_of_range("Out of Buffer Range");
				count -= sizeof(UInt32);
				pPos += sizeof(UInt32);
			}
			
			// Move 1 byte (Creation)
			if(count < sizeof(UInt8))
				throw std::out_of_range("Out of Buffer Range");
			count -= sizeof(UInt8);
			pPos += sizeof(UInt8);
			
			// Move N*-byte (ID) for NEW_REFERENCE_EXT
			if(tag == NEW_REFERENCE_EXT) {
				_ASSERTE(len);
				if(count < 4U*len)
					throw std::out_of_range("Out of Buffer Range");
				count -= (size_t)(4U*len);
				pPos += (4U*len);
			}
			
			Reference ref(pBuffer_, (size_t)(pPos - pBuffer_));
			pBuffer_ = pPos;
			return ref;
		}

		public: Binary ReadBinary(void)
		{
			UInt8 tag = 0;
			UInt32 len = 0;
			const byte* pPos = pBuffer_;
			size_t count = RestSize();
			
			if(count < sizeof(tag))
				throw std::out_of_range("Out of Buffer Range");
			count -= sizeof(tag);
			pPos = RWBinary::Read(pPos, tag);
			if(tag != BINARY_EXT)
				throw std::runtime_error("Invalid Operation");
			if(count < sizeof(len))
				throw std::out_of_range("Out of Buffer Range");
			count -= sizeof(len);
			pPos = RWBinary::Read(pPos, len);
			if(count < len)
				throw std::out_of_range("Out of Buffer Range");

			count -= len;
			pPos += len;
			Binary binary(pBuffer_, (size_t)(pPos - pBuffer_));

			pBuffer_ = pPos;
			return binary;
		}
	};
	
	class ETFWriter // External Term Format Writer
	{
		private: static const size_t INITIAL_SIZE = 1024;
		
		private: byte* Ptr_;
		private: byte* pBuffer_;
		private: size_t Size_;
		
		public: ETFWriter(void):
			Ptr_(NULL),
			pBuffer_(NULL),
			Size_(0)
		{
			Ptr_ = pBuffer_ = new byte[INITIAL_SIZE];
			Size_ = INITIAL_SIZE;
			*pBuffer_ = (byte)ERL_VERSION; // Write Version Number
			++pBuffer_;
		}
		
		public: ETFWriter(const ETFWriter& rhs):
			Ptr_(NULL),
			pBuffer_(NULL),
			Size_(0)
		{
			operator =(rhs);
		}
		
		public: ~ETFWriter(void)
		{
			if(Ptr_)
				delete[] Ptr_;
		}
		
		private: void WriteToBuffer(const byte* pSrcBuffer, size_t srcCount)
		{
			_ASSERTE(pSrcBuffer);
			_ASSERTE(srcCount);
			
			size_t count = BytesCount();
			size_t rest = Size_ - count;
			size_t maxSize = (std::numeric_limits<size_t>::max)();
			
			// Reallocate Buffer for New Chunk
			if(rest < srcCount) {
				size_t addSize = ((srcCount - rest)/INITIAL_SIZE + 1)*INITIAL_SIZE;
				// If Size_ + addSize > maxSize then make addSize up to maxSize
				addSize = (Size_ > maxSize - addSize ? maxSize - Size_ : addSize);
				// Check if new size can cover written buffer size
				if(rest + addSize < srcCount)
					throw std::overflow_error("Can't Allocate");
				// Allocate New Buffer
				_ASSERTE(Size_ <= maxSize - addSize);
				byte* pNewBuffer = new byte[Size_ + addSize];
				memcpy(pNewBuffer, Ptr_, count);
				delete[] Ptr_;
				Ptr_ = pBuffer_ = pNewBuffer;
				pBuffer_ += count;
				Size_ += addSize;
				_ASSERTE(count == BytesCount());
			}
			
			// Copy source buffer to dest
			memcpy(pBuffer_, pSrcBuffer, srcCount);
			pBuffer_ += srcCount;
		}
		
		public: ETFWriter& operator =(const ETFWriter& rhs)
		{
			if(this != &rhs) {
				byte* p = new byte[rhs.Size_];
				if(Ptr_)
					delete[] Ptr_;
				Ptr_ = pBuffer_ = p;
				Size_ = rhs.Size_;
				memcpy(Ptr_, rhs.Ptr_, Size_);
				pBuffer_ += rhs.BytesCount();
			}
			return *this;
		}
		
		public: operator const byte*(void) const
		{
			return Ptr_;
		}
		
		public: template<typename T> std::vector<T> ToVector(void) const
		{
			return std::vector<T>(&Ptr_[0], &Ptr_[BytesCount()]);
		}
		
		public: size_t BytesCount(void) const
		{
			return size_t(pBuffer_ - Ptr_);
		}
		
		public: ETFWriter& WriteTuple(UInt32 tupleSize)
		{
			byte tuple[] = { LARGE_TUPLE_EXT, 0, 0, 0, 0 };
			RWBinary::Write(&tuple[1], tupleSize);
			WriteToBuffer(tuple, sizeof(tuple));
			return *this;
		}
		
		public: template<typename T> ETFWriter& WriteNumber(T number)
		{
			if(sizeof(T)*8 > 32) {
				byte bnumber[] = { NEW_FLOAT_EXT, 0, 0, 0, 0, 0, 0, 0, 0 };
				RWBinary::Write(&bnumber[1], (1.0*number)); // convert small type to double
				WriteToBuffer(bnumber, sizeof(bnumber));
			}
			else {
				byte bnumber[] = { INTEGER_EXT, 0, 0, 0, 0 };
				RWBinary::Write(&bnumber[1], 1*number); // convert small type to integer
				WriteToBuffer(bnumber, sizeof(bnumber));
			}
			return *this;
		}
		
		public: ETFWriter& WriteNil(void)
		{
			byte nil[] = { NIL_EXT };
			WriteToBuffer(nil, sizeof(nil));
			return *this;
		}
		
		public: ETFWriter& WriteString(const char* str)
		{
			return WriteString((const unsigned char*)str);
		}
		
		public: ETFWriter& WriteString(const unsigned char* str)
		{
			size_t strLen = (str ? strlen((const char*)str) : 0);
			size_t listLen = 1 + 4 + (1 + 1)*strLen + 1;
			std::auto_ptr<byte> list(new byte[listLen]);
			byte* ptr = list.get();
			
			*ptr++ = LIST_EXT;
			RWBinary::Write(ptr, strLen);
			ptr += 4;
			for(size_t i = 0; i < strLen; ++i) {
				*ptr++ = SMALL_INTEGER_EXT;
				*ptr++ = str[i];
			}
			*ptr++ = NIL_EXT;
			
			_ASSERTE(size_t(ptr - list.get()) == listLen);
			WriteToBuffer(list.get(), listLen);
			
			return *this;
	}
		
		public: ETFWriter& WriteString(const wchar_t* str)
		{
			size_t strLen = (str ? wcslen(str) : 0);
			size_t listLen = 1 + 4 + (1 + 4)*strLen + 1;
			std::auto_ptr<byte> list(new byte[listLen]);
			byte* ptr = list.get();
			
			*ptr++ = LIST_EXT;
			RWBinary::Write(ptr, strLen);
			ptr += 4;
			for(size_t i = 0; i < strLen; ++i) {
				*ptr++ = INTEGER_EXT;
				RWBinary::Write(ptr, (UInt32)str[i]);
				ptr += 4;
			}
			*ptr++ = NIL_EXT;
			
			_ASSERTE(size_t(ptr - list.get()) == listLen);
			WriteToBuffer(list.get(), listLen);
			
			return *this;
		}
		
		public: ETFWriter& WriteList(UInt32 listSize)
		{
			byte list[] = { LIST_EXT, 0, 0, 0, 0 };
			RWBinary::Write(&list[1], listSize);
			WriteToBuffer(list, sizeof(list));
			return *this;
		}
		
		public: ETFWriter& WriteAtom(const unsigned char* atomName)
		{
			size_t atomNameLen = (atomName ? strlen((const char*)atomName) : 0);
			byte atom[1 + 2 + 255] = { ATOM_EXT, 0, };
			if(!atomNameLen || atomNameLen > 255)
				throw std::length_error("Invalid Length of Atom Name");
			RWBinary::Write(&atom[1], (UInt16)atomNameLen);
			RWBinary::Write(&atom[3], atomName, NULL);
			WriteToBuffer(atom, 1 + 2 + atomNameLen);
			return *this;
		}
		
		public: ETFWriter& WriteAtom(const char* atomName)
		{
			return WriteAtom((const unsigned char*)atomName);
		}
		
		public: ETFWriter& WriteReference(const Reference& ref)
		{
			WriteToBuffer(ref, ref.Size());
			return *this;
		}

		public: ETFWriter& WriteBinary(const Binary& bin)
		{
			WriteToBuffer(bin, bin.Size());
			return *this;
		}
	};
}
//-------------------------------------------------------------------------------------------------
#endif /* __ERLANG_HPP__ */
