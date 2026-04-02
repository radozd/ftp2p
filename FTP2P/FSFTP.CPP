/*
	FTP2P - dual-pane FTP client for MS-DOS
	Copyright (C) 2026 Konstantin Bortnikov

	This file is part of FTP2P.

	FTP2P is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	FTP2P is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with FTP2P. If not, see <https://www.gnu.org/licenses/>.
*/

#include "packet.h"
#include "arp.h"
#include "udp.h"
#include "dns.h"
#include "tcp.h"
#include "tcpsockm.h"

#define _NL_ "\r\n"

#define CONTROL_RECV_SIZE     (512)
#define INBUFSIZE             (512)
#define DATA_RECV_SIZE        (8192)
#define RECV_FILE_BUF_SIZE    (8192)
#define SEND_FILE_BUF_SIZE    (8192)

#define SERVERFILE_MAX_LEN    (512)
#define LOCALFILE_MAX_LEN     (13)
#define SERVER_RESP_MAX_LEN   (160)
#define FTP_REPLY_TEXT_MAX    (1024)

#define UI_LINE_MAX           (256)
#define UI_REMOTE_CWD_MAX     (128)

enum ClientState_t
{
	Uninitialized = 0,
	ServerConnected,
	SentUser,
	UserOkSendPass,
	SentPass,
	CmdLine,

	ListSentPasv,
	ListSentAfterPasv,
	ListSentActive,
	ListSentPort,
	ListSentAfterPort,

	RetrSentPasv,
	RetrSentAfterPasv,
	RetrSentActive,
	RetrSentPort,
	RetrSentAfterPort,

	StorSentPasv,
	StorSentAfterPasv,
	StorSentActive,
	StorSentPort,
	StorSentAfterPort,

	CmdSent,
	Closing
};

enum TransferModes_t
{
	Classic,
	Passive,
	PortFirst
};

typedef struct
{
	TcpBuffer b;
	uint8_t data[1460];
} DataBuf;

typedef struct
{
	char serverFile[SERVERFILE_MAX_LEN];
	char localFile[LOCALFILE_MAX_LEN];
	char _padding;
	uint32_t totalBytes;
} FtpTransferContext;

typedef struct
{
	uint16_t code;
	bool multiline;
	char _padding;
	char lastLine[SERVER_RESP_MAX_LEN];
	char text[FTP_REPLY_TEXT_MAX];
} FtpReply;

typedef struct
{
	uint32_t command;
	uint32_t list;
	uint32_t transfer;
} FtpTimeoutProfile;

class FtpTransport
{
public:	
	static volatile bool _ctrlBreakDetected;

	FtpTransport();

	bool Init(StatusCallback statusCallback, UiList *remoteList);
	void SetTimeoutProfile(uint32_t commandTimeoutMs, uint32_t listTimeoutMs, uint32_t transferTimeoutMs);

	bool ConnectAndLogin(const char *host, uint16_t port, const char *user, const char *pass);
	void Quit();
	void Cleanup();

	void PollControlSocket(uint32_t timeout);
	bool IsControlClosed();
	
	bool SetBinaryType();

	bool ReadCwd();
	bool ListCwd();
	bool RefreshList();

	bool ChangeToParentDir();
	bool ChangeWorkingDir(const char *path);
	bool RemoveDir(const char *path);
	bool DeleteFile(const char *path);
	bool MakeDir(const char *path);
	bool RenameItem(const char *fromPath, const char *toPath);

	bool DoGetAndWait(const char *serverFile, const char *localFile, uint32_t totalBytes);
	bool DoPutAndWait(const char *localFile, const char *serverFile, uint32_t totalBytes);

private:
	FtpTransferContext _activeTransfer;
	char _ftpServer[FTPSERVERNAME_MAX_LEN];
	char _user[USERNAME_MAX_LEN];
	char _password[PASSWORD_MAX_LEN];
	IpAddr_t _ftpServerAddr;

	TcpSocket *_controlSocket;
	TcpSocket *_dataSocket;
	TcpSocket *_listenSocket;

	void (__interrupt __far *_oldCtrlBreakHandler)();
	void (__interrupt __far *_oldCtrlCHandler)();

	StatusCallback UiStatusCallback;
	UiList *_remoteList;

	ClientState_t _clientState;
	TransferModes_t _transferMode;
	char _lastReplyLine[SERVER_RESP_MAX_LEN];
	char _replyText[FTP_REPLY_TEXT_MAX];
	uint16_t _replyCodeInProgress;

	uint16_t _dataRecvSize;
	uint16_t _recvFileBufSize;
	uint16_t _sendFileBufSize;

	uint8_t *_fileBuffer;
	uint16_t _fileBufferSize;

	uint8_t *_inBuf;
	uint16_t _inBufIndex;
	uint16_t _inBufSearchIndex;

	uint16_t _controlPort;
	uint16_t _dataPort;
	uint16_t _ftpServerPort;
	uint16_t _nextDataPort;
	uint16_t _lastReplyCode;

	IpAddr_t _pasvAddr;
	uint16_t _pasvPort;

	uint32_t _connectTimeout;
	FtpTimeoutProfile _timeouts;
	
	bool _ctrlHandlersInstalled;
	bool _multilineResponse;
	bool _stackStarted;

	void CloseDataSocketsOnly();
	void DestroySockets();

	void SetStatus(char type, const char *fmt, ...);
	void SetTransferProgress(bool get, const char *name, uint32_t bytes, uint32_t totalBytes);
	void CheckAbortKey();

	bool WaitForReplyAndFinalize(uint32_t timeoutMs, FtpReply *replyOut);
	void ResetReplyState();
	void AppendReplyLine(const char *line);
	void FinalizeReply(FtpReply *replyOut);
	bool WaitForStateSet(uint32_t timeoutMs, uint32_t mask0, uint32_t mask1);

	bool SendAndWaitRawEx(const char *verb, const char *arg, FtpReply *replyOut, uint32_t timeoutMs);
	bool SendAndWaitRaw(const char *verb, const char *arg, uint32_t timeoutMs);
	bool RunStarterAndWaitEx(uint32_t timeoutMs, void (FtpTransport::*starter)(), FtpReply *replyOut);
	bool RunStarterAndWait(uint32_t timeoutMs, void (FtpTransport::*starter)());
	bool ExecutePasvCommandAndWait(ClientState_t nextState, FtpReply *replyOut, uint32_t timeoutMs);

	void ReadConfigParms();
	bool ResolveServer(char *name, IpAddr_t addr);
	bool QueryFeatures(uint32_t timeoutMs);
	bool WaitForLoginState(uint32_t timeoutMs, uint32_t states, const char *waitText, const char *timeoutText);
	bool WaitForGreeting(uint32_t timeoutMs);
	bool LoginWithCredentials(uint32_t timeoutMs);

	void SendCommandWithState(const char *verb, const char *arg, ClientState_t nextState);
	void SendPasvCommand();
	void SendPortCommand();
	void SendUserCommand(const char *username);
	void SendPassCommand(const char *password);
	void SendStorCommand(ClientState_t nextState);

	void DoGet();
	void DoPut();
	void StartTransfer(bool get);
	void PrepareTransferContext(const char *serverFile, const char *localFile, uint32_t totalBytes);
	void ClearTransferContext();
	uint16_t AllocNextEphemeralPort();

