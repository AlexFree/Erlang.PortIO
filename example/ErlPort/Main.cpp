/*

*/

#include <sys/stat.h>
#include <crtdbg.h>
#include <eh.h>
#include <fstream>

#include "IOStream.hpp"
#include "Erlang.hpp"
#include "Defines.hpp"

class Application
{
	private: Application(void)
	{
	}
	
	private: static void unexpected_function(void)
	{
		Log("An Unexpected Exception! Terminate!");
		terminate();
	}
	
	private: template<typename T> static void Log(const T& s)
	{
		std::wofstream OFStream;
		try
		{
			OFStream.open(L"Log.txt", std::ios::app);
			OFStream<<s<<L"\n";
			OFStream.flush();
			OFStream.close();
		}
		catch(...)
		{
			OFStream.flush();
			OFStream.close();
			std::cerr<<"Log Error";
		}
	}
	
	public: static void Initialize(int argc, wchar_t* argv[])
	{
		(void)(argc);
		(void)(argv);
		set_unexpected(unexpected_function);
		Stream::SetMode(Stream::StdIn, Stream::Binary);
		Stream::SetMode(Stream::StdOut, Stream::Binary);
	}
	
	public: static int Run(void)
	{
		while(true)
		{
			ErrorInfo rei;
			byte Buffer[MAX_MESSAGE_LENGTH] = { 0 };
			
			// Suspend While Read Buffer
			UInt16 size = Stream::Read2(Buffer, &rei); 
			
			// Port Closed
			if(!rei.WasError && !rei.ReturnValue && !rei.ErrorCode && !size) {
				Log("Port closed");
				break;
			}
			// An Error Occured!!!
			else if(rei.WasError || rei.ErrorCode || !size) {
				Log("An IO Runtime Error Occured While Read Stream");
				terminate();
			}
			
			// Read the Command Id and DS (Digital Sign)
			Erlang::ETFReader er(Buffer, size);
			unsigned tupleSize = er.ReadTuple();
			int command = er.ReadNumber<int>();
			Erlang::Reference ds = er.ReadReference();
			
			if(command == 1)
			{
				// {?CMD_COMMAND1,DS,"hi there !",'a.t.o.m',[],"",<<>>,"???"} // ??? = 11025,11206,10255 - unicode
				_ASSERTE(tupleSize == 8);
				ErrorInfo ei;
				long ret = 0;
				try
				{
					UInt8 tag = er.GetNextTag();
					void* p = (void*)er.ReadASCII();
					Log("Got ascii string from command 1 :");
					Log((char*)p);
					delete[] p;

					p = er.ReadAtom();
					Log("Got atom from command 1 :");
					Log((char*)p);
					delete[] p;

					er.ReadNil();
					Log("Got empty list from command 1");

					er.ReadNil();
					Log("Got empty string from command 1");

					Erlang::Binary emptyBinary = er.ReadBinary();
					Log("Got empty bynary, size :");
					Log(emptyBinary.Size() - 5);

					p = (void*)er.ReadUnicode();
					Log("Got unocode string from command 1 :");
					Log((int)(((wchar_t*)p)[0])); // 11025
					Log((int)(((wchar_t*)p)[1])); // 11206
					Log((int)(((wchar_t*)p)[0])); // 10255
					delete[] p;
				}
				catch(const std::exception& e)
				{
					Log("An Exception When Read Command 1");
					Log(e.what());
					// NOTE: Use er.ToVector<int>() to check bytes were read
					terminate();
				}
				Erlang::ETFWriter ewr; // {command1,1,DS,{0,"Unicode String"}}
				ewr.WriteTuple(4).
						WriteAtom("command1").
						WriteNumber(command).
						WriteReference(ds).
						WriteTuple(2).
							WriteNumber(ret).
							WriteString(L"Unicode String");
				Stream::Write2(ewr, (UInt16)ewr.BytesCount(), &ei);
				if(ei.WasError || ei.ErrorCode) {
					// NOTE: Use ewr.ToVector<int>() to check bytes were written
					Log("IO Error When Command 1");
					terminate();
				}
			}
			else if(command == 2)
			{
				// read {?CMD_PING,DS,[-1.23,<<"Чело"/utf8>>],9223372036854775807}
				_ASSERTE(tupleSize == 4);
				UInt32 listSize = er.ReadList();
				Int64 value = er.ReadNumber<Int64>();
				Log("Got value from command 2 :");
				Log(*((double*)&value));
				Erlang::Binary p = er.ReadBinary();
				std::wstring wstr((const wchar_t*)((const byte*)(p) + 5), (p.Size() - 5)/2);
				Log("Got utf8 binary from command 2 :");
				Log(wstr.c_str());
				er.ReadNil(); // read end of list
				Int64 bigValue = er.ReadNumber<Int64>();
				Log("Got big value from command 2 :");
				Log(bigValue);

				byte buf[] = {131,109,0,0,0,0};
				Erlang::ETFReader er(buf, sizeof(buf)/sizeof(buf[0]));
				Erlang::Binary emptyBinary = er.ReadBinary();

				ErrorInfo ei;
				Erlang::ETFWriter ewr; // {'pi.ng',[{value,"ASCII string","",<<>>,[]},-123.456],2,DS,pong}
				ewr.WriteTuple(5).
						WriteAtom("pi.ng").
						WriteList(2).
							WriteTuple(5).
								WriteAtom("value").
								WriteString("ASCII string").
								WriteString("").
								WriteBinary(emptyBinary).
								WriteList(0).
									WriteNil().
							WriteNumber(-123.456).
							WriteNil().
						WriteNumber(command).
						WriteReference(ds).
						WriteAtom("pong");
				Stream::Write2(ewr, (UInt16)ewr.BytesCount(), &ei);
				if(ei.WasError || ei.ErrorCode) {
					// NOTE: Use ewr.ToVector<int>() to check bytes were written
					Log("IO Error When Command 2");
					terminate();
				}
			}
			else if(command == 3)
			{
				Log("Close Command");
				break;
			}
			else
			{
				_ASSERTE(false);
				Log("Unknown Command Given");
				terminate();
			}
		} // while(true)
		
		return 0;
	}
	
	public: static int Run(int)
	{
		// Directly write Reference
		byte buf[] = {131,114,0,3,100,0,13,110,111,110,111,100,101,64,110,111,104,111,115,116,0,0,0,1,134,0,0,0,0,0,0,0,0};
		Erlang::ETFReader er(buf, sizeof(buf)/sizeof(buf[0]));
		Erlang::Reference r = er.ReadReference();
		return 0;
	}
};
	
int wmain(int argc, wchar_t* argv[])
{
	Application::Initialize(argc, argv);
	return Application::Run();
}
