#include "../common/journal_files.h"
#include <xmqtt/store_state.h>
#include <xrt/memory.h>
#include <stdio.h>
#include <string.h>

/* One APP-owned FIFO per original journal namespace. Keys never reset within it. */
enum {InboxRecordHeader=17,InboxCapacity=1024,InboxMaxBytes=16u*1024u*1024u};
enum {InboxPut=1,InboxAck=2};
#define INBOX_SCHEMA UINT32_C(0x58494201)
typedef struct inboxitem {
    uint64 Epoch,Key;
    xbytesview Wire;
} inboxitem;
typedef struct appinbox {
    inboxitem Items[InboxCapacity];
    size_t Head,Count,Bytes;
    uint64 LastImported;
} appinbox;

static uint64 readKey(const unsigned char* Data)
{
    uint64 Key=0;
    for(unsigned I=0;I<8;++I)Key=(Key<<8)|Data[I];
    return Key;
}

static void writeKey(unsigned char* Data,uint64 Key)
{
    for(unsigned I=8;I>0;--I){Data[I-1]=(unsigned char)Key;Key>>=8;}
}

static void removeHead(appinbox* Inbox)
{
    inboxitem* Item=&Inbox->Items[Inbox->Head];
    Inbox->Bytes-=Item->Wire.Size;
    xrtFree((ptr)Item->Wire.Data);
    Inbox->Head=(Inbox->Head+1)%InboxCapacity;
    --Inbox->Count;
}

static bool addPending(appinbox* Inbox,uint64 Epoch,uint64 Key,xbytesview Wire)
{
    if(!Epoch||Key<=Inbox->LastImported||!Wire.Size||
       Inbox->Count==InboxCapacity||Wire.Size>InboxMaxBytes-Inbox->Bytes)return false;
    unsigned char* Copy=xrtMalloc(Wire.Size);
    if(!Copy)return false;
    memcpy(Copy,Wire.Data,Wire.Size);
    Inbox->Items[(Inbox->Head+Inbox->Count)%InboxCapacity]=(inboxitem){Epoch,Key,{Copy,Wire.Size}};
    ++Inbox->Count;
    Inbox->Bytes+=Wire.Size;
    Inbox->LastImported=Key;
    return true;
}

static bool recoverInbox(xmqttjournal* Journal,appinbox* Inbox)
{
    xmqttjournalrecord Record;
    xmqttjournalstatus Status;
    while((Status=xmqttJournalNext(Journal,&Record))==XMQTT_JOURNAL_DONE){
        xbytesview Data=Record.Data;
        if(Data.Size<InboxRecordHeader)return false;
        uint64 Epoch=readKey(Data.Data+1),Key=readKey(Data.Data+9);
        if(Data.Data[0]==InboxPut){
            xbytesview Wire={Data.Data+InboxRecordHeader,Data.Size-InboxRecordHeader};
            if(!addPending(Inbox,Epoch,Key,Wire))return false;
        }else {
            inboxitem* Item=&Inbox->Items[Inbox->Head];
            if(Data.Data[0]!=InboxAck||Data.Size!=InboxRecordHeader||
               !Inbox->Count||Item->Epoch!=Epoch||Item->Key!=Key)return false;
            removeHead(Inbox);
        }
    }
    return Status==XMQTT_JOURNAL_EMPTY;
}

static bool appendInbox(xmqttjournal* Journal,unsigned Kind,inboxitem Item)
{
    unsigned char Header[InboxRecordHeader];
    Header[0]=(unsigned char)Kind;
    writeKey(Header+1,Item.Epoch);
    writeKey(Header+9,Item.Key);
    uint64 Sequence;
    if(Kind==InboxAck)
        return xmqttJournalAppend(Journal,(xbytesview){Header,sizeof(Header)},&Sequence)==XMQTT_JOURNAL_DONE;
    size_t Bytes=sizeof(Header)+Item.Wire.Size;
    unsigned char* Record=xrtMalloc(Bytes);
    if(!Record)return false;
    memcpy(Record,Header,sizeof(Header));
    memcpy(Record+sizeof(Header),Item.Wire.Data,Item.Wire.Size);
    bool OK=xmqttJournalAppend(Journal,(xbytesview){Record,Bytes},&Sequence)==XMQTT_JOURNAL_DONE;
    xrtFree(Record);
    return OK;
}