	bool ListenForDataSocket();
	bool WaitForDataSocket();
	bool ConnectDataSocket();
	void CloseAndFreeDataSocket();

	void ProcessSocketInput();
	int GetLineFromInBuf(char *target, int16_t targetLen);
	bool ParsePASVResponse(char *pos);
	bool HandlePreliminaryReply(ClientState_t replyState);
	void HandlePortReadyReply(ClientState_t replyState);
	void HandlePasvReply(ClientState_t replyState, char *pos);

	bool ReadFileList();
	bool ReceiveFile();
	bool SendFile();

	bool TryParseUnixListLine(char *line, char *nameOut, int nameOutSize, bool &isDir, uint32_t &sizeOut, char *modifyOut, int modifyOutSize);
	bool TryParseDosListLine(char *line, char *nameOut, int nameOutSize, bool &isDir, uint32_t &sizeOut, char *modifyOut, int modifyOutSize);
	void ParseListLine(char *line);
};


volatile bool FtpTransport::_ctrlBreakDetected = false;

void __interrupt __far ctrlBreakHandler()
{
	FtpTransport::_ctrlBreakDetected = true;
}


FtpTransport::FtpTransport()
{
	_clientState = Uninitialized;
	_transferMode = Passive;

	_controlSocket = NULL;
	_dataSocket = NULL;
	_listenSocket = NULL;

	_ftpServer[0] = '\0';
	_ftpServerPort = 0;
	_user[0] = '\0';
	_password[0] = '\0';

	memset(&_activeTransfer, 0, sizeof(FtpTransferContext));
	memset(&_ftpServerAddr, 0, sizeof(IpAddr_t));
	memset(&_pasvAddr, 0, sizeof(IpAddr_t));

	_oldCtrlBreakHandler = NULL;
	_oldCtrlCHandler = NULL;

	UiStatusCallback = NULL;
	_remoteList = NULL;

	_lastReplyCode = 0;
	_lastReplyLine[0] = '\0';
	_replyText[0] = '\0';
	_replyCodeInProgress = 0;
	_multilineResponse = false;
	_stackStarted = false;
	_ctrlHandlersInstalled = false;

	_inBuf = NULL;
	_inBufIndex = 0;
	_inBufSearchIndex = 0;

	_dataRecvSize = DATA_RECV_SIZE;
	_recvFileBufSize = RECV_FILE_BUF_SIZE;
	_sendFileBufSize = SEND_FILE_BUF_SIZE;
	_fileBuffer = NULL;
	_fileBufferSize = 0;

	_controlPort = 0;
	_dataPort = 0;
	_nextDataPort = 0;
	_pasvPort = 0;

	_connectTimeout = 10000ul;
	SetTimeoutProfile(15000ul, 45000ul, 1200000ul);
}

bool FtpTransport::Init(StatusCallback statusCallback, UiList *remoteList)
{
	UiStatusCallback = statusCallback;
	_remoteList = remoteList;

	_inBuf = (uint8_t *)malloc(INBUFSIZE);
	if( _inBuf == NULL )
	{
		printf("Not enough free memory\n");
		return false;
	}

	return true;
}

bool FtpTransport::ConnectAndLogin(const char *host, uint16_t port, const char *user, const char *pass)
{
	CopyTrunc(_ftpServer, sizeof(_ftpServer), host);
	_ftpServerPort = port;
	CopyTrunc(_user, sizeof(_user), user);
	CopyTrunc(_password, sizeof(_password), pass);

	if( Utils::parseEnv() != 0 )
	{
		printf("Could not parse mTCP environment\n");
		return false;
	}

	printf("mTCP environment ok\n");
	ReadConfigParms();

	_fileBufferSize = (_recvFileBufSize > _sendFileBufSize ? _recvFileBufSize : _sendFileBufSize) + 1;
	_fileBuffer = (uint8_t *)malloc(_fileBufferSize);
	if( _fileBuffer == NULL )
	{
		printf("Not enough free memory\n");
		return false;
	}

	if( Utils::initStack(3, 10, ctrlBreakHandler, ctrlBreakHandler) )
	{
		printf("Could not start TCP/IP\n");
		return false;
	}
	_stackStarted = true;

	_oldCtrlBreakHandler = getvect(0x1b);
	_oldCtrlCHandler = getvect(0x23);
	setvect(0x1b, ctrlBreakHandler);
	setvect(0x23, ctrlBreakHandler);
	_ctrlHandlersInstalled = true;

	printf("Resolving server %s:%u\n", _ftpServer, _ftpServerPort);
	if( !ResolveServer(_ftpServer, _ftpServerAddr) )
	{
		printf("Error resolving FTP address\n");
		return false;
	}

	_controlSocket = TcpSocketMgr::getSocket();
	_listenSocket = TcpSocketMgr::getSocket();
	_dataSocket = NULL;

	if( _controlSocket == NULL || _listenSocket == NULL )
	{
		printf("Could not allocate TCP sockets\n");
		return false;
	}

	int8_t rc = _controlSocket->setRecvBuffer(CONTROL_RECV_SIZE);
	if( rc )
	{
		printf("Could not allocate control receive buffer\n");
		return false;
	}

	srand((unsigned)time(NULL));
	_controlPort = 1024 + (rand() % 1024);
	_nextDataPort = 4096 + (rand() % 20480);

	printf("Opening control connection to %d.%d.%d.%d:%u with local port %u\n",
		_ftpServerAddr[0], _ftpServerAddr[1], _ftpServerAddr[2], _ftpServerAddr[3],
		_ftpServerPort, _controlPort);

	rc = _controlSocket->connect(_controlPort, _ftpServerAddr, _ftpServerPort, _connectTimeout);
	if( rc != 0 )
	{
		printf("Could not connect to FTP server\n");
		return false;
	}

	if( !WaitForGreeting(_timeouts.command) )
	{
		printf("Failed to wait for FTP greeting\n");
		return false;
	}

	if( !LoginWithCredentials(_timeouts.command) )
	{
		printf("Failed to login\n");
		return false;
	}

	printf("Login accepted\n");

	//QueryFeatures(_timeouts.command);

	if( !SetBinaryType() )
	{
		printf("Could not set binary transfer mode\n");
		return false;
	}

	_transferMode = Passive;
	return true;
}

void FtpTransport::Cleanup()
{
	DestroySockets();

	if( _ctrlHandlersInstalled )
	{
		if( _oldCtrlBreakHandler )
			setvect(0x1b, _oldCtrlBreakHandler);
		if( _oldCtrlCHandler )
			setvect(0x23, _oldCtrlCHandler);
		_ctrlHandlersInstalled = false;
	}

	if( _inBuf )
	{
		free(_inBuf);
		_inBuf = NULL;
	}
	if( _fileBuffer )
	{
		free(_fileBuffer);
		_fileBuffer = NULL;
	}

	if( _stackStarted )
	{
		Utils::endStack();
		_stackStarted = false;
	}
}

