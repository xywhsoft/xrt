#include <xcron/tzdb_file.h>
#include "../tzdb/tzdb_internal.h"
#if defined(XCRON_FEATURE_XCRON_TZDB_FILE)
static void xcronTzdbFileError(xcrontzdbissue* W,xcrontzdberror Code)
{W->Code=Code;W->Cause=xrtErrorKind(xrtGetError());W->SystemCode=xrtErrorSystemCode(xrtGetError());}
static bool xcronTzdbFileSame(const xfileinfo* A,const xfileinfo* B)
{
    if(A->Type!=B->Type || !(B->Available&XFILE_INFO_SIZE) || A->Size!=B->Size)return false;
    if((A->Available&XFILE_INFO_IDENTITY) && (!(B->Available&XFILE_INFO_IDENTITY) || A->Identity!=B->Identity || A->Device!=B->Device))return false;
    if((A->Available&XFILE_INFO_MODIFY_TIME) && (!(B->Available&XFILE_INFO_MODIFY_TIME) || A->Modified!=B->Modified))return false;
    if((A->Available&XFILE_INFO_CHANGE_TIME) && (!(B->Available&XFILE_INFO_CHANGE_TIME) || A->Changed!=B->Changed))return false;
    return true;
}
static bool xcronTzdbReadRoot(ptr Raw,xstrview Name,xbytesview* Data,ptr* Owned,xcrontzdbissue* Work)
{
    xroot Root=Raw;xfileoptions Options={XFILE_READ,0,XFILE_SHARE_READ|XFILE_SHARE_DELETE};
    xfile File=xrtRootFileOpen(Root,Name.Data,&Options);
    if(!File){xcronTzdbFileError(Work,XCRON_TZDB_IO);return false;}
    xfileinfo Before,After;bool OK=xrtFileStat(File,&Before);ptr Buffer=NULL;size_t Size=0;
    if(!OK)xcronTzdbFileError(Work,XCRON_TZDB_IO);
    else if(Before.Type!=XFILE_TYPE_FILE || !(Before.Available&XFILE_INFO_SIZE)){Work->Code=XCRON_TZDB_IO;OK=false;}
    else if(Before.Size>XCRON_ZONE_TZIF_MAX){Work->Code=XCRON_TZDB_LIMIT;OK=false;}
    else {
        Size=(size_t)Before.Size;Buffer=xrtMalloc(Size?Size:1);
        if(!Buffer){Work->Code=XCRON_TZDB_MEMORY;OK=false;}
        else if(!xrtReadFull(File,Buffer,Size,NULL)){xcronTzdbFileError(Work,XCRON_TZDB_IO);OK=false;}
        else {uint8 Extra;size_t Read=0;
            if(!xrtRead(File,&Extra,1,&Read) || !xrtFileStat(File,&After)){xcronTzdbFileError(Work,XCRON_TZDB_IO);OK=false;}
            else if(Read || !xcronTzdbFileSame(&Before,&After)){Work->Code=XCRON_TZDB_IO;OK=false;}
        }
    }
    xerror* Error=OK?NULL:xrtTakeError();bool Closed=xrtClose(File);
    if(!Closed && OK){xcronTzdbFileError(Work,XCRON_TZDB_IO);OK=false;}
    if(Error)xrtSetErrorTake(Error);
    if(!OK){xrtFree(Buffer);return false;}
    *Data=(xbytesview){Buffer,Size};*Owned=Buffer;return true;
}
XCRON_API bool xcronTzdbLoadRoot(xcrontzdb* D,xroot Root,xstrview Version,const xstrview* Names,size_t Count,uint64 Expected,uint64* Revision,xcrontzdbissue* Issue)
{
    if(!Root){xrtSetErrorInfo(XERR_ARGUMENT,"xrt.cron.tzdb",XCRON_TZDB_ARGUMENT,"cron tzdb requires a live explicit root");return false;}
    xcrontzdbsource Source={.Names=Names,.Count=Count,.Read=xcronTzdbReadRoot,.Context=Root};
    return xcronTzdbLoadSource(D,Version,&Source,Expected,Revision,Issue);
}
#endif
