# xssh 公共符号参考

此文件由 `tools/generate_api_reference.py` 从 `extlibs/xssh/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `586` 个函数、`517` 个常量或宏、
`155` 个公共类型。

## `extlibs/xssh/include/xrt/ssh_auth_guard.h`

[查看带契约注释的公共头](../../include/xrt/ssh_auth_guard.h)

### 函数 (5)

- `xrtSshAuthGuardCheck`
- `xrtSshAuthGuardComplete`
- `xrtSshAuthGuardInit`
- `xrtSshAuthGuardPolicyInit`
- `xrtSshAuthGuardReserve`

### 常量与宏 (19)

- `XSSH_AUTH_DEFAULT_ATTEMPT_LIMIT`
- `XSSH_AUTH_DEFAULT_BYTE_LIMIT`
- `XSSH_AUTH_DEFAULT_MESSAGE_LIMIT`
- `XSSH_AUTH_DEFAULT_ROUND_LIMIT`
- `XSSH_AUTH_DEFAULT_TIMEOUT_MS`
- `XSSH_AUTH_EVENT_ATTEMPT`
- `XSSH_AUTH_EVENT_MESSAGE`
- `XSSH_AUTH_EVENT_ROUND`
- `XSSH_AUTH_EXHAUST_ATTEMPTS`
- `XSSH_AUTH_EXHAUST_BYTES`
- `XSSH_AUTH_EXHAUST_MESSAGES`
- `XSSH_AUTH_EXHAUST_NONE`
- `XSSH_AUTH_EXHAUST_ROUNDS`
- `XSSH_AUTH_EXHAUST_TIMEOUT`
- `XSSH_AUTH_GUARD_ALLOW`
- `XSSH_AUTH_GUARD_DISCONNECT`
- `XSSH_AUTH_GUARD_IGNORE`
- `XSSH_FEATURE_AUTH_GUARD`
- `XSSH_FEATURE_WIRE`

### 类型 (6)

- `xsshauthevent`
- `xsshauthexhaustion`
- `xsshauthguard`
- `xsshauthguarddecision`
- `xsshauthguardpolicy`
- `xsshcode`

## `extlibs/xssh/include/xrt/ssh_auth_hostbased.h`

[查看带契约注释的公共头](../../include/xrt/ssh_auth_hostbased.h)

### 函数 (4)

- `xrtSshAuthHostBasedRead`
- `xrtSshAuthHostBasedSignDataWrite`
- `xrtSshAuthHostBasedWrite`
- `xrtSshAuthHostNameValid`

### 常量与宏 (4)

- `XSSH_AUTH_HOST_NAME_MAX`
- `XSSH_FEATURE_AUTH_HOSTBASED`
- `XSSH_FEATURE_AUTH_MESSAGE`
- `XSSH_FEATURE_HOSTKEY`

### 类型 (2)

- `xsshauthhostbased`
- `xsshwriter`

## `extlibs/xssh/include/xrt/ssh_auth_keyboard.h`

[查看带契约注释的公共头](../../include/xrt/ssh_auth_keyboard.h)

### 函数 (9)

- `xrtSshAuthKeyboardChallengeNext`
- `xrtSshAuthKeyboardChallengeRead`
- `xrtSshAuthKeyboardChallengeWrite`
- `xrtSshAuthKeyboardRead`
- `xrtSshAuthKeyboardResponseNext`
- `xrtSshAuthKeyboardResponseRead`
- `xrtSshAuthKeyboardResponseWrite`
- `xrtSshAuthKeyboardWrite`
- `xrtSshAuthKeyboardWriteLanguage`

### 常量与宏 (3)

- `XSSH_FEATURE_AUTH_KEYBOARD`
- `XSSH_MSG_USERAUTH_INFO_REQUEST`
- `XSSH_MSG_USERAUTH_INFO_RESPONSE`

### 类型 (5)

- `xsshauthkeyboard`
- `xsshauthkeyboardchallenge`
- `xsshauthkeyboardprompt`
- `xsshauthkeyboardresponses`
- `xsshreader`

## `extlibs/xssh/include/xrt/ssh_auth_message.h`

[查看带契约注释的公共头](../../include/xrt/ssh_auth_message.h)

### 函数 (11)

- `xrtSshAuthBannerRead`
- `xrtSshAuthBannerWrite`
- `xrtSshAuthFailureRead`
- `xrtSshAuthFailureWrite`
- `xrtSshAuthNoneRead`
- `xrtSshAuthNoneWrite`
- `xrtSshAuthRequestRead`
- `xrtSshAuthRequestSize`
- `xrtSshAuthRequestWrite`
- `xrtSshAuthSuccessRead`
- `xrtSshAuthSuccessWrite`

### 常量与宏 (11)

- `XSSH_AUTH_METHOD_HOSTBASED`
- `XSSH_AUTH_METHOD_KEYBOARD_INTERACTIVE`
- `XSSH_AUTH_METHOD_NONE`
- `XSSH_AUTH_METHOD_PASSWORD`
- `XSSH_AUTH_METHOD_PUBLICKEY`
- `XSSH_MSG_USERAUTH_BANNER`
- `XSSH_MSG_USERAUTH_FAILURE`
- `XSSH_MSG_USERAUTH_REQUEST`
- `XSSH_MSG_USERAUTH_SUCCESS`
- `XSSH_SERVICE_CONNECTION`
- `XSSH_SERVICE_USERAUTH`

### 类型 (3)

- `xsshauthbanner`
- `xsshauthfailure`
- `xsshauthrequest`

## `extlibs/xssh/include/xrt/ssh_auth_password.h`

[查看带契约注释的公共头](../../include/xrt/ssh_auth_password.h)

### 函数 (5)

- `xrtSshAuthPasswordChangeWrite`
- `xrtSshAuthPasswordPromptRead`
- `xrtSshAuthPasswordPromptWrite`
- `xrtSshAuthPasswordRead`
- `xrtSshAuthPasswordWrite`

### 常量与宏 (2)

- `XSSH_FEATURE_AUTH_PASSWORD`
- `XSSH_MSG_USERAUTH_PASSWD_CHANGEREQ`

### 类型 (2)

- `xsshauthpassword`
- `xsshauthpasswordprompt`

## `extlibs/xssh/include/xrt/ssh_auth_publickey.h`

[查看带契约注释的公共头](../../include/xrt/ssh_auth_publickey.h)

### 函数 (6)

- `xrtSshAuthPublicKeyOkRead`
- `xrtSshAuthPublicKeyOkWrite`
- `xrtSshAuthPublicKeyRead`
- `xrtSshAuthPublicKeySignDataWrite`
- `xrtSshAuthPublicKeySignedWrite`
- `xrtSshAuthPublicKeyWrite`

### 常量与宏 (2)

- `XSSH_FEATURE_AUTH_PUBLICKEY`
- `XSSH_MSG_USERAUTH_PK_OK`

### 类型 (2)

- `xsshauthpublickey`
- `xsshauthpublickeyok`

## `extlibs/xssh/include/xrt/ssh_auth_session.h`

[查看带契约注释的公共头](../../include/xrt/ssh_auth_session.h)

### 函数 (18)

- `xrtSshAuthSessionBanner`
- `xrtSshAuthSessionBegin`
- `xrtSshAuthSessionBudget`
- `xrtSshAuthSessionCheck`
- `xrtSshAuthSessionClear`
- `xrtSshAuthSessionComplete`
- `xrtSshAuthSessionEvent`
- `xrtSshAuthSessionFail`
- `xrtSshAuthSessionFailure`
- `xrtSshAuthSessionInit`
- `xrtSshAuthSessionMethod`
- `xrtSshAuthSessionReadAbort`
- `xrtSshAuthSessionReadCommit`
- `xrtSshAuthSessionReadPrepare`
- `xrtSshAuthSessionRequest`
- `xrtSshAuthSessionWriteAbort`
- `xrtSshAuthSessionWriteCommit`
- `xrtSshAuthSessionWritePrepare`

### 常量与宏 (26)

- `XSSH_AUTH_SESSION_AUTHENTICATION`
- `XSSH_AUTH_SESSION_COMPLETE`
- `XSSH_AUTH_SESSION_EVENT_COMPLETE`
- `XSSH_AUTH_SESSION_EVENT_FAILED`
- `XSSH_AUTH_SESSION_EVENT_NONE`
- `XSSH_AUTH_SESSION_EVENT_READ_REQUEST`
- `XSSH_AUTH_SESSION_EVENT_READ_RESULT`
- `XSSH_AUTH_SESSION_EVENT_READ_SERVICE_ACCEPT`
- `XSSH_AUTH_SESSION_EVENT_READ_SERVICE_REQUEST`
- `XSSH_AUTH_SESSION_EVENT_WRITE_REQUEST`
- `XSSH_AUTH_SESSION_EVENT_WRITE_RESULT`
- `XSSH_AUTH_SESSION_EVENT_WRITE_SERVICE_ACCEPT`
- `XSSH_AUTH_SESSION_EVENT_WRITE_SERVICE_REQUEST`
- `XSSH_AUTH_SESSION_FAILED`
- `XSSH_AUTH_SESSION_IDLE`
- `XSSH_AUTH_SESSION_PACKET_BANNER`
- `XSSH_AUTH_SESSION_PACKET_FAILURE`
- `XSSH_AUTH_SESSION_PACKET_METHOD`
- `XSSH_AUTH_SESSION_PACKET_NONE`
- `XSSH_AUTH_SESSION_PACKET_REQUEST`
- `XSSH_AUTH_SESSION_PACKET_SERVICE_ACCEPT`
- `XSSH_AUTH_SESSION_PACKET_SERVICE_REQUEST`
- `XSSH_AUTH_SESSION_PACKET_SUCCESS`
- `XSSH_AUTH_SESSION_SERVICE`
- `XSSH_FEATURE_AUTH_SESSION`
- `XSSH_FEATURE_TRANSPORT_CORE`

### 类型 (6)

- `xsshauthsession`
- `xsshauthsessionevent`
- `xsshauthsessionpacket`
- `xsshauthsessionphase`
- `xsshrole`
- `xsshtransportcore`

## `extlibs/xssh/include/xrt/ssh_channel_core.h`

[查看带契约注释的公共头](../../include/xrt/ssh_channel_core.h)

### 函数 (26)

- `xrtSshChannelCoreAcceptCommit`
- `xrtSshChannelCoreAcceptInit`
- `xrtSshChannelCoreAdjustLimit`
- `xrtSshChannelCoreAdjustReady`
- `xrtSshChannelCoreAdjustReceiveCommit`
- `xrtSshChannelCoreAdjustSendCommit`
- `xrtSshChannelCoreCanReceiveRequest`
- `xrtSshChannelCoreCanSendRequest`
- `xrtSshChannelCoreClear`
- `xrtSshChannelCoreCloseReceiveCommit`
- `xrtSshChannelCoreCloseSendCommit`
- `xrtSshChannelCoreClosed`
- `xrtSshChannelCoreConfirmationCommit`
- `xrtSshChannelCoreDataConsume`
- `xrtSshChannelCoreDataReceiveCommit`
- `xrtSshChannelCoreDataSendCommit`
- `xrtSshChannelCoreEofReceiveCommit`
- `xrtSshChannelCoreEofSendCommit`
- `xrtSshChannelCoreFailureCommit`
- `xrtSshChannelCoreIds`
- `xrtSshChannelCoreOpen`
- `xrtSshChannelCoreOpenInit`
- `xrtSshChannelCorePhase`
- `xrtSshChannelCoreRecipientCheck`
- `xrtSshChannelCoreRejectCommit`
- `xrtSshChannelCoreSendLimit`

### 常量与宏 (9)

- `XSSH_CHANNEL_CORE_ACCEPTING`
- `XSSH_CHANNEL_CORE_CLOSED`
- `XSSH_CHANNEL_CORE_FAILED`
- `XSSH_CHANNEL_CORE_OPEN`
- `XSSH_CHANNEL_CORE_OPENING`
- `XSSH_FEATURE_CHANNEL_CORE`
- `XSSH_FEATURE_CHANNEL_MESSAGE`
- `XSSH_FEATURE_CHANNEL_STATE`
- `XSSH_FEATURE_CHANNEL_WINDOW`

### 类型 (8)

- `xsshchanneladjust`
- `xsshchannelconfirmation`
- `xsshchannelcore`
- `xsshchannelcorephase`
- `xsshchannelopen`
- `xsshchannelopenfailure`
- `xsshchannelstate`
- `xsshchannelwindow`

## `extlibs/xssh/include/xrt/ssh_channel_io.h`

[查看带契约注释的公共头](../../include/xrt/ssh_channel_io.h)

### 函数 (21)

- `xrtSshChannelIoClear`
- `xrtSshChannelIoConfigInit`
- `xrtSshChannelIoConsume`
- `xrtSshChannelIoInit`
- `xrtSshChannelIoQueued`
- `xrtSshChannelIoRead`
- `xrtSshChannelIoReadBuffer`
- `xrtSshChannelIoReadable`
- `xrtSshChannelIoReceiveAbort`
- `xrtSshChannelIoReceiveCommit`
- `xrtSshChannelIoReceivePrepare`
- `xrtSshChannelIoSendAbort`
- `xrtSshChannelIoSendCommit`
- `xrtSshChannelIoSendLimit`
- `xrtSshChannelIoSendPrepare`
- `xrtSshChannelIoWritable`
- `xrtSshChannelIoWrite`
- `xrtSshChannelIoWriteBorrow`
- `xrtSshChannelIoWriteBuffer`
- `xrtSshChannelIoWriteRef`
- `xrtSshChannelIoWriteTake`

### 常量与宏 (7)

- `XSSH_CHANNEL_IO_DATA`
- `XSSH_CHANNEL_IO_LIMIT_DEFAULT`
- `XSSH_CHANNEL_IO_PENDING_NONE`
- `XSSH_CHANNEL_IO_PENDING_RECEIVE`
- `XSSH_CHANNEL_IO_PENDING_SEND`
- `XSSH_CHANNEL_IO_STDERR`
- `XSSH_FEATURE_CHANNEL_IO`

### 类型 (4)

- `xsshchannelio`
- `xsshchannelioconfig`
- `xsshchanneliopending`
- `xsshchanneliostream`

## `extlibs/xssh/include/xrt/ssh_channel_message.h`

[查看带契约注释的公共头](../../include/xrt/ssh_channel_message.h)

### 函数 (22)

- `xrtSshChannelCloseRead`
- `xrtSshChannelCloseWrite`
- `xrtSshChannelDataRead`
- `xrtSshChannelDataWrite`
- `xrtSshChannelEofRead`
- `xrtSshChannelEofWrite`
- `xrtSshChannelExtendedDataRead`
- `xrtSshChannelExtendedDataWrite`
- `xrtSshChannelFailureRead`
- `xrtSshChannelFailureWrite`
- `xrtSshChannelOpenConfirmationRead`
- `xrtSshChannelOpenConfirmationWrite`
- `xrtSshChannelOpenFailureRead`
- `xrtSshChannelOpenFailureWrite`
- `xrtSshChannelOpenRead`
- `xrtSshChannelOpenWrite`
- `xrtSshChannelRequestRead`
- `xrtSshChannelRequestWrite`
- `xrtSshChannelSuccessRead`
- `xrtSshChannelSuccessWrite`
- `xrtSshChannelWindowAdjustRead`
- `xrtSshChannelWindowAdjustWrite`

### 常量与宏 (16)

- `XSSH_CHANNEL_EXTENDED_DATA_STDERR`
- `XSSH_CHANNEL_OPEN_ADMINISTRATIVELY_PROHIBITED`
- `XSSH_CHANNEL_OPEN_CONNECT_FAILED`
- `XSSH_CHANNEL_OPEN_RESOURCE_SHORTAGE`
- `XSSH_CHANNEL_OPEN_UNKNOWN_CHANNEL_TYPE`
- `XSSH_MSG_CHANNEL_CLOSE`
- `XSSH_MSG_CHANNEL_DATA`
- `XSSH_MSG_CHANNEL_EOF`
- `XSSH_MSG_CHANNEL_EXTENDED_DATA`
- `XSSH_MSG_CHANNEL_FAILURE`
- `XSSH_MSG_CHANNEL_OPEN`
- `XSSH_MSG_CHANNEL_OPEN_CONFIRMATION`
- `XSSH_MSG_CHANNEL_OPEN_FAILURE`
- `XSSH_MSG_CHANNEL_REQUEST`
- `XSSH_MSG_CHANNEL_SUCCESS`
- `XSSH_MSG_CHANNEL_WINDOW_ADJUST`

### 类型 (3)

- `xsshchanneldata`
- `xsshchannelextendeddata`
- `xsshchannelrequest`

## `extlibs/xssh/include/xrt/ssh_channel_pty.h`

[查看带契约注释的公共头](../../include/xrt/ssh_channel_pty.h)

### 函数 (6)

- `xrtSshChannelPtyRead`
- `xrtSshChannelPtyWrite`
- `xrtSshTerminalModeEnd`
- `xrtSshTerminalModeWrite`
- `xrtSshTerminalModesNext`
- `xrtSshTerminalModesRead`

### 常量与宏 (60)

- `XSSH_CHANNEL_REQUEST_PTY`
- `XSSH_FEATURE_CHANNEL_PTY`
- `XSSH_FEATURE_CHANNEL_REQUEST`
- `XSSH_TTY_OP_CS7`
- `XSSH_TTY_OP_CS8`
- `XSSH_TTY_OP_ECHO`
- `XSSH_TTY_OP_ECHOCTL`
- `XSSH_TTY_OP_ECHOE`
- `XSSH_TTY_OP_ECHOK`
- `XSSH_TTY_OP_ECHOKE`
- `XSSH_TTY_OP_ECHONL`
- `XSSH_TTY_OP_END`
- `XSSH_TTY_OP_ICANON`
- `XSSH_TTY_OP_ICRNL`
- `XSSH_TTY_OP_IEXTEN`
- `XSSH_TTY_OP_IGNCR`
- `XSSH_TTY_OP_IGNPAR`
- `XSSH_TTY_OP_IMAXBEL`
- `XSSH_TTY_OP_INLCR`
- `XSSH_TTY_OP_INPCK`
- `XSSH_TTY_OP_ISIG`
- `XSSH_TTY_OP_ISPEED`
- `XSSH_TTY_OP_ISTRIP`
- `XSSH_TTY_OP_IUCLC`
- `XSSH_TTY_OP_IXANY`
- `XSSH_TTY_OP_IXOFF`
- `XSSH_TTY_OP_IXON`
- `XSSH_TTY_OP_NOFLSH`
- `XSSH_TTY_OP_OCRNL`
- `XSSH_TTY_OP_OLCUC`
- `XSSH_TTY_OP_ONLCR`
- `XSSH_TTY_OP_ONLRET`
- `XSSH_TTY_OP_ONOCR`
- `XSSH_TTY_OP_OPOST`
- `XSSH_TTY_OP_OSPEED`
- `XSSH_TTY_OP_PARENB`
- `XSSH_TTY_OP_PARMRK`
- `XSSH_TTY_OP_PARODD`
- `XSSH_TTY_OP_PENDIN`
- `XSSH_TTY_OP_TOSTOP`
- `XSSH_TTY_OP_UNSUPPORTED_MIN`
- `XSSH_TTY_OP_VDISCARD`
- `XSSH_TTY_OP_VDSUSP`
- `XSSH_TTY_OP_VEOF`
- `XSSH_TTY_OP_VEOL`
- `XSSH_TTY_OP_VEOL2`
- `XSSH_TTY_OP_VERASE`
- `XSSH_TTY_OP_VFLUSH`
- `XSSH_TTY_OP_VINTR`
- `XSSH_TTY_OP_VKILL`
- `XSSH_TTY_OP_VLNEXT`
- `XSSH_TTY_OP_VQUIT`
- `XSSH_TTY_OP_VREPRINT`
- `XSSH_TTY_OP_VSTART`
- `XSSH_TTY_OP_VSTATUS`
- `XSSH_TTY_OP_VSTOP`
- `XSSH_TTY_OP_VSUSP`
- `XSSH_TTY_OP_VSWTCH`
- `XSSH_TTY_OP_VWERASE`
- `XSSH_TTY_OP_XCASE`

### 类型 (3)

- `xsshchannelpty`
- `xsshterminalmode`
- `xsshterminalmodes`

## `extlibs/xssh/include/xrt/ssh_channel_request.h`

[查看带契约注释的公共头](../../include/xrt/ssh_channel_request.h)

### 函数 (21)

- `xrtSshChannelBreakRead`
- `xrtSshChannelBreakWrite`
- `xrtSshChannelEnvRead`
- `xrtSshChannelEnvWrite`
- `xrtSshChannelExecRead`
- `xrtSshChannelExecWrite`
- `xrtSshChannelExitSignalRead`
- `xrtSshChannelExitSignalWrite`
- `xrtSshChannelExitStatusRead`
- `xrtSshChannelExitStatusWrite`
- `xrtSshChannelShellRead`
- `xrtSshChannelShellWrite`
- `xrtSshChannelSignalRead`
- `xrtSshChannelSignalValid`
- `xrtSshChannelSignalWrite`
- `xrtSshChannelSubsystemRead`
- `xrtSshChannelSubsystemWrite`
- `xrtSshChannelWindowChangeRead`
- `xrtSshChannelWindowChangeWrite`
- `xrtSshChannelXonXoffRead`
- `xrtSshChannelXonXoffWrite`

### 常量与宏 (10)

- `XSSH_CHANNEL_REQUEST_BREAK`
- `XSSH_CHANNEL_REQUEST_ENV`
- `XSSH_CHANNEL_REQUEST_EXEC`
- `XSSH_CHANNEL_REQUEST_EXIT_SIGNAL`
- `XSSH_CHANNEL_REQUEST_EXIT_STATUS`
- `XSSH_CHANNEL_REQUEST_SHELL`
- `XSSH_CHANNEL_REQUEST_SIGNAL`
- `XSSH_CHANNEL_REQUEST_SUBSYSTEM`
- `XSSH_CHANNEL_REQUEST_WINDOW_CHANGE`
- `XSSH_CHANNEL_REQUEST_XON_XOFF`

### 类型 (3)

- `xsshchannelenv`
- `xsshchannelexitsignal`
- `xsshchannelwindowchange`

## `extlibs/xssh/include/xrt/ssh_channel_state.h`

[查看带契约注释的公共头](../../include/xrt/ssh_channel_state.h)

### 函数 (10)

- `xrtSshChannelCanReceiveData`
- `xrtSshChannelCanSendData`
- `xrtSshChannelCanSendRequest`
- `xrtSshChannelCloseReplyNeeded`
- `xrtSshChannelClosed`
- `xrtSshChannelLocalCloseCommit`
- `xrtSshChannelLocalEofCommit`
- `xrtSshChannelRemoteCloseCommit`
- `xrtSshChannelRemoteEofCommit`
- `xrtSshChannelStateInit`

## `extlibs/xssh/include/xrt/ssh_channel_window.h`

[查看带契约注释的公共头](../../include/xrt/ssh_channel_window.h)

### 函数 (10)

- `xrtSshChannelReceiveAdjustCommit`
- `xrtSshChannelReceiveAdjustLimit`
- `xrtSshChannelReceiveAdjustReady`
- `xrtSshChannelReceiveCommit`
- `xrtSshChannelReceiveConsume`
- `xrtSshChannelReceiveGrantCommit`
- `xrtSshChannelSendAdjust`
- `xrtSshChannelSendCommit`
- `xrtSshChannelSendLimit`
- `xrtSshChannelWindowInit`

## `extlibs/xssh/include/xrt/ssh_channels.h`

[查看带契约注释的公共头](../../include/xrt/ssh_channels.h)

### 函数 (16)

- `xrtSshChannelReplyReserve`
- `xrtSshChannelsAccept`
- `xrtSshChannelsClear`
- `xrtSshChannelsConfigInit`
- `xrtSshChannelsConstGet`
- `xrtSshChannelsCount`
- `xrtSshChannelsDiscard`
- `xrtSshChannelsGet`
- `xrtSshChannelsInit`
- `xrtSshChannelsIterBegin`
- `xrtSshChannelsIterEnd`
- `xrtSshChannelsIterNext`
- `xrtSshChannelsOnRemoved`
- `xrtSshChannelsOpen`
- `xrtSshChannelsRemove`
- `xrtSshChannelsResolve`

### 常量与宏 (7)

- `XSSH_CHANNELS_ADJUST_DEFAULT`
- `XSSH_CHANNELS_MAX_DEFAULT`
- `XSSH_CHANNELS_PACKET_DEFAULT`
- `XSSH_CHANNELS_REPLY_LIMIT_DEFAULT`
- `XSSH_CHANNELS_WINDOW_DEFAULT`
- `XSSH_FEATURE_CHANNELS`
- `XSSH_FEATURE_CONNECTION_SESSION`

### 类型 (7)

- `xsshchannel`
- `xsshchannelresolveproc`
- `xsshchannels`
- `xsshchannelsconfig`
- `xsshchannelsiter`
- `xsshchannelsremovedproc`
- `xsshreplyqueue`

## `extlibs/xssh/include/xrt/ssh_client.h`

[查看带契约注释的公共头](../../include/xrt/ssh_client.h)

### 函数 (31)

- `xrtSshClientAbort`
- `xrtSshClientAttach`
- `xrtSshClientBuild`
- `xrtSshClientChannelAccept`
- `xrtSshClientChannelAdjust`
- `xrtSshClientChannelClose`
- `xrtSshClientChannelEof`
- `xrtSshClientChannelFlush`
- `xrtSshClientChannelOpen`
- `xrtSshClientChannelReject`
- `xrtSshClientChannels`
- `xrtSshClientClear`
- `xrtSshClientConfigInit`
- `xrtSshClientContinue`
- `xrtSshClientGlobalReplies`
- `xrtSshClientGlobalReplyReserve`
- `xrtSshClientHostKeyAccept`
- `xrtSshClientHostKeyReject`
- `xrtSshClientInit`
- `xrtSshClientIsCurrent`
- `xrtSshClientNetData`
- `xrtSshClientNetEvents`
- `xrtSshClientOwnsChannel`
- `xrtSshClientPacketAccept`
- `xrtSshClientPacketReject`
- `xrtSshClientPacketRetry`
- `xrtSshClientReader`
- `xrtSshClientSend`
- `xrtSshClientSession`
- `xrtSshClientState`
- `xrtSshClientStream`

### 常量与宏 (26)

- `XSSH_CLIENT_CHANNEL_ACCEPT`
- `XSSH_CLIENT_CHANNEL_EVENT_CLOSED`
- `XSSH_CLIENT_CHANNEL_EVENT_EOF`
- `XSSH_CLIENT_CHANNEL_EVENT_OPENED`
- `XSSH_CLIENT_CHANNEL_EVENT_OPEN_FAILED`
- `XSSH_CLIENT_CHANNEL_EVENT_REQUEST_FAILURE`
- `XSSH_CLIENT_CHANNEL_EVENT_REQUEST_SUCCESS`
- `XSSH_CLIENT_CHANNEL_EVENT_WRITABLE`
- `XSSH_CLIENT_CHANNEL_NONE`
- `XSSH_CLIENT_CHANNEL_REJECT`
- `XSSH_CLIENT_CLOSED`
- `XSSH_CLIENT_CLOSING`
- `XSSH_CLIENT_CONTROL_INITIAL_DEFAULT`
- `XSSH_CLIENT_CONTROL_LIMIT_DEFAULT`
- `XSSH_CLIENT_CREATED`
- `XSSH_CLIENT_GLOBAL_EVENT_REQUEST_FAILURE`
- `XSSH_CLIENT_GLOBAL_EVENT_REQUEST_SUCCESS`
- `XSSH_CLIENT_GLOBAL_REPLY_LIMIT_DEFAULT`
- `XSSH_CLIENT_HANDSHAKE`
- `XSSH_CLIENT_INVALID`
- `XSSH_CLIENT_READY`
- `XSSH_CLIENT_READY_TIMEOUT_DEFAULT`
- `XSSH_FEATURE_CLIENT`
- `XSSH_FEATURE_CLIENT_CORE`
- `XSSH_FEATURE_CLIENT_FUTURE`
- `XSSH_FEATURE_SESSION_STREAM`

### 类型 (19)

- `xsshclient`
- `xsshclientbuildproc`
- `xsshclientchanneldecision`
- `xsshclientchannelevent`
- `xsshclientchannelnotice`
- `xsshclientchannelopenproc`
- `xsshclientconfig`
- `xsshclientcore`
- `xsshclientcoreconfig`
- `xsshclientevents`
- `xsshclientglobalevent`
- `xsshclientglobalnotice`
- `xsshclientstate`
- `xsshrekeydecision`
- `xsshsessionreader`
- `xsshsessionstream`
- `xsshsessionstreamdecision`
- `xsshsessiontcp`
- `xsshsessiontcppacket`

## `extlibs/xssh/include/xrt/ssh_client_auth_ed25519.h`

[查看带契约注释的公共头](../../include/xrt/ssh_client_auth_ed25519.h)

### 函数 (1)

- `xrtSshClientEd25519Auth`

### 常量与宏 (2)

- `XSSH_FEATURE_CLIENT_AUTH_ED25519`
- `XSSH_FEATURE_PRIVATE_KEY_ED25519`

### 类型 (1)

- `xsshclientauth`

## `extlibs/xssh/include/xrt/ssh_client_core.h`

[查看带契约注释的公共头](../../include/xrt/ssh_client_core.h)

### 函数 (9)

- `xrtSshClientCoreAuthMethods`
- `xrtSshClientCoreClear`
- `xrtSshClientCoreConfigInit`
- `xrtSshClientCoreHostKeyAccept`
- `xrtSshClientCoreHostKeyReject`
- `xrtSshClientCoreInit`
- `xrtSshClientCoreNext`
- `xrtSshClientCoreObserve`
- `xrtSshClientPasswordAuth`

### 常量与宏 (17)

- `XSSH_CLIENT_HOST_ACCEPT`
- `XSSH_CLIENT_HOST_DEFER`
- `XSSH_CLIENT_HOST_REJECT`
- `XSSH_CLIENT_NEXT_AUTH`
- `XSSH_CLIENT_NEXT_CLOSING`
- `XSSH_CLIENT_NEXT_HOST_KEY`
- `XSSH_CLIENT_NEXT_IDENTIFICATION`
- `XSSH_CLIENT_NEXT_INPUT`
- `XSSH_CLIENT_NEXT_PAYLOAD`
- `XSSH_CLIENT_NEXT_READY`
- `XSSH_CLIENT_NEXT_TRANSACTION`
- `XSSH_CLIENT_OUTPUT_INITIAL_DEFAULT`
- `XSSH_CLIENT_OUTPUT_LIMIT_DEFAULT`
- `XSSH_CLIENT_VERSION_DEFAULT`
- `XSSH_FEATURE_KEXINIT_RANDOM`
- `XSSH_FEATURE_SESSION_READER`
- `XSSH_FEATURE_SESSION_TCP_RANDOM`

### 类型 (8)

- `xsshclientauthproc`
- `xsshclienthost`
- `xsshclienthostdecision`
- `xsshclienthostproc`
- `xsshclientnext`
- `xsshclientnextkind`
- `xsshkexinitconfig`
- `xsshkexnegotiation`

## `extlibs/xssh/include/xrt/ssh_client_dial.h`

[查看带契约注释的公共头](../../include/xrt/ssh_client_dial.h)

### 函数 (1)

- `xrtSshClientDial`

### 常量与宏 (1)

- `XSSH_FEATURE_CLIENT_DIAL`

## `extlibs/xssh/include/xrt/ssh_client_forward.h`

[查看带契约注释的公共头](../../include/xrt/ssh_client_forward.h)

### 函数 (4)

- `xrtSshClientDirectTcpipOpen`
- `xrtSshClientForwardedTcpipAccept`
- `xrtSshClientTcpipForward`
- `xrtSshClientTcpipForwardCancel`

### 常量与宏 (2)

- `XSSH_FEATURE_CLIENT_FORWARD`
- `XSSH_FEATURE_FORWARD_MESSAGE`

### 类型 (1)

- `xsshtcpipopen`

## `extlibs/xssh/include/xrt/ssh_client_future.h`

[查看带契约注释的公共头](../../include/xrt/ssh_client_future.h)

### 函数 (5)

- `xrtSshClientChannelReadAsync`
- `xrtSshClientChannelReplyAsync`
- `xrtSshClientChannelWaitAsync`
- `xrtSshClientGlobalReplyAsync`
- `xrtSshClientWaitAsync`

### 常量与宏 (7)

- `XSSH_CLIENT_CHANNEL_WAIT_CLOSE`
- `XSSH_CLIENT_CHANNEL_WAIT_EOF`
- `XSSH_CLIENT_CHANNEL_WAIT_OPEN`
- `XSSH_CLIENT_CHANNEL_WAIT_WRITE`
- `XSSH_CLIENT_WAIT_CLOSE`
- `XSSH_CLIENT_WAIT_DRAIN`
- `XSSH_CLIENT_WAIT_READY`

### 类型 (2)

- `xsshclientchannelwait`
- `xsshclientwait`

## `extlibs/xssh/include/xrt/ssh_client_pty.h`

[查看带契约注释的公共头](../../include/xrt/ssh_client_pty.h)

### 函数 (2)

- `xrtSshClientSessionPty`
- `xrtSshClientSessionResize`

### 常量与宏 (2)

- `XSSH_FEATURE_CLIENT_PTY`
- `XSSH_FEATURE_CLIENT_SESSION`

## `extlibs/xssh/include/xrt/ssh_client_session.h`

[查看带契约注释的公共头](../../include/xrt/ssh_client_session.h)

### 函数 (8)

- `xrtSshClientSessionBreak`
- `xrtSshClientSessionEnv`
- `xrtSshClientSessionExec`
- `xrtSshClientSessionOpen`
- `xrtSshClientSessionRequest`
- `xrtSshClientSessionShell`
- `xrtSshClientSessionSignal`
- `xrtSshClientSessionSubsystem`

### 常量与宏 (1)

- `XSSH_CHANNEL_TYPE_SESSION`

## `extlibs/xssh/include/xrt/ssh_connection_message.h`

[查看带契约注释的公共头](../../include/xrt/ssh_connection_message.h)

### 函数 (6)

- `xrtSshGlobalFailureRead`
- `xrtSshGlobalFailureWrite`
- `xrtSshGlobalRequestRead`
- `xrtSshGlobalRequestWrite`
- `xrtSshGlobalSuccessRead`
- `xrtSshGlobalSuccessWrite`

### 常量与宏 (4)

- `XSSH_FEATURE_CONNECTION_MESSAGE`
- `XSSH_MSG_GLOBAL_REQUEST`
- `XSSH_MSG_REQUEST_FAILURE`
- `XSSH_MSG_REQUEST_SUCCESS`

### 类型 (1)

- `xsshglobalrequest`

## `extlibs/xssh/include/xrt/ssh_connection_session.h`

[查看带契约注释的公共头](../../include/xrt/ssh_connection_session.h)

### 函数 (11)

- `xrtSshConnectionSessionActive`
- `xrtSshConnectionSessionBegin`
- `xrtSshConnectionSessionClear`
- `xrtSshConnectionSessionFail`
- `xrtSshConnectionSessionInit`
- `xrtSshConnectionSessionReadAbort`
- `xrtSshConnectionSessionReadCommit`
- `xrtSshConnectionSessionReadPrepare`
- `xrtSshConnectionSessionWriteAbort`
- `xrtSshConnectionSessionWriteCommit`
- `xrtSshConnectionSessionWritePrepare`

### 常量与宏 (19)

- `XSSH_CONNECTION_PACKET_CHANNEL_ADJUST`
- `XSSH_CONNECTION_PACKET_CHANNEL_CLOSE`
- `XSSH_CONNECTION_PACKET_CHANNEL_CONFIRMATION`
- `XSSH_CONNECTION_PACKET_CHANNEL_DATA`
- `XSSH_CONNECTION_PACKET_CHANNEL_EOF`
- `XSSH_CONNECTION_PACKET_CHANNEL_EXTENDED_DATA`
- `XSSH_CONNECTION_PACKET_CHANNEL_FAILURE`
- `XSSH_CONNECTION_PACKET_CHANNEL_OPEN`
- `XSSH_CONNECTION_PACKET_CHANNEL_OPEN_FAILURE`
- `XSSH_CONNECTION_PACKET_CHANNEL_REQUEST`
- `XSSH_CONNECTION_PACKET_CHANNEL_SUCCESS`
- `XSSH_CONNECTION_PACKET_GLOBAL_FAILURE`
- `XSSH_CONNECTION_PACKET_GLOBAL_REQUEST`
- `XSSH_CONNECTION_PACKET_GLOBAL_SUCCESS`
- `XSSH_CONNECTION_PACKET_NONE`
- `XSSH_CONNECTION_QUEUE_NONE`
- `XSSH_CONNECTION_QUEUE_POP`
- `XSSH_CONNECTION_QUEUE_PUSH`
- `XSSH_FEATURE_REPLY_QUEUE`

### 类型 (5)

- `xsshconnectionmessage`
- `xsshconnectionpacket`
- `xsshconnectionpacketkind`
- `xsshconnectionqueueaction`
- `xsshconnectionsession`

## `extlibs/xssh/include/xrt/ssh_fingerprint.h`

[查看带契约注释的公共头](../../include/xrt/ssh_fingerprint.h)

### 函数 (2)

- `xrtSshHostKeyDigestSha256`
- `xrtSshHostKeyFingerprintSha256`

### 常量与宏 (2)

- `XSSH_FEATURE_FINGERPRINT`
- `XSSH_FINGERPRINT_SHA256_SIZE`

## `extlibs/xssh/include/xrt/ssh_forward_message.h`

[查看带契约注释的公共头](../../include/xrt/ssh_forward_message.h)

### 函数 (10)

- `xrtSshDirectTcpipOpenRead`
- `xrtSshDirectTcpipOpenWrite`
- `xrtSshForwardedTcpipOpenRead`
- `xrtSshForwardedTcpipOpenWrite`
- `xrtSshTcpipForwardCancelRead`
- `xrtSshTcpipForwardCancelWrite`
- `xrtSshTcpipForwardRead`
- `xrtSshTcpipForwardSuccessRead`
- `xrtSshTcpipForwardSuccessWrite`
- `xrtSshTcpipForwardWrite`

### 常量与宏 (4)

- `XSSH_CHANNEL_TYPE_DIRECT_TCPIP`
- `XSSH_CHANNEL_TYPE_FORWARDED_TCPIP`
- `XSSH_GLOBAL_REQUEST_CANCEL_TCPIP_FORWARD`
- `XSSH_GLOBAL_REQUEST_TCPIP_FORWARD`

### 类型 (1)

- `xsshtcpipforward`

## `extlibs/xssh/include/xrt/ssh_hostkey.h`

[查看带契约注释的公共头](../../include/xrt/ssh_hostkey.h)

### 函数 (7)

- `xrtSshEd25519PublicKeyRead`
- `xrtSshEd25519PublicKeyWrite`
- `xrtSshEd25519SignatureRead`
- `xrtSshEd25519SignatureWrite`
- `xrtSshPublicKeyRead`
- `xrtSshSignatureRead`
- `xrtSshSignatureWrite`

### 常量与宏 (3)

- `XSSH_ED25519_PUBLIC_SIZE`
- `XSSH_ED25519_SIGNATURE_SIZE`
- `XSSH_HOSTKEY_ED25519`

### 类型 (2)

- `xsshpublickey`
- `xsshsignature`

## `extlibs/xssh/include/xrt/ssh_hostkey_ed25519.h`

[查看带契约注释的公共头](../../include/xrt/ssh_hostkey_ed25519.h)

### 函数 (1)

- `xrtSshEd25519HostKeyVerify`

### 常量与宏 (1)

- `XSSH_FEATURE_HOSTKEY_ED25519`

## `extlibs/xssh/include/xrt/ssh_kex_curve25519.h`

[查看带契约注释的公共头](../../include/xrt/ssh_kex_curve25519.h)

### 函数 (2)

- `xrtSshCurve25519Public`
- `xrtSshCurve25519Shared`

### 常量与宏 (4)

- `XSSH_CURVE25519_PRIVATE_SIZE`
- `XSSH_CURVE25519_PUBLIC_SIZE`
- `XSSH_CURVE25519_SHARED_SIZE`
- `XSSH_FEATURE_KEX_CURVE25519`

## `extlibs/xssh/include/xrt/ssh_kex_curve25519_random.h`

[查看带契约注释的公共头](../../include/xrt/ssh_kex_curve25519_random.h)

### 函数 (1)

- `xrtSshCurve25519KeyPair`

### 常量与宏 (1)

- `XSSH_FEATURE_KEX_CURVE25519_RANDOM`

## `extlibs/xssh/include/xrt/ssh_kex_ecdh.h`

[查看带契约注释的公共头](../../include/xrt/ssh_kex_ecdh.h)

### 函数 (4)

- `xrtSshEcdhInitRead`
- `xrtSshEcdhInitWrite`
- `xrtSshEcdhReplyRead`
- `xrtSshEcdhReplyWrite`

### 常量与宏 (3)

- `XSSH_FEATURE_KEX_ECDH`
- `XSSH_MSG_KEX_ECDH_INIT`
- `XSSH_MSG_KEX_ECDH_REPLY`

### 类型 (2)

- `xsshecdhinit`
- `xsshecdhreply`

## `extlibs/xssh/include/xrt/ssh_kex_exchange.h`

[查看带契约注释的公共头](../../include/xrt/ssh_kex_exchange.h)

### 函数 (15)

- `xrtSshKexExchangeBeginWithPrivate`
- `xrtSshKexExchangeClear`
- `xrtSshKexExchangeComplete`
- `xrtSshKexExchangeFail`
- `xrtSshKexExchangeInit`
- `xrtSshKexExchangeKexInitAbort`
- `xrtSshKexExchangeKexInitCommit`
- `xrtSshKexExchangeKexInitPrepare`
- `xrtSshKexExchangeReady`
- `xrtSshKexExchangeSession`
- `xrtSshKexExchangeSessionConst`
- `xrtSshKexExchangeTranscript`
- `xrtSshKexExchangeVersionAbort`
- `xrtSshKexExchangeVersionCommit`
- `xrtSshKexExchangeVersionPrepare`

### 常量与宏 (11)

- `XSSH_FEATURE_KEX_EXCHANGE`
- `XSSH_FEATURE_KEX_SESSION`
- `XSSH_KEX_EXCHANGE_COMPLETE`
- `XSSH_KEX_EXCHANGE_FAILED`
- `XSSH_KEX_EXCHANGE_IDENTIFICATION`
- `XSSH_KEX_EXCHANGE_KEXINIT`
- `XSSH_KEX_EXCHANGE_METHOD`
- `XSSH_KEX_EXCHANGE_PENDING_KEXINIT`
- `XSSH_KEX_EXCHANGE_PENDING_NONE`
- `XSSH_KEX_EXCHANGE_PENDING_VERSION`
- `XSSH_KEX_EXCHANGE_READY`

### 类型 (7)

- `xsshkexexchange`
- `xsshkexexchangepending`
- `xsshkexexchangephase`
- `xsshkexsession`
- `xsshkextranscript`
- `xsshtransportdirection`
- `xsshtransportphase`

## `extlibs/xssh/include/xrt/ssh_kex_exchange_random.h`

[查看带契约注释的公共头](../../include/xrt/ssh_kex_exchange_random.h)

### 函数 (1)

- `xrtSshKexExchangeBegin`

### 常量与宏 (1)

- `XSSH_FEATURE_KEX_EXCHANGE_RANDOM`

## `extlibs/xssh/include/xrt/ssh_kex_session.h`

[查看带契约注释的公共头](../../include/xrt/ssh_kex_session.h)

### 函数 (24)

- `xrtSshKexSessionActivateRead`
- `xrtSshKexSessionActivateWrite`
- `xrtSshKexSessionBeginWithPrivate`
- `xrtSshKexSessionClear`
- `xrtSshKexSessionComplete`
- `xrtSshKexSessionEcdhInitPrepare`
- `xrtSshKexSessionEcdhReplyPrepare`
- `xrtSshKexSessionEvent`
- `xrtSshKexSessionExchangeHash`
- `xrtSshKexSessionFail`
- `xrtSshKexSessionHostKey`
- `xrtSshKexSessionHostKeyAccept`
- `xrtSshKexSessionId`
- `xrtSshKexSessionInit`
- `xrtSshKexSessionNegotiation`
- `xrtSshKexSessionNewKeysPrepare`
- `xrtSshKexSessionReadAbort`
- `xrtSshKexSessionReadCommit`
- `xrtSshKexSessionReadPrepare`
- `xrtSshKexSessionWriteAbort`
- `xrtSshKexSessionWriteCommit`
- `xrtSshKexTranscriptInit`
- `xrtSshKexTranscriptMeasure`
- `xrtSshKexTranscriptWrite`

### 常量与宏 (27)

- `XSSH_AES_GCM_IV_SIZE`
- `XSSH_FEATURE_KEX_SHA256`
- `XSSH_KEX_EVENT_ACTIVATE_READ`
- `XSSH_KEX_EVENT_ACTIVATE_WRITE`
- `XSSH_KEX_EVENT_COMPLETE`
- `XSSH_KEX_EVENT_FAILED`
- `XSSH_KEX_EVENT_NONE`
- `XSSH_KEX_EVENT_READ_ECDH_INIT`
- `XSSH_KEX_EVENT_READ_ECDH_REPLY`
- `XSSH_KEX_EVENT_READ_NEWKEYS`
- `XSSH_KEX_EVENT_VERIFY_HOST_KEY`
- `XSSH_KEX_EVENT_WRITE_ECDH_INIT`
- `XSSH_KEX_EVENT_WRITE_ECDH_REPLY`
- `XSSH_KEX_EVENT_WRITE_NEWKEYS`
- `XSSH_KEX_PACKET_DISCARD`
- `XSSH_KEX_PACKET_ECDH_INIT`
- `XSSH_KEX_PACKET_ECDH_REPLY`
- `XSSH_KEX_PACKET_NEWKEYS`
- `XSSH_KEX_PACKET_NONE`
- `XSSH_KEX_SESSION_COMPLETE`
- `XSSH_KEX_SESSION_FAILED`
- `XSSH_KEX_SESSION_HOST_KEY`
- `XSSH_KEX_SESSION_IDLE`
- `XSSH_KEX_SESSION_KEY_MAX`
- `XSSH_KEX_SESSION_METHOD`
- `XSSH_KEX_SESSION_NEW_KEYS`
- `XSSH_SHA256_SIZE`

### 类型 (3)

- `xsshkexsessionevent`
- `xsshkexsessionpacket`
- `xsshkexsessionphase`

## `extlibs/xssh/include/xrt/ssh_kex_session_random.h`

[查看带契约注释的公共头](../../include/xrt/ssh_kex_session_random.h)

### 函数 (1)

- `xrtSshKexSessionBegin`

### 常量与宏 (1)

- `XSSH_FEATURE_KEX_SESSION_RANDOM`

## `extlibs/xssh/include/xrt/ssh_kex_sha256.h`

[查看带契约注释的公共头](../../include/xrt/ssh_kex_sha256.h)

### 函数 (2)

- `xrtSshKexDeriveSha256`
- `xrtSshKexHashSha256`

### 类型 (1)

- `xsshkexhashsha256`

## `extlibs/xssh/include/xrt/ssh_kexinit.h`

[查看带契约注释的公共头](../../include/xrt/ssh_kexinit.h)

### 函数 (7)

- `xrtSshCipherIsAead`
- `xrtSshKexFeatures`
- `xrtSshKexGuessSkip`
- `xrtSshKexInitConfigInit`
- `xrtSshKexInitRead`
- `xrtSshKexInitWrite`
- `xrtSshKexNegotiate`

### 常量与宏 (18)

- `XSSH_CIPHER_DEFAULT`
- `XSSH_COMPRESSION_DEFAULT`
- `XSSH_FEATURE_KEXINIT`
- `XSSH_HOSTKEY_DEFAULT`
- `XSSH_KEX_CLIENT_INITIAL_DEFAULT`
- `XSSH_KEX_COOKIE_SIZE`
- `XSSH_KEX_DEFAULT`
- `XSSH_KEX_EXT_INFO_CLIENT`
- `XSSH_KEX_EXT_INFO_SERVER`
- `XSSH_KEX_SERVER_INITIAL_DEFAULT`
- `XSSH_KEX_STRICT_CLIENT`
- `XSSH_KEX_STRICT_CLIENT_PRE_STANDARD`
- `XSSH_KEX_STRICT_SERVER`
- `XSSH_KEX_STRICT_SERVER_PRE_STANDARD`
- `XSSH_MAC_DEFAULT`
- `XSSH_MSG_KEXINIT`
- `XSSH_ROLE_CLIENT`
- `XSSH_ROLE_SERVER`

### 类型 (2)

- `xsshkexfeatures`
- `xsshkexinit`

## `extlibs/xssh/include/xrt/ssh_kexinit_random.h`

[查看带契约注释的公共头](../../include/xrt/ssh_kexinit_random.h)

### 函数 (1)

- `xrtSshKexInitWriteSecure`

## `extlibs/xssh/include/xrt/ssh_key_text.h`

[查看带契约注释的公共头](../../include/xrt/ssh_key_text.h)

### 函数 (3)

- `xrtSshPublicKeyLineDecode`
- `xrtSshPublicKeyLineMatch`
- `xrtSshPublicKeyLineRead`

### 常量与宏 (1)

- `XSSH_FEATURE_KEY_TEXT`

### 类型 (1)

- `xsshopensshkeyline`

## `extlibs/xssh/include/xrt/ssh_known_host.h`

[查看带契约注释的公共头](../../include/xrt/ssh_known_host.h)

### 函数 (5)

- `xrtSshKnownHostLineDecode`
- `xrtSshKnownHostLineKeyMatch`
- `xrtSshKnownHostLineMatch`
- `xrtSshKnownHostLineRead`
- `xrtSshKnownHostPatternsMatch`

### 常量与宏 (11)

- `XSSH_DEFAULT_PORT`
- `XSSH_FEATURE_KNOWN_HOST`
- `XSSH_KNOWN_HOST_CERT_AUTHORITY`
- `XSSH_KNOWN_HOST_MARKER_CERT_AUTHORITY`
- `XSSH_KNOWN_HOST_MARKER_NONE`
- `XSSH_KNOWN_HOST_MARKER_REVOKED`
- `XSSH_KNOWN_HOST_MARKER_UNKNOWN`
- `XSSH_KNOWN_HOST_MATCH`
- `XSSH_KNOWN_HOST_NEGATED`
- `XSSH_KNOWN_HOST_NO_MATCH`
- `XSSH_KNOWN_HOST_REVOKED`

### 类型 (3)

- `xsshknownhostline`
- `xsshknownhostmarker`
- `xsshknownhostmatch`

## `extlibs/xssh/include/xrt/ssh_known_host_db.h`

[查看带契约注释的公共头](../../include/xrt/ssh_known_host_db.h)

### 函数 (3)

- `xrtSshKnownHostDbCheck`
- `xrtSshKnownHostDbInit`
- `xrtSshKnownHostDbNext`

### 常量与宏 (10)

- `XSSH_FEATURE_KNOWN_HOST_DB`
- `XSSH_FEATURE_KNOWN_HOST_HASH`
- `XSSH_KNOWN_HOST_DB_STRICT`
- `XSSH_KNOWN_HOST_TRUST_CERT_AUTHORITY`
- `XSSH_KNOWN_HOST_TRUST_CHANGED`
- `XSSH_KNOWN_HOST_TRUST_INVALID`
- `XSSH_KNOWN_HOST_TRUST_MATCH`
- `XSSH_KNOWN_HOST_TRUST_NEW`
- `XSSH_KNOWN_HOST_TRUST_REVOKED`
- `XSSH_NEED_MORE`

### 类型 (5)

- `xsshknownhostcheck`
- `xsshknownhostdb`
- `xsshknownhostdbflag`
- `xsshknownhostentry`
- `xsshknownhosttrust`

## `extlibs/xssh/include/xrt/ssh_known_host_hash.h`

[查看带契约注释的公共头](../../include/xrt/ssh_known_host_hash.h)

### 函数 (4)

- `xrtSshKnownHostHash`
- `xrtSshKnownHostHashMatch`
- `xrtSshKnownHostHashWrite`
- `xrtSshKnownHostLineHashMatch`

### 常量与宏 (1)

- `XSSH_KNOWN_HOST_HASH_SIZE`

## `extlibs/xssh/include/xrt/ssh_packet.h`

[查看带契约注释的公共头](../../include/xrt/ssh_packet.h)

### 函数 (3)

- `xrtSshPacketMeasure`
- `xrtSshPacketRead`
- `xrtSshPacketWrite`

### 常量与宏 (5)

- `XSSH_FEATURE_PACKET`
- `XSSH_PACKET_BLOCK_MIN`
- `XSSH_PACKET_MAX_DEFAULT`
- `XSSH_PACKET_PADDING_MAX`
- `XSSH_PACKET_PADDING_MIN`

### 类型 (2)

- `xsshpacketview`
- `xsshpaddingproc`

## `extlibs/xssh/include/xrt/ssh_packet_aes_gcm.h`

[查看带契约注释的公共头](../../include/xrt/ssh_packet_aes_gcm.h)

### 函数 (6)

- `xrtSshAesGcmClear`
- `xrtSshAesGcmInit`
- `xrtSshAesGcmInvocation`
- `xrtSshAesGcmMeasure`
- `xrtSshAesGcmRead`
- `xrtSshAesGcmWrite`

### 常量与宏 (4)

- `XSSH_AES_GCM_BLOCK_SIZE`
- `XSSH_AES_GCM_FIXED_IV_SIZE`
- `XSSH_AES_GCM_TAG_SIZE`
- `XSSH_FEATURE_PACKET_AES_GCM`

### 类型 (1)

- `xsshaesgcm`

## `extlibs/xssh/include/xrt/ssh_packet_codec.h`

[查看带契约注释的公共头](../../include/xrt/ssh_packet_codec.h)

### 函数 (13)

- `xrtSshPacketCodecClear`
- `xrtSshPacketCodecInit`
- `xrtSshPacketCodecInspect`
- `xrtSshPacketCodecRead`
- `xrtSshPacketCodecResetReadSequence`
- `xrtSshPacketCodecResetWriteSequence`
- `xrtSshPacketCodecSetReadAesGcm`
- `xrtSshPacketCodecSetWriteAesGcm`
- `xrtSshPacketCodecWriteAbort`
- `xrtSshPacketCodecWriteCommit`
- `xrtSshPacketCodecWriteMeasure`
- `xrtSshPacketCodecWritePrepareWithPadding`
- `xrtSshPacketCodecWriteWithPadding`

### 常量与宏 (3)

- `XSSH_FEATURE_PACKET_CODEC`
- `XSSH_PACKET_MODE_AES_GCM`
- `XSSH_PACKET_MODE_PLAIN`

### 类型 (3)

- `xsshpacketcodec`
- `xsshpacketmode`
- `xsshpacketneed`

## `extlibs/xssh/include/xrt/ssh_packet_codec_random.h`

[查看带契约注释的公共头](../../include/xrt/ssh_packet_codec_random.h)

### 函数 (2)

- `xrtSshPacketCodecWrite`
- `xrtSshPacketCodecWritePrepare`

### 常量与宏 (2)

- `XSSH_FEATURE_PACKET_CODEC_RANDOM`
- `XSSH_FEATURE_PACKET_RANDOM`

## `extlibs/xssh/include/xrt/ssh_packet_random.h`

[查看带契约注释的公共头](../../include/xrt/ssh_packet_random.h)

### 函数 (2)

- `xrtSshPacketWriteSecure`
- `xrtSshSecurePadding`

## `extlibs/xssh/include/xrt/ssh_private_key.h`

[查看带契约注释的公共头](../../include/xrt/ssh_private_key.h)

### 函数 (4)

- `xrtSshPrivateKeyIsEncrypted`
- `xrtSshPrivateKeyPublicsInit`
- `xrtSshPrivateKeyPublicsNext`
- `xrtSshPrivateKeyRead`

### 常量与宏 (4)

- `XSSH_FEATURE_PRIVATE_KEY`
- `XSSH_PRIVATE_KEY_MAGIC`
- `XSSH_PRIVATE_KEY_MAGIC_SIZE`
- `XSSH_PRIVATE_KEY_NONE`

### 类型 (2)

- `xsshopensshprivatekey`
- `xsshprivatekeypublics`

## `extlibs/xssh/include/xrt/ssh_private_key_ed25519.h`

[查看带契约注释的公共头](../../include/xrt/ssh_private_key_ed25519.h)

### 函数 (3)

- `xrtSshPrivateKeyEd25519Read`
- `xrtSshPrivateKeyEd25519Sign`
- `xrtSshPrivateKeyEd25519SignatureWrite`

### 类型 (1)

- `xsshed25519identity`

## `extlibs/xssh/include/xrt/ssh_private_key_pem.h`

[查看带契约注释的公共头](../../include/xrt/ssh_private_key_pem.h)

### 函数 (1)

- `xrtSshPrivateKeyPemRead`

### 常量与宏 (2)

- `XSSH_FEATURE_PRIVATE_KEY_PEM`
- `XSSH_PRIVATE_KEY_PEM_LABEL`

## `extlibs/xssh/include/xrt/ssh_reply_queue.h`

[查看带契约注释的公共头](../../include/xrt/ssh_reply_queue.h)

### 函数 (6)

- `xrtSshReplyQueueCount`
- `xrtSshReplyQueueFront`
- `xrtSshReplyQueueInit`
- `xrtSshReplyQueuePop`
- `xrtSshReplyQueuePush`
- `xrtSshReplyQueueRebind`

## `extlibs/xssh/include/xrt/ssh_session_core.h`

[查看带契约注释的公共头](../../include/xrt/ssh_session_core.h)

### 函数 (23)

- `xrtSshSessionCoreAction`
- `xrtSshSessionCoreAuth`
- `xrtSshSessionCoreAuthBegin`
- `xrtSshSessionCoreAuthConst`
- `xrtSshSessionCoreClear`
- `xrtSshSessionCoreConnection`
- `xrtSshSessionCoreConnectionConst`
- `xrtSshSessionCoreFail`
- `xrtSshSessionCoreInit`
- `xrtSshSessionCoreKex`
- `xrtSshSessionCoreKexBeginWithPrivate`
- `xrtSshSessionCoreKexConst`
- `xrtSshSessionCorePhase`
- `xrtSshSessionCoreReadAbort`
- `xrtSshSessionCoreReadCommit`
- `xrtSshSessionCoreReadPrepare`
- `xrtSshSessionCoreVersionAbort`
- `xrtSshSessionCoreVersionCommit`
- `xrtSshSessionCoreVersionPrepare`
- `xrtSshSessionCoreWriteAbort`
- `xrtSshSessionCoreWriteBind`
- `xrtSshSessionCoreWriteCommit`
- `xrtSshSessionCoreWritePrepare`

### 常量与宏 (51)

- `XSSH_FEATURE_SESSION_CORE`
- `XSSH_SESSION_ACTION_ACTIVATE_READ_KEYS`
- `XSSH_SESSION_ACTION_ACTIVATE_WRITE_KEYS`
- `XSSH_SESSION_ACTION_BEGIN_AUTH`
- `XSSH_SESSION_ACTION_BEGIN_KEX`
- `XSSH_SESSION_ACTION_CLOSING`
- `XSSH_SESSION_ACTION_COMPLETE_AUTH`
- `XSSH_SESSION_ACTION_COMPLETE_KEX`
- `XSSH_SESSION_ACTION_CONNECTION`
- `XSSH_SESSION_ACTION_FAILED`
- `XSSH_SESSION_ACTION_NONE`
- `XSSH_SESSION_ACTION_READ_AUTH_REQUEST`
- `XSSH_SESSION_ACTION_READ_AUTH_RESULT`
- `XSSH_SESSION_ACTION_READ_ECDH_INIT`
- `XSSH_SESSION_ACTION_READ_ECDH_REPLY`
- `XSSH_SESSION_ACTION_READ_IDENTIFICATION`
- `XSSH_SESSION_ACTION_READ_KEXINIT`
- `XSSH_SESSION_ACTION_READ_NEWKEYS`
- `XSSH_SESSION_ACTION_READ_PENDING`
- `XSSH_SESSION_ACTION_READ_SERVICE_ACCEPT`
- `XSSH_SESSION_ACTION_READ_SERVICE_REQUEST`
- `XSSH_SESSION_ACTION_VERIFY_HOST_KEY`
- `XSSH_SESSION_ACTION_WRITE_AUTH_REQUEST`
- `XSSH_SESSION_ACTION_WRITE_AUTH_RESULT`
- `XSSH_SESSION_ACTION_WRITE_ECDH_INIT`
- `XSSH_SESSION_ACTION_WRITE_ECDH_REPLY`
- `XSSH_SESSION_ACTION_WRITE_IDENTIFICATION`
- `XSSH_SESSION_ACTION_WRITE_KEXINIT`
- `XSSH_SESSION_ACTION_WRITE_NEWKEYS`
- `XSSH_SESSION_ACTION_WRITE_PENDING`
- `XSSH_SESSION_ACTION_WRITE_SERVICE_ACCEPT`
- `XSSH_SESSION_ACTION_WRITE_SERVICE_REQUEST`
- `XSSH_SESSION_AUTHENTICATION`
- `XSSH_SESSION_CLOSING`
- `XSSH_SESSION_CONNECTION`
- `XSSH_SESSION_FAILED`
- `XSSH_SESSION_IDENTIFICATION`
- `XSSH_SESSION_KEY_EXCHANGE`
- `XSSH_SESSION_PACKET_AUTH`
- `XSSH_SESSION_PACKET_CONNECTION`
- `XSSH_SESSION_PACKET_DEBUG`
- `XSSH_SESSION_PACKET_DISCONNECT`
- `XSSH_SESSION_PACKET_EXTENSION`
- `XSSH_SESSION_PACKET_EXT_INFO`
- `XSSH_SESSION_PACKET_IGNORE`
- `XSSH_SESSION_PACKET_KEX`
- `XSSH_SESSION_PACKET_KEXINIT`
- `XSSH_SESSION_PACKET_NEWCOMPRESS`
- `XSSH_SESSION_PACKET_NONE`
- `XSSH_SESSION_PACKET_UNIMPLEMENTED`
- `XSSH_SESSION_REKEY`

### 类型 (10)

- `xsshdebug`
- `xsshdisconnect`
- `xsshextinfo`
- `xsshignore`
- `xsshsessionaction`
- `xsshsessioncore`
- `xsshsessionmessage`
- `xsshsessionpacket`
- `xsshsessionpacketkind`
- `xsshsessionphase`

## `extlibs/xssh/include/xrt/ssh_session_core_random.h`

[查看带契约注释的公共头](../../include/xrt/ssh_session_core_random.h)

### 函数 (1)

- `xrtSshSessionCoreKexBegin`

### 常量与宏 (1)

- `XSSH_FEATURE_SESSION_CORE_RANDOM`

## `extlibs/xssh/include/xrt/ssh_session_reader.h`

[查看带契约注释的公共头](../../include/xrt/ssh_session_reader.h)

### 函数 (9)

- `xrtSshSessionReaderAbort`
- `xrtSshSessionReaderClear`
- `xrtSshSessionReaderCommit`
- `xrtSshSessionReaderHostKey`
- `xrtSshSessionReaderInit`
- `xrtSshSessionReaderPrepare`
- `xrtSshSessionReaderSession`
- `xrtSshSessionReaderSessionConst`
- `xrtSshSessionReaderState`

### 常量与宏 (6)

- `XSSH_FEATURE_SESSION_TCP`
- `XSSH_SESSION_READER_HOST_KEY`
- `XSSH_SESSION_READER_IDLE`
- `XSSH_SESSION_READER_INVALID`
- `XSSH_SESSION_READER_READY`
- `XSSH_SESSION_READER_RETRY`

### 类型 (1)

- `xsshsessionreaderstate`

## `extlibs/xssh/include/xrt/ssh_session_stream.h`

[查看带契约注释的公共头](../../include/xrt/ssh_session_stream.h)

### 函数 (14)

- `xrtSshSessionStreamAbort`
- `xrtSshSessionStreamAccept`
- `xrtSshSessionStreamAttach`
- `xrtSshSessionStreamClear`
- `xrtSshSessionStreamDrive`
- `xrtSshSessionStreamInit`
- `xrtSshSessionStreamNetEvents`
- `xrtSshSessionStreamPacket`
- `xrtSshSessionStreamReader`
- `xrtSshSessionStreamReject`
- `xrtSshSessionStreamSession`
- `xrtSshSessionStreamState`
- `xrtSshSessionStreamTcp`
- `xrtSshSessionStreamVersion`

### 常量与宏 (11)

- `XSSH_SESSION_STREAM_ABORT`
- `XSSH_SESSION_STREAM_ACCEPT`
- `XSSH_SESSION_STREAM_CLOSED`
- `XSSH_SESSION_STREAM_CLOSING`
- `XSSH_SESSION_STREAM_CREATED`
- `XSSH_SESSION_STREAM_HOLD`
- `XSSH_SESSION_STREAM_HOLD_IDENTIFICATION`
- `XSSH_SESSION_STREAM_HOLD_PACKET`
- `XSSH_SESSION_STREAM_INVALID`
- `XSSH_SESSION_STREAM_OPEN`
- `XSSH_SESSION_STREAM_RETRY`

### 类型 (3)

- `xsshsessionstreamevents`
- `xsshsessionstreamstate`
- `xsshsessiontcpconfig`

## `extlibs/xssh/include/xrt/ssh_session_tcp.h`

[查看带契约注释的公共头](../../include/xrt/ssh_session_tcp.h)

### 函数 (21)

- `xrtSshSessionTcpAction`
- `xrtSshSessionTcpAuthBegin`
- `xrtSshSessionTcpClear`
- `xrtSshSessionTcpConfigInit`
- `xrtSshSessionTcpCore`
- `xrtSshSessionTcpCoreConst`
- `xrtSshSessionTcpIdentificationReadPrepare`
- `xrtSshSessionTcpIdentificationWritePrepare`
- `xrtSshSessionTcpInit`
- `xrtSshSessionTcpKexBeginWithPrivate`
- `xrtSshSessionTcpPhase`
- `xrtSshSessionTcpReadAbort`
- `xrtSshSessionTcpReadCommit`
- `xrtSshSessionTcpReadInspect`
- `xrtSshSessionTcpReadPrepare`
- `xrtSshSessionTcpTransport`
- `xrtSshSessionTcpTransportConst`
- `xrtSshSessionTcpWriteAbort`
- `xrtSshSessionTcpWritePrepareWithPadding`
- `xrtSshSessionTcpWriteSize`
- `xrtSshSessionTcpWriteSubmit`

### 常量与宏 (1)

- `XSSH_FEATURE_TRANSPORT_TCP`

### 类型 (2)

- `xsshtransporttcp`
- `xsshtransporttcpconfig`

## `extlibs/xssh/include/xrt/ssh_session_tcp_random.h`

[查看带契约注释的公共头](../../include/xrt/ssh_session_tcp_random.h)

### 函数 (2)

- `xrtSshSessionTcpKexBegin`
- `xrtSshSessionTcpWritePrepare`

### 常量与宏 (1)

- `XSSH_FEATURE_TRANSPORT_TCP_RANDOM`

## `extlibs/xssh/include/xrt/ssh_transport_core.h`

[查看带契约注释的公共头](../../include/xrt/ssh_transport_core.h)

### 函数 (21)

- `xrtSshTransportCoreCanApplication`
- `xrtSshTransportCoreClear`
- `xrtSshTransportCoreClose`
- `xrtSshTransportCoreIdentificationCommit`
- `xrtSshTransportCoreInit`
- `xrtSshTransportCoreInspect`
- `xrtSshTransportCoreKexComplete`
- `xrtSshTransportCoreKexConfigure`
- `xrtSshTransportCoreKexReplyNeeded`
- `xrtSshTransportCoreReadAbort`
- `xrtSshTransportCoreReadCommit`
- `xrtSshTransportCoreReadKeysPending`
- `xrtSshTransportCoreReadPrepare`
- `xrtSshTransportCoreRekeyCheck`
- `xrtSshTransportCoreRekeyRequest`
- `xrtSshTransportCoreSetReadAesGcm`
- `xrtSshTransportCoreSetWriteAesGcm`
- `xrtSshTransportCoreWriteAbort`
- `xrtSshTransportCoreWriteCommit`
- `xrtSshTransportCoreWriteKeysPending`
- `xrtSshTransportCoreWritePrepareWithPadding`

### 常量与宏 (6)

- `XSSH_FEATURE_TRANSPORT_REKEY`
- `XSSH_FEATURE_TRANSPORT_STATE`
- `XSSH_TRANSPORT_PACKET_AUTH_SUCCESS`
- `XSSH_TRANSPORT_PACKET_KEXINIT`
- `XSSH_TRANSPORT_PACKET_MESSAGE`
- `XSSH_TRANSPORT_PACKET_NEWKEYS`

### 类型 (6)

- `xsshrekeypolicy`
- `xsshrekeystate`
- `xsshtransportkexrules`
- `xsshtransportpacketkind`
- `xsshtransportpending`
- `xsshtransportstate`

## `extlibs/xssh/include/xrt/ssh_transport_message.h`

[查看带契约注释的公共头](../../include/xrt/ssh_transport_message.h)

### 函数 (20)

- `xrtSshDebugRead`
- `xrtSshDebugWrite`
- `xrtSshDisconnectRead`
- `xrtSshDisconnectWrite`
- `xrtSshExtInfoNext`
- `xrtSshExtInfoRead`
- `xrtSshExtInfoWrite`
- `xrtSshIgnoreRead`
- `xrtSshIgnoreWrite`
- `xrtSshMessageType`
- `xrtSshNewCompressRead`
- `xrtSshNewCompressWrite`
- `xrtSshNewKeysRead`
- `xrtSshNewKeysWrite`
- `xrtSshServiceAcceptRead`
- `xrtSshServiceAcceptWrite`
- `xrtSshServiceRequestRead`
- `xrtSshServiceRequestWrite`
- `xrtSshUnimplementedRead`
- `xrtSshUnimplementedWrite`

### 常量与宏 (25)

- `XSSH_DISCONNECT_AUTH_CANCELLED_BY_USER`
- `XSSH_DISCONNECT_BY_APPLICATION`
- `XSSH_DISCONNECT_COMPRESSION_ERROR`
- `XSSH_DISCONNECT_CONNECTION_LOST`
- `XSSH_DISCONNECT_HOST_KEY_NOT_VERIFIABLE`
- `XSSH_DISCONNECT_HOST_NOT_ALLOWED_TO_CONNECT`
- `XSSH_DISCONNECT_ILLEGAL_USER_NAME`
- `XSSH_DISCONNECT_KEY_EXCHANGE_FAILED`
- `XSSH_DISCONNECT_MAC_ERROR`
- `XSSH_DISCONNECT_NO_MORE_AUTH_METHODS_AVAILABLE`
- `XSSH_DISCONNECT_PROTOCOL_ERROR`
- `XSSH_DISCONNECT_PROTOCOL_VERSION_NOT_SUPPORTED`
- `XSSH_DISCONNECT_RESERVED`
- `XSSH_DISCONNECT_SERVICE_NOT_AVAILABLE`
- `XSSH_DISCONNECT_TOO_MANY_CONNECTIONS`
- `XSSH_FEATURE_TRANSPORT_MESSAGE`
- `XSSH_MSG_DEBUG`
- `XSSH_MSG_DISCONNECT`
- `XSSH_MSG_EXT_INFO`
- `XSSH_MSG_IGNORE`
- `XSSH_MSG_NEWCOMPRESS`
- `XSSH_MSG_NEWKEYS`
- `XSSH_MSG_SERVICE_ACCEPT`
- `XSSH_MSG_SERVICE_REQUEST`
- `XSSH_MSG_UNIMPLEMENTED`

### 类型 (3)

- `xsshdisconnectreason`
- `xsshextension`
- `xsshservice`

## `extlibs/xssh/include/xrt/ssh_transport_rekey.h`

[查看带契约注释的公共头](../../include/xrt/ssh_transport_rekey.h)

### 函数 (10)

- `xrtSshRekeyCheck`
- `xrtSshRekeyComplete`
- `xrtSshRekeyInit`
- `xrtSshRekeyPolicyInit`
- `xrtSshRekeyRequest`
- `xrtSshRekeyReserveReceive`
- `xrtSshRekeyReserveSend`
- `xrtSshRekeyReset`
- `xrtSshRekeyResetReceive`
- `xrtSshRekeyResetSend`

### 常量与宏 (9)

- `XSSH_REKEY_DEFAULT_BLOCK_LIMIT`
- `XSSH_REKEY_DEFAULT_BYTE_LIMIT`
- `XSSH_REKEY_DEFAULT_RECEIVE_PACKET_LIMIT`
- `XSSH_REKEY_DEFAULT_SEND_PACKET_LIMIT`
- `XSSH_REKEY_DEFAULT_TIME_LIMIT_MS`
- `XSSH_REKEY_HARD_PACKET_LIMIT`
- `XSSH_REKEY_NONE`
- `XSSH_REKEY_RECOMMENDED`
- `XSSH_REKEY_REQUIRED`

### 类型 (1)

- `xsshrekeycounter`

## `extlibs/xssh/include/xrt/ssh_transport_state.h`

[查看带契约注释的公共头](../../include/xrt/ssh_transport_state.h)

### 函数 (17)

- `xrtSshTransportAuthSuccessCheck`
- `xrtSshTransportAuthSuccessCommit`
- `xrtSshTransportCanApplication`
- `xrtSshTransportClose`
- `xrtSshTransportIdentificationCommit`
- `xrtSshTransportKexConfigure`
- `xrtSshTransportKexInitCheck`
- `xrtSshTransportKexInitCommit`
- `xrtSshTransportKexReplyNeeded`
- `xrtSshTransportKexRuleSet`
- `xrtSshTransportKexRulesInit`
- `xrtSshTransportMessageCheck`
- `xrtSshTransportMessageCommit`
- `xrtSshTransportNewKeysCheck`
- `xrtSshTransportNewKeysCommit`
- `xrtSshTransportStateClear`
- `xrtSshTransportStateInit`

### 常量与宏 (14)

- `XSSH_KEX_METHOD_COUNT`
- `XSSH_KEX_METHOD_MAX`
- `XSSH_KEX_METHOD_MIN`
- `XSSH_TRANSPORT_ACTION_ACTIVATE_KEYS`
- `XSSH_TRANSPORT_ACTION_KEX_COMPLETE`
- `XSSH_TRANSPORT_ACTION_NONE`
- `XSSH_TRANSPORT_ACTION_RESET_SEQUENCE`
- `XSSH_TRANSPORT_CLOSED`
- `XSSH_TRANSPORT_CLOSING`
- `XSSH_TRANSPORT_IDENTIFICATION`
- `XSSH_TRANSPORT_KEY_EXCHANGE`
- `XSSH_TRANSPORT_LOCAL`
- `XSSH_TRANSPORT_OPEN`
- `XSSH_TRANSPORT_PEER`

### 类型 (1)

- `xsshtransportaction`

## `extlibs/xssh/include/xrt/ssh_transport_tcp.h`

[查看带契约注释的公共头](../../include/xrt/ssh_transport_tcp.h)

### 函数 (15)

- `xrtSshTransportTcpClear`
- `xrtSshTransportTcpConfigInit`
- `xrtSshTransportTcpCore`
- `xrtSshTransportTcpCoreConst`
- `xrtSshTransportTcpIdentificationPrepare`
- `xrtSshTransportTcpIdentificationReadPrepare`
- `xrtSshTransportTcpInit`
- `xrtSshTransportTcpReadAbort`
- `xrtSshTransportTcpReadCommit`
- `xrtSshTransportTcpReadInspect`
- `xrtSshTransportTcpReadPrepare`
- `xrtSshTransportTcpWriteAbort`
- `xrtSshTransportTcpWritePrepareWithPadding`
- `xrtSshTransportTcpWriteSize`
- `xrtSshTransportTcpWriteSubmit`

### 常量与宏 (4)

- `XSSH_TRANSPORT_TCP_BANNER_LIMIT_DEFAULT`
- `XSSH_TRANSPORT_TCP_PENDING_IDENTIFICATION`
- `XSSH_TRANSPORT_TCP_PENDING_NONE`
- `XSSH_TRANSPORT_TCP_PENDING_PACKET`

### 类型 (1)

- `xsshtransporttcppending`

## `extlibs/xssh/include/xrt/ssh_transport_tcp_random.h`

[查看带契约注释的公共头](../../include/xrt/ssh_transport_tcp_random.h)

### 函数 (1)

- `xrtSshTransportTcpWritePrepare`

## `extlibs/xssh/include/xrt/ssh_wire.h`

[查看带契约注释的公共头](../../include/xrt/ssh_wire.h)

### 函数 (31)

- `xrtSshBannerRead`
- `xrtSshBannerWrite`
- `xrtSshLanguageValid`
- `xrtSshNameListContains`
- `xrtSshNameListFirstMatch`
- `xrtSshNameListHasDuplicate`
- `xrtSshNameListValid`
- `xrtSshNameValid`
- `xrtSshReadBool`
- `xrtSshReadByte`
- `xrtSshReadBytes`
- `xrtSshReadMpint`
- `xrtSshReadSignedMpint`
- `xrtSshReadString`
- `xrtSshReadU32`
- `xrtSshReadU64`
- `xrtSshReaderInit`
- `xrtSshReaderRemaining`
- `xrtSshWriteBool`
- `xrtSshWriteByte`
- `xrtSshWriteBytes`
- `xrtSshWriteMpint`
- `xrtSshWriteNameList`
- `xrtSshWriteSignedMpint`
- `xrtSshWriteString`
- `xrtSshWriteU32`
- `xrtSshWriteU64`
- `xrtSshWriterInit`
- `xrtSshWriterRemaining`
- `xrtSshWriterReserve`
- `xrtSshWriterReserveInputs`

### 常量与宏 (12)

- `XSSH_ERROR_ARGUMENT`
- `XSSH_ERROR_AUTHENTICATION`
- `XSSH_ERROR_CALLBACK`
- `XSSH_ERROR_OVERFLOW`
- `XSSH_ERROR_PROTOCOL`
- `XSSH_ERROR_SPACE`
- `XSSH_ERROR_STATE`
- `XSSH_ERROR_TIMEOUT`
- `XSSH_ERROR_UNSUPPORTED`
- `XSSH_FEATURE_SSH`
- `XSSH_IDENTIFICATION_MAX`
- `XSSH_OK`
