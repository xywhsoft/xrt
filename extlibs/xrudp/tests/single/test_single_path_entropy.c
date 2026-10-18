#ifndef XRUDP_MODULE_RUDP_ENDPOINT
#define XRUDP_MODULE_RUDP_ENDPOINT
#endif
#ifndef XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_MEMORY_DEBUG
#endif
#include "../../include/xrudp/features.h"
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"

static bool entropy_fail;
static uint32_t entropy_calls;
static bool entropy_random(void* output,size_t size)
{
	++entropy_calls;
	if(entropy_fail) { memset(output,0xa5,size);return false; }
	return xrtSecureRandom(output,size);
}
/* Test-only call-site substitution after the real XRT implementation. No
 * product hook, global OS RNG replacement or weakened production randomness. */
#define xrtSecureRandom entropy_random
#define XRUDP_IMPLEMENTATION
#include "../../../../single/extlibs/xrudp.h"
#undef xrtSecureRandom
#define TEST_SESSION_FAULTS
#define main entropy_session_helpers_main
#include "../test_session.c"
#undef main

static xrudpaddress entropy_active={4,41000,0,{192,0,2,1}},entropy_candidate={4,42000,0,{192,0,2,2}};
static unsigned char entropy_packets[16][1200];
static uint32_t entropy_count;
static uint16_t entropy_last_port;
static xrudpresult entropy_output(const void* data,size_t size,const xrudpaddress* to,void* context)
{
	(void)context;if(size>1200 || entropy_count==16)return XRUDP_IO_ERROR;
	memcpy(entropy_packets[entropy_count++],data,size);entropy_last_port=to->Port;return XRUDP_OK;
}
static int entropy_case(bool passive)
{
	memset(&net,0,sizeof(net));entropy_count=entropy_calls=0;entropy_fail=false;
	xrudpsessionconfig config;configure(&config,256,512);xrudpcipher* peer;xrudpsession* session;
	CHECK(raw_session(&peer,&session,&config)==0 && xrtRudpSessionEnablePaths(session,&entropy_active,entropy_output)==XRUDP_OK);
	CHECK(xrtMemDebugFailAfter(0));entropy_fail=true;
	unsigned char ping[4]={XRUDP_FRAME_PING,0,0,0},packet[1200],zero[16]={0};size_t size=0;
	if(passive) {
		CHECK(xrtRudpCipherSeal(peer,ping,sizeof(ping),packet,sizeof(packet),&size)==XRUDP_OK);
		CHECK(xrtRudpSessionPathInput(session,&entropy_candidate,packet,size,0)==XRUDP_BACKPRESSURE);
	} else CHECK(xrtRudpSessionProbePath(session,&entropy_candidate,0)==XRUDP_IO_ERROR);
	xrudpsessionstate state;
	CHECK(entropy_calls==1 && xrtRudpSessionState(session,&state)==XRUDP_OK && state.Reason==XRUDP_OK && state.Path.Failure==XRUDP_IO_ERROR && state.Path.Failures==1 && !state.Path.Validating && state.Path.CooldownUntilUs==3000000);
	CHECK(!state.Path.Attempts && !state.Path.CandidateReceived && !state.Path.CandidateSent && !entropy_count && !session->Path.Trusted && !session->Path.Echo[1].Pending && memcmp(session->Path.Token,zero,sizeof(zero))==0);
	entropy_fail=false;
	if(passive) {
		CHECK(session->Path.UnvalidatedNumber==2);
		CHECK(xrtRudpSessionPathInput(session,&entropy_candidate,packet,size,3000000)==XRUDP_BACKPRESSURE && entropy_calls==1);
		CHECK(xrtRudpSessionState(session,&state)==XRUDP_OK && !state.Path.Validating);
		CHECK(xrtRudpCipherSeal(peer,ping,sizeof(ping),packet,sizeof(packet),&size)==XRUDP_OK && xrtRudpSessionPathInput(session,&entropy_candidate,packet,size,3001000)==XRUDP_BACKPRESSURE && entropy_calls==2);
		CHECK(xrtRudpSessionState(session,&state)==XRUDP_OK && state.Path.Validating && state.Path.Failure==XRUDP_OK && state.Path.CandidateReceived==size);
	} else {
		size_t written;uint64_t due;
		CHECK(xrtRudpSessionWrite(session,"live",4,&written)==XRUDP_OK && written==4 && xrtRudpSessionPoll(session,0,&due)==XRUDP_OK);
		CHECK(entropy_count>0 && entropy_last_port==entropy_active.Port);
		CHECK(xrtRudpSessionProbePath(session,&entropy_candidate,3000000)==XRUDP_OK && entropy_calls==2);
		CHECK(xrtRudpSessionState(session,&state)==XRUDP_OK && state.Path.Validating && state.Path.Failure==XRUDP_OK);
	}
	CHECK(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();xrtRudpSessionDestroy(session);xrtRudpCipherDestroy(peer);return 0;
}
int main(void)
{
	xmemdebugsnapshot before,after;xrtClearError();xrtMemDebugSnapshot(&before);
	CHECK(entropy_case(false)==0 && entropy_case(true)==0);
	xrtClearError();xrtMemDebugSnapshot(&after);
	CHECK(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes && before.DoubleFreeCount==after.DoubleFreeCount && before.UseAfterFreeCount==after.UseAfterFreeCount);
	puts("xrudp path entropy: partial RNG failure clears candidate, preserves active, spends passive nonce once and recovers without I/O allocation");return 0;
}
