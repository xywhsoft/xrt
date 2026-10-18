#include <xrudp/message.h>
#include <xrt/memory.h>
#include <string.h>

struct xrudpmessage {
	xrudpmessagestate State;
	bool Used;
	unsigned char Data[];
};

xrudpresult xrtRudpMessageAllocationSize(uint32_t maximum, size_t* output)
{
	if (output == NULL || maximum > XRUDP_MESSAGE_SIZE_MAX) return XRUDP_ARGUMENT;
	*output = sizeof(xrudpmessage) + maximum; return XRUDP_OK;
}
xrudpresult xrtRudpMessageCreate(uint32_t maximum, xrudpmessage** output)
{
	size_t size;
	if (output == NULL || xrtRudpMessageAllocationSize(maximum, &size) != XRUDP_OK) return XRUDP_ARGUMENT;
	xrudpmessage* m = (xrudpmessage*)xrtMalloc(size);
	if (m == NULL) return XRUDP_NO_MEMORY;
	memset(m, 0, size); m->State.Maximum = maximum; *output = m; return XRUDP_OK;
}
void xrtRudpMessageDestroy(xrudpmessage* m)
{
	if (m != NULL) { xrtSecureZero(m, sizeof(*m) + m->State.Maximum); xrtFree(m); }
}
xrudpresult xrtRudpMessageState(const xrudpmessage* m, xrudpmessagestate* output)
{
	if (m == NULL || output == NULL) return XRUDP_ARGUMENT;
	*output = m->State; return XRUDP_OK;
}
static void xrudp__message_clear(xrudpmessage* m, xrudpresult reason)
{
	xrtSecureZero(m->Data, m->State.Buffered);
	uint32_t maximum = m->State.Maximum;
	memset(&m->State, 0, sizeof(m->State)); m->State.Maximum = maximum; m->State.Reason = reason;
}
xrudpresult xrtRudpMessagePush(xrudpmessage* m, const void* data, size_t size, size_t* consumed)
{
	if (consumed != NULL) *consumed = 0;
	if (m == NULL || consumed == NULL || (data == NULL && size != 0)) return XRUDP_ARGUMENT;
	xrudpmessagestate* s = &m->State;
	if (s->Reason != XRUDP_OK) return s->Reason;
	if (s->Finished) return XRUDP_CLOSED;
	if (s->Ready) return XRUDP_BACKPRESSURE;
	if (size != 0) m->Used = true;
	const unsigned char* bytes = (const unsigned char*)data;
	while (s->HeaderBytes < 4 && *consumed < size) {
		s->Expected = (s->Expected << 8) | bytes[(*consumed)++]; ++s->HeaderBytes;
	}
	if (s->HeaderBytes != 4) return XRUDP_OK;
	if (s->Expected > s->Maximum) { xrudp__message_clear(m, XRUDP_MALFORMED); return XRUDP_MALFORMED; }
	size_t count = s->Expected - s->Buffered;
	if (count > size - *consumed) count = size - *consumed;
	if (count != 0) memcpy(m->Data + s->Buffered, bytes + *consumed, count);
	s->Buffered += (uint32_t)count; *consumed += count; s->Ready = s->Buffered == s->Expected;
	return XRUDP_OK;
}
xrudpresult xrtRudpMessagePeek(const xrudpmessage* m, size_t* size)
{
	if (size != NULL) *size = 0;
	if (m == NULL || size == NULL) return XRUDP_ARGUMENT;
	if (m->State.Reason != XRUDP_OK) return m->State.Reason;
	if (m->State.Ready) { *size = m->State.Expected; return XRUDP_OK; }
	return m->State.Finished ? XRUDP_DONE : XRUDP_BACKPRESSURE;
}
xrudpresult xrtRudpMessageRead(xrudpmessage* m, void* output, size_t capacity, size_t* size)
{
	if (size != NULL) *size = 0;
	if (size == NULL || (output == NULL && capacity != 0)) return XRUDP_ARGUMENT;
	xrudpresult result = xrtRudpMessagePeek(m, size);
	if (result != XRUDP_OK) return result;
	if (capacity < *size) return XRUDP_TRUNCATED;
	if (*size != 0) memcpy(output, m->Data, *size);
	bool finished = m->State.Finished; xrudp__message_clear(m, XRUDP_OK); m->State.Finished = finished;
	return XRUDP_OK;
}
xrudpresult xrtRudpMessageFinish(xrudpmessage* m)
{
	if (m == NULL) return XRUDP_ARGUMENT;
	if (m->State.Reason != XRUDP_OK) return m->State.Reason;
	if (m->State.Finished) return XRUDP_DUPLICATE;
	if (!m->State.Ready && m->State.HeaderBytes != 0) { xrudp__message_clear(m, XRUDP_MALFORMED); return XRUDP_MALFORMED; }
	m->State.Finished = true; return XRUDP_OK;
}
xrudpresult xrtRudpMessageReset(xrudpmessage* m)
{
	if (m == NULL) return XRUDP_ARGUMENT;
	if (m->State.Reason == XRUDP_CANCELLED) return XRUDP_DUPLICATE;
	if (m->State.Reason != XRUDP_OK) return m->State.Reason;
	if (m->State.Finished && !m->State.Ready) return XRUDP_DONE;
	xrudp__message_clear(m, XRUDP_CANCELLED); return XRUDP_OK;
}
xrudpresult xrtRudpMessageRestart(xrudpmessage* m)
{
	if (m == NULL) return XRUDP_ARGUMENT;
	if (m->State.Reason == XRUDP_OK && ((m->Used && !m->State.Finished) || m->State.Ready || m->State.HeaderBytes != 0)) return XRUDP_BACKPRESSURE;
	xrudp__message_clear(m, XRUDP_OK); m->Used = false; return XRUDP_OK;
}
