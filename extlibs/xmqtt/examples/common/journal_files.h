#ifndef XMQTT_EXAMPLE_JOURNAL_FILES_H
#define XMQTT_EXAMPLE_JOURNAL_FILES_H

#include <xmqtt/journal_slot.h>
#include <xrt/file.h>

static inline bool mqttExampleCloseJournalFile(xfile* File)
{
    xfile Owned=*File;
    *File=NULL;
    return !Owned||xrtClose(Owned);
}

static inline xmqttjournalstatus mqttExampleJournalIOResult(xmqttjournalstatus Status,xerror* Saved)
{
    if(Status<0){
        xrtErrorFree(Saved);
        if(!xrtGetError())xrtSetErrorKind(XERR_IO);
        return Status;
    }
    xerror* Discard=NULL;
    if(!xrtTrySwapErrorTake(Saved,&Discard)){
        xrtErrorFree(Saved);
        return XMQTT_JOURNAL_ERROR;
    }
    xrtErrorFree(Discard);
    return Status;
}

static inline xmqttjournalstatus mqttExampleOpenJournalFile(xroot Root,cstr Path,xfile* File,xfileinfo* Info)
{
    if(!xrtRootStat(Root,Path,false,Info))return XMQTT_JOURNAL_IO;
    if(Info->Type!=XFILE_TYPE_FILE){
        xrtSetErrorKind(XERR_ARGUMENT);
        return XMQTT_JOURNAL_ERROR;
    }
    xfileoptions Options;
    xrtFileOptionsInit(&Options);
    Options.Flags=XFILE_READ|XFILE_WRITE|XFILE_NOFOLLOW;
    Options.Share=XFILE_SHARE_READ|XFILE_SHARE_WRITE;
    *File=xrtRootFileOpen(Root,Path,&Options);
    if(!*File||!xrtFileStat(*File,Info))return XMQTT_JOURNAL_IO;
    if(Info->Type!=XFILE_TYPE_FILE){
        xrtSetErrorKind(XERR_ARGUMENT);
        return XMQTT_JOURNAL_ERROR;
    }
    return XMQTT_JOURNAL_DONE;
}

static inline xmqttjournalstatus mqttExampleLockJournalFile(xfile File)
{
    if(xrtFileTryLockExclusiveUntilClose(File))return XMQTT_JOURNAL_DONE;
    return xrtErrorKind(xrtGetError())==XERR_AGAIN?XMQTT_JOURNAL_BUSY:XMQTT_JOURNAL_IO;
}

/* The APP retains the file even after the Journal transfers to a Writer.
 * Destroy the final owning Journal or Writer before closing the file. */
static inline xmqttjournalstatus mqttExampleOpenNativeJournal(xroot Root,cstr Path,const xmqttjournalconfig* Config,
    bool Create,xfile* FileOut,xmqttjournal** Out)
{
    xerror* Saved=NULL;
    if(!xrtTrySwapErrorTake(NULL,&Saved))return XMQTT_JOURNAL_ERROR;
    xfile File=NULL;
    xfileinfo Info;
    xmqttjournal* Journal=NULL;
    xmqttjournalstatus Status=mqttExampleOpenJournalFile(Root,Path,&File,&Info);
    if(Status==XMQTT_JOURNAL_DONE)Status=mqttExampleLockJournalFile(File);
    if(Status==XMQTT_JOURNAL_DONE)
        Status=Create?xmqttJournalCreate(File,Config,&Journal):xmqttJournalOpen(File,Config,&Journal);
    if(Status!=XMQTT_JOURNAL_DONE&&!mqttExampleCloseJournalFile(&File))Status=XMQTT_JOURNAL_IO;
    Status=mqttExampleJournalIOResult(Status,Saved);
    if(Status==XMQTT_JOURNAL_DONE){
        *FileOut=File;
        *Out=Journal;
    }else if(Journal){
        (void)xmqttJournalDestroy(Journal);
        (void)mqttExampleCloseJournalFile(&File);
    }
    return Status;
}