static bool migrate(xmqttstorestate* Legacy,xmqttjournal* Journal,appinbox* Inbox)
{
    xmqttstorestateinfo Info;
    if(!xmqttStoreStateInfo(Legacy,&Info))return false;
    size_t Imported=0;
    for(size_t I=0;I<Info.Inbox;++I){
        xmqttstoreview Delivery;
        if(xmqttStoreStateView(Legacy,XMQTT_STORE_INBOX,I,&Delivery)!=XMQTT_STORE_DONE)return false;
        if(Delivery.Key<=Inbox->LastImported)continue;
        if(!addPending(Inbox,Delivery.Epoch,Delivery.Key,Delivery.Wire)||
           !appendInbox(Journal,InboxPut,(inboxitem){Delivery.Epoch,Delivery.Key,Delivery.Wire}))return false;
        ++Imported;
    }
    printf("Imported %zu, pending %zu.\n",Imported,Inbox->Count);
    return true;
}

static bool consume(xmqttjournal* Journal,appinbox* Inbox)
{
    while(Inbox->Count){
        inboxitem Item=Inbox->Items[Inbox->Head];
        /* The APP's effect uses the original epoch/key for idempotency. */
        if(printf("Processing %llu/%llu (%zu bytes, last byte %02x).\n",
                  (unsigned long long)Item.Epoch,(unsigned long long)Item.Key,Item.Wire.Size,
                  (unsigned)Item.Wire.Data[Item.Wire.Size-1])<0||fflush(stdout)!=0||
           !appendInbox(Journal,InboxAck,Item))return false;
        removeHead(Inbox);
    }
    puts("No pending deliveries.");
    return true;
}

static bool seed(xmqttjournal* Legacy,const xmqttstorestateconfig* Config)
{
    const unsigned char Wire5[][7]={{0x30,5,0,1,'t',0,'a'},{0x30,5,0,1,'t',0,'b'},{0x30,5,0,1,'t',0,'c'}};
    const unsigned char Wire311[][6]={{0x30,4,0,1,'t','a'},{0x30,4,0,1,'t','b'},{0x30,4,0,1,'t','c'}};
    xmqttstoredelta Ops[]={
        {.Type=XMQTT_STORE_BIND,.Epoch=1,.WallMilliseconds=1,.Data=XRT_BYTES_LITERAL("inbox-demo")},
        {.Type=XMQTT_STORE_OPEN,.Epoch=1,.WallMilliseconds=2,.Expiry=Config->Delta.Version==XMQTT_VERSION_5?60:UINT32_MAX},
        {.Type=XMQTT_STORE_IN_ACCEPT,.Epoch=1,.Key=1},
        {.Type=XMQTT_STORE_IN_ACCEPT,.Epoch=1,.Key=2},
        {.Type=XMQTT_STORE_IN_ACCEPT,.Epoch=1,.Key=3},
        {.Type=XMQTT_STORE_INBOX_ACK,.Epoch=1,.Key=2}
    };
    for(size_t I=0;I<3;++I){
        Ops[I+2].Data=Config->Delta.Version==XMQTT_VERSION_5?
            (xbytesview){Wire5[I],sizeof(Wire5[I])}:(xbytesview){Wire311[I],sizeof(Wire311[I])};
    }
    unsigned char Record[512];
    size_t Bytes;
    uint64 Sequence;
    return xmqttStoreDeltaWrite(&Config->Delta,Ops,sizeof(Ops)/sizeof(Ops[0]),
        NULL,0,Record,sizeof(Record),&Bytes)==XMQTT_STORE_DELTA_DONE&&
        xmqttJournalAppend(Legacy,(xbytesview){Record,Bytes},&Sequence)==XMQTT_JOURNAL_DONE;
}

