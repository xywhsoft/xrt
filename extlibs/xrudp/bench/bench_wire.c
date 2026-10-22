#include "../../../dev/bench/bench_common.h"
#include <xrudp.h>

/* Offline codec and replay-window work; no socket or Engine is created. */
int main(int argc, char** argv)
{
	const uint32_t Count = xbenchArgU32(argc, argv, 1, 1000000u);
	unsigned char Payload[64] = { 1 };
	unsigned char Output[XRUDP_HEADER_SIZE + sizeof(Payload)];
	xrudpwireheader Header = { 0 };
	xrudpwirepacket Packet;
	xrudpreplay Replay;
	xbenchtimer Timer;
	uint64_t Checksum = 0;
	size_t Written = 0;

	if (Count == 0) return 1;
	Header.Type = XRUDP_PACKET_HELLO;
	Header.PayloadSize = (uint16_t)sizeof(Payload);
	Header.ConnectionId[0] = 1;
	xbenchTimerStart(&Timer);
	for (uint32_t i = 0; i < Count; ++i) {
		Header.PacketNumber = (uint64_t)i + 1u;
		if (xrtRudpPacketEncode(Output, sizeof(Output), &Written,
			&Header, Payload, sizeof(Payload)) != XRUDP_OK ||
			Written != sizeof(Output) ||
			xrtRudpPacketDecode(Output, Written, &Packet) != XRUDP_OK ||
			Packet.Header.PacketNumber != Header.PacketNumber ||
			Packet.Header.PayloadSize != sizeof(Payload) ||
			memcmp(Packet.Payload, Payload, sizeof(Payload)) != 0) return 1;
		Checksum += Packet.Header.PacketNumber;
	}
	xbenchTimerStop(&Timer);
	xbenchPrintMetricDouble("rudp_wire_roundtrips_per_sec",
		xbenchSafeRate(Count, xbenchTimerElapsedNs(&Timer)));
	if (Checksum != (uint64_t)Count * ((uint64_t)Count + 1u) / 2u) return 1;

	xrtRudpReplayInit(&Replay);
	xbenchTimerStart(&Timer);
	for (uint32_t i = 0; i < Count; ++i) {
		const uint64_t Number = (uint64_t)i + 1u;
		if (xrtRudpReplayAccept(&Replay, Number) != XRUDP_OK ||
			xrtRudpReplayCheck(&Replay, Number) != XRUDP_DUPLICATE) return 1;
	}
	xbenchTimerStop(&Timer);
	xbenchPrintMetricDouble("rudp_replay_pairs_per_sec",
		xbenchSafeRate(Count, xbenchTimerElapsedNs(&Timer)));
	return 0;
}
