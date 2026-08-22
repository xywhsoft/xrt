#include "../internal/xrt_websocket_upgrade.h"



#if defined(XRT_FEATURE_WEBSOCKET_UPGRADE_STREAM)

/* 把握手事实应用到新的 Stream 配置，调用方仍可继续收紧资源上限。 */
XRT_API bool xrtWsUpgradeStreamConfig(
	xwsstreamconfig* pOutput,
	xwsrole Role,
	const xwsupgrade* pInput
)
{
	xwsstreamconfig Config;
	xwsupgrade Upgrade;

	if ( !__xrtRangeValid(pOutput, sizeof(Config)) ||
		!__xrtRangeValid(pInput, sizeof(Upgrade)) ||
		__xrtRangesOverlap(
			pOutput,
			sizeof(Config),
			pInput,
			sizeof(Upgrade)
		) || ((Role != XWS_ROLE_CLIENT) &&
		 (Role != XWS_ROLE_SERVER)) ) {
		__xrtWsHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"config-websocket-upgrade-stream",
			"WebSocket Upgrade Stream configuration input is invalid"
		);
		return false;
	}
	memcpy(&Upgrade, pInput, sizeof(Upgrade));
	if ( !__xrtRangeValid(
		Upgrade.Protocol.Data,
		Upgrade.Protocol.Size
	) || ((Upgrade.Protocol.Size != 0) &&
		 !xrtHttpTokenValid(Upgrade.Protocol))
		#if defined(XRT_FEATURE_WEBSOCKET_UPGRADE_DEFLATE)
			|| (Upgrade.ExtensionSize > XWS_DEFLATE_MAX_SIZE)
			|| (Upgrade.Extensions[Upgrade.ExtensionSize] != '\0')
		#endif
	) {
		__xrtWsHandshakeError(
			XERR_ARGUMENT,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"config-websocket-upgrade-stream",
			"WebSocket Upgrade result is invalid"
		);
		return false;
	}
	xrtWsStreamConfigInit(&Config);
	Config.Role = Role;
	Config.Protocol = Upgrade.Protocol;
	#if defined(XRT_FEATURE_WEBSOCKET_UPGRADE_DEFLATE)
		Config.DeflateEnabled = Upgrade.DeflateEnabled;
		if ( Upgrade.DeflateEnabled ) {
			Config.Deflate = Upgrade.Deflate;
		}
	#endif
	if ( !xrtWsStreamConfigValid(&Config) ) {
		__xrtWsHandshakeWrap(
			XERR_VALUE,
			XWS_HANDSHAKE_ERROR_ARGUMENT,
			"config-websocket-upgrade-stream",
			"WebSocket Upgrade result cannot configure the Stream"
		);
		return false;
	}
	memcpy(pOutput, &Config, sizeof(Config));
	return true;
}

#endif
