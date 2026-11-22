/* Native hot loop for the independent, byte-verifying xrpc TCP observer. */
#include "../../../../dev/bench/bench_common.h"

#if defined(_WIN32)
typedef SOCKET benchsocket;
#define BENCH_SHUT_WRITE SD_SEND
#define BENCH_EXPORT __declspec(dllexport)
#else
#include <sys/select.h>
#include <sys/socket.h>
typedef int benchsocket;
#define BENCH_SHUT_WRITE SHUT_WR
#define BENCH_EXPORT __attribute__((visibility("default")))
#endif

enum { TCP_GROUP_DEPTH = 8, TCP_MAX_PEERS = 32 };
#define TCP_PERIOD_NS UINT64_C(8000000000)
/* Leave one coarse Winsock timeout interval before a scheduled offer. */
#if defined(_WIN32)
#define TCP_SELECT_GUARD_NS UINT64_C(16000000)
#else
#define TCP_SELECT_GUARD_NS UINT64_C(2000000)
#endif

typedef struct tcpgroup {
    const char* Wire;
    const char* Reply;
    size_t WireSize, ReplySize;
    size_t Ends[TCP_GROUP_DEPTH], Calls;
} tcpgroup;

typedef struct tcppeer {
    uintptr_t Socket;
    const tcpgroup* Groups;
    size_t Count, Calls;
    uint64_t Phase;
    uint64_t* Sent;
    uint64_t* CreditWait;
    uint64_t* Received;
    size_t Peak, Stalls;
} tcppeer;

typedef struct tcpstate {
    size_t Offered, Completed, Calls;
    size_t SendOffset, ReplyOffset, ReplyIndex;
    uint64_t BlockedSince;
    bool Writing, Closed;
} tcpstate;

static bool tcpWouldBlock(void)
{
#if defined(_WIN32)
    int Error = WSAGetLastError();
    return Error == WSAEWOULDBLOCK || Error == WSAEINTR;
#else
    return errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR;
#endif
}

static bool tcpFlush(tcppeer* Peer, tcpstate* State)
{
    const tcpgroup* Group = &Peer->Groups[State->Offered - 1];
    benchsocket Socket = (benchsocket)Peer->Socket;
    int Left = (int)(Group->WireSize - State->SendOffset);
    int Count = (int)send(Socket, Group->Wire + State->SendOffset, Left, 0);
    if (Count < 0) return tcpWouldBlock();
    if (!Count) return false;
    State->SendOffset += (size_t)Count;
    if (State->SendOffset != Group->WireSize) return true;
    State->Writing = false;
    return State->Offered != Peer->Count || shutdown(Socket, BENCH_SHUT_WRITE) == 0;
}

static int tcpRead(tcppeer* Peer, tcpstate* State, char* Data, int Capacity)
{
    int Size = (int)recv((benchsocket)Peer->Socket, Data, Capacity, 0);
    if (Size < 0) return tcpWouldBlock() ? 0 : 3;
    uint64_t Observed = xbenchNowNs();
    if (!Size) {
        State->Closed = true;
        return State->Completed == Peer->Count && State->Calls == Peer->Calls && !State->Writing ? 0 : 4;
    }
    size_t Offset = 0;
    while (Offset < (size_t)Size) {
        if (State->Completed == Peer->Count || State->Completed == State->Offered) return 4;
        const tcpgroup* Group = &Peer->Groups[State->Completed];
        size_t Available = (size_t)Size - Offset;
        size_t Left = Group->ReplySize - State->ReplyOffset;
        size_t Match = Available < Left ? Available : Left;
        if (memcmp(Data + Offset, Group->Reply + State->ReplyOffset, Match)) return 4;
        Offset += Match;
        State->ReplyOffset += Match;
        while (State->ReplyIndex < Group->Calls && Group->Ends[State->ReplyIndex] <= State->ReplyOffset) {
            if (State->Calls == Peer->Calls) return 4;
            Peer->Received[State->Calls++] = Observed;
            ++State->ReplyIndex;
        }
        if (State->ReplyOffset == Group->ReplySize) {
            if (State->ReplyIndex != Group->Calls) return 4;
            ++State->Completed;
            State->ReplyOffset = State->ReplyIndex = 0;
        }
    }
    return 0;
}

BENCH_EXPORT uint64_t xrpcTcpObserverNow(void)
{
    return xbenchNowNs();
}