void FtpTransport::CloseDataSocketsOnly()
{
	if( _listenSocket != NULL )
		_listenSocket->close();

	if( _dataSocket != NULL )
		CloseAndFreeDataSocket();

	while( _listenSocket != NULL )
	{
		TcpSocket *pendingSocket = TcpSocketMgr::accept();
		if( pendingSocket == NULL )
			break;

		SetStatus('W', "Cleaning up socket that was not accepted.");
		pendingSocket->close();
		TcpSocketMgr::freeSocket(pendingSocket);
	}
}

void FtpTransport::DestroySockets()
{
	CloseDataSocketsOnly();

	if( _listenSocket != NULL )
	{
		_listenSocket->close();
		TcpSocketMgr::freeSocket(_listenSocket);
		_listenSocket = NULL;
	}

	if( _controlSocket != NULL )
	{
		_controlSocket->close();
		TcpSocketMgr::freeSocket(_controlSocket);
		_controlSocket = NULL;
	}
}

void FtpTransport::PollControlSocket(uint32_t timeout)
{
	if( _controlSocket == NULL || _inBuf == NULL )
		return;

	clockTicks_t start = TIMER_GET_CURRENT();

	while( 1 )
	{
		PACKET_PROCESS_SINGLE;
		Arp::driveArp();
		Tcp::drivePackets();

		int rc = _controlSocket->recv(_inBuf + _inBufIndex, (INBUFSIZE - _inBufIndex));
		if( rc > -1 )
			_inBufIndex += rc;

		ProcessSocketInput();

		if( Timer_diff(start, TIMER_GET_CURRENT()) * TIMER_TICK_LEN > timeout )
			break;
	}
}

bool FtpTransport::IsControlClosed()
{
	return _controlSocket && _controlSocket->isRemoteClosed();
}

void FtpTransport::Quit()
{
	SendCommandWithState("QUIT", NULL, CmdSent);
}

void FtpTransport::SetTimeoutProfile(uint32_t commandTimeoutMs, uint32_t listTimeoutMs, uint32_t transferTimeoutMs)
{
	_timeouts.command = commandTimeoutMs;
	_timeouts.list = listTimeoutMs;
	_timeouts.transfer = transferTimeoutMs;
}

void FtpTransport::SetStatus(char type, const char *fmt, ...)
{
	if( !UiStatusCallback )
		return;

	char tmp[1024];
	va_list ap;

	va_start(ap, fmt);
	SprintfTruncV(tmp, sizeof(tmp), fmt, ap);
	va_end(ap);

	UiStatusCallback(type, tmp);
}

void FtpTransport::SetTransferProgress(bool get, const char *name, uint32_t bytes, uint32_t totalBytes)
{
	char shown[32];
	CropLeft(shown, sizeof(shown), name ? name : "");

	char sizebuf[32];
	FormatSize(sizebuf, sizeof(sizebuf), bytes);

	if( totalBytes == 0 )
	{
		SetStatus('T', "%s %s : %s", get ? "GET":"PUT", shown, sizebuf);
		return;
	}

	char totalbuf[32];
	FormatSize(totalbuf, sizeof(totalbuf), totalBytes);

	uint32_t percent;
	if( bytes >= totalBytes )
		percent = 100u;
	else if( totalBytes >= 100u )
		percent = bytes / (totalBytes / 100u);
	else
		percent = (bytes * 100u) / totalBytes;
	
	if( percent > 100u )
		percent = 100u;

	SetStatus('T', "%s %s : %s/%s %3lu%%", get ? "GET":"PUT", shown, sizebuf, totalbuf, percent);
}

void FtpTransport::CheckAbortKey()
{
	if( _ctrlBreakDetected )
		return;

	if( biosIsKeyReady() )
	{
		uint16_t key = biosKeyRead();
		if( key == 27 || key == 0x011B || key == 0x0100 )
			_ctrlBreakDetected = true;
	}
}

bool FtpTransport::SetBinaryType()
{
	return SendAndWaitRaw("TYPE", "I", _timeouts.command);
}

bool FtpTransport::ReadCwd()
{
	FtpReply reply;
	if( !SendAndWaitRawEx("PWD", NULL, &reply, _timeouts.command) )
		return false;
	if( reply.code != 257 )
		return false;

	char tmp[SERVER_RESP_MAX_LEN];
	CopyTrunc(tmp, sizeof(tmp), reply.lastLine);
	char *p1 = strchr(tmp, '"');
	if( p1 == NULL )
		return false;
	char *p2 = strchr(p1 + 1, '"');
	if( p2 == NULL )
		return false;

	*p2 = '\0';
	_remoteList->SetCwd(p1 + 1);
	return true;
}

bool FtpTransport::ListCwd()
{
	FtpReply reply;
	if( !ExecutePasvCommandAndWait(ListSentPasv, &reply, _timeouts.list) )
		return false;
	return reply.code < 400u;
}

bool FtpTransport::RefreshList()
{
	if( !ReadCwd() )
		return false;
	return ListCwd();
}

bool FtpTransport::ChangeToParentDir()
{
	return SendAndWaitRaw("CDUP", NULL, _timeouts.command);
}

bool FtpTransport::ChangeWorkingDir(const char *path)
{
	return SendAndWaitRaw("CWD", path, _timeouts.command);
}

bool FtpTransport::RemoveDir(const char *path)
{
	return SendAndWaitRaw("RMD", path, _timeouts.command);
}

bool FtpTransport::DeleteFile(const char *path)
{
	return SendAndWaitRaw("DELE", path, _timeouts.command);
}

bool FtpTransport::MakeDir(const char *path)
{
	return SendAndWaitRaw("MKD", path, _timeouts.command);
}

bool FtpTransport::RenameItem(const char *fromPath, const char *toPath)
{
	FtpReply reply;

	if( !SendAndWaitRawEx("RNFR", fromPath, &reply, _timeouts.command) )
		return false;
	if( reply.code != 350u )
		return false;

	if( !SendAndWaitRawEx("RNTO", toPath, &reply, _timeouts.command) )
		return false;

	return reply.code < 400u;
}

bool FtpTransport::DoGetAndWait(const char *serverFile, const char *localFile, uint32_t totalBytes)
{
	PrepareTransferContext(serverFile, localFile, totalBytes);
	bool rc = RunStarterAndWait(_timeouts.transfer, &FtpTransport::DoGet);
	if( !rc )
		ClearTransferContext();
	return rc;
}

bool FtpTransport::DoPutAndWait(const char *localFile, const char *serverFile, uint32_t totalBytes)
{
	PrepareTransferContext(serverFile, localFile, totalBytes);
	bool rc = RunStarterAndWait(_timeouts.transfer, &FtpTransport::DoPut);
	if( !rc )
		ClearTransferContext();
	return rc;
}

bool FtpTransport::WaitForReplyAndFinalize(uint32_t timeoutMs, FtpReply *replyOut)
{
	if( !WaitForStateSet(timeoutMs, (1ul << CmdLine), 0ul) )
		return false;

	FinalizeReply(replyOut);
	return true;
}

void FtpTransport::ResetReplyState()
{
	_lastReplyCode = 0;
	_lastReplyLine[0] = '\0';
	_replyText[0] = '\0';
	_replyCodeInProgress = 0;
	_multilineResponse = false;
}

