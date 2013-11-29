Erlang.PortIO
=============

 
KEYWORDS

erlang, c++, port, terms, binary, read, write, parse


INTRODUCTION

Erlang.PortIO is a small C++ library to read\write Erlang terms from\to stdout\stdin via port.
For more info see http://www.erlang.org/documentation/doc-5.7.4/erts-5.7.4/doc/html/erl_ext_dist.html


DEPENDENCIES

Win7, MSVS2012, boost_1_55_0, R16B(erts-5.10.1)


SUPPLIED

Src - contains 3 files for read\write terms and parsing from\to raw binary. Supporting almost all base terms.
		Other terms (like fun, pid and etc if needed) can be transformed to binary using BIF term_to_binary() 
		and	binary_to_term().
		
Example/ErlPort - contains VS solution to create exe as port for Erlang client.

Example/ErlClient - contains Erlang source file as client to use port.


EXAMPLE

// Suspend While Read Buffer
ErrorInfo rei;
byte Buffer[MAX_MESSAGE_LENGTH] = { 0 };
UInt16 size = Stream::Read2(Buffer, &rei); 
// Read the Command Id and DS (Digital Sign)
Erlang::ETFReader er(Buffer, size);
unsigned tupleSize = er.ReadTuple();
int command = er.ReadNumber<int>();
Erlang::Reference ds = er.ReadReference();


HOW TO USE

Erlang Side
- open Erlang console and cd("Erlang.PortIO/example/ErlClient").
- c(client).
- client:start([]).
- client:ping().
- client:command1().
- client:close().

Observe which terms are sent to port and recived from.

C++ Side
- open ErlPort/ErlPort.sln
- setup boost src and lib directories
- build solution

Observe Log.txt file to check which data are received\send to port.


USE CASES

Half-duplex

Initial version of example (ErlPort and ErlClient) is half-duplex, i.e. client sends only one command and 
wait for a response, than client can send another command. All input commands stores in the queue before
going to the port.
I used this case for Directory Watcher. Port starts separate thread to watching directory and if content
is changed it sends info to Erlang side. Also there are a few commands to manage port. 

Full-duplex

For it there is a need a queue storage and separate thread. Once command comes to port main thread stores 
command in the storage and go to the idle, while second thread consumes command from top of the queue and
probably sends result back to client.

Internodes communication

It is possible to make two Erlang nodes with two ports. These two ports communicate together via 
interprocessing communications (Shared Memory, Pipes, Files and etc). As result one node can send a message
to another node via ports.

Master-Slave

Sometimes it is need to make port as Master but Erlang side as Slave. There is no problem to do it, just
make Erlang side as command listener.
