#include <xrudp/ack.h>
#include <string.h>

static void xrudp__ack_put64(unsigned char* p, uint64_t n)
{
	for (size_t i = 0; i < 8; ++i) p[7 - i] = (unsigned char)(n >> (i * 8));
}

static uint64_t xrudp__ack_get64(const unsigned char* p)
{
	uint64_t n = 0;
	for (size_t i = 0; i < 8; ++i) n = (n << 8) | p[i];
	return n;
}

void xrtRudpAckSetInit(xrudpackset* pSet)
{
	if (pSet != NULL) memset(pSet, 0, sizeof(*pSet));
}

bool xrtRudpAckSetContains(const xrudpackset* pSet, uint64_t number)
{
	if (pSet == NULL || number == 0 || pSet->Count > XRUDP_ACK_RANGES_MAX) return false;
	for (size_t i = 0; i < pSet->Count; ++i) {
		if (number >= pSet->Ranges[i].First && number <= pSet->Ranges[i].Last) return true;
	}
	return false;
}

xrudpresult xrtRudpAckSetRecord(xrudpackset* pSet, uint64_t number, uint64_t now)
{
	xrudpackrange sorted[XRUDP_ACK_RANGES_MAX + 1];
	xrudpackrange merged[XRUDP_ACK_RANGES_MAX + 1];
	size_t index = 0, count, used = 0;
	if (pSet == NULL || number == 0 || pSet->Count > XRUDP_ACK_RANGES_MAX ||
		(pSet->Count != 0 && now < pSet->LargestAtUs)) return XRUDP_ARGUMENT;
	count = pSet->Count;
	for (size_t i = 0; i < count; ++i) {
		if (number >= pSet->Ranges[i].First && number <= pSet->Ranges[i].Last)
			return XRUDP_DUPLICATE;
	}
	while (index < count && pSet->Ranges[index].Last > number) ++index;
	for (size_t i = 0; i < index; ++i) sorted[i] = pSet->Ranges[i];
	sorted[index].First = sorted[index].Last = number;
	for (size_t i = index; i < count; ++i) sorted[i + 1] = pSet->Ranges[i];
	for (size_t i = 0; i < count + 1; ++i) {
		if (used != 0 && (sorted[i].Last >= merged[used - 1].First ||
			merged[used - 1].First - sorted[i].Last == 1)) {
			if (sorted[i].First < merged[used - 1].First)
				merged[used - 1].First = sorted[i].First;
		} else merged[used++] = sorted[i];
	}
	if (used > XRUDP_ACK_RANGES_MAX) {
		if (number < merged[XRUDP_ACK_RANGES_MAX - 1].First) return XRUDP_TOO_OLD;
		used = XRUDP_ACK_RANGES_MAX;
	}
	if (count == 0 || number > pSet->Ranges[0].Last) pSet->LargestAtUs = now;
	pSet->Count = (uint16_t)used;
	memcpy(pSet->Ranges, merged, used * sizeof(merged[0]));
	memset(pSet->Ranges + used, 0, (XRUDP_ACK_RANGES_MAX - used) * sizeof(merged[0]));
	return XRUDP_OK;
}

xrudpresult xrtRudpAckSetEncode(const xrudpackset* pSet, uint64_t now,
	void* output, size_t capacity, size_t* written)
{
	unsigned char body[8 + XRUDP_ACK_RANGES_MAX * 16] = {0};
	uint64_t elapsed;
	uint32_t delay;
	if (written != NULL) *written = 0;
	if (pSet == NULL || output == NULL || written == NULL ||
		pSet->Count > XRUDP_ACK_RANGES_MAX) return XRUDP_ARGUMENT;
	if (pSet->Count == 0) return XRUDP_DONE;
	if (now < pSet->LargestAtUs) return XRUDP_ARGUMENT;
	elapsed = now - pSet->LargestAtUs;
	delay = elapsed > UINT32_MAX ? UINT32_MAX : (uint32_t)elapsed;
	for (size_t i = 0; i < 4; ++i) body[3 - i] = (unsigned char)(delay >> (i * 8));
	body[4] = (unsigned char)(pSet->Count >> 8);
	body[5] = (unsigned char)pSet->Count;
	for (size_t i = 0; i < pSet->Count; ++i) {
		xrudp__ack_put64(body + 8 + i * 16, pSet->Ranges[i].First);
		xrudp__ack_put64(body + 16 + i * 16, pSet->Ranges[i].Last);
	}
	return xrtRudpFrameAppend(output, capacity, written, XRUDP_FRAME_ACK, 0,
		body, 8u + (size_t)pSet->Count * 16u);
}

xrudpresult xrtRudpAckDecode(const void* data, size_t size, xrudpack* ack)
{
	xrudpwireframe frame;
	xrudpack decoded;
	xrudpresult result;
	size_t offset = 0;
	if (ack == NULL) return XRUDP_ARGUMENT;
	result = xrtRudpFrameNext(data, size, &offset, &frame);
	if (result != XRUDP_OK) return result == XRUDP_DONE ? XRUDP_MALFORMED : result;
	if (frame.Type != XRUDP_FRAME_ACK || offset != size) return XRUDP_MALFORMED;
	memset(&decoded, 0, sizeof(decoded));
	for (size_t i = 0; i < 4; ++i) decoded.DelayUs = (decoded.DelayUs << 8) | frame.Data[i];
	decoded.Count = (uint16_t)(((uint16_t)frame.Data[4] << 8) | frame.Data[5]);
	for (size_t i = 0; i < decoded.Count; ++i) {
		decoded.Ranges[i].First = xrudp__ack_get64(frame.Data + 8 + i * 16);
		decoded.Ranges[i].Last = xrudp__ack_get64(frame.Data + 16 + i * 16);
	}
	*ack = decoded;
	return XRUDP_OK;
}