void FtpTransport::AppendReplyLine(const char *line)
{
	if( line == NULL )
		return;

	if( _replyText[0] != '\0' )
		strncat(_replyText, "\n", sizeof(_replyText) - 1u - strlen(_replyText));

	strncat(_replyText, line, sizeof(_replyText) - 1u - strlen(_replyText));
	_replyText[sizeof(_replyText) - 1u] = '\0';
}

void FtpTransport::FinalizeReply(FtpReply *replyOut)
{
	if( replyOut == NULL )
		return;

	replyOut->code = _lastReplyCode;
	replyOut->multiline = (strchr(_replyText, '\n') != NULL);
	CopyTrunc(replyOut->lastLine, sizeof(replyOut->lastLine), _lastReplyLine);
	CopyTrunc(replyOut->text, sizeof(replyOut->text), _replyText);
}

bool FtpTransport::WaitForStateSet(uint32_t timeoutMs, uint32_t mask0, uint32_t mask1)
{
	clockTicks_t start = TIMER_GET_CURRENT();

	while( 1 )
	{
		CheckAbortKey();
		PollControlSocket(50);
		if( IsControlClosed() )
			return false;

		if( _clientState < 32 )
		{
			uint32_t bit = 1ul << (unsigned)_clientState;
			if( mask0 & bit )
				return true;
		}
		else
		{
			uint32_t bit2 = 1ul << ((unsigned)_clientState - 32u);
			if( mask1 & bit2 )
				return true;
		}

		if( Timer_diff(start, TIMER_GET_CURRENT()) * TIMER_TICK_LEN > timeoutMs )
		{
			SetStatus('W', "Timeout");
			return false;
		}
		if( _ctrlBreakDetected )
		{
			SetStatus('W', "Abort");
			return false;
		}
	}
}

bool FtpTransport::SendAndWaitRawEx(const char *verb, const char *arg, FtpReply *replyOut, uint32_t timeoutMs)
{
	_ctrlBreakDetected = false;
	ResetReplyState();
	SendCommandWithState(verb, arg, CmdSent);
	return WaitForReplyAndFinalize(timeoutMs, replyOut);
}

bool FtpTransport::SendAndWaitRaw(const char *verb, const char *arg, uint32_t timeoutMs)
{
	FtpReply reply;
	if( !SendAndWaitRawEx(verb, arg, &reply, timeoutMs) )
		return false;
	return reply.code < 400u;
}

bool FtpTransport::RunStarterAndWaitEx(uint32_t timeoutMs, void (FtpTransport::*starter)(), FtpReply *replyOut)
{
	_ctrlBreakDetected = false;
	ResetReplyState();
	(this->*starter)();
	return WaitForReplyAndFinalize(timeoutMs, replyOut);
}

bool FtpTransport::RunStarterAndWait(uint32_t timeoutMs, void (FtpTransport::*starter)())
{
	FtpReply reply;
	if( !RunStarterAndWaitEx(timeoutMs, starter, &reply) )
		return false;
	return reply.code < 400u;
}

bool FtpTransport::ExecutePasvCommandAndWait(ClientState_t nextState, FtpReply *replyOut, uint32_t timeoutMs)
{
	_ctrlBreakDetected = false;
	ResetReplyState();
	SendPasvCommand();
	_clientState = nextState;
	return WaitForReplyAndFinalize(timeoutMs, replyOut);
}

void FtpTransport::ReadConfigParms()
{
	char tmp[10];
	int val;

	Utils::openCfgFile();

	if( Utils::getAppValue("FTP_CONNECT_TIMEOUT", tmp, 10) == 0 &&
		((val = atoi(tmp)) != 0) )
	{
		_connectTimeout = val;
		_connectTimeout = _connectTimeout * 1000ul;
	}

	if( Utils::getAppValue("FTP_TCP_BUFFER", tmp, 10) == 0 &&
		((val = atoi(tmp)) >= 512) && (val <= 16384) )
	{
		_dataRecvSize = val;
	}

	if( Utils::getAppValue("FTP_FILE_BUFFER", tmp, 10) == 0 &&
		((val = atoi(tmp)) >= 512) && (val <= 32768) )
	{
		_recvFileBufSize = val;
	}

	if( Utils::getAppValue("FTP_SEND_FILE_BUFFER", tmp, 10) == 0 &&
		((val = atoi(tmp)) >= 512) && (val <= 16384) )
	{
		_sendFileBufSize = val;
	}

	Utils::closeCfgFile();
}

bool FtpTransport::ResolveServer(char *name, IpAddr_t addr)
{
	int8_t rc = Dns::resolve(name, addr, 1);
	if( rc < 0 )
		return false;

	while( 1 )
	{
		if( !Dns::isQueryPending() || _ctrlBreakDetected )
			break;
		PACKET_PROCESS_SINGLE;
		Arp::driveArp();
		Dns::drivePendingQuery();
	}

	rc = Dns::resolve(name, addr, 0);
	if( rc != 0 )
		return false;

	return true;
}

static bool FtpPromptForConsolePassword(char *buf, size_t bufSize)
{
	if( buf == NULL || bufSize == 0 )
		return false;

	printf("Password: ");
	fflush(stdout);

	if( fgets(buf, (int)bufSize, stdin) == NULL )
	{
		buf[0] = '\0';
		return false;
	}

	TrimRight(buf);
	return buf[0] != '\0';
}

bool FtpTransport::QueryFeatures(uint32_t timeoutMs)
{
	FtpReply reply;
	if( !SendAndWaitRawEx("FEAT", NULL, &reply, timeoutMs) )
		return false;
	
	printf("%s\n", reply.text);

	return (reply.code == 211) || (reply.code == 500) || (reply.code == 502) || (reply.code == 530);
}

bool FtpTransport::WaitForLoginState(uint32_t timeoutMs, uint32_t states, const char *waitText, const char *timeoutText)
{
	if( waitText && waitText[0] )
		printf(waitText);

	if( WaitForStateSet(timeoutMs, states, 0ul) )
		return true;

	if( timeoutText && timeoutText[0] )
		printf(timeoutText);

	return false;
}

bool FtpTransport::WaitForGreeting(uint32_t timeoutMs)
{
	if( !WaitForLoginState(timeoutMs,
			(1ul << ServerConnected) | (1ul << CmdLine) | (1ul << UserOkSendPass) | (1ul << Closing),
			NULL,
			NULL) )
	{
		printf("Greeting wait timeout, last reply code=%u line=%s\n", _lastReplyCode, _lastReplyLine);
		return false;
	}

	if( _clientState == Closing )
	{
		printf("Server closed connection\n");
		return false;
	}

	return true;
}

