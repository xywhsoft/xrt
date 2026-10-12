#include "../../../dev/bench/bench_common.h"
#include <xhttp-client.h>
#include "../tests/fixtures/http_origin.h"
int main(int argc,char** argv) {
 static const char Wire[]="HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nOK";
 uint32 Iterations=xbenchArgU32(argc,argv,1,100u); xnetengineconfig EngineConfig; xhttpclientconfig Config; xbenchtimer Timer;
 if (!Iterations) return 1;
 xrtNetEngineConfigInit(&EngineConfig); EngineConfig.Backend=XNET_PORT_SELECT;
 xnetengine* Engine=xrtNetEngineCreate(&EngineConfig);
 if (!Engine || !xrtNetEngineStart(Engine)) return 2;
 xrtHttpClientConfigInit(&Config); Config.Dial.Family=XNET_FAMILY_IPV4; Config.Dial.MaxAttempts=1;
 xhttpclient* Client=xrtHttpClientCreate(Engine,&Config); if (!Client) return 3;
 xbenchTimerStart(&Timer);
 for (uint32 i=0;i<Iterations;i++) {
  testhttporigin Origin; char Url[128]; testHttpOriginStart(&Origin,Engine,Wire,sizeof(Wire)-1);
  int Size=snprintf(Url,sizeof(Url),"http://127.0.0.1:%u/bench",(unsigned)testHttpOriginPort(&Origin));
  xhttpresult* Result=xrtHttpClientGetSync(Client,(xstrview){Url,(size_t)Size},NULL);
  const xhttpresponse* Response=Result ? xrtHttpResultResponse(Result) : NULL;
  if (!Response || xrtHttpResponseStatus(Response)!=200 || xrtHttpResponseBody(Response).Size!=2) return 4;
  xrtHttpResultDestroy(Result); testHttpOriginStop(&Origin);
 }
 xbenchTimerStop(&Timer);
 xbenchPrintMetricDouble("client_tcp_requests_per_sec",xbenchSafeRate(Iterations,xbenchTimerElapsedNs(&Timer)));
 xrtHttpClientDestroy(Client);
 double Deadline=__xrtWaitAfter(INT64_C(10000));
 while (!xrtNetEngineDestroy(Engine)) { xrtClearError(); if (__xrtWaitExpired(Deadline)) return 5; xrtThreadYield(); }
 return 0;
}