static inline bool mqttExampleCloseNativeJournal(xmqttjournal* Journal,xfile* File)
{
    return xmqttJournalDestroy(Journal)&&mqttExampleCloseJournalFile(File);
}

#if defined(XMQTT_FEATURE_XMQTT_JOURNAL_SLOT)
static inline bool mqttExampleCloseJournalSlotFiles(xmqttjournalslotfiles* Files)
{
    bool OK=mqttExampleCloseJournalFile(&Files->Data[1]);
    if(!mqttExampleCloseJournalFile(&Files->Data[0]))OK=false;
    if(!mqttExampleCloseJournalFile(&Files->Control))OK=false;
    return OK;
}

static inline bool mqttExampleSameJournalFile(const xfileinfo* A,const xfileinfo* B)
{return A->Device==B->Device&&A->Identity==B->Identity;}

/* Use a stable anchored directory with three pre-provisioned names.
 * Keep all three native locks through the final Slot/Writer retirement. */
static inline xmqttjournalstatus mqttExampleOpenNativeSlot(xroot Root,const xmqttjournalconfig* Config,
    bool Create,xmqttjournalslotfiles* FilesOut,xmqttjournalslot** Out)
{
    xerror* Saved=NULL;
    if(!xrtTrySwapErrorTake(NULL,&Saved))return XMQTT_JOURNAL_ERROR;
    xmqttjournalslotfiles Files={0};
    xfileinfo Control,A,B;
    xmqttjournalslot* Slot=NULL;
    xmqttjournalstatus Status=mqttExampleOpenJournalFile(Root,"journal.slot",&Files.Control,&Control);
    if(Status==XMQTT_JOURNAL_DONE)Status=mqttExampleLockJournalFile(Files.Control);
    if(Status==XMQTT_JOURNAL_DONE)Status=mqttExampleOpenJournalFile(Root,"journal.0",&Files.Data[0],&A);
    if(Status==XMQTT_JOURNAL_DONE)Status=mqttExampleOpenJournalFile(Root,"journal.1",&Files.Data[1],&B);
    if(Status==XMQTT_JOURNAL_DONE&&
       (!(Control.Available&A.Available&B.Available&XFILE_INFO_IDENTITY)||
        mqttExampleSameJournalFile(&Control,&A)||mqttExampleSameJournalFile(&Control,&B)||mqttExampleSameJournalFile(&A,&B))){
        xrtSetErrorKind(XERR_STATE);
        Status=XMQTT_JOURNAL_STATE;
    }
    if(Status==XMQTT_JOURNAL_DONE)Status=mqttExampleLockJournalFile(Files.Data[0]);
    if(Status==XMQTT_JOURNAL_DONE)Status=mqttExampleLockJournalFile(Files.Data[1]);
    if(Status==XMQTT_JOURNAL_DONE)
        Status=Create?xmqttJournalSlotCreate(&Files,Config,&Slot):xmqttJournalSlotOpen(&Files,Config,&Slot);
    if(Status!=XMQTT_JOURNAL_DONE&&!mqttExampleCloseJournalSlotFiles(&Files))Status=XMQTT_JOURNAL_IO;
    Status=mqttExampleJournalIOResult(Status,Saved);
    if(Status==XMQTT_JOURNAL_DONE){
        *FilesOut=Files;
        *Out=Slot;
    }else if(Slot){
        (void)xmqttJournalSlotDestroy(Slot);
        (void)mqttExampleCloseJournalSlotFiles(&Files);
    }
    return Status;
}

static inline bool mqttExampleCloseNativeSlot(xmqttjournalslot* Slot,xmqttjournalslotfiles* Files)
{
    return xmqttJournalSlotDestroy(Slot)&&mqttExampleCloseJournalSlotFiles(Files);
}
#endif

#endif