bool FtpTransport::LoginWithCredentials(uint32_t timeoutMs)
{
	char prompt[PASSWORD_MAX_LEN];
	const char *user = _user;
	const char *password = _password;

	if( _clientState == CmdLine )
		return true;

	if( user[0] == '\0' )
	{
		printf("No user specified, trying anonymous login\n");
		user = "anonymous";
		password = "ftp2p@anonymous";
	}

	printf("Sending USER %s\n", user);
	SendUserCommand(user);

	if( !WaitForLoginState(timeoutMs,
			(1ul << CmdLine) | (1ul << UserOkSendPass) | (1ul << Closing),
			NULL,
			"Timeout waiting for USER response\n") )
	{
		return false;
	}

	if( _clientState == Closing )
	{
		printf("Server closed after USER\n");
		return false;
	}

	if( _lastReplyCode == 230 )
		return true;

	if( _lastReplyCode != 331 )
	{
		printf("Login failed, server replied %u\n", _lastReplyCode);
		return false;
	}

	if( _user[0] != '\0' && password[0] == '\0' )
	{
		if( !FtpPromptForConsolePassword(prompt, sizeof(prompt)) )
		{
			printf("Server requires password; none provided\n");
			return false;
		}

		password = prompt;
	}

	if( password[0] == '\0' )
	{
		printf("Server requires password; none provided\n");
		return false;
	}

	printf("Sending PASS ******\n");
	SendPassCommand(password);

	if( !WaitForLoginState(timeoutMs,
			(1ul << CmdLine) | (1ul << Closing),
			NULL,
			"Timeout waiting for PASS response\n") )
		return false;

	if( _lastReplyCode != 230 )
	{
		if( _user[0] == '\0' )
			printf("Anonymous login failed, server replied %u\n", _lastReplyCode);
		else
			printf("Login failed, server replied %u\n", _lastReplyCode);
		return false;
	}

	return true;
}

void FtpTransport::SendCommandWithState(const char *verb, const char *arg, ClientState_t nextState)
{
	char out[SERVERFILE_MAX_LEN + 16];

	if( _controlSocket == NULL )
	{
		_lastReplyCode = 421;
		CopyTrunc(_lastReplyLine, sizeof(_lastReplyLine), "Control socket is closed");
		_clientState = CmdLine;
		return;
	}

	if( arg && *arg != '\0' )
	{
		if( strlen(verb) + 1u + strlen(arg) + 2u >= sizeof(out) )
		{
			SetStatus('E', "FTP command too long: %s", verb);
			_lastReplyCode = 553;
			CopyTrunc(_lastReplyLine, sizeof(_lastReplyLine), "FTP command too long");
			_clientState = CmdLine;
			return;
		}
		SprintfTrunc(out, sizeof(out), "%s %s" _NL_, verb, arg);
	}
	else
	{
		SprintfTrunc(out, sizeof(out), "%s" _NL_, verb);
	}

	_controlSocket->send((uint8_t *)out, strlen(out));
	_clientState = nextState;
}

void FtpTransport::SendPasvCommand()
{
	_controlSocket->send((uint8_t *)"PASV" _NL_, 6);
}

void FtpTransport::SendPortCommand()
{
	_dataPort = AllocNextEphemeralPort();

	char buf[50];
	int bytes = sprintf(buf, "PORT %d,%d,%d,%d,%d,%d" _NL_, MyIpAddr[0], MyIpAddr[1], MyIpAddr[2], MyIpAddr[3], _dataPort / 256, _dataPort % 256);

	_controlSocket->send((uint8_t *)buf, bytes);
	ListenForDataSocket();
}

void FtpTransport::SendUserCommand(const char *username)
{
	if( username == NULL || *username == '\0' )
		printf("You need to enter a username\n");
	else
		SendCommandWithState("USER", username, SentUser);
}

void FtpTransport::SendPassCommand(const char *password)
{
	if( password == NULL || *password == '\0' )
		printf("You need to enter a password\n");
	else
		SendCommandWithState("PASS", password, SentPass);
}

void FtpTransport::SendStorCommand(ClientState_t nextState)
{
	const char *filename = _activeTransfer.localFile;

	if( *_activeTransfer.serverFile != 0 )
		filename = _activeTransfer.serverFile;

	SendCommandWithState("STOR", filename, nextState);
}

void FtpTransport::DoGet()
{
	StartTransfer(true);
}

void FtpTransport::DoPut()
{
	StartTransfer(false);
}

void FtpTransport::StartTransfer(bool get)
{
	if( _transferMode == Passive )
	{
		SendPasvCommand();
		_clientState = get ? RetrSentPasv : StorSentPasv;
	}
	else if( _transferMode == PortFirst )
	{
		SendPortCommand();
		_clientState = get ? RetrSentPort : StorSentPort;
	}
	else
	{
		_dataPort = AllocNextEphemeralPort();
		ListenForDataSocket();
		if( get )
			SendCommandWithState("RETR", _activeTransfer.serverFile, RetrSentActive);
		else
			SendStorCommand(StorSentActive);
	}
}

void FtpTransport::PrepareTransferContext(const char *serverFile, const char *localFile, uint32_t totalBytes)
{
	if( serverFile != NULL )
		CopyTrunc(_activeTransfer.serverFile, sizeof(_activeTransfer.serverFile), serverFile);
	else
		_activeTransfer.serverFile[0] = '\0';

	if( localFile != NULL )
		CopyTrunc(_activeTransfer.localFile, sizeof(_activeTransfer.localFile), localFile);
	else
		_activeTransfer.localFile[0] = '\0';

	_activeTransfer.totalBytes = totalBytes;
}

void FtpTransport::ClearTransferContext()
{
	_activeTransfer.serverFile[0] = '\0';
	_activeTransfer.localFile[0] = '\0';
	_activeTransfer.totalBytes = 0;
}

uint16_t FtpTransport::AllocNextEphemeralPort()
{
	uint16_t port = _nextDataPort;
	_nextDataPort = (((_nextDataPort - 4096) + 1) % 20480) + 4096;
	return port;
}

bool FtpTransport::ListenForDataSocket()
{
	_listenSocket->close();
	_listenSocket->reinit();

	int8_t rc = _listenSocket->listen(_dataPort, _dataRecvSize);
	if( rc )
	{
		SetStatus('E', "Error opening listening socket for incoming data (%d)", rc);
		return false;
	}
	return true;
}

bool FtpTransport::WaitForDataSocket()
{
	clockTicks_t start = TIMER_GET_CURRENT();

	while( 1 )
	{
		CheckAbortKey();
		if( _ctrlBreakDetected )
			return false;

		PACKET_PROCESS_SINGLE;
		Arp::driveArp();
		Tcp::drivePackets();

		_dataSocket = TcpSocketMgr::accept();
		if( _dataSocket != NULL )
		{
			_listenSocket->close();
			break;
		}

		if( Timer_diff(start, TIMER_GET_CURRENT()) * TIMER_TICK_LEN > _connectTimeout )
		{
			SetStatus('E', "Timeout waiting for incoming data connection");
			return false;
		}
	}
	return true;
}

bool FtpTransport::ConnectDataSocket()
{
	_dataSocket = TcpSocketMgr::getSocket();
	if( _dataSocket == NULL )
	{
		ClearTransferContext();
		return false;
	}

	int8_t rc = _dataSocket->setRecvBuffer(_dataRecvSize);
	if( rc )
	{
		CloseAndFreeDataSocket();
		return false;
	}

	uint16_t port = AllocNextEphemeralPort();

	rc = _dataSocket->connect(port, _pasvAddr, _pasvPort, _connectTimeout);
	if( rc != 0 )
	{
		CloseAndFreeDataSocket();
		return false;
	}

	return true;
}

void FtpTransport::CloseAndFreeDataSocket()
{
	if( _dataSocket != NULL )
	{
		_dataSocket->close();
		TcpSocketMgr::freeSocket(_dataSocket);
		_dataSocket = NULL;
	}
}