int main(int Argc,char** Argv)
{
    if(Argc!=4){
        puts("Usage: examples_store_inbox_main seed|migrate|consume DIRECTORY 5|311");
        return Argc==1?0:2;
    }
    bool Seed=!strcmp(Argv[1],"seed"),Migrate=!strcmp(Argv[1],"migrate");
    bool V5=!strcmp(Argv[3],"5");
    if((!Seed&&!Migrate&&strcmp(Argv[1],"consume"))||(!V5&&strcmp(Argv[3],"311")))return 2;
    xroot Root=xrtRootOpen(Argv[2]);
    if(!Root)return 1;

    xmqttjournalconfig LegacyConfig;
    xmqttJournalConfigInit(&LegacyConfig);
    LegacyConfig.Version=V5?XMQTT_VERSION_5:XMQTT_VERSION_311;
    LegacyConfig.Binding=XRT_BYTES_LITERAL("demo/broker/inbox-demo");
    LegacyConfig.Generation=1;
    LegacyConfig.PayloadSchema=XMQTT_STORE_DELTA_SCHEMA;
    xmqttjournalconfig InboxConfig=LegacyConfig;
    InboxConfig.PayloadSchema=INBOX_SCHEMA;
    InboxConfig.MaxTransactionBytes+=InboxRecordHeader;

    xmqttstorestateconfig StateConfig;
    xmqttStoreStateConfigInit(&StateConfig);
    StateConfig.Delta.Version=LegacyConfig.Version;
    StateConfig.Binding=LegacyConfig.Binding;
    StateConfig.Generation=LegacyConfig.Generation;
    StateConfig.ClientId=XRT_STR_LITERAL("inbox-demo");
    StateConfig.WallMilliseconds=1000;

    xfile LegacyFile=NULL,InboxFile=NULL;
    xfileinfo LegacyInfo,InboxInfo;
    xmqttjournal* LegacyJournal=NULL;
    xmqttjournal* InboxJournal=NULL;
    xmqttstorestate* Legacy=NULL;
    appinbox Inbox={0};
    bool OK=false;
    if(Seed||Migrate){
        if(mqttExampleOpenJournalFile(Root,"session.journal",&LegacyFile,&LegacyInfo)!=XMQTT_JOURNAL_DONE||
           mqttExampleLockJournalFile(LegacyFile)!=XMQTT_JOURNAL_DONE)goto Done;
        if(Seed){
            OK=xmqttJournalCreate(LegacyFile,&LegacyConfig,&LegacyJournal)==XMQTT_JOURNAL_DONE&&
               seed(LegacyJournal,&StateConfig);
            goto Done;
        }
        if(xmqttJournalOpen(LegacyFile,&LegacyConfig,&LegacyJournal)!=XMQTT_JOURNAL_DONE||
           xmqttStoreStateRecover(LegacyJournal,&StateConfig,&Legacy)!=XMQTT_STORE_DONE)goto Done;
    }
    if(mqttExampleOpenJournalFile(Root,"inbox.journal",&InboxFile,&InboxInfo)!=XMQTT_JOURNAL_DONE)goto Done;
    if(Migrate&&(!(LegacyInfo.Available&InboxInfo.Available&XFILE_INFO_IDENTITY)||
        mqttExampleSameJournalFile(&LegacyInfo,&InboxInfo)))goto Done;
    if(mqttExampleLockJournalFile(InboxFile)!=XMQTT_JOURNAL_DONE||!xrtFileStat(InboxFile,&InboxInfo))goto Done;
    if(Migrate&&!InboxInfo.Size){
        if(xmqttJournalCreate(InboxFile,&InboxConfig,&InboxJournal)!=XMQTT_JOURNAL_DONE)goto Done;
    }else if(xmqttJournalOpen(InboxFile,&InboxConfig,&InboxJournal)!=XMQTT_JOURNAL_DONE||
             !recoverInbox(InboxJournal,&Inbox))goto Done;
    OK=Migrate?migrate(Legacy,InboxJournal,&Inbox):consume(InboxJournal,&Inbox);

Done:
    while(Inbox.Count)removeHead(&Inbox);
    xmqttStoreStateDestroy(Legacy);
    if(!xmqttJournalDestroy(InboxJournal))OK=false;
    if(!xmqttJournalDestroy(LegacyJournal))OK=false;
    if(!mqttExampleCloseJournalFile(&InboxFile))OK=false;
    if(!mqttExampleCloseJournalFile(&LegacyFile))OK=false;
    if(!xrtRootClose(Root))OK=false;
    if(!OK)fputs("Inbox operation failed. Reopen both journals before retrying.\n",stderr);
    return OK?0:1;
}
