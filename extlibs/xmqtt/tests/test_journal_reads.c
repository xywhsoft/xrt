#include "support/journal_peer.h"
/* Independent failed-read provider; preserve the existing successful/native
 * peer so earlier forced-kill binaries retain exactly the same source inputs. */
static unsigned ReadCalls,FailAt;static size_t Partial;static jpdrive Drive,Base;
static xvfsfileops_v1 ReadOps;
static bool jrRead(void* State,uint64 At,void* B,size_t N,size_t* Done)
{
    bool OK=++ReadCalls!=FailAt;if(!OK&&N>Partial)N=Partial;
    bool Result=jpRead(State,At,B,N,Done);return OK&&Result;
}
static xvfslookup jrOpen(void* C,xvfscase Case,xstrview Path,const xfileoptions* O,xvfsfile_v1* F)
{xvfslookup Result=jpOpen(C,Case,Path,O,F);if(Result==XVFS_LOOKUP_OPENED)F->Ops=&ReadOps;return Result;}
static jpfile jrFile(void)
{
    ReadOps=jpOps;ReadOps.ReadAt=jrRead;xvfsprovider_v1 Provider=jpProvider;Provider.Open=jrOpen;
    jpfile F={0};F.Vfs=xrtVfsCreate();testRequire(F.Vfs!=NULL,"read-failure namespace");F.Mount=xrtVfsMount(F.Vfs,"/",0,XVFS_CASE_SENSITIVE,&Provider,&Drive,0);testRequire(F.Mount!=NULL,"read-failure public mount");
    xfileoptions O;xrtFileOptionsInit(&O);O.Flags=XFILE_READ|XFILE_WRITE;F.File=xrtVfsOpen(F.Vfs,"/journal",&O);testRequire(F.File!=NULL,"read-failure public file");return F;
}
int main(void)
{
    jpfile F=jrFile();xmqttjournalconfig C=jpConfig();xmqttjournal* J=jpCreate(F,&C);jpAppend(J,"first",1);jpAppend(J,"second",2);testRequire(xmqttJournalDestroy(J),"read-failure baseline released");Base=Drive;
    const size_t Partials[]={0,1,7,47,48,63,4095,4096};unsigned Cases=0;
    for(unsigned Kind=0;Kind<2;++Kind)for(unsigned Point=1;Point<=(Kind?6u:3u);++Point)
    for(unsigned P=0;P<sizeof(Partials)/sizeof(*Partials);++P){
        Drive=Base;FailAt=0;Partial=Partials[P];ReadCalls=0;
        if(Kind){J=jpReopen(F,&C);ReadCalls=0;FailAt=Point;}
        else {J=(xmqttjournal*)(uintptr_t)1;FailAt=Point;}
        xrtSetErrorInfo(XERR_STATE,"old.read.ambient",17,"cannot become a read cause");
        if(!Kind){jpStatus(xmqttJournalOpen(F.File,&C,&J),XMQTT_JOURNAL_IO,"descriptor/witness failed read");testRequire(J==(xmqttjournal*)(uintptr_t)1,"failed constructor read output atomic");}
        else {xmqttjournalrecord R={99,{0}};xmqttjournalstatus S;unsigned Seen=0;
            while(true){xmqttjournalrecord Before=R;S=xmqttJournalNext(J,&R);if(S!=XMQTT_JOURNAL_DONE){testRequire(!memcmp(&R,&Before,sizeof(R)),"failed partial replay read leaves output untouched");break;}
                testRequire(R.Sequence==++Seen,"earlier complete transaction read order");}
            jpStatus(S,XMQTT_JOURNAL_IO,"each header/payload/footer failed read");testRequire(jpInfo(J).State==XMQTT_JOURNAL_POISONED,"read failure cannot advertise recovered readiness");}
        testRequire(!xrtErrorFind(xrtGetError(),"old.read.ambient",17),"read I/O isolates stale ambient errors");
        if(Kind)testRequire(xmqttJournalDestroy(J),"failed read owner physically released");
        xrtClearError();FailAt=0;++Cases;
    }
    Drive=Base;J=jpReopen(F,&C);testRequire(jpReplay(J)==2&&xmqttJournalDestroy(J),"fresh recovery succeeds after injected reader faults");jpFileClose(F);printf("[PASS] %u descriptor/witness/header/payload/footer partial-read failures and untouched recovery outputs\n",Cases);return 0;
}