bool FtpTransport::HandlePreliminaryReply(ClientState_t replyState)
{
	switch( replyState )
	{
	case RetrSentAfterPasv:
		return ReceiveFile();
	case RetrSentActive:
	case RetrSentAfterPort:
		if( !WaitForDataSocket() )
			return false;
		return ReceiveFile();

	case StorSentAfterPasv:
		return SendFile();
	case StorSentActive:
	case StorSentAfterPort:
		if( !WaitForDataSocket() )
			return false;
		return SendFile();

	case ListSentAfterPasv:
		return ReadFileList();
	case ListSentActive:
	case ListSentAfterPort:
		if( !WaitForDataSocket() )
			return false;
		return ReadFileList();

	default:
		return true;
	}
}

void FtpTransport::HandlePortReadyReply(ClientState_t replyState)
{
	switch( replyState )
	{
	case ListSentPort:
		SendCommandWithState("LIST", NULL, ListSentAfterPort);
		break;
	case RetrSentPort:
		SendCommandWithState("RETR", _activeTransfer.serverFile, RetrSentAfterPort);
		break;
	case StorSentPort:
		SendStorCommand(StorSentAfterPort);
		break;
	default:
		_clientState = CmdLine;
		break;
	}
}

void FtpTransport::HandlePasvReply(ClientState_t replyState, char *pos)
{
	if( !ParsePASVResponse(pos) )
	{
		SetStatus('E', "Unable to parse PASV response");
		_clientState = CmdLine;
		return;
	}

	if( replyState == ListSentPasv || replyState == RetrSentPasv || replyState == StorSentPasv )
	{
		if( !ConnectDataSocket() )
		{
			SetStatus('E', "Error connecting data socket");
			_clientState = CmdLine;
			return;
		}
	}

	switch( replyState )
	{
	case ListSentPasv:
		SendCommandWithState("LIST", NULL, ListSentAfterPasv);
		break;
	case RetrSentPasv:
		SendCommandWithState("RETR", _activeTransfer.serverFile, RetrSentAfterPasv);
		break;
	case StorSentPasv:
		SendStorCommand(StorSentAfterPasv);
		break;
	default:
		_clientState = CmdLine;
		break;
	}
}

void FtpTransport::ProcessSocketInput()
{
	char buf[SERVER_RESP_MAX_LEN];

	if( _inBufIndex == 0 )
		return;

	int rc = GetLineFromInBuf(buf, SERVER_RESP_MAX_LEN);
	if( rc == 0 )
		return;
	if( rc < 0 )
		SetStatus('W', "Response overflowed the buffer");

	CopyTrunc(_lastReplyLine, sizeof(_lastReplyLine), buf);
	AppendReplyLine(buf);

	if( strlen(buf) < 3 )
		return;

	if( !(isdigit((unsigned char)buf[0]) && isdigit((unsigned char)buf[1]) && isdigit((unsigned char)buf[2])) )
		return;

	uint16_t code = (uint16_t)((buf[0] - '0') * 100 + (buf[1] - '0') * 10 + (buf[2] - '0'));
	ClientState_t reply_state = _clientState;

	if( (strlen(buf) > 3) && (buf[3] == '-') && !_multilineResponse )
	{
		_multilineResponse = true;
		_replyCodeInProgress = code;
		return;
	}

	if( _multilineResponse )
	{
		if( (strlen(buf) > 3) && (buf[3] == ' ') && (code == _replyCodeInProgress) )
		{
			_multilineResponse = false;
			_lastReplyCode = code;
		}
		else
		{
			return;
		}
	}
	else
	{
		_lastReplyCode = code;
	}

	char token[30];
	char *pos = Utils::getNextToken(buf, token, 30);

	switch( _lastReplyCode )
	{
	case 110:
	case 120:
		break;

	case 125:
	case 150:
		if( !HandlePreliminaryReply(reply_state) )
		{
			_clientState = CmdLine;
			CloseDataSocketsOnly();
		}
		break;

	case 200:
		HandlePortReadyReply(reply_state);
		break;

	case 202:
	case 211:
	case 212:
	case 213:
	case 214:
	case 215:
		_clientState = CmdLine;
		break;

	case 220:
		_clientState = ServerConnected;
		break;

	case 221:
		_clientState = Closing;
		break;

	case 225:
	case 226:
		_clientState = CmdLine;
		break;

	case 227:
		HandlePasvReply(reply_state, pos);
			break;

	case 230:
	case 250:
	case 257:
		_clientState = CmdLine;
		break;

	case 331:
		_clientState = UserOkSendPass;
		break;

	case 332:
	case 350:
		_clientState = CmdLine;
		break;

	case 421:
		_clientState = Closing;
		break;

	case 425:
	case 426:
	case 450:
	case 451:
	case 452:
	case 500:
	case 501:
	case 502:
	case 503:
	case 504:
	case 530:
	case 550:
	case 551:
	case 552:
	case 553:
		_clientState = CmdLine;
		break;

	default:
		SetStatus('W', "Unrecognized response from server");
		break;
	}

	if( _clientState == CmdLine )
		CloseDataSocketsOnly();
}

int FtpTransport::GetLineFromInBuf(char *target, int16_t targetLen)
{
	int i;
	for( i = _inBufSearchIndex; (i < (_inBufIndex - 1)) && (i < (targetLen - 1)); i++ )
	{
		if( _inBuf[i] == '\r' && _inBuf[i + 1] == '\n' )
		{
			memcpy(target, _inBuf, i);
			target[i] = '\0';

			memmove(_inBuf, _inBuf + i + 2, (_inBufIndex - (i + 2)));
			_inBufIndex = _inBufIndex - (i + 2);
			_inBufSearchIndex = 0;
			return 1;
		}
	}

	if( i < targetLen - 1 )
	{
		_inBufSearchIndex = i;
		return 0;
	}

	CopyDataTrunc(target, (size_t)targetLen, (const char *)_inBuf, (size_t)(targetLen - 1));

	memmove(_inBuf, _inBuf + (targetLen - 1), (_inBufIndex - (targetLen - 1)));
	_inBufIndex = _inBufIndex - (targetLen - 1);
	_inBufSearchIndex = 0;
	return -1;
}

bool FtpTransport::ParsePASVResponse(char *pos)
{
	uint8_t found = 0;
	while( *pos )
	{
		if( isdigit((unsigned char)*pos) )
		{
			found = 1;
			break;
		}
		pos++;
	}

	if( !found )
	{
		_pasvAddr[0] = _pasvAddr[1] = _pasvAddr[2] = _pasvAddr[3] = 0;
		_pasvPort = 0;
		return false;
	}

	uint16_t t0, t1, t2, t3, t4, t5;
	int rc = sscanf(pos, "%d,%d,%d,%d,%d,%d", &t0, &t1, &t2, &t3, &t4, &t5);
	if( rc != 6 )
	{
		_pasvAddr[0] = _pasvAddr[1] = _pasvAddr[2] = _pasvAddr[3] = 0;
		_pasvPort = 0;
		return false;
	}

	if( (t0 != _ftpServerAddr[0]) || (t1 != _ftpServerAddr[1]) || (t2 != _ftpServerAddr[2]) || (t3 != _ftpServerAddr[3]) )
		SetStatus('W', "Found a third party address on the PASV response. Ignoring it.");

	Ip::copy(_pasvAddr, _ftpServerAddr);
	_pasvPort = (t4 << 8) + t5;

	return true;
}

