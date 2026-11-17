#include "support/store_state_peer.h"
static jpdrive Drive;
static unsigned Notices;
static void stNotice(const xerror* E,ptr P){(void)E;(void)P;++Notices;}
static void stCuts(xmqttversion V,bool Split)
{
    static const unsigned Outs[]={0,0,0,1,1,2,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    static const unsigned Ins[] ={0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0};
    static const unsigned Inbox[]={0,0,0,0,0,0,0,0,0,1,2,2,2,2,2,2,3,2,1,0,0,0};
    for(size_t Cut=0;Cut<=ST_COUNT;++Cut){
        memset(&Drive,0,sizeof(Drive));jpfile F=jpFile(&Drive);stsource Source;stSource(V,&Source);xmqttjournal* J=stWrite(F,V,Source.Ops,Cut,Split);xmqttstorestateconfig C=stConfig(V);xmqttstorestate* S=NULL;
        xrtSetErrorInfo(XERR_AGAIN,"state.caller",1,"exact previous owner");const xerror* Ambient=xrtGetError();Notices=0;xrtSetErrorHandler(stNotice,NULL);
        stStatus(xmqttStoreStateRecover(J,&C,&S),XMQTT_STORE_DONE,"whole committed cut recovered");testRequire(xrtGetError()==Ambient&&!Notices,"successful replay preserves exact error without notices");xrtSetErrorHandler(NULL,NULL);xrtClearError();
        xmqttstorestateinfo I=stInfo(S);testRequire(I.Outgoing==Outs[Cut]&&I.Incoming==Ins[Cut]&&I.Inbox==Inbox[Cut]&&I.Operations==Cut&&I.Transactions==(Split?Cut:Cut?1u:0u)&&I.MemoryBytes<=I.PeakMemoryBytes&&I.PeakMemoryBytes<=C.MaxMemoryBytes,"independent per-cut state/count/memory oracle");
        if(Cut)testRequire(I.ClientId.Size==12&&!memcmp(I.ClientId.Data,"delta-client",12),"bound immutable ClientId");
        if(Cut>=6&&Cut<7)testRequire(stView(S,XMQTT_STORE_OUTGOING,1).Pubrel&&stView(S,XMQTT_STORE_OUTGOING,1).Attempted&&stView(S,XMQTT_STORE_OUTGOING,1).Sent,"QoS2 PUBREL never reverts to PUBLISH");
        if(Cut==11)testRequire(stView(S,XMQTT_STORE_INCOMING,0).Pubrel,"received PUBREL retained");
        if(Cut>=14&&Cut<=18)testRequire(I.Inbox&&stView(S,XMQTT_STORE_INBOX,0).Epoch==(Cut<=16?1u:Cut==17?1u:2u),"accepted Inbox survives protocol reset and old epoch ACKs correlate only old keys");
        xmqttstoreview Sentinel,Keep;memset(&Sentinel,0x5a,sizeof(Sentinel));Keep=Sentinel;stStatus(xmqttStoreStateView(S,XMQTT_STORE_INBOX,I.Inbox,&Sentinel),XMQTT_STORE_EMPTY,"missing view");testRequire(!memcmp(&Sentinel,&Keep,sizeof(Keep)),"EMPTY does not publish a view");
        memset(&Source,0xcc,sizeof(Source));testRequire(xmqttJournalDestroy(J),"journal released before retained views");jpFileClose(F);memset(&Drive,0xcc,sizeof(Drive));
        if(I.Inbox){xmqttstoreview R=stView(S,XMQTT_STORE_INBOX,0);testRequire(R.Wire.Size&&R.Wire.Data[0]>>4==3,"wire owned independently of overwritten Journal/file/source");}
        xmqttStoreStateDestroy(S);
    }
}
static void stExpiry(xmqttversion V)
{
    for(unsigned Kind=0;Kind<4;++Kind){memset(&Drive,0,sizeof(Drive));jpfile F=jpFile(&Drive);stsource Source;stSource(V,&Source);xmqttstoredelta Ops[12];memcpy(Ops,Source.Ops,9*sizeof(*Ops));size_t Count=9;
        if(Kind){Ops[Count++]=(xmqttstoredelta){.Type=XMQTT_STORE_END,.Epoch=1,.WallMilliseconds=2000,.Expiry=V==XMQTT_VERSION_5?60:UINT32_MAX};}
        if(Kind==3&&V==XMQTT_VERSION_5){Ops[Count-1].Flags=1;Ops[Count-1].Expiry=0;}
        xmqttjournal* J=stWrite(F,V,Ops,Count,false);xmqttstorestateconfig C=stConfig(V);C.WallMilliseconds=Kind==1?61999:62000;xmqttstorestate* S=NULL;stStatus(xmqttStoreStateRecover(J,&C,&S),XMQTT_STORE_DONE,"expiry cut recovered");xmqttstorestateinfo I=stInfo(S);
        bool Keep=V==XMQTT_VERSION_311||Kind<2;testRequire(I.Inbox==1&&I.Incoming==(Keep?1u:0u)&&I.HasSession==Keep,"expiry clears only protocol identities, never accepted Inbox");
        if(!Kind)testRequire(I.LiveAtCut&&I.RemainingMilliseconds==UINT64_MAX,"live cut never guesses an actual disconnect/expiry start");
        if(Kind==1&&V==XMQTT_VERSION_5)testRequire(!I.LiveAtCut&&I.RemainingMilliseconds==1,"closed expiry charges downtime at exact millisecond boundary");
        testRequire(xmqttJournalDestroy(J),"expiry journal released");xmqttStoreStateDestroy(S);jpFileClose(F);}
}
static int stNative(int Argc,char** Argv)
{
    testRequire(Argc==4,"state native CLI");xmqttversion V=!strcmp(Argv[3],"5")?XMQTT_VERSION_5:XMQTT_VERSION_311;bool Write=!strcmp(Argv[1],"--write");
    xfile File=xrtOpen(Argv[2],XFILE_READ|XFILE_WRITE|(Write?XFILE_CREATE|XFILE_TRUNCATE:0));testRequire(File!=NULL,"native store file");xmqttjournalconfig JC=stJournalConfig(V);xmqttjournal* J=NULL;
    if(Write){jpStatus(xmqttJournalCreate(File,&JC,&J),XMQTT_JOURNAL_DONE,"native create");stsource Source;stSource(V,&Source);xmqttstoredeltaconfig DC=sdConfig(V);xmqttproperty P[4];unsigned char B[SD_BUFFER];size_t N;
        sdStatus(xmqttStoreDeltaWrite(&DC,Source.Ops,16,P,4,B,sizeof(B),&N),XMQTT_STORE_DELTA_DONE,"native pending Inbox history");uint64 Seq=0;jpStatus(xmqttJournalAppend(J,(xbytesview){B,N},&Seq),XMQTT_JOURNAL_DONE,"native durable prefix");testRequire(xmqttJournalDestroy(J),"native writer released");}
    else testRequire(!strcmp(Argv[1],"--read"),"native read mode");
    J=NULL;jpStatus(xmqttJournalOpen(File,&JC,&J),XMQTT_JOURNAL_DONE,"native recovery open");xmqttstorestateconfig C=stConfig(V);xmqttstorestate* S=NULL;stStatus(xmqttStoreStateRecover(J,&C,&S),XMQTT_STORE_DONE,"native whole history recovery");testRequire(stInfo(S).Inbox==3&&stView(S,XMQTT_STORE_INBOX,0).Epoch==1&&stView(S,XMQTT_STORE_INBOX,2).Epoch==2,"actual recovered backlog crosses protocol reset");
    testRequire(xmqttJournalDestroy(J)&&xrtClose(File),"native journal/file physically released");testRequire(stView(S,XMQTT_STORE_INBOX,0).Wire.Data[0]>>4==3,"retained payload after actual native close");xmqttStoreStateDestroy(S);puts("[PASS] native committed MQTT recovery and independent Inbox payloads");return 0;
}
int main(int Argc,char** Argv)
{if(Argc>1)return stNative(Argc,Argv);for(unsigned V=0;V<2;++V){xmqttversion Version=V?XMQTT_VERSION_5:XMQTT_VERSION_311;stCuts(Version,false);stCuts(Version,true);stExpiry(Version);}puts("[PASS]88 whole-prefix transaction cuts,8 expiry cuts,ordered QoS/Inbox/ownership and quiet errors");return 0;}
