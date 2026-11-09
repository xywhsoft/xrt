#ifndef XMQTT_SESSION_REFERENCE_PROBE
#define XMQTT_SESSION_REFERENCE_PROBE 1
/* Test-only interception, installed after Core implementation and removed
 * before test code. No hook is compiled into the production source/headers. */
static xatomic32* sessionRefProbeTarget;
static void (*sessionRefProbePressure)(void);
static void (*sessionRefProbeRefund)(void);
static uint32 sessionRefProbeAdd(xatomic32* A,uint32 V,xmemoryorder O)
{
    if(A==sessionRefProbeTarget && V==2 && sessionRefProbePressure){void (*F)(void)=sessionRefProbePressure;sessionRefProbePressure=NULL;F();}
    return (xrtAtomic32FetchAdd)(A,V,O);
}
static uint32 sessionRefProbeSub(xatomic32* A,uint32 V,xmemoryorder O)
{
    if(A==sessionRefProbeTarget && V==2 && sessionRefProbeRefund){void (*F)(void)=sessionRefProbeRefund;sessionRefProbeRefund=NULL;F();}
    return (xrtAtomic32FetchSub)(A,V,O);
}
#define xrtAtomic32FetchAdd(A,V,O) sessionRefProbeAdd(A,V,O)
#define xrtAtomic32FetchSub(A,V,O) sessionRefProbeSub(A,V,O)
#endif
