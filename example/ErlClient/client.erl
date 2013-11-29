
%
%
%

-module(client).
-behaviour(gen_server).

-export([start/1, ping/0, command1/0, close/0, stop/0]).
-export([init/1, handle_call/3, handle_cast/2, handle_info/2, terminate/2, code_change/3]).

-define(SERVER, ?MODULE).
-define(APP, "../ErlPort/Debug/ErlPort.exe").
-define(CMD_COMMAND1, 1).
-define(CMD_PING, 2).
-define(CMD_CLOSE, 3).

-record
(
	state,
	{
		ds,			% reference() - digital sign
		cmdq,		% queue:new() - command queue
		process_cmd,% bool() - if command is in progress
		port		% port() - external program port
	}
).


start(Args) ->
	gen_server:start({local,?SERVER},?MODULE,Args,[]).

command1() ->
	gen_server:call(?SERVER,command1).

ping() ->
	gen_server:call(?SERVER,ping).

close() ->
	gen_server:cast(?SERVER,close).

stop() ->
	gen_server:call(?SERVER,stop).

%%--------------------------------------------------------------------
init(_Args) ->
	process_flag(trap_exit,true),
	Port = open_port({spawn_executable,?APP},[binary,{packet,2},use_stdio,exit_status]),
	State = #state{ds=make_ref(),cmdq=queue:new(),process_cmd=false,port=Port},
	{ok,State}.

handle_call(ping,From,State) ->
	#state{cmdq=CmdQ} = State,
	CmdQ2 = queue:in({?CMD_PING,From},CmdQ),
	self() ! process_cmdq,
	{noreply,State#state{cmdq=CmdQ2}};

handle_call(command1,From,State) ->
	#state{cmdq=CmdQ} = State,
	CmdQ2 = queue:in({?CMD_COMMAND1,From},CmdQ),
	self() ! process_cmdq,
	{noreply,State#state{cmdq=CmdQ2}};

handle_call(stop,_From,State) ->
    {stop,shutdown,stopped,State}.

handle_cast(close,State) ->
	#state{cmdq=CmdQ} = State,
	CmdQ2 = queue:in({?CMD_CLOSE},CmdQ),
	self() ! process_cmdq,
	{noreply,State#state{cmdq=CmdQ2}};

handle_cast(_Msg,State) ->
	{noreply,State}.

handle_info(Info,State) ->
	#state{port=Port} = State,
	case Info of
		{'EXIT',Port,Reason} ->
			io:format("port crashed ~p~n",[Reason]),
			{stop,{port_crashed,Reason},State};
		process_cmdq ->
			State2 = process_cmdq(State),
			{noreply,State2};
		{Port,{exit_status,0}} ->
			io:format("port exited status OK ~n",[]),
			{stop,shutdown,State};
		{Port,{exit_status,S}} ->
			io:format("port exited status ~p~n",[S]),
			{stop,{port_closed,S},State};
		{Port,{data,Data}} ->
			process_port_data(State,Data);
		_ ->
			error_logger:error_msg("handle_info unknown message: ~w~n",[Info]),
			{noreply,State}
	end.

process_port_data(State,Data) ->
	#state{ds=DS,cmdq=CmdQ} = State,
	case (catch binary_to_term(Data,[safe])) of
		{command1,1,DS,{0,_UnicodeString}} = Answer ->
			io:format("Got answer from Port ~p~n",[Answer]),
			self() ! process_cmdq,
			{{value,{?CMD_COMMAND1,From}},CmdQ2} = queue:out(CmdQ),
			gen_server:reply(From,{port_answer,Answer}),
			{noreply,State#state{cmdq=CmdQ2,process_cmd=false}};
		{'pi.ng',[{value,"ASCII string","",<<>>,[]},_Float],2,DS,pong} = Answer ->
			io:format("Got answer from Port ~p~n",[Answer]),
			self() ! process_cmdq,
			{{value,{?CMD_PING,From}},CmdQ2} = queue:out(CmdQ),
			gen_server:reply(From,{port_answer,Answer}),
			{noreply,State#state{cmdq=CmdQ2,process_cmd=false}};
		U ->
			error_logger:error_msg("handle_info couldn't decode/process port data: ~w~n",[U]),
			{noreply, State}
	end.

terminate(shutdown,#state{port=Port}) ->
	catch port_close(Port),
	ok.

code_change(_OldVsn, State, _Extra) ->
    {ok, State}.

%%--------------------------------------------------------------------

process_cmdq(State) when State#state.process_cmd ->
	State;
process_cmdq(State) ->
	#state{cmdq=CmdQ,port=Port,ds=DS} = State,
	Value = case queue:is_empty(CmdQ) of
				true -> empty;
				false -> queue:get(CmdQ)
			end,
	case Value of
		empty ->
			State;
		{?CMD_COMMAND1,_From} ->
			Cmd = {?CMD_COMMAND1,DS,"hi there !",'a.t.o.m',[],"",<<>>,[11025,11206,10255]},
			io:format("Send to port ~p~n",[Cmd]),
			CmdBin = term_to_binary(Cmd,[{compressed,0}]),
			erlang:port_command(Port,CmdBin), % [nosuspend]
			State#state{process_cmd=true};
		{?CMD_PING,_From} ->
			Cmd = {?CMD_PING,DS,[-1.23,<<"Чело"/utf8>>],9223372036854775807},
			io:format("Send to port ~p~n",[Cmd]),
			CmdBin = term_to_binary(Cmd,[{minor_version,1}]),
			erlang:port_command(Port,CmdBin), % [nosuspend]
			State#state{process_cmd=true};
		{?CMD_CLOSE} ->
			Cmd = {?CMD_CLOSE,DS},
			io:format("Send to port ~p~n",[Cmd]),
			CmdBin = term_to_binary(Cmd,[{minor_version,1}]),
			erlang:port_command(Port,CmdBin), % [nosuspend]
			State#state{process_cmd=true}
	end.

