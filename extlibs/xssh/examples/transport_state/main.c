#include <stdio.h>
#include <xssh.h>



/* 以 client 角色推进一次无分配 strict ECDH transport 状态。 */
int main(void)
{
	xsshtransportstate State;
	xsshtransportkexrules Rules;
	xsshkexinit Local = { 0 };
	xsshkexinit Peer = { 0 };
	xsshkexnegotiation Negotiation = { 0 };
	uint32 iActions;

	Local.KexAlgorithms = XRT_STR_LITERAL(XSSH_KEX_CLIENT_INITIAL_DEFAULT);
	Local.ServerHostKeyAlgorithms = XRT_STR_LITERAL("ssh-ed25519");
	Local.EncryptionClientToServer = XRT_STR_LITERAL(XSSH_CIPHER_DEFAULT);
	Local.EncryptionServerToClient = XRT_STR_LITERAL(XSSH_CIPHER_DEFAULT);
	Local.MacClientToServer = XRT_STR_LITERAL(XSSH_MAC_DEFAULT);
	Local.MacServerToClient = XRT_STR_LITERAL(XSSH_MAC_DEFAULT);
	Local.CompressionClientToServer =
		XRT_STR_LITERAL(XSSH_COMPRESSION_DEFAULT);
	Local.CompressionServerToClient =
		XRT_STR_LITERAL(XSSH_COMPRESSION_DEFAULT);
	Peer.KexAlgorithms = XRT_STR_LITERAL(XSSH_KEX_SERVER_INITIAL_DEFAULT);
	Peer.ServerHostKeyAlgorithms = XRT_STR_LITERAL("ssh-ed25519");
	Peer.EncryptionClientToServer = XRT_STR_LITERAL(XSSH_CIPHER_DEFAULT);
	Peer.EncryptionServerToClient = XRT_STR_LITERAL(XSSH_CIPHER_DEFAULT);
	Peer.MacClientToServer = XRT_STR_LITERAL(XSSH_MAC_DEFAULT);
	Peer.MacServerToClient = XRT_STR_LITERAL(XSSH_MAC_DEFAULT);
	Peer.CompressionClientToServer =
		XRT_STR_LITERAL(XSSH_COMPRESSION_DEFAULT);
	Peer.CompressionServerToClient =
		XRT_STR_LITERAL(XSSH_COMPRESSION_DEFAULT);
	if ( !xrtSshTransportStateInit(&State, XSSH_ROLE_CLIENT) ||
		!xrtSshTransportKexRulesInit(&Rules) ||
		!xrtSshTransportKexRuleSet(
			&Rules,
			XSSH_TRANSPORT_LOCAL,
			30u,
			1u
		) || !xrtSshTransportKexRuleSet(
			&Rules,
			XSSH_TRANSPORT_PEER,
			31u,
			1u
		) ||
		(xrtSshKexNegotiate(&Local, &Peer, &Negotiation) != XSSH_OK) ||
		(xrtSshTransportIdentificationCommit(
			&State,
			XSSH_TRANSPORT_LOCAL
		) != XSSH_OK) || (xrtSshTransportIdentificationCommit(
			&State,
			XSSH_TRANSPORT_PEER
		) != XSSH_OK) || (xrtSshTransportKexInitCommit(
			&State,
			XSSH_TRANSPORT_LOCAL,
			false
		) != XSSH_OK) || (xrtSshTransportKexInitCommit(
			&State,
			XSSH_TRANSPORT_PEER,
			false
		) != XSSH_OK) || (xrtSshTransportKexConfigure(
			&State,
			&Local,
			&Peer,
			&Negotiation,
			&Rules
		) != XSSH_OK) || (xrtSshTransportMessageCommit(
			&State,
			XSSH_TRANSPORT_LOCAL,
			30u
		) != XSSH_OK) || (xrtSshTransportMessageCommit(
			&State,
			XSSH_TRANSPORT_PEER,
			31u
		) != XSSH_OK) || (xrtSshTransportNewKeysCommit(
			&State,
			XSSH_TRANSPORT_LOCAL,
			&iActions
		) != XSSH_OK) ) {
		return 1;
	}
	printf("strict=%d write-actions=%u\n",
		State.Strict ? 1 : 0,
		(unsigned int)iActions);
	return 0;
}