/* The coordinator owns all storage and sockets through this synchronous call.
 * Return 0 on verified EOF, 1 for an invalid plan, 2 for missed deadlines,
 * 3 for I/O, and 4 for a mismatching reply or premature EOF. */
BENCH_EXPORT int xrpcTcpObserve(tcppeer* Peers, size_t Count, size_t Window, uint64_t Rate,
    uint64_t Start, uint64_t End, uint64_t Deadline)
{
    if (!Count || Count > TCP_MAX_PEERS || !Window || !Rate || End <= Start || !Deadline) return 1;
    tcpstate State[TCP_MAX_PEERS] = {0};
    char Data[65536];
    for (size_t I = 0; I < Count; ++I) {
        const tcppeer* Peer = &Peers[I];
#if !defined(_WIN32)
        if (Peer->Socket >= FD_SETSIZE) return 1;
#endif
        size_t Calls = 0;
        for (size_t J = 0; J < Peer->Count; ++J) {
            const tcpgroup* Group = &Peer->Groups[J];
            if (!Group->WireSize || Group->WireSize > INT_MAX || !Group->ReplySize ||
                !Group->Calls || Group->Calls > TCP_GROUP_DEPTH ||
                Group->Ends[Group->Calls - 1] != Group->ReplySize) return 1;
            Calls += Group->Calls;
        }
        if (!Peer->Count || Calls != Peer->Calls) return 1;
    }

    for (;;) {
        fd_set Readable, Writable;
        FD_ZERO(&Readable);
        FD_ZERO(&Writable);
        size_t Live = 0;
        int MaxFd = 0;
        uint64_t Now = xbenchNowNs(), Next = Now + 50000000;
        if (Now >= End) return 2;

        for (size_t I = 0; I < Count; ++I) {
            tcppeer* Peer = &Peers[I];
            tcpstate* Connection = &State[I];
            if (Connection->Closed) continue;
            ++Live;
            benchsocket Socket = (benchsocket)Peer->Socket;
#if !defined(_WIN32)
            if (Socket > MaxFd) MaxFd = Socket;
#endif
            FD_SET(Socket, &Readable);

            if (!Connection->Writing && Connection->Offered < Peer->Count) {
                size_t Index = Connection->Offered;
                uint64_t Due = Start + Peer->Phase + Index * TCP_PERIOD_NS / Rate;
                size_t Used = Index - Connection->Completed;
                if (Due > Now) {
                    if (Due < Next) Next = Due;
                } else if (Used == Window) {
                    if (!Connection->BlockedSince) {
                        Connection->BlockedSince = Now;
                        ++Peer->Stalls;
                    }
                } else {
                    uint64_t Sent = xbenchNowNs();
                    if (Sent - Due >= Deadline) return 2;
                    Peer->Sent[Index] = Sent;
                    Peer->CreditWait[Index] = Connection->BlockedSince ? Sent - Connection->BlockedSince : 0;
                    Connection->BlockedSince = 0;
                    if (Used + 1 > Peer->Peak) Peer->Peak = Used + 1;
                    Connection->Offered = Index + 1;
                    Connection->SendOffset = 0;
                    Connection->Writing = true;
                    if (!tcpFlush(Peer, Connection)) return 3;
                    Due = Start + Peer->Phase + (Index + 1) * TCP_PERIOD_NS / Rate;
                    if (Due < Next) Next = Due;
                }
            }
            if (Connection->Writing) FD_SET(Socket, &Writable);
        }
        if (!Live) return 0;

        Now = xbenchNowNs();
        uint64_t Wait = Next > Now + TCP_SELECT_GUARD_NS ?
            (Next - Now - TCP_SELECT_GUARD_NS + 999) / 1000 : 0;
        struct timeval Timeout = {(long)(Wait / 1000000), (long)(Wait % 1000000)};
        if (select(MaxFd + 1, &Readable, &Writable, NULL, &Timeout) < 0) {
            if (tcpWouldBlock()) continue;
            return 3;
        }
        for (size_t I = 0; I < Count; ++I)
            if (!State[I].Closed && FD_ISSET((benchsocket)Peers[I].Socket, &Writable) &&
                !tcpFlush(&Peers[I], &State[I])) return 3;
        for (size_t I = 0; I < Count; ++I) {
            if (!State[I].Closed && FD_ISSET((benchsocket)Peers[I].Socket, &Readable)) {
                int Error = tcpRead(&Peers[I], &State[I], Data, sizeof(Data));
                if (Error) return Error;
            }
        }
    }
}