bool FtpTransport::ReadFileList()
{
	uint8_t done = 0;
	int bytes_read;
	char line[UI_LINE_MAX];
	unsigned line_len = 0;

	if( _dataSocket == NULL )
	{
		ClearTransferContext();
		return false;
	}

	_remoteList->Clear();
	_remoteList->SetFreeBytes(-1);
	_remoteList->Add("..", true, true, 0, "");

	while( !done )
	{
		CheckAbortKey();
		if( _ctrlBreakDetected )
		{
			done = 4;
			break;
		}

		PACKET_PROCESS_SINGLE;
		Arp::driveArp();
		Tcp::drivePackets();

		if( _dataSocket->isRemoteClosed() )
			done = 1;

		while( (bytes_read = _dataSocket->recv(_fileBuffer, _fileBufferSize - 1)) != 0 )
		{
			if( bytes_read < 0 )
			{
				done = 2;
				break;
			}

			for( int i = 0; i < bytes_read; i++ )
			{
				char ch = (char)_fileBuffer[i];
				if( ch == '\r' )
					continue;

				if( ch == '\n' )
				{
					line[line_len] = '\0';
					if( line_len > 0 )
						ParseListLine(line);
					line_len = 0;
					continue;
				}

				if( line_len < sizeof(line) - 1 )
					line[line_len++] = ch;
			}
		}
	}

	if( line_len > 0 )
	{
		line[line_len] = '\0';
		ParseListLine(line);
	}

	_remoteList->UpdateEncoding();

	CloseAndFreeDataSocket();

	switch( done )
	{
	case 2: SetStatus('E', "Listing aborted due to socket error"); break;
	case 4: SetStatus('W', "Listing aborted with Ctrl-Break"); break;
	}

	return done == 1;
}

static uint32_t ComputeRate(uint32_t bytes, uint32_t elapsed)
{
	uint32_t rate;

	if( elapsed == 0 )
		elapsed = 55;

	if( bytes < 2000000ul )
		rate = (bytes * 1000) / elapsed;
	else if( bytes < 20000000ul )
		rate = (bytes * 100) / (elapsed / 10);
	else if( bytes < 200000000ul )
		rate = (bytes * 10) / (elapsed / 100);
	else
		rate = bytes / (elapsed / 1000);

	return rate;
}

bool FtpTransport::ReceiveFile()
{
	char *target_filename = _activeTransfer.localFile;
	uint32_t total_received = 0;
	uint16_t bytes_read = 0;
	uint16_t bytes_to_read = _recvFileBufSize;
	clockTicks_t last_tick = 0;
	uint32_t last_bytes = 0;
	int rc;

	if( _dataSocket == NULL )
	{
		ClearTransferContext();
		return false;
	}
	if( *target_filename == '\0' )
		target_filename = _activeTransfer.serverFile;

	FILE *target_file = fopen(target_filename, "wb");
	if( target_file == NULL )
	{
		SetStatus('E', "Error opening file %s for writing", target_filename);
		CloseAndFreeDataSocket();
		ClearTransferContext();
		return false;
	}

	clockTicks_t start = TIMER_GET_CURRENT();

	int done = 0;
	while( !done )
	{
		CheckAbortKey();
		if( _ctrlBreakDetected )
		{
			done = 4;
			break;
		}

		PACKET_PROCESS_SINGLE;
		Arp::driveArp();
		Tcp::drivePackets();

		if( _dataSocket->isRemoteClosed() )
			done = 1;

		int16_t recv_rc;
		while( (recv_rc = _dataSocket->recv(_fileBuffer + bytes_read, bytes_to_read)) != 0 )
		{
			if( recv_rc < 0 )
			{
				done = 2;
				break;
			}

			if( recv_rc > 0 )
			{
				total_received += recv_rc;
				bytes_read += recv_rc;
				bytes_to_read -= recv_rc;

				if( bytes_to_read == 0 )
				{
					rc = fwrite(_fileBuffer, bytes_read, 1, target_file);
					if( rc != 1 )
						done = 3;

					bytes_to_read = _recvFileBufSize;
					bytes_read = 0;

					if( ShouldUpdateProgress(last_tick, last_bytes, total_received) )
						SetTransferProgress(true, target_filename, total_received, _activeTransfer.totalBytes);
				}
			}
		}
	}

	if( bytes_read )
	{
		rc = fwrite(_fileBuffer, bytes_read, 1, target_file);
		if( rc != 1 )
			done = 3;
	}

	if( total_received > 0 )
		SetTransferProgress(true, target_filename, total_received, _activeTransfer.totalBytes);

	switch( done )
	{
	case 2: SetStatus('W', "Data socket closed early - close reason: %d", _dataSocket->getCloseReason()); break;
	case 3: SetStatus('E', "Error writing to file: %s", target_filename); break;
	case 4: SetStatus('W', "Xfer aborted due to Ctrl-Break"); break;
	}

	fclose(target_file);
	CloseAndFreeDataSocket();

	if( done == 1 )
	{
		uint32_t elapsed = Timer_diff(start, TIMER_GET_CURRENT()) * TIMER_TICK_LEN;
		uint32_t rate = ComputeRate(total_received, elapsed);

		char sizebuf[32];
		FormatSize(sizebuf, sizeof(sizebuf), total_received);

		SetStatus('T', "%s received in %lu.%1lu seconds (%lu.%1lu KBytes/sec)",
			sizebuf, elapsed / 1000u, (elapsed % 1000u) / 100u, rate / 1024u, ((rate % 1024u) * 10u) / 1024u);
	}
	ClearTransferContext();

	return done == 1;
}

