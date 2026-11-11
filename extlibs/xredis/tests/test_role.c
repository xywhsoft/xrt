#include "test.h"
#include <xredis/role.h>
#include <xrt/memory.h>
#if defined(XRT_FEATURE_THREAD)
#include <xrt/thread.h>
#endif
#include <string.h>
static xredisreply* roleParse(const void* Data,size_t Bytes,size_t Fragment)
{
    xredisreplyreader* Reader=NULL;xredisreply* R=NULL;testRequire(xredisReplyReaderCreate(NULL,&Reader),"role reader");size_t Offset=0;
    while(Offset<Bytes){size_t N=Bytes-Offset;if(N>Fragment)N=Fragment;size_t Used=0;xredisreplystatus S=xredisReplyReaderFeed(Reader,(xbytesview){(cbytes)Data+Offset,N},&Used,&R);
        testRequire(S!=XREDIS_REPLY_ERROR && (Used || S==XREDIS_REPLY_YIELD),"role frame fragmentation");Offset+=Used;if(S==XREDIS_REPLY_READY){testRequire(Offset==Bytes,"single role frame");break;}}
    testRequire(R && xredisReplyReaderFinish(Reader),"complete role frame");xredisReplyReaderDestroy(Reader);return R;
}
#define ROLE_MASTER "*3\r\n+master\r\n:9\r\n*2\r\n*3\r\n+a\r\n+1\r\n+2\r\n*3\r\n+b\r\n:3\r\n:4\r\n"
static void roleShapes(void)
{
    struct {cstr Wire;xredisrolestatus Status;xredisrolekind Kind;int64 Offset;size_t Entries;} Cases[]={
        {ROLE_MASTER,XREDIS_ROLE_VALUE,XREDIS_ROLE_MASTER,9,2},
        {"*3\r\n$6\r\nmaster\r\n:9223372036854775807\r\n*1\r\n*3\r\n+h\r\n$5\r\n65535\r\n$19\r\n9223372036854775807\r\n",XREDIS_ROLE_VALUE,XREDIS_ROLE_MASTER,INT64_MAX,1},
        {"*3\r\n+master\r\n:0\r\n*0\r\n",XREDIS_ROLE_VALUE,XREDIS_ROLE_MASTER,0,0},
        {"|1\r\n+k\r\n+v\r\n*?\r\n+master\r\n:0\r\n*?\r\n*3\r\n+h\r\n+00000000000000000000001\r\n+0\r\n.\r\n.\r\n",XREDIS_ROLE_VALUE,XREDIS_ROLE_MASTER,0,1},
        {"*5\r\n+slave\r\n+h\r\n:0\r\n+connect\r\n:-1\r\n",XREDIS_ROLE_VALUE,XREDIS_ROLE_REPLICA,-1,0},
        {"*5\r\n+slave\r\n+h\r\n:65535\r\n+connected\r\n:9223372036854775807\r\n",XREDIS_ROLE_VALUE,XREDIS_ROLE_REPLICA,INT64_MAX,0},
        {"*5\r\n+slave\r\n$0\r\n\r\n:1\r\n+future-state\r\n:0\r\n",XREDIS_ROLE_VALUE,XREDIS_ROLE_REPLICA,0,0},
        {"*2\r\n+sentinel\r\n*2\r\n+alpha\r\n$0\r\n\r\n",XREDIS_ROLE_VALUE,XREDIS_ROLE_SENTINEL,0,2},
        {"*2\r\n+sentinel\r\n*0\r\n",XREDIS_ROLE_VALUE,XREDIS_ROLE_SENTINEL,0,0},
        {"*2\r\n+master\r\n:0\r\n",XREDIS_ROLE_MALFORMED,0,0,0},
        {"*3\r\n+master\r\n:-1\r\n*0\r\n",XREDIS_ROLE_MALFORMED,0,0,0},
        {"*3\r\n+master\r\n:0\r\n*-1\r\n",XREDIS_ROLE_MALFORMED,0,0,0},
        {"*3\r\n+master\r\n:0\r\n*1\r\n*2\r\n+h\r\n+1\r\n",XREDIS_ROLE_MALFORMED,0,0,0},
        {"*3\r\n+master\r\n:0\r\n*1\r\n*3\r\n_\r\n+1\r\n+2\r\n",XREDIS_ROLE_MALFORMED,0,0,0},
        {"*3\r\n+master\r\n:0\r\n*1\r\n*3\r\n+h\r\n+65536\r\n+2\r\n",XREDIS_ROLE_MALFORMED,0,0,0},
        {"*3\r\n+master\r\n:0\r\n*1\r\n*3\r\n+h\r\n+-1\r\n+2\r\n",XREDIS_ROLE_MALFORMED,0,0,0},
        {"*3\r\n+master\r\n:0\r\n*1\r\n*3\r\n+h\r\n+1\r\n+9223372036854775808\r\n",XREDIS_ROLE_MALFORMED,0,0,0},
        {"*3\r\n+master\r\n:0\r\n*1\r\n*3\r\n+h\r\n+1x\r\n+2\r\n",XREDIS_ROLE_MALFORMED,0,0,0},
        {"*5\r\n+slave\r\n+h\r\n+1\r\n+sync\r\n:0\r\n",XREDIS_ROLE_MALFORMED,0,0,0},
        {"*5\r\n+slave\r\n+h\r\n:65536\r\n+sync\r\n:0\r\n",XREDIS_ROLE_MALFORMED,0,0,0},
        {"*5\r\n+slave\r\n+h\r\n:1\r\n_\r\n:0\r\n",XREDIS_ROLE_MALFORMED,0,0,0},
        {"*5\r\n+slave\r\n+h\r\n:1\r\n+sync\r\n:-2\r\n",XREDIS_ROLE_MALFORMED,0,0,0},
        {"*2\r\n+sentinel\r\n*1\r\n:1\r\n",XREDIS_ROLE_MALFORMED,0,0,0},
        {"*2\r\n+sentinel\r\n_\r\n",XREDIS_ROLE_MALFORMED,0,0,0},
        {"*1\r\n+unknown\r\n",XREDIS_ROLE_OTHER,0,0,0},{"*1\r\n$-1\r\n",XREDIS_ROLE_OTHER,0,0,0},
        {"+master\r\n",XREDIS_ROLE_OTHER,0,0,0},{"*0\r\n",XREDIS_ROLE_OTHER,0,0,0},{"*-1\r\n",XREDIS_ROLE_OTHER,0,0,0},
        {">3\r\n+master\r\n:0\r\n*0\r\n",XREDIS_ROLE_OTHER,0,0,0},
        {"-ERR denied\r\n",XREDIS_ROLE_SERVER_ERROR,0,0,0},{"!3\r\nbad\r\n",XREDIS_ROLE_SERVER_ERROR,0,0,0}
    };
    for(size_t N=0;N<sizeof(Cases)/sizeof(*Cases);++N){size_t Bytes=strlen(Cases[N].Wire);
        for(size_t Fragment=1;Fragment<=Bytes;++Fragment){xredisreply* R=roleParse(Cases[N].Wire,Bytes,Fragment);xredisroleinfo I;memset(&I,0x5a,sizeof(I));unsigned char Saved[sizeof(I)];memcpy(Saved,&I,sizeof(I));
            xrtSetErrorKind(XERR_MEMORY);const xerror* Error=xrtGetError();xredisrolestatus S=xredisRoleRead(R,NULL,&I);testRequire(S==Cases[N].Status && xrtGetError()==Error,"role classification and ambient error");
            if(S==XREDIS_ROLE_VALUE){testRequire(I.Kind==Cases[N].Kind && I.Offset==Cases[N].Offset,"role identity and offset");size_t Count=0;
                if(I.Kind==XREDIS_ROLE_MASTER){xredisrolereplica Peer;while(xredisRoleReplicaNext(&I.Replicas,&Peer)){testRequire(Peer.Host.Size==1,"peer host");if(N==1)testRequire(Peer.Port==65535 && Peer.Offset==INT64_MAX,"full integer precision");++Count;}testRequire(!I.Replicas.Remaining,"replica cursor exhaustion");}
                if(I.Kind==XREDIS_ROLE_SENTINEL){xbytesview Name;while(xredisRoleMasterNext(&I.Monitored,&Name))++Count;testRequire(!I.Monitored.Remaining,"monitor cursor exhaustion");}
                testRequire(Count==Cases[N].Entries && xrtGetError()==Error,"complete list and ambient error");
            }else testRequire(!memcmp(&I,Saved,sizeof(I)),"non-value output remains untouched");
            xrtClearError();xredisReplyRelease(R);
        }
    }
    const cstr States[]={"connect","connecting","sync","connected","future"};
    for(unsigned N=0;N<5;++N){char Wire[160];int Bytes=snprintf(Wire,sizeof(Wire),"*5\r\n+slave\r\n+h\r\n:1\r\n+%s\r\n:-1\r\n",States[N]);xredisreply* R=roleParse(Wire,(size_t)Bytes,1);xredisroleinfo I;testRequire(xredisRoleRead(R,NULL,&I)==XREDIS_ROLE_VALUE && I.Link==(N<4?(xredisrolelink)(N+1):XREDIS_ROLE_LINK_OTHER) && I.LinkText.Size==strlen(States[N]),"known and future link text");xredisReplyRelease(R);}
}
static void roleOwnership(void)
{
    static const char Wire[]="|1\r\n+k\r\n+v\r\n*2\r\n+sentinel\r\n*2\r\n$3\r\na\0b\r\n$0\r\n\r\n";
    char Input[sizeof(Wire)];memcpy(Input,Wire,sizeof(Wire));xredisreply* R=roleParse(Input,sizeof(Wire)-1,1);memset(Input,0xcc,sizeof(Input));xredisroleinfo I;xredisrolelimits K;testRequire(xredisRoleLimitsInit(&K) && K.MaxEntries==4096 && K.MaxPayloadBytes==1048576,"default limits");
    xredisreplyinfo RI={0};RI.Size=sizeof(RI);testRequire(xredisReplyInfo(R,&RI),"role owner info");K.MaxEntries=2;K.MaxPayloadBytes=RI.PayloadBytes;testRequire(xredisRoleRead(R,&K,&I)==XREDIS_ROLE_VALUE,"exact limits including attributes");
    xbytesview Name;testRequire(xredisRoleMasterNext(&I.Monitored,&Name) && Name.Size==3 && !memcmp(Name.Data,"a\0b",3),"borrowed binary name is owned by reply");
    testRequire(xredisRoleRead(R,NULL,(xredisroleinfo*)Name.Data)==XREDIS_ROLE_ERROR,"reply payload cannot be an output");xrtClearError();testRequire(xredisRoleRead(R,NULL,(xredisroleinfo*)R)==XREDIS_ROLE_ERROR,"reply header cannot be output");xrtClearError();
    unsigned char Saved[sizeof(I)];memcpy(Saved,&I,sizeof(I));xrtSetErrorKind(XERR_MEMORY);const xerror* Error=xrtGetError();--K.MaxPayloadBytes;testRequire(xredisRoleRead(R,&K,&I)==XREDIS_ROLE_LIMIT && !memcmp(&I,Saved,sizeof(I)) && xrtGetError()==Error,"payload cap includes attribute bytes and is atomic");++K.MaxPayloadBytes;K.MaxEntries=1;testRequire(xredisRoleRead(R,&K,&I)==XREDIS_ROLE_LIMIT && !memcmp(&I,Saved,sizeof(I)),"entry cap is atomic");xrtClearError();
    testRequire(xredisReplyRetain(R),"escape reply ownership");xredisReplyRelease(R);testRequire(xredisRoleMasterNext(&I.Monitored,&Name) && !Name.Size,"retained cursor after original owner release");
    xbytesview Before=Name;xredisrolecursor C=I.Monitored;xrtSetErrorKind(XERR_MEMORY);Error=xrtGetError();testRequire(!xredisRoleMasterNext(&C,&Name) && !memcmp(&Name,&Before,sizeof(Name)) && xrtGetError()==Error,"exhaustion preserves output and error");xrtClearError();xredisReplyRelease(R);
    R=roleParse(ROLE_MASTER,strlen(ROLE_MASTER),1);testRequire(xredisRoleRead(R,NULL,&I)==XREDIS_ROLE_VALUE,"master cursors");C=I.Replicas;unsigned char Old[sizeof(C)];memcpy(Old,&C,sizeof(C));
    testRequire(!xredisRoleReplicaNext(&C,(xredisrolereplica*)&C) && !memcmp(&C,Old,sizeof(C)),"cursor/output overlap is refused atomically");xrtClearError();
    testRequire(!xredisRoleReplicaNext(&C,(xredisrolereplica*)R),"iterator output cannot alter source reply");xrtClearError();C.Next=SIZE_MAX;xredisrolereplica Peer;testRequire(!xredisRoleReplicaNext(&C,&Peer),"bad cursor index");xrtClearError();
    union {xredisrolelimits K;xredisroleinfo I;} Alias;Alias.K=(xredisrolelimits){sizeof(K),2,100};testRequire(xredisRoleRead(R,&Alias.K,&Alias.I)==XREDIS_ROLE_ERROR,"config/output alias");xrtClearError();K.Size=0;testRequire(xredisRoleRead(R,&K,&I)==XREDIS_ROLE_ERROR,"invalid limit version");xrtClearError();
    testRequire(xredisRoleRead(R,(const xredisrolelimits*)(uintptr_t)(UINTPTR_MAX-1),&I)==XREDIS_ROLE_ERROR,"wrapped limits span");xrtClearError();testRequire(!xredisRoleLimitsInit(NULL),"null limits output");xrtClearError();
    testRequire(xredisRoleLimitsInit(&K),"reset limits");K.MaxEntries=0;testRequire(xredisRoleRead(R,&K,&I)==XREDIS_ROLE_LIMIT,"zero entries is a cap");K.MaxEntries=2;K.MaxPayloadBytes=0;testRequire(xredisRoleRead(R,&K,&I)==XREDIS_ROLE_LIMIT,"zero payload is a cap");
    xredisreply* Empty=roleParse("*3\r\n+master\r\n:0\r\n*0\r\n",strlen("*3\r\n+master\r\n:0\r\n*0\r\n"),1);K.MaxEntries=0;K.MaxPayloadBytes=100;testRequire(xredisRoleRead(Empty,&K,&I)==XREDIS_ROLE_VALUE,"empty peer list fits zero entry cap");xredisReplyRelease(Empty);
    xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&B) && xrtAllocationBudgetEnter(B,&Scope),"one-byte decode budget");
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats A,Z;xrtMemStatsEnable(true);xrtMemStatsGet(&A);
#endif
    for(size_t N=0;N<4096;++N){testRequire(xredisRoleRead(R,NULL,&I)==XREDIS_ROLE_VALUE,"allocation-free validation");while(xredisRoleReplicaNext(&I.Replicas,&Peer)){} }
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&Z);xrtMemStatsEnable(false);testRequire(A.MallocCalls==Z.MallocCalls && A.CallocCalls==Z.CallocCalls && A.ReallocCalls==Z.ReallocCalls && A.BlockAllocCalls==Z.BlockAllocCalls,"no allocator calls in views/iterators");
#endif
    testRequire(xrtAllocationBudgetLeave(&Scope),"budget leave");xallocationbudgetinfo BI={0};BI.Size=sizeof(BI);testRequire(xrtAllocationBudgetInfo(B,&BI) && !BI.LiveBytes && !BI.LiveCount,"no outstanding decode allocation");xrtAllocationBudgetFree(B);xredisReplyRelease(R);
}
#if defined(XRT_FEATURE_THREAD)
typedef struct rolethread {xredisreply* Reply;uint64 Sum;} rolethread;
static int32 roleThread(ptr User)
{rolethread* T=User;for(unsigned N=0;N<2000;++N){xredisroleinfo I;if(xredisRoleRead(T->Reply,NULL,&I)!=XREDIS_ROLE_VALUE)return 1;T->Sum+=(uint64)I.Offset;xredisrolereplica P;while(xredisRoleReplicaNext(&I.Replicas,&P))T->Sum+=P.Port+(uint64)P.Offset+P.Host.Size;}xredisReplyRelease(T->Reply);return 0;}
static void roleThreads(void)
{xredisreply* R=roleParse(ROLE_MASTER,strlen(ROLE_MASTER),1);rolethread T[4]={{0}};xthread* Threads[4];for(unsigned N=0;N<4;++N){testRequire(xredisReplyRetain(R),"thread reference");T[N].Reply=R;Threads[N]=xrtThreadCreate(roleThread,T+N,0);testRequire(Threads[N]!=NULL,"role reader thread");}xredisReplyRelease(R);for(unsigned N=0;N<4;++N){testRequire(xrtThreadWait(Threads[N])==XWAIT_OK && xrtThreadExitCode(Threads[N])==0 && T[N].Sum==42000,"independent concurrent role cursors");xrtThreadDestroy(Threads[N]);}}
#endif
int main(void)
{roleShapes();roleOwnership();
#if defined(XRT_FEATURE_THREAD)
roleThreads();
#endif
puts("[PASS] ROLE: complete shapes, decimals, limits, streams/attributes, all fragments, binary ownership, aliases, quota/zero-heap and concurrent cursors");return 0;}
