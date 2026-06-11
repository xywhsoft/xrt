#ifndef XRT_EXTLIB_XSSH_H
#define XRT_EXTLIB_XSSH_H

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#if defined(_WIN32) || defined(_WIN64)
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#endif

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

#define XSSH_DEFAULT_PORT          22u
#define XSSH_DEFAULT_TIMEOUT_MS    30000u
#define XSSH_BANNER_MAX            255u
#define XSSH_PACKET_MAX_DEFAULT    35000u
#define XSSH_CHANNEL_WINDOW_DEFAULT 2097152u
#define XSSH_CHANNEL_WINDOW_MAX     16777216u
#define XSSH_PACKET_BLOCK_MIN      8u
#define XSSH_PACKET_PADDING_MIN    4u
#define XSSH_KEX_COOKIE_SIZE       16u
#define XSSH_SHA1_SIZE             20u
#define XSSH_MD5_SIZE              16u
#define XSSH_SHA256_SIZE           32u
#define XSSH_SHA512_SIZE           64u
#define XSSH_AEAD_TAG_SIZE         16u
#define XSSH_AES128_KEY_SIZE       16u
#define XSSH_AES256_KEY_SIZE       32u
#define XSSH_GCM_NONCE_SIZE        12u
#define XSSH_CHACHA20_KEY_SIZE     32u
#define XSSH_CHACHA20_NONCE_SIZE   12u

#define XSSH_OK                    0
#define XSSH_WIRE_NEED_MORE        1
#define XSSH_ERR_INVALID          -1
#define XSSH_ERR_NO_SPACE         -2
#define XSSH_ERR_OVERFLOW         -3
#define XSSH_ERR_UNSUPPORTED      -4
#define XSSH_ERR_BAD_PACKET       -5
#define XSSH_ERR_HOSTKEY_UNKNOWN  -6
#define XSSH_ERR_HOSTKEY_MISMATCH -7
#define XSSH_ERR_AUTH_FAILED      -8
#define XSSH_ERR_NOT_CONNECTED    -9
#define XSSH_ERR_CLOSED          -10
#define XSSH_ERR_IO              -11
#define XSSH_ERR_TIMEOUT         -12
#define XSSH_ERR_CHANNEL_FAILED  -13

#define XSSH_STAGE_NONE            0
#define XSSH_STAGE_CONNECT         1
#define XSSH_STAGE_BANNER          2
#define XSSH_STAGE_KEX             3
#define XSSH_STAGE_HOSTKEY         4
#define XSSH_STAGE_NEWKEYS         5
#define XSSH_STAGE_AUTH            6
#define XSSH_STAGE_CHANNEL         7
#define XSSH_STAGE_DISCONNECT      8

#define XSSH_HOSTKEY_STRICT        0
#define XSSH_HOSTKEY_ACCEPT_NEW    1
#define XSSH_HOSTKEY_INSECURE      2

#define XSSH_PACKET_MODE_PLAIN     0
#define XSSH_PACKET_MODE_AESGCM    1

#define XSSH_EVENT_NONE                 0
#define XSSH_EVENT_CONNECTED            1
#define XSSH_EVENT_AUTHENTICATED        2
#define XSSH_EVENT_CHANNEL_DATA         3
#define XSSH_EVENT_CHANNEL_EXTENDED_DATA 4
#define XSSH_EVENT_CHANNEL_EOF          5
#define XSSH_EVENT_CHANNEL_CLOSE        6
#define XSSH_EVENT_DISCONNECT           7
#define XSSH_EVENT_ERROR                8
#define XSSH_EVENT_CHANNEL_OPEN         9

#define XSSH_TRANSPORT_STATE_NONE       0
#define XSSH_TRANSPORT_STATE_CONNECTING 1
#define XSSH_TRANSPORT_STATE_OPEN       2
#define XSSH_TRANSPORT_STATE_IDENTIFIED 3
#define XSSH_TRANSPORT_STATE_CLOSED     4
#define XSSH_CLIENT_BANNER "SSH-2.0-xssh_0.1"

#define XSSH_MSG_DISCONNECT       1u
#define XSSH_MSG_IGNORE           2u
#define XSSH_MSG_UNIMPLEMENTED    3u
#define XSSH_MSG_DEBUG            4u
#define XSSH_MSG_SERVICE_REQUEST  5u
#define XSSH_MSG_SERVICE_ACCEPT   6u
#define XSSH_MSG_KEXINIT          20u
#define XSSH_MSG_NEWKEYS          21u
#define XSSH_MSG_KEX_ECDH_INIT    30u
#define XSSH_MSG_KEX_ECDH_REPLY   31u
#define XSSH_MSG_USERAUTH_REQUEST 50u
#define XSSH_MSG_USERAUTH_FAILURE 51u
#define XSSH_MSG_USERAUTH_SUCCESS 52u
#define XSSH_MSG_USERAUTH_BANNER  53u
#define XSSH_MSG_USERAUTH_PK_OK   60u
#define XSSH_MSG_USERAUTH_INFO_REQUEST 60u
#define XSSH_MSG_USERAUTH_INFO_RESPONSE 61u
#define XSSH_MSG_GLOBAL_REQUEST   80u
#define XSSH_MSG_REQUEST_SUCCESS  81u
#define XSSH_MSG_REQUEST_FAILURE  82u
#define XSSH_MSG_CHANNEL_OPEN     90u
#define XSSH_MSG_CHANNEL_OPEN_CONFIRMATION 91u
#define XSSH_MSG_CHANNEL_OPEN_FAILURE 92u
#define XSSH_MSG_CHANNEL_WINDOW_ADJUST 93u
#define XSSH_MSG_CHANNEL_DATA     94u
#define XSSH_MSG_CHANNEL_EXTENDED_DATA 95u
#define XSSH_MSG_CHANNEL_EOF      96u
#define XSSH_MSG_CHANNEL_CLOSE    97u
#define XSSH_MSG_CHANNEL_REQUEST  98u
#define XSSH_MSG_CHANNEL_SUCCESS  99u
#define XSSH_MSG_CHANNEL_FAILURE  100u

#define XSSH_ALG_KEX_DEFAULT "curve25519-sha256,curve25519-sha256@libssh.org,ecdh-sha2-nistp256,ecdh-sha2-nistp384,diffie-hellman-group14-sha256,diffie-hellman-group16-sha512"
#define XSSH_ALG_HOSTKEY_DEFAULT "ssh-ed25519,rsa-sha2-512,rsa-sha2-256,ecdsa-sha2-nistp256,ecdsa-sha2-nistp384"
#define XSSH_ALG_CIPHER_DEFAULT "aes128-gcm@openssh.com,aes256-gcm@openssh.com,chacha20-poly1305@openssh.com,aes128-ctr,aes256-ctr"
#define XSSH_ALG_MAC_DEFAULT "hmac-sha2-256-etm@openssh.com,hmac-sha2-512-etm@openssh.com,hmac-sha2-256,hmac-sha2-512"
#define XSSH_ALG_COMP_DEFAULT "none"

#define XSSH_RUNTIME_MAX_CHANNELS 16u
#define XSSH_RUNTIME_EVENT_CAP    64u
#define XSSH_RUNTIME_EVENT_DATA_CAP 1024u
#define XSSH_RUNTIME_CHANNEL_BUFFER_CAP 16384u
#define XSSH_RUNTIME_CHANNEL_PENDING_CAP 16384u
#define XSSH_RUNTIME_WIRE_RECV_CAP 32768u
#define XSSH_AUTH_KBDINT_PROMPT_MAX 8u
#define XSSH_AUTH_KBDINT_RESPONSE_MAX 256u
#define XSSH_AUTH_PACKET_MAX        64u
#define XSSH_AUTH_KBDINT_ROUND_MAX  8u
#define XSSH_PTY_MODES_MAX          128u

#define XSSH_TTY_OP_END             0u
#define XSSH_TTY_OP_VINTR           1u
#define XSSH_TTY_OP_VERASE          3u
#define XSSH_TTY_OP_VEOF            5u
#define XSSH_TTY_OP_ISIG            50u
#define XSSH_TTY_OP_ICANON          51u
#define XSSH_TTY_OP_ECHO            53u
#define XSSH_TTY_OP_ECHOE           54u
#define XSSH_TTY_OP_ECHOK           55u
#define XSSH_TTY_OP_ECHONL          56u
#define XSSH_TTY_OP_IEXTEN          59u
#define XSSH_TTY_OP_ECHOCTL         60u
#define XSSH_TTY_OP_ECHOKE          61u
#define XSSH_TTY_OP_OPOST           70u
#define XSSH_TTY_OP_ONLCR           72u
#define XSSH_TTY_OP_CS8             91u
#define XSSH_TTY_OP_ISPEED          128u
#define XSSH_TTY_OP_OSPEED          129u

#define XSSH_OPEN_UNKNOWN_CHANNEL_TYPE 3u
#define XSSH_OPEN_RESOURCE_SHORTAGE 4u

typedef struct xssh_session_t xssh_session_t;
typedef struct xssh_channel_t xssh_channel_t;
typedef struct xssh_known_hosts_t xssh_known_hosts_t;

typedef struct {
	const uint8* pData;
	size_t iLen;
} xsshslice;

typedef struct {
	const uint8* pData;
	size_t iSize;
	size_t iPos;
} xsshreader;

typedef struct {
	uint8* pData;
	size_t iCap;
	size_t iLen;
} xsshwriter;

typedef struct {
	uint32 iSeqNo;
	uint32 iPacketLen;
	uint8 iPaddingLen;
	xsshslice tPayload;
	xsshslice tPadding;
} xsshpacket;

typedef int (*xsshrandomfunc)(void* pUser, uint8* pOut, size_t iLen);

typedef struct {
	uint8 arrKey[XSSH_AES256_KEY_SIZE];
	size_t iKeyLen;
	uint8 arrFixedIV[4];
	uint64 iInvocationCounter;
} xsshaesgcmstate;

typedef struct {
	int iReadMode;
	int iWriteMode;
	uint32 iReadSeqNo;
	uint32 iWriteSeqNo;
	xsshaesgcmstate tReadAESGCM;
	xsshaesgcmstate tWriteAESGCM;
} xsshpacketcodec;

typedef struct {
	const uint8* pCookie;
	const char* sKexAlgorithms;
	const char* sServerHostKeyAlgorithms;
	const char* sEncryptionClientToServer;
	const char* sEncryptionServerToClient;
	const char* sMacClientToServer;
	const char* sMacServerToClient;
	const char* sCompressionClientToServer;
	const char* sCompressionServerToClient;
	const char* sLanguagesClientToServer;
	const char* sLanguagesServerToClient;
	bool bFirstKexPacketFollows;
} xsshkexinitdesc;

typedef struct {
	uint8 arrCookie[XSSH_KEX_COOKIE_SIZE];
	xsshslice tKexAlgorithms;
	xsshslice tServerHostKeyAlgorithms;
	xsshslice tEncryptionClientToServer;
	xsshslice tEncryptionServerToClient;
	xsshslice tMacClientToServer;
	xsshslice tMacServerToClient;
	xsshslice tCompressionClientToServer;
	xsshslice tCompressionServerToClient;
	xsshslice tLanguagesClientToServer;
	xsshslice tLanguagesServerToClient;
	bool bFirstKexPacketFollows;
	uint32 iReserved;
} xsshkexinit;

typedef struct {
	xsshslice tKexAlgorithm;
	xsshslice tServerHostKeyAlgorithm;
	xsshslice tCipherClientToServer;
	xsshslice tCipherServerToClient;
	xsshslice tMacClientToServer;
	xsshslice tMacServerToClient;
	xsshslice tCompressionClientToServer;
	xsshslice tCompressionServerToClient;
} xsshkexnegotiation;

typedef struct {
	xsshslice tClientVersion;
	xsshslice tServerVersion;
	xsshslice tClientKexInit;
	xsshslice tServerKexInit;
	xsshslice tServerHostKey;
	xsshslice tClientEphemeral;
	xsshslice tServerEphemeral;
	xsshslice tSharedSecret;
} xsshkexhashdesc;

typedef struct {
	xsshslice tClientPublic;
} xsshkexecdhinit;

typedef struct {
	xsshslice tServerHostKey;
	xsshslice tServerPublic;
	xsshslice tSignature;
} xsshkexecdhreply;

typedef struct {
	xsshslice tAlgorithm;
	xsshslice tKeyData;
} xsshpublickey;

typedef struct {
	xsshslice tAlgorithm;
	xsshslice tSignature;
} xsshsignature;

typedef struct {
	uint32 iReason;
	xsshslice tDescription;
	xsshslice tLanguageTag;
} xsshdisconnectmsg;

typedef struct {
	xsshslice tData;
} xsshignoremsg;

typedef struct {
	bool bAlwaysDisplay;
	xsshslice tMessage;
	xsshslice tLanguageTag;
} xsshdebugmsg;

typedef struct {
	xsshslice tServiceName;
} xsshservicemsg;

typedef struct {
	xsshslice tRequestName;
	bool bWantReply;
	xsshslice tPayload;
} xsshglobalrequestmsg;

typedef struct {
	xsshslice tBindAddress;
	uint32 iBindPort;
} xsshglobaltcpipforwardmsg;

typedef struct {
	bool bHasBoundPort;
	uint32 iBoundPort;
} xsshglobalrequestsuccessmsg;

typedef struct {
	xsshslice tUserName;
	xsshslice tServiceName;
	xsshslice tMethodName;
	bool bPasswordChange;
	xsshslice tPassword;
	bool bPublicKeyHasSignature;
	xsshslice tPublicKeyAlgorithm;
	xsshslice tPublicKeyBlob;
	xsshslice tSignature;
	xsshslice tKbdIntLanguageTag;
	xsshslice tKbdIntSubmethods;
} xsshauthrequestmsg;

typedef struct {
	xsshslice tMethods;
	bool bPartialSuccess;
} xsshauthfailuremsg;

typedef struct {
	xsshslice tMessage;
	xsshslice tLanguageTag;
} xsshauthbannermsg;

typedef struct {
	xsshslice tPublicKeyAlgorithm;
	xsshslice tPublicKeyBlob;
} xsshauthpkokmsg;

typedef struct {
	xsshslice tName;
	xsshslice tInstruction;
	xsshslice tLanguageTag;
	uint32 iPromptCount;
	xsshslice arrPrompts[XSSH_AUTH_KBDINT_PROMPT_MAX];
	bool arrEcho[XSSH_AUTH_KBDINT_PROMPT_MAX];
} xsshauthkbdintrequestmsg;

typedef struct {
	uint32 iResponseCount;
	xsshslice arrResponses[XSSH_AUTH_KBDINT_PROMPT_MAX];
} xsshauthkbdintresponsemsg;

typedef struct {
	xsshslice tChannelType;
	uint32 iSenderChannel;
	uint32 iInitialWindow;
	uint32 iMaxPacket;
	xsshslice tExtra;
} xsshchannelopenmsg;

typedef struct {
	xsshslice tConnectedHost;
	uint32 iConnectedPort;
	xsshslice tOriginatorHost;
	uint32 iOriginatorPort;
} xsshchanneltcpipopenmsg;

typedef struct {
	uint32 iRecipientChannel;
	uint32 iSenderChannel;
	uint32 iInitialWindow;
	uint32 iMaxPacket;
} xsshchannelopenconfirmmsg;

typedef struct {
	uint32 iRecipientChannel;
	uint32 iReason;
	xsshslice tDescription;
	xsshslice tLanguageTag;
} xsshchannelopenfailuremsg;

typedef struct {
	uint32 iRecipientChannel;
	uint32 iBytes;
} xsshchannelwindowadjustmsg;

typedef struct {
	uint32 iRecipientChannel;
	uint32 iDataType;
	xsshslice tData;
} xsshchanneldataeventmsg;

typedef struct {
	uint32 iRecipientChannel;
	xsshslice tRequestType;
	bool bWantReply;
	xsshslice tPayload;
} xsshchannelrequestmsg;

typedef struct {
	xsshslice tSignalName;
	bool bCoreDumped;
	xsshslice tErrorMessage;
	xsshslice tLanguageTag;
} xsshchannelexitsignalmsg;

typedef struct {
	xsshslice tAlgorithm;
	xsshslice tBase64;
	xsshslice tComment;
	xsshslice tBlob;
	xsshpublickey tKey;
} xsshopensshpublickeyline;

typedef struct {
	xsshslice tHosts;
	xsshslice tAlgorithm;
	xsshslice tBase64;
	xsshslice tComment;
	xsshslice tBlob;
	xsshpublickey tKey;
} xsshknownhostline;

typedef struct {
	char sAlgorithm[32];
	uint8 arrPublicKeyBlob[128];
	size_t iPublicKeyBlobLen;
	uint8 arrEd25519Seed[32];
	uint8 arrEd25519Pub[32];
	bool bHasEd25519;
} xsshprivateidentity;

typedef struct {
	const char* sHost;
	uint16 iPort;
	const char* sUser;
	uint32 iConnectTimeoutMs;
	uint32 iIoTimeoutMs;
	int iHostKeyPolicy;
	const char* sKnownHostsPath;
	uint32 iMaxPacketSize;
	uint32 iChannelWindow;
	void (*pLog)(ptr pUser, int iLevel, const char* sText);
	ptr pUser;
} xsshconfig;

typedef struct {
	bool bSuccess;
	int iError;
	int iStage;
	int iDisconnectReason;
	char sError[256];
	char sAuthMethods[128];
	char sServerBanner[XSSH_BANNER_MAX + 1u];
	char sKex[64];
	char sHostKey[64];
	char sCipherClientToServer[64];
	char sCipherServerToClient[64];
	char sMacClientToServer[64];
	char sMacServerToClient[64];
	char sHostKeyFingerprint[128];
	bool bHostKeyInsecure;
	bool bHostKeyAcceptedNew;
} xsshresult;

typedef struct {
	const char* sPassword;
	const char* sPrivateKeyPath;
	const void* pPrivateKeyData;
	size_t iPrivateKeySize;
	const char* sPrivateKeyPassphrase;
	const char* sPublicKeyAlgorithm;
	const void* pPublicKeyBlob;
	size_t iPublicKeyBlobLen;
	int (*pSign)(ptr pUser, const uint8* pData, size_t iLen, uint8* pSig, size_t* piSigLen);
	ptr pSignerUser;
	int (*pKeyboardInteractive)(ptr pUser, const char* sName, const char* sInstruction, const char* sPrompt, bool bEcho, char* sOut, size_t iOutCap);
	ptr pKeyboardUser;
} xsshauth;

typedef struct {
	const char* sTerm;
	uint32 iCols;
	uint32 iRows;
	uint32 iPixelWidth;
	uint32 iPixelHeight;
	uint8 arrTerminalModes[XSSH_PTY_MODES_MAX];
	size_t iTerminalModesLen;
} xsshpty;

typedef struct {
	int iType;
	xssh_channel_t* pChannel;
	const uint8* pData;
	size_t iLen;
	int iCode;
	char sText[256];
} xsshevent;

typedef struct {
	const char* sBindHost;
	uint16 iBindPort;
	const char* sTargetHost;
	uint16 iTargetPort;
} xsshforward;

typedef struct {
	xsshevent tEvent;
	uint8 arrData[XSSH_RUNTIME_EVENT_DATA_CAP];
	size_t iDataLen;
} xsshevententry;

struct xssh_channel_t {
	xssh_session_t* pSession;
	uint32 iLocalChannel;
	uint32 iRemoteChannel;
	uint32 iLocalWindow;
	uint32 iRemoteWindow;
	uint32 iMaxPacket;
	char sChannelType[32];
	char sConnectedHost[128];
	uint32 iConnectedPort;
	char sOriginatorHost[128];
	uint32 iOriginatorPort;
	bool bOpen;
	bool bEofSent;
	bool bEofRecv;
	bool bCloseSent;
	bool bCloseRecv;
	bool bClosed;
	bool bHasExitStatus;
	uint32 iExitStatus;
	bool bHasExitSignal;
	char sExitSignal[64];
	uint8 arrStdout[XSSH_RUNTIME_CHANNEL_BUFFER_CAP];
	size_t iStdoutLen;
	size_t iStdoutPos;
	uint8 arrStderr[XSSH_RUNTIME_CHANNEL_BUFFER_CAP];
	size_t iStderrLen;
	size_t iStderrPos;
	uint8 arrPendingWrite[XSSH_RUNTIME_CHANNEL_PENDING_CAP];
	size_t iPendingWriteLen;
	size_t iPendingWritePos;
};

struct xssh_session_t {
	xsshconfig tConfig;
	xsshauth tAuth;
	xsshresult tResult;
	bool bConnected;
	bool bAuthenticated;
	bool bMock;
	int iTransportState;
	xnetengine* pEngine;
	xnetstream* pStream;
	xmutex pLock;
	xcond pCond;
	bool bOwnEngine;
	bool bStreamOpen;
	bool bStreamClosed;
	bool bRecvOverflow;
	int iCloseReason;
	int iLastSysErr;
	uint32 iTransportTimeoutMs;
	int iPacketIoStage;
	xsshpacketcodec tPacketCodec;
	uint8 arrSessionId[XSSH_SHA256_SIZE];
	bool bHasSessionId;
	uint32 iNextLocalChannel;
	xssh_channel_t arrChannels[XSSH_RUNTIME_MAX_CHANNELS];
	xsshevententry arrEvents[XSSH_RUNTIME_EVENT_CAP];
	size_t iEventHead;
	size_t iEventCount;
	uint8 arrLastEventData[XSSH_RUNTIME_EVENT_DATA_CAP];
	uint8 arrWireRecv[XSSH_RUNTIME_WIRE_RECV_CAP];
	size_t iWireRecvLen;
};

static void xsshConfigInit(xsshconfig* pCfg)
{
	if ( pCfg == NULL ) {
		return;
	}
	memset(pCfg, 0, sizeof(*pCfg));
	pCfg->iPort = (uint16)XSSH_DEFAULT_PORT;
	pCfg->iConnectTimeoutMs = XSSH_DEFAULT_TIMEOUT_MS;
	pCfg->iIoTimeoutMs = XSSH_DEFAULT_TIMEOUT_MS;
	pCfg->iHostKeyPolicy = XSSH_HOSTKEY_STRICT;
	pCfg->iMaxPacketSize = XSSH_PACKET_MAX_DEFAULT;
	pCfg->iChannelWindow = XSSH_CHANNEL_WINDOW_DEFAULT;
}

static void xsshResultInit(xsshresult* pRet)
{
	if ( pRet == NULL ) {
		return;
	}
	memset(pRet, 0, sizeof(*pRet));
	pRet->iStage = XSSH_STAGE_NONE;
}

static void xsshAuthInit(xsshauth* pAuth)
{
	if ( pAuth == NULL ) {
		return;
	}
	memset(pAuth, 0, sizeof(*pAuth));
}

static void xsshConfigNormalize(xsshconfig* pCfg)
{
	if ( pCfg == NULL ) {
		return;
	}
	if ( pCfg->iMaxPacketSize == 0u || pCfg->iMaxPacketSize > XSSH_PACKET_MAX_DEFAULT ) {
		pCfg->iMaxPacketSize = XSSH_PACKET_MAX_DEFAULT;
	}
	if ( pCfg->iChannelWindow == 0u ) {
		pCfg->iChannelWindow = XSSH_CHANNEL_WINDOW_DEFAULT;
	} else if ( pCfg->iChannelWindow > XSSH_CHANNEL_WINDOW_MAX ) {
		pCfg->iChannelWindow = XSSH_CHANNEL_WINDOW_MAX;
	}
}

static void xsshPtyInit(xsshpty* pPty)
{
	if ( pPty == NULL ) {
		return;
	}
	memset(pPty, 0, sizeof(*pPty));
	pPty->sTerm = "xterm-256color";
	pPty->iCols = 80u;
	pPty->iRows = 24u;
}

static void xsshPtyModeClear(xsshpty* pPty)
{
	volatile uint8* p;
	size_t i;

	if ( pPty == NULL ) {
		return;
	}
	p = (volatile uint8*)pPty->arrTerminalModes;
	for ( i = 0u; i < sizeof(pPty->arrTerminalModes); ++i ) {
		p[i] = 0u;
	}
	pPty->iTerminalModesLen = 0u;
}

static int xsshPtyModeSet(xsshpty* pPty, uint8 iOpcode, uint32 iValue)
{
	size_t i;
	uint8* pEntry = NULL;

	if ( pPty == NULL || iOpcode == XSSH_TTY_OP_END ) {
		return XSSH_ERR_INVALID;
	}
	if ( pPty->iTerminalModesLen > sizeof(pPty->arrTerminalModes) ||
		(pPty->iTerminalModesLen % 5u) != 0u ) {
		return XSSH_ERR_INVALID;
	}
	for ( i = 0u; i < pPty->iTerminalModesLen; i += 5u ) {
		if ( pPty->arrTerminalModes[i] == iOpcode ) {
			pEntry = pPty->arrTerminalModes + i;
			break;
		}
	}
	if ( pEntry == NULL ) {
		if ( pPty->iTerminalModesLen + 5u > sizeof(pPty->arrTerminalModes) ) {
			return XSSH_ERR_NO_SPACE;
		}
		pEntry = pPty->arrTerminalModes + pPty->iTerminalModesLen;
		pPty->iTerminalModesLen += 5u;
	}
	/* RFC 4254 terminal modes 用 1 字节 opcode + 4 字节大端值，最后由 TTY_OP_END 结束。 */
	pEntry[0] = iOpcode;
	pEntry[1] = (uint8)(iValue >> 24u);
	pEntry[2] = (uint8)(iValue >> 16u);
	pEntry[3] = (uint8)(iValue >> 8u);
	pEntry[4] = (uint8)iValue;
	return XSSH_OK;
}

static void xsshKexInitDescInit(xsshkexinitdesc* pDesc)
{
	if ( pDesc == NULL ) {
		return;
	}
	memset(pDesc, 0, sizeof(*pDesc));
	pDesc->sKexAlgorithms = XSSH_ALG_KEX_DEFAULT;
	pDesc->sServerHostKeyAlgorithms = XSSH_ALG_HOSTKEY_DEFAULT;
	pDesc->sEncryptionClientToServer = XSSH_ALG_CIPHER_DEFAULT;
	pDesc->sEncryptionServerToClient = XSSH_ALG_CIPHER_DEFAULT;
	pDesc->sMacClientToServer = XSSH_ALG_MAC_DEFAULT;
	pDesc->sMacServerToClient = XSSH_ALG_MAC_DEFAULT;
	pDesc->sCompressionClientToServer = XSSH_ALG_COMP_DEFAULT;
	pDesc->sCompressionServerToClient = XSSH_ALG_COMP_DEFAULT;
	pDesc->sLanguagesClientToServer = "";
	pDesc->sLanguagesServerToClient = "";
}

static void xsshReaderInit(xsshreader* pReader, const void* pData, size_t iSize)
{
	if ( pReader == NULL ) {
		return;
	}
	pReader->pData = (const uint8*)pData;
	pReader->iSize = iSize;
	pReader->iPos = 0u;
}

static void xsshWriterInit(xsshwriter* pWriter, void* pData, size_t iCap)
{
	if ( pWriter == NULL ) {
		return;
	}
	pWriter->pData = (uint8*)pData;
	pWriter->iCap = iCap;
	pWriter->iLen = 0u;
}

static void xsshSecureZero(void* pData, size_t iLen)
{
	volatile uint8* p = (volatile uint8*)pData;

	if ( pData == NULL ) {
		return;
	}
	while ( iLen-- > 0u ) {
		*p++ = 0u;
	}
}

static size_t xsshReaderLeft(const xsshreader* pReader)
{
	if ( pReader == NULL || pReader->iPos > pReader->iSize ) {
		return 0u;
	}
	return pReader->iSize - pReader->iPos;
}

static int xsshWireReadU8(xsshreader* pReader, uint8* pOut)
{
	if ( pReader == NULL || pOut == NULL || pReader->pData == NULL ) {
		return XSSH_ERR_INVALID;
	}
	if ( xsshReaderLeft(pReader) < 1u ) {
		return XSSH_WIRE_NEED_MORE;
	}
	*pOut = pReader->pData[pReader->iPos++];
	return XSSH_OK;
}

static int xsshWireReadBool(xsshreader* pReader, bool* pOut)
{
	uint8 iValue;
	int iRet = xsshWireReadU8(pReader, &iValue);

	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	*pOut = (iValue != 0u) ? TRUE : FALSE;
	return XSSH_OK;
}

static int xsshWireReadU32(xsshreader* pReader, uint32* pOut)
{
	const uint8* p;

	if ( pReader == NULL || pOut == NULL || pReader->pData == NULL ) {
		return XSSH_ERR_INVALID;
	}
	if ( xsshReaderLeft(pReader) < 4u ) {
		return XSSH_WIRE_NEED_MORE;
	}
	p = pReader->pData + pReader->iPos;
	*pOut = ((uint32)p[0] << 24) | ((uint32)p[1] << 16) | ((uint32)p[2] << 8) | (uint32)p[3];
	pReader->iPos += 4u;
	return XSSH_OK;
}

static int xsshWireReadU64(xsshreader* pReader, uint64* pOut)
{
	size_t iSavedPos;
	uint32 iHi;
	uint32 iLo;
	int iRet;

	if ( pReader == NULL || pOut == NULL ) {
		return XSSH_ERR_INVALID;
	}
	iSavedPos = pReader->iPos;
	iRet = xsshWireReadU32(pReader, &iHi);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	iRet = xsshWireReadU32(pReader, &iLo);
	if ( iRet != XSSH_OK ) {
		pReader->iPos = iSavedPos;
		return iRet;
	}
	*pOut = ((uint64)iHi << 32) | (uint64)iLo;
	return XSSH_OK;
}

static int xsshWireReadString(xsshreader* pReader, xsshslice* pOut)
{
	uint32 iLen;
	int iRet;

	if ( pReader == NULL || pOut == NULL ) {
		return XSSH_ERR_INVALID;
	}
	iRet = xsshWireReadU32(pReader, &iLen);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( xsshReaderLeft(pReader) < (size_t)iLen ) {
		pReader->iPos -= 4u;
		return XSSH_WIRE_NEED_MORE;
	}
	pOut->pData = pReader->pData + pReader->iPos;
	pOut->iLen = (size_t)iLen;
	pReader->iPos += (size_t)iLen;
	return XSSH_OK;
}

static int xsshWireReadBytes(xsshreader* pReader, size_t iLen, xsshslice* pOut)
{
	if ( pReader == NULL || pOut == NULL || pReader->pData == NULL ) {
		return XSSH_ERR_INVALID;
	}
	if ( xsshReaderLeft(pReader) < iLen ) {
		return XSSH_WIRE_NEED_MORE;
	}
	pOut->pData = pReader->pData + pReader->iPos;
	pOut->iLen = iLen;
	pReader->iPos += iLen;
	return XSSH_OK;
}

static int xsshWireReserve(xsshwriter* pWriter, size_t iNeed)
{
	if ( pWriter == NULL || (pWriter->pData == NULL && pWriter->iCap != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	if ( iNeed > pWriter->iCap || pWriter->iLen > pWriter->iCap - iNeed ) {
		return XSSH_ERR_NO_SPACE;
	}
	return XSSH_OK;
}

static int xsshWireWriteU8(xsshwriter* pWriter, uint8 iValue)
{
	int iRet = xsshWireReserve(pWriter, 1u);

	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	pWriter->pData[pWriter->iLen++] = iValue;
	return XSSH_OK;
}

static int xsshWireWriteBool(xsshwriter* pWriter, bool bValue)
{
	return xsshWireWriteU8(pWriter, bValue ? 1u : 0u);
}

static int xsshWireWriteU32(xsshwriter* pWriter, uint32 iValue)
{
	int iRet = xsshWireReserve(pWriter, 4u);

	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	pWriter->pData[pWriter->iLen++] = (uint8)((iValue >> 24) & 0xffu);
	pWriter->pData[pWriter->iLen++] = (uint8)((iValue >> 16) & 0xffu);
	pWriter->pData[pWriter->iLen++] = (uint8)((iValue >> 8) & 0xffu);
	pWriter->pData[pWriter->iLen++] = (uint8)(iValue & 0xffu);
	return XSSH_OK;
}

static int xsshWireWriteU64(xsshwriter* pWriter, uint64 iValue)
{
	int iRet;

	iRet = xsshWireReserve(pWriter, 8u);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	iRet = xsshWireWriteU32(pWriter, (uint32)(iValue >> 32));
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	return xsshWireWriteU32(pWriter, (uint32)(iValue & 0xffffffffu));
}

static int xsshWireWriteString(xsshwriter* pWriter, const void* pData, size_t iLen)
{
	int iRet;

	if ( iLen > 0xffffffffu ) {
		return XSSH_ERR_OVERFLOW;
	}
	if ( pData == NULL && iLen != 0u ) {
		return XSSH_ERR_INVALID;
	}
	iRet = xsshWireReserve(pWriter, 4u + iLen);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	iRet = xsshWireWriteU32(pWriter, (uint32)iLen);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( iLen != 0u ) {
		memcpy(pWriter->pData + pWriter->iLen, pData, iLen);
		pWriter->iLen += iLen;
	}
	return XSSH_OK;
}

static int xsshWireWriteBytes(xsshwriter* pWriter, const void* pData, size_t iLen)
{
	int iRet;

	if ( pData == NULL && iLen != 0u ) {
		return XSSH_ERR_INVALID;
	}
	iRet = xsshWireReserve(pWriter, iLen);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( iLen != 0u ) {
		memcpy(pWriter->pData + pWriter->iLen, pData, iLen);
		pWriter->iLen += iLen;
	}
	return XSSH_OK;
}

static int xsshWireWriteNameList(xsshwriter* pWriter, const char* sList)
{
	return xsshWireWriteString(pWriter, sList, sList ? strlen(sList) : 0u);
}

static bool xsshWireNameListIsValid(const char* sList, size_t iLen)
{
	size_t i;

	if ( sList == NULL && iLen != 0u ) {
		return FALSE;
	}
	if ( iLen == 0u ) {
		return TRUE;
	}
	if ( sList[0] == ',' || sList[iLen - 1u] == ',' ) {
		return FALSE;
	}
	for ( i = 1u; i < iLen; ++i ) {
		if ( sList[i] == ',' && sList[i - 1u] == ',' ) {
			return FALSE;
		}
	}
	return TRUE;
}

static bool xsshWireNameListContains(const char* sList, size_t iLen, const char* sName, size_t iNameLen)
{
	size_t iStart;
	size_t i;

	if ( !xsshWireNameListIsValid(sList, iLen) || sName == NULL || iNameLen == 0u ) {
		return FALSE;
	}
	iStart = 0u;
	for ( i = 0u; i <= iLen; ++i ) {
		if ( i == iLen || sList[i] == ',' ) {
			if ( i - iStart == iNameLen && memcmp(sList + iStart, sName, iNameLen) == 0 ) {
				return TRUE;
			}
			iStart = i + 1u;
		}
	}
	return FALSE;
}

static bool xsshWireNameListTokenEqual(const char* sList, size_t iStart, size_t iEnd, const char* sName, size_t iNameLen)
{
	return (iEnd - iStart == iNameLen && memcmp(sList + iStart, sName, iNameLen) == 0) ? TRUE : FALSE;
}

static bool xsshWireNameListHasDuplicate(const char* sList, size_t iLen)
{
	size_t iStart;
	size_t i;

	if ( !xsshWireNameListIsValid(sList, iLen) ) {
		return FALSE;
	}
	iStart = 0u;
	for ( i = 0u; i <= iLen; ++i ) {
		if ( i == iLen || sList[i] == ',' ) {
			size_t jStart = i + 1u;
			size_t j;

			for ( j = i + 1u; j <= iLen; ++j ) {
				if ( j == iLen || sList[j] == ',' ) {
					if ( xsshWireNameListTokenEqual(sList, iStart, i, sList + jStart, j - jStart) ) {
						return TRUE;
					}
					jStart = j + 1u;
				}
			}
			iStart = i + 1u;
		}
	}
	return FALSE;
}

static int xsshWireNameListFirstMatch(const char* sClientList, size_t iClientLen, const char* sServerList, size_t iServerLen, xsshslice* pOut)
{
	size_t iStart;
	size_t i;

	if ( pOut == NULL || !xsshWireNameListIsValid(sClientList, iClientLen) || !xsshWireNameListIsValid(sServerList, iServerLen) ) {
		return XSSH_ERR_INVALID;
	}
	pOut->pData = NULL;
	pOut->iLen = 0u;
	iStart = 0u;
	for ( i = 0u; i <= iClientLen; ++i ) {
		if ( i == iClientLen || sClientList[i] == ',' ) {
			if ( i > iStart && xsshWireNameListContains(sServerList, iServerLen, sClientList + iStart, i - iStart) ) {
				pOut->pData = (const uint8*)sClientList + iStart;
				pOut->iLen = i - iStart;
				return XSSH_OK;
			}
			iStart = i + 1u;
		}
	}
	return XSSH_ERR_UNSUPPORTED;
}

static bool xsshWireMpintPositiveIsCanonical(const uint8* pData, size_t iLen)
{
	if ( pData == NULL && iLen != 0u ) {
		return FALSE;
	}
	if ( iLen == 0u ) {
		return TRUE;
	}
	/* mpint 正数最高位为 1 时必须补 0；否则会被 SSH 解释为负数。 */
	if ( (pData[0] & 0x80u) != 0u ) {
		return FALSE;
	}
	if ( iLen > 1u && pData[0] == 0u && (pData[1] & 0x80u) == 0u ) {
		return FALSE;
	}
	return TRUE;
}

static int xsshWireReadMpintPositive(xsshreader* pReader, xsshslice* pOut)
{
	int iRet = xsshWireReadString(pReader, pOut);

	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( !xsshWireMpintPositiveIsCanonical(pOut->pData, pOut->iLen) ) {
		return XSSH_ERR_INVALID;
	}
	return XSSH_OK;
}

static int xsshWireWriteMpintPositive(xsshwriter* pWriter, const uint8* pValue, size_t iLen)
{
	size_t iOff = 0u;
	bool bNeedZero;
	int iRet;

	if ( pValue == NULL && iLen != 0u ) {
		return XSSH_ERR_INVALID;
	}
	while ( iOff < iLen && pValue[iOff] == 0u ) {
		iOff++;
	}
	if ( iOff == iLen ) {
		return xsshWireWriteU32(pWriter, 0u);
	}

	pValue += iOff;
	iLen -= iOff;
	bNeedZero = ((pValue[0] & 0x80u) != 0u) ? TRUE : FALSE;
	if ( iLen + (bNeedZero ? 1u : 0u) > 0xffffffffu ) {
		return XSSH_ERR_OVERFLOW;
	}
	iRet = xsshWireReserve(pWriter, 4u + iLen + (bNeedZero ? 1u : 0u));
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	iRet = xsshWireWriteU32(pWriter, (uint32)(iLen + (bNeedZero ? 1u : 0u)));
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( bNeedZero ) {
		pWriter->pData[pWriter->iLen++] = 0u;
	}
	memcpy(pWriter->pData + pWriter->iLen, pValue, iLen);
	pWriter->iLen += iLen;
	return XSSH_OK;
}

static bool xsshWireMpintSignedIsCanonical(const uint8* pData, size_t iLen)
{
	if ( pData == NULL && iLen != 0u ) {
		return FALSE;
	}
	if ( iLen == 0u ) {
		return TRUE;
	}
	/* SSH mpint 是有符号二进制补码；正数/负数都不能保留多余符号扩展字节。 */
	if ( iLen > 1u && pData[0] == 0u && (pData[1] & 0x80u) == 0u ) {
		return FALSE;
	}
	if ( iLen > 1u && pData[0] == 0xffu && (pData[1] & 0x80u) != 0u ) {
		return FALSE;
	}
	return TRUE;
}

static int xsshWireReadMpintSigned(xsshreader* pReader, xsshslice* pOut, bool* pNegative)
{
	int iRet;

	if ( pNegative == NULL ) {
		return XSSH_ERR_INVALID;
	}
	*pNegative = FALSE;
	iRet = xsshWireReadString(pReader, pOut);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( !xsshWireMpintSignedIsCanonical(pOut->pData, pOut->iLen) ) {
		return XSSH_ERR_INVALID;
	}
	*pNegative = (pOut->iLen != 0u && (pOut->pData[0] & 0x80u) != 0u) ? TRUE : FALSE;
	return XSSH_OK;
}

static int xsshWireWriteMpintSigned(xsshwriter* pWriter, const uint8* pMagnitude, size_t iLen, bool bNegative)
{
	size_t iOff = 0u;
	size_t iStart;
	size_t iEncodedLen;
	uint8* pOut;
	uint8* pComp;
	uint16 iCarry = 1u;
	bool bNeedPrefix;
	size_t i;
	int iRet;

	if ( pMagnitude == NULL && iLen != 0u ) {
		return XSSH_ERR_INVALID;
	}
	if ( !bNegative ) {
		return xsshWireWriteMpintPositive(pWriter, pMagnitude, iLen);
	}
	while ( iOff < iLen && pMagnitude[iOff] == 0u ) {
		iOff++;
	}
	if ( iOff == iLen ) {
		return xsshWireWriteU32(pWriter, 0u);
	}
	pMagnitude += iOff;
	iLen -= iOff;
	if ( iLen > 0xffffffffu - 1u ) {
		return XSSH_ERR_OVERFLOW;
	}
	iRet = xsshWireReserve(pWriter, 4u + iLen + 1u);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	iStart = pWriter->iLen;
	pOut = pWriter->pData + iStart + 4u;
	pComp = pOut + 1u;
	for ( i = iLen; i > 0u; --i ) {
		uint16 v = (uint16)((uint8)~pMagnitude[i - 1u]) + iCarry;
		pComp[i - 1u] = (uint8)(v & 0xffu);
		iCarry = (uint16)(v >> 8);
	}
	bNeedPrefix = ((pComp[0] & 0x80u) == 0u) ? TRUE : FALSE;
	if ( bNeedPrefix ) {
		pOut[0] = 0xffu;
		iEncodedLen = iLen + 1u;
	} else {
		memmove(pOut, pComp, iLen);
		iEncodedLen = iLen;
	}
	pWriter->pData[iStart + 0u] = (uint8)((iEncodedLen >> 24) & 0xffu);
	pWriter->pData[iStart + 1u] = (uint8)((iEncodedLen >> 16) & 0xffu);
	pWriter->pData[iStart + 2u] = (uint8)((iEncodedLen >> 8) & 0xffu);
	pWriter->pData[iStart + 3u] = (uint8)(iEncodedLen & 0xffu);
	pWriter->iLen = iStart + 4u + iEncodedLen;
	return XSSH_OK;
}

static int xsshWireReadBanner(const void* pData, size_t iSize, xsshslice* pBanner, size_t* piConsumed)
{
	const uint8* p = (const uint8*)pData;
	size_t iLineStart = 0u;
	size_t i;

	if ( pBanner == NULL || piConsumed == NULL || (p == NULL && iSize != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	pBanner->pData = NULL;
	pBanner->iLen = 0u;
	*piConsumed = 0u;

	for ( i = 0u; i < iSize; ++i ) {
		if ( p[i] == '\n' ) {
			size_t iLineEnd = i;
			size_t iLineLen;

			if ( iLineEnd > iLineStart && p[iLineEnd - 1u] == '\r' ) {
				iLineEnd--;
			}
			iLineLen = iLineEnd - iLineStart;
			if ( iLineLen > XSSH_BANNER_MAX ) {
				return XSSH_ERR_OVERFLOW;
			}
			if ( iLineLen >= 4u && memcmp(p + iLineStart, "SSH-", 4u) == 0 ) {
				bool bVersion2 = (iLineLen >= 8u && memcmp(p + iLineStart, "SSH-2.0-", 8u) == 0) ? TRUE : FALSE;
				bool bVersion199 = (iLineLen >= 9u && memcmp(p + iLineStart, "SSH-1.99-", 9u) == 0) ? TRUE : FALSE;

				/* SSH-1.99 表示兼容 SSH-2.0 的旧写法，仍可进入后续 SSH-2 握手。 */
				if ( !bVersion2 && !bVersion199 ) {
					return XSSH_ERR_INVALID;
				}
				pBanner->pData = p + iLineStart;
				pBanner->iLen = iLineLen;
				*piConsumed = i + 1u;
				return XSSH_OK;
			}
			iLineStart = i + 1u;
		}
	}

	if ( iSize - iLineStart > XSSH_BANNER_MAX ) {
		return XSSH_ERR_OVERFLOW;
	}
	return XSSH_WIRE_NEED_MORE;
}

static size_t xsshPacketNormalizeBlockSize(size_t iBlockSize)
{
	return (iBlockSize < XSSH_PACKET_BLOCK_MIN) ? XSSH_PACKET_BLOCK_MIN : iBlockSize;
}

static int xsshPacketCalcPlainPadding(size_t iPayloadLen, size_t iBlockSize, uint8* pPaddingLen, uint32* pPacketLen)
{
	size_t iBaseLen;
	size_t iPaddingLen;
	size_t iPacketLen;

	if ( pPaddingLen == NULL || pPacketLen == NULL ) {
		return XSSH_ERR_INVALID;
	}
	iBlockSize = xsshPacketNormalizeBlockSize(iBlockSize);
	if ( iBlockSize == 0u || iBlockSize > 255u ) {
		return XSSH_ERR_INVALID;
	}
	if ( iPayloadLen > 0xffffffffu - 1u - 255u ) {
		return XSSH_ERR_OVERFLOW;
	}

	/* packet_length 字段本身也参与块对齐，所以基础长度要从 4 字节长度字段开始算。 */
	iBaseLen = 4u + 1u + iPayloadLen;
	iPaddingLen = iBaseLen % iBlockSize;
	iPaddingLen = (iPaddingLen == 0u) ? 0u : (iBlockSize - iPaddingLen);
	while ( iPaddingLen < XSSH_PACKET_PADDING_MIN ) {
		iPaddingLen += iBlockSize;
	}
	if ( iPaddingLen > 255u ) {
		return XSSH_ERR_OVERFLOW;
	}
	iPacketLen = 1u + iPayloadLen + iPaddingLen;
	if ( iPacketLen > 0xffffffffu ) {
		return XSSH_ERR_OVERFLOW;
	}

	*pPaddingLen = (uint8)iPaddingLen;
	*pPacketLen = (uint32)iPacketLen;
	return XSSH_OK;
}

static int xsshPacketWritePlain(xsshwriter* pWriter, const void* pPayload, size_t iPayloadLen, size_t iBlockSize, uint32* pSeqNo)
{
	const uint8* p = (const uint8*)pPayload;
	uint8 iPaddingLen;
	uint32 iPacketLen;
	size_t iTotalLen;
	int iRet;

	if ( pWriter == NULL || (p == NULL && iPayloadLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	iRet = xsshPacketCalcPlainPadding(iPayloadLen, iBlockSize, &iPaddingLen, &iPacketLen);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	iTotalLen = 4u + (size_t)iPacketLen;
	iRet = xsshWireReserve(pWriter, iTotalLen);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}

	/* 当前 plain packet 的 padding 用 0 填充；加密阶段会替换为随机 padding。 */
	xsshWireWriteU32(pWriter, iPacketLen);
	xsshWireWriteU8(pWriter, iPaddingLen);
	if ( iPayloadLen != 0u ) {
		memcpy(pWriter->pData + pWriter->iLen, p, iPayloadLen);
		pWriter->iLen += iPayloadLen;
	}
	memset(pWriter->pData + pWriter->iLen, 0, iPaddingLen);
	pWriter->iLen += iPaddingLen;

	if ( pSeqNo != NULL ) {
		*pSeqNo = *pSeqNo + 1u;
	}
	return XSSH_OK;
}

static uint32 xsshPacketPeekU32(const uint8* p)
{
	return ((uint32)p[0] << 24) | ((uint32)p[1] << 16) | ((uint32)p[2] << 8) | (uint32)p[3];
}

static int xsshPacketReadPlain(xsshreader* pReader, size_t iBlockSize, uint32 iMaxPacketSize, uint32* pSeqNo, xsshpacket* pOut)
{
	const uint8* p;
	size_t iLeft;
	size_t iTotalLen;
	uint32 iPacketLen;
	uint8 iPaddingLen;
	size_t iPayloadLen;

	if ( pReader == NULL || pOut == NULL || pReader->pData == NULL ) {
		return XSSH_ERR_INVALID;
	}
	memset(pOut, 0, sizeof(*pOut));
	iBlockSize = xsshPacketNormalizeBlockSize(iBlockSize);
	if ( iBlockSize == 0u || iBlockSize > 255u ) {
		return XSSH_ERR_INVALID;
	}

	iLeft = xsshReaderLeft(pReader);
	if ( iLeft < 4u ) {
		return XSSH_WIRE_NEED_MORE;
	}
	p = pReader->pData + pReader->iPos;
	iPacketLen = xsshPacketPeekU32(p);
	if ( iPacketLen < 1u + XSSH_PACKET_PADDING_MIN ) {
		return XSSH_ERR_BAD_PACKET;
	}
	if ( iMaxPacketSize == 0u ) {
		iMaxPacketSize = XSSH_PACKET_MAX_DEFAULT;
	}
	if ( iPacketLen > iMaxPacketSize ) {
		return XSSH_ERR_BAD_PACKET;
	}
	iTotalLen = 4u + (size_t)iPacketLen;
	if ( iLeft < iTotalLen ) {
		return XSSH_WIRE_NEED_MORE;
	}
	if ( (iTotalLen % iBlockSize) != 0u ) {
		return XSSH_ERR_BAD_PACKET;
	}

	iPaddingLen = p[4];
	if ( iPaddingLen < XSSH_PACKET_PADDING_MIN || (size_t)iPaddingLen + 1u > (size_t)iPacketLen ) {
		return XSSH_ERR_BAD_PACKET;
	}
	iPayloadLen = (size_t)iPacketLen - (size_t)iPaddingLen - 1u;

	pOut->iSeqNo = pSeqNo ? *pSeqNo : 0u;
	pOut->iPacketLen = iPacketLen;
	pOut->iPaddingLen = iPaddingLen;
	pOut->tPayload.pData = p + 5u;
	pOut->tPayload.iLen = iPayloadLen;
	pOut->tPadding.pData = p + 5u + iPayloadLen;
	pOut->tPadding.iLen = iPaddingLen;

	pReader->iPos += iTotalLen;
	if ( pSeqNo != NULL ) {
		*pSeqNo = *pSeqNo + 1u;
	}
	return XSSH_OK;
}

static int xsshCryptoHMACSHA1(const uint8* pKey, size_t iKeyLen, const uint8* pMsg, size_t iMsgLen, uint8 pOut[XSSH_SHA1_SIZE])
{
	uint8 arrKey[64];
	uint8 arrPad[64];
	uint8 arrInner[XSSH_SHA1_SIZE];
	xsha1_ctx tCtx;
	size_t i;

	if ( pOut == NULL || (pKey == NULL && iKeyLen != 0u) || (pMsg == NULL && iMsgLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	memset(arrKey, 0, sizeof(arrKey));
	if ( iKeyLen > sizeof(arrKey) ) {
		xrtSHA1((ptr)pKey, iKeyLen, arrKey);
	} else if ( iKeyLen != 0u ) {
		memcpy(arrKey, pKey, iKeyLen);
	}
	for ( i = 0u; i < sizeof(arrPad); ++i ) {
		arrPad[i] = (uint8)(arrKey[i] ^ 0x36u);
	}
	xrtSHA1Init(&tCtx);
	xrtSHA1Update(&tCtx, arrPad, sizeof(arrPad));
	xrtSHA1Update(&tCtx, (ptr)pMsg, iMsgLen);
	xrtSHA1Final(&tCtx, arrInner);
	for ( i = 0u; i < sizeof(arrPad); ++i ) {
		arrPad[i] = (uint8)(arrKey[i] ^ 0x5cu);
	}
	xrtSHA1Init(&tCtx);
	xrtSHA1Update(&tCtx, arrPad, sizeof(arrPad));
	xrtSHA1Update(&tCtx, arrInner, sizeof(arrInner));
	xrtSHA1Final(&tCtx, pOut);
	xsshSecureZero(arrKey, sizeof(arrKey));
	xsshSecureZero(arrPad, sizeof(arrPad));
	xsshSecureZero(arrInner, sizeof(arrInner));
	return XSSH_OK;
}

static int xsshCryptoHMACSHA256(const uint8* pKey, size_t iKeyLen, const uint8* pMsg, size_t iMsgLen, uint8 pOut[XSSH_SHA256_SIZE])
{
	if ( pOut == NULL || (pKey == NULL && iKeyLen != 0u) || (pMsg == NULL && iMsgLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	/* HMAC-SHA2 同时服务 SSH、TLS 与其他协议，xssh 只做 SSH 错误码适配，算法实现保留在 XRT core。 */
	xrtHMAC_SHA256(pKey, iKeyLen, pMsg, iMsgLen, pOut);
	return XSSH_OK;
}

static int xsshCryptoHMACSHA512(const uint8* pKey, size_t iKeyLen, const uint8* pMsg, size_t iMsgLen, uint8 pOut[XSSH_SHA512_SIZE])
{
	if ( pOut == NULL || (pKey == NULL && iKeyLen != 0u) || (pMsg == NULL && iMsgLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	xrtHMAC_SHA512(pKey, iKeyLen, pMsg, iMsgLen, pOut);
	return XSSH_OK;
}

static int xsshCryptoCheckAEADArgs(uint8* pOut, size_t iOutCap, const uint8* pKey, const uint8* pNonce, size_t iNonceLen,
	const uint8* pAAD, size_t iAADLen, const uint8* pIn, size_t iInLen, size_t iNeedLen)
{
	if ( pOut == NULL || pKey == NULL || pNonce == NULL ||
		(pAAD == NULL && iAADLen != 0u) || (pIn == NULL && iInLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	if ( iNonceLen != XSSH_GCM_NONCE_SIZE ) {
		return XSSH_ERR_INVALID;
	}
	if ( iOutCap < iNeedLen ) {
		return XSSH_ERR_NO_SPACE;
	}
	return XSSH_OK;
}

static int xsshCryptoAES128GCMEncrypt(uint8* pOut, size_t iOutCap, const uint8 pKey[XSSH_AES128_KEY_SIZE],
	const uint8* pNonce, size_t iNonceLen, const uint8* pAAD, size_t iAADLen, const uint8* pPlain, size_t iPlainLen)
{
	int iRet;

	if ( iPlainLen > ((size_t)-1) - XSSH_AEAD_TAG_SIZE ) {
		return XSSH_ERR_OVERFLOW;
	}
	iRet = xsshCryptoCheckAEADArgs(pOut, iOutCap, pKey, pNonce, iNonceLen, pAAD, iAADLen, pPlain, iPlainLen,
		iPlainLen + XSSH_AEAD_TAG_SIZE);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	return xrtAES128GCMEncrypt(pOut, pKey, pNonce, iNonceLen, pAAD, iAADLen, pPlain, iPlainLen) ? XSSH_OK : XSSH_ERR_INVALID;
}

static int xsshCryptoAES128GCMDecrypt(uint8* pOut, size_t iOutCap, const uint8 pKey[XSSH_AES128_KEY_SIZE],
	const uint8* pNonce, size_t iNonceLen, const uint8* pAAD, size_t iAADLen, const uint8* pCipher, size_t iCipherLen)
{
	int iRet;

	if ( iCipherLen < XSSH_AEAD_TAG_SIZE ) {
		return XSSH_ERR_BAD_PACKET;
	}
	iRet = xsshCryptoCheckAEADArgs(pOut, iOutCap, pKey, pNonce, iNonceLen, pAAD, iAADLen, pCipher, iCipherLen,
		iCipherLen - XSSH_AEAD_TAG_SIZE);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	return xrtAES128GCMDecrypt(pOut, pKey, pNonce, iNonceLen, pAAD, iAADLen, pCipher, iCipherLen) ? XSSH_OK : XSSH_ERR_BAD_PACKET;
}

static int xsshCryptoAES256GCMEncrypt(uint8* pOut, size_t iOutCap, const uint8 pKey[XSSH_AES256_KEY_SIZE],
	const uint8* pNonce, size_t iNonceLen, const uint8* pAAD, size_t iAADLen, const uint8* pPlain, size_t iPlainLen)
{
	int iRet;

	if ( iPlainLen > ((size_t)-1) - XSSH_AEAD_TAG_SIZE ) {
		return XSSH_ERR_OVERFLOW;
	}
	iRet = xsshCryptoCheckAEADArgs(pOut, iOutCap, pKey, pNonce, iNonceLen, pAAD, iAADLen, pPlain, iPlainLen,
		iPlainLen + XSSH_AEAD_TAG_SIZE);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	return xrtAES256GCMEncrypt(pOut, pKey, pNonce, iNonceLen, pAAD, iAADLen, pPlain, iPlainLen) ? XSSH_OK : XSSH_ERR_INVALID;
}

static int xsshCryptoAES256GCMDecrypt(uint8* pOut, size_t iOutCap, const uint8 pKey[XSSH_AES256_KEY_SIZE],
	const uint8* pNonce, size_t iNonceLen, const uint8* pAAD, size_t iAADLen, const uint8* pCipher, size_t iCipherLen)
{
	int iRet;

	if ( iCipherLen < XSSH_AEAD_TAG_SIZE ) {
		return XSSH_ERR_BAD_PACKET;
	}
	iRet = xsshCryptoCheckAEADArgs(pOut, iOutCap, pKey, pNonce, iNonceLen, pAAD, iAADLen, pCipher, iCipherLen,
		iCipherLen - XSSH_AEAD_TAG_SIZE);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	return xrtAES256GCMDecrypt(pOut, pKey, pNonce, iNonceLen, pAAD, iAADLen, pCipher, iCipherLen) ? XSSH_OK : XSSH_ERR_BAD_PACKET;
}

static int xsshCryptoChaCha20Poly1305Encrypt(uint8* pOut, size_t iOutCap, const uint8 pKey[XSSH_CHACHA20_KEY_SIZE],
	const uint8 pNonce[XSSH_CHACHA20_NONCE_SIZE], const uint8* pAAD, size_t iAADLen, const uint8* pPlain, size_t iPlainLen)
{
	if ( iPlainLen > ((size_t)-1) - XSSH_AEAD_TAG_SIZE ) {
		return XSSH_ERR_OVERFLOW;
	}
	if ( pOut == NULL || pKey == NULL || pNonce == NULL ||
		(pAAD == NULL && iAADLen != 0u) || (pPlain == NULL && iPlainLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	if ( iOutCap < iPlainLen + XSSH_AEAD_TAG_SIZE ) {
		return XSSH_ERR_NO_SPACE;
	}
	/* 这里封装的是 RFC 8439 AEAD；OpenSSH chacha20-poly1305 packet 还需要单独的 SSH 专属封包层。 */
	return xrtChaCha20Poly1305Encrypt(pOut, pKey, pNonce, pAAD, iAADLen, pPlain, iPlainLen) ? XSSH_OK : XSSH_ERR_INVALID;
}

static int xsshCryptoChaCha20Poly1305Decrypt(uint8* pOut, size_t iOutCap, const uint8 pKey[XSSH_CHACHA20_KEY_SIZE],
	const uint8 pNonce[XSSH_CHACHA20_NONCE_SIZE], const uint8* pAAD, size_t iAADLen, const uint8* pCipher, size_t iCipherLen)
{
	if ( iCipherLen < XSSH_AEAD_TAG_SIZE ) {
		return XSSH_ERR_BAD_PACKET;
	}
	if ( pOut == NULL || pKey == NULL || pNonce == NULL ||
		(pAAD == NULL && iAADLen != 0u) || (pCipher == NULL && iCipherLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	if ( iOutCap < iCipherLen - XSSH_AEAD_TAG_SIZE ) {
		return XSSH_ERR_NO_SPACE;
	}
	return xrtChaCha20Poly1305Decrypt(pOut, pKey, pNonce, pAAD, iAADLen, pCipher, iCipherLen) ? XSSH_OK : XSSH_ERR_BAD_PACKET;
}

static uint64 xsshLoadU64BE(const uint8* pData)
{
	uint64 v = 0u;
	size_t i;

	for ( i = 0u; i < 8u; ++i ) {
		v = (v << 8) | (uint64)pData[i];
	}
	return v;
}

static void xsshStoreU64BE(uint8* pData, uint64 v)
{
	size_t i;

	for ( i = 0u; i < 8u; ++i ) {
		pData[7u - i] = (uint8)(v & 0xffu);
		v >>= 8;
	}
}

static int xsshAESGCMStateInit(xsshaesgcmstate* pState, const uint8* pKey, size_t iKeyLen, const uint8 pInitialIV[XSSH_GCM_NONCE_SIZE])
{
	if ( pState == NULL || pKey == NULL || pInitialIV == NULL ||
		(iKeyLen != XSSH_AES128_KEY_SIZE && iKeyLen != XSSH_AES256_KEY_SIZE) ) {
		return XSSH_ERR_INVALID;
	}
	memset(pState, 0, sizeof(*pState));
	memcpy(pState->arrKey, pKey, iKeyLen);
	pState->iKeyLen = iKeyLen;
	memcpy(pState->arrFixedIV, pInitialIV, sizeof(pState->arrFixedIV));
	pState->iInvocationCounter = xsshLoadU64BE(pInitialIV + sizeof(pState->arrFixedIV));
	return XSSH_OK;
}

static void xsshAESGCMStateClear(xsshaesgcmstate* pState)
{
	if ( pState != NULL ) {
		xsshSecureZero(pState, sizeof(*pState));
	}
}

static void xsshAESGCMMakeNonce(const xsshaesgcmstate* pState, uint8 pNonce[XSSH_GCM_NONCE_SIZE])
{
	memcpy(pNonce, pState->arrFixedIV, sizeof(pState->arrFixedIV));
	xsshStoreU64BE(pNonce + sizeof(pState->arrFixedIV), pState->iInvocationCounter);
}

static bool xsshAESGCMStateCanUse(const xsshaesgcmstate* pState)
{
	if ( pState == NULL || (pState->iKeyLen != XSSH_AES128_KEY_SIZE && pState->iKeyLen != XSSH_AES256_KEY_SIZE) ) {
		return FALSE;
	}
	/* counter 不能回绕；宁可提前失败，也不能重复 nonce。 */
	return (pState->iInvocationCounter != (uint64)0xffffffffffffffffULL) ? TRUE : FALSE;
}

static int xsshPacketCalcAEADPadding(size_t iPayloadLen, uint8* pPaddingLen, uint32* pPacketLen)
{
	size_t iBaseLen;
	size_t iPaddingLen;
	size_t iPacketLen;

	if ( pPaddingLen == NULL || pPacketLen == NULL ) {
		return XSSH_ERR_INVALID;
	}
	if ( iPayloadLen > 0xffffffffu - 1u - 255u ) {
		return XSSH_ERR_OVERFLOW;
	}

	/* RFC 5647: GCM 明文是 padding_length|payload|padding，不包含 4 字节 packet_length。 */
	iBaseLen = 1u + iPayloadLen;
	iPaddingLen = iBaseLen % 16u;
	iPaddingLen = (iPaddingLen == 0u) ? 0u : (16u - iPaddingLen);
	while ( iPaddingLen < XSSH_PACKET_PADDING_MIN ) {
		iPaddingLen += 16u;
	}
	if ( iPaddingLen > 255u ) {
		return XSSH_ERR_OVERFLOW;
	}
	iPacketLen = iBaseLen + iPaddingLen;
	if ( iPacketLen > 0xffffffffu ) {
		return XSSH_ERR_OVERFLOW;
	}

	*pPaddingLen = (uint8)iPaddingLen;
	*pPacketLen = (uint32)iPacketLen;
	return XSSH_OK;
}

static int xsshPacketFillPadding(uint8* pPadding, size_t iPaddingLen, xsshrandomfunc fnRandom, void* pRandomUser)
{
	if ( pPadding == NULL && iPaddingLen != 0u ) {
		return XSSH_ERR_INVALID;
	}
	if ( iPaddingLen == 0u ) {
		return XSSH_OK;
	}
	if ( fnRandom != NULL ) {
		return fnRandom(pRandomUser, pPadding, iPaddingLen);
	}
	xrtRandomBytes(pPadding, iPaddingLen);
	return XSSH_OK;
}

static int xsshPacketWriteAESGCMEx(xsshwriter* pWriter, const void* pPayload, size_t iPayloadLen, xsshaesgcmstate* pState,
	uint32* pSeqNo, xsshrandomfunc fnRandom, void* pRandomUser)
{
	const uint8* pPayloadBytes = (const uint8*)pPayload;
	uint8* pPlain = NULL;
	uint8* pPacket;
	uint8 arrNonce[XSSH_GCM_NONCE_SIZE];
	uint8 iPaddingLen;
	uint32 iPacketLen;
	size_t iTotalLen;
	size_t iStart;
	int iRet;

	if ( pWriter == NULL || (pPayloadBytes == NULL && iPayloadLen != 0u) || !xsshAESGCMStateCanUse(pState) ) {
		return XSSH_ERR_INVALID;
	}
	iRet = xsshPacketCalcAEADPadding(iPayloadLen, &iPaddingLen, &iPacketLen);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	iTotalLen = 4u + (size_t)iPacketLen + XSSH_AEAD_TAG_SIZE;
	iRet = xsshWireReserve(pWriter, iTotalLen);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}

	pPlain = (uint8*)xrtMalloc((size_t)iPacketLen);
	if ( pPlain == NULL ) {
		return XSSH_ERR_NO_SPACE;
	}
	pPlain[0] = iPaddingLen;
	if ( iPayloadLen != 0u ) {
		memcpy(pPlain + 1u, pPayloadBytes, iPayloadLen);
	}
	iRet = xsshPacketFillPadding(pPlain + 1u + iPayloadLen, iPaddingLen, fnRandom, pRandomUser);
	if ( iRet != XSSH_OK ) {
		xsshSecureZero(pPlain, (size_t)iPacketLen);
		xrtFree(pPlain);
		return iRet;
	}

	iStart = pWriter->iLen;
	pPacket = pWriter->pData + iStart;
	pPacket[0] = (uint8)((iPacketLen >> 24) & 0xffu);
	pPacket[1] = (uint8)((iPacketLen >> 16) & 0xffu);
	pPacket[2] = (uint8)((iPacketLen >> 8) & 0xffu);
	pPacket[3] = (uint8)(iPacketLen & 0xffu);
	xsshAESGCMMakeNonce(pState, arrNonce);

	if ( pState->iKeyLen == XSSH_AES128_KEY_SIZE ) {
		iRet = xsshCryptoAES128GCMEncrypt(pPacket + 4u, (size_t)iPacketLen + XSSH_AEAD_TAG_SIZE,
			pState->arrKey, arrNonce, sizeof(arrNonce), pPacket, 4u, pPlain, (size_t)iPacketLen);
	} else {
		iRet = xsshCryptoAES256GCMEncrypt(pPacket + 4u, (size_t)iPacketLen + XSSH_AEAD_TAG_SIZE,
			pState->arrKey, arrNonce, sizeof(arrNonce), pPacket, 4u, pPlain, (size_t)iPacketLen);
	}
	xsshSecureZero(pPlain, (size_t)iPacketLen);
	xrtFree(pPlain);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}

	pWriter->iLen += iTotalLen;
	pState->iInvocationCounter++;
	if ( pSeqNo != NULL ) {
		*pSeqNo = *pSeqNo + 1u;
	}
	return XSSH_OK;
}

static int xsshPacketWriteAESGCM(xsshwriter* pWriter, const void* pPayload, size_t iPayloadLen, xsshaesgcmstate* pState, uint32* pSeqNo)
{
	return xsshPacketWriteAESGCMEx(pWriter, pPayload, iPayloadLen, pState, pSeqNo, NULL, NULL);
}

static int xsshPacketReadAESGCM(xsshreader* pReader, xsshaesgcmstate* pState, uint32 iMaxPacketSize, uint32* pSeqNo,
	xsshpacket* pOut, uint8* pPlainBuf, size_t iPlainCap)
{
	const uint8* pPacket;
	uint8 arrNonce[XSSH_GCM_NONCE_SIZE];
	size_t iLeft;
	size_t iTotalLen;
	uint32 iPacketLen;
	uint8 iPaddingLen;
	size_t iPayloadLen;
	int iRet;

	if ( pReader == NULL || pOut == NULL || pPlainBuf == NULL || pReader->pData == NULL || !xsshAESGCMStateCanUse(pState) ) {
		return XSSH_ERR_INVALID;
	}
	memset(pOut, 0, sizeof(*pOut));

	iLeft = xsshReaderLeft(pReader);
	if ( iLeft < 4u ) {
		return XSSH_WIRE_NEED_MORE;
	}
	pPacket = pReader->pData + pReader->iPos;
	iPacketLen = xsshPacketPeekU32(pPacket);
	if ( iPacketLen < 1u + XSSH_PACKET_PADDING_MIN || (iPacketLen % 16u) != 0u ) {
		return XSSH_ERR_BAD_PACKET;
	}
	if ( iMaxPacketSize == 0u ) {
		iMaxPacketSize = XSSH_PACKET_MAX_DEFAULT;
	}
	if ( iPacketLen > iMaxPacketSize ) {
		return XSSH_ERR_BAD_PACKET;
	}
	iTotalLen = 4u + (size_t)iPacketLen + XSSH_AEAD_TAG_SIZE;
	if ( iLeft < iTotalLen ) {
		return XSSH_WIRE_NEED_MORE;
	}
	if ( iPlainCap < (size_t)iPacketLen ) {
		return XSSH_ERR_NO_SPACE;
	}

	xsshAESGCMMakeNonce(pState, arrNonce);
	if ( pState->iKeyLen == XSSH_AES128_KEY_SIZE ) {
		iRet = xsshCryptoAES128GCMDecrypt(pPlainBuf, iPlainCap, pState->arrKey, arrNonce, sizeof(arrNonce),
			pPacket, 4u, pPacket + 4u, (size_t)iPacketLen + XSSH_AEAD_TAG_SIZE);
	} else {
		iRet = xsshCryptoAES256GCMDecrypt(pPlainBuf, iPlainCap, pState->arrKey, arrNonce, sizeof(arrNonce),
			pPacket, 4u, pPacket + 4u, (size_t)iPacketLen + XSSH_AEAD_TAG_SIZE);
	}
	if ( iRet != XSSH_OK ) {
		xsshSecureZero(pPlainBuf, (size_t)iPacketLen);
		return iRet;
	}

	iPaddingLen = pPlainBuf[0];
	if ( iPaddingLen < XSSH_PACKET_PADDING_MIN || (size_t)iPaddingLen + 1u > (size_t)iPacketLen ) {
		xsshSecureZero(pPlainBuf, (size_t)iPacketLen);
		return XSSH_ERR_BAD_PACKET;
	}
	iPayloadLen = (size_t)iPacketLen - (size_t)iPaddingLen - 1u;

	pOut->iSeqNo = pSeqNo ? *pSeqNo : 0u;
	pOut->iPacketLen = iPacketLen;
	pOut->iPaddingLen = iPaddingLen;
	pOut->tPayload.pData = pPlainBuf + 1u;
	pOut->tPayload.iLen = iPayloadLen;
	pOut->tPadding.pData = pPlainBuf + 1u + iPayloadLen;
	pOut->tPadding.iLen = iPaddingLen;

	pReader->iPos += iTotalLen;
	pState->iInvocationCounter++;
	if ( pSeqNo != NULL ) {
		*pSeqNo = *pSeqNo + 1u;
	}
	return XSSH_OK;
}

static void xsshPacketCodecInit(xsshpacketcodec* pCodec)
{
	if ( pCodec == NULL ) {
		return;
	}
	memset(pCodec, 0, sizeof(*pCodec));
	pCodec->iReadMode = XSSH_PACKET_MODE_PLAIN;
	pCodec->iWriteMode = XSSH_PACKET_MODE_PLAIN;
}

static void xsshPacketCodecClear(xsshpacketcodec* pCodec)
{
	if ( pCodec == NULL ) {
		return;
	}
	xsshAESGCMStateClear(&pCodec->tReadAESGCM);
	xsshAESGCMStateClear(&pCodec->tWriteAESGCM);
	memset(pCodec, 0, sizeof(*pCodec));
}

static int xsshPacketCodecSetReadAESGCM(xsshpacketcodec* pCodec, const uint8* pKey, size_t iKeyLen, const uint8 pInitialIV[XSSH_GCM_NONCE_SIZE])
{
	int iRet;

	if ( pCodec == NULL ) {
		return XSSH_ERR_INVALID;
	}
	iRet = xsshAESGCMStateInit(&pCodec->tReadAESGCM, pKey, iKeyLen, pInitialIV);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	pCodec->iReadMode = XSSH_PACKET_MODE_AESGCM;
	return XSSH_OK;
}

static int xsshPacketCodecSetWriteAESGCM(xsshpacketcodec* pCodec, const uint8* pKey, size_t iKeyLen, const uint8 pInitialIV[XSSH_GCM_NONCE_SIZE])
{
	int iRet;

	if ( pCodec == NULL ) {
		return XSSH_ERR_INVALID;
	}
	iRet = xsshAESGCMStateInit(&pCodec->tWriteAESGCM, pKey, iKeyLen, pInitialIV);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	pCodec->iWriteMode = XSSH_PACKET_MODE_AESGCM;
	return XSSH_OK;
}

static int xsshPacketCodecWriteEx(xsshpacketcodec* pCodec, xsshwriter* pWriter, const void* pPayload, size_t iPayloadLen,
	xsshrandomfunc fnRandom, void* pRandomUser)
{
	if ( pCodec == NULL ) {
		return XSSH_ERR_INVALID;
	}
	if ( pCodec->iWriteMode == XSSH_PACKET_MODE_PLAIN ) {
		return xsshPacketWritePlain(pWriter, pPayload, iPayloadLen, XSSH_PACKET_BLOCK_MIN, &pCodec->iWriteSeqNo);
	}
	if ( pCodec->iWriteMode == XSSH_PACKET_MODE_AESGCM ) {
		return xsshPacketWriteAESGCMEx(pWriter, pPayload, iPayloadLen, &pCodec->tWriteAESGCM,
			&pCodec->iWriteSeqNo, fnRandom, pRandomUser);
	}
	return XSSH_ERR_UNSUPPORTED;
}

static int xsshPacketCodecWrite(xsshpacketcodec* pCodec, xsshwriter* pWriter, const void* pPayload, size_t iPayloadLen)
{
	return xsshPacketCodecWriteEx(pCodec, pWriter, pPayload, iPayloadLen, NULL, NULL);
}

static int xsshPacketCodecRead(xsshpacketcodec* pCodec, xsshreader* pReader, uint32 iMaxPacketSize, xsshpacket* pOut,
	uint8* pPlainBuf, size_t iPlainCap)
{
	if ( pCodec == NULL ) {
		return XSSH_ERR_INVALID;
	}
	if ( pCodec->iReadMode == XSSH_PACKET_MODE_PLAIN ) {
		(void)pPlainBuf;
		(void)iPlainCap;
		return xsshPacketReadPlain(pReader, XSSH_PACKET_BLOCK_MIN, iMaxPacketSize, &pCodec->iReadSeqNo, pOut);
	}
	if ( pCodec->iReadMode == XSSH_PACKET_MODE_AESGCM ) {
		return xsshPacketReadAESGCM(pReader, &pCodec->tReadAESGCM, iMaxPacketSize,
			&pCodec->iReadSeqNo, pOut, pPlainBuf, iPlainCap);
	}
	return XSSH_ERR_UNSUPPORTED;
}

static const char* xsshKexListOrDefault(const char* sList, const char* sDefault)
{
	return sList ? sList : sDefault;
}

static int xsshKexInitValidateList(const char* sList, size_t* pLen)
{
	size_t iLen;

	if ( sList == NULL || pLen == NULL ) {
		return XSSH_ERR_INVALID;
	}
	iLen = strlen(sList);
	if ( !xsshWireNameListIsValid(sList, iLen) ) {
		return XSSH_ERR_INVALID;
	}
	*pLen = iLen;
	return XSSH_OK;
}

static int xsshKexInitWrite(xsshwriter* pWriter, const xsshkexinitdesc* pDesc)
{
	xsshkexinitdesc tDefaultDesc;
	uint8 arrZeroCookie[XSSH_KEX_COOKIE_SIZE];
	const uint8* pCookie;
	const char* arrLists[10];
	size_t arrLens[10];
	size_t i;
	size_t iTotalLen;
	int iRet;

	if ( pWriter == NULL ) {
		return XSSH_ERR_INVALID;
	}
	if ( pDesc == NULL ) {
		xsshKexInitDescInit(&tDefaultDesc);
		pDesc = &tDefaultDesc;
	}
	memset(arrZeroCookie, 0, sizeof(arrZeroCookie));
	pCookie = pDesc->pCookie ? pDesc->pCookie : arrZeroCookie;

	arrLists[0] = xsshKexListOrDefault(pDesc->sKexAlgorithms, XSSH_ALG_KEX_DEFAULT);
	arrLists[1] = xsshKexListOrDefault(pDesc->sServerHostKeyAlgorithms, XSSH_ALG_HOSTKEY_DEFAULT);
	arrLists[2] = xsshKexListOrDefault(pDesc->sEncryptionClientToServer, XSSH_ALG_CIPHER_DEFAULT);
	arrLists[3] = xsshKexListOrDefault(pDesc->sEncryptionServerToClient, XSSH_ALG_CIPHER_DEFAULT);
	arrLists[4] = xsshKexListOrDefault(pDesc->sMacClientToServer, XSSH_ALG_MAC_DEFAULT);
	arrLists[5] = xsshKexListOrDefault(pDesc->sMacServerToClient, XSSH_ALG_MAC_DEFAULT);
	arrLists[6] = xsshKexListOrDefault(pDesc->sCompressionClientToServer, XSSH_ALG_COMP_DEFAULT);
	arrLists[7] = xsshKexListOrDefault(pDesc->sCompressionServerToClient, XSSH_ALG_COMP_DEFAULT);
	arrLists[8] = xsshKexListOrDefault(pDesc->sLanguagesClientToServer, "");
	arrLists[9] = xsshKexListOrDefault(pDesc->sLanguagesServerToClient, "");

	iTotalLen = 1u + XSSH_KEX_COOKIE_SIZE + 1u + 4u;
	for ( i = 0u; i < 10u; ++i ) {
		iRet = xsshKexInitValidateList(arrLists[i], &arrLens[i]);
		if ( iRet != XSSH_OK ) {
			return iRet;
		}
		iTotalLen += 4u + arrLens[i];
	}
	iRet = xsshWireReserve(pWriter, iTotalLen);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}

	xsshWireWriteU8(pWriter, XSSH_MSG_KEXINIT);
	xsshWireWriteBytes(pWriter, pCookie, XSSH_KEX_COOKIE_SIZE);
	for ( i = 0u; i < 10u; ++i ) {
		xsshWireWriteString(pWriter, arrLists[i], arrLens[i]);
	}
	xsshWireWriteBool(pWriter, pDesc->bFirstKexPacketFollows);
	xsshWireWriteU32(pWriter, 0u);
	return XSSH_OK;
}

static int xsshKexInitReadNameList(xsshreader* pReader, xsshslice* pOut)
{
	int iRet = xsshWireReadString(pReader, pOut);

	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( !xsshWireNameListIsValid((const char*)pOut->pData, pOut->iLen) ) {
		return XSSH_ERR_BAD_PACKET;
	}
	return XSSH_OK;
}

static int xsshKexInitRead(const void* pPayload, size_t iPayloadLen, xsshkexinit* pOut)
{
	xsshreader rd;
	xsshslice sCookie;
	uint8 iMsg = 0u;
	int iRet;

	if ( pOut == NULL || (pPayload == NULL && iPayloadLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	memset(pOut, 0, sizeof(*pOut));
	xsshReaderInit(&rd, pPayload, iPayloadLen);

	iRet = xsshWireReadU8(&rd, &iMsg);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( iMsg != XSSH_MSG_KEXINIT ) {
		return XSSH_ERR_BAD_PACKET;
	}
	iRet = xsshWireReadBytes(&rd, XSSH_KEX_COOKIE_SIZE, &sCookie);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	memcpy(pOut->arrCookie, sCookie.pData, XSSH_KEX_COOKIE_SIZE);

	if ( (iRet = xsshKexInitReadNameList(&rd, &pOut->tKexAlgorithms)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshKexInitReadNameList(&rd, &pOut->tServerHostKeyAlgorithms)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshKexInitReadNameList(&rd, &pOut->tEncryptionClientToServer)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshKexInitReadNameList(&rd, &pOut->tEncryptionServerToClient)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshKexInitReadNameList(&rd, &pOut->tMacClientToServer)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshKexInitReadNameList(&rd, &pOut->tMacServerToClient)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshKexInitReadNameList(&rd, &pOut->tCompressionClientToServer)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshKexInitReadNameList(&rd, &pOut->tCompressionServerToClient)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshKexInitReadNameList(&rd, &pOut->tLanguagesClientToServer)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshKexInitReadNameList(&rd, &pOut->tLanguagesServerToClient)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadBool(&rd, &pOut->bFirstKexPacketFollows)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadU32(&rd, &pOut->iReserved)) != XSSH_OK ) return iRet;
	if ( pOut->iReserved != 0u || xsshReaderLeft(&rd) != 0u ) {
		return XSSH_ERR_BAD_PACKET;
	}
	return XSSH_OK;
}

static bool xsshSliceIsValid(xsshslice tSlice)
{
	return (tSlice.pData != NULL || tSlice.iLen == 0u) ? TRUE : FALSE;
}

static bool xsshKexSliceEqText(xsshslice tSlice, const char* sText)
{
	size_t iLen;

	if ( sText == NULL || !xsshSliceIsValid(tSlice) ) {
		return FALSE;
	}
	iLen = strlen(sText);
	return (tSlice.iLen == iLen && memcmp(tSlice.pData, sText, iLen) == 0) ? TRUE : FALSE;
}

static int xsshKexNameListFirstName(xsshslice tList, xsshslice* pOut)
{
	size_t i;

	if ( pOut == NULL || !xsshSliceIsValid(tList) ||
		!xsshWireNameListIsValid((const char*)tList.pData, tList.iLen) ) {
		return XSSH_ERR_INVALID;
	}
	pOut->pData = NULL;
	pOut->iLen = 0u;
	if ( tList.iLen == 0u ) {
		return XSSH_ERR_UNSUPPORTED;
	}
	for ( i = 0u; i < tList.iLen; ++i ) {
		if ( tList.pData[i] == ',' ) {
			break;
		}
	}
	pOut->pData = tList.pData;
	pOut->iLen = i;
	return (i != 0u) ? XSSH_OK : XSSH_ERR_UNSUPPORTED;
}

static bool xsshKexNameListFirstEquals(xsshslice tList, xsshslice tSelected)
{
	xsshslice tFirst;

	if ( !xsshSliceIsValid(tSelected) ||
		xsshKexNameListFirstName(tList, &tFirst) != XSSH_OK ) {
		return FALSE;
	}
	return (tFirst.iLen == tSelected.iLen &&
		memcmp(tFirst.pData, tSelected.pData, tSelected.iLen) == 0) ? TRUE : FALSE;
}

static bool xsshKexShouldSkipFirstKexFollower(const xsshkexinit* pPeerKex, const xsshkexnegotiation* pNeg)
{
	if ( pPeerKex == NULL || pNeg == NULL || !pPeerKex->bFirstKexPacketFollows ) {
		return FALSE;
	}
	/*
	 * RFC 4253 的 first_kex_packet_follows 只允许跳过“猜错”的第一包。
	 * 对当前实现的 curve25519/hostkey KEX 来说，猜测是否正确由 peer 列表首项
	 * 是否等于最终协商出的 KEX 算法和 host key 算法决定；猜对时下一包就是合法 KEX 回复。
	 */
	return (!xsshKexNameListFirstEquals(pPeerKex->tKexAlgorithms, pNeg->tKexAlgorithm) ||
		!xsshKexNameListFirstEquals(pPeerKex->tServerHostKeyAlgorithms, pNeg->tServerHostKeyAlgorithm)) ? TRUE : FALSE;
}

static bool xsshKexCipherIsAEAD(xsshslice tCipher)
{
	return (xsshKexSliceEqText(tCipher, "aes128-gcm@openssh.com") ||
		xsshKexSliceEqText(tCipher, "aes256-gcm@openssh.com") ||
		xsshKexSliceEqText(tCipher, "chacha20-poly1305@openssh.com")) ? TRUE : FALSE;
}

static int xsshKexSelectNameList(xsshslice tClientList, xsshslice tServerList, xsshslice* pOut)
{
	if ( pOut == NULL || !xsshSliceIsValid(tClientList) || !xsshSliceIsValid(tServerList) ) {
		return XSSH_ERR_INVALID;
	}
	return xsshWireNameListFirstMatch((const char*)tClientList.pData, tClientList.iLen,
		(const char*)tServerList.pData, tServerList.iLen, pOut);
}

static int xsshKexNegotiate(const xsshkexinit* pClient, const xsshkexinit* pServer, xsshkexnegotiation* pOut)
{
	int iRet;

	if ( pClient == NULL || pServer == NULL || pOut == NULL ) {
		return XSSH_ERR_INVALID;
	}
	memset(pOut, 0, sizeof(*pOut));
	if ( (iRet = xsshKexSelectNameList(pClient->tKexAlgorithms, pServer->tKexAlgorithms, &pOut->tKexAlgorithm)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshKexSelectNameList(pClient->tServerHostKeyAlgorithms, pServer->tServerHostKeyAlgorithms, &pOut->tServerHostKeyAlgorithm)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshKexSelectNameList(pClient->tEncryptionClientToServer, pServer->tEncryptionClientToServer, &pOut->tCipherClientToServer)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshKexSelectNameList(pClient->tEncryptionServerToClient, pServer->tEncryptionServerToClient, &pOut->tCipherServerToClient)) != XSSH_OK ) return iRet;
	if ( !xsshKexCipherIsAEAD(pOut->tCipherClientToServer) ) {
		if ( (iRet = xsshKexSelectNameList(pClient->tMacClientToServer, pServer->tMacClientToServer, &pOut->tMacClientToServer)) != XSSH_OK ) return iRet;
	}
	if ( !xsshKexCipherIsAEAD(pOut->tCipherServerToClient) ) {
		if ( (iRet = xsshKexSelectNameList(pClient->tMacServerToClient, pServer->tMacServerToClient, &pOut->tMacServerToClient)) != XSSH_OK ) return iRet;
	}
	if ( (iRet = xsshKexSelectNameList(pClient->tCompressionClientToServer, pServer->tCompressionClientToServer, &pOut->tCompressionClientToServer)) != XSSH_OK ) return iRet;
	return xsshKexSelectNameList(pClient->tCompressionServerToClient, pServer->tCompressionServerToClient, &pOut->tCompressionServerToClient);
}

static int xsshKexHashInputWriteCurve25519(xsshwriter* pWriter, const xsshkexhashdesc* pDesc)
{
	int iRet;

	if ( pWriter == NULL || pDesc == NULL ||
		!xsshSliceIsValid(pDesc->tClientVersion) ||
		!xsshSliceIsValid(pDesc->tServerVersion) ||
		!xsshSliceIsValid(pDesc->tClientKexInit) ||
		!xsshSliceIsValid(pDesc->tServerKexInit) ||
		!xsshSliceIsValid(pDesc->tServerHostKey) ||
		!xsshSliceIsValid(pDesc->tClientEphemeral) ||
		!xsshSliceIsValid(pDesc->tServerEphemeral) ||
		!xsshSliceIsValid(pDesc->tSharedSecret) ) {
		return XSSH_ERR_INVALID;
	}

	/*
		SSH 的 exchange hash 不是简单拼接明文。
		V_C/V_S/I_C/I_S/K_S/Q_C/Q_S 都按 SSH string 编码，
		共享秘密 K 按 mpint 编码；这里先覆盖 curve25519/ecdh 这类
		“临时公钥为 string”的 KEX 形态。
	*/
	if ( (iRet = xsshWireWriteString(pWriter, pDesc->tClientVersion.pData, pDesc->tClientVersion.iLen)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteString(pWriter, pDesc->tServerVersion.pData, pDesc->tServerVersion.iLen)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteString(pWriter, pDesc->tClientKexInit.pData, pDesc->tClientKexInit.iLen)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteString(pWriter, pDesc->tServerKexInit.pData, pDesc->tServerKexInit.iLen)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteString(pWriter, pDesc->tServerHostKey.pData, pDesc->tServerHostKey.iLen)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteString(pWriter, pDesc->tClientEphemeral.pData, pDesc->tClientEphemeral.iLen)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteString(pWriter, pDesc->tServerEphemeral.pData, pDesc->tServerEphemeral.iLen)) != XSSH_OK ) return iRet;
	return xsshWireWriteMpintPositive(pWriter, pDesc->tSharedSecret.pData, pDesc->tSharedSecret.iLen);
}

static void xsshKexSHA256UpdateU32(xsha256_ctx* pCtx, uint32 iValue)
{
	uint8 arrLen[4];

	arrLen[0] = (uint8)((iValue >> 24) & 0xffu);
	arrLen[1] = (uint8)((iValue >> 16) & 0xffu);
	arrLen[2] = (uint8)((iValue >> 8) & 0xffu);
	arrLen[3] = (uint8)(iValue & 0xffu);
	xrtSHA256Update(pCtx, arrLen, sizeof(arrLen));
}

static int xsshKexSHA256UpdateString(xsha256_ctx* pCtx, xsshslice tSlice)
{
	if ( pCtx == NULL || !xsshSliceIsValid(tSlice) || tSlice.iLen > 0xffffffffu ) {
		return XSSH_ERR_INVALID;
	}
	xsshKexSHA256UpdateU32(pCtx, (uint32)tSlice.iLen);
	if ( tSlice.iLen != 0u ) {
		xrtSHA256Update(pCtx, (ptr)tSlice.pData, tSlice.iLen);
	}
	return XSSH_OK;
}

static int xsshKexSHA256UpdateMpintPositive(xsha256_ctx* pCtx, xsshslice tValue)
{
	const uint8* p;
	size_t iLen;
	size_t iOff = 0u;
	bool bNeedZero;
	uint8 iZero = 0u;

	if ( pCtx == NULL || !xsshSliceIsValid(tValue) ) {
		return XSSH_ERR_INVALID;
	}
	p = tValue.pData;
	iLen = tValue.iLen;
	while ( iOff < iLen && p[iOff] == 0u ) {
		iOff++;
	}
	if ( iOff == iLen ) {
		xsshKexSHA256UpdateU32(pCtx, 0u);
		return XSSH_OK;
	}
	p += iOff;
	iLen -= iOff;
	bNeedZero = ((p[0] & 0x80u) != 0u) ? TRUE : FALSE;
	if ( iLen + (bNeedZero ? 1u : 0u) > 0xffffffffu ) {
		return XSSH_ERR_OVERFLOW;
	}
	xsshKexSHA256UpdateU32(pCtx, (uint32)(iLen + (bNeedZero ? 1u : 0u)));
	if ( bNeedZero ) {
		xrtSHA256Update(pCtx, &iZero, 1u);
	}
	xrtSHA256Update(pCtx, (ptr)p, iLen);
	return XSSH_OK;
}

static int xsshKexHashSHA256Curve25519(const xsshkexhashdesc* pDesc, uint8 pOut[XSSH_SHA256_SIZE])
{
	xsha256_ctx tCtx;
	int iRet;

	if ( pDesc == NULL || pOut == NULL ) {
		return XSSH_ERR_INVALID;
	}
	xrtSHA256Init(&tCtx);
	if ( (iRet = xsshKexSHA256UpdateString(&tCtx, pDesc->tClientVersion)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshKexSHA256UpdateString(&tCtx, pDesc->tServerVersion)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshKexSHA256UpdateString(&tCtx, pDesc->tClientKexInit)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshKexSHA256UpdateString(&tCtx, pDesc->tServerKexInit)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshKexSHA256UpdateString(&tCtx, pDesc->tServerHostKey)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshKexSHA256UpdateString(&tCtx, pDesc->tClientEphemeral)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshKexSHA256UpdateString(&tCtx, pDesc->tServerEphemeral)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshKexSHA256UpdateMpintPositive(&tCtx, pDesc->tSharedSecret)) != XSSH_OK ) return iRet;
	xrtSHA256Final(&tCtx, pOut);
	return XSSH_OK;
}

static int xsshKexDeriveKeySHA256(uint8* pOut, size_t iOutLen, xsshslice tSharedSecret, const uint8 pHash[XSSH_SHA256_SIZE], const uint8 pSessionId[XSSH_SHA256_SIZE], uint8 iLetter)
{
	size_t iDone = 0u;
	uint8 arrDigest[XSSH_SHA256_SIZE];
	xsha256_ctx tCtx;
	int iRet;

	if ( (pOut == NULL && iOutLen != 0u) || !xsshSliceIsValid(tSharedSecret) || pHash == NULL || pSessionId == NULL ||
		iLetter < (uint8)'A' || iLetter > (uint8)'F' ) {
		return XSSH_ERR_INVALID;
	}
	while ( iDone < iOutLen ) {
		size_t iCopy;

		xrtSHA256Init(&tCtx);
		if ( (iRet = xsshKexSHA256UpdateMpintPositive(&tCtx, tSharedSecret)) != XSSH_OK ) {
			memset(arrDigest, 0, sizeof(arrDigest));
			return iRet;
		}
		xrtSHA256Update(&tCtx, (ptr)pHash, XSSH_SHA256_SIZE);
		if ( iDone == 0u ) {
			xrtSHA256Update(&tCtx, &iLetter, 1u);
			xrtSHA256Update(&tCtx, (ptr)pSessionId, XSSH_SHA256_SIZE);
		} else {
			xrtSHA256Update(&tCtx, pOut, iDone);
		}
		xrtSHA256Final(&tCtx, arrDigest);

		iCopy = iOutLen - iDone;
		if ( iCopy > XSSH_SHA256_SIZE ) {
			iCopy = XSSH_SHA256_SIZE;
		}
		memcpy(pOut + iDone, arrDigest, iCopy);
		iDone += iCopy;
	}
	memset(arrDigest, 0, sizeof(arrDigest));
	return XSSH_OK;
}

static bool xsshMemIsAllZero(const uint8* pData, size_t iLen)
{
	size_t i;
	uint8 iAcc = 0u;

	if ( pData == NULL && iLen != 0u ) {
		return FALSE;
	}
	for ( i = 0u; i < iLen; ++i ) {
		iAcc |= pData[i];
	}
	return (iAcc == 0u) ? TRUE : FALSE;
}

static int xsshKexCurve25519Keypair(uint8 pPrivKey[32], uint8 pPubKey[32])
{
	if ( pPrivKey == NULL || pPubKey == NULL ) {
		return XSSH_ERR_INVALID;
	}
	xrtX25519Keypair(pPrivKey, pPubKey);
	return XSSH_OK;
}

static int xsshKexCurve25519SharedSecret(uint8 pOut[32], const uint8 pPrivKey[32], const uint8 pPeerPubKey[32])
{
	if ( pOut == NULL || pPrivKey == NULL || pPeerPubKey == NULL ) {
		return XSSH_ERR_INVALID;
	}
	xrtX25519SharedSecret(pOut, pPrivKey, pPeerPubKey);
	/* RFC 7748 建议拒绝全 0 共享秘密，避免小阶点导致的无效密钥交换。 */
	if ( xsshMemIsAllZero(pOut, 32u) ) {
		memset(pOut, 0, 32u);
		return XSSH_ERR_BAD_PACKET;
	}
	return XSSH_OK;
}

static int xsshKexECDHInitWrite(xsshwriter* pWriter, const void* pClientPublic, size_t iClientPublicLen)
{
	int iRet;

	if ( pWriter == NULL || (pClientPublic == NULL && iClientPublicLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	iRet = xsshWireReserve(pWriter, 1u + 4u + iClientPublicLen);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( (iRet = xsshWireWriteU8(pWriter, XSSH_MSG_KEX_ECDH_INIT)) != XSSH_OK ) return iRet;
	return xsshWireWriteString(pWriter, pClientPublic, iClientPublicLen);
}

static int xsshKexECDHInitRead(const void* pPayload, size_t iPayloadLen, xsshkexecdhinit* pOut)
{
	xsshreader rd;
	uint8 iMsg = 0u;
	int iRet;

	if ( pOut == NULL || (pPayload == NULL && iPayloadLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	memset(pOut, 0, sizeof(*pOut));
	xsshReaderInit(&rd, pPayload, iPayloadLen);
	if ( (iRet = xsshWireReadU8(&rd, &iMsg)) != XSSH_OK ) return iRet;
	if ( iMsg != XSSH_MSG_KEX_ECDH_INIT ) {
		return XSSH_ERR_BAD_PACKET;
	}
	if ( (iRet = xsshWireReadString(&rd, &pOut->tClientPublic)) != XSSH_OK ) return iRet;
	if ( xsshReaderLeft(&rd) != 0u ) {
		return XSSH_ERR_BAD_PACKET;
	}
	return XSSH_OK;
}

static int xsshKexECDHReplyWrite(xsshwriter* pWriter, xsshslice tServerHostKey, xsshslice tServerPublic, xsshslice tSignature)
{
	int iRet;

	if ( pWriter == NULL || !xsshSliceIsValid(tServerHostKey) || !xsshSliceIsValid(tServerPublic) || !xsshSliceIsValid(tSignature) ) {
		return XSSH_ERR_INVALID;
	}
	iRet = xsshWireReserve(pWriter, 1u + 4u + tServerHostKey.iLen + 4u + tServerPublic.iLen + 4u + tSignature.iLen);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( (iRet = xsshWireWriteU8(pWriter, XSSH_MSG_KEX_ECDH_REPLY)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteString(pWriter, tServerHostKey.pData, tServerHostKey.iLen)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteString(pWriter, tServerPublic.pData, tServerPublic.iLen)) != XSSH_OK ) return iRet;
	return xsshWireWriteString(pWriter, tSignature.pData, tSignature.iLen);
}

static int xsshKexECDHReplyRead(const void* pPayload, size_t iPayloadLen, xsshkexecdhreply* pOut)
{
	xsshreader rd;
	uint8 iMsg = 0u;
	int iRet;

	if ( pOut == NULL || (pPayload == NULL && iPayloadLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	memset(pOut, 0, sizeof(*pOut));
	xsshReaderInit(&rd, pPayload, iPayloadLen);
	if ( (iRet = xsshWireReadU8(&rd, &iMsg)) != XSSH_OK ) return iRet;
	if ( iMsg != XSSH_MSG_KEX_ECDH_REPLY ) {
		return XSSH_ERR_BAD_PACKET;
	}
	if ( (iRet = xsshWireReadString(&rd, &pOut->tServerHostKey)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadString(&rd, &pOut->tServerPublic)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadString(&rd, &pOut->tSignature)) != XSSH_OK ) return iRet;
	if ( xsshReaderLeft(&rd) != 0u ) {
		return XSSH_ERR_BAD_PACKET;
	}
	return XSSH_OK;
}

static bool xsshSliceEqualText(xsshslice tSlice, const char* sText)
{
	size_t iLen;

	if ( sText == NULL || !xsshSliceIsValid(tSlice) ) {
		return FALSE;
	}
	iLen = strlen(sText);
	return (tSlice.iLen == iLen && memcmp(tSlice.pData, sText, iLen) == 0) ? TRUE : FALSE;
}

static int xsshPublicKeyReadEd25519(const void* pBlob, size_t iBlobLen, xsshpublickey* pOut)
{
	xsshreader rd;
	int iRet;

	if ( pOut == NULL || (pBlob == NULL && iBlobLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	memset(pOut, 0, sizeof(*pOut));
	xsshReaderInit(&rd, pBlob, iBlobLen);
	if ( (iRet = xsshWireReadString(&rd, &pOut->tAlgorithm)) != XSSH_OK ) return iRet;
	if ( !xsshSliceEqualText(pOut->tAlgorithm, "ssh-ed25519") ) {
		return XSSH_ERR_UNSUPPORTED;
	}
	if ( (iRet = xsshWireReadString(&rd, &pOut->tKeyData)) != XSSH_OK ) return iRet;
	if ( pOut->tKeyData.iLen != 32u || xsshReaderLeft(&rd) != 0u ) {
		return XSSH_ERR_BAD_PACKET;
	}
	return XSSH_OK;
}

static int xsshPublicKeyWriteEd25519(xsshwriter* pWriter, const uint8 pPubKey[32])
{
	int iRet;

	if ( pWriter == NULL || pPubKey == NULL ) {
		return XSSH_ERR_INVALID;
	}
	iRet = xsshWireReserve(pWriter, 4u + strlen("ssh-ed25519") + 4u + 32u);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( (iRet = xsshWireWriteString(pWriter, "ssh-ed25519", strlen("ssh-ed25519"))) != XSSH_OK ) return iRet;
	return xsshWireWriteString(pWriter, pPubKey, 32u);
}

static int xsshSignatureRead(const void* pBlob, size_t iBlobLen, xsshsignature* pOut)
{
	xsshreader rd;
	int iRet;

	if ( pOut == NULL || (pBlob == NULL && iBlobLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	memset(pOut, 0, sizeof(*pOut));
	xsshReaderInit(&rd, pBlob, iBlobLen);
	if ( (iRet = xsshWireReadString(&rd, &pOut->tAlgorithm)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadString(&rd, &pOut->tSignature)) != XSSH_OK ) return iRet;
	if ( xsshReaderLeft(&rd) != 0u ) {
		return XSSH_ERR_BAD_PACKET;
	}
	return XSSH_OK;
}

static int xsshSignatureWrite(xsshwriter* pWriter, const char* sAlgorithm, const void* pSig, size_t iSigLen)
{
	int iRet;

	if ( pWriter == NULL || sAlgorithm == NULL || (pSig == NULL && iSigLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	iRet = xsshWireReserve(pWriter, 4u + strlen(sAlgorithm) + 4u + iSigLen);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( (iRet = xsshWireWriteString(pWriter, sAlgorithm, strlen(sAlgorithm))) != XSSH_OK ) return iRet;
	return xsshWireWriteString(pWriter, pSig, iSigLen);
}

static int xsshHostKeyVerifyEd25519(const xsshpublickey* pKey, const xsshsignature* pSig, const uint8* pMsg, size_t iMsgLen)
{
	if ( pKey == NULL || pSig == NULL || (pMsg == NULL && iMsgLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	if ( !xsshSliceEqualText(pKey->tAlgorithm, "ssh-ed25519") || !xsshSliceEqualText(pSig->tAlgorithm, "ssh-ed25519") ) {
		return XSSH_ERR_UNSUPPORTED;
	}
	if ( pKey->tKeyData.iLen != 32u || pSig->tSignature.iLen != 64u ) {
		return XSSH_ERR_BAD_PACKET;
	}
	return xrtEd25519Verify(pMsg, iMsgLen, pSig->tSignature.pData, pKey->tKeyData.pData) ? XSSH_OK : XSSH_ERR_BAD_PACKET;
}

static int xsshMsgReadId(xsshreader* pReader, uint8 iExpected)
{
	uint8 iMsg = 0u;
	int iRet;

	if ( (iRet = xsshWireReadU8(pReader, &iMsg)) != XSSH_OK ) {
		return iRet;
	}
	return (iMsg == iExpected) ? XSSH_OK : XSSH_ERR_BAD_PACKET;
}

static int xsshMsgReadNoTrailing(xsshreader* pReader)
{
	return (xsshReaderLeft(pReader) == 0u) ? XSSH_OK : XSSH_ERR_BAD_PACKET;
}

static int xsshMsgWriteNewKeys(xsshwriter* pWriter)
{
	int iRet = xsshWireReserve(pWriter, 1u);

	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	return xsshWireWriteU8(pWriter, XSSH_MSG_NEWKEYS);
}

static int xsshMsgReadNewKeys(const void* pPayload, size_t iPayloadLen)
{
	xsshreader rd;
	int iRet;

	if ( pPayload == NULL && iPayloadLen != 0u ) {
		return XSSH_ERR_INVALID;
	}
	xsshReaderInit(&rd, pPayload, iPayloadLen);
	if ( (iRet = xsshMsgReadId(&rd, XSSH_MSG_NEWKEYS)) != XSSH_OK ) {
		return iRet;
	}
	return xsshMsgReadNoTrailing(&rd);
}

static int xsshMsgWriteService(xsshwriter* pWriter, uint8 iMsg, const char* sServiceName)
{
	size_t iNameLen;
	int iRet;

	if ( pWriter == NULL || sServiceName == NULL ||
		(iMsg != XSSH_MSG_SERVICE_REQUEST && iMsg != XSSH_MSG_SERVICE_ACCEPT) ) {
		return XSSH_ERR_INVALID;
	}
	iNameLen = strlen(sServiceName);
	iRet = xsshWireReserve(pWriter, 1u + 4u + iNameLen);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( (iRet = xsshWireWriteU8(pWriter, iMsg)) != XSSH_OK ) return iRet;
	return xsshWireWriteString(pWriter, sServiceName, iNameLen);
}

static int xsshMsgWriteServiceRequest(xsshwriter* pWriter, const char* sServiceName)
{
	return xsshMsgWriteService(pWriter, XSSH_MSG_SERVICE_REQUEST, sServiceName);
}

static int xsshMsgWriteServiceAccept(xsshwriter* pWriter, const char* sServiceName)
{
	return xsshMsgWriteService(pWriter, XSSH_MSG_SERVICE_ACCEPT, sServiceName);
}

static int xsshMsgReadService(const void* pPayload, size_t iPayloadLen, uint8 iExpected, xsshservicemsg* pOut)
{
	xsshreader rd;
	int iRet;

	if ( pOut == NULL || (pPayload == NULL && iPayloadLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	memset(pOut, 0, sizeof(*pOut));
	xsshReaderInit(&rd, pPayload, iPayloadLen);
	if ( (iRet = xsshMsgReadId(&rd, iExpected)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadString(&rd, &pOut->tServiceName)) != XSSH_OK ) return iRet;
	return xsshMsgReadNoTrailing(&rd);
}

static int xsshMsgReadServiceRequest(const void* pPayload, size_t iPayloadLen, xsshservicemsg* pOut)
{
	return xsshMsgReadService(pPayload, iPayloadLen, XSSH_MSG_SERVICE_REQUEST, pOut);
}

static int xsshMsgReadServiceAccept(const void* pPayload, size_t iPayloadLen, xsshservicemsg* pOut)
{
	return xsshMsgReadService(pPayload, iPayloadLen, XSSH_MSG_SERVICE_ACCEPT, pOut);
}

static int xsshMsgWriteGlobalRequestStart(xsshwriter* pWriter, const char* sRequestName, bool bWantReply, size_t iExtraLen)
{
	size_t iNameLen;
	int iRet;

	if ( pWriter == NULL || sRequestName == NULL ) {
		return XSSH_ERR_INVALID;
	}
	iNameLen = strlen(sRequestName);
	iRet = xsshWireReserve(pWriter, 1u + 4u + iNameLen + 1u + iExtraLen);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( (iRet = xsshWireWriteU8(pWriter, XSSH_MSG_GLOBAL_REQUEST)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteString(pWriter, sRequestName, iNameLen)) != XSSH_OK ) return iRet;
	return xsshWireWriteBool(pWriter, bWantReply);
}

static int xsshMsgWriteTcpipForward(xsshwriter* pWriter, const char* sBindAddress, uint32 iBindPort, bool bWantReply)
{
	size_t iBindLen;
	int iRet;

	if ( sBindAddress == NULL ) {
		return XSSH_ERR_INVALID;
	}
	iBindLen = strlen(sBindAddress);
	iRet = xsshMsgWriteGlobalRequestStart(pWriter, "tcpip-forward", bWantReply, 4u + iBindLen + 4u);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( (iRet = xsshWireWriteString(pWriter, sBindAddress, iBindLen)) != XSSH_OK ) return iRet;
	return xsshWireWriteU32(pWriter, iBindPort);
}

static int xsshMsgWriteCancelTcpipForward(xsshwriter* pWriter, const char* sBindAddress, uint32 iBindPort, bool bWantReply)
{
	size_t iBindLen;
	int iRet;

	if ( sBindAddress == NULL ) {
		return XSSH_ERR_INVALID;
	}
	iBindLen = strlen(sBindAddress);
	iRet = xsshMsgWriteGlobalRequestStart(pWriter, "cancel-tcpip-forward", bWantReply, 4u + iBindLen + 4u);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( (iRet = xsshWireWriteString(pWriter, sBindAddress, iBindLen)) != XSSH_OK ) return iRet;
	return xsshWireWriteU32(pWriter, iBindPort);
}

static int xsshMsgWriteKeepalive(xsshwriter* pWriter, bool bWantReply)
{
	return xsshMsgWriteGlobalRequestStart(pWriter, "keepalive@openssh.com", bWantReply, 0u);
}

static int xsshMsgReadGlobalRequest(const void* pPayload, size_t iPayloadLen, xsshglobalrequestmsg* pOut)
{
	xsshreader rd;
	int iRet;

	if ( pOut == NULL || (pPayload == NULL && iPayloadLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	memset(pOut, 0, sizeof(*pOut));
	xsshReaderInit(&rd, pPayload, iPayloadLen);
	if ( (iRet = xsshMsgReadId(&rd, XSSH_MSG_GLOBAL_REQUEST)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadString(&rd, &pOut->tRequestName)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadBool(&rd, &pOut->bWantReply)) != XSSH_OK ) return iRet;
	pOut->tPayload.pData = rd.pData + rd.iPos;
	pOut->tPayload.iLen = xsshReaderLeft(&rd);
	rd.iPos = rd.iSize;
	return XSSH_OK;
}

static int xsshMsgReadTcpipForwardPayload(const xsshglobalrequestmsg* pMsg, xsshglobaltcpipforwardmsg* pOut)
{
	xsshreader rd;
	int iRet;

	if ( pMsg == NULL || pOut == NULL ||
		(!xsshSliceEqualText(pMsg->tRequestName, "tcpip-forward") && !xsshSliceEqualText(pMsg->tRequestName, "cancel-tcpip-forward")) ) {
		return XSSH_ERR_INVALID;
	}
	memset(pOut, 0, sizeof(*pOut));
	xsshReaderInit(&rd, pMsg->tPayload.pData, pMsg->tPayload.iLen);
	if ( (iRet = xsshWireReadString(&rd, &pOut->tBindAddress)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadU32(&rd, &pOut->iBindPort)) != XSSH_OK ) return iRet;
	return xsshMsgReadNoTrailing(&rd);
}

static int xsshMsgWriteRequestSuccess(xsshwriter* pWriter, bool bHasBoundPort, uint32 iBoundPort)
{
	int iRet = xsshWireReserve(pWriter, 1u + (bHasBoundPort ? 4u : 0u));

	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( (iRet = xsshWireWriteU8(pWriter, XSSH_MSG_REQUEST_SUCCESS)) != XSSH_OK ) return iRet;
	if ( bHasBoundPort ) {
		return xsshWireWriteU32(pWriter, iBoundPort);
	}
	return XSSH_OK;
}

static int xsshMsgReadRequestSuccess(const void* pPayload, size_t iPayloadLen, xsshglobalrequestsuccessmsg* pOut)
{
	xsshreader rd;
	int iRet;

	if ( pOut == NULL || (pPayload == NULL && iPayloadLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	memset(pOut, 0, sizeof(*pOut));
	xsshReaderInit(&rd, pPayload, iPayloadLen);
	if ( (iRet = xsshMsgReadId(&rd, XSSH_MSG_REQUEST_SUCCESS)) != XSSH_OK ) return iRet;
	if ( xsshReaderLeft(&rd) == 0u ) {
		return XSSH_OK;
	}
	if ( xsshReaderLeft(&rd) != 4u ) {
		return XSSH_ERR_BAD_PACKET;
	}
	pOut->bHasBoundPort = TRUE;
	return xsshWireReadU32(&rd, &pOut->iBoundPort);
}

static int xsshMsgWriteRequestFailure(xsshwriter* pWriter)
{
	int iRet = xsshWireReserve(pWriter, 1u);

	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	return xsshWireWriteU8(pWriter, XSSH_MSG_REQUEST_FAILURE);
}

static int xsshMsgReadRequestFailure(const void* pPayload, size_t iPayloadLen)
{
	xsshreader rd;
	int iRet;

	if ( pPayload == NULL && iPayloadLen != 0u ) {
		return XSSH_ERR_INVALID;
	}
	xsshReaderInit(&rd, pPayload, iPayloadLen);
	if ( (iRet = xsshMsgReadId(&rd, XSSH_MSG_REQUEST_FAILURE)) != XSSH_OK ) return iRet;
	return xsshMsgReadNoTrailing(&rd);
}

static int xsshMsgWriteDisconnect(xsshwriter* pWriter, uint32 iReason, const char* sDescription, const char* sLanguageTag)
{
	size_t iDescLen;
	size_t iLangLen;
	int iRet;

	if ( pWriter == NULL ) {
		return XSSH_ERR_INVALID;
	}
	sDescription = sDescription ? sDescription : "";
	sLanguageTag = sLanguageTag ? sLanguageTag : "";
	iDescLen = strlen(sDescription);
	iLangLen = strlen(sLanguageTag);
	iRet = xsshWireReserve(pWriter, 1u + 4u + 4u + iDescLen + 4u + iLangLen);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( (iRet = xsshWireWriteU8(pWriter, XSSH_MSG_DISCONNECT)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteU32(pWriter, iReason)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteString(pWriter, sDescription, iDescLen)) != XSSH_OK ) return iRet;
	return xsshWireWriteString(pWriter, sLanguageTag, iLangLen);
}

static int xsshMsgReadDisconnect(const void* pPayload, size_t iPayloadLen, xsshdisconnectmsg* pOut)
{
	xsshreader rd;
	int iRet;

	if ( pOut == NULL || (pPayload == NULL && iPayloadLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	memset(pOut, 0, sizeof(*pOut));
	xsshReaderInit(&rd, pPayload, iPayloadLen);
	if ( (iRet = xsshMsgReadId(&rd, XSSH_MSG_DISCONNECT)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadU32(&rd, &pOut->iReason)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadString(&rd, &pOut->tDescription)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadString(&rd, &pOut->tLanguageTag)) != XSSH_OK ) return iRet;
	return xsshMsgReadNoTrailing(&rd);
}

static int xsshMsgWriteIgnore(xsshwriter* pWriter, const void* pData, size_t iLen)
{
	int iRet;

	if ( pWriter == NULL || (pData == NULL && iLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	iRet = xsshWireReserve(pWriter, 1u + 4u + iLen);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( (iRet = xsshWireWriteU8(pWriter, XSSH_MSG_IGNORE)) != XSSH_OK ) return iRet;
	return xsshWireWriteString(pWriter, pData, iLen);
}

static int xsshMsgReadIgnore(const void* pPayload, size_t iPayloadLen, xsshignoremsg* pOut)
{
	xsshreader rd;
	int iRet;

	if ( pOut == NULL || (pPayload == NULL && iPayloadLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	memset(pOut, 0, sizeof(*pOut));
	xsshReaderInit(&rd, pPayload, iPayloadLen);
	if ( (iRet = xsshMsgReadId(&rd, XSSH_MSG_IGNORE)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadString(&rd, &pOut->tData)) != XSSH_OK ) return iRet;
	return xsshMsgReadNoTrailing(&rd);
}

static int xsshMsgWriteDebug(xsshwriter* pWriter, bool bAlwaysDisplay, const char* sMessage, const char* sLanguageTag)
{
	size_t iMsgLen;
	size_t iLangLen;
	int iRet;

	if ( pWriter == NULL ) {
		return XSSH_ERR_INVALID;
	}
	sMessage = sMessage ? sMessage : "";
	sLanguageTag = sLanguageTag ? sLanguageTag : "";
	iMsgLen = strlen(sMessage);
	iLangLen = strlen(sLanguageTag);
	iRet = xsshWireReserve(pWriter, 1u + 1u + 4u + iMsgLen + 4u + iLangLen);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( (iRet = xsshWireWriteU8(pWriter, XSSH_MSG_DEBUG)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteBool(pWriter, bAlwaysDisplay)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteString(pWriter, sMessage, iMsgLen)) != XSSH_OK ) return iRet;
	return xsshWireWriteString(pWriter, sLanguageTag, iLangLen);
}

static int xsshMsgReadDebug(const void* pPayload, size_t iPayloadLen, xsshdebugmsg* pOut)
{
	xsshreader rd;
	int iRet;

	if ( pOut == NULL || (pPayload == NULL && iPayloadLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	memset(pOut, 0, sizeof(*pOut));
	xsshReaderInit(&rd, pPayload, iPayloadLen);
	if ( (iRet = xsshMsgReadId(&rd, XSSH_MSG_DEBUG)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadBool(&rd, &pOut->bAlwaysDisplay)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadString(&rd, &pOut->tMessage)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadString(&rd, &pOut->tLanguageTag)) != XSSH_OK ) return iRet;
	return xsshMsgReadNoTrailing(&rd);
}

static int xsshAuthWriteRequestPrefix(xsshwriter* pWriter, const char* sUser, const char* sMethod, size_t iExtraLen)
{
	size_t iUserLen;
	size_t iServiceLen = strlen("ssh-connection");
	size_t iMethodLen;
	int iRet;

	if ( pWriter == NULL || sUser == NULL || sMethod == NULL ) {
		return XSSH_ERR_INVALID;
	}
	iUserLen = strlen(sUser);
	iMethodLen = strlen(sMethod);
	iRet = xsshWireReserve(pWriter, 1u + 4u + iUserLen + 4u + iServiceLen + 4u + iMethodLen + iExtraLen);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( (iRet = xsshWireWriteU8(pWriter, XSSH_MSG_USERAUTH_REQUEST)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteString(pWriter, sUser, iUserLen)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteString(pWriter, "ssh-connection", iServiceLen)) != XSSH_OK ) return iRet;
	return xsshWireWriteString(pWriter, sMethod, iMethodLen);
}

static int xsshAuthWriteNoneRequest(xsshwriter* pWriter, const char* sUser)
{
	return xsshAuthWriteRequestPrefix(pWriter, sUser, "none", 0u);
}

static int xsshAuthWritePasswordRequest(xsshwriter* pWriter, const char* sUser, const char* sPassword)
{
	size_t iPassLen;
	int iRet;

	if ( sPassword == NULL ) {
		return XSSH_ERR_INVALID;
	}
	iPassLen = strlen(sPassword);
	/* password 不写入日志、不缓存到 result；这里仅按 RFC 4252 编码请求 payload。 */
	iRet = xsshAuthWriteRequestPrefix(pWriter, sUser, "password", 1u + 4u + iPassLen);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( (iRet = xsshWireWriteBool(pWriter, FALSE)) != XSSH_OK ) return iRet;
	return xsshWireWriteString(pWriter, sPassword, iPassLen);
}

static int xsshAuthWriteKeyboardInteractiveRequest(xsshwriter* pWriter, const char* sUser, const char* sSubmethods)
{
	size_t iSubLen;
	int iRet;

	if ( sSubmethods == NULL ) {
		sSubmethods = "";
	}
	iSubLen = strlen(sSubmethods);
	iRet = xsshAuthWriteRequestPrefix(pWriter, sUser, "keyboard-interactive", 4u + 4u + iSubLen);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( (iRet = xsshWireWriteString(pWriter, "", 0u)) != XSSH_OK ) return iRet;
	return xsshWireWriteString(pWriter, sSubmethods, iSubLen);
}

static int xsshAuthWritePublicKeyRequest(xsshwriter* pWriter, const char* sUser, bool bHasSignature, const char* sAlgorithm, xsshslice tPublicKeyBlob, xsshslice tSignature)
{
	size_t iAlgLen;
	size_t iExtraLen;
	int iRet;

	if ( sAlgorithm == NULL || !xsshSliceIsValid(tPublicKeyBlob) || !xsshSliceIsValid(tSignature) ) {
		return XSSH_ERR_INVALID;
	}
	iAlgLen = strlen(sAlgorithm);
	iExtraLen = 1u + 4u + iAlgLen + 4u + tPublicKeyBlob.iLen + (bHasSignature ? (4u + tSignature.iLen) : 0u);
	iRet = xsshAuthWriteRequestPrefix(pWriter, sUser, "publickey", iExtraLen);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( (iRet = xsshWireWriteBool(pWriter, bHasSignature)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteString(pWriter, sAlgorithm, iAlgLen)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteString(pWriter, tPublicKeyBlob.pData, tPublicKeyBlob.iLen)) != XSSH_OK ) return iRet;
	if ( bHasSignature ) {
		return xsshWireWriteString(pWriter, tSignature.pData, tSignature.iLen);
	}
	return XSSH_OK;
}

static int xsshAuthWritePublicKeySignData(xsshwriter* pWriter, xsshslice tSessionId, const char* sUser, const char* sAlgorithm, xsshslice tPublicKeyBlob)
{
	size_t iUserLen;
	size_t iAlgLen;
	size_t iServiceLen = strlen("ssh-connection");
	size_t iMethodLen = strlen("publickey");
	size_t iTotalLen;
	int iRet;

	if ( pWriter == NULL || !xsshSliceIsValid(tSessionId) || tSessionId.iLen == 0u ||
		sUser == NULL || sAlgorithm == NULL || !xsshSliceIsValid(tPublicKeyBlob) ) {
		return XSSH_ERR_INVALID;
	}
	iUserLen = strlen(sUser);
	iAlgLen = strlen(sAlgorithm);
	if ( tSessionId.iLen > 0xffffffffu || tPublicKeyBlob.iLen > 0xffffffffu ||
		iUserLen > 0xffffffffu || iAlgLen > 0xffffffffu ) {
		return XSSH_ERR_OVERFLOW;
	}
	iTotalLen = 4u + tSessionId.iLen +
		1u + 4u + iUserLen + 4u + iServiceLen + 4u + iMethodLen +
		1u + 4u + iAlgLen + 4u + tPublicKeyBlob.iLen;
	iRet = xsshWireReserve(pWriter, iTotalLen);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}

	/* RFC 4252 publickey 签名原文 = string(session_id) || SSH_MSG_USERAUTH_REQUEST 的无签名字段。 */
	if ( (iRet = xsshWireWriteString(pWriter, tSessionId.pData, tSessionId.iLen)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshAuthWriteRequestPrefix(pWriter, sUser, "publickey",
			1u + 4u + iAlgLen + 4u + tPublicKeyBlob.iLen)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteBool(pWriter, TRUE)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteString(pWriter, sAlgorithm, iAlgLen)) != XSSH_OK ) return iRet;
	return xsshWireWriteString(pWriter, tPublicKeyBlob.pData, tPublicKeyBlob.iLen);
}

static int xsshAuthReadRequest(const void* pPayload, size_t iPayloadLen, xsshauthrequestmsg* pOut)
{
	xsshreader rd;
	int iRet;

	if ( pOut == NULL || (pPayload == NULL && iPayloadLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	memset(pOut, 0, sizeof(*pOut));
	xsshReaderInit(&rd, pPayload, iPayloadLen);
	if ( (iRet = xsshMsgReadId(&rd, XSSH_MSG_USERAUTH_REQUEST)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadString(&rd, &pOut->tUserName)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadString(&rd, &pOut->tServiceName)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadString(&rd, &pOut->tMethodName)) != XSSH_OK ) return iRet;

	if ( xsshSliceEqualText(pOut->tMethodName, "none") ) {
		return xsshMsgReadNoTrailing(&rd);
	}
	if ( xsshSliceEqualText(pOut->tMethodName, "password") ) {
		if ( (iRet = xsshWireReadBool(&rd, &pOut->bPasswordChange)) != XSSH_OK ) return iRet;
		if ( pOut->bPasswordChange ) {
			return XSSH_ERR_UNSUPPORTED;
		}
		if ( (iRet = xsshWireReadString(&rd, &pOut->tPassword)) != XSSH_OK ) return iRet;
		return xsshMsgReadNoTrailing(&rd);
	}
	if ( xsshSliceEqualText(pOut->tMethodName, "publickey") ) {
		if ( (iRet = xsshWireReadBool(&rd, &pOut->bPublicKeyHasSignature)) != XSSH_OK ) return iRet;
		if ( (iRet = xsshWireReadString(&rd, &pOut->tPublicKeyAlgorithm)) != XSSH_OK ) return iRet;
		if ( (iRet = xsshWireReadString(&rd, &pOut->tPublicKeyBlob)) != XSSH_OK ) return iRet;
		if ( pOut->bPublicKeyHasSignature ) {
			if ( (iRet = xsshWireReadString(&rd, &pOut->tSignature)) != XSSH_OK ) return iRet;
		}
		return xsshMsgReadNoTrailing(&rd);
	}
	if ( xsshSliceEqualText(pOut->tMethodName, "keyboard-interactive") ) {
		if ( (iRet = xsshWireReadString(&rd, &pOut->tKbdIntLanguageTag)) != XSSH_OK ) return iRet;
		if ( (iRet = xsshWireReadString(&rd, &pOut->tKbdIntSubmethods)) != XSSH_OK ) return iRet;
		return xsshMsgReadNoTrailing(&rd);
	}
	return XSSH_ERR_UNSUPPORTED;
}

static int xsshAuthWriteFailure(xsshwriter* pWriter, const char* sMethods, bool bPartialSuccess)
{
	size_t iMethodsLen;
	int iRet;

	if ( pWriter == NULL || sMethods == NULL ) {
		return XSSH_ERR_INVALID;
	}
	iMethodsLen = strlen(sMethods);
	if ( !xsshWireNameListIsValid(sMethods, iMethodsLen) ) {
		return XSSH_ERR_INVALID;
	}
	iRet = xsshWireReserve(pWriter, 1u + 4u + iMethodsLen + 1u);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( (iRet = xsshWireWriteU8(pWriter, XSSH_MSG_USERAUTH_FAILURE)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteString(pWriter, sMethods, iMethodsLen)) != XSSH_OK ) return iRet;
	return xsshWireWriteBool(pWriter, bPartialSuccess);
}

static int xsshAuthReadFailure(const void* pPayload, size_t iPayloadLen, xsshauthfailuremsg* pOut)
{
	xsshreader rd;
	int iRet;

	if ( pOut == NULL || (pPayload == NULL && iPayloadLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	memset(pOut, 0, sizeof(*pOut));
	xsshReaderInit(&rd, pPayload, iPayloadLen);
	if ( (iRet = xsshMsgReadId(&rd, XSSH_MSG_USERAUTH_FAILURE)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadString(&rd, &pOut->tMethods)) != XSSH_OK ) return iRet;
	if ( !xsshWireNameListIsValid((const char*)pOut->tMethods.pData, pOut->tMethods.iLen) ) return XSSH_ERR_BAD_PACKET;
	if ( (iRet = xsshWireReadBool(&rd, &pOut->bPartialSuccess)) != XSSH_OK ) return iRet;
	return xsshMsgReadNoTrailing(&rd);
}

static int xsshAuthWriteSuccess(xsshwriter* pWriter)
{
	int iRet = xsshWireReserve(pWriter, 1u);

	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	return xsshWireWriteU8(pWriter, XSSH_MSG_USERAUTH_SUCCESS);
}

static int xsshAuthReadSuccess(const void* pPayload, size_t iPayloadLen)
{
	xsshreader rd;
	int iRet;

	if ( pPayload == NULL && iPayloadLen != 0u ) {
		return XSSH_ERR_INVALID;
	}
	xsshReaderInit(&rd, pPayload, iPayloadLen);
	if ( (iRet = xsshMsgReadId(&rd, XSSH_MSG_USERAUTH_SUCCESS)) != XSSH_OK ) return iRet;
	return xsshMsgReadNoTrailing(&rd);
}

static int xsshAuthWriteBanner(xsshwriter* pWriter, const char* sMessage, const char* sLanguageTag)
{
	size_t iMsgLen;
	size_t iLangLen;
	int iRet;

	if ( pWriter == NULL ) {
		return XSSH_ERR_INVALID;
	}
	sMessage = sMessage ? sMessage : "";
	sLanguageTag = sLanguageTag ? sLanguageTag : "";
	iMsgLen = strlen(sMessage);
	iLangLen = strlen(sLanguageTag);
	iRet = xsshWireReserve(pWriter, 1u + 4u + iMsgLen + 4u + iLangLen);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( (iRet = xsshWireWriteU8(pWriter, XSSH_MSG_USERAUTH_BANNER)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteString(pWriter, sMessage, iMsgLen)) != XSSH_OK ) return iRet;
	return xsshWireWriteString(pWriter, sLanguageTag, iLangLen);
}

static int xsshAuthReadBanner(const void* pPayload, size_t iPayloadLen, xsshauthbannermsg* pOut)
{
	xsshreader rd;
	int iRet;

	if ( pOut == NULL || (pPayload == NULL && iPayloadLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	memset(pOut, 0, sizeof(*pOut));
	xsshReaderInit(&rd, pPayload, iPayloadLen);
	if ( (iRet = xsshMsgReadId(&rd, XSSH_MSG_USERAUTH_BANNER)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadString(&rd, &pOut->tMessage)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadString(&rd, &pOut->tLanguageTag)) != XSSH_OK ) return iRet;
	return xsshMsgReadNoTrailing(&rd);
}

static int xsshAuthWritePublicKeyOk(xsshwriter* pWriter, const char* sAlgorithm, xsshslice tPublicKeyBlob)
{
	size_t iAlgLen;
	int iRet;

	if ( pWriter == NULL || sAlgorithm == NULL || !xsshSliceIsValid(tPublicKeyBlob) ) {
		return XSSH_ERR_INVALID;
	}
	iAlgLen = strlen(sAlgorithm);
	iRet = xsshWireReserve(pWriter, 1u + 4u + iAlgLen + 4u + tPublicKeyBlob.iLen);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( (iRet = xsshWireWriteU8(pWriter, XSSH_MSG_USERAUTH_PK_OK)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteString(pWriter, sAlgorithm, iAlgLen)) != XSSH_OK ) return iRet;
	return xsshWireWriteString(pWriter, tPublicKeyBlob.pData, tPublicKeyBlob.iLen);
}

static int xsshAuthReadPublicKeyOk(const void* pPayload, size_t iPayloadLen, xsshauthpkokmsg* pOut)
{
	xsshreader rd;
	int iRet;

	if ( pOut == NULL || (pPayload == NULL && iPayloadLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	memset(pOut, 0, sizeof(*pOut));
	xsshReaderInit(&rd, pPayload, iPayloadLen);
	if ( (iRet = xsshMsgReadId(&rd, XSSH_MSG_USERAUTH_PK_OK)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadString(&rd, &pOut->tPublicKeyAlgorithm)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadString(&rd, &pOut->tPublicKeyBlob)) != XSSH_OK ) return iRet;
	return xsshMsgReadNoTrailing(&rd);
}

static int xsshAuthWriteKeyboardInteractiveInfoRequest(xsshwriter* pWriter, const char* sName, const char* sInstruction,
	const char* const* psPrompts, const bool* pEcho, uint32 iPromptCount)
{
	size_t iNameLen;
	size_t iInstructionLen;
	size_t iNeed;
	uint32 i;
	int iRet;

	if ( pWriter == NULL || (iPromptCount != 0u && (psPrompts == NULL || pEcho == NULL)) ||
		iPromptCount > XSSH_AUTH_KBDINT_PROMPT_MAX ) {
		return XSSH_ERR_INVALID;
	}
	sName = sName ? sName : "";
	sInstruction = sInstruction ? sInstruction : "";
	iNameLen = strlen(sName);
	iInstructionLen = strlen(sInstruction);
	iNeed = 1u + 4u + iNameLen + 4u + iInstructionLen + 4u + 4u;
	for ( i = 0u; i < iPromptCount; ++i ) {
		if ( psPrompts[i] == NULL ) {
			return XSSH_ERR_INVALID;
		}
		iNeed += 4u + strlen(psPrompts[i]) + 1u;
	}
	iRet = xsshWireReserve(pWriter, iNeed);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( (iRet = xsshWireWriteU8(pWriter, XSSH_MSG_USERAUTH_INFO_REQUEST)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteString(pWriter, sName, iNameLen)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteString(pWriter, sInstruction, iInstructionLen)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteString(pWriter, "", 0u)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteU32(pWriter, iPromptCount)) != XSSH_OK ) return iRet;
	for ( i = 0u; i < iPromptCount; ++i ) {
		if ( (iRet = xsshWireWriteString(pWriter, psPrompts[i], strlen(psPrompts[i]))) != XSSH_OK ) return iRet;
		if ( (iRet = xsshWireWriteBool(pWriter, pEcho[i])) != XSSH_OK ) return iRet;
	}
	return XSSH_OK;
}

static int xsshAuthReadKeyboardInteractiveInfoRequest(const void* pPayload, size_t iPayloadLen, xsshauthkbdintrequestmsg* pOut)
{
	xsshreader rd;
	uint32 i;
	int iRet;

	if ( pOut == NULL || (pPayload == NULL && iPayloadLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	memset(pOut, 0, sizeof(*pOut));
	xsshReaderInit(&rd, pPayload, iPayloadLen);
	if ( (iRet = xsshMsgReadId(&rd, XSSH_MSG_USERAUTH_INFO_REQUEST)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadString(&rd, &pOut->tName)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadString(&rd, &pOut->tInstruction)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadString(&rd, &pOut->tLanguageTag)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadU32(&rd, &pOut->iPromptCount)) != XSSH_OK ) return iRet;
	if ( pOut->iPromptCount > XSSH_AUTH_KBDINT_PROMPT_MAX ) {
		return XSSH_ERR_UNSUPPORTED;
	}
	for ( i = 0u; i < pOut->iPromptCount; ++i ) {
		if ( (iRet = xsshWireReadString(&rd, &pOut->arrPrompts[i])) != XSSH_OK ) return iRet;
		if ( (iRet = xsshWireReadBool(&rd, &pOut->arrEcho[i])) != XSSH_OK ) return iRet;
	}
	return xsshMsgReadNoTrailing(&rd);
}

static int xsshAuthWriteKeyboardInteractiveInfoResponse(xsshwriter* pWriter, const char* const* psResponses, uint32 iResponseCount)
{
	size_t iNeed = 1u + 4u;
	uint32 i;
	int iRet;

	if ( pWriter == NULL || (iResponseCount != 0u && psResponses == NULL) || iResponseCount > XSSH_AUTH_KBDINT_PROMPT_MAX ) {
		return XSSH_ERR_INVALID;
	}
	for ( i = 0u; i < iResponseCount; ++i ) {
		if ( psResponses[i] == NULL ) {
			return XSSH_ERR_INVALID;
		}
		iNeed += 4u + strlen(psResponses[i]);
	}
	iRet = xsshWireReserve(pWriter, iNeed);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( (iRet = xsshWireWriteU8(pWriter, XSSH_MSG_USERAUTH_INFO_RESPONSE)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteU32(pWriter, iResponseCount)) != XSSH_OK ) return iRet;
	for ( i = 0u; i < iResponseCount; ++i ) {
		if ( (iRet = xsshWireWriteString(pWriter, psResponses[i], strlen(psResponses[i]))) != XSSH_OK ) return iRet;
	}
	return XSSH_OK;
}

static int xsshAuthReadKeyboardInteractiveInfoResponse(const void* pPayload, size_t iPayloadLen, xsshauthkbdintresponsemsg* pOut)
{
	xsshreader rd;
	uint32 i;
	int iRet;

	if ( pOut == NULL || (pPayload == NULL && iPayloadLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	memset(pOut, 0, sizeof(*pOut));
	xsshReaderInit(&rd, pPayload, iPayloadLen);
	if ( (iRet = xsshMsgReadId(&rd, XSSH_MSG_USERAUTH_INFO_RESPONSE)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadU32(&rd, &pOut->iResponseCount)) != XSSH_OK ) return iRet;
	if ( pOut->iResponseCount > XSSH_AUTH_KBDINT_PROMPT_MAX ) {
		return XSSH_ERR_UNSUPPORTED;
	}
	for ( i = 0u; i < pOut->iResponseCount; ++i ) {
		if ( (iRet = xsshWireReadString(&rd, &pOut->arrResponses[i])) != XSSH_OK ) return iRet;
	}
	return xsshMsgReadNoTrailing(&rd);
}

static int xsshChannelOpenSessionWrite(xsshwriter* pWriter, uint32 iSenderChannel, uint32 iInitialWindow, uint32 iMaxPacket)
{
	int iRet;
	size_t iTypeLen = strlen("session");

	if ( pWriter == NULL ) {
		return XSSH_ERR_INVALID;
	}
	iRet = xsshWireReserve(pWriter, 1u + 4u + iTypeLen + 4u + 4u + 4u);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( (iRet = xsshWireWriteU8(pWriter, XSSH_MSG_CHANNEL_OPEN)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteString(pWriter, "session", iTypeLen)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteU32(pWriter, iSenderChannel)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteU32(pWriter, iInitialWindow)) != XSSH_OK ) return iRet;
	return xsshWireWriteU32(pWriter, iMaxPacket);
}

static int xsshChannelOpenTcpipWrite(xsshwriter* pWriter, const char* sChannelType, uint32 iSenderChannel, uint32 iInitialWindow, uint32 iMaxPacket,
	const char* sConnectedHost, uint32 iConnectedPort, const char* sOriginatorHost, uint32 iOriginatorPort)
{
	size_t iTypeLen;
	size_t iConnectedLen;
	size_t iOriginLen;
	int iRet;

	if ( pWriter == NULL || sChannelType == NULL || sConnectedHost == NULL || sOriginatorHost == NULL ) {
		return XSSH_ERR_INVALID;
	}
	iTypeLen = strlen(sChannelType);
	iConnectedLen = strlen(sConnectedHost);
	iOriginLen = strlen(sOriginatorHost);
	iRet = xsshWireReserve(pWriter,
		1u + 4u + iTypeLen + 4u + 4u + 4u +
		4u + iConnectedLen + 4u + 4u + iOriginLen + 4u);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( (iRet = xsshWireWriteU8(pWriter, XSSH_MSG_CHANNEL_OPEN)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteString(pWriter, sChannelType, iTypeLen)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteU32(pWriter, iSenderChannel)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteU32(pWriter, iInitialWindow)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteU32(pWriter, iMaxPacket)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteString(pWriter, sConnectedHost, iConnectedLen)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteU32(pWriter, iConnectedPort)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteString(pWriter, sOriginatorHost, iOriginLen)) != XSSH_OK ) return iRet;
	return xsshWireWriteU32(pWriter, iOriginatorPort);
}

static int xsshChannelOpenDirectTcpipWrite(xsshwriter* pWriter, uint32 iSenderChannel, uint32 iInitialWindow, uint32 iMaxPacket,
	const char* sHostToConnect, uint32 iPortToConnect, const char* sOriginatorHost, uint32 iOriginatorPort)
{
	return xsshChannelOpenTcpipWrite(pWriter, "direct-tcpip", iSenderChannel, iInitialWindow, iMaxPacket,
		sHostToConnect, iPortToConnect, sOriginatorHost, iOriginatorPort);
}

static int xsshChannelOpenForwardedTcpipWrite(xsshwriter* pWriter, uint32 iSenderChannel, uint32 iInitialWindow, uint32 iMaxPacket,
	const char* sConnectedHost, uint32 iConnectedPort, const char* sOriginatorHost, uint32 iOriginatorPort)
{
	return xsshChannelOpenTcpipWrite(pWriter, "forwarded-tcpip", iSenderChannel, iInitialWindow, iMaxPacket,
		sConnectedHost, iConnectedPort, sOriginatorHost, iOriginatorPort);
}

static int xsshChannelOpenRead(const void* pPayload, size_t iPayloadLen, xsshchannelopenmsg* pOut)
{
	xsshreader rd;
	int iRet;

	if ( pOut == NULL || (pPayload == NULL && iPayloadLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	memset(pOut, 0, sizeof(*pOut));
	xsshReaderInit(&rd, pPayload, iPayloadLen);
	if ( (iRet = xsshMsgReadId(&rd, XSSH_MSG_CHANNEL_OPEN)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadString(&rd, &pOut->tChannelType)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadU32(&rd, &pOut->iSenderChannel)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadU32(&rd, &pOut->iInitialWindow)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadU32(&rd, &pOut->iMaxPacket)) != XSSH_OK ) return iRet;
	pOut->tExtra.pData = rd.pData + rd.iPos;
	pOut->tExtra.iLen = xsshReaderLeft(&rd);
	rd.iPos = rd.iSize;
	return XSSH_OK;
}

static int xsshChannelOpenTcpipExtraRead(const xsshchannelopenmsg* pOpen, xsshchanneltcpipopenmsg* pOut)
{
	xsshreader rd;
	int iRet;

	if ( pOpen == NULL || pOut == NULL ||
		(!xsshSliceEqualText(pOpen->tChannelType, "direct-tcpip") && !xsshSliceEqualText(pOpen->tChannelType, "forwarded-tcpip")) ) {
		return XSSH_ERR_INVALID;
	}
	memset(pOut, 0, sizeof(*pOut));
	xsshReaderInit(&rd, pOpen->tExtra.pData, pOpen->tExtra.iLen);
	if ( (iRet = xsshWireReadString(&rd, &pOut->tConnectedHost)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadU32(&rd, &pOut->iConnectedPort)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadString(&rd, &pOut->tOriginatorHost)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadU32(&rd, &pOut->iOriginatorPort)) != XSSH_OK ) return iRet;
	return xsshMsgReadNoTrailing(&rd);
}

static int xsshChannelOpenConfirmationWrite(xsshwriter* pWriter, uint32 iRecipientChannel, uint32 iSenderChannel, uint32 iInitialWindow, uint32 iMaxPacket)
{
	int iRet;

	if ( pWriter == NULL ) {
		return XSSH_ERR_INVALID;
	}
	iRet = xsshWireReserve(pWriter, 1u + 4u + 4u + 4u + 4u);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( (iRet = xsshWireWriteU8(pWriter, XSSH_MSG_CHANNEL_OPEN_CONFIRMATION)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteU32(pWriter, iRecipientChannel)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteU32(pWriter, iSenderChannel)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteU32(pWriter, iInitialWindow)) != XSSH_OK ) return iRet;
	return xsshWireWriteU32(pWriter, iMaxPacket);
}

static int xsshChannelOpenConfirmationRead(const void* pPayload, size_t iPayloadLen, xsshchannelopenconfirmmsg* pOut)
{
	xsshreader rd;
	int iRet;

	if ( pOut == NULL || (pPayload == NULL && iPayloadLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	memset(pOut, 0, sizeof(*pOut));
	xsshReaderInit(&rd, pPayload, iPayloadLen);
	if ( (iRet = xsshMsgReadId(&rd, XSSH_MSG_CHANNEL_OPEN_CONFIRMATION)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadU32(&rd, &pOut->iRecipientChannel)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadU32(&rd, &pOut->iSenderChannel)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadU32(&rd, &pOut->iInitialWindow)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadU32(&rd, &pOut->iMaxPacket)) != XSSH_OK ) return iRet;
	return xsshMsgReadNoTrailing(&rd);
}

static int xsshChannelOpenFailureWrite(xsshwriter* pWriter, uint32 iRecipientChannel, uint32 iReason, const char* sDescription, const char* sLanguageTag)
{
	size_t iDescLen;
	size_t iLangLen;
	int iRet;

	if ( pWriter == NULL ) {
		return XSSH_ERR_INVALID;
	}
	sDescription = sDescription ? sDescription : "";
	sLanguageTag = sLanguageTag ? sLanguageTag : "";
	iDescLen = strlen(sDescription);
	iLangLen = strlen(sLanguageTag);
	iRet = xsshWireReserve(pWriter, 1u + 4u + 4u + 4u + iDescLen + 4u + iLangLen);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( (iRet = xsshWireWriteU8(pWriter, XSSH_MSG_CHANNEL_OPEN_FAILURE)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteU32(pWriter, iRecipientChannel)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteU32(pWriter, iReason)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteString(pWriter, sDescription, iDescLen)) != XSSH_OK ) return iRet;
	return xsshWireWriteString(pWriter, sLanguageTag, iLangLen);
}

static int xsshChannelOpenFailureRead(const void* pPayload, size_t iPayloadLen, xsshchannelopenfailuremsg* pOut)
{
	xsshreader rd;
	int iRet;

	if ( pOut == NULL || (pPayload == NULL && iPayloadLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	memset(pOut, 0, sizeof(*pOut));
	xsshReaderInit(&rd, pPayload, iPayloadLen);
	if ( (iRet = xsshMsgReadId(&rd, XSSH_MSG_CHANNEL_OPEN_FAILURE)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadU32(&rd, &pOut->iRecipientChannel)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadU32(&rd, &pOut->iReason)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadString(&rd, &pOut->tDescription)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadString(&rd, &pOut->tLanguageTag)) != XSSH_OK ) return iRet;
	return xsshMsgReadNoTrailing(&rd);
}

static int xsshChannelWindowAdjustWrite(xsshwriter* pWriter, uint32 iRecipientChannel, uint32 iBytes)
{
	int iRet;

	if ( pWriter == NULL ) {
		return XSSH_ERR_INVALID;
	}
	iRet = xsshWireReserve(pWriter, 1u + 4u + 4u);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( (iRet = xsshWireWriteU8(pWriter, XSSH_MSG_CHANNEL_WINDOW_ADJUST)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteU32(pWriter, iRecipientChannel)) != XSSH_OK ) return iRet;
	return xsshWireWriteU32(pWriter, iBytes);
}

static int xsshChannelWindowAdjustRead(const void* pPayload, size_t iPayloadLen, xsshchannelwindowadjustmsg* pOut)
{
	xsshreader rd;
	int iRet;

	if ( pOut == NULL || (pPayload == NULL && iPayloadLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	memset(pOut, 0, sizeof(*pOut));
	xsshReaderInit(&rd, pPayload, iPayloadLen);
	if ( (iRet = xsshMsgReadId(&rd, XSSH_MSG_CHANNEL_WINDOW_ADJUST)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadU32(&rd, &pOut->iRecipientChannel)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadU32(&rd, &pOut->iBytes)) != XSSH_OK ) return iRet;
	return xsshMsgReadNoTrailing(&rd);
}

static int xsshChannelDataWriteEx(xsshwriter* pWriter, uint8 iMsg, uint32 iRecipientChannel, uint32 iDataType, const void* pData, size_t iLen)
{
	int iRet;

	if ( pWriter == NULL || (pData == NULL && iLen != 0u) ||
		(iMsg != XSSH_MSG_CHANNEL_DATA && iMsg != XSSH_MSG_CHANNEL_EXTENDED_DATA) ) {
		return XSSH_ERR_INVALID;
	}
	iRet = xsshWireReserve(pWriter, 1u + 4u + ((iMsg == XSSH_MSG_CHANNEL_EXTENDED_DATA) ? 4u : 0u) + 4u + iLen);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( (iRet = xsshWireWriteU8(pWriter, iMsg)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteU32(pWriter, iRecipientChannel)) != XSSH_OK ) return iRet;
	if ( iMsg == XSSH_MSG_CHANNEL_EXTENDED_DATA ) {
		if ( (iRet = xsshWireWriteU32(pWriter, iDataType)) != XSSH_OK ) return iRet;
	}
	return xsshWireWriteString(pWriter, pData, iLen);
}

static int xsshChannelDataWrite(xsshwriter* pWriter, uint32 iRecipientChannel, const void* pData, size_t iLen)
{
	return xsshChannelDataWriteEx(pWriter, XSSH_MSG_CHANNEL_DATA, iRecipientChannel, 0u, pData, iLen);
}

static int xsshChannelExtendedDataWrite(xsshwriter* pWriter, uint32 iRecipientChannel, uint32 iDataType, const void* pData, size_t iLen)
{
	return xsshChannelDataWriteEx(pWriter, XSSH_MSG_CHANNEL_EXTENDED_DATA, iRecipientChannel, iDataType, pData, iLen);
}

static int xsshChannelDataReadEx(const void* pPayload, size_t iPayloadLen, uint8 iMsg, xsshchanneldataeventmsg* pOut)
{
	xsshreader rd;
	int iRet;

	if ( pOut == NULL || (pPayload == NULL && iPayloadLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	memset(pOut, 0, sizeof(*pOut));
	xsshReaderInit(&rd, pPayload, iPayloadLen);
	if ( (iRet = xsshMsgReadId(&rd, iMsg)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadU32(&rd, &pOut->iRecipientChannel)) != XSSH_OK ) return iRet;
	if ( iMsg == XSSH_MSG_CHANNEL_EXTENDED_DATA ) {
		if ( (iRet = xsshWireReadU32(&rd, &pOut->iDataType)) != XSSH_OK ) return iRet;
	}
	if ( (iRet = xsshWireReadString(&rd, &pOut->tData)) != XSSH_OK ) return iRet;
	return xsshMsgReadNoTrailing(&rd);
}

static int xsshChannelDataRead(const void* pPayload, size_t iPayloadLen, xsshchanneldataeventmsg* pOut)
{
	return xsshChannelDataReadEx(pPayload, iPayloadLen, XSSH_MSG_CHANNEL_DATA, pOut);
}

static int xsshChannelExtendedDataRead(const void* pPayload, size_t iPayloadLen, xsshchanneldataeventmsg* pOut)
{
	return xsshChannelDataReadEx(pPayload, iPayloadLen, XSSH_MSG_CHANNEL_EXTENDED_DATA, pOut);
}

static int xsshChannelSimpleWrite(xsshwriter* pWriter, uint8 iMsg, uint32 iRecipientChannel)
{
	int iRet;

	if ( pWriter == NULL ) {
		return XSSH_ERR_INVALID;
	}
	iRet = xsshWireReserve(pWriter, 1u + 4u);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( (iRet = xsshWireWriteU8(pWriter, iMsg)) != XSSH_OK ) return iRet;
	return xsshWireWriteU32(pWriter, iRecipientChannel);
}

static int xsshChannelSimpleRead(const void* pPayload, size_t iPayloadLen, uint8 iMsg, uint32* pRecipientChannel)
{
	xsshreader rd;
	int iRet;

	if ( pRecipientChannel == NULL || (pPayload == NULL && iPayloadLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	xsshReaderInit(&rd, pPayload, iPayloadLen);
	if ( (iRet = xsshMsgReadId(&rd, iMsg)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadU32(&rd, pRecipientChannel)) != XSSH_OK ) return iRet;
	return xsshMsgReadNoTrailing(&rd);
}

static int xsshChannelEofWrite(xsshwriter* pWriter, uint32 iRecipientChannel) { return xsshChannelSimpleWrite(pWriter, XSSH_MSG_CHANNEL_EOF, iRecipientChannel); }
static int xsshChannelCloseWrite(xsshwriter* pWriter, uint32 iRecipientChannel) { return xsshChannelSimpleWrite(pWriter, XSSH_MSG_CHANNEL_CLOSE, iRecipientChannel); }
static int xsshChannelSuccessWrite(xsshwriter* pWriter, uint32 iRecipientChannel) { return xsshChannelSimpleWrite(pWriter, XSSH_MSG_CHANNEL_SUCCESS, iRecipientChannel); }
static int xsshChannelFailureWrite(xsshwriter* pWriter, uint32 iRecipientChannel) { return xsshChannelSimpleWrite(pWriter, XSSH_MSG_CHANNEL_FAILURE, iRecipientChannel); }
static int xsshChannelEofRead(const void* pPayload, size_t iPayloadLen, uint32* pRecipientChannel) { return xsshChannelSimpleRead(pPayload, iPayloadLen, XSSH_MSG_CHANNEL_EOF, pRecipientChannel); }
static int xsshChannelCloseRead(const void* pPayload, size_t iPayloadLen, uint32* pRecipientChannel) { return xsshChannelSimpleRead(pPayload, iPayloadLen, XSSH_MSG_CHANNEL_CLOSE, pRecipientChannel); }
static int xsshChannelSuccessRead(const void* pPayload, size_t iPayloadLen, uint32* pRecipientChannel) { return xsshChannelSimpleRead(pPayload, iPayloadLen, XSSH_MSG_CHANNEL_SUCCESS, pRecipientChannel); }
static int xsshChannelFailureRead(const void* pPayload, size_t iPayloadLen, uint32* pRecipientChannel) { return xsshChannelSimpleRead(pPayload, iPayloadLen, XSSH_MSG_CHANNEL_FAILURE, pRecipientChannel); }

static int xsshChannelRequestStart(xsshwriter* pWriter, uint32 iRecipientChannel, const char* sRequestType, bool bWantReply, size_t iExtraLen)
{
	size_t iTypeLen;
	int iRet;

	if ( pWriter == NULL || sRequestType == NULL ) {
		return XSSH_ERR_INVALID;
	}
	iTypeLen = strlen(sRequestType);
	iRet = xsshWireReserve(pWriter, 1u + 4u + 4u + iTypeLen + 1u + iExtraLen);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( (iRet = xsshWireWriteU8(pWriter, XSSH_MSG_CHANNEL_REQUEST)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteU32(pWriter, iRecipientChannel)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteString(pWriter, sRequestType, iTypeLen)) != XSSH_OK ) return iRet;
	return xsshWireWriteBool(pWriter, bWantReply);
}

static int xsshChannelRequestShellWrite(xsshwriter* pWriter, uint32 iRecipientChannel, bool bWantReply)
{
	return xsshChannelRequestStart(pWriter, iRecipientChannel, "shell", bWantReply, 0u);
}

static int xsshChannelRequestExecWrite(xsshwriter* pWriter, uint32 iRecipientChannel, bool bWantReply, const char* sCommand)
{
	size_t iCommandLen;
	int iRet;

	if ( sCommand == NULL ) {
		return XSSH_ERR_INVALID;
	}
	iCommandLen = strlen(sCommand);
	iRet = xsshChannelRequestStart(pWriter, iRecipientChannel, "exec", bWantReply, 4u + iCommandLen);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	return xsshWireWriteString(pWriter, sCommand, iCommandLen);
}

static int xsshChannelRequestSubsystemWrite(xsshwriter* pWriter, uint32 iRecipientChannel, bool bWantReply, const char* sSubsystem)
{
	size_t iSubsystemLen;
	int iRet;

	if ( sSubsystem == NULL ) {
		return XSSH_ERR_INVALID;
	}
	iSubsystemLen = strlen(sSubsystem);
	iRet = xsshChannelRequestStart(pWriter, iRecipientChannel, "subsystem", bWantReply, 4u + iSubsystemLen);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	return xsshWireWriteString(pWriter, sSubsystem, iSubsystemLen);
}

static int xsshChannelRequestEnvWrite(xsshwriter* pWriter, uint32 iRecipientChannel, bool bWantReply, const char* sName, const char* sValue)
{
	size_t iNameLen;
	size_t iValueLen;
	int iRet;

	if ( sName == NULL || sValue == NULL ) {
		return XSSH_ERR_INVALID;
	}
	iNameLen = strlen(sName);
	iValueLen = strlen(sValue);
	iRet = xsshChannelRequestStart(pWriter, iRecipientChannel, "env", bWantReply, 4u + iNameLen + 4u + iValueLen);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( (iRet = xsshWireWriteString(pWriter, sName, iNameLen)) != XSSH_OK ) return iRet;
	return xsshWireWriteString(pWriter, sValue, iValueLen);
}

static int xsshChannelRequestSignalWrite(xsshwriter* pWriter, uint32 iRecipientChannel, bool bWantReply, const char* sSignalName)
{
	size_t iSignalLen;
	int iRet;

	if ( sSignalName == NULL ) {
		return XSSH_ERR_INVALID;
	}
	iSignalLen = strlen(sSignalName);
	iRet = xsshChannelRequestStart(pWriter, iRecipientChannel, "signal", bWantReply, 4u + iSignalLen);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	return xsshWireWriteString(pWriter, sSignalName, iSignalLen);
}

static int xsshChannelRequestBreakWrite(xsshwriter* pWriter, uint32 iRecipientChannel, bool bWantReply, uint32 iBreakLengthMs)
{
	int iRet = xsshChannelRequestStart(pWriter, iRecipientChannel, "break", bWantReply, 4u);

	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	return xsshWireWriteU32(pWriter, iBreakLengthMs);
}

static int xsshChannelRequestPtyWrite(xsshwriter* pWriter, uint32 iRecipientChannel, bool bWantReply, const xsshpty* pPty)
{
	xsshpty tDefaultPty;
	const char* sTerm;
	size_t iTermLen;
	size_t iModesPayloadLen;
	int iRet;

	if ( pWriter == NULL ) {
		return XSSH_ERR_INVALID;
	}
	if ( pPty == NULL ) {
		xsshPtyInit(&tDefaultPty);
		pPty = &tDefaultPty;
	}
	sTerm = pPty->sTerm ? pPty->sTerm : "xterm-256color";
	iTermLen = strlen(sTerm);
	if ( pPty->iTerminalModesLen > sizeof(pPty->arrTerminalModes) ||
		(pPty->iTerminalModesLen % 5u) != 0u ) {
		return XSSH_ERR_INVALID;
	}
	iModesPayloadLen = (pPty->iTerminalModesLen == 0u) ? 0u : (pPty->iTerminalModesLen + 1u);
	iRet = xsshChannelRequestStart(pWriter, iRecipientChannel, "pty-req", bWantReply, 4u + iTermLen + 4u + 4u + 4u + 4u + 4u + iModesPayloadLen);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( (iRet = xsshWireWriteString(pWriter, sTerm, iTermLen)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteU32(pWriter, pPty->iCols)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteU32(pWriter, pPty->iRows)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteU32(pWriter, pPty->iPixelWidth)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteU32(pWriter, pPty->iPixelHeight)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteU32(pWriter, (uint32)iModesPayloadLen)) != XSSH_OK ) return iRet;
	if ( iModesPayloadLen == 0u ) {
		return XSSH_OK;
	}
	if ( (iRet = xsshWireWriteBytes(pWriter, pPty->arrTerminalModes, pPty->iTerminalModesLen)) != XSSH_OK ) return iRet;
	return xsshWireWriteU8(pWriter, XSSH_TTY_OP_END);
}

static int xsshChannelRequestWindowChangeWrite(xsshwriter* pWriter, uint32 iRecipientChannel, uint32 iCols, uint32 iRows, uint32 iPixelWidth, uint32 iPixelHeight)
{
	int iRet = xsshChannelRequestStart(pWriter, iRecipientChannel, "window-change", FALSE, 4u + 4u + 4u + 4u);

	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( (iRet = xsshWireWriteU32(pWriter, iCols)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteU32(pWriter, iRows)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteU32(pWriter, iPixelWidth)) != XSSH_OK ) return iRet;
	return xsshWireWriteU32(pWriter, iPixelHeight);
}

static int xsshChannelRequestExitStatusWrite(xsshwriter* pWriter, uint32 iRecipientChannel, uint32 iExitStatus)
{
	int iRet = xsshChannelRequestStart(pWriter, iRecipientChannel, "exit-status", FALSE, 4u);

	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	return xsshWireWriteU32(pWriter, iExitStatus);
}

static int xsshChannelRequestExitSignalWrite(xsshwriter* pWriter, uint32 iRecipientChannel, const char* sSignalName, bool bCoreDumped, const char* sErrorMessage, const char* sLanguageTag)
{
	size_t iSignalLen;
	size_t iErrorLen;
	size_t iLangLen;
	int iRet;

	if ( sSignalName == NULL ) {
		return XSSH_ERR_INVALID;
	}
	if ( sErrorMessage == NULL ) {
		sErrorMessage = "";
	}
	if ( sLanguageTag == NULL ) {
		sLanguageTag = "";
	}
	iSignalLen = strlen(sSignalName);
	iErrorLen = strlen(sErrorMessage);
	iLangLen = strlen(sLanguageTag);
	iRet = xsshChannelRequestStart(pWriter, iRecipientChannel, "exit-signal", FALSE, 4u + iSignalLen + 1u + 4u + iErrorLen + 4u + iLangLen);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( (iRet = xsshWireWriteString(pWriter, sSignalName, iSignalLen)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteBool(pWriter, bCoreDumped)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireWriteString(pWriter, sErrorMessage, iErrorLen)) != XSSH_OK ) return iRet;
	return xsshWireWriteString(pWriter, sLanguageTag, iLangLen);
}

static int xsshChannelRequestRead(const void* pPayload, size_t iPayloadLen, xsshchannelrequestmsg* pOut)
{
	xsshreader rd;
	int iRet;

	if ( pOut == NULL || (pPayload == NULL && iPayloadLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	memset(pOut, 0, sizeof(*pOut));
	xsshReaderInit(&rd, pPayload, iPayloadLen);
	if ( (iRet = xsshMsgReadId(&rd, XSSH_MSG_CHANNEL_REQUEST)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadU32(&rd, &pOut->iRecipientChannel)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadString(&rd, &pOut->tRequestType)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadBool(&rd, &pOut->bWantReply)) != XSSH_OK ) return iRet;
	pOut->tPayload.pData = rd.pData + rd.iPos;
	pOut->tPayload.iLen = xsshReaderLeft(&rd);
	rd.iPos = rd.iSize;
	return XSSH_OK;
}

static int xsshChannelRequestExitStatusParse(const xsshchannelrequestmsg* pMsg, uint32* pExitStatus)
{
	xsshreader rd;
	int iRet;

	if ( pMsg == NULL || pExitStatus == NULL || !xsshSliceEqualText(pMsg->tRequestType, "exit-status") ) {
		return XSSH_ERR_INVALID;
	}
	xsshReaderInit(&rd, pMsg->tPayload.pData, pMsg->tPayload.iLen);
	if ( (iRet = xsshWireReadU32(&rd, pExitStatus)) != XSSH_OK ) return iRet;
	return xsshMsgReadNoTrailing(&rd);
}

static int xsshChannelRequestExitSignalParse(const xsshchannelrequestmsg* pMsg, xsshchannelexitsignalmsg* pOut)
{
	xsshreader rd;
	int iRet;

	if ( pMsg == NULL || pOut == NULL || !xsshSliceEqualText(pMsg->tRequestType, "exit-signal") ) {
		return XSSH_ERR_INVALID;
	}
	memset(pOut, 0, sizeof(*pOut));
	xsshReaderInit(&rd, pMsg->tPayload.pData, pMsg->tPayload.iLen);
	if ( (iRet = xsshWireReadString(&rd, &pOut->tSignalName)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadBool(&rd, &pOut->bCoreDumped)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadString(&rd, &pOut->tErrorMessage)) != XSSH_OK ) return iRet;
	if ( (iRet = xsshWireReadString(&rd, &pOut->tLanguageTag)) != XSSH_OK ) return iRet;
	return xsshMsgReadNoTrailing(&rd);
}

static int xsshBase64Value(uint8 ch)
{
	if ( ch >= 'A' && ch <= 'Z' ) return (int)(ch - 'A');
	if ( ch >= 'a' && ch <= 'z' ) return 26 + (int)(ch - 'a');
	if ( ch >= '0' && ch <= '9' ) return 52 + (int)(ch - '0');
	if ( ch == '+' ) return 62;
	if ( ch == '/' ) return 63;
	return -1;
}

static int xsshBase64DecodeTo(const char* sText, size_t iLen, uint8* pOut, size_t iCap, size_t* pOutLen)
{
	size_t i;
	size_t j = 0u;
	size_t iPad = 0u;

	if ( pOutLen == NULL || (sText == NULL && iLen != 0u) || (pOut == NULL && iCap != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	*pOutLen = 0u;
	if ( iLen == 0u || (iLen % 4u) != 0u ) {
		return XSSH_ERR_INVALID;
	}
	if ( sText[iLen - 1u] == '=' ) iPad++;
	if ( sText[iLen - 2u] == '=' ) iPad++;
	if ( (iLen / 4u) * 3u < iPad ) {
		return XSSH_ERR_INVALID;
	}
	if ( ((iLen / 4u) * 3u) - iPad > iCap ) {
		return XSSH_ERR_NO_SPACE;
	}

	for ( i = 0u; i < iLen; i += 4u ) {
		int a = xsshBase64Value((uint8)sText[i]);
		int b = xsshBase64Value((uint8)sText[i + 1u]);
		int c = (sText[i + 2u] == '=') ? 0 : xsshBase64Value((uint8)sText[i + 2u]);
		int d = (sText[i + 3u] == '=') ? 0 : xsshBase64Value((uint8)sText[i + 3u]);
		uint32 v;

		if ( a < 0 || b < 0 || c < 0 || d < 0 ) {
			return XSSH_ERR_INVALID;
		}
		if ( (sText[i + 2u] == '=' && i + 4u != iLen) ||
			(sText[i + 3u] == '=' && i + 4u != iLen) ) {
			return XSSH_ERR_INVALID;
		}
		v = ((uint32)a << 18) | ((uint32)b << 12) | ((uint32)c << 6) | (uint32)d;
		pOut[j++] = (uint8)((v >> 16) & 0xffu);
		if ( sText[i + 2u] != '=' ) {
			pOut[j++] = (uint8)((v >> 8) & 0xffu);
		}
		if ( sText[i + 3u] != '=' ) {
			pOut[j++] = (uint8)(v & 0xffu);
		}
	}
	*pOutLen = j;
	return XSSH_OK;
}

static bool xsshAsciiIsSpace(char ch)
{
	return (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') ? TRUE : FALSE;
}

static void xsshLineTrim(const char** ppText, size_t* pLen)
{
	const char* s;
	size_t iLen;

	if ( ppText == NULL || pLen == NULL || *ppText == NULL ) {
		return;
	}
	s = *ppText;
	iLen = *pLen;
	while ( iLen > 0u && xsshAsciiIsSpace(*s) ) {
		s++;
		iLen--;
	}
	while ( iLen > 0u && xsshAsciiIsSpace(s[iLen - 1u]) ) {
		iLen--;
	}
	*ppText = s;
	*pLen = iLen;
}

static bool xsshSliceEqTextN(xsshslice tSlice, const char* sText, size_t iLen)
{
	return (xsshSliceIsValid(tSlice) && tSlice.iLen == iLen && memcmp(tSlice.pData, sText, iLen) == 0) ? TRUE : FALSE;
}

static bool xsshIsSupportedPublicKeyName(const char* sText, size_t iLen)
{
	return (iLen == strlen("ssh-ed25519") && memcmp(sText, "ssh-ed25519", iLen) == 0) ? TRUE : FALSE;
}

static bool xsshNextToken(const char* sLine, size_t iLen, size_t* pPos, xsshslice* pTok)
{
	size_t i;
	size_t iStart;

	if ( sLine == NULL || pPos == NULL || pTok == NULL ) {
		return FALSE;
	}
	i = *pPos;
	while ( i < iLen && xsshAsciiIsSpace(sLine[i]) ) {
		i++;
	}
	if ( i >= iLen ) {
		return FALSE;
	}
	iStart = i;
	while ( i < iLen && !xsshAsciiIsSpace(sLine[i]) ) {
		i++;
	}
	pTok->pData = (const uint8*)sLine + iStart;
	pTok->iLen = i - iStart;
	*pPos = i;
	return TRUE;
}

static int xsshOpenSshPublicKeyLineRead(const char* sLine, size_t iLen, uint8* pBlobBuf, size_t iBlobCap, xsshopensshpublickeyline* pOut)
{
	const char* s;
	size_t iPos = 0u;
	size_t iBlobLen = 0u;
	xsshslice tok;
	xsshslice alg;
	xsshslice b64;
	int iRet;

	if ( pOut == NULL || (sLine == NULL && iLen != 0u) || (pBlobBuf == NULL && iBlobCap != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	memset(pOut, 0, sizeof(*pOut));
	s = sLine;
	xsshLineTrim(&s, &iLen);
	if ( iLen == 0u || s[0] == '#' ) {
		return XSSH_ERR_INVALID;
	}

	while ( xsshNextToken(s, iLen, &iPos, &tok) ) {
		if ( xsshIsSupportedPublicKeyName((const char*)tok.pData, tok.iLen) ) {
			alg = tok;
			if ( !xsshNextToken(s, iLen, &iPos, &b64) ) {
				return XSSH_ERR_INVALID;
			}
			while ( iPos < iLen && xsshAsciiIsSpace(s[iPos]) ) {
				iPos++;
			}
			pOut->tAlgorithm = alg;
			pOut->tBase64 = b64;
			pOut->tComment.pData = (const uint8*)s + iPos;
			pOut->tComment.iLen = iLen - iPos;
			iRet = xsshBase64DecodeTo((const char*)b64.pData, b64.iLen, pBlobBuf, iBlobCap, &iBlobLen);
			if ( iRet != XSSH_OK ) {
				return iRet;
			}
			pOut->tBlob.pData = pBlobBuf;
			pOut->tBlob.iLen = iBlobLen;
			iRet = xsshPublicKeyReadEd25519(pBlobBuf, iBlobLen, &pOut->tKey);
			if ( iRet != XSSH_OK ) {
				return iRet;
			}
			if ( !xsshSliceEqTextN(pOut->tKey.tAlgorithm, (const char*)alg.pData, alg.iLen) ) {
				return XSSH_ERR_BAD_PACKET;
			}
			return XSSH_OK;
		}
	}
	return XSSH_ERR_UNSUPPORTED;
}

static const char* xsshMemFindText(const char* sData, size_t iLen, const char* sNeedle)
{
	size_t iNeedleLen;
	size_t i;

	if ( sData == NULL || sNeedle == NULL ) {
		return NULL;
	}
	iNeedleLen = strlen(sNeedle);
	if ( iNeedleLen == 0u || iNeedleLen > iLen ) {
		return NULL;
	}
	for ( i = 0u; i <= iLen - iNeedleLen; ++i ) {
		if ( memcmp(sData + i, sNeedle, iNeedleLen) == 0 ) {
			return sData + i;
		}
	}
	return NULL;
}

static int xsshOpenSshPrivateKeyDecodePem(const void* pData, size_t iLen, uint8* pOut, size_t iOutCap, size_t* pOutLen)
{
	static const char sBegin[] = "-----BEGIN OPENSSH PRIVATE KEY-----";
	static const char sEnd[] = "-----END OPENSSH PRIVATE KEY-----";
	char sB64[8192];
	const char* sText = (const char*)pData;
	const char* sBody;
	const char* sStop;
	size_t iBeginLen = strlen(sBegin);
	size_t iB64Len = 0u;
	const char* p;

	if ( pData == NULL || pOut == NULL || pOutLen == NULL ) {
		return XSSH_ERR_INVALID;
	}
	*pOutLen = 0u;
	sBody = xsshMemFindText(sText, iLen, sBegin);
	if ( sBody == NULL ) {
		return XSSH_ERR_INVALID;
	}
	sBody += iBeginLen;
	sStop = xsshMemFindText(sBody, iLen - (size_t)(sBody - sText), sEnd);
	if ( sStop == NULL ) {
		return XSSH_ERR_INVALID;
	}
	for ( p = sBody; p < sStop; ++p ) {
		unsigned char ch = (unsigned char)*p;
		if ( ch == '\r' || ch == '\n' || ch == '\t' || ch == ' ' ) {
			continue;
		}
		if ( iB64Len >= sizeof(sB64) ) {
			return XSSH_ERR_NO_SPACE;
		}
		sB64[iB64Len++] = (char)ch;
	}
	if ( iB64Len == 0u ) {
		return XSSH_ERR_INVALID;
	}
	return xsshBase64DecodeTo(sB64, iB64Len, pOut, iOutCap, pOutLen);
}

static int xsshOpenSshPrivateKeyV1Read(const void* pData, size_t iLen, xsshprivateidentity* pOut)
{
	static const uint8 arrMagic[] = {
		'o','p','e','n','s','s','h','-','k','e','y','-','v','1','\0'
	};
	uint8 arrBin[8192];
	const uint8* pBin = NULL;
	size_t iBinLen = 0u;
	xsshreader rd;
	xsshreader privRd;
	xsshslice cipherName;
	xsshslice kdfName;
	xsshslice kdfOptions;
	xsshslice publicBlob;
	xsshslice privateList;
	xsshslice keyType;
	xsshslice publicRaw;
	xsshslice privateRaw;
	xsshslice comment;
	xsshpublickey publicKey;
	uint32 iKeyCount = 0u;
	uint32 iCheckA = 0u;
	uint32 iCheckB = 0u;
	size_t i;
	int iRet;

	if ( pData == NULL || iLen == 0u || pOut == NULL ) {
		return XSSH_ERR_INVALID;
	}
	memset(pOut, 0, sizeof(*pOut));
	if ( iLen >= sizeof(arrMagic) && memcmp(pData, arrMagic, sizeof(arrMagic)) == 0 ) {
		pBin = (const uint8*)pData;
		iBinLen = iLen;
	} else {
		iRet = xsshOpenSshPrivateKeyDecodePem(pData, iLen, arrBin, sizeof(arrBin), &iBinLen);
		if ( iRet != XSSH_OK ) {
			xsshSecureZero(arrBin, sizeof(arrBin));
			return iRet;
		}
		pBin = arrBin;
	}
	if ( iBinLen < sizeof(arrMagic) || memcmp(pBin, arrMagic, sizeof(arrMagic)) != 0 ) {
		xsshSecureZero(arrBin, sizeof(arrBin));
		return XSSH_ERR_INVALID;
	}
	xsshReaderInit(&rd, pBin, iBinLen);
	rd.iPos = sizeof(arrMagic);
	if ( (iRet = xsshWireReadString(&rd, &cipherName)) != XSSH_OK ||
		(iRet = xsshWireReadString(&rd, &kdfName)) != XSSH_OK ||
		(iRet = xsshWireReadString(&rd, &kdfOptions)) != XSSH_OK ||
		(iRet = xsshWireReadU32(&rd, &iKeyCount)) != XSSH_OK ||
		(iRet = xsshWireReadString(&rd, &publicBlob)) != XSSH_OK ||
		(iRet = xsshWireReadString(&rd, &privateList)) != XSSH_OK ) {
		xsshSecureZero(arrBin, sizeof(arrBin));
		return iRet;
	}
	if ( xsshMsgReadNoTrailing(&rd) != XSSH_OK || iKeyCount != 1u ) {
		xsshSecureZero(arrBin, sizeof(arrBin));
		return XSSH_ERR_BAD_PACKET;
	}
	if ( !xsshSliceEqualText(cipherName, "none") || !xsshSliceEqualText(kdfName, "none") || kdfOptions.iLen != 0u ) {
		xsshSecureZero(arrBin, sizeof(arrBin));
		return XSSH_ERR_UNSUPPORTED;
	}
	if ( publicBlob.iLen > sizeof(pOut->arrPublicKeyBlob) ||
		xsshPublicKeyReadEd25519(publicBlob.pData, publicBlob.iLen, &publicKey) != XSSH_OK ) {
		xsshSecureZero(arrBin, sizeof(arrBin));
		return XSSH_ERR_UNSUPPORTED;
	}
	xsshReaderInit(&privRd, privateList.pData, privateList.iLen);
	if ( (iRet = xsshWireReadU32(&privRd, &iCheckA)) != XSSH_OK ||
		(iRet = xsshWireReadU32(&privRd, &iCheckB)) != XSSH_OK ||
		(iRet = xsshWireReadString(&privRd, &keyType)) != XSSH_OK ||
		(iRet = xsshWireReadString(&privRd, &publicRaw)) != XSSH_OK ||
		(iRet = xsshWireReadString(&privRd, &privateRaw)) != XSSH_OK ||
		(iRet = xsshWireReadString(&privRd, &comment)) != XSSH_OK ) {
		xsshSecureZero(arrBin, sizeof(arrBin));
		return iRet;
	}
	(void)comment;
	if ( iCheckA != iCheckB ||
		!xsshSliceEqualText(keyType, "ssh-ed25519") ||
		publicRaw.iLen != 32u ||
		privateRaw.iLen != 64u ||
		memcmp(publicKey.tKeyData.pData, publicRaw.pData, 32u) != 0 ||
		memcmp(privateRaw.pData + 32u, publicRaw.pData, 32u) != 0 ) {
		xsshSecureZero(arrBin, sizeof(arrBin));
		return XSSH_ERR_BAD_PACKET;
	}
	for ( i = 0u; xsshReaderLeft(&privRd) > 0u; ++i ) {
		uint8 ch = 0u;
		if ( xsshWireReadU8(&privRd, &ch) != XSSH_OK || ch != (uint8)((i % 255u) + 1u) ) {
			xsshSecureZero(arrBin, sizeof(arrBin));
			return XSSH_ERR_BAD_PACKET;
		}
	}
	snprintf(pOut->sAlgorithm, sizeof(pOut->sAlgorithm), "%s", "ssh-ed25519");
	memcpy(pOut->arrPublicKeyBlob, publicBlob.pData, publicBlob.iLen);
	pOut->iPublicKeyBlobLen = publicBlob.iLen;
	memcpy(pOut->arrEd25519Seed, privateRaw.pData, 32u);
	memcpy(pOut->arrEd25519Pub, publicRaw.pData, 32u);
	pOut->bHasEd25519 = TRUE;
	xsshSecureZero(arrBin, sizeof(arrBin));
	return XSSH_OK;
}

static int xsshAuthLoadPrivateIdentity(const xsshauth* pAuth, xsshprivateidentity* pOut)
{
	char* sText = NULL;
	size_t iTextLen = 0u;
	int iRet;

	if ( pAuth == NULL || pOut == NULL ) {
		return XSSH_ERR_INVALID;
	}
	if ( pAuth->pPrivateKeyData != NULL && pAuth->iPrivateKeySize != 0u ) {
		return xsshOpenSshPrivateKeyV1Read(pAuth->pPrivateKeyData, pAuth->iPrivateKeySize, pOut);
	}
	if ( pAuth->sPrivateKeyPath == NULL || pAuth->sPrivateKeyPath[0] == '\0' ) {
		return XSSH_ERR_INVALID;
	}
	sText = (char*)xrtFileGetAll((str)pAuth->sPrivateKeyPath, &iTextLen);
	if ( sText == NULL ) {
		if ( xrtGetError() ) {
			xrtClearError();
		}
		return XSSH_ERR_IO;
	}
	iRet = xsshOpenSshPrivateKeyV1Read(sText, iTextLen, pOut);
	xsshSecureZero(sText, iTextLen);
	xrtFree(sText);
	return iRet;
}

static int xsshKnownHostLineReadHeader(const char* sLine, size_t iLen, xsshknownhostline* pOut)
{
	const char* s;
	size_t iPos = 0u;
	xsshslice hosts;
	xsshslice alg;
	xsshslice b64;

	if ( pOut == NULL || (sLine == NULL && iLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	memset(pOut, 0, sizeof(*pOut));
	s = sLine;
	xsshLineTrim(&s, &iLen);
	if ( iLen == 0u || s[0] == '#' ) {
		return XSSH_ERR_INVALID;
	}
	if ( !xsshNextToken(s, iLen, &iPos, &hosts) ||
		!xsshNextToken(s, iLen, &iPos, &alg) ||
		!xsshNextToken(s, iLen, &iPos, &b64) ) {
		return XSSH_ERR_INVALID;
	}
	while ( iPos < iLen && xsshAsciiIsSpace(s[iPos]) ) {
		iPos++;
	}
	pOut->tHosts = hosts;
	pOut->tAlgorithm = alg;
	pOut->tBase64 = b64;
	pOut->tComment.pData = (const uint8*)s + iPos;
	pOut->tComment.iLen = iLen - iPos;
	return XSSH_OK;
}

static int xsshKnownHostLineRead(const char* sLine, size_t iLen, uint8* pBlobBuf, size_t iBlobCap, xsshknownhostline* pOut)
{
	size_t iBlobLen = 0u;
	int iRet;

	if ( pOut == NULL || (pBlobBuf == NULL && iBlobCap != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	iRet = xsshKnownHostLineReadHeader(sLine, iLen, pOut);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	if ( !xsshIsSupportedPublicKeyName((const char*)pOut->tAlgorithm.pData, pOut->tAlgorithm.iLen) ) {
		return XSSH_ERR_UNSUPPORTED;
	}
	iRet = xsshBase64DecodeTo((const char*)pOut->tBase64.pData, pOut->tBase64.iLen, pBlobBuf, iBlobCap, &iBlobLen);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	pOut->tBlob.pData = pBlobBuf;
	pOut->tBlob.iLen = iBlobLen;
	iRet = xsshPublicKeyReadEd25519(pBlobBuf, iBlobLen, &pOut->tKey);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	return xsshSliceEqTextN(pOut->tKey.tAlgorithm, (const char*)pOut->tAlgorithm.pData, pOut->tAlgorithm.iLen) ? XSSH_OK : XSSH_ERR_BAD_PACKET;
}

static bool xsshKnownHostHashedOneMatches(const char* sPattern, size_t iPatternLen, const char* sHost, size_t iHostLen, uint16 iPort)
{
	const char* sSalt;
	const char* sHash;
	const char* sSep;
	uint8 arrSalt[64];
	uint8 arrExpected[XSSH_SHA1_SIZE];
	uint8 arrActual[XSSH_SHA1_SIZE];
	char sHostPort[512];
	size_t iSaltLen = 0u;
	size_t iHashLen = 0u;
	size_t iHashB64Len;
	int iWritten;

	if ( sPattern == NULL || sHost == NULL || iPatternLen < 5u ||
		memcmp(sPattern, "|1|", 3u) != 0 ) {
		return FALSE;
	}
	sSalt = sPattern + 3u;
	sSep = (const char*)memchr(sSalt, '|', iPatternLen - 3u);
	if ( sSep == NULL || sSep == sSalt ) {
		return FALSE;
	}
	sHash = sSep + 1u;
	iHashB64Len = (sPattern + iPatternLen) - sHash;
	if ( iHashB64Len == 0u ||
		xsshBase64DecodeTo(sSalt, (size_t)(sSep - sSalt), arrSalt, sizeof(arrSalt), &iSaltLen) != XSSH_OK ||
		xsshBase64DecodeTo(sHash, iHashB64Len, arrExpected, sizeof(arrExpected), &iHashLen) != XSSH_OK ||
		iSaltLen == 0u ||
		iHashLen != XSSH_SHA1_SIZE ) {
		return FALSE;
	}
	if ( iPort == XSSH_DEFAULT_PORT ) {
		if ( iHostLen >= sizeof(sHostPort) ) {
			return FALSE;
		}
		memcpy(sHostPort, sHost, iHostLen);
		sHostPort[iHostLen] = '\0';
	} else {
		iWritten = snprintf(sHostPort, sizeof(sHostPort), "[%s]:%u", sHost, (unsigned)iPort);
		if ( iWritten <= 0 || (size_t)iWritten >= sizeof(sHostPort) ) {
			return FALSE;
		}
	}
	if ( xsshCryptoHMACSHA1(arrSalt, iSaltLen, (const uint8*)sHostPort, strlen(sHostPort), arrActual) != XSSH_OK ) {
		return FALSE;
	}
	return (memcmp(arrExpected, arrActual, sizeof(arrActual)) == 0) ? TRUE : FALSE;
}

static bool xsshKnownHostOneMatches(const char* sPattern, size_t iPatternLen, const char* sHost, size_t iHostLen, uint16 iPort)
{
	if ( sPattern == NULL || sHost == NULL ) {
		return FALSE;
	}
	if ( iPatternLen > 3u && sPattern[0] == '|' ) {
		return xsshKnownHostHashedOneMatches(sPattern, iPatternLen, sHost, iHostLen, iPort);
	}
	if ( iPatternLen > 3u && sPattern[0] == '[' ) {
		size_t i;
		size_t iHostPartLen;
		uint32 iParsedPort = 0u;

		for ( i = 1u; i < iPatternLen; ++i ) {
			if ( sPattern[i] == ']' ) {
				break;
			}
		}
		if ( i >= iPatternLen || i + 2u >= iPatternLen || sPattern[i + 1u] != ':' ) {
			return FALSE;
		}
		iHostPartLen = i - 1u;
		if ( iHostPartLen != iHostLen || memcmp(sPattern + 1u, sHost, iHostLen) != 0 ) {
			return FALSE;
		}
		for ( i = i + 2u; i < iPatternLen; ++i ) {
			if ( sPattern[i] < '0' || sPattern[i] > '9' ) {
				return FALSE;
			}
			iParsedPort = iParsedPort * 10u + (uint32)(sPattern[i] - '0');
			if ( iParsedPort > 65535u ) {
				return FALSE;
			}
		}
		return (iParsedPort == (uint32)iPort) ? TRUE : FALSE;
	}
	return (iPatternLen == iHostLen && memcmp(sPattern, sHost, iHostLen) == 0) ? TRUE : FALSE;
}

static bool xsshKnownHostMatches(xsshslice tHosts, const char* sHost, uint16 iPort)
{
	size_t iStart = 0u;
	size_t i;
	const char* sPatterns = (const char*)tHosts.pData;
	size_t iHostLen;

	if ( !xsshSliceIsValid(tHosts) || sHost == NULL ) {
		return FALSE;
	}
	iHostLen = strlen(sHost);
	for ( i = 0u; i <= tHosts.iLen; ++i ) {
		if ( i == tHosts.iLen || sPatterns[i] == ',' ) {
			if ( i > iStart && xsshKnownHostOneMatches(sPatterns + iStart, i - iStart, sHost, iHostLen, iPort) ) {
				return TRUE;
			}
			iStart = i + 1u;
		}
	}
	return FALSE;
}

static int xsshKnownHostMakeLine(char* sOut, size_t iOutCap, const char* sHost, uint16 iPort, xsshslice tHostKeyBlob)
{
	xsshpublickey key;
	str sB64;
	int iWritten;

	if ( sOut == NULL || iOutCap == 0u || sHost == NULL || !xsshSliceIsValid(tHostKeyBlob) ) {
		return XSSH_ERR_INVALID;
	}
	if ( xsshPublicKeyReadEd25519(tHostKeyBlob.pData, tHostKeyBlob.iLen, &key) != XSSH_OK ) {
		return XSSH_ERR_UNSUPPORTED;
	}
	sB64 = xrtBase64Encode((ptr)tHostKeyBlob.pData, tHostKeyBlob.iLen, NULL);
	if ( sB64 == NULL ) {
		return XSSH_ERR_NO_SPACE;
	}
	if ( iPort == XSSH_DEFAULT_PORT ) {
		iWritten = snprintf(sOut, iOutCap, "%s ssh-ed25519 %s", sHost, sB64);
	} else {
		iWritten = snprintf(sOut, iOutCap, "[%s]:%u ssh-ed25519 %s", sHost, (unsigned)iPort, sB64);
	}
	xrtFree(sB64);
	if ( iWritten <= 0 || (size_t)iWritten >= iOutCap ) {
		return XSSH_ERR_NO_SPACE;
	}
	return XSSH_OK;
}

static int xsshHostKeyFingerprintSHA256(char* sOut, size_t iOutCap, xsshslice tHostKeyBlob)
{
	uint8 arrHash[XSSH_SHA256_SIZE];
	str sB64;
	size_t iB64Len;
	int iWritten;

	if ( sOut == NULL || iOutCap == 0u || !xsshSliceIsValid(tHostKeyBlob) ) {
		return XSSH_ERR_INVALID;
	}
	sOut[0] = '\0';
	xrtSHA256((const ptr)tHostKeyBlob.pData, tHostKeyBlob.iLen, arrHash);
	sB64 = xrtBase64Encode((ptr)arrHash, sizeof(arrHash), NULL);
	if ( sB64 == NULL ) {
		xsshSecureZero(arrHash, sizeof(arrHash));
		return XSSH_ERR_NO_SPACE;
	}
	iB64Len = strlen((const char*)sB64);
	while ( iB64Len > 0u && sB64[iB64Len - 1u] == '=' ) {
		iB64Len--;
	}
	/* OpenSSH 展示 SHA256 指纹时去掉 base64 padding，便于和 ssh-keygen 输出对照。 */
	iWritten = snprintf(sOut, iOutCap, "SHA256:%.*s", (int)iB64Len, sB64);
	xrtFree(sB64);
	xsshSecureZero(arrHash, sizeof(arrHash));
	if ( iWritten <= 0 || (size_t)iWritten >= iOutCap ) {
		return XSSH_ERR_NO_SPACE;
	}
	return XSSH_OK;
}

static uint32 xsshMd5Rotl(uint32 iValue, uint32 iBits)
{
	return (iValue << iBits) | (iValue >> (32u - iBits));
}

static uint32 xsshMd5ReadLE32(const uint8* pData)
{
	return ((uint32)pData[0]) |
		((uint32)pData[1] << 8u) |
		((uint32)pData[2] << 16u) |
		((uint32)pData[3] << 24u);
}

static void xsshMd5WriteLE32(uint8* pOut, uint32 iValue)
{
	pOut[0] = (uint8)(iValue & 0xffu);
	pOut[1] = (uint8)((iValue >> 8u) & 0xffu);
	pOut[2] = (uint8)((iValue >> 16u) & 0xffu);
	pOut[3] = (uint8)((iValue >> 24u) & 0xffu);
}

static void xsshMd5Block(uint32 arrState[4], const uint8 arrBlock[64])
{
	static const uint32 arrShift[64] = {
		7u,12u,17u,22u, 7u,12u,17u,22u, 7u,12u,17u,22u, 7u,12u,17u,22u,
		5u,9u,14u,20u, 5u,9u,14u,20u, 5u,9u,14u,20u, 5u,9u,14u,20u,
		4u,11u,16u,23u, 4u,11u,16u,23u, 4u,11u,16u,23u, 4u,11u,16u,23u,
		6u,10u,15u,21u, 6u,10u,15u,21u, 6u,10u,15u,21u, 6u,10u,15u,21u
	};
	static const uint32 arrK[64] = {
		0xd76aa478u,0xe8c7b756u,0x242070dbu,0xc1bdceeeu,0xf57c0fafu,0x4787c62au,0xa8304613u,0xfd469501u,
		0x698098d8u,0x8b44f7afu,0xffff5bb1u,0x895cd7beu,0x6b901122u,0xfd987193u,0xa679438eu,0x49b40821u,
		0xf61e2562u,0xc040b340u,0x265e5a51u,0xe9b6c7aau,0xd62f105du,0x02441453u,0xd8a1e681u,0xe7d3fbc8u,
		0x21e1cde6u,0xc33707d6u,0xf4d50d87u,0x455a14edu,0xa9e3e905u,0xfcefa3f8u,0x676f02d9u,0x8d2a4c8au,
		0xfffa3942u,0x8771f681u,0x6d9d6122u,0xfde5380cu,0xa4beea44u,0x4bdecfa9u,0xf6bb4b60u,0xbebfbc70u,
		0x289b7ec6u,0xeaa127fau,0xd4ef3085u,0x04881d05u,0xd9d4d039u,0xe6db99e5u,0x1fa27cf8u,0xc4ac5665u,
		0xf4292244u,0x432aff97u,0xab9423a7u,0xfc93a039u,0x655b59c3u,0x8f0ccc92u,0xffeff47du,0x85845dd1u,
		0x6fa87e4fu,0xfe2ce6e0u,0xa3014314u,0x4e0811a1u,0xf7537e82u,0xbd3af235u,0x2ad7d2bbu,0xeb86d391u
	};
	uint32 arrM[16];
	uint32 a = arrState[0];
	uint32 b = arrState[1];
	uint32 c = arrState[2];
	uint32 d = arrState[3];
	uint32 f;
	uint32 g;
	uint32 i;

	for ( i = 0u; i < 16u; ++i ) {
		arrM[i] = xsshMd5ReadLE32(arrBlock + i * 4u);
	}
	for ( i = 0u; i < 64u; ++i ) {
		if ( i < 16u ) {
			f = (b & c) | ((~b) & d);
			g = i;
		} else if ( i < 32u ) {
			f = (d & b) | ((~d) & c);
			g = (5u * i + 1u) & 15u;
		} else if ( i < 48u ) {
			f = b ^ c ^ d;
			g = (3u * i + 5u) & 15u;
		} else {
			f = c ^ (b | (~d));
			g = (7u * i) & 15u;
		}
		{
			uint32 t = d;
			d = c;
			c = b;
			b = b + xsshMd5Rotl(a + f + arrK[i] + arrM[g], arrShift[i]);
			a = t;
		}
	}
	arrState[0] += a;
	arrState[1] += b;
	arrState[2] += c;
	arrState[3] += d;
}

static int xsshCryptoMD5(const uint8* pData, size_t iLen, uint8 pOut[XSSH_MD5_SIZE])
{
	uint32 arrState[4];
	uint8 arrFinal[128];
	uint64 iBitLen;
	size_t iOffset = 0u;
	size_t iRemain;
	size_t iFinalLen;
	size_t i;

	if ( pOut == NULL || (pData == NULL && iLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	arrState[0] = 0x67452301u;
	arrState[1] = 0xefcdab89u;
	arrState[2] = 0x98badcfeu;
	arrState[3] = 0x10325476u;
	while ( iLen - iOffset >= 64u ) {
		xsshMd5Block(arrState, pData + iOffset);
		iOffset += 64u;
	}
	iRemain = iLen - iOffset;
	memset(arrFinal, 0, sizeof(arrFinal));
	if ( iRemain != 0u ) {
		memcpy(arrFinal, pData + iOffset, iRemain);
	}
	arrFinal[iRemain] = 0x80u;
	iFinalLen = (iRemain + 1u <= 56u) ? 64u : 128u;
	iBitLen = ((uint64)iLen) * 8u;
	for ( i = 0u; i < 8u; ++i ) {
		arrFinal[iFinalLen - 8u + i] = (uint8)((iBitLen >> (8u * i)) & 0xffu);
	}
	xsshMd5Block(arrState, arrFinal);
	if ( iFinalLen == 128u ) {
		xsshMd5Block(arrState, arrFinal + 64u);
	}
	for ( i = 0u; i < 4u; ++i ) {
		xsshMd5WriteLE32(pOut + i * 4u, arrState[i]);
	}
	xsshSecureZero(arrFinal, sizeof(arrFinal));
	return XSSH_OK;
}

static int xsshHostKeyFingerprintMD5(char* sOut, size_t iOutCap, xsshslice tHostKeyBlob)
{
	static const char sHex[] = "0123456789abcdef";
	uint8 arrHash[XSSH_MD5_SIZE];
	size_t iPos = 0u;
	size_t i;
	int iRet;

	if ( sOut == NULL || iOutCap == 0u || !xsshSliceIsValid(tHostKeyBlob) ) {
		return XSSH_ERR_INVALID;
	}
	sOut[0] = '\0';
	if ( iOutCap < 4u + XSSH_MD5_SIZE * 3u ) {
		return XSSH_ERR_NO_SPACE;
	}
	iRet = xsshCryptoMD5(tHostKeyBlob.pData, tHostKeyBlob.iLen, arrHash);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	/* MD5 指纹仅用于兼容旧式显示，不能作为新的安全决策依据。 */
	memcpy(sOut, "MD5:", 4u);
	iPos = 4u;
	for ( i = 0u; i < XSSH_MD5_SIZE; ++i ) {
		if ( i != 0u ) {
			sOut[iPos++] = ':';
		}
		sOut[iPos++] = sHex[arrHash[i] >> 4u];
		sOut[iPos++] = sHex[arrHash[i] & 0x0fu];
	}
	sOut[iPos] = '\0';
	xsshSecureZero(arrHash, sizeof(arrHash));
	return XSSH_OK;
}

static int xsshKnownHostsCheckText(const char* sText, size_t iLen, const char* sHost, uint16 iPort, xsshslice tHostKeyBlob,
	int iPolicy, bool* pAcceptedNew, char* sAcceptNewLine, size_t iAcceptNewCap)
{
	uint8 arrDecoded[4096];
	xsshpublickey hostKey;
	bool bMatchedHost = FALSE;
	bool bMatchedKey = FALSE;
	size_t iStart = 0u;

	if ( sHost == NULL || !xsshSliceIsValid(tHostKeyBlob) || (sText == NULL && iLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	if ( pAcceptedNew != NULL ) {
		*pAcceptedNew = FALSE;
	}
	if ( xsshPublicKeyReadEd25519(tHostKeyBlob.pData, tHostKeyBlob.iLen, &hostKey) != XSSH_OK ) {
		return XSSH_ERR_UNSUPPORTED;
	}
	if ( iPolicy == XSSH_HOSTKEY_INSECURE ) {
		return XSSH_OK;
	}

	while ( iStart < iLen ) {
		const char* sLine;
		size_t iEnd = iStart;
		size_t iLineLen;
		xsshknownhostline header;
		xsshknownhostline line;
		int iRet;

		while ( iEnd < iLen && sText[iEnd] != '\n' ) {
			iEnd++;
		}
		sLine = sText + iStart;
		iLineLen = iEnd - iStart;
		xsshLineTrim(&sLine, &iLineLen);
		iStart = iEnd + 1u;
		if ( iLineLen == 0u || sLine[0] == '#' ) {
			continue;
		}
		iRet = xsshKnownHostLineReadHeader(sLine, iLineLen, &header);
		if ( iRet == XSSH_ERR_INVALID ) {
			continue;
		}
		if ( iRet != XSSH_OK ) {
			return iRet;
		}
		if ( !xsshKnownHostMatches(header.tHosts, sHost, iPort) ) {
			continue;
		}
		/*
		 * known_hosts 允许同一个主机保存多种 host key。不同算法的记录不能
		 * 触发 mismatch，否则以后接入 RSA/ECDSA 后会把正常轮换误判成中间人风险。
		 */
		if ( !xsshSliceEqTextN(header.tAlgorithm, (const char*)hostKey.tAlgorithm.pData, hostKey.tAlgorithm.iLen) ) {
			continue;
		}
		bMatchedHost = TRUE;
		iRet = xsshKnownHostLineRead(sLine, iLineLen, arrDecoded, sizeof(arrDecoded), &line);
		if ( iRet == XSSH_ERR_UNSUPPORTED || iRet == XSSH_ERR_INVALID ) {
			continue;
		}
		if ( iRet != XSSH_OK ) {
			return iRet;
		}
		if ( line.tBlob.iLen == tHostKeyBlob.iLen &&
			memcmp(line.tBlob.pData, tHostKeyBlob.pData, tHostKeyBlob.iLen) == 0 ) {
			bMatchedKey = TRUE;
			break;
		}
	}

	if ( bMatchedKey ) {
		return XSSH_OK;
	}
	if ( bMatchedHost ) {
		return XSSH_ERR_HOSTKEY_MISMATCH;
	}
	if ( iPolicy == XSSH_HOSTKEY_ACCEPT_NEW ) {
		int iRet = XSSH_OK;
		if ( sAcceptNewLine != NULL && iAcceptNewCap != 0u ) {
			iRet = xsshKnownHostMakeLine(sAcceptNewLine, iAcceptNewCap, sHost, iPort, tHostKeyBlob);
		}
		if ( iRet == XSSH_OK && pAcceptedNew != NULL ) {
			*pAcceptedNew = TRUE;
		}
		return iRet;
	}
	return XSSH_ERR_HOSTKEY_UNKNOWN;
}

static void xsshRuntimeSetResult(xssh_session_t* pSession, bool bSuccess, int iError, int iStage, const char* sText)
{
	if ( pSession == NULL ) {
		return;
	}
	pSession->tResult.bSuccess = bSuccess;
	pSession->tResult.iError = iError;
	pSession->tResult.iStage = iStage;
	if ( sText != NULL ) {
		snprintf(pSession->tResult.sError, sizeof(pSession->tResult.sError), "%s", sText);
	}
}

static int xsshRuntimePushEvent(xssh_session_t* pSession, int iType, xssh_channel_t* pChannel, const void* pData, size_t iLen, int iCode, const char* sText)
{
	xsshevententry* pEntry;
	size_t iTail;

	if ( pSession == NULL || (pData == NULL && iLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	if ( iLen > XSSH_RUNTIME_EVENT_DATA_CAP ) {
		return XSSH_ERR_NO_SPACE;
	}
	if ( pSession->iEventCount >= XSSH_RUNTIME_EVENT_CAP ) {
		return XSSH_ERR_NO_SPACE;
	}
	iTail = (pSession->iEventHead + pSession->iEventCount) % XSSH_RUNTIME_EVENT_CAP;
	pEntry = &pSession->arrEvents[iTail];
	memset(pEntry, 0, sizeof(*pEntry));
	pEntry->tEvent.iType = iType;
	pEntry->tEvent.pChannel = pChannel;
	pEntry->tEvent.iCode = iCode;
	if ( iLen != 0u ) {
		memcpy(pEntry->arrData, pData, iLen);
		pEntry->iDataLen = iLen;
		pEntry->tEvent.pData = pEntry->arrData;
		pEntry->tEvent.iLen = iLen;
	}
	if ( sText != NULL ) {
		snprintf(pEntry->tEvent.sText, sizeof(pEntry->tEvent.sText), "%s", sText);
	}
	pSession->iEventCount++;
	return XSSH_OK;
}

static int xsshRuntimeAppendChannelData(xssh_channel_t* pChannel, bool bStderr, const void* pData, size_t iLen)
{
	uint8* pBuf;
	size_t* pLen;
	size_t* pPos;
	int iEvent;
	int iRet;

	if ( pChannel == NULL || (pData == NULL && iLen != 0u) || pChannel->pSession == NULL ) {
		return XSSH_ERR_INVALID;
	}
	pBuf = bStderr ? pChannel->arrStderr : pChannel->arrStdout;
	pLen = bStderr ? &pChannel->iStderrLen : &pChannel->iStdoutLen;
	pPos = bStderr ? &pChannel->iStderrPos : &pChannel->iStdoutPos;
	if ( *pPos > *pLen ) {
		return XSSH_ERR_INVALID;
	}
	if ( *pPos != 0u ) {
		size_t iRemain = *pLen - *pPos;

		/*
		 * channel 是长生命周期字节流。读 API 只移动读指针，如果这里不回收前缀空间，
		 * exec 小输出没问题，但 shell/port-forwarding 会在持续读写后把固定缓冲耗尽。
		 */
		if ( iRemain != 0u ) {
			memmove(pBuf, pBuf + *pPos, iRemain);
		}
		*pLen = iRemain;
		*pPos = 0u;
	}
	if ( iLen > XSSH_RUNTIME_CHANNEL_BUFFER_CAP - *pLen ) {
		return XSSH_ERR_NO_SPACE;
	}
	if ( iLen != 0u ) {
		memcpy(pBuf + *pLen, pData, iLen);
		*pLen += iLen;
	}
	iEvent = bStderr ? XSSH_EVENT_CHANNEL_EXTENDED_DATA : XSSH_EVENT_CHANNEL_DATA;
	iRet = xsshRuntimePushEvent(pChannel->pSession, iEvent, pChannel, pData, iLen, bStderr ? 1 : 0, NULL);
	return iRet;
}

static int xsshRuntimeChannelReadBuffer(uint8* pBuf, size_t iLen, size_t* pPos, void* pOut, size_t iCap, size_t* pRead)
{
	size_t iAvail;
	size_t iTake;

	if ( pRead == NULL || (pOut == NULL && iCap != 0u) || pPos == NULL ) {
		return XSSH_ERR_INVALID;
	}
	*pRead = 0u;
	if ( *pPos > iLen ) {
		return XSSH_ERR_INVALID;
	}
	iAvail = iLen - *pPos;
	iTake = (iAvail < iCap) ? iAvail : iCap;
	if ( iTake != 0u ) {
		memcpy(pOut, pBuf + *pPos, iTake);
		*pPos += iTake;
	}
	*pRead = iTake;
	return XSSH_OK;
}

static bool xsshRuntimeIsMockHost(const xsshconfig* pCfg)
{
	if ( pCfg == NULL || pCfg->sHost == NULL ) {
		return FALSE;
	}
	return (strcmp(pCfg->sHost, "mock") == 0 || strcmp(pCfg->sHost, "xssh-mock") == 0 || strcmp(pCfg->sHost, "localhost-mock") == 0) ? TRUE : FALSE;
}

static uint64 xsshTransportNowMs(void)
{
	double fNow = xrtTimer();
	if ( fNow <= 0.0 ) {
		return 0u;
	}
	return (uint64)(fNow * 1000.0);
}

static void xsshTransportSignal(xssh_session_t* pSession)
{
	if ( pSession == NULL || pSession->pCond == NULL ) {
		return;
	}
	xrtCondBroadcast(pSession->pCond);
}

static void xsshTransportOnOpen(ptr pOwner, xnetstream* pStream)
{
	xssh_session_t* pSession = (xssh_session_t*)pOwner;
	(void)pStream;
	if ( pSession == NULL || pSession->pLock == NULL ) {
		return;
	}
	xrtMutexLock(pSession->pLock);
	pSession->bStreamOpen = TRUE;
	pSession->iTransportState = XSSH_TRANSPORT_STATE_OPEN;
	xsshTransportSignal(pSession);
	xrtMutexUnlock(pSession->pLock);
}

static void xsshTransportOnRecv(ptr pOwner, xnetstream* pStream, xnetchain* pChain)
{
	xssh_session_t* pSession = (xssh_session_t*)pOwner;
	uint8 arrBuf[4096];
	size_t iLeft;
	(void)pStream;

	if ( pSession == NULL || pSession->pLock == NULL || pChain == NULL ) {
		return;
	}
	xrtMutexLock(pSession->pLock);
	iLeft = xrtNetChainBytes(pChain);
	while ( iLeft > 0u ) {
		size_t iChunk = (iLeft > sizeof(arrBuf)) ? sizeof(arrBuf) : iLeft;
		size_t iRead = xrtNetChainPeek(pChain, arrBuf, iChunk);
		if ( iRead == 0u ) {
			break;
		}
		if ( iRead > XSSH_RUNTIME_WIRE_RECV_CAP - pSession->iWireRecvLen ) {
			pSession->bRecvOverflow = TRUE;
			xsshTransportSignal(pSession);
			xrtMutexUnlock(pSession->pLock);
			xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
			return;
		}
		memcpy(pSession->arrWireRecv + pSession->iWireRecvLen, arrBuf, iRead);
		pSession->iWireRecvLen += iRead;
		xrtNetChainConsume(pChain, iRead);
		iLeft -= iRead;
	}
	xsshTransportSignal(pSession);
	xrtMutexUnlock(pSession->pLock);
}

static void xsshTransportOnClose(ptr pOwner, xnetstream* pStream, xnet_result iReason)
{
	xssh_session_t* pSession = (xssh_session_t*)pOwner;
	(void)pStream;
	if ( pSession == NULL || pSession->pLock == NULL ) {
		return;
	}
	xrtMutexLock(pSession->pLock);
	pSession->bStreamClosed = TRUE;
	pSession->iCloseReason = (int)iReason;
	pSession->iTransportState = XSSH_TRANSPORT_STATE_CLOSED;
	xsshTransportSignal(pSession);
	xrtMutexUnlock(pSession->pLock);
}

static void xsshTransportOnError(ptr pOwner, xnetstream* pStream, int iSysErr)
{
	xssh_session_t* pSession = (xssh_session_t*)pOwner;
	(void)pStream;
	if ( pSession == NULL || pSession->pLock == NULL ) {
		return;
	}
	xrtMutexLock(pSession->pLock);
	pSession->iLastSysErr = iSysErr;
	xsshTransportSignal(pSession);
	xrtMutexUnlock(pSession->pLock);
}

static const xnetstreamevents* xsshTransportStreamEvents(void)
{
	static const xnetstreamevents tEvents = {
		xsshTransportOnOpen,
		xsshTransportOnRecv,
		NULL,
		xsshTransportOnClose,
		xsshTransportOnError,
		NULL,
		NULL
	};
	return &tEvents;
}

static bool xsshTransportWaitConnectState(xssh_session_t* pSession, uint64 iDeadlineMs)
{
	if ( pSession == NULL || pSession->pLock == NULL || pSession->pCond == NULL ) {
		return FALSE;
	}
	for (;;) {
		uint64 iNow;
		uint32 iRemain;
		int iWaitRet;

		if ( pSession->bStreamOpen || pSession->bStreamClosed || pSession->iLastSysErr != 0 ) {
			return TRUE;
		}
		if ( iDeadlineMs == 0u ) {
			xrtCondWait(pSession->pCond, pSession->pLock);
			continue;
		}
		iNow = xsshTransportNowMs();
		if ( iNow >= iDeadlineMs ) {
			return FALSE;
		}
		iRemain = (uint32)(iDeadlineMs - iNow);
		if ( iRemain == 0u ) {
			return FALSE;
		}
		iWaitRet = xrtCondWaitTimeout(pSession->pCond, pSession->pLock, iRemain);
		if ( iWaitRet == XRT_WAIT_TIMEOUT || iWaitRet == XRT_WAIT_ERROR ) {
			return FALSE;
		}
	}
}

static bool xsshTransportWaitRecvState(xssh_session_t* pSession, uint64 iDeadlineMs)
{
	if ( pSession == NULL || pSession->pLock == NULL || pSession->pCond == NULL ) {
		return FALSE;
	}
	for (;;) {
		uint64 iNow;
		uint32 iRemain;
		int iWaitRet;

		if ( pSession->iWireRecvLen > 0u || pSession->bStreamClosed || pSession->iLastSysErr != 0 || pSession->bRecvOverflow ) {
			return TRUE;
		}
		if ( iDeadlineMs == 0u ) {
			xrtCondWait(pSession->pCond, pSession->pLock);
			continue;
		}
		iNow = xsshTransportNowMs();
		if ( iNow >= iDeadlineMs ) {
			return FALSE;
		}
		iRemain = (uint32)(iDeadlineMs - iNow);
		if ( iRemain == 0u ) {
			return FALSE;
		}
		iWaitRet = xrtCondWaitTimeout(pSession->pCond, pSession->pLock, iRemain);
		if ( iWaitRet == XRT_WAIT_TIMEOUT || iWaitRet == XRT_WAIT_ERROR ) {
			return FALSE;
		}
	}
}

static void xsshTransportCloseDestroy(xssh_session_t* pSession)
{
	if ( pSession == NULL ) {
		return;
	}
	if ( pSession->pStream != NULL ) {
		xrtNetStreamClose(pSession->pStream, XNET_CLOSE_F_ABORT);
		(void)xrtNetStreamWaitTimeoutEx(pSession->pStream, XNET_STREAM_WAIT_CLOSE,
			pSession->iTransportTimeoutMs ? pSession->iTransportTimeoutMs : 1000u);
		if ( xrtGetError() ) {
			xrtClearError();
		}
		xrtNetStreamDestroy(pSession->pStream);
		if ( xrtGetError() ) {
			xrtClearError();
		}
		pSession->pStream = NULL;
	}
	if ( pSession->pEngine != NULL ) {
		if ( pSession->bOwnEngine ) {
			xrtNetEngineDestroy(pSession->pEngine);
		}
		pSession->pEngine = NULL;
	}
	if ( pSession->pCond != NULL ) {
		xrtCondDestroy(pSession->pCond);
		pSession->pCond = NULL;
	}
	if ( pSession->pLock != NULL ) {
		xrtMutexDestroy(pSession->pLock);
		pSession->pLock = NULL;
	}
	pSession->bOwnEngine = FALSE;
	pSession->bStreamOpen = FALSE;
	pSession->bStreamClosed = FALSE;
	pSession->bRecvOverflow = FALSE;
	pSession->iCloseReason = 0;
	pSession->iLastSysErr = 0;
	pSession->iWireRecvLen = 0u;
	xsshPacketCodecClear(&pSession->tPacketCodec);
}

static int xsshTransportConnectTcp(xssh_session_t* pSession, const xsshconfig* pCfg)
{
	xnetengineconfig tEngineCfg;
	xnetconnectconfig tConnCfg;
	uint64 iDeadlineMs;
	str sErr;
	const char* sErrText;

	if ( pSession == NULL || pCfg == NULL || pCfg->sHost == NULL || pCfg->sHost[0] == '\0' ) {
		return XSSH_ERR_INVALID;
	}
	(void)xrtInit();
	pSession->iTransportTimeoutMs = pCfg->iConnectTimeoutMs ? pCfg->iConnectTimeoutMs : XSSH_DEFAULT_TIMEOUT_MS;
	pSession->pLock = xrtMutexCreate();
	pSession->pCond = xrtCondCreate();
	if ( pSession->pLock == NULL || pSession->pCond == NULL ) {
		xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_NO_SPACE, XSSH_STAGE_CONNECT, "xssh transport sync init failed");
		return XSSH_ERR_NO_SPACE;
	}
	xrtNetEngineConfigInit(&tEngineCfg);
	tEngineCfg.iWorkerCount = 1u;
	pSession->pEngine = xrtNetEngineCreate(&tEngineCfg);
	if ( pSession->pEngine == NULL ) {
		sErr = xrtGetError();
		sErrText = (sErr && sErr[0]) ? (const char*)sErr : "";
		snprintf(pSession->tResult.sError, sizeof(pSession->tResult.sError), "xssh xnet engine create failed%s%s", (sErrText[0] != '\0') ? ": " : "", sErrText);
		pSession->tResult.bSuccess = FALSE;
		pSession->tResult.iError = XSSH_ERR_IO;
		pSession->tResult.iStage = XSSH_STAGE_CONNECT;
		return XSSH_ERR_IO;
	}
	pSession->bOwnEngine = TRUE;
	if ( xrtNetEngineStart(pSession->pEngine) != XRT_NET_OK ) {
		sErr = xrtGetError();
		sErrText = (sErr && sErr[0]) ? (const char*)sErr : "";
		snprintf(pSession->tResult.sError, sizeof(pSession->tResult.sError), "xssh xnet engine start failed%s%s", (sErrText[0] != '\0') ? ": " : "", sErrText);
		pSession->tResult.bSuccess = FALSE;
		pSession->tResult.iError = XSSH_ERR_IO;
		pSession->tResult.iStage = XSSH_STAGE_CONNECT;
		return XSSH_ERR_IO;
	}
	pSession->pStream = xrtNetStreamCreate(pSession->pEngine, xsshTransportStreamEvents(), pSession);
	if ( pSession->pStream == NULL ) {
		sErr = xrtGetError();
		sErrText = (sErr && sErr[0]) ? (const char*)sErr : "";
		snprintf(pSession->tResult.sError, sizeof(pSession->tResult.sError), "xssh xnet stream create failed%s%s", (sErrText[0] != '\0') ? ": " : "", sErrText);
		pSession->tResult.bSuccess = FALSE;
		pSession->tResult.iError = XSSH_ERR_IO;
		pSession->tResult.iStage = XSSH_STAGE_CONNECT;
		return XSSH_ERR_IO;
	}
	xrtNetConnectConfigInit(&tConnCfg);
	tConnCfg.sHost = pCfg->sHost;
	tConnCfg.iPort = pCfg->iPort ? pCfg->iPort : (uint16)XSSH_DEFAULT_PORT;
	tConnCfg.iConnectTimeoutMs = pSession->iTransportTimeoutMs;
	tConnCfg.iRecvLimit = XSSH_RUNTIME_WIRE_RECV_CAP;
	pSession->iTransportState = XSSH_TRANSPORT_STATE_CONNECTING;
	if ( xrtNetStreamConnect(pSession->pStream, &tConnCfg) != XRT_NET_OK ) {
		sErr = xrtGetError();
		sErrText = (sErr && sErr[0]) ? (const char*)sErr : "";
		snprintf(pSession->tResult.sError, sizeof(pSession->tResult.sError), "xssh xnet stream connect submit failed%s%s", (sErrText[0] != '\0') ? ": " : "", sErrText);
		pSession->tResult.bSuccess = FALSE;
		pSession->tResult.iError = XSSH_ERR_IO;
		pSession->tResult.iStage = XSSH_STAGE_CONNECT;
		return XSSH_ERR_IO;
	}
	iDeadlineMs = xsshTransportNowMs() + pSession->iTransportTimeoutMs;
	xrtMutexLock(pSession->pLock);
	if ( !xsshTransportWaitConnectState(pSession, iDeadlineMs) ) {
		xrtMutexUnlock(pSession->pLock);
		xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_TIMEOUT, XSSH_STAGE_CONNECT, "xssh xnet stream connect timeout");
		return XSSH_ERR_TIMEOUT;
	}
	if ( pSession->iLastSysErr != 0 ) {
		int iSysErr = pSession->iLastSysErr;
		xrtMutexUnlock(pSession->pLock);
		snprintf(pSession->tResult.sError, sizeof(pSession->tResult.sError), "xssh xnet stream connect failed (sys=%d)", iSysErr);
		pSession->tResult.bSuccess = FALSE;
		pSession->tResult.iError = XSSH_ERR_IO;
		pSession->tResult.iStage = XSSH_STAGE_CONNECT;
		return XSSH_ERR_IO;
	}
	if ( pSession->bStreamClosed && !pSession->bStreamOpen ) {
		int iReason = pSession->iCloseReason;
		xrtMutexUnlock(pSession->pLock);
		snprintf(pSession->tResult.sError, sizeof(pSession->tResult.sError), "xssh xnet stream closed during connect (reason=%d)", iReason);
		pSession->tResult.bSuccess = FALSE;
		pSession->tResult.iError = XSSH_ERR_IO;
		pSession->tResult.iStage = XSSH_STAGE_CONNECT;
		return XSSH_ERR_IO;
	}
	xrtMutexUnlock(pSession->pLock);
	pSession->bConnected = TRUE;
	return XSSH_OK;
}

static int xsshTransportSendAll(xssh_session_t* pSession, const void* pData, size_t iLen)
{
	xnet_result iWaitRet;
	str sErr;
	const char* sErrText;

	if ( pSession == NULL || pSession->pStream == NULL || pData == NULL || iLen == 0u ) {
		return XSSH_ERR_INVALID;
	}
	if ( xrtNetStreamSend(pSession->pStream, pData, iLen) != XRT_NET_OK ) {
		sErr = xrtGetError();
		sErrText = (sErr && sErr[0]) ? (const char*)sErr : "";
		snprintf(pSession->tResult.sError, sizeof(pSession->tResult.sError), "xssh stream send failed%s%s", (sErrText[0] != '\0') ? ": " : "", sErrText);
		pSession->tResult.bSuccess = FALSE;
		pSession->tResult.iError = XSSH_ERR_IO;
		pSession->tResult.iStage = XSSH_STAGE_BANNER;
		return XSSH_ERR_IO;
	}
	iWaitRet = xrtNetStreamWaitTimeoutEx(pSession->pStream, XNET_STREAM_WAIT_DRAIN,
		pSession->iTransportTimeoutMs ? pSession->iTransportTimeoutMs : XSSH_DEFAULT_TIMEOUT_MS);
	if ( iWaitRet != XRT_NET_OK && xrtNetStreamPendingSend(pSession->pStream) != 0u ) {
		xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_TIMEOUT, XSSH_STAGE_BANNER, "xssh stream send drain timeout");
		return XSSH_ERR_TIMEOUT;
	}
	return XSSH_OK;
}

static int xsshTransportRecvSome(xssh_session_t* pSession, void* pOut, size_t iCap, size_t* pRead)
{
	uint64 iDeadlineMs;
	size_t iCopy;

	if ( pRead != NULL ) {
		*pRead = 0u;
	}
	if ( pSession == NULL || pOut == NULL || iCap == 0u || pRead == NULL ) {
		return XSSH_ERR_INVALID;
	}
	iDeadlineMs = xsshTransportNowMs() + (pSession->iTransportTimeoutMs ? pSession->iTransportTimeoutMs : XSSH_DEFAULT_TIMEOUT_MS);
	xrtMutexLock(pSession->pLock);
	for (;;) {
		if ( pSession->iWireRecvLen > 0u ) {
			break;
		}
		if ( pSession->bRecvOverflow ) {
			xrtMutexUnlock(pSession->pLock);
			xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_NO_SPACE, XSSH_STAGE_BANNER, "xssh recv buffer overflow");
			return XSSH_ERR_NO_SPACE;
		}
		if ( pSession->iLastSysErr != 0 ) {
			int iSysErr = pSession->iLastSysErr;
			xrtMutexUnlock(pSession->pLock);
			snprintf(pSession->tResult.sError, sizeof(pSession->tResult.sError), "xssh recv failed (sys=%d)", iSysErr);
			pSession->tResult.bSuccess = FALSE;
			pSession->tResult.iError = XSSH_ERR_IO;
			pSession->tResult.iStage = XSSH_STAGE_BANNER;
			return XSSH_ERR_IO;
		}
		if ( pSession->bStreamClosed ) {
			int iReason = pSession->iCloseReason;
			xrtMutexUnlock(pSession->pLock);
			snprintf(pSession->tResult.sError, sizeof(pSession->tResult.sError), "xssh stream closed while reading banner (reason=%d)", iReason);
			pSession->tResult.bSuccess = FALSE;
			pSession->tResult.iError = XSSH_ERR_IO;
			pSession->tResult.iStage = XSSH_STAGE_BANNER;
			return XSSH_ERR_IO;
		}
		if ( !xsshTransportWaitRecvState(pSession, iDeadlineMs) ) {
			xrtMutexUnlock(pSession->pLock);
			xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_TIMEOUT, XSSH_STAGE_BANNER, "xssh banner recv timeout");
			return XSSH_ERR_TIMEOUT;
		}
	}
	iCopy = (pSession->iWireRecvLen > iCap) ? iCap : pSession->iWireRecvLen;
	memcpy(pOut, pSession->arrWireRecv, iCopy);
	if ( iCopy < pSession->iWireRecvLen ) {
		memmove(pSession->arrWireRecv, pSession->arrWireRecv + iCopy, pSession->iWireRecvLen - iCopy);
	}
	pSession->iWireRecvLen -= iCopy;
	xrtMutexUnlock(pSession->pLock);
	*pRead = iCopy;
	return XSSH_OK;
}

static int xsshTransportReadLine(xssh_session_t* pSession, char* sOut, size_t iOutCap)
{
	size_t iLen = 0u;

	if ( pSession == NULL || sOut == NULL || iOutCap == 0u ) {
		return XSSH_ERR_INVALID;
	}
	for (;;) {
		char ch = '\0';
		size_t iRead = 0u;
		int iRet = xsshTransportRecvSome(pSession, &ch, 1u, &iRead);
		if ( iRet != XSSH_OK ) {
			return iRet;
		}
		if ( iRead != 1u ) {
			return XSSH_WIRE_NEED_MORE;
		}
		if ( ch == '\n' ) {
			if ( iLen > 0u && sOut[iLen - 1u] == '\r' ) {
				iLen--;
			}
			sOut[iLen] = '\0';
			return XSSH_OK;
		}
		if ( iLen + 1u >= iOutCap ) {
			xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_BAD_PACKET, XSSH_STAGE_BANNER, "xssh server banner line too long");
			return XSSH_ERR_BAD_PACKET;
		}
		sOut[iLen++] = ch;
	}
}

static int xsshTransportIdentificationExchange(xssh_session_t* pSession)
{
	char sLine[XSSH_BANNER_MAX + 1u];
	char sClientLine[sizeof(XSSH_CLIENT_BANNER) + 2u];
	int i;
	int iRet;

	if ( pSession == NULL ) {
		return XSSH_ERR_INVALID;
	}
	snprintf(sClientLine, sizeof(sClientLine), "%s\r\n", XSSH_CLIENT_BANNER);
	iRet = xsshTransportSendAll(pSession, sClientLine, strlen(sClientLine));
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	for ( i = 0; i < 32; ++i ) {
		iRet = xsshTransportReadLine(pSession, sLine, sizeof(sLine));
		if ( iRet != XSSH_OK ) {
			return iRet;
		}
		if ( strncmp(sLine, "SSH-", 4) != 0 ) {
			continue;
		}
		if ( strncmp(sLine, "SSH-2.0-", 8) != 0 && strncmp(sLine, "SSH-1.99-", 9) != 0 ) {
			xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_UNSUPPORTED, XSSH_STAGE_BANNER, "server does not advertise SSH-2 compatible protocol");
			return XSSH_ERR_UNSUPPORTED;
		}
		snprintf(pSession->tResult.sServerBanner, sizeof(pSession->tResult.sServerBanner), "%s", sLine);
		pSession->iTransportState = XSSH_TRANSPORT_STATE_IDENTIFIED;
		return XSSH_OK;
	}
	xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_BAD_PACKET, XSSH_STAGE_BANNER, "server banner not found before pre-banner line limit");
	return XSSH_ERR_BAD_PACKET;
}

static void xsshTransportCopySliceText(char* sOut, size_t iOutCap, xsshslice tText)
{
	size_t iCopy;

	if ( sOut == NULL || iOutCap == 0u ) {
		return;
	}
	iCopy = (tText.iLen < iOutCap - 1u) ? tText.iLen : iOutCap - 1u;
	if ( iCopy != 0u ) {
		memcpy(sOut, tText.pData, iCopy);
	}
	sOut[iCopy] = '\0';
}

static const char* xsshKnownHostsDefaultPath(char* sOut, size_t iOutCap)
{
	const char* sHome = NULL;

	if ( sOut == NULL || iOutCap == 0u ) {
		return NULL;
	}
#if defined(_WIN32) || defined(_WIN64)
	sHome = getenv("USERPROFILE");
	if ( sHome != NULL && sHome[0] != '\0' ) {
		int iWritten = snprintf(sOut, iOutCap, "%s\\.ssh\\known_hosts", sHome);
		if ( iWritten > 0 && (size_t)iWritten < iOutCap ) {
			return sOut;
		}
	}
#endif
	sHome = getenv("HOME");
	if ( sHome != NULL && sHome[0] != '\0' ) {
		int iWritten = snprintf(sOut, iOutCap, "%s/.ssh/known_hosts", sHome);
		if ( iWritten > 0 && (size_t)iWritten < iOutCap ) {
			return sOut;
		}
	}
	return NULL;
}

static int xsshKnownHostsCreateParentDir(const char* sPath)
{
	char sDir[512];
	size_t iLen;
	size_t iSep = (size_t)-1;
	size_t i;

	if ( sPath == NULL || sPath[0] == '\0' ) {
		return XSSH_ERR_INVALID;
	}
	iLen = strlen(sPath);
	for ( i = iLen; i-- > 0u; ) {
		if ( sPath[i] == '/' || sPath[i] == '\\' ) {
			iSep = i;
			break;
		}
	}
	if ( iSep == (size_t)-1 || iSep == 0u ) {
		return XSSH_OK;
	}
	if ( iSep >= sizeof(sDir) ) {
		return XSSH_ERR_NO_SPACE;
	}
	memcpy(sDir, sPath, iSep);
	sDir[iSep] = '\0';
	if ( xrtDirExists((str)sDir) || xrtDirCreateAll((str)sDir) ) {
		return XSSH_OK;
	}
	if ( xrtGetError() ) {
		xrtClearError();
	}
	return XSSH_ERR_IO;
}

static int xsshKnownHostsMakeLockPath(char* sOut, size_t iOutCap, const char* sPath)
{
	int iWritten;

	if ( sOut == NULL || iOutCap == 0u || sPath == NULL || sPath[0] == '\0' ) {
		return XSSH_ERR_INVALID;
	}
	iWritten = snprintf(sOut, iOutCap, "%s.lock", sPath);
	return (iWritten > 0 && (size_t)iWritten < iOutCap) ? XSSH_OK : XSSH_ERR_NO_SPACE;
}

static int xsshKnownHostsTryCreateLockFile(const char* sLockPath)
{
	int iFd;

	if ( sLockPath == NULL || sLockPath[0] == '\0' ) {
		return XSSH_ERR_INVALID;
	}
#if defined(_WIN32) || defined(_WIN64)
	iFd = _open(sLockPath, _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY, _S_IREAD | _S_IWRITE);
	if ( iFd >= 0 ) {
		_close(iFd);
		return XSSH_OK;
	}
#else
	iFd = open(sLockPath, O_WRONLY | O_CREAT | O_EXCL, 0600);
	if ( iFd >= 0 ) {
		close(iFd);
		return XSSH_OK;
	}
#endif
	return (errno == EEXIST || errno == EACCES) ? XSSH_WIRE_NEED_MORE : XSSH_ERR_IO;
}

static int xsshKnownHostsAcquireAppendLock(const char* sPath, char* sLockPath, size_t iLockCap)
{
	uint32 i;
	int iRet;

	iRet = xsshKnownHostsMakeLockPath(sLockPath, iLockCap, sPath);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	for ( i = 0u; i < 200u; ++i ) {
		iRet = xsshKnownHostsTryCreateLockFile(sLockPath);
		if ( iRet == XSSH_OK ) {
			return XSSH_OK;
		}
		if ( iRet != XSSH_WIRE_NEED_MORE ) {
			return iRet;
		}
		/* accept-new 只在首次信任主机时写文件；短暂轮询能覆盖并发连接，不把锁机制做成后台服务。 */
		xrtSleep(10u);
	}
	return XSSH_ERR_TIMEOUT;
}

static void xsshKnownHostsReleaseAppendLock(const char* sLockPath)
{
	if ( sLockPath != NULL && sLockPath[0] != '\0' ) {
		if ( !xrtFileDelete((str)sLockPath) && xrtGetError() ) {
			xrtClearError();
		}
	}
}

static int xsshTransportCheckHostKey(xssh_session_t* pSession, xsshslice tHostKeyBlob)
{
	char sDefaultPath[512];
	char sAcceptLine[8192];
	char sAppendLine[8200];
	char sLockPath[512];
	const char* sPath;
	char* sText = NULL;
	size_t iTextLen = 0u;
	bool bAcceptedNew = FALSE;
	int iRet;

	if ( pSession == NULL || !xsshSliceIsValid(tHostKeyBlob) ) {
		return XSSH_ERR_INVALID;
	}
	(void)xsshHostKeyFingerprintSHA256(pSession->tResult.sHostKeyFingerprint,
		sizeof(pSession->tResult.sHostKeyFingerprint), tHostKeyBlob);
	if ( pSession->tConfig.iHostKeyPolicy == XSSH_HOSTKEY_INSECURE ) {
		pSession->tResult.bHostKeyInsecure = TRUE;
		return XSSH_OK;
	}
	sPath = pSession->tConfig.sKnownHostsPath;
	if ( sPath == NULL || sPath[0] == '\0' ) {
		sPath = xsshKnownHostsDefaultPath(sDefaultPath, sizeof(sDefaultPath));
	}
	if ( sPath != NULL && sPath[0] != '\0' ) {
		sText = (char*)xrtFileGetAll((str)sPath, &iTextLen);
		if ( sText == NULL ) {
			iTextLen = 0u;
			if ( xrtGetError() ) {
				xrtClearError();
			}
		}
	}

	/*
	 * host key 必须在进入 USERAUTH 前固定下来。strict 缺失记录直接失败；
	 * accept-new 只追加新主机，mismatch 仍拒绝，避免静默覆盖已信任密钥。
	 */
	iRet = xsshKnownHostsCheckText(sText, iTextLen,
		pSession->tConfig.sHost,
		pSession->tConfig.iPort ? pSession->tConfig.iPort : (uint16)XSSH_DEFAULT_PORT,
		tHostKeyBlob,
		pSession->tConfig.iHostKeyPolicy,
		&bAcceptedNew,
		sAcceptLine,
		sizeof(sAcceptLine));
	if ( sText != NULL ) {
		xrtFree(sText);
	}
	if ( iRet == XSSH_OK && bAcceptedNew ) {
		int iWritten;

		if ( sPath == NULL || sPath[0] == '\0' ) {
			xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_HOSTKEY_UNKNOWN, XSSH_STAGE_HOSTKEY, "known_hosts path is required for accept-new host key");
			return XSSH_ERR_HOSTKEY_UNKNOWN;
		}
		iRet = xsshKnownHostsCreateParentDir(sPath);
		if ( iRet != XSSH_OK ) {
			xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_HOSTKEY, "known_hosts directory create failed");
			return iRet;
		}
		iRet = xsshKnownHostsAcquireAppendLock(sPath, sLockPath, sizeof(sLockPath));
		if ( iRet != XSSH_OK ) {
			xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_HOSTKEY, "known_hosts append lock failed");
			return iRet;
		}
		sText = (char*)xrtFileGetAll((str)sPath, &iTextLen);
		if ( sText == NULL ) {
			iTextLen = 0u;
			if ( xrtGetError() ) {
				xrtClearError();
			}
		}
		bAcceptedNew = FALSE;
		iRet = xsshKnownHostsCheckText(sText, iTextLen,
			pSession->tConfig.sHost,
			pSession->tConfig.iPort ? pSession->tConfig.iPort : (uint16)XSSH_DEFAULT_PORT,
			tHostKeyBlob,
			pSession->tConfig.iHostKeyPolicy,
			&bAcceptedNew,
			sAcceptLine,
			sizeof(sAcceptLine));
		if ( sText != NULL ) {
			xrtFree(sText);
			sText = NULL;
		}
		if ( iRet != XSSH_OK || !bAcceptedNew ) {
			xsshKnownHostsReleaseAppendLock(sLockPath);
			goto done;
		}
		iWritten = snprintf(sAppendLine, sizeof(sAppendLine), "%s\n", sAcceptLine);
		if ( iWritten <= 0 || (size_t)iWritten >= sizeof(sAppendLine) ||
			xrtFileAppend((str)sPath, (str)sAppendLine, (size_t)iWritten, XRT_CP_UTF8) != (int)iWritten ) {
			xsshKnownHostsReleaseAppendLock(sLockPath);
			xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_IO, XSSH_STAGE_HOSTKEY, "known_hosts append failed");
			if ( xrtGetError() ) {
				xrtClearError();
			}
			return XSSH_ERR_IO;
		}
		xsshKnownHostsReleaseAppendLock(sLockPath);
		pSession->tResult.bHostKeyAcceptedNew = TRUE;
	}
done:
	if ( iRet == XSSH_ERR_HOSTKEY_UNKNOWN ) {
		xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_HOSTKEY, "server host key is unknown");
	} else if ( iRet == XSSH_ERR_HOSTKEY_MISMATCH ) {
		xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_HOSTKEY, "server host key mismatch");
	} else if ( iRet != XSSH_OK ) {
		xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_HOSTKEY, "known_hosts check failed");
	}
	return iRet;
}

static int xsshTransportSendPacket(xssh_session_t* pSession, const void* pPayload, size_t iPayloadLen)
{
	uint8 arrWire[XSSH_PACKET_MAX_DEFAULT + 1024u];
	xsshwriter wr;
	int iStage;
	int iRet;

	if ( pSession == NULL || (pPayload == NULL && iPayloadLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	iStage = pSession->iPacketIoStage ? pSession->iPacketIoStage : XSSH_STAGE_KEX;
	xsshWriterInit(&wr, arrWire, sizeof(arrWire));
	iRet = xsshPacketCodecWrite(&pSession->tPacketCodec, &wr, pPayload, iPayloadLen);
	if ( iRet != XSSH_OK ) {
		xsshRuntimeSetResult(pSession, FALSE, iRet, iStage, "xssh packet encode failed");
		return iRet;
	}
	iRet = xsshTransportSendAll(pSession, arrWire, wr.iLen);
	if ( iRet != XSSH_OK ) {
		pSession->tResult.iStage = iStage;
	}
	return iRet;
}

static int xsshTransportReadPacket(xssh_session_t* pSession, xsshpacket* pOut, uint8* pPlain, size_t iPlainCap)
{
	uint64 iDeadlineMs;
	int iStage;

	if ( pSession == NULL || pOut == NULL || pPlain == NULL || iPlainCap == 0u ) {
		return XSSH_ERR_INVALID;
	}
	iStage = pSession->iPacketIoStage ? pSession->iPacketIoStage : XSSH_STAGE_KEX;
	iDeadlineMs = xsshTransportNowMs() + (pSession->iTransportTimeoutMs ? pSession->iTransportTimeoutMs : XSSH_DEFAULT_TIMEOUT_MS);
	for (;;) {
		xsshreader rd;
		int iRet;

		xrtMutexLock(pSession->pLock);
		xsshReaderInit(&rd, pSession->arrWireRecv, pSession->iWireRecvLen);
		iRet = xsshPacketCodecRead(&pSession->tPacketCodec, &rd,
			pSession->tConfig.iMaxPacketSize ? pSession->tConfig.iMaxPacketSize : XSSH_PACKET_MAX_DEFAULT,
			pOut, pPlain, iPlainCap);
		if ( iRet == XSSH_OK ) {
			if ( rd.iPos < pSession->iWireRecvLen ) {
				memmove(pSession->arrWireRecv, pSession->arrWireRecv + rd.iPos, pSession->iWireRecvLen - rd.iPos);
			}
			pSession->iWireRecvLen -= rd.iPos;
			xrtMutexUnlock(pSession->pLock);
			return XSSH_OK;
		}
		if ( iRet != XSSH_WIRE_NEED_MORE ) {
			xrtMutexUnlock(pSession->pLock);
			xsshRuntimeSetResult(pSession, FALSE, iRet, iStage, "xssh packet decode failed");
			return iRet;
		}
		if ( pSession->bRecvOverflow ) {
			xrtMutexUnlock(pSession->pLock);
			xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_NO_SPACE, iStage, "xssh packet recv buffer overflow");
			return XSSH_ERR_NO_SPACE;
		}
		if ( pSession->iLastSysErr != 0 ) {
			int iSysErr = pSession->iLastSysErr;
			xrtMutexUnlock(pSession->pLock);
			snprintf(pSession->tResult.sError, sizeof(pSession->tResult.sError), "xssh packet recv failed (sys=%d)", iSysErr);
			pSession->tResult.bSuccess = FALSE;
			pSession->tResult.iError = XSSH_ERR_IO;
			pSession->tResult.iStage = iStage;
			return XSSH_ERR_IO;
		}
		if ( pSession->bStreamClosed ) {
			int iReason = pSession->iCloseReason;
			xrtMutexUnlock(pSession->pLock);
			snprintf(pSession->tResult.sError, sizeof(pSession->tResult.sError), "xssh stream closed while reading packet (reason=%d)", iReason);
			pSession->tResult.bSuccess = FALSE;
			pSession->tResult.iError = XSSH_ERR_IO;
			pSession->tResult.iStage = iStage;
			return XSSH_ERR_IO;
		}
		if ( !xsshTransportWaitRecvState(pSession, iDeadlineMs) ) {
			xrtMutexUnlock(pSession->pLock);
			xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_TIMEOUT, iStage, "xssh packet recv timeout");
			return XSSH_ERR_TIMEOUT;
		}
		xrtMutexUnlock(pSession->pLock);
	}
}

static int xsshTransportReadExpectedPacket(xssh_session_t* pSession, uint8 iExpectedMsg, xsshpacket* pOut, uint8* pPlain, size_t iPlainCap)
{
	for (;;) {
		int iRet = xsshTransportReadPacket(pSession, pOut, pPlain, iPlainCap);
		if ( iRet != XSSH_OK ) {
			return iRet;
		}
		if ( pOut->tPayload.iLen == 0u ) {
			xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_BAD_PACKET,
				pSession->iPacketIoStage ? pSession->iPacketIoStage : XSSH_STAGE_KEX, "empty SSH packet payload");
			return XSSH_ERR_BAD_PACKET;
		}
		if ( pOut->tPayload.pData[0] == XSSH_MSG_IGNORE || pOut->tPayload.pData[0] == XSSH_MSG_DEBUG ) {
			continue;
		}
		if ( pOut->tPayload.pData[0] != iExpectedMsg ) {
			xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_BAD_PACKET,
				pSession->iPacketIoStage ? pSession->iPacketIoStage : XSSH_STAGE_KEX, "unexpected SSH packet type");
			return XSSH_ERR_BAD_PACKET;
		}
		return XSSH_OK;
	}
}

static int xsshTransportRunKex(xssh_session_t* pSession)
{
	uint8 arrClientCookie[XSSH_KEX_COOKIE_SIZE];
	uint8 arrClientKexInit[2048];
	uint8 arrServerKexInit[4096];
	uint8 arrClientPriv[32];
	uint8 arrClientPub[32];
	uint8 arrSharedSecret[32];
	uint8 arrHash[XSSH_SHA256_SIZE];
	uint8 arrPayload[512];
	uint8 arrPlain[8192];
	uint8 arrC2SIV[XSSH_GCM_NONCE_SIZE];
	uint8 arrS2CIV[XSSH_GCM_NONCE_SIZE];
	uint8 arrC2SKey[XSSH_AES256_KEY_SIZE];
	uint8 arrS2CKey[XSSH_AES256_KEY_SIZE];
	xsshkexinitdesc clientDesc;
	xsshkexinit clientKex;
	xsshkexinit serverKex;
	xsshkexnegotiation neg;
	xsshwriter wr;
	xsshpacket pkt;
	xsshkexecdhreply reply;
	xsshpublickey hostKey;
	xsshsignature hostSig;
	xsshkexhashdesc hashDesc;
	xsshslice sharedSecret;
	size_t iClientKexInitLen;
	size_t iServerKexInitLen;
	size_t iC2SKeyLen;
	size_t iS2CKeyLen;
	bool bSkipServerFirstKexFollower = FALSE;
	int iRet;
	int iStatus = XSSH_OK;

	if ( pSession == NULL ) {
		return XSSH_ERR_INVALID;
	}
	memset(arrClientPriv, 0, sizeof(arrClientPriv));
	memset(arrClientPub, 0, sizeof(arrClientPub));
	memset(arrSharedSecret, 0, sizeof(arrSharedSecret));
	memset(arrHash, 0, sizeof(arrHash));
	memset(arrC2SIV, 0, sizeof(arrC2SIV));
	memset(arrS2CIV, 0, sizeof(arrS2CIV));
	memset(arrC2SKey, 0, sizeof(arrC2SKey));
	memset(arrS2CKey, 0, sizeof(arrS2CKey));
	pSession->iPacketIoStage = XSSH_STAGE_KEX;
	xsshPacketCodecInit(&pSession->tPacketCodec);
	xrtRandomBytes(arrClientCookie, sizeof(arrClientCookie));
	xsshKexInitDescInit(&clientDesc);
	clientDesc.pCookie = arrClientCookie;
	xsshWriterInit(&wr, arrClientKexInit, sizeof(arrClientKexInit));
	iRet = xsshKexInitWrite(&wr, &clientDesc);
	if ( iRet != XSSH_OK ) {
		xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_KEX, "client KEXINIT build failed");
		iStatus = iRet;
		goto cleanup;
	}
	iClientKexInitLen = wr.iLen;
	iRet = xsshKexInitRead(arrClientKexInit, wr.iLen, &clientKex);
	if ( iRet != XSSH_OK ) {
		xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_KEX, "client KEXINIT self-parse failed");
		iStatus = iRet;
		goto cleanup;
	}
	iRet = xsshTransportSendPacket(pSession, arrClientKexInit, wr.iLen);
	if ( iRet != XSSH_OK ) {
		iStatus = iRet;
		goto cleanup;
	}

	iRet = xsshTransportReadExpectedPacket(pSession, XSSH_MSG_KEXINIT, &pkt, arrPlain, sizeof(arrPlain));
	if ( iRet != XSSH_OK ) {
		iStatus = iRet;
		goto cleanup;
	}
	if ( pkt.tPayload.iLen > sizeof(arrServerKexInit) ) {
		xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_NO_SPACE, XSSH_STAGE_KEX, "server KEXINIT too large");
		iStatus = XSSH_ERR_NO_SPACE;
		goto cleanup;
	}
	memcpy(arrServerKexInit, pkt.tPayload.pData, pkt.tPayload.iLen);
	iServerKexInitLen = pkt.tPayload.iLen;
	iRet = xsshKexInitRead(arrServerKexInit, iServerKexInitLen, &serverKex);
	if ( iRet != XSSH_OK ) {
		xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_KEX, "server KEXINIT parse failed");
		iStatus = iRet;
		goto cleanup;
	}
	iRet = xsshKexNegotiate(&clientKex, &serverKex, &neg);
	if ( iRet != XSSH_OK ) {
		xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_KEX, "no mutually supported SSH algorithms");
		iStatus = iRet;
		goto cleanup;
	}
	bSkipServerFirstKexFollower = xsshKexShouldSkipFirstKexFollower(&serverKex, &neg);
	if ( !xsshKexSliceEqText(neg.tKexAlgorithm, "curve25519-sha256") &&
		!xsshKexSliceEqText(neg.tKexAlgorithm, "curve25519-sha256@libssh.org") ) {
		xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_UNSUPPORTED, XSSH_STAGE_KEX, "negotiated KEX algorithm is not implemented");
		iStatus = XSSH_ERR_UNSUPPORTED;
		goto cleanup;
	}
	if ( !xsshKexSliceEqText(neg.tServerHostKeyAlgorithm, "ssh-ed25519") ) {
		xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_UNSUPPORTED, XSSH_STAGE_HOSTKEY, "negotiated host key algorithm is not implemented");
		iStatus = XSSH_ERR_UNSUPPORTED;
		goto cleanup;
	}
	if ( !xsshKexSliceEqText(neg.tCipherClientToServer, "aes128-gcm@openssh.com") &&
		!xsshKexSliceEqText(neg.tCipherClientToServer, "aes256-gcm@openssh.com") ) {
		xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_UNSUPPORTED, XSSH_STAGE_NEWKEYS, "negotiated C2S cipher is not implemented in transport");
		iStatus = XSSH_ERR_UNSUPPORTED;
		goto cleanup;
	}
	if ( !xsshKexSliceEqText(neg.tCipherServerToClient, "aes128-gcm@openssh.com") &&
		!xsshKexSliceEqText(neg.tCipherServerToClient, "aes256-gcm@openssh.com") ) {
		xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_UNSUPPORTED, XSSH_STAGE_NEWKEYS, "negotiated S2C cipher is not implemented in transport");
		iStatus = XSSH_ERR_UNSUPPORTED;
		goto cleanup;
	}

	iRet = xsshKexCurve25519Keypair(arrClientPriv, arrClientPub);
	if ( iRet != XSSH_OK ) {
		xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_KEX, "curve25519 keypair failed");
		iStatus = iRet;
		goto cleanup;
	}
	xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
	iRet = xsshKexECDHInitWrite(&wr, arrClientPub, sizeof(arrClientPub));
	if ( iRet != XSSH_OK ) {
		xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_KEX, "ECDH_INIT build failed");
		iStatus = iRet;
		goto cleanup;
	}
	iRet = xsshTransportSendPacket(pSession, arrPayload, wr.iLen);
	if ( iRet != XSSH_OK ) {
		iStatus = iRet;
		goto cleanup;
	}
	if ( bSkipServerFirstKexFollower ) {
		/*
		 * 服务端声明 first_kex_packet_follows 且首选算法猜错时，下一包必须静默丢弃。
		 * 这里只消费完整 SSH packet，不解析 payload，避免错误猜测包污染真正的 KEX 回复。
		 */
		iRet = xsshTransportReadPacket(pSession, &pkt, arrPlain, sizeof(arrPlain));
		if ( iRet != XSSH_OK ) {
			iStatus = iRet;
			goto cleanup;
		}
		xsshSecureZero(arrPlain, sizeof(arrPlain));
	}
	iRet = xsshTransportReadExpectedPacket(pSession, XSSH_MSG_KEX_ECDH_REPLY, &pkt, arrPlain, sizeof(arrPlain));
	if ( iRet != XSSH_OK ) {
		iStatus = iRet;
		goto cleanup;
	}
	iRet = xsshKexECDHReplyRead(pkt.tPayload.pData, pkt.tPayload.iLen, &reply);
	if ( iRet != XSSH_OK ) {
		xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_KEX, "ECDH_REPLY parse failed");
		iStatus = iRet;
		goto cleanup;
	}
	if ( reply.tServerPublic.iLen != 32u ) {
		xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_BAD_PACKET, XSSH_STAGE_KEX, "curve25519 server public key length mismatch");
		iStatus = XSSH_ERR_BAD_PACKET;
		goto cleanup;
	}
	iRet = xsshKexCurve25519SharedSecret(arrSharedSecret, arrClientPriv, reply.tServerPublic.pData);
	if ( iRet != XSSH_OK ) {
		xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_KEX, "curve25519 shared secret failed");
		iStatus = iRet;
		goto cleanup;
	}
	memset(&hashDesc, 0, sizeof(hashDesc));
	hashDesc.tClientVersion.pData = (const uint8*)XSSH_CLIENT_BANNER;
	hashDesc.tClientVersion.iLen = strlen(XSSH_CLIENT_BANNER);
	hashDesc.tServerVersion.pData = (const uint8*)pSession->tResult.sServerBanner;
	hashDesc.tServerVersion.iLen = strlen(pSession->tResult.sServerBanner);
	hashDesc.tClientKexInit.pData = arrClientKexInit;
	hashDesc.tClientKexInit.iLen = iClientKexInitLen;
	hashDesc.tServerKexInit.pData = arrServerKexInit;
	hashDesc.tServerKexInit.iLen = iServerKexInitLen;
	hashDesc.tServerHostKey = reply.tServerHostKey;
	hashDesc.tClientEphemeral.pData = arrClientPub;
	hashDesc.tClientEphemeral.iLen = sizeof(arrClientPub);
	hashDesc.tServerEphemeral = reply.tServerPublic;
	hashDesc.tSharedSecret.pData = arrSharedSecret;
	hashDesc.tSharedSecret.iLen = sizeof(arrSharedSecret);
	iRet = xsshKexHashSHA256Curve25519(&hashDesc, arrHash);
	if ( iRet != XSSH_OK ) {
		xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_KEX, "exchange hash failed");
		iStatus = iRet;
		goto cleanup;
	}
	iRet = xsshPublicKeyReadEd25519(reply.tServerHostKey.pData, reply.tServerHostKey.iLen, &hostKey);
	if ( iRet != XSSH_OK ) {
		xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_HOSTKEY, "server Ed25519 host key parse failed");
		iStatus = iRet;
		goto cleanup;
	}
	iRet = xsshSignatureRead(reply.tSignature.pData, reply.tSignature.iLen, &hostSig);
	if ( iRet != XSSH_OK ) {
		xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_HOSTKEY, "server host key signature parse failed");
		iStatus = iRet;
		goto cleanup;
	}
	iRet = xsshHostKeyVerifyEd25519(&hostKey, &hostSig, arrHash, sizeof(arrHash));
	if ( iRet != XSSH_OK ) {
		xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_HOSTKEY, "server host key signature verify failed");
		iStatus = iRet;
		goto cleanup;
	}
	iRet = xsshTransportCheckHostKey(pSession, reply.tServerHostKey);
	if ( iRet != XSSH_OK ) {
		iStatus = iRet;
		goto cleanup;
	}

	xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
	iRet = xsshMsgWriteNewKeys(&wr);
	if ( iRet != XSSH_OK ) {
		xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_NEWKEYS, "NEWKEYS build failed");
		iStatus = iRet;
		goto cleanup;
	}
	iRet = xsshTransportSendPacket(pSession, arrPayload, wr.iLen);
	if ( iRet != XSSH_OK ) {
		iStatus = iRet;
		goto cleanup;
	}
	iRet = xsshTransportReadExpectedPacket(pSession, XSSH_MSG_NEWKEYS, &pkt, arrPlain, sizeof(arrPlain));
	if ( iRet != XSSH_OK ) {
		iStatus = iRet;
		goto cleanup;
	}
	iRet = xsshMsgReadNewKeys(pkt.tPayload.pData, pkt.tPayload.iLen);
	if ( iRet != XSSH_OK ) {
		xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_NEWKEYS, "server NEWKEYS parse failed");
		iStatus = iRet;
		goto cleanup;
	}

	sharedSecret.pData = arrSharedSecret;
	sharedSecret.iLen = sizeof(arrSharedSecret);
	if ( !pSession->bHasSessionId ) {
		memcpy(pSession->arrSessionId, arrHash, sizeof(arrHash));
		pSession->bHasSessionId = TRUE;
	}
	iC2SKeyLen = xsshKexSliceEqText(neg.tCipherClientToServer, "aes256-gcm@openssh.com") ? XSSH_AES256_KEY_SIZE : XSSH_AES128_KEY_SIZE;
	iS2CKeyLen = xsshKexSliceEqText(neg.tCipherServerToClient, "aes256-gcm@openssh.com") ? XSSH_AES256_KEY_SIZE : XSSH_AES128_KEY_SIZE;
	if ( xsshKexDeriveKeySHA256(arrC2SIV, sizeof(arrC2SIV), sharedSecret, arrHash, pSession->arrSessionId, 'A') != XSSH_OK ||
		xsshKexDeriveKeySHA256(arrS2CIV, sizeof(arrS2CIV), sharedSecret, arrHash, pSession->arrSessionId, 'B') != XSSH_OK ||
		xsshKexDeriveKeySHA256(arrC2SKey, iC2SKeyLen, sharedSecret, arrHash, pSession->arrSessionId, 'C') != XSSH_OK ||
		xsshKexDeriveKeySHA256(arrS2CKey, iS2CKeyLen, sharedSecret, arrHash, pSession->arrSessionId, 'D') != XSSH_OK ) {
		xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_BAD_PACKET, XSSH_STAGE_NEWKEYS, "key derivation failed");
		iStatus = XSSH_ERR_BAD_PACKET;
		goto cleanup;
	}
	if ( xsshPacketCodecSetWriteAESGCM(&pSession->tPacketCodec, arrC2SKey, iC2SKeyLen, arrC2SIV) != XSSH_OK ||
		xsshPacketCodecSetReadAESGCM(&pSession->tPacketCodec, arrS2CKey, iS2CKeyLen, arrS2CIV) != XSSH_OK ) {
		xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_UNSUPPORTED, XSSH_STAGE_NEWKEYS, "packet codec key switch failed");
		iStatus = XSSH_ERR_UNSUPPORTED;
		goto cleanup;
	}
	xsshTransportCopySliceText(pSession->tResult.sKex, sizeof(pSession->tResult.sKex), neg.tKexAlgorithm);
	xsshTransportCopySliceText(pSession->tResult.sHostKey, sizeof(pSession->tResult.sHostKey), neg.tServerHostKeyAlgorithm);
	xsshTransportCopySliceText(pSession->tResult.sCipherClientToServer, sizeof(pSession->tResult.sCipherClientToServer), neg.tCipherClientToServer);
	xsshTransportCopySliceText(pSession->tResult.sCipherServerToClient, sizeof(pSession->tResult.sCipherServerToClient), neg.tCipherServerToClient);

cleanup:
	xsshSecureZero(arrClientPriv, sizeof(arrClientPriv));
	xsshSecureZero(arrSharedSecret, sizeof(arrSharedSecret));
	xsshSecureZero(arrC2SIV, sizeof(arrC2SIV));
	xsshSecureZero(arrS2CIV, sizeof(arrS2CIV));
	xsshSecureZero(arrC2SKey, sizeof(arrC2SKey));
	xsshSecureZero(arrS2CKey, sizeof(arrS2CKey));
	return iStatus;
}

static int xsshTransportRunAuth(xssh_session_t* pSession)
{
	uint8 arrPayload[8192];
	uint8 arrPlain[8192];
	uint8 arrSignData[4096];
	uint8 arrSig[512];
	uint8 arrSigBlob[768];
	char arrKbdResponses[XSSH_AUTH_KBDINT_PROMPT_MAX][XSSH_AUTH_KBDINT_RESPONSE_MAX];
	const char* arrKbdResponsePtrs[XSSH_AUTH_KBDINT_PROMPT_MAX];
	xsshwriter wr;
	xsshpacket pkt;
	xsshservicemsg svc;
	xsshprivateidentity identity;
	const char* sUser;
	const char* sPublicKeyAlgorithm = NULL;
	const uint8* pPublicKeyBlob = NULL;
	size_t iPublicKeyBlobLen = 0u;
	bool bPasswordAttempt;
	bool bPublicKeyAttempt;
	bool bPublicKeySigned = FALSE;
	bool bPrivateKeyIdentity = FALSE;
	bool bKeyboardAttempt = FALSE;
	uint32 iAuthPacketCount = 0u;
	uint32 iKbdIntRoundCount = 0u;
	int iRet;
	int iStatus = XSSH_OK;

	if ( pSession == NULL ) {
		return XSSH_ERR_INVALID;
	}
	sUser = pSession->tConfig.sUser;
	if ( sUser == NULL || sUser[0] == '\0' ) {
		xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_INVALID, XSSH_STAGE_AUTH, "ssh user name is required for USERAUTH");
		return XSSH_ERR_INVALID;
	}

	pSession->iPacketIoStage = XSSH_STAGE_AUTH;
	memset(&identity, 0, sizeof(identity));
	memset(arrPayload, 0, sizeof(arrPayload));
	memset(arrPlain, 0, sizeof(arrPlain));

	xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
	iRet = xsshMsgWriteServiceRequest(&wr, "ssh-userauth");
	if ( iRet != XSSH_OK ) {
		xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_AUTH, "USERAUTH service request build failed");
		iStatus = iRet;
		goto cleanup;
	}
	iRet = xsshTransportSendPacket(pSession, arrPayload, wr.iLen);
	if ( iRet != XSSH_OK ) {
		iStatus = iRet;
		goto cleanup;
	}
	iRet = xsshTransportReadExpectedPacket(pSession, XSSH_MSG_SERVICE_ACCEPT, &pkt, arrPlain, sizeof(arrPlain));
	if ( iRet != XSSH_OK ) {
		iStatus = iRet;
		goto cleanup;
	}
	iRet = xsshMsgReadServiceAccept(pkt.tPayload.pData, pkt.tPayload.iLen, &svc);
	if ( iRet != XSSH_OK || !xsshSliceEqualText(svc.tServiceName, "ssh-userauth") ) {
		xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_BAD_PACKET, XSSH_STAGE_AUTH, "USERAUTH service accept mismatch");
		iStatus = XSSH_ERR_BAD_PACKET;
		goto cleanup;
	}

	xsshSecureZero(arrPayload, sizeof(arrPayload));
	xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
	bPasswordAttempt = (pSession->tAuth.sPassword != NULL) ? TRUE : FALSE;
	if ( !bPasswordAttempt &&
		pSession->tAuth.pSign != NULL &&
		pSession->tAuth.sPublicKeyAlgorithm != NULL &&
		pSession->tAuth.pPublicKeyBlob != NULL &&
		pSession->tAuth.iPublicKeyBlobLen != 0u ) {
		sPublicKeyAlgorithm = pSession->tAuth.sPublicKeyAlgorithm;
		pPublicKeyBlob = (const uint8*)pSession->tAuth.pPublicKeyBlob;
		iPublicKeyBlobLen = pSession->tAuth.iPublicKeyBlobLen;
	} else if ( !bPasswordAttempt &&
		((pSession->tAuth.pPrivateKeyData != NULL && pSession->tAuth.iPrivateKeySize != 0u) ||
		(pSession->tAuth.sPrivateKeyPath != NULL && pSession->tAuth.sPrivateKeyPath[0] != '\0')) ) {
		iRet = xsshAuthLoadPrivateIdentity(&pSession->tAuth, &identity);
		if ( iRet != XSSH_OK ) {
			xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_AUTH, "OpenSSH private key parse failed");
			iStatus = iRet;
			goto cleanup;
		}
		sPublicKeyAlgorithm = identity.sAlgorithm;
		pPublicKeyBlob = identity.arrPublicKeyBlob;
		iPublicKeyBlobLen = identity.iPublicKeyBlobLen;
		bPrivateKeyIdentity = TRUE;
	}
	bPublicKeyAttempt = (!bPasswordAttempt && sPublicKeyAlgorithm != NULL && pPublicKeyBlob != NULL && iPublicKeyBlobLen != 0u) ? TRUE : FALSE;
	bKeyboardAttempt = (!bPasswordAttempt && !bPublicKeyAttempt && pSession->tAuth.pKeyboardInteractive != NULL) ? TRUE : FALSE;
	if ( bPasswordAttempt ) {
		/* password 只存在于调用方和短生命周期 payload 缓冲，发送后立即清零。 */
		iRet = xsshAuthWritePasswordRequest(&wr, sUser, pSession->tAuth.sPassword);
	} else if ( bPublicKeyAttempt ) {
		xsshslice keyBlob;
		xsshslice emptySig;

		keyBlob.pData = pPublicKeyBlob;
		keyBlob.iLen = iPublicKeyBlobLen;
		emptySig.pData = NULL;
		emptySig.iLen = 0u;
		iRet = xsshAuthWritePublicKeyRequest(&wr, sUser, FALSE, sPublicKeyAlgorithm, keyBlob, emptySig);
	} else if ( bKeyboardAttempt ) {
		/* keyboard-interactive 的 challenge 由服务端随后下发；初始请求只声明语言和子方法。 */
		iRet = xsshAuthWriteKeyboardInteractiveRequest(&wr, sUser, "");
	} else {
		/* 没有可用凭据时先发 none，用服务端返回的 method-list 给调用方可诊断信息。 */
		iRet = xsshAuthWriteNoneRequest(&wr, sUser);
	}
	if ( iRet != XSSH_OK ) {
		xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_AUTH, "USERAUTH request build failed");
		iStatus = iRet;
		goto cleanup;
	}
	iRet = xsshTransportSendPacket(pSession, arrPayload, wr.iLen);
	xsshSecureZero(arrPayload, sizeof(arrPayload));
	if ( iRet != XSSH_OK ) {
		iStatus = iRet;
		goto cleanup;
	}

	for (;;) {
		uint8 iMsg;

		iRet = xsshTransportReadPacket(pSession, &pkt, arrPlain, sizeof(arrPlain));
		if ( iRet != XSSH_OK ) {
			iStatus = iRet;
			goto cleanup;
		}
		if ( pkt.tPayload.iLen == 0u ) {
			xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_BAD_PACKET, XSSH_STAGE_AUTH, "empty USERAUTH packet payload");
			iStatus = XSSH_ERR_BAD_PACKET;
			goto cleanup;
		}
		if ( ++iAuthPacketCount > XSSH_AUTH_PACKET_MAX ) {
			xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_AUTH_FAILED, XSSH_STAGE_AUTH, "too many packets during USERAUTH");
			(void)xsshRuntimePushEvent(pSession, XSSH_EVENT_ERROR, NULL, NULL, 0u, XSSH_ERR_AUTH_FAILED, "ssh userauth packet limit exceeded");
			iStatus = XSSH_ERR_AUTH_FAILED;
			goto cleanup;
		}
		iMsg = pkt.tPayload.pData[0];
		if ( iMsg == XSSH_MSG_IGNORE || iMsg == XSSH_MSG_DEBUG ) {
			continue;
		}
		if ( iMsg == XSSH_MSG_USERAUTH_SUCCESS ) {
			iRet = xsshAuthReadSuccess(pkt.tPayload.pData, pkt.tPayload.iLen);
			if ( iRet != XSSH_OK ) {
				xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_AUTH, "USERAUTH_SUCCESS parse failed");
				iStatus = iRet;
				goto cleanup;
			}
			pSession->bAuthenticated = TRUE;
			xsshRuntimeSetResult(pSession, TRUE, XSSH_OK, XSSH_STAGE_AUTH, NULL);
			iRet = xsshRuntimePushEvent(pSession, XSSH_EVENT_AUTHENTICATED, NULL, NULL, 0u, XSSH_OK, "ssh userauth success");
			if ( iRet != XSSH_OK ) {
				xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_AUTH, "USERAUTH event queue push failed");
				iStatus = iRet;
				goto cleanup;
			}
			iStatus = XSSH_OK;
			goto cleanup;
		}
		if ( iMsg == XSSH_MSG_USERAUTH_PK_OK && bPublicKeyAttempt && !bPublicKeySigned ) {
			xsshauthpkokmsg pkok;
			xsshslice sessionId;
			xsshslice keyBlob;
			xsshslice sigBlob;
			xsshwriter signWr;
			xsshwriter sigWr;
			size_t iSigLen = sizeof(arrSig);

			iRet = xsshAuthReadPublicKeyOk(pkt.tPayload.pData, pkt.tPayload.iLen, &pkok);
			if ( iRet != XSSH_OK ) {
				xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_AUTH, "USERAUTH_PK_OK parse failed");
				iStatus = iRet;
				goto cleanup;
			}
			keyBlob.pData = pPublicKeyBlob;
			keyBlob.iLen = iPublicKeyBlobLen;
			if ( !xsshSliceEqualText(pkok.tPublicKeyAlgorithm, sPublicKeyAlgorithm) ||
				pkok.tPublicKeyBlob.iLen != keyBlob.iLen ||
				memcmp(pkok.tPublicKeyBlob.pData, keyBlob.pData, keyBlob.iLen) != 0u ||
				!pSession->bHasSessionId ) {
				xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_BAD_PACKET, XSSH_STAGE_AUTH, "USERAUTH_PK_OK mismatch");
				iStatus = XSSH_ERR_BAD_PACKET;
				goto cleanup;
			}
			sessionId.pData = pSession->arrSessionId;
			sessionId.iLen = XSSH_SHA256_SIZE;
			xsshWriterInit(&signWr, arrSignData, sizeof(arrSignData));
			iRet = xsshAuthWritePublicKeySignData(&signWr, sessionId, sUser, sPublicKeyAlgorithm, keyBlob);
			if ( iRet != XSSH_OK ) {
				xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_AUTH, "publickey sign data build failed");
				iStatus = iRet;
				goto cleanup;
			}
			if ( bPrivateKeyIdentity ) {
				if ( !identity.bHasEd25519 || !xsshSliceEqualText(pkok.tPublicKeyAlgorithm, "ssh-ed25519") || iSigLen < 64u ||
					!xrtEd25519Sign(arrSig, arrSignData, signWr.iLen, identity.arrEd25519Seed) ) {
					xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_UNSUPPORTED, XSSH_STAGE_AUTH, "private key signing failed");
					iStatus = XSSH_ERR_UNSUPPORTED;
					goto cleanup;
				}
				iSigLen = 64u;
			} else {
				iRet = pSession->tAuth.pSign(pSession->tAuth.pSignerUser, arrSignData, signWr.iLen, arrSig, &iSigLen);
				if ( iRet != XSSH_OK ) {
					xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_AUTH, "publickey signer callback failed");
					iStatus = iRet;
					goto cleanup;
				}
			}
			xsshWriterInit(&sigWr, arrSigBlob, sizeof(arrSigBlob));
			iRet = xsshSignatureWrite(&sigWr, sPublicKeyAlgorithm, arrSig, iSigLen);
			if ( iRet != XSSH_OK ) {
				xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_AUTH, "publickey signature blob build failed");
				iStatus = iRet;
				goto cleanup;
			}
			sigBlob.pData = arrSigBlob;
			sigBlob.iLen = sigWr.iLen;
			xsshSecureZero(arrPayload, sizeof(arrPayload));
			xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
			iRet = xsshAuthWritePublicKeyRequest(&wr, sUser, TRUE, sPublicKeyAlgorithm, keyBlob, sigBlob);
			if ( iRet != XSSH_OK ) {
				xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_AUTH, "signed publickey request build failed");
				iStatus = iRet;
				goto cleanup;
			}
			iRet = xsshTransportSendPacket(pSession, arrPayload, wr.iLen);
			xsshSecureZero(arrPayload, sizeof(arrPayload));
			xsshSecureZero(arrSignData, sizeof(arrSignData));
			xsshSecureZero(arrSig, sizeof(arrSig));
			xsshSecureZero(arrSigBlob, sizeof(arrSigBlob));
			if ( iRet != XSSH_OK ) {
				iStatus = iRet;
				goto cleanup;
			}
			bPublicKeySigned = TRUE;
			continue;
		}
		if ( iMsg == XSSH_MSG_USERAUTH_INFO_REQUEST && bKeyboardAttempt ) {
			xsshauthkbdintrequestmsg infoReq;
			char sName[128];
			char sInstruction[256];
			char sPrompt[256];
			uint32 i;

			if ( ++iKbdIntRoundCount > XSSH_AUTH_KBDINT_ROUND_MAX ) {
				xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_AUTH_FAILED, XSSH_STAGE_AUTH, "too many keyboard-interactive rounds");
				(void)xsshRuntimePushEvent(pSession, XSSH_EVENT_ERROR, NULL, NULL, 0u, XSSH_ERR_AUTH_FAILED, "keyboard-interactive round limit exceeded");
				iStatus = XSSH_ERR_AUTH_FAILED;
				goto cleanup;
			}
			iRet = xsshAuthReadKeyboardInteractiveInfoRequest(pkt.tPayload.pData, pkt.tPayload.iLen, &infoReq);
			if ( iRet != XSSH_OK ) {
				xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_AUTH, "USERAUTH_INFO_REQUEST parse failed");
				iStatus = iRet;
				goto cleanup;
			}
			xsshTransportCopySliceText(sName, sizeof(sName), infoReq.tName);
			xsshTransportCopySliceText(sInstruction, sizeof(sInstruction), infoReq.tInstruction);
			memset(arrKbdResponses, 0, sizeof(arrKbdResponses));
			memset(arrKbdResponsePtrs, 0, sizeof(arrKbdResponsePtrs));
			for ( i = 0u; i < infoReq.iPromptCount; ++i ) {
				xsshTransportCopySliceText(sPrompt, sizeof(sPrompt), infoReq.arrPrompts[i]);
				iRet = pSession->tAuth.pKeyboardInteractive(pSession->tAuth.pKeyboardUser,
					sName, sInstruction, sPrompt, infoReq.arrEcho[i],
					arrKbdResponses[i], sizeof(arrKbdResponses[i]));
				if ( iRet != XSSH_OK ) {
					xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_AUTH, "keyboard-interactive callback failed");
					iStatus = iRet;
					goto cleanup;
				}
				arrKbdResponses[i][sizeof(arrKbdResponses[i]) - 1u] = '\0';
				arrKbdResponsePtrs[i] = arrKbdResponses[i];
			}
			xsshSecureZero(arrPayload, sizeof(arrPayload));
			xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
			iRet = xsshAuthWriteKeyboardInteractiveInfoResponse(&wr, arrKbdResponsePtrs, infoReq.iPromptCount);
			if ( iRet != XSSH_OK ) {
				xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_AUTH, "USERAUTH_INFO_RESPONSE build failed");
				iStatus = iRet;
				goto cleanup;
			}
			iRet = xsshTransportSendPacket(pSession, arrPayload, wr.iLen);
			xsshSecureZero(arrPayload, sizeof(arrPayload));
			xsshSecureZero(arrKbdResponses, sizeof(arrKbdResponses));
			if ( iRet != XSSH_OK ) {
				iStatus = iRet;
				goto cleanup;
			}
			continue;
		}
		if ( iMsg == XSSH_MSG_USERAUTH_FAILURE ) {
			xsshauthfailuremsg fail;
			char sMethods[128];

			iRet = xsshAuthReadFailure(pkt.tPayload.pData, pkt.tPayload.iLen, &fail);
			if ( iRet != XSSH_OK ) {
				xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_AUTH, "USERAUTH_FAILURE parse failed");
				iStatus = iRet;
				goto cleanup;
			}
			xsshTransportCopySliceText(sMethods, sizeof(sMethods), fail.tMethods);
			xsshTransportCopySliceText(pSession->tResult.sAuthMethods, sizeof(pSession->tResult.sAuthMethods), fail.tMethods);
			snprintf(pSession->tResult.sError, sizeof(pSession->tResult.sError),
				bPasswordAttempt ? "ssh password authentication failed; server allows: %s" :
					(bPublicKeyAttempt ? "ssh publickey authentication failed; server allows: %s" :
						(bKeyboardAttempt ? "ssh keyboard-interactive authentication failed; server allows: %s" :
							"ssh authentication required; server allows: %s")),
				sMethods);
			pSession->tResult.bSuccess = FALSE;
			pSession->tResult.iError = XSSH_ERR_AUTH_FAILED;
			pSession->tResult.iStage = XSSH_STAGE_AUTH;
			(void)xsshRuntimePushEvent(pSession, XSSH_EVENT_ERROR, NULL, NULL, 0u, XSSH_ERR_AUTH_FAILED, "ssh userauth failed");
			iStatus = XSSH_ERR_AUTH_FAILED;
			goto cleanup;
		}
		if ( iMsg == XSSH_MSG_USERAUTH_BANNER ) {
			xsshauthbannermsg banner;
			iRet = xsshAuthReadBanner(pkt.tPayload.pData, pkt.tPayload.iLen, &banner);
			if ( iRet != XSSH_OK ) {
				xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_AUTH, "USERAUTH_BANNER parse failed");
				iStatus = iRet;
				goto cleanup;
			}
			continue;
		}
		if ( iMsg == XSSH_MSG_DISCONNECT ) {
			xsshdisconnectmsg disc;
			iRet = xsshMsgReadDisconnect(pkt.tPayload.pData, pkt.tPayload.iLen, &disc);
			if ( iRet != XSSH_OK ) {
				xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_DISCONNECT, "DISCONNECT parse failed during USERAUTH");
				iStatus = iRet;
				goto cleanup;
			}
			pSession->tResult.bSuccess = FALSE;
			pSession->tResult.iError = XSSH_ERR_CLOSED;
			pSession->tResult.iStage = XSSH_STAGE_DISCONNECT;
			pSession->tResult.iDisconnectReason = (int)disc.iReason;
			snprintf(pSession->tResult.sError, sizeof(pSession->tResult.sError),
				"server disconnected during USERAUTH: %.*s",
				(int)((disc.tDescription.iLen < 160u) ? disc.tDescription.iLen : 160u),
				(const char*)disc.tDescription.pData);
			(void)xsshRuntimePushEvent(pSession, XSSH_EVENT_DISCONNECT, NULL, NULL, 0u, (int)disc.iReason, "server disconnect");
			iStatus = XSSH_ERR_CLOSED;
			goto cleanup;
		}
		xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_BAD_PACKET, XSSH_STAGE_AUTH, "unexpected packet during USERAUTH");
		iStatus = XSSH_ERR_BAD_PACKET;
		goto cleanup;
	}

cleanup:
	xsshSecureZero(arrPayload, sizeof(arrPayload));
	xsshSecureZero(arrPlain, sizeof(arrPlain));
	xsshSecureZero(arrSignData, sizeof(arrSignData));
	xsshSecureZero(arrSig, sizeof(arrSig));
	xsshSecureZero(arrSigBlob, sizeof(arrSigBlob));
	xsshSecureZero(arrKbdResponses, sizeof(arrKbdResponses));
	xsshSecureZero(&identity, sizeof(identity));
	return iStatus;
}

static int xsshConnect(const xsshconfig* pCfg, const xsshauth* pAuth, xssh_session_t** ppSession)
{
	xsshconfig tDefaultConfig;
	xssh_session_t* pSession;
	int iRet;

	if ( ppSession == NULL ) {
		return XSSH_ERR_INVALID;
	}
	*ppSession = NULL;
	xsshConfigInit(&tDefaultConfig);
	if ( pCfg == NULL ) {
		pCfg = &tDefaultConfig;
	}
	pSession = (xssh_session_t*)xrtMalloc(sizeof(*pSession));
	if ( pSession == NULL ) {
		return XSSH_ERR_NO_SPACE;
	}
	memset(pSession, 0, sizeof(*pSession));
	pSession->tConfig = *pCfg;
	xsshConfigNormalize(&pSession->tConfig);
	if ( pAuth != NULL ) {
		pSession->tAuth = *pAuth;
	}
	xsshResultInit(&pSession->tResult);

	/* mock host 走确定性内存 runtime；真实 host 先完成 xnetstream TCP 与 SSH identification exchange。 */
	if ( !xsshRuntimeIsMockHost(pCfg) ) {
		iRet = xsshTransportConnectTcp(pSession, pCfg);
		if ( iRet != XSSH_OK ) {
			*ppSession = pSession;
			return iRet;
		}
		iRet = xsshTransportIdentificationExchange(pSession);
		if ( iRet != XSSH_OK ) {
			*ppSession = pSession;
			return iRet;
		}
		iRet = xsshTransportRunKex(pSession);
		if ( iRet != XSSH_OK ) {
			*ppSession = pSession;
			return iRet;
		}
		iRet = xsshRuntimePushEvent(pSession, XSSH_EVENT_CONNECTED, NULL, NULL, 0u, XSSH_OK, "ssh transport connected");
		if ( iRet != XSSH_OK ) {
			xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_CONNECT, "connect event queue push failed");
			*ppSession = pSession;
			return iRet;
		}
		iRet = xsshTransportRunAuth(pSession);
		if ( iRet != XSSH_OK ) {
			*ppSession = pSession;
			return iRet;
		}
		*ppSession = pSession;
		return XSSH_OK;
	}
	pSession->bMock = TRUE;
	pSession->bConnected = TRUE;
	pSession->iNextLocalChannel = 0u;
	snprintf(pSession->tResult.sServerBanner, sizeof(pSession->tResult.sServerBanner), "%s", "SSH-2.0-xssh-mock-server");
	snprintf(pSession->tResult.sKex, sizeof(pSession->tResult.sKex), "%s", "curve25519-sha256");
	snprintf(pSession->tResult.sHostKey, sizeof(pSession->tResult.sHostKey), "%s", "ssh-ed25519");
	snprintf(pSession->tResult.sCipherClientToServer, sizeof(pSession->tResult.sCipherClientToServer), "%s", "aes128-gcm@openssh.com");
	snprintf(pSession->tResult.sCipherServerToClient, sizeof(pSession->tResult.sCipherServerToClient), "%s", "aes128-gcm@openssh.com");
	xsshRuntimeSetResult(pSession, TRUE, XSSH_OK, XSSH_STAGE_AUTH, NULL);
	iRet = xsshRuntimePushEvent(pSession, XSSH_EVENT_CONNECTED, NULL, NULL, 0u, XSSH_OK, "mock connected");
	if ( iRet != XSSH_OK ) {
		xrtFree(pSession);
		return iRet;
	}
	if ( pAuth != NULL && pAuth->sPassword != NULL ) {
		pSession->bAuthenticated = TRUE;
		iRet = xsshRuntimePushEvent(pSession, XSSH_EVENT_AUTHENTICATED, NULL, NULL, 0u, XSSH_OK, "mock password accepted");
		if ( iRet != XSSH_OK ) {
			xrtFree(pSession);
			return iRet;
		}
	}
	*ppSession = pSession;
	return XSSH_OK;
}

static int xsshDisconnect(xssh_session_t* pSession)
{
	if ( pSession == NULL ) {
		return XSSH_ERR_INVALID;
	}
	if ( !pSession->bConnected ) {
		if ( !pSession->bMock ) {
			xsshTransportCloseDestroy(pSession);
		}
		return XSSH_OK;
	}
	if ( !pSession->bMock && pSession->pStream != NULL && !pSession->bStreamClosed ) {
		uint8 arrPayload[512];
		xsshwriter wr;

		/*
		 * 主动关闭时尽量发送 SSH_MSG_DISCONNECT，让服务端能区分正常应用关闭与 TCP abort。
		 * 关闭路径不能因为发送失败而阻塞释放，所以这里只做 best-effort。
		 */
		pSession->iPacketIoStage = XSSH_STAGE_DISCONNECT;
		xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
		if ( xsshMsgWriteDisconnect(&wr, 11u, "xssh disconnect", "") == XSSH_OK ) {
			(void)xsshTransportSendPacket(pSession, arrPayload, wr.iLen);
		}
		xsshSecureZero(arrPayload, sizeof(arrPayload));
	}
	pSession->bConnected = FALSE;
	if ( !pSession->bMock ) {
		xsshTransportCloseDestroy(pSession);
	}
	xsshRuntimeSetResult(pSession, TRUE, XSSH_OK, XSSH_STAGE_DISCONNECT, NULL);
	return xsshRuntimePushEvent(pSession, XSSH_EVENT_DISCONNECT, NULL, NULL, 0u, XSSH_OK, "disconnect");
}

static void xsshFree(xssh_session_t* pSession)
{
	if ( pSession == NULL ) {
		return;
	}
	if ( !pSession->bMock ) {
		xsshTransportCloseDestroy(pSession);
	}
	xsshSecureZero(&pSession->tAuth, sizeof(pSession->tAuth));
	xsshSecureZero(pSession, sizeof(*pSession));
	xrtFree(pSession);
}

static int xsshGetLastResult(xssh_session_t* pSession, xsshresult* pOut)
{
	if ( pSession == NULL || pOut == NULL ) {
		return XSSH_ERR_INVALID;
	}
	*pOut = pSession->tResult;
	return XSSH_OK;
}

static xssh_channel_t* xsshRuntimeAllocChannel(xssh_session_t* pSession)
{
	size_t i;
	xssh_channel_t* pChannel = NULL;

	if ( pSession == NULL ) {
		return NULL;
	}
	for ( i = 0u; i < XSSH_RUNTIME_MAX_CHANNELS; ++i ) {
		if ( pSession->arrChannels[i].pSession == NULL || pSession->arrChannels[i].bClosed ) {
			pChannel = &pSession->arrChannels[i];
			break;
		}
	}
	if ( pChannel == NULL ) {
		return NULL;
	}
	memset(pChannel, 0, sizeof(*pChannel));
	pChannel->pSession = pSession;
	pChannel->iLocalChannel = pSession->iNextLocalChannel++;
	pChannel->iLocalWindow = pSession->tConfig.iChannelWindow ? pSession->tConfig.iChannelWindow : XSSH_CHANNEL_WINDOW_DEFAULT;
	pChannel->iMaxPacket = 32768u;
	return pChannel;
}

static xssh_channel_t* xsshRuntimeFindChannelByLocal(xssh_session_t* pSession, uint32 iLocalChannel)
{
	size_t i;

	if ( pSession == NULL ) {
		return NULL;
	}
	for ( i = 0u; i < XSSH_RUNTIME_MAX_CHANNELS; ++i ) {
		xssh_channel_t* pChannel = &pSession->arrChannels[i];
		if ( pChannel->pSession == pSession && pChannel->iLocalChannel == iLocalChannel && !pChannel->bClosed ) {
			return pChannel;
		}
	}
	return NULL;
}

static int xsshRuntimeFinalizeChannelClose(xssh_channel_t* pChannel)
{
	if ( pChannel == NULL ) {
		return XSSH_ERR_INVALID;
	}
	/* SSH close 是双向确认：双方都发送 CHANNEL_CLOSE 后，channel slot 才能复用。 */
	if ( pChannel->bCloseSent && pChannel->bCloseRecv && !pChannel->bClosed ) {
		pChannel->bClosed = TRUE;
		pChannel->bOpen = FALSE;
		return xsshRuntimePushEvent(pChannel->pSession, XSSH_EVENT_CHANNEL_CLOSE, pChannel, NULL, 0u, XSSH_OK, NULL);
	}
	return XSSH_OK;
}

static int xsshTransportHandleDisconnectPacket(xssh_session_t* pSession, const xsshpacket* pPkt)
{
	xsshdisconnectmsg disc;
	int iRet;

	if ( pSession == NULL || pPkt == NULL ) {
		return XSSH_ERR_INVALID;
	}
	iRet = xsshMsgReadDisconnect(pPkt->tPayload.pData, pPkt->tPayload.iLen, &disc);
	if ( iRet != XSSH_OK ) {
		xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_DISCONNECT, "DISCONNECT parse failed");
		return iRet;
	}
	pSession->bConnected = FALSE;
	pSession->tResult.bSuccess = FALSE;
	pSession->tResult.iError = XSSH_ERR_CLOSED;
	pSession->tResult.iStage = XSSH_STAGE_DISCONNECT;
	pSession->tResult.iDisconnectReason = (int)disc.iReason;
	snprintf(pSession->tResult.sError, sizeof(pSession->tResult.sError),
		"server disconnected: %.*s",
		(int)((disc.tDescription.iLen < 180u) ? disc.tDescription.iLen : 180u),
		(const char*)disc.tDescription.pData);
	(void)xsshRuntimePushEvent(pSession, XSSH_EVENT_DISCONNECT, NULL, NULL, 0u, (int)disc.iReason, "server disconnect");
	return XSSH_ERR_CLOSED;
}

static int xsshTransportMaybeAdjustLocalWindow(xssh_channel_t* pChannel)
{
	xssh_session_t* pSession;
	uint32 iTargetWindow;
	uint32 iAdjust;
	uint8 arrPayload[64];
	xsshwriter wr;
	int iRet;

	if ( pChannel == NULL || pChannel->pSession == NULL || pChannel->pSession->bMock ) {
		return XSSH_OK;
	}
	pSession = pChannel->pSession;
	iTargetWindow = pSession->tConfig.iChannelWindow ? pSession->tConfig.iChannelWindow : XSSH_CHANNEL_WINDOW_DEFAULT;
	if ( pChannel->iLocalWindow >= iTargetWindow / 2u ) {
		return XSSH_OK;
	}
	iAdjust = iTargetWindow - pChannel->iLocalWindow;
	xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
	iRet = xsshChannelWindowAdjustWrite(&wr, pChannel->iRemoteChannel, iAdjust);
	if ( iRet != XSSH_OK ) {
		xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_CHANNEL, "CHANNEL_WINDOW_ADJUST build failed");
		return iRet;
	}
	pSession->iPacketIoStage = XSSH_STAGE_CHANNEL;
	iRet = xsshTransportSendPacket(pSession, arrPayload, wr.iLen);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	pChannel->iLocalWindow += iAdjust;
	return XSSH_OK;
}

static size_t xsshChannelPendingLen(const xssh_channel_t* pChannel)
{
	if ( pChannel == NULL || pChannel->iPendingWritePos >= pChannel->iPendingWriteLen ) {
		return 0u;
	}
	return pChannel->iPendingWriteLen - pChannel->iPendingWritePos;
}

static size_t xsshChannelAppendPending(xssh_channel_t* pChannel, const uint8* pData, size_t iLen)
{
	size_t iRemain;
	size_t iAvail;
	size_t iCopy;

	if ( pChannel == NULL || (pData == NULL && iLen != 0u) ) {
		return 0u;
	}
	if ( pChannel->iPendingWritePos != 0u ) {
		iRemain = xsshChannelPendingLen(pChannel);
		if ( iRemain != 0u ) {
			memmove(pChannel->arrPendingWrite,
				pChannel->arrPendingWrite + pChannel->iPendingWritePos, iRemain);
		}
		xsshSecureZero(pChannel->arrPendingWrite + iRemain,
			pChannel->iPendingWriteLen - iRemain);
		pChannel->iPendingWriteLen = iRemain;
		pChannel->iPendingWritePos = 0u;
	}
	iAvail = XSSH_RUNTIME_CHANNEL_PENDING_CAP - pChannel->iPendingWriteLen;
	iCopy = (iLen < iAvail) ? iLen : iAvail;
	if ( iCopy != 0u ) {
		memcpy(pChannel->arrPendingWrite + pChannel->iPendingWriteLen, pData, iCopy);
		pChannel->iPendingWriteLen += iCopy;
	}
	return iCopy;
}

static int xsshChannelFlushPending(xssh_channel_t* pChannel)
{
	uint8 arrPayload[XSSH_PACKET_MAX_DEFAULT];
	xsshwriter wr;
	xssh_session_t* pSession;
	size_t iFrameCap = sizeof(arrPayload) - 16u;
	int iRet;

	if ( pChannel == NULL || pChannel->pSession == NULL || pChannel->pSession->bMock ) {
		return XSSH_OK;
	}
	pSession = pChannel->pSession;
	while ( xsshChannelPendingLen(pChannel) != 0u && pChannel->iRemoteWindow != 0u ) {
		size_t iSendLen = xsshChannelPendingLen(pChannel);

		if ( iSendLen > (size_t)pChannel->iMaxPacket ) {
			iSendLen = (size_t)pChannel->iMaxPacket;
		}
		if ( iSendLen > (size_t)pChannel->iRemoteWindow ) {
			iSendLen = (size_t)pChannel->iRemoteWindow;
		}
		if ( iSendLen > iFrameCap ) {
			iSendLen = iFrameCap;
		}
		if ( iSendLen == 0u ) {
			break;
		}
		xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
		iRet = xsshChannelDataWrite(&wr, pChannel->iRemoteChannel,
			pChannel->arrPendingWrite + pChannel->iPendingWritePos, iSendLen);
		if ( iRet != XSSH_OK ) {
			xsshSecureZero(arrPayload, sizeof(arrPayload));
			xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_CHANNEL, "pending CHANNEL_DATA build failed");
			return iRet;
		}
		pSession->iPacketIoStage = XSSH_STAGE_CHANNEL;
		iRet = xsshTransportSendPacket(pSession, arrPayload, wr.iLen);
		xsshSecureZero(arrPayload, sizeof(arrPayload));
		if ( iRet != XSSH_OK ) {
			return iRet;
		}
		pChannel->iRemoteWindow -= (uint32)iSendLen;
		pChannel->iPendingWritePos += iSendLen;
	}
	if ( pChannel->iPendingWritePos >= pChannel->iPendingWriteLen ) {
		xsshSecureZero(pChannel->arrPendingWrite, pChannel->iPendingWriteLen);
		pChannel->iPendingWriteLen = 0u;
		pChannel->iPendingWritePos = 0u;
	}
	return XSSH_OK;
}

static int xsshTransportHandleChannelOpenPacket(xssh_session_t* pSession, const xsshpacket* pPkt)
{
	xsshchannelopenmsg openMsg;
	xsshchanneltcpipopenmsg tcpipOpen;
	xssh_channel_t* pChannel;
	uint8 arrPayload[512];
	xsshwriter wr;
	int iRet;

	if ( pSession == NULL || pPkt == NULL ) {
		return XSSH_ERR_INVALID;
	}
	iRet = xsshChannelOpenRead(pPkt->tPayload.pData, pPkt->tPayload.iLen, &openMsg);
	if ( iRet != XSSH_OK ) {
		xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_CHANNEL, "CHANNEL_OPEN parse failed");
		return iRet;
	}
	if ( !xsshSliceEqualText(openMsg.tChannelType, "forwarded-tcpip") ||
		xsshChannelOpenTcpipExtraRead(&openMsg, &tcpipOpen) != XSSH_OK ) {
		xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
		if ( xsshChannelOpenFailureWrite(&wr, openMsg.iSenderChannel, XSSH_OPEN_UNKNOWN_CHANNEL_TYPE,
				"unsupported server channel type", "") == XSSH_OK ) {
			(void)xsshTransportSendPacket(pSession, arrPayload, wr.iLen);
		}
		xsshSecureZero(arrPayload, sizeof(arrPayload));
		return XSSH_OK;
	}
	pChannel = xsshRuntimeAllocChannel(pSession);
	if ( pChannel == NULL ) {
		xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
		if ( xsshChannelOpenFailureWrite(&wr, openMsg.iSenderChannel, XSSH_OPEN_RESOURCE_SHORTAGE,
				"no local channel slots", "") == XSSH_OK ) {
			(void)xsshTransportSendPacket(pSession, arrPayload, wr.iLen);
		}
		xsshSecureZero(arrPayload, sizeof(arrPayload));
		xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_NO_SPACE, XSSH_STAGE_CHANNEL, "no local channel slots for CHANNEL_OPEN");
		return XSSH_ERR_NO_SPACE;
	}
	pChannel->iRemoteChannel = openMsg.iSenderChannel;
	pChannel->iRemoteWindow = openMsg.iInitialWindow;
	if ( openMsg.iMaxPacket != 0u && openMsg.iMaxPacket < pChannel->iMaxPacket ) {
		pChannel->iMaxPacket = openMsg.iMaxPacket;
	}
	pChannel->bOpen = TRUE;
	xsshTransportCopySliceText(pChannel->sChannelType, sizeof(pChannel->sChannelType), openMsg.tChannelType);
	xsshTransportCopySliceText(pChannel->sConnectedHost, sizeof(pChannel->sConnectedHost), tcpipOpen.tConnectedHost);
	pChannel->iConnectedPort = tcpipOpen.iConnectedPort;
	xsshTransportCopySliceText(pChannel->sOriginatorHost, sizeof(pChannel->sOriginatorHost), tcpipOpen.tOriginatorHost);
	pChannel->iOriginatorPort = tcpipOpen.iOriginatorPort;

	xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
	iRet = xsshChannelOpenConfirmationWrite(&wr, openMsg.iSenderChannel, pChannel->iLocalChannel,
		pChannel->iLocalWindow, pChannel->iMaxPacket);
	if ( iRet != XSSH_OK ) {
		xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_CHANNEL, "CHANNEL_OPEN_CONFIRMATION build failed");
		xsshSecureZero(arrPayload, sizeof(arrPayload));
		return iRet;
	}
	iRet = xsshTransportSendPacket(pSession, arrPayload, wr.iLen);
	xsshSecureZero(arrPayload, sizeof(arrPayload));
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	return xsshRuntimePushEvent(pSession, XSSH_EVENT_CHANNEL_OPEN, pChannel, NULL, 0u, XSSH_OK, "forwarded-tcpip");
}

static int xsshTransportHandleChannelPacket(xssh_session_t* pSession, const xsshpacket* pPkt)
{
	uint8 iMsg;

	if ( pSession == NULL || pPkt == NULL || pPkt->tPayload.iLen == 0u ) {
		return XSSH_ERR_BAD_PACKET;
	}
	iMsg = pPkt->tPayload.pData[0];
	if ( iMsg == XSSH_MSG_IGNORE || iMsg == XSSH_MSG_DEBUG ) {
		return XSSH_OK;
	}
	if ( iMsg == XSSH_MSG_DISCONNECT ) {
		return xsshTransportHandleDisconnectPacket(pSession, pPkt);
	}
	if ( iMsg == XSSH_MSG_GLOBAL_REQUEST ) {
		xsshglobalrequestmsg req;
		int iRet = xsshMsgReadGlobalRequest(pPkt->tPayload.pData, pPkt->tPayload.iLen, &req);

		if ( iRet != XSSH_OK ) {
			xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_CHANNEL, "GLOBAL_REQUEST parse failed");
			return iRet;
		}
		if ( req.bWantReply ) {
			uint8 arrPayload[16];
			xsshwriter wr;

			xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
			iRet = xsshMsgWriteRequestFailure(&wr);
			if ( iRet != XSSH_OK ) {
				xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_CHANNEL, "REQUEST_FAILURE build failed");
				return iRet;
			}
			pSession->iPacketIoStage = XSSH_STAGE_CHANNEL;
			iRet = xsshTransportSendPacket(pSession, arrPayload, wr.iLen);
			xsshSecureZero(arrPayload, sizeof(arrPayload));
			if ( iRet != XSSH_OK ) {
				return iRet;
			}
		}
		return XSSH_OK;
	}
	if ( iMsg == XSSH_MSG_CHANNEL_OPEN ) {
		return xsshTransportHandleChannelOpenPacket(pSession, pPkt);
	}
	if ( iMsg == XSSH_MSG_CHANNEL_DATA || iMsg == XSSH_MSG_CHANNEL_EXTENDED_DATA ) {
		xsshchanneldataeventmsg data;
		xssh_channel_t* pChannel;
		int iRet = (iMsg == XSSH_MSG_CHANNEL_DATA) ?
			xsshChannelDataRead(pPkt->tPayload.pData, pPkt->tPayload.iLen, &data) :
			xsshChannelExtendedDataRead(pPkt->tPayload.pData, pPkt->tPayload.iLen, &data);
		if ( iRet != XSSH_OK ) {
			xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_CHANNEL, "CHANNEL_DATA parse failed");
			return iRet;
		}
		pChannel = xsshRuntimeFindChannelByLocal(pSession, data.iRecipientChannel);
		if ( pChannel == NULL ) {
			xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_BAD_PACKET, XSSH_STAGE_CHANNEL, "CHANNEL_DATA recipient not found");
			return XSSH_ERR_BAD_PACKET;
		}
		if ( data.tData.iLen <= pChannel->iLocalWindow ) {
			pChannel->iLocalWindow -= (uint32)data.tData.iLen;
		} else {
			pChannel->iLocalWindow = 0u;
		}
		iRet = xsshRuntimeAppendChannelData(pChannel, (iMsg == XSSH_MSG_CHANNEL_EXTENDED_DATA) ? TRUE : FALSE,
			data.tData.pData, data.tData.iLen);
		if ( iRet != XSSH_OK ) {
			return iRet;
		}
		return xsshTransportMaybeAdjustLocalWindow(pChannel);
	}
	if ( iMsg == XSSH_MSG_CHANNEL_WINDOW_ADJUST ) {
		xsshchannelwindowadjustmsg adj;
		xssh_channel_t* pChannel;
		int iRet = xsshChannelWindowAdjustRead(pPkt->tPayload.pData, pPkt->tPayload.iLen, &adj);
		if ( iRet != XSSH_OK ) {
			xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_CHANNEL, "CHANNEL_WINDOW_ADJUST parse failed");
			return iRet;
		}
		pChannel = xsshRuntimeFindChannelByLocal(pSession, adj.iRecipientChannel);
		if ( pChannel == NULL ) {
			xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_BAD_PACKET, XSSH_STAGE_CHANNEL, "CHANNEL_WINDOW_ADJUST recipient not found");
			return XSSH_ERR_BAD_PACKET;
		}
		if ( adj.iBytes > 0xffffffffu - pChannel->iRemoteWindow ) {
			pChannel->iRemoteWindow = 0xffffffffu;
		} else {
			pChannel->iRemoteWindow += adj.iBytes;
		}
		return xsshChannelFlushPending(pChannel);
	}
	if ( iMsg == XSSH_MSG_CHANNEL_EOF || iMsg == XSSH_MSG_CHANNEL_CLOSE ) {
		uint32 iRecipient = 0u;
		xssh_channel_t* pChannel;
		int iRet = (iMsg == XSSH_MSG_CHANNEL_EOF) ?
			xsshChannelEofRead(pPkt->tPayload.pData, pPkt->tPayload.iLen, &iRecipient) :
			xsshChannelCloseRead(pPkt->tPayload.pData, pPkt->tPayload.iLen, &iRecipient);
		if ( iRet != XSSH_OK ) {
			xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_CHANNEL,
				(iMsg == XSSH_MSG_CHANNEL_EOF) ? "CHANNEL_EOF parse failed" : "CHANNEL_CLOSE parse failed");
			return iRet;
		}
		pChannel = xsshRuntimeFindChannelByLocal(pSession, iRecipient);
		if ( pChannel == NULL ) {
			xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_BAD_PACKET, XSSH_STAGE_CHANNEL, "channel recipient not found");
			return XSSH_ERR_BAD_PACKET;
		}
		if ( iMsg == XSSH_MSG_CHANNEL_EOF ) {
			pChannel->bEofRecv = TRUE;
			return xsshRuntimePushEvent(pSession, XSSH_EVENT_CHANNEL_EOF, pChannel, NULL, 0u, XSSH_OK, NULL);
		}
		pChannel->bCloseRecv = TRUE;
		pChannel->bOpen = FALSE;
		if ( !pChannel->bCloseSent ) {
			uint8 arrPayload[64];
			xsshwriter wr;
			xsshresult tSavedResult;

			xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
			iRet = xsshChannelCloseWrite(&wr, pChannel->iRemoteChannel);
			if ( iRet != XSSH_OK ) {
				xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_CHANNEL, "CHANNEL_CLOSE reply build failed");
				return iRet;
			}
			tSavedResult = pSession->tResult;
			pSession->iPacketIoStage = XSSH_STAGE_CHANNEL;
			iRet = xsshTransportSendPacket(pSession, arrPayload, wr.iLen);
			xsshSecureZero(arrPayload, sizeof(arrPayload));
			if ( iRet != XSSH_OK ) {
				/* 远端已经关闭 channel，回 close 是 best-effort；不能吞掉已收到的合法 close 事件。 */
				pSession->tResult = tSavedResult;
			}
			pChannel->bCloseSent = TRUE;
		}
		return xsshRuntimeFinalizeChannelClose(pChannel);
	}
	if ( iMsg == XSSH_MSG_CHANNEL_REQUEST ) {
		xsshchannelrequestmsg req;
		xssh_channel_t* pChannel;
		int iRet = xsshChannelRequestRead(pPkt->tPayload.pData, pPkt->tPayload.iLen, &req);
		if ( iRet != XSSH_OK ) {
			xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_CHANNEL, "CHANNEL_REQUEST parse failed");
			return iRet;
		}
		pChannel = xsshRuntimeFindChannelByLocal(pSession, req.iRecipientChannel);
		if ( pChannel == NULL ) {
			xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_BAD_PACKET, XSSH_STAGE_CHANNEL, "CHANNEL_REQUEST recipient not found");
			return XSSH_ERR_BAD_PACKET;
		}
		if ( xsshSliceEqualText(req.tRequestType, "exit-status") ) {
			uint32 iExitStatus = 0u;
			iRet = xsshChannelRequestExitStatusParse(&req, &iExitStatus);
			if ( iRet != XSSH_OK ) {
				xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_CHANNEL, "exit-status parse failed");
				return iRet;
			}
			pChannel->bHasExitStatus = TRUE;
			pChannel->iExitStatus = iExitStatus;
			return XSSH_OK;
		}
		if ( xsshSliceEqualText(req.tRequestType, "exit-signal") ) {
			xsshchannelexitsignalmsg sig;
			iRet = xsshChannelRequestExitSignalParse(&req, &sig);
			if ( iRet != XSSH_OK ) {
				xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_CHANNEL, "exit-signal parse failed");
				return iRet;
			}
			xsshTransportCopySliceText(pChannel->sExitSignal, sizeof(pChannel->sExitSignal), sig.tSignalName);
			pChannel->bHasExitSignal = TRUE;
			return XSSH_OK;
		}
		return XSSH_OK;
	}
	return XSSH_OK;
}

static int xsshTransportWaitChannelReply(xssh_channel_t* pChannel, const void* pPayload, size_t iPayloadLen, const char* sContext)
{
	xssh_session_t* pSession;
	uint8 arrPlain[8192];
	xsshpacket pkt;
	int iRet;

	if ( pChannel == NULL || pChannel->pSession == NULL || (pPayload == NULL && iPayloadLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	pSession = pChannel->pSession;
	pSession->iPacketIoStage = XSSH_STAGE_CHANNEL;
	iRet = xsshTransportSendPacket(pSession, pPayload, iPayloadLen);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	for (;;) {
		if ( xsshTransportReadPacket(pSession, &pkt, arrPlain, sizeof(arrPlain)) != XSSH_OK ) {
			xsshSecureZero(arrPlain, sizeof(arrPlain));
			return pSession->tResult.iError ? pSession->tResult.iError : XSSH_ERR_IO;
		}
		if ( pkt.tPayload.iLen == 0u ) {
			xsshSecureZero(arrPlain, sizeof(arrPlain));
			xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_BAD_PACKET, XSSH_STAGE_CHANNEL, "empty channel reply payload");
			return XSSH_ERR_BAD_PACKET;
		}
		if ( pkt.tPayload.pData[0] == XSSH_MSG_CHANNEL_SUCCESS || pkt.tPayload.pData[0] == XSSH_MSG_CHANNEL_FAILURE ) {
			uint8 iReplyMsg = pkt.tPayload.pData[0];
			uint32 iRecipient = 0u;
			iRet = (iReplyMsg == XSSH_MSG_CHANNEL_SUCCESS) ?
				xsshChannelSuccessRead(pkt.tPayload.pData, pkt.tPayload.iLen, &iRecipient) :
				xsshChannelFailureRead(pkt.tPayload.pData, pkt.tPayload.iLen, &iRecipient);
			xsshSecureZero(arrPlain, sizeof(arrPlain));
			if ( iRet != XSSH_OK ) {
				xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_CHANNEL, "channel request reply parse failed");
				return iRet;
			}
			if ( iRecipient != pChannel->iLocalChannel ) {
				xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_BAD_PACKET, XSSH_STAGE_CHANNEL, "channel request reply recipient mismatch");
				return XSSH_ERR_BAD_PACKET;
			}
			if ( iReplyMsg == XSSH_MSG_CHANNEL_FAILURE ) {
				xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_CHANNEL_FAILED, XSSH_STAGE_CHANNEL,
					sContext ? sContext : "channel request failed");
				return XSSH_ERR_CHANNEL_FAILED;
			}
			return XSSH_OK;
		}
		iRet = xsshTransportHandleChannelPacket(pSession, &pkt);
		if ( iRet != XSSH_OK ) {
			xsshSecureZero(arrPlain, sizeof(arrPlain));
			return iRet;
		}
	}
}

static int xsshTransportWaitGlobalRequestReplyEx(xssh_session_t* pSession, const void* pPayload, size_t iPayloadLen,
	xsshglobalrequestsuccessmsg* pSuccess, const char* sContext, bool bAcceptFailure)
{
	uint8 arrPlain[8192];
	xsshpacket pkt;
	int iRet;

	if ( pSession == NULL || (pPayload == NULL && iPayloadLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	pSession->iPacketIoStage = XSSH_STAGE_CHANNEL;
	iRet = xsshTransportSendPacket(pSession, pPayload, iPayloadLen);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	for (;;) {
		iRet = xsshTransportReadPacket(pSession, &pkt, arrPlain, sizeof(arrPlain));
		if ( iRet != XSSH_OK ) {
			xsshSecureZero(arrPlain, sizeof(arrPlain));
			return iRet;
		}
		if ( pkt.tPayload.iLen == 0u ) {
			xsshSecureZero(arrPlain, sizeof(arrPlain));
			xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_BAD_PACKET, XSSH_STAGE_CHANNEL, "empty global request reply payload");
			return XSSH_ERR_BAD_PACKET;
		}
		if ( pkt.tPayload.pData[0] == XSSH_MSG_REQUEST_SUCCESS ) {
			xsshglobalrequestsuccessmsg tSuccess;
			memset(&tSuccess, 0, sizeof(tSuccess));
			iRet = xsshMsgReadRequestSuccess(pkt.tPayload.pData, pkt.tPayload.iLen, &tSuccess);
			xsshSecureZero(arrPlain, sizeof(arrPlain));
			if ( iRet != XSSH_OK ) {
				xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_CHANNEL, "REQUEST_SUCCESS parse failed");
				return iRet;
			}
			if ( pSuccess != NULL ) {
				*pSuccess = tSuccess;
			}
			return XSSH_OK;
		}
		if ( pkt.tPayload.pData[0] == XSSH_MSG_REQUEST_FAILURE ) {
			iRet = xsshMsgReadRequestFailure(pkt.tPayload.pData, pkt.tPayload.iLen);
			xsshSecureZero(arrPlain, sizeof(arrPlain));
			if ( iRet != XSSH_OK ) {
				xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_CHANNEL, "REQUEST_FAILURE parse failed");
				return iRet;
			}
			if ( bAcceptFailure ) {
				return XSSH_OK;
			}
			xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_CHANNEL_FAILED, XSSH_STAGE_CHANNEL,
				sContext ? sContext : "global request failed");
			return XSSH_ERR_CHANNEL_FAILED;
		}
		iRet = xsshTransportHandleChannelPacket(pSession, &pkt);
		if ( iRet != XSSH_OK ) {
			xsshSecureZero(arrPlain, sizeof(arrPlain));
			return iRet;
		}
	}
}

static int xsshTransportWaitGlobalRequestReply(xssh_session_t* pSession, const void* pPayload, size_t iPayloadLen,
	xsshglobalrequestsuccessmsg* pSuccess, const char* sContext)
{
	return xsshTransportWaitGlobalRequestReplyEx(pSession, pPayload, iPayloadLen, pSuccess, sContext, FALSE);
}

static int xsshOpenSessionChannel(xssh_session_t* pSession, xssh_channel_t** ppChannel)
{
	size_t i;
	xssh_channel_t* pChannel = NULL;

	if ( pSession == NULL || ppChannel == NULL ) {
		return XSSH_ERR_INVALID;
	}
	*ppChannel = NULL;
	if ( !pSession->bConnected ) {
		return XSSH_ERR_NOT_CONNECTED;
	}
	if ( !pSession->bAuthenticated ) {
		return XSSH_ERR_AUTH_FAILED;
	}
	for ( i = 0u; i < XSSH_RUNTIME_MAX_CHANNELS; ++i ) {
		if ( pSession->arrChannels[i].pSession == NULL || pSession->arrChannels[i].bClosed ) {
			pChannel = &pSession->arrChannels[i];
			break;
		}
	}
	if ( pChannel == NULL ) {
		return XSSH_ERR_NO_SPACE;
	}
	memset(pChannel, 0, sizeof(*pChannel));
	pChannel->pSession = pSession;
	pChannel->iLocalChannel = pSession->iNextLocalChannel++;
	pChannel->iLocalWindow = pSession->tConfig.iChannelWindow ? pSession->tConfig.iChannelWindow : XSSH_CHANNEL_WINDOW_DEFAULT;
	pChannel->iMaxPacket = 32768u;
	snprintf(pChannel->sChannelType, sizeof(pChannel->sChannelType), "%s", "session");
	if ( pSession->bMock ) {
		pChannel->iRemoteChannel = 1000u + pChannel->iLocalChannel;
		pChannel->iRemoteWindow = pChannel->iLocalWindow;
		pChannel->bOpen = TRUE;
		*ppChannel = pChannel;
		return XSSH_OK;
	}
	{
		uint8 arrPayload[256];
		uint8 arrPlain[8192];
		xsshwriter wr;
		xsshpacket pkt;
		int iRet;

		pSession->iPacketIoStage = XSSH_STAGE_CHANNEL;
		xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
		iRet = xsshChannelOpenSessionWrite(&wr, pChannel->iLocalChannel, pChannel->iLocalWindow, pChannel->iMaxPacket);
		if ( iRet != XSSH_OK ) {
			xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_CHANNEL, "CHANNEL_OPEN build failed");
			memset(pChannel, 0, sizeof(*pChannel));
			return iRet;
		}
		iRet = xsshTransportSendPacket(pSession, arrPayload, wr.iLen);
		if ( iRet != XSSH_OK ) {
			memset(pChannel, 0, sizeof(*pChannel));
			return iRet;
		}
		for (;;) {
			iRet = xsshTransportReadPacket(pSession, &pkt, arrPlain, sizeof(arrPlain));
			if ( iRet != XSSH_OK ) {
				xsshSecureZero(arrPlain, sizeof(arrPlain));
				memset(pChannel, 0, sizeof(*pChannel));
				return iRet;
			}
			if ( pkt.tPayload.iLen == 0u ) {
				xsshSecureZero(arrPlain, sizeof(arrPlain));
				memset(pChannel, 0, sizeof(*pChannel));
				xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_BAD_PACKET, XSSH_STAGE_CHANNEL, "empty CHANNEL_OPEN reply");
				return XSSH_ERR_BAD_PACKET;
			}
			if ( pkt.tPayload.pData[0] == XSSH_MSG_CHANNEL_OPEN_CONFIRMATION ) {
				xsshchannelopenconfirmmsg confirm;
				iRet = xsshChannelOpenConfirmationRead(pkt.tPayload.pData, pkt.tPayload.iLen, &confirm);
				xsshSecureZero(arrPlain, sizeof(arrPlain));
				if ( iRet != XSSH_OK || confirm.iRecipientChannel != pChannel->iLocalChannel ) {
					memset(pChannel, 0, sizeof(*pChannel));
					xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_BAD_PACKET, XSSH_STAGE_CHANNEL, "CHANNEL_OPEN_CONFIRMATION mismatch");
					return XSSH_ERR_BAD_PACKET;
				}
				pChannel->iRemoteChannel = confirm.iSenderChannel;
				pChannel->iRemoteWindow = confirm.iInitialWindow;
				pChannel->iMaxPacket = confirm.iMaxPacket;
				pChannel->bOpen = TRUE;
				*ppChannel = pChannel;
				return XSSH_OK;
			}
			if ( pkt.tPayload.pData[0] == XSSH_MSG_CHANNEL_OPEN_FAILURE ) {
				xsshchannelopenfailuremsg fail;
				iRet = xsshChannelOpenFailureRead(pkt.tPayload.pData, pkt.tPayload.iLen, &fail);
				xsshSecureZero(arrPlain, sizeof(arrPlain));
				memset(pChannel, 0, sizeof(*pChannel));
				if ( iRet != XSSH_OK ) {
					xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_CHANNEL, "CHANNEL_OPEN_FAILURE parse failed");
					return iRet;
				}
				xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_CHANNEL_FAILED, XSSH_STAGE_CHANNEL, "CHANNEL_OPEN rejected by server");
				return XSSH_ERR_CHANNEL_FAILED;
			}
			iRet = xsshTransportHandleChannelPacket(pSession, &pkt);
			if ( iRet != XSSH_OK ) {
				xsshSecureZero(arrPlain, sizeof(arrPlain));
				memset(pChannel, 0, sizeof(*pChannel));
				return iRet;
			}
		}
	}
	pChannel->bOpen = TRUE;
	*ppChannel = pChannel;
	return XSSH_OK;
}

static int xsshOpenDirectTcpipChannel(xssh_session_t* pSession, xssh_channel_t** ppChannel,
	const char* sTargetHost, uint16 iTargetPort, const char* sOriginatorHost, uint16 iOriginatorPort)
{
	size_t i;
	xssh_channel_t* pChannel = NULL;

	if ( pSession == NULL || ppChannel == NULL || sTargetHost == NULL || iTargetPort == 0u ) {
		return XSSH_ERR_INVALID;
	}
	*ppChannel = NULL;
	if ( !pSession->bConnected ) {
		return XSSH_ERR_NOT_CONNECTED;
	}
	if ( !pSession->bAuthenticated ) {
		return XSSH_ERR_AUTH_FAILED;
	}
	for ( i = 0u; i < XSSH_RUNTIME_MAX_CHANNELS; ++i ) {
		if ( pSession->arrChannels[i].pSession == NULL || pSession->arrChannels[i].bClosed ) {
			pChannel = &pSession->arrChannels[i];
			break;
		}
	}
	if ( pChannel == NULL ) {
		return XSSH_ERR_NO_SPACE;
	}
	memset(pChannel, 0, sizeof(*pChannel));
	pChannel->pSession = pSession;
	pChannel->iLocalChannel = pSession->iNextLocalChannel++;
	pChannel->iLocalWindow = pSession->tConfig.iChannelWindow ? pSession->tConfig.iChannelWindow : XSSH_CHANNEL_WINDOW_DEFAULT;
	pChannel->iMaxPacket = 32768u;
	if ( sOriginatorHost == NULL ) {
		sOriginatorHost = "127.0.0.1";
	}
	snprintf(pChannel->sChannelType, sizeof(pChannel->sChannelType), "%s", "direct-tcpip");
	snprintf(pChannel->sConnectedHost, sizeof(pChannel->sConnectedHost), "%s", sTargetHost);
	pChannel->iConnectedPort = iTargetPort;
	snprintf(pChannel->sOriginatorHost, sizeof(pChannel->sOriginatorHost), "%s", sOriginatorHost);
	pChannel->iOriginatorPort = iOriginatorPort;
	if ( pSession->bMock ) {
		pChannel->iRemoteChannel = 1000u + pChannel->iLocalChannel;
		pChannel->iRemoteWindow = pChannel->iLocalWindow;
		pChannel->bOpen = TRUE;
		*ppChannel = pChannel;
		(void)sOriginatorHost;
		(void)iOriginatorPort;
		return XSSH_OK;
	}
	{
		uint8 arrPayload[1024];
		uint8 arrPlain[8192];
		xsshwriter wr;
		xsshpacket pkt;
		int iRet;

		pSession->iPacketIoStage = XSSH_STAGE_CHANNEL;
		xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
		iRet = xsshChannelOpenDirectTcpipWrite(&wr, pChannel->iLocalChannel, pChannel->iLocalWindow, pChannel->iMaxPacket,
			sTargetHost, iTargetPort, sOriginatorHost, iOriginatorPort);
		if ( iRet != XSSH_OK ) {
			xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_CHANNEL, "direct-tcpip CHANNEL_OPEN build failed");
			memset(pChannel, 0, sizeof(*pChannel));
			return iRet;
		}
		iRet = xsshTransportSendPacket(pSession, arrPayload, wr.iLen);
		if ( iRet != XSSH_OK ) {
			memset(pChannel, 0, sizeof(*pChannel));
			return iRet;
		}
		for (;;) {
			iRet = xsshTransportReadPacket(pSession, &pkt, arrPlain, sizeof(arrPlain));
			if ( iRet != XSSH_OK ) {
				xsshSecureZero(arrPlain, sizeof(arrPlain));
				memset(pChannel, 0, sizeof(*pChannel));
				return iRet;
			}
			if ( pkt.tPayload.iLen == 0u ) {
				xsshSecureZero(arrPlain, sizeof(arrPlain));
				memset(pChannel, 0, sizeof(*pChannel));
				xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_BAD_PACKET, XSSH_STAGE_CHANNEL, "empty direct-tcpip reply");
				return XSSH_ERR_BAD_PACKET;
			}
			if ( pkt.tPayload.pData[0] == XSSH_MSG_CHANNEL_OPEN_CONFIRMATION ) {
				xsshchannelopenconfirmmsg confirm;
				iRet = xsshChannelOpenConfirmationRead(pkt.tPayload.pData, pkt.tPayload.iLen, &confirm);
				xsshSecureZero(arrPlain, sizeof(arrPlain));
				if ( iRet != XSSH_OK || confirm.iRecipientChannel != pChannel->iLocalChannel ) {
					memset(pChannel, 0, sizeof(*pChannel));
					xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_BAD_PACKET, XSSH_STAGE_CHANNEL, "direct-tcpip confirmation mismatch");
					return XSSH_ERR_BAD_PACKET;
				}
				pChannel->iRemoteChannel = confirm.iSenderChannel;
				pChannel->iRemoteWindow = confirm.iInitialWindow;
				pChannel->iMaxPacket = confirm.iMaxPacket;
				pChannel->bOpen = TRUE;
				*ppChannel = pChannel;
				return XSSH_OK;
			}
			if ( pkt.tPayload.pData[0] == XSSH_MSG_CHANNEL_OPEN_FAILURE ) {
				xsshchannelopenfailuremsg fail;
				iRet = xsshChannelOpenFailureRead(pkt.tPayload.pData, pkt.tPayload.iLen, &fail);
				xsshSecureZero(arrPlain, sizeof(arrPlain));
				memset(pChannel, 0, sizeof(*pChannel));
				if ( iRet != XSSH_OK ) {
					xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_CHANNEL, "direct-tcpip failure parse failed");
					return iRet;
				}
				xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_CHANNEL_FAILED, XSSH_STAGE_CHANNEL, "direct-tcpip rejected by server");
				return XSSH_ERR_CHANNEL_FAILED;
			}
			iRet = xsshTransportHandleChannelPacket(pSession, &pkt);
			if ( iRet != XSSH_OK ) {
				xsshSecureZero(arrPlain, sizeof(arrPlain));
				memset(pChannel, 0, sizeof(*pChannel));
				return iRet;
			}
		}
	}
}

static int xsshRequestTcpipForward(xssh_session_t* pSession, const char* sBindHost, uint16 iBindPort, uint16* pBoundPort)
{
	if ( pSession == NULL || sBindHost == NULL || pBoundPort == NULL ) {
		return XSSH_ERR_INVALID;
	}
	*pBoundPort = 0u;
	if ( !pSession->bConnected ) {
		return XSSH_ERR_NOT_CONNECTED;
	}
	if ( !pSession->bAuthenticated ) {
		return XSSH_ERR_AUTH_FAILED;
	}
	if ( !pSession->bMock ) {
		uint8 arrPayload[512];
		xsshwriter wr;
		xsshglobalrequestsuccessmsg success;
		int iRet;

		xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
		iRet = xsshMsgWriteTcpipForward(&wr, sBindHost, iBindPort, TRUE);
		if ( iRet != XSSH_OK ) {
			xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_CHANNEL, "tcpip-forward request build failed");
			return iRet;
		}
		memset(&success, 0, sizeof(success));
		iRet = xsshTransportWaitGlobalRequestReply(pSession, arrPayload, wr.iLen, &success, "tcpip-forward rejected by server");
		if ( iRet != XSSH_OK ) {
			return iRet;
		}
		if ( iBindPort == 0u ) {
			if ( !success.bHasBoundPort || success.iBoundPort > 65535u ) {
				xsshRuntimeSetResult(pSession, FALSE, XSSH_ERR_BAD_PACKET, XSSH_STAGE_CHANNEL, "tcpip-forward success missing bound port");
				return XSSH_ERR_BAD_PACKET;
			}
			*pBoundPort = (uint16)success.iBoundPort;
		} else {
			*pBoundPort = iBindPort;
		}
		return XSSH_OK;
	}
	/* mock runtime 不打开真实 remote bind；端口 0 用固定候选值模拟服务端分配端口。 */
	*pBoundPort = (iBindPort == 0u) ? 10022u : iBindPort;
	return XSSH_OK;
}

static int xsshCancelTcpipForward(xssh_session_t* pSession, const char* sBindHost, uint16 iBindPort)
{
	if ( pSession == NULL || sBindHost == NULL || iBindPort == 0u ) {
		return XSSH_ERR_INVALID;
	}
	if ( !pSession->bConnected ) {
		return XSSH_ERR_NOT_CONNECTED;
	}
	if ( !pSession->bAuthenticated ) {
		return XSSH_ERR_AUTH_FAILED;
	}
	if ( !pSession->bMock ) {
		uint8 arrPayload[512];
		xsshwriter wr;
		int iRet;

		xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
		iRet = xsshMsgWriteCancelTcpipForward(&wr, sBindHost, iBindPort, TRUE);
		if ( iRet != XSSH_OK ) {
			xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_CHANNEL, "cancel-tcpip-forward request build failed");
			return iRet;
		}
		return xsshTransportWaitGlobalRequestReply(pSession, arrPayload, wr.iLen, NULL, "cancel-tcpip-forward rejected by server");
	}
	return XSSH_OK;
}

static int xsshSendIgnore(xssh_session_t* pSession, const void* pData, size_t iLen)
{
	uint8 arrPayload[1024];
	xsshwriter wr;
	int iRet;

	if ( pSession == NULL || (pData == NULL && iLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	if ( !pSession->bConnected ) {
		return XSSH_ERR_NOT_CONNECTED;
	}
	if ( pSession->bMock ) {
		return XSSH_OK;
	}
	xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
	iRet = xsshMsgWriteIgnore(&wr, pData, iLen);
	if ( iRet != XSSH_OK ) {
		xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_CHANNEL, "IGNORE build failed");
		return iRet;
	}
	pSession->iPacketIoStage = XSSH_STAGE_CHANNEL;
	return xsshTransportSendPacket(pSession, arrPayload, wr.iLen);
}

static int xsshSendDebug(xssh_session_t* pSession, bool bAlwaysDisplay, const char* sMessage, const char* sLanguageTag)
{
	uint8 arrPayload[1024];
	xsshwriter wr;
	int iRet;

	if ( pSession == NULL ) {
		return XSSH_ERR_INVALID;
	}
	if ( !pSession->bConnected ) {
		return XSSH_ERR_NOT_CONNECTED;
	}
	if ( pSession->bMock ) {
		(void)bAlwaysDisplay;
		(void)sMessage;
		(void)sLanguageTag;
		return XSSH_OK;
	}
	xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
	iRet = xsshMsgWriteDebug(&wr, bAlwaysDisplay, sMessage, sLanguageTag);
	if ( iRet != XSSH_OK ) {
		xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_CHANNEL, "DEBUG build failed");
		return iRet;
	}
	pSession->iPacketIoStage = XSSH_STAGE_CHANNEL;
	return xsshTransportSendPacket(pSession, arrPayload, wr.iLen);
}

static int xsshSendKeepalive(xssh_session_t* pSession)
{
	uint8 arrPayload[128];
	xsshwriter wr;
	int iRet;

	if ( pSession == NULL ) {
		return XSSH_ERR_INVALID;
	}
	if ( !pSession->bConnected ) {
		return XSSH_ERR_NOT_CONNECTED;
	}
	if ( !pSession->bAuthenticated ) {
		return XSSH_ERR_AUTH_FAILED;
	}
	if ( pSession->bMock ) {
		return XSSH_OK;
	}
	xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
	iRet = xsshMsgWriteKeepalive(&wr, TRUE);
	if ( iRet != XSSH_OK ) {
		xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_CHANNEL, "keepalive request build failed");
		return iRet;
	}
	/*
		OpenSSH 常见实现会对未知 global request 返回 REQUEST_FAILURE。
		对 keepalive 来说，能收到规范回复就已经证明连接仍然存活。
	*/
	return xsshTransportWaitGlobalRequestReplyEx(pSession, arrPayload, wr.iLen, NULL,
		"keepalive request failed", TRUE);
}

static int xsshChannelRequestPty(xssh_channel_t* pChannel, const xsshpty* pPty)
{
	uint8 arrPayload[512];
	xsshwriter wr;
	int iRet;

	if ( pChannel == NULL || !pChannel->bOpen || pChannel->bClosed ) {
		return XSSH_ERR_CLOSED;
	}
	if ( pChannel->pSession != NULL && pChannel->pSession->bMock ) {
		return XSSH_OK;
	}
	xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
	iRet = xsshChannelRequestPtyWrite(&wr, pChannel->iRemoteChannel, TRUE, pPty);
	if ( iRet != XSSH_OK ) {
		xsshRuntimeSetResult(pChannel->pSession, FALSE, iRet, XSSH_STAGE_CHANNEL, "PTY request build failed");
		return iRet;
	}
	return xsshTransportWaitChannelReply(pChannel, arrPayload, wr.iLen, "PTY request failed");
}

static int xsshChannelRequestShell(xssh_channel_t* pChannel)
{
	uint8 arrPayload[128];
	xsshwriter wr;
	int iRet;

	if ( pChannel == NULL || !pChannel->bOpen || pChannel->bClosed ) {
		return XSSH_ERR_CLOSED;
	}
	if ( pChannel->pSession != NULL && !pChannel->pSession->bMock ) {
		xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
		iRet = xsshChannelRequestShellWrite(&wr, pChannel->iRemoteChannel, TRUE);
		if ( iRet != XSSH_OK ) {
			xsshRuntimeSetResult(pChannel->pSession, FALSE, iRet, XSSH_STAGE_CHANNEL, "shell request build failed");
			return iRet;
		}
		return xsshTransportWaitChannelReply(pChannel, arrPayload, wr.iLen, "shell request failed");
	}
	return xsshRuntimeAppendChannelData(pChannel, FALSE, "$ ", 2u);
}

static int xsshChannelRequestExec(xssh_channel_t* pChannel, const char* sCommand)
{
	uint8 arrPayload[2048];
	xsshwriter wr;
	const char* sOut = "ok\n";
	int iRet;

	if ( pChannel == NULL || !pChannel->bOpen || pChannel->bClosed || sCommand == NULL ) {
		return XSSH_ERR_INVALID;
	}
	if ( pChannel->pSession != NULL && !pChannel->pSession->bMock ) {
		xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
		iRet = xsshChannelRequestExecWrite(&wr, pChannel->iRemoteChannel, TRUE, sCommand);
		if ( iRet != XSSH_OK ) {
			xsshRuntimeSetResult(pChannel->pSession, FALSE, iRet, XSSH_STAGE_CHANNEL, "exec request build failed");
			return iRet;
		}
		return xsshTransportWaitChannelReply(pChannel, arrPayload, wr.iLen, "exec request failed");
	}
	/* mock runtime 只验证 exec 数据通路；真实服务端输出由 transport pump 注入。 */
	if ( strstr(sCommand, "stderr") != NULL ) {
		iRet = xsshRuntimeAppendChannelData(pChannel, TRUE, "err\n", 4u);
		if ( iRet != XSSH_OK ) return iRet;
	}
	iRet = xsshRuntimeAppendChannelData(pChannel, FALSE, sOut, strlen(sOut));
	if ( iRet != XSSH_OK ) return iRet;
	pChannel->bHasExitStatus = TRUE;
	pChannel->iExitStatus = 0u;
	return XSSH_OK;
}

static int xsshChannelRequestSubsystem(xssh_channel_t* pChannel, const char* sSubsystem)
{
	uint8 arrPayload[512];
	xsshwriter wr;
	int iRet;

	if ( pChannel == NULL || sSubsystem == NULL ) {
		return XSSH_ERR_INVALID;
	}
	if ( !pChannel->bOpen || pChannel->bClosed ) {
		return XSSH_ERR_CLOSED;
	}
	if ( pChannel->pSession != NULL && pChannel->pSession->bMock ) {
		return XSSH_OK;
	}
	xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
	iRet = xsshChannelRequestSubsystemWrite(&wr, pChannel->iRemoteChannel, TRUE, sSubsystem);
	if ( iRet != XSSH_OK ) {
		xsshRuntimeSetResult(pChannel->pSession, FALSE, iRet, XSSH_STAGE_CHANNEL, "subsystem request build failed");
		return iRet;
	}
	return xsshTransportWaitChannelReply(pChannel, arrPayload, wr.iLen, "subsystem request failed");
}

static int xsshChannelSetEnv(xssh_channel_t* pChannel, const char* sName, const char* sValue)
{
	uint8 arrPayload[1024];
	xsshwriter wr;
	int iRet;

	if ( pChannel == NULL || sName == NULL || sValue == NULL ) {
		return XSSH_ERR_INVALID;
	}
	if ( !pChannel->bOpen || pChannel->bClosed ) {
		return XSSH_ERR_CLOSED;
	}
	if ( pChannel->pSession != NULL && pChannel->pSession->bMock ) {
		return XSSH_OK;
	}
	xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
	iRet = xsshChannelRequestEnvWrite(&wr, pChannel->iRemoteChannel, TRUE, sName, sValue);
	if ( iRet != XSSH_OK ) {
		xsshRuntimeSetResult(pChannel->pSession, FALSE, iRet, XSSH_STAGE_CHANNEL, "env request build failed");
		return iRet;
	}
	return xsshTransportWaitChannelReply(pChannel, arrPayload, wr.iLen, "env request failed");
}

static int xsshChannelWrite(xssh_channel_t* pChannel, const void* pData, size_t iLen, size_t* pWritten)
{
	uint8 arrPayload[XSSH_PACKET_MAX_DEFAULT];
	xsshwriter wr;
	int iRet;

	if ( pWritten == NULL || pChannel == NULL || (pData == NULL && iLen != 0u) ) {
		return XSSH_ERR_INVALID;
	}
	*pWritten = 0u;
	if ( !pChannel->bOpen || pChannel->bClosed ) {
		return XSSH_ERR_CLOSED;
	}
	if ( pChannel->pSession != NULL && !pChannel->pSession->bMock ) {
		const uint8* pBytes = (const uint8*)pData;
		size_t iFrameCap = sizeof(arrPayload) - 16u;
		size_t iAccepted = 0u;

		if ( iLen == 0u ) {
			return XSSH_OK;
		}
		iRet = xsshChannelFlushPending(pChannel);
		if ( iRet != XSSH_OK ) {
			return iRet;
		}
		if ( xsshChannelPendingLen(pChannel) != 0u ) {
			iAccepted = xsshChannelAppendPending(pChannel, pBytes, iLen);
			*pWritten = iAccepted;
			if ( iAccepted == 0u ) {
				xsshRuntimeSetResult(pChannel->pSession, FALSE, XSSH_ERR_NO_SPACE, XSSH_STAGE_CHANNEL, "channel pending write queue is full");
				return XSSH_ERR_NO_SPACE;
			}
			return XSSH_OK;
		}
		while ( iAccepted < iLen && pChannel->iRemoteWindow != 0u ) {
			size_t iSendLen = iLen - iAccepted;

			if ( iSendLen > (size_t)pChannel->iMaxPacket ) {
				iSendLen = (size_t)pChannel->iMaxPacket;
			}
			if ( iSendLen > (size_t)pChannel->iRemoteWindow ) {
				iSendLen = (size_t)pChannel->iRemoteWindow;
			}
			if ( iSendLen > iFrameCap ) {
				iSendLen = iFrameCap;
			}
			if ( iSendLen == 0u ) {
				break;
			}
			xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
			iRet = xsshChannelDataWrite(&wr, pChannel->iRemoteChannel, pBytes + iAccepted, iSendLen);
			if ( iRet != XSSH_OK ) {
				xsshRuntimeSetResult(pChannel->pSession, FALSE, iRet, XSSH_STAGE_CHANNEL, "CHANNEL_DATA build failed");
				return iRet;
			}
			pChannel->pSession->iPacketIoStage = XSSH_STAGE_CHANNEL;
			iRet = xsshTransportSendPacket(pChannel->pSession, arrPayload, wr.iLen);
			xsshSecureZero(arrPayload, sizeof(arrPayload));
			if ( iRet != XSSH_OK ) {
				return iRet;
			}
			pChannel->iRemoteWindow -= (uint32)iSendLen;
			iAccepted += iSendLen;
		}
		if ( iAccepted < iLen ) {
			iAccepted += xsshChannelAppendPending(pChannel, pBytes + iAccepted, iLen - iAccepted);
		}
		*pWritten = iAccepted;
		if ( iAccepted == 0u ) {
			xsshRuntimeSetResult(pChannel->pSession, FALSE, XSSH_ERR_NO_SPACE, XSSH_STAGE_CHANNEL, "channel remote window and pending queue are full");
			return XSSH_ERR_NO_SPACE;
		}
		return XSSH_OK;
	}
	iRet = xsshRuntimeAppendChannelData(pChannel, FALSE, pData, iLen);
	if ( iRet != XSSH_OK ) {
		return iRet;
	}
	*pWritten = iLen;
	return XSSH_OK;
}

static int xsshChannelRead(xssh_channel_t* pChannel, void* pOut, size_t iCap, size_t* pRead)
{
	if ( pChannel == NULL ) {
		return XSSH_ERR_INVALID;
	}
	return xsshRuntimeChannelReadBuffer(pChannel->arrStdout, pChannel->iStdoutLen, &pChannel->iStdoutPos, pOut, iCap, pRead);
}

static int xsshChannelReadStderr(xssh_channel_t* pChannel, void* pOut, size_t iCap, size_t* pRead)
{
	if ( pChannel == NULL ) {
		return XSSH_ERR_INVALID;
	}
	return xsshRuntimeChannelReadBuffer(pChannel->arrStderr, pChannel->iStderrLen, &pChannel->iStderrPos, pOut, iCap, pRead);
}

static int xsshChannelResizePty(xssh_channel_t* pChannel, uint32 iCols, uint32 iRows, uint32 iPixelWidth, uint32 iPixelHeight)
{
	uint8 arrPayload[128];
	xsshwriter wr;
	int iRet;

	if ( pChannel == NULL || !pChannel->bOpen || pChannel->bClosed ) {
		return XSSH_ERR_CLOSED;
	}
	if ( pChannel->pSession != NULL && pChannel->pSession->bMock ) {
		(void)iCols;
		(void)iRows;
		(void)iPixelWidth;
		(void)iPixelHeight;
		return XSSH_OK;
	}
	xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
	iRet = xsshChannelRequestWindowChangeWrite(&wr, pChannel->iRemoteChannel, iCols, iRows, iPixelWidth, iPixelHeight);
	if ( iRet != XSSH_OK ) {
		xsshRuntimeSetResult(pChannel->pSession, FALSE, iRet, XSSH_STAGE_CHANNEL, "window-change build failed");
		return iRet;
	}
	pChannel->pSession->iPacketIoStage = XSSH_STAGE_CHANNEL;
	return xsshTransportSendPacket(pChannel->pSession, arrPayload, wr.iLen);
}

static int xsshChannelSendSignal(xssh_channel_t* pChannel, const char* sSignalName)
{
	uint8 arrPayload[128];
	xsshwriter wr;
	int iRet;

	if ( pChannel == NULL || sSignalName == NULL ) {
		return XSSH_ERR_INVALID;
	}
	if ( !pChannel->bOpen || pChannel->bClosed ) {
		return XSSH_ERR_CLOSED;
	}
	if ( pChannel->pSession != NULL && pChannel->pSession->bMock ) {
		return XSSH_OK;
	}
	xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
	iRet = xsshChannelRequestSignalWrite(&wr, pChannel->iRemoteChannel, FALSE, sSignalName);
	if ( iRet != XSSH_OK ) {
		xsshRuntimeSetResult(pChannel->pSession, FALSE, iRet, XSSH_STAGE_CHANNEL, "signal request build failed");
		return iRet;
	}
	pChannel->pSession->iPacketIoStage = XSSH_STAGE_CHANNEL;
	return xsshTransportSendPacket(pChannel->pSession, arrPayload, wr.iLen);
}

static int xsshChannelSendBreak(xssh_channel_t* pChannel, uint32 iBreakLengthMs)
{
	uint8 arrPayload[128];
	xsshwriter wr;
	int iRet;

	if ( pChannel == NULL || !pChannel->bOpen || pChannel->bClosed ) {
		return XSSH_ERR_CLOSED;
	}
	if ( pChannel->pSession != NULL && pChannel->pSession->bMock ) {
		(void)iBreakLengthMs;
		return XSSH_OK;
	}
	xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
	iRet = xsshChannelRequestBreakWrite(&wr, pChannel->iRemoteChannel, FALSE, iBreakLengthMs);
	if ( iRet != XSSH_OK ) {
		xsshRuntimeSetResult(pChannel->pSession, FALSE, iRet, XSSH_STAGE_CHANNEL, "break request build failed");
		return iRet;
	}
	pChannel->pSession->iPacketIoStage = XSSH_STAGE_CHANNEL;
	return xsshTransportSendPacket(pChannel->pSession, arrPayload, wr.iLen);
}

static int xsshChannelSendEof(xssh_channel_t* pChannel)
{
	uint8 arrPayload[64];
	xsshwriter wr;
	int iRet;

	if ( pChannel == NULL || !pChannel->bOpen || pChannel->bClosed ) {
		return XSSH_ERR_CLOSED;
	}
	if ( pChannel->pSession != NULL && !pChannel->pSession->bMock ) {
		xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
		iRet = xsshChannelEofWrite(&wr, pChannel->iRemoteChannel);
		if ( iRet != XSSH_OK ) {
			xsshRuntimeSetResult(pChannel->pSession, FALSE, iRet, XSSH_STAGE_CHANNEL, "CHANNEL_EOF build failed");
			return iRet;
		}
		pChannel->pSession->iPacketIoStage = XSSH_STAGE_CHANNEL;
		iRet = xsshTransportSendPacket(pChannel->pSession, arrPayload, wr.iLen);
		if ( iRet != XSSH_OK ) {
			return iRet;
		}
	}
	pChannel->bEofSent = TRUE;
	return xsshRuntimePushEvent(pChannel->pSession, XSSH_EVENT_CHANNEL_EOF, pChannel, NULL, 0u, XSSH_OK, NULL);
}

static int xsshChannelClose(xssh_channel_t* pChannel)
{
	uint8 arrPayload[64];
	xsshwriter wr;
	int iRet;

	if ( pChannel == NULL ) {
		return XSSH_ERR_INVALID;
	}
	if ( pChannel->pSession == NULL ) {
		return XSSH_ERR_INVALID;
	}
	if ( pChannel->bClosed ) {
		return XSSH_OK;
	}
	if ( pChannel->pSession->bMock ) {
		pChannel->bCloseSent = TRUE;
		pChannel->bCloseRecv = TRUE;
		pChannel->bOpen = FALSE;
		return xsshRuntimeFinalizeChannelClose(pChannel);
	}
	if ( !pChannel->bCloseSent && pChannel->bOpen ) {
		xsshWriterInit(&wr, arrPayload, sizeof(arrPayload));
		iRet = xsshChannelCloseWrite(&wr, pChannel->iRemoteChannel);
		if ( iRet != XSSH_OK ) {
			xsshRuntimeSetResult(pChannel->pSession, FALSE, iRet, XSSH_STAGE_CHANNEL, "CHANNEL_CLOSE build failed");
			return iRet;
		}
		pChannel->pSession->iPacketIoStage = XSSH_STAGE_CHANNEL;
		iRet = xsshTransportSendPacket(pChannel->pSession, arrPayload, wr.iLen);
		xsshSecureZero(arrPayload, sizeof(arrPayload));
		if ( iRet != XSSH_OK ) {
			return iRet;
		}
	}
	pChannel->bCloseSent = TRUE;
	pChannel->bOpen = FALSE;
	return xsshRuntimeFinalizeChannelClose(pChannel);
}

static int xsshChannelGetExitStatus(xssh_channel_t* pChannel, uint32* pExitStatus)
{
	if ( pChannel == NULL || pExitStatus == NULL ) {
		return XSSH_ERR_INVALID;
	}
	if ( !pChannel->bHasExitStatus ) {
		return XSSH_WIRE_NEED_MORE;
	}
	*pExitStatus = pChannel->iExitStatus;
	return XSSH_OK;
}

static int xsshChannelGetExitSignal(xssh_channel_t* pChannel, char* sOut, size_t iOutCap)
{
	if ( pChannel == NULL || sOut == NULL || iOutCap == 0u ) {
		return XSSH_ERR_INVALID;
	}
	sOut[0] = '\0';
	if ( !pChannel->bHasExitSignal ) {
		return XSSH_WIRE_NEED_MORE;
	}
	snprintf(sOut, iOutCap, "%s", pChannel->sExitSignal);
	return XSSH_OK;
}

static int xsshTransportTryReadBufferedPacket(xssh_session_t* pSession, xsshpacket* pOut, uint8* pPlain, size_t iPlainCap, bool* pHasPacket)
{
	xsshreader rd;
	int iRet;

	if ( pHasPacket != NULL ) {
		*pHasPacket = FALSE;
	}
	if ( pSession == NULL || pOut == NULL || pPlain == NULL || iPlainCap == 0u || pHasPacket == NULL ) {
		return XSSH_ERR_INVALID;
	}
	pSession->iPacketIoStage = XSSH_STAGE_CHANNEL;
	xrtMutexLock(pSession->pLock);
	if ( pSession->iWireRecvLen == 0u ) {
		xrtMutexUnlock(pSession->pLock);
		return XSSH_OK;
	}
	xsshReaderInit(&rd, pSession->arrWireRecv, pSession->iWireRecvLen);
	iRet = xsshPacketCodecRead(&pSession->tPacketCodec, &rd,
		pSession->tConfig.iMaxPacketSize ? pSession->tConfig.iMaxPacketSize : XSSH_PACKET_MAX_DEFAULT,
		pOut, pPlain, iPlainCap);
	if ( iRet == XSSH_OK ) {
		if ( rd.iPos < pSession->iWireRecvLen ) {
			memmove(pSession->arrWireRecv, pSession->arrWireRecv + rd.iPos, pSession->iWireRecvLen - rd.iPos);
		}
		pSession->iWireRecvLen -= rd.iPos;
		*pHasPacket = TRUE;
		xrtMutexUnlock(pSession->pLock);
		return XSSH_OK;
	}
	xrtMutexUnlock(pSession->pLock);
	if ( iRet == XSSH_WIRE_NEED_MORE ) {
		return XSSH_OK;
	}
	xsshRuntimeSetResult(pSession, FALSE, iRet, XSSH_STAGE_CHANNEL, "xssh buffered packet decode failed");
	return iRet;
}

static int xsshPoll(xssh_session_t* pSession, uint32 iTimeoutMs)
{
	uint8 arrPlain[8192];
	xsshpacket pkt;
	bool bHasPacket = FALSE;
	int iRet;

	if ( pSession == NULL || !pSession->bConnected ) {
		return XSSH_ERR_NOT_CONNECTED;
	}
	if ( pSession->bMock ) {
		(void)iTimeoutMs;
		return XSSH_OK;
	}
	if ( iTimeoutMs != 0u ) {
		pSession->iPacketIoStage = XSSH_STAGE_CHANNEL;
		iRet = xsshTransportReadPacket(pSession, &pkt, arrPlain, sizeof(arrPlain));
		if ( iRet != XSSH_OK ) {
			xsshSecureZero(arrPlain, sizeof(arrPlain));
			return iRet;
		}
		iRet = xsshTransportHandleChannelPacket(pSession, &pkt);
		xsshSecureZero(arrPlain, sizeof(arrPlain));
		return iRet;
	}
	for (;;) {
		iRet = xsshTransportTryReadBufferedPacket(pSession, &pkt, arrPlain, sizeof(arrPlain), &bHasPacket);
		if ( iRet != XSSH_OK ) {
			xsshSecureZero(arrPlain, sizeof(arrPlain));
			return iRet;
		}
		if ( !bHasPacket ) {
			xsshSecureZero(arrPlain, sizeof(arrPlain));
			return XSSH_OK;
		}
		iRet = xsshTransportHandleChannelPacket(pSession, &pkt);
		if ( iRet != XSSH_OK ) {
			xsshSecureZero(arrPlain, sizeof(arrPlain));
			return iRet;
		}
	}
}

static int xsshNextEvent(xssh_session_t* pSession, xsshevent* pOut)
{
	xsshevententry* pEntry;

	if ( pSession == NULL || pOut == NULL ) {
		return XSSH_ERR_INVALID;
	}
	memset(pOut, 0, sizeof(*pOut));
	if ( pSession->iEventCount == 0u ) {
		pOut->iType = XSSH_EVENT_NONE;
		return XSSH_OK;
	}
	pEntry = &pSession->arrEvents[pSession->iEventHead];
	*pOut = pEntry->tEvent;
	if ( pEntry->iDataLen != 0u ) {
		memcpy(pSession->arrLastEventData, pEntry->arrData, pEntry->iDataLen);
		pOut->pData = pSession->arrLastEventData;
		pOut->iLen = pEntry->iDataLen;
	}
	memset(pEntry, 0, sizeof(*pEntry));
	pSession->iEventHead = (pSession->iEventHead + 1u) % XSSH_RUNTIME_EVENT_CAP;
	pSession->iEventCount--;
	return XSSH_OK;
}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#endif