bool FtpTransport::SendFile()
{
	uint32_t total_sent = 0;
	clockTicks_t last_tick = 0;
	uint32_t last_bytes = 0;
	DataBuf *buf = NULL;

	if( _dataSocket == NULL )
	{
		ClearTransferContext();
		return false;
	}

	FILE *source_file = fopen(_activeTransfer.localFile, "rb");
	if( source_file == NULL )
	{
		SetStatus('E', "Error opening file %s for reading", _activeTransfer.localFile);
		CloseAndFreeDataSocket();
		ClearTransferContext();
		return false;
	}

	setvbuf(source_file, (char *)_fileBuffer, _IOFBF, _sendFileBufSize);
	clockTicks_t start = TIMER_GET_CURRENT();

	int done = 0;
	while( !done )
	{
		CheckAbortKey();
		if( _ctrlBreakDetected )
		{
			done = 4;
			break;
		}

		PACKET_PROCESS_MULT(5);
		Arp::driveArp();
		Tcp::drivePackets();

		if( _dataSocket->isRemoteClosed() )
		{
			done = 2;
			break;
		}

		while( _dataSocket->outgoing.hasRoom() )
		{
			uint16_t bytes_to_send;

			CheckAbortKey();
			if( _ctrlBreakDetected )
			{
				done = 4;
				break;
			}

			if( buf == NULL )
			{
				buf = (DataBuf *)TcpBuffer::getXmitBuf();
				if( buf == NULL )
					break;

				bytes_to_send = fread(buf->data, 1, _dataSocket->maxEnqueueSize, source_file);
				if( bytes_to_send == 0 )
				{
					TcpBuffer::returnXmitBuf((TcpBuffer *)buf);
					if( feof(source_file) )
						done = 1;
					else if( ferror(source_file) )
						done = 3;
					break;
				}
				buf->b.dataLen = bytes_to_send;
			}

			if( _dataSocket->enqueue(&buf->b) )
			{
				done = 2;
				break;
			}

			total_sent += buf->b.dataLen;
			PACKET_PROCESS_MULT(5);
			Tcp::drivePackets();

			if( ShouldUpdateProgress(last_tick, last_bytes, total_sent) )
				SetTransferProgress(false, _activeTransfer.localFile, total_sent, _activeTransfer.totalBytes);

			buf = NULL;
		}

		if( buf == NULL )
		{
			buf = (DataBuf *)TcpBuffer::getXmitBuf();
			if( buf != NULL )
			{
				uint16_t bytes_to_send = fread(buf->data, 1, _dataSocket->maxEnqueueSize, source_file);
				if( bytes_to_send == 0 )
				{
					TcpBuffer::returnXmitBuf((TcpBuffer *)buf);
					if( feof(source_file) )
						done = 1;
					else if( ferror(source_file) )
						done = 3;
				}
				else
					buf->b.dataLen = bytes_to_send;
			}
		}
	}

	if( total_sent > 0 )
		SetTransferProgress(false, _activeTransfer.localFile, total_sent, _activeTransfer.totalBytes);

	switch( done )
	{
	case 2: SetStatus('W', "Data socket closed early - close reason: %d", _dataSocket->getCloseReason()); break;
	case 3: SetStatus('E', "Xfer aborted due to filesystem error"); break;
	case 4: SetStatus('W', "Xfer aborted due to Ctrl-Break"); break;
	}

	fclose(source_file);
	CloseAndFreeDataSocket();

	if( done == 1 )
	{
		uint32_t elapsed = Timer_diff(start, TIMER_GET_CURRENT()) * TIMER_TICK_LEN;
		uint32_t rate = ComputeRate(total_sent, elapsed);

		char sizebuf[32];
		FormatSize(sizebuf, sizeof(sizebuf), total_sent);
		
		SetStatus('T', "%s sent in %lu.%1lu seconds (%lu.%1lu KBytes/sec)",
			sizebuf, elapsed / 1000u, (elapsed % 1000u) / 100u, rate / 1024u, ((rate % 1024u) * 10u) / 1024u);
	}
	ClearTransferContext();

	return done == 1;
}

bool FtpTransport::TryParseUnixListLine(char *line, char *nameOut, int nameOutSize, bool &isDir, uint32_t &sizeOut, char *modifyOut, int modifyOutSize)
{
	char tmp[UI_LINE_MAX];
	char *fields[16];

	CopyTrunc(tmp, sizeof(tmp), line);
	TrimBoth(tmp);

	if( tmp[0] == '\0' )
		return false;
	if( strncmp(tmp, "total ", 6) == 0 )
		return false;
	if( !(tmp[0] == 'd' || tmp[0] == '-' || tmp[0] == 'l') )
		return false;
	if( strlen(tmp) < 10 )
		return false;

	int count = 0;
	char *p = tmp;
	while( *p && count < 16 )
	{
		while( *p == ' ' || *p == '\t' )
			p++;
		if( !*p )
			break;
		fields[count++] = p;
		while( *p && *p != ' ' && *p != '\t' )
			p++;
		if( *p )
			*p++ = '\0';
	}

	if( count < 9 )
		return false;

	isDir = (fields[0][0] == 'd');
	sizeOut = strtoul(fields[4], NULL, 10);

	modifyOut[0] = '\0';
	if( modifyOutSize > 0 )
		SprintfTrunc(modifyOut, modifyOutSize, "%s %s %s", fields[5], fields[6], fields[7]);

	char *name = strstr(line, fields[8]);
	if( name == NULL )
		return false;

	TrimRight(name);

	if( fields[0][0] == 'l' )
	{
		char *arrow = strstr(name, " -> ");
		if( arrow )
			*arrow = '\0';
	}

	for( int i = 0; name[i]; i++ )
	{
		if( name[i] == '\r' || name[i] == '\n' )
		{
			name[i] = '\0';
			break;
		}
	}

	TrimBoth(name);
	if( name[0] == '\0' )
		return false;

	CopyTrunc(nameOut, (size_t)nameOutSize, name);
	return true;
}

bool FtpTransport::TryParseDosListLine(char *line, char *nameOut, int nameOutSize, bool &isDir, uint32_t &sizeOut, char *modifyOut, int modifyOutSize)
{
	char *fields[8];

	char tmp[UI_LINE_MAX];
	CopyTrunc(tmp, sizeof(tmp), line);
	TrimBoth(tmp);

	if( tmp[0] == '\0' )
		return false;
	if( !isdigit((unsigned char)tmp[0]) )
		return false;

	int count = 0;
	char *p = tmp;
	while( *p && count < 8 )
	{
		while( *p == ' ' || *p == '\t' )
			p++;
		if( !*p )
			break;
		fields[count++] = p;
		while( *p && *p != ' ' && *p != '\t' )
			p++;
		if( *p )
			*p++ = '\0';
	}

	if( count < 4 )
		return false;
	if( strchr(fields[0], '-') == NULL && strchr(fields[0], '/') == NULL )
		return false;
	if( strchr(fields[1], ':') == NULL )
		return false;

	sizeOut = 0;
	if( stricmp(fields[2], "<DIR>") == 0 )
	{
		isDir = true;
	}
	else
	{
		char *q = fields[2];
		while( *q )
		{
			if( !isdigit((unsigned char)*q) && *q != ',' )
				return false;
			q++;
		}
		isDir = false;
		uint32_t sz = 0;
		q = fields[2];
		while( *q )
		{
			if( *q != ',' ) sz = sz * 10 + (*q - '0');
			q++;
		}
		sizeOut = sz;
	}

	modifyOut[0] = '\0';
	if( modifyOutSize > 0 )
		SprintfTrunc(modifyOut, modifyOutSize, "%s %s", fields[0], fields[1]);

	char *name = strstr(line, fields[3]);
	if( name == NULL )
		return false;

	for( int i = 0; name[i]; i++ )
	{
		if( name[i] == '\r' || name[i] == '\n' )
		{
			name[i] = '\0';
			break;
		}
	}

	TrimBoth(name);
	if( name[0] == '\0' )
		return false;

	CopyTrunc(nameOut, (size_t)nameOutSize, name);
	return true;
}

void FtpTransport::ParseListLine(char *line)
{
	char name[UI_NAME_MAX];
	char modif[20];
	uint32_t size = 0;
	bool is_dir = false;

	if( !TryParseUnixListLine(line, name, sizeof(name), is_dir, size, modif, sizeof(modif)) &&
		!TryParseDosListLine(line, name, sizeof(name), is_dir, size, modif, sizeof(modif)) )
	{
		return;
	}

	if( !IsDots(name) )
		_remoteList->Add(name, is_dir, false, size, modif);
}
