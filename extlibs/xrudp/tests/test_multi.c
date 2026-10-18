#include <xrudp.h>
#include <stdio.h>
#include <string.h>
#ifdef TEST_MULTI_MEMORY
#include <xrt/memory_debug.h>
#endif

#define CHECK(e) do { if (!(e)) { fprintf(stderr,"%s:%d: %s (tick=%u)\n",__FILE__,__LINE__,#e,tick); return 1; } } while (0)
#define MULTI_BYTES 8192u
#define MULTI_COUNT 5u
#define QUEUE_COUNT 512u
typedef struct multi_packet { unsigned char Data[1200]; size_t Size; uint32_t Target,Due; bool Used;
#ifdef TEST_MULTI_PATH
	xrudpaddress Source,Destination;
#endif
} multi_packet;
static struct {
	xrudpsession* Session[2]; multi_packet Packet[QUEUE_COUNT];
	bool Active[2][MULTI_COUNT], Eof[2][MULTI_COUNT];
	size_t Written[2][MULTI_COUNT], Read[2][MULTI_COUNT];
	uint32_t Deferred,Dropped,Blocked,Duplicated;
	uint32_t Epoch[2],RetiredKeys;
#ifdef TEST_MULTI_STOP
	bool ReceiveStopped,PeerStopped;
#endif
#ifdef TEST_MULTI_DATAGRAMS
	uint32_t GameWritten[2][4],GameRead[2][4],GameLast[2][4];
	bool GameSeen[2][3][4001];
#endif
} net;
static uint32_t tick, randomState;
#ifdef TEST_MULTI_PATH
static xrudpaddress pathDestination;
static xrudpaddress multi_address(uint32_t side)
{
	xrudpaddress a={4,0,0,{192,0,2,1}};a.Port=(uint16_t)(41000+side+(side && tick>=600?1000:0));a.Address[3]=(unsigned char)(1+side);return a;
}
#endif
static uint32_t ids[MULTI_COUNT]={1,3,5,2,4};
static uint32_t random_next(void) { randomState^=randomState<<13;randomState^=randomState>>17;randomState^=randomState<<5;return randomState; }
static unsigned char pattern(uint32_t side,uint32_t id,size_t pos) { return (unsigned char)((side*19+id*37+pos*13)^(pos>>5)); }
static xrudpresult output(const void* data,size_t size,void* context)
{
	uint32_t side=(uint32_t)(uintptr_t)context;
	if (random_next()%19==0) { ++net.Blocked;return XRUDP_BACKPRESSURE; }
	if ((tick>=80 && tick<240) || random_next()%100<25) { ++net.Dropped;return XRUDP_OK; }
	unsigned int copies=random_next()%5==0?2:1; if(copies==2)++net.Duplicated;
	for(unsigned int c=0;c<copies;++c) {
		size_t i;for(i=0;i<QUEUE_COUNT && net.Packet[i].Used;++i){}
		if(i==QUEUE_COUNT)return XRUDP_IO_ERROR;
		multi_packet* p=&net.Packet[i];p->Used=true;p->Size=size;p->Target=1-side;p->Due=tick+1+random_next()%12;memcpy(p->Data,data,size);
#ifdef TEST_MULTI_PATH
		p->Source=multi_address(side);p->Destination=pathDestination;
#endif
	}
	return XRUDP_OK;
}
#ifdef TEST_MULTI_PATH
static xrudpresult multi_path_output(const void* data,size_t size,const xrudpaddress* to,void* context)
{
	pathDestination=*to;return output(data,size,context);
}
#endif
static int cipher_pair(xrudpcipher* cipher[2])
{
	unsigned char key[2][32]={{3},{11}},id[2][16]={{7},{19}},packet[2][100],plain[100];size_t size[2],read;
	unsigned char proof[12]={XRUDP_FRAME_PING,0,0,0,XRUDP_FRAME_KEY_CONFIRM,0,0,4,0,0,0,1};
	for(size_t i=0;i<2;++i) {
		CHECK(xrtRudpCipherCreate(key[i],key[1-i],id[i],id[1-i],0,&cipher[i])==XRUDP_OK);
		CHECK(xrtRudpCipherSeal(cipher[i],proof,12,packet[i],100,&size[i])==XRUDP_OK);
	}
	for(size_t i=0;i<2;++i)CHECK(xrtRudpCipherOpen(cipher[i],packet[1-i],size[1-i],plain,100,&read)==XRUDP_OK);
	return 0;
}
static void config(xrudpsessionconfig* c,uint32_t side)
{
	xrtRudpSessionConfigInit(c);c->Role=side?XRUDP_RESPONDER:XRUDP_INITIATOR;
	c->StreamCapacity=3;c->Local.MaxStreams=c->Peer.MaxStreams=3;
	c->ReceiveCapacity=c->TransmitCapacity=256;c->Local.StreamCredit=c->Peer.StreamCredit=256;
	c->Local.ConnectionCredit=c->Peer.ConnectionCredit=1024;c->Local.DatagramSize=c->Peer.DatagramSize=512;
	c->PacketCapacity=64;c->InitialRttUs=10000;c->Local.MaxAckDelayUs=c->Peer.MaxAckDelayUs=1000;
	c->Local.IdleTimeoutMs=c->Peer.IdleTimeoutMs=60000;c->KeyUpdatePackets=16;
#ifdef TEST_MULTI_DATAGRAMS
	c->Datagrams.Channels=3;c->Datagrams.SendCapacity=16;c->Datagrams.ReceiveCapacity=32;c->Datagrams.PayloadCapacity=424;
#endif
}
static int events(uint32_t side)
{
	xrudpstreamevent event;
	for(size_t n=0;n<MULTI_COUNT+1;++n) {
		xrudpresult r=xrtRudpSessionStreamNextEvent(net.Session[side],&event);
		if(r==XRUDP_DONE)return 0;
		CHECK(r==XRUDP_OK);
		size_t k;for(k=0;k<MULTI_COUNT && ids[k]!=event.Id;++k){}
		CHECK(k<MULTI_COUNT);
		if(event.Events&XRUDP_STREAM_OPENED) {
			CHECK(!net.Active[side][k]);net.Active[side][k]=true;
			xrudpstreamstate state;CHECK(xrtRudpSessionStreamState(net.Session[side],event.Id,&state)==XRUDP_OK && !state.Local);
		}
#ifdef TEST_MULTI_STOP
		if(event.Events&XRUDP_STREAM_WRITE_STOPPED) {
			xrudpstreamstate state;CHECK(side==0 && event.Id==3 && xrtRudpSessionStreamState(net.Session[side],3,&state)==XRUDP_OK && state.PeerStopped && state.PeerStopReason==71);
			net.PeerStopped=true;
		}
#endif
	}
	CHECK(false);return 1;
}
static int scenario(uint32_t seed)
{
	memset(&net,0,sizeof(net));net.Epoch[0]=net.Epoch[1]=1;randomState=seed;tick=0;xrudpcipher* cipher[2]={NULL,NULL};CHECK(cipher_pair(cipher)==0);
	for(uint32_t side=0;side<2;++side) {
		xrudpsessionconfig c;config(&c,side);CHECK(xrtRudpSessionCreate(&c,cipher[side],0,output,(void*)(uintptr_t)side,&net.Session[side])==XRUDP_OK);
#ifdef TEST_MULTI_PATH
		xrudpaddress address=multi_address(1-side);CHECK(xrtRudpSessionEnablePaths(net.Session[side],&address,multi_path_output)==XRUDP_OK);
#endif
#ifdef TEST_MULTI_DATAGRAMS
		for(uint32_t id=1;id<=3;++id) { xrudpdatagramchannel ch;xrtRudpDatagramChannelInit(&ch);ch.Id=id;ch.Mode=(uint8_t)(id-1);ch.ReplacePending=id==3;CHECK(xrtRudpSessionDatagramConfigure(net.Session[side],&ch)==XRUDP_OK); }
#endif
		net.Active[side][0]=true;
		for(size_t n=0;n<2;++n) {
			xrudpstreamconfig stream;xrtRudpStreamConfigInit(&stream);stream.Priority=(uint8_t)n;stream.Weight=(uint16_t)(n+1);
			uint32_t id;CHECK(xrtRudpSessionStreamOpen(net.Session[side],&stream,&id)==XRUDP_OK && id==ids[side?3+n:1+n]);net.Active[side][side?3+n:1+n]=true;
		}
		uint32_t extra=77;xrudpstreamconfig stream;xrtRudpStreamConfigInit(&stream);
		CHECK(xrtRudpSessionStreamOpen(net.Session[side],&stream,&extra)==XRUDP_BACKPRESSURE && extra==77);
	}
#ifdef TEST_MULTI_MEMORY
	CHECK(xrtMemDebugFailAfter(0));
#endif
	bool done=false,otherProgress=false;uint64_t due[2]={0,0};uint32_t wake=3;
	for(tick=1;tick<150000 && !done;++tick) {
		uint64_t now=(uint64_t)tick*1000;
		for(size_t i=0;i<QUEUE_COUNT;++i)if(net.Packet[i].Used && net.Packet[i].Due<=tick) {
			multi_packet packet=net.Packet[i];net.Packet[i].Used=false;
#ifdef TEST_MULTI_PATH
			xrudpaddress current=multi_address(packet.Target);if(packet.Destination.Port!=current.Port) { ++net.Dropped;continue; }
#endif
			if(random_next()%11==0) {
				unsigned char bad[1200];memcpy(bad,packet.Data,packet.Size);bad[packet.Size-1]^=1;
#ifdef TEST_MULTI_PATH
				xrudpresult badResult=xrtRudpSessionPathInput(net.Session[packet.Target],&packet.Source,bad,packet.Size,now);CHECK(badResult==XRUDP_AUTH_FAILED || badResult==XRUDP_BACKPRESSURE);
#else
				CHECK(xrtRudpSessionInput(net.Session[packet.Target],bad,packet.Size,now)==XRUDP_AUTH_FAILED);
#endif
			}
#ifdef TEST_MULTI_PATH
			xrudpresult r=xrtRudpSessionPathInput(net.Session[packet.Target],&packet.Source,packet.Data,packet.Size,now);
#else
			xrudpresult r=xrtRudpSessionInput(net.Session[packet.Target],packet.Data,packet.Size,now);
#endif
			xrudpwirepacket header;CHECK(xrtRudpPacketDecode(packet.Data,packet.Size,&header)==XRUDP_OK);
			if(r==XRUDP_AUTH_FAILED && header.Header.KeyEpoch+1<net.Epoch[packet.Target]) { ++net.RetiredKeys;continue; }
			if(r!=XRUDP_AUTH_FAILED && header.Header.KeyEpoch>net.Epoch[packet.Target])net.Epoch[packet.Target]=header.Header.KeyEpoch;
			if(r!=XRUDP_OK && r!=XRUDP_DUPLICATE && r!=XRUDP_TOO_OLD && r!=XRUDP_BACKPRESSURE) {
				xrudpwirepacket parsed;(void)xrtRudpPacketDecode(packet.Data,packet.Size,&parsed);
				fprintf(stderr,"multi input result=%u side=%u PN=%llu size=%zu\n",(unsigned int)r,packet.Target,(unsigned long long)parsed.Header.PacketNumber,packet.Size);
			}
			CHECK(r==XRUDP_OK || r==XRUDP_DUPLICATE || r==XRUDP_TOO_OLD || r==XRUDP_BACKPRESSURE);
			if(r==XRUDP_BACKPRESSURE)++net.Deferred;
			wake|=1u<<packet.Target;
		}
		for(uint32_t side=0;side<2;++side) {
			CHECK(events(side)==0);
#ifdef TEST_MULTI_STOP
			if(side==1 && tick>=600 && net.Active[1][1] && !net.ReceiveStopped) {
				CHECK(xrtRudpSessionStreamStop(net.Session[1],3,71)==XRUDP_OK);net.ReceiveStopped=true;wake|=2;
			}
#endif
#ifdef TEST_MULTI_DATAGRAMS
			if(tick<4000 && tick%4==0)for(uint32_t id=1;id<=3;++id)for(uint32_t repeat=0;repeat<(id==3?3u:1u);++repeat) {
				unsigned char bytes[424];size_t size=id==3?128:sizeof(bytes);uint32_t serial=net.GameWritten[side][id]+1;
				for(size_t j=0;j<4;++j)bytes[3-j]=(unsigned char)(serial>>(j*8));
				for(size_t j=4;j<size;++j)bytes[j]=pattern(side,id,j+serial);
				uint64_t seq;xrudpresult r=xrtRudpSessionDatagramWrite(net.Session[side],id,bytes,size,now,now+(id==3?1000:50000),&seq);
				CHECK(r==XRUDP_OK || r==XRUDP_BACKPRESSURE);
				if(r==XRUDP_OK) { CHECK(seq==(id==1?0:serial));net.GameWritten[side][id]=serial;wake|=1u<<side; }
			}
			for(size_t n=0;n<8;++n) {
				unsigned char bytes[424];xrudpdatagraminfo info;xrudpresult r=xrtRudpSessionDatagramRead(net.Session[side],bytes,sizeof(bytes),now,&info);
				if(r==XRUDP_BACKPRESSURE)break;
				CHECK(r==XRUDP_OK && info.Channel>=1 && info.Channel<=3 && info.Size==(info.Channel==3?128:424));
				uint32_t serial=0;for(size_t j=0;j<4;++j)serial=(serial<<8)|bytes[j];uint32_t id=info.Channel;
				CHECK(serial>0 && serial<=4000 && serial<=net.GameWritten[1-side][id] && !net.GameSeen[side][id-1][serial]);
				net.GameSeen[side][id-1][serial]=true;
				if(id!=1)CHECK(serial>net.GameLast[side][id] && info.Sequence==serial);
				net.GameLast[side][id]=serial;++net.GameRead[side][id];
				for(size_t j=4;j<info.Size;++j)CHECK(bytes[j]==pattern(1-side,id,j+serial));
			}
#endif
			for(size_t k=0;k<MULTI_COUNT;++k)if(net.Active[side][k]) {
				bool stopped=false;
#ifdef TEST_MULTI_STOP
				stopped=side==0 && k==1 && net.PeerStopped;
#endif
				if(net.Written[side][k]<MULTI_BYTES && !stopped) {
					unsigned char source[128];size_t n=MULTI_BYTES-net.Written[side][k];if(n>sizeof(source))n=sizeof(source);
					for(size_t j=0;j<n;++j)source[j]=pattern(side,ids[k],net.Written[side][k]+j);
					size_t written;xrudpresult r=xrtRudpSessionStreamWrite(net.Session[side],ids[k],source,n,&written);
					CHECK(r==XRUDP_OK || r==XRUDP_BACKPRESSURE);net.Written[side][k]+=written;if(written)wake|=1u<<side;
					if(net.Written[side][k]==MULTI_BYTES)CHECK(xrtRudpSessionStreamFinish(net.Session[side],ids[k])==XRUDP_OK);
				}
				/* One stalled consumer must not block delivery on other streams. */
				if(!(side==1 && ids[k]==3 && tick<1500) && random_next()%3==0) {
					unsigned char out[97];size_t read;xrudpresult r=xrtRudpSessionStreamRead(net.Session[side],ids[k],out,1+random_next()%sizeof(out),&read);
					bool cancelled=false;
#ifdef TEST_MULTI_STOP
					cancelled=side==1 && k==1 && net.ReceiveStopped && r==XRUDP_CANCELLED;
					if(cancelled) { CHECK(read==0 && net.Read[side][k]==0);net.Eof[side][k]=true; }
#endif
					CHECK(r==XRUDP_OK || r==XRUDP_BACKPRESSURE || r==XRUDP_DONE || cancelled);
					for(size_t j=0;j<read;++j)CHECK(out[j]==pattern(1-side,ids[k],net.Read[side][k]+j));
					net.Read[side][k]+=read;CHECK(net.Read[side][k]<=MULTI_BYTES);if(read)wake|=1u<<side;
					if(r==XRUDP_DONE) { CHECK(net.Read[side][k]==MULTI_BYTES);net.Eof[side][k]=true; }
					if(tick<1500 && ids[k]!=3 && read)otherProgress=true;
				}
			}
			if(now>=due[side] || (wake&(1u<<side))) {
				CHECK(xrtRudpSessionPoll(net.Session[side],now,&due[side])==XRUDP_OK && due[side]>now);wake&=~(1u<<side);
			}
			xrudpsessionstate state;CHECK(xrtRudpSessionState(net.Session[side],&state)==XRUDP_OK && state.Sent.BytesInFlight==state.Congestion.BytesInFlight);
			uint64_t sent=0,received=0;
			for(size_t k=0;k<MULTI_COUNT;++k)if(net.Active[side][k]) {
				xrudpstreamstate stream;CHECK(xrtRudpSessionStreamState(net.Session[side],ids[k],&stream)==XRUDP_OK);
				sent+=stream.Transmit.HighestSent;received+=stream.Receive.HighestOffset;
			}
			CHECK(state.SentConnection==sent && state.ReceivedConnection==received && sent<=state.PeerLimit && received<=state.ReceiveLimit);
		}
		done=true;
		for(uint32_t side=0;side<2;++side)for(size_t k=0;k<MULTI_COUNT;++k) {
			if(!net.Active[side][k]) { done=false;continue; }
			xrudpstreamstate stream;CHECK(xrtRudpSessionStreamState(net.Session[side],ids[k],&stream)==XRUDP_OK);
			if(!stream.WriteDone || stream.References || !net.Eof[side][k])done=false;
			if(stream.Receive.Stopped && !stream.Receive.HasFinal)done=false;
		}
	}
	CHECK(done && otherProgress && net.Dropped && net.Blocked && net.Duplicated && net.Deferred);
#ifdef TEST_MULTI_STOP
	xrudpstreamstate sender,receiver;CHECK(net.ReceiveStopped && net.PeerStopped);
	CHECK(xrtRudpSessionStreamState(net.Session[0],3,&sender)==XRUDP_OK && xrtRudpSessionStreamState(net.Session[1],3,&receiver)==XRUDP_OK);
	CHECK(sender.ResetAcknowledged && receiver.Receive.Reset && sender.ResetFinal==receiver.Receive.FinalOffset && sender.ResetFinal<net.Written[0][1]);
	printf("stop mixed: seed=%u cancelled-final=%llu buffered-before-stop=%zu reverse-delivered=%zu\n",seed,(unsigned long long)sender.ResetFinal,net.Written[0][1],net.Read[0][1]);
#endif
	for(uint32_t side=0;side<2;++side) {
#ifdef TEST_MULTI_PATH
		xrudpsessionstate pathState;CHECK(xrtRudpSessionState(net.Session[side],&pathState)==XRUDP_OK);
		CHECK(pathState.Path.Validations==(side?0u:1u) && pathState.Path.Active.Port==multi_address(1-side).Port);
		printf("path mixed: side=%u migrations=%llu failures=%llu unvalidated-sent=%llu received=%llu\n",side,(unsigned long long)pathState.Path.Validations,(unsigned long long)pathState.Path.Failures,(unsigned long long)pathState.Path.SentUnvalidated,(unsigned long long)pathState.Path.ReceivedUnvalidated);
#endif
#ifdef TEST_MULTI_DATAGRAMS
		xrudpsessionstate state;CHECK(xrtRudpSessionState(net.Session[side],&state)==XRUDP_OK);
		CHECK(state.Datagrams.Sent>0 && state.Datagrams.Accepted>0 && state.DatagramLost>0 && net.GameRead[side][1]>0 && net.GameRead[side][2]>0);
		printf("game mixed: side=%u sent=%llu accepted=%llu lost=%llu expired=%llu replaced=%llu\n",side,(unsigned long long)state.Datagrams.Sent,(unsigned long long)state.Datagrams.Accepted,(unsigned long long)state.DatagramLost,(unsigned long long)state.Datagrams.SendExpired,(unsigned long long)state.Datagrams.SendReplaced);
#endif
		for(size_t k=1;k<MULTI_COUNT;++k) {
			CHECK(xrtRudpSessionStreamRetire(net.Session[side],ids[k])==XRUDP_OK);
			xrudpstreamstate stream;CHECK(xrtRudpSessionStreamState(net.Session[side],ids[k],&stream)==XRUDP_CLOSED);
		}
		CHECK(xrtRudpSessionStreamRetire(net.Session[side],1)==XRUDP_ARGUMENT);
		xrtRudpSessionDestroy(net.Session[side]);net.Session[side]=NULL;
	}
#ifdef TEST_MULTI_MEMORY
	CHECK(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
#endif
	printf("xrudp multistream: seed=%u ticks=%u deferred=%u loss=%u duplicate=%u admission-pressure=%u\n",seed,tick,net.Deferred,net.Dropped,net.Duplicated,net.Blocked);
	return 0;
}
int main(void)
{
	CHECK(scenario(0x8734bc17)==0);CHECK(scenario(0x1357a9cf)==0);return 0;
}
