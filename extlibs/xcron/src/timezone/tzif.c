#include "timezone_internal.h"
#include <xrt/memory.h>
#include <string.h>
#if defined(XCRON_FEATURE_XCRON_TIMEZONE)
#define XCRON_TZIF_EPOCH_SECONDS (XRT_TIME_UNIX_EPOCH/1000)
#define XCRON_TZIF_CYCLE_SECONDS INT64_C(12622780800)
typedef struct xcrontziftype {int32 Offset;uint32 Name;uint16 Length;uint8 Flags;} xcrontziftype;
typedef struct xcrontzifrange {int64 Begin,End;} xcrontzifrange;
struct xcronzonehistory {
    xcronzone* Footer;
    int64 Last;
    uint32 Count,TypesCount,OffsetsCount,RangesCount;
    bool AllFooter,FooterUnknown;
    char Unknown[4];
    int64* Times;
    uint16* Indices;
    xcrontziftype* Types;
    int32* Offsets;
    xcrontzifrange* Ranges;
    char* Names;
};
typedef struct xcrontzifblock {
    uint8 Version,Width;
    uint32 Count,TypesCount,Chars,LeapsCount,StandardCount,UniversalCount;
    size_t At,End;
    const uint8 *Times,*Indices,*Types,*Names,*Leaps,*Standard,*Universal;
} xcrontzifblock;
typedef struct xcrontzifparsed {int32 Offset;bool DST,Known,Numeric;xstrview Name;uint32 NameOffset;} xcrontzifparsed;
static uint32 xcronTzifU32(const uint8* P)
{return ((uint32)P[0]<<24)|((uint32)P[1]<<16)|((uint32)P[2]<<8)|P[3];}
static int32 xcronTzifI32(const uint8* P)
{uint32 U=xcronTzifU32(P);return U<=INT32_MAX?(int32)U:-1-(int32)(UINT32_MAX-U);}
static int64 xcronTzifSecond(const uint8* P,unsigned Width)
{
    if(Width==4)return xcronTzifI32(P);
    uint64 U=((uint64)xcronTzifU32(P)<<32)|xcronTzifU32(P+4);
    return U<=INT64_MAX?(int64)U:-1-(int64)(UINT64_MAX-U);
}
static int64 xcronTzifUnix(xtime When)
{int64 S=When/1000-(When%1000<0);return S-XCRON_TZIF_EPOCH_SECONDS;}
static bool xcronTzifNameKnown(xstrview Name)
{return Name.Size!=3 || memcmp(Name.Data,"-00",3)!=0;}
static bool xcronTzifBlock(xbytesview Data,size_t At,unsigned Width,bool Main,xcrontzifblock* B,xcronzoneissue* Issue)
{
    if(At>Data.Size || Data.Size-At<44 || memcmp(Data.Data+At,"TZif",4))return xcronZoneFail(XCRON_ZONE_TZIF,At,Issue);
    const uint8* P=Data.Data+At;
    *B=(xcrontzifblock){0};B->At=At;B->Version=P[4];B->Width=(uint8)Width;
    B->UniversalCount=xcronTzifU32(P+20);B->StandardCount=xcronTzifU32(P+24);B->LeapsCount=xcronTzifU32(P+28);
    B->Count=xcronTzifU32(P+32);B->TypesCount=xcronTzifU32(P+36);B->Chars=xcronTzifU32(P+40);
    if(Main){
        if(!B->TypesCount || !B->Chars || (B->StandardCount && B->StandardCount!=B->TypesCount) ||
           (B->UniversalCount && B->UniversalCount!=B->TypesCount))return xcronZoneFail(XCRON_ZONE_TZIF,At+20,Issue);
        if(B->Count>XCRON_ZONE_TRANSITIONS_MAX || B->TypesCount>256 || B->Chars>65536 || B->LeapsCount>4096)return xcronZoneFail(XCRON_ZONE_LIMIT,At+28,Issue);
    }
    uint64 Size=(uint64)B->Count*(Width+1)+(uint64)B->TypesCount*6+B->Chars+(uint64)B->LeapsCount*(Width+4)+B->StandardCount+B->UniversalCount;
    if(Size>Data.Size-At-44)return xcronZoneFail(XCRON_ZONE_TZIF,At,Issue);
    B->Times=P+44;B->Indices=B->Times+(size_t)B->Count*Width;B->Types=B->Indices+B->Count;
    B->Names=B->Types+(size_t)B->TypesCount*6;B->Leaps=B->Names+B->Chars;
    B->Standard=B->Leaps+(size_t)B->LeapsCount*(Width+4);B->Universal=B->Standard+B->StandardCount;
    B->End=At+44+(size_t)Size;return true;
}
static size_t xcronTzifNumeric(int32 Offset,char* Out)
{
    uint32 N=(uint32)(Offset<0?-(int64)Offset:Offset),Hour=N/3600,Minute=N/60%60,Second=N%60;
    size_t At=0,Digits=0;char Reverse[10];Out[At++]=Offset<0?'-':'+';
    do {Reverse[Digits++]=(char)('0'+Hour%10);Hour/=10;} while(Hour);
    if(Digits<2)Out[At++]='0';
    while(Digits)Out[At++]=Reverse[--Digits];
    if(Minute || Second){Out[At++]=(char)('0'+Minute/10);Out[At++]=(char)('0'+Minute%10);}
    if(Second){Out[At++]=(char)('0'+Second/10);Out[At++]=(char)('0'+Second%10);}
    Out[At]=0;return At;
}
static bool xcronTzifValidate(const xcrontzifblock* B,const uint8* Base,xcrontzifparsed* Types,char Numeric[256][16],size_t* Extra,xcronzoneissue* Issue)
{
    int64 Previous=0;
    for(uint32 I=0;I<B->Count;++I){int64 S=xcronTzifSecond(B->Times+(size_t)I*B->Width,B->Width);
        if((I && S<=Previous) || B->Indices[I]>=B->TypesCount)return xcronZoneFail(XCRON_ZONE_TZIF,(size_t)(B->Times-Base)+(size_t)I*B->Width,Issue);
        Previous=S;
    }
    if(B->Names[B->Chars-1])return xcronZoneFail(XCRON_ZONE_TZIF,(size_t)(B->Names-Base)+B->Chars-1,Issue);
    *Extra=0;
    for(uint32 I=0;I<B->TypesCount;++I){const uint8* P=B->Types+(size_t)I*6;uint32 Index=P[5];int32 Offset=xcronTzifI32(P);
        if(Offset==INT32_MIN || P[4]>1 || Index>=B->Chars)return xcronZoneFail(XCRON_ZONE_TZIF,(size_t)(P-Base),Issue);
        const char* Name=(const char*)B->Names+Index;const char* End=memchr(Name,0,B->Chars-Index);
        if(!End)return xcronZoneFail(XCRON_ZONE_TZIF,(size_t)(P-Base)+5,Issue);
        size_t Length=(size_t)(End-Name);bool Safe=true;
        for(size_t K=0;K<Length;++K){unsigned char C=(unsigned char)Name[K];
            if(!((C>='A' && C<='Z') || (C>='a' && C<='z') || (C>='0' && C<='9') || C=='+' || C=='-')){Safe=false;break;}}
        Types[I]=(xcrontzifparsed){Offset,P[4]!=0,true,!Safe,{Name,Length},Index};
        if(!Safe){Length=xcronTzifNumeric(Offset,Numeric[I]);Types[I].Name=(xstrview){Numeric[I],Length};Types[I].NameOffset=(uint32)(B->Chars+*Extra);*Extra+=Length+1;}
        if(Length>XCRON_ZONE_NAME_MAX)return xcronZoneFail(XCRON_ZONE_LIMIT,(size_t)(P-Base)+5,Issue);
        Types[I].Known=xcronTzifNameKnown(Types[I].Name);
        uint8 Standard=B->StandardCount?B->Standard[I]:0,Universal=B->UniversalCount?B->Universal[I]:0;
        if(Standard>1 || Universal>1 || (Universal && !Standard))return xcronZoneFail(XCRON_ZONE_TZIF,(size_t)(B->Standard-Base),Issue);
    }
    int32 Correction=0;Previous=0;
    for(uint32 I=0;I<B->LeapsCount;++I){const uint8* P=B->Leaps+(size_t)I*(B->Width+4);int64 S=xcronTzifSecond(P,B->Width);int32 C=xcronTzifI32(P+B->Width);
        int64 Difference=(int64)C-Correction;bool Expiration=I && I+1==B->LeapsCount && B->Version=='4' && !Difference;
        if((I && S<=Previous) || (!I && S<0) || (!I && B->Version!='4' && C!=1 && C!=-1) ||
           (I && Difference!=1 && Difference!=-1 && !Expiration))return xcronZoneFail(XCRON_ZONE_TZIF,(size_t)(P-Base),Issue);
        if(!Expiration){
            bool Positive=I?Difference>0:C>0;
            int64 R=(S%XCRON_TZIF_CYCLE_SECONDS-C%XCRON_TZIF_CYCLE_SECONDS+(Positive?1:0))%XCRON_TZIF_CYCLE_SECONDS;
            if(R<0)R+=XCRON_TZIF_CYCLE_SECONDS;
            xdatetime D;(void)xrtTimeSplit(XRT_TIME_UNIX_EPOCH+R*1000,&D);
            if(D.Day!=1 || D.Hour || D.Minute || D.Second)return xcronZoneFail(XCRON_ZONE_TZIF,(size_t)(P-Base),Issue);
        }
        Previous=S;Correction=C;
    }
    return true;
}
static xcronzonestate xcronTzifUnknown(const xcronzonehistory* H)
{return (xcronzonestate){XCRON_ZONE_OFFSET_UNKNOWN,false,{H->Unknown,3}};}
static xcronzonestate xcronTzifFooterAt(const xcronzone* Footer,int64 Unix)
{
    int64 R=Unix%XCRON_TZIF_CYCLE_SECONDS;if(R<0)R+=XCRON_TZIF_CYCLE_SECONDS;
    xcronzonestate State;(void)xcronZoneAt(Footer,XRT_TIME_UNIX_EPOCH+R*1000,&State,NULL);
    return State;
}
static bool xcronTzifEqualParsed(const xcrontzifparsed* A,const xcrontzifparsed* B)
{return (!A->Known && !B->Known) || (A->Known==B->Known && A->Offset==B->Offset && A->DST==B->DST && A->Name.Size==B->Name.Size && !memcmp(A->Name.Data,B->Name.Data,A->Name.Size));}
static xcronzonestate xcronTzifType(const xcronzonehistory* H,uint32 Index)
{
    const xcrontziftype* T=H->Types+Index;
    if(!(T->Flags&2))return xcronTzifUnknown(H);
    return (xcronzonestate){T->Offset,(T->Flags&1)!=0,{H->Names+T->Name,T->Length}};
}
static uint32 xcronTzifUpper(const int64* Times,uint32 Count,int64 Second)
{uint32 Low=0,High=Count;while(Low<High){uint32 Middle=Low+(High-Low)/2;if(Times[Middle]<=Second)Low=Middle+1;else High=Middle;}return Low;}
static xcronzonestate xcronTzifAt(const xcronzonehistory* H,int64 Second)
{
    if(H->Footer && (H->AllFooter || Second>=H->Last)){
        xcronzonestate S=xcronTzifFooterAt(H->Footer,Second);return xcronTzifNameKnown(S.Abbreviation)?S:xcronTzifUnknown(H);
    }
    uint32 Index=xcronTzifUpper(H->Times,H->Count,Second);
    return xcronTzifType(H,Index?H->Indices[Index-1]:0);
}
static bool xcronTzifStateEqual(const xcronzonestate* A,const xcronzonestate* B)
{return A->Offset==B->Offset && A->IsDST==B->IsDST && A->Abbreviation.Size==B->Abbreviation.Size && !memcmp(A->Abbreviation.Data,B->Abbreviation.Data,A->Abbreviation.Size);}
static void xcronTzifOffset(int32* Offsets,uint32* Count,int32 Offset)
{
    uint32 I=0;while(I<*Count && Offsets[I]>Offset)++I;
    if(I<*Count && Offsets[I]==Offset)return;
    memmove(Offsets+I+1,Offsets+I,(*Count-I)*sizeof(*Offsets));Offsets[I]=Offset;++*Count;
}
static size_t xcronTzifAlign(size_t Size,size_t Alignment)
{return (Size+Alignment-1)/Alignment*Alignment;}
XCRON_API bool xcronZoneCreateTzif(xbytesview Data,xcronzone** Out,xcronzoneissue* Issue)
{
    if(!Out || !xrtMemRangeValid(Out,sizeof(*Out)) || (!Data.Data && Data.Size) || !xrtMemRangeValid(Data.Data,Data.Size) ||
       (Issue && !xrtMemRangeValid(Issue,sizeof(*Issue))) || xrtMemRangesOverlap(Out,sizeof(*Out),Data.Data,Data.Size) ||
       (Issue && (xrtMemRangesOverlap(Issue,sizeof(*Issue),Out,sizeof(*Out)) || xrtMemRangesOverlap(Issue,sizeof(*Issue),Data.Data,Data.Size))))return xcronZoneFail(XCRON_ZONE_ARGUMENT,0,NULL);
    if(Data.Size>XCRON_ZONE_TZIF_MAX)return xcronZoneFail(XCRON_ZONE_LIMIT,XCRON_ZONE_TZIF_MAX,Issue);
    xcrontzifblock B={0};
    if(!xcronTzifBlock(Data,0,4,false,&B,Issue))return false;
    uint8 Version=B.Version;xstrview FooterText={0};size_t FooterAt=0;
    if(Version && Version!='2' && Version!='3' && Version!='4')return xcronZoneFail(XCRON_ZONE_TZIF,4,Issue);
    if(Version){size_t At=B.End;
        if(!xcronTzifBlock(Data,At,8,true,&B,Issue))return false;
        if(B.Version!=Version)return xcronZoneFail(XCRON_ZONE_TZIF,At+4,Issue);
        if(Data.Size-B.End<2 || Data.Data[B.End]!='\n' || Data.Data[Data.Size-1]!='\n')return xcronZoneFail(XCRON_ZONE_TZIF,B.End,Issue);
        FooterAt=B.End+1;FooterText=(xstrview){(const char*)Data.Data+FooterAt,Data.Size-FooterAt-1};
        if(FooterText.Size>XCRON_ZONE_TEXT_MAX)return xcronZoneFail(XCRON_ZONE_LIMIT,FooterAt,Issue);
        if(memchr(FooterText.Data,0,FooterText.Size))return xcronZoneFail(XCRON_ZONE_TZIF,FooterAt,Issue);
    }else if(!xcronTzifBlock(Data,0,4,true,&B,Issue))return false;
    else if(B.End!=Data.Size)return xcronZoneFail(XCRON_ZONE_TZIF,B.End,Issue);
    xcrontzifparsed Parsed[257]={{0}};char Numeric[256][16];size_t Extra=0;
    if(!xcronTzifValidate(&B,Data.Data,Parsed,Numeric,&Extra,Issue))return false;
    if(B.LeapsCount)return xcronZoneFail(XCRON_ZONE_SCALE,(size_t)(B.Leaps-Data.Data),Issue);
    xcronzone* Footer=NULL;
    if(FooterText.Size){xcronzoneissue Local;
        if(!xcronZoneCreatePosix(FooterText,Version=='2'?XCRON_ZONE_POSIX:XCRON_ZONE_TZIF3,&Footer,&Local)){
            Local.Offset+=FooterAt;if(Issue)*Issue=Local;return false;
        }
    }
    int64 Last=B.Count?xcronTzifSecond(B.Times+(size_t)(B.Count-1)*B.Width,B.Width):0;
    if(Footer && B.Count){xcronzonestate S=xcronTzifFooterAt(Footer,Last);const xcrontzifparsed* T=Parsed+B.Indices[B.Count-1];
        if(S.Offset!=T->Offset || S.IsDST!=T->DST || S.Abbreviation.Size!=T->Name.Size || memcmp(S.Abbreviation.Data,T->Name.Data,T->Name.Size)){
            xcronZoneRelease(Footer);return xcronZoneFail(XCRON_ZONE_TZIF,FooterAt,Issue);
        }
    }
    Parsed[B.TypesCount]=(xcrontzifparsed){INT32_MIN,false,false,false,{"-00",3},0};
    uint32 Effective=0,Previous=0,UnknownCapacity=(!Footer || B.Count) && !Parsed[0].Known?1:0;
    for(uint32 I=0;I<B.Count;++I){uint32 Next=I+1==B.Count && !Footer?B.TypesCount:B.Indices[I];
        if(!xcronTzifEqualParsed(Parsed+Previous,Parsed+Next))++Effective;
        if((!Footer || I+1<B.Count) && Parsed[Previous].Known && !Parsed[Next].Known)++UnknownCapacity;
        Previous=Next;
    }
    int32 Offsets[258];uint32 OffsetCount=0;
    for(uint32 I=0;I<B.TypesCount;++I)if(Parsed[I].Known)xcronTzifOffset(Offsets,&OffsetCount,Parsed[I].Offset);
    bool FooterUnknown=false;
    if(Footer){const char* Names=(const char*)(Footer->Changes+(Footer->HasDST?800:0));
        for(unsigned K=0;K<(Footer->HasDST?2u:1u);++K){if((Footer->Mode==0 && K) || (Footer->Mode==1 && !K))continue;
            xstrview N={Names+(K?Footer->Names[0]+1:0),Footer->Names[K]};
            if(xcronTzifNameKnown(N))xcronTzifOffset(Offsets,&OffsetCount,Footer->Offsets[K]);else FooterUnknown=true;
        }
    }
    /* Count caps above bound this whole layout below 32MiB even on size_t32. */
    size_t Bytes=sizeof(xcronzone)+sizeof(xcronzonehistory);Bytes=xcronTzifAlign(Bytes,8);size_t TimesAt=Bytes;Bytes+=(size_t)Effective*sizeof(int64);
    size_t IndicesAt=Bytes;Bytes+=(size_t)Effective*sizeof(uint16);Bytes=xcronTzifAlign(Bytes,4);size_t TypesAt=Bytes;Bytes+=(B.TypesCount+1)*sizeof(xcrontziftype);
    size_t OffsetsAt=Bytes;Bytes+=OffsetCount*sizeof(int32);Bytes=xcronTzifAlign(Bytes,8);size_t RangesAt=Bytes;Bytes+=(size_t)UnknownCapacity*sizeof(xcrontzifrange);
    size_t NamesAt=Bytes;Bytes+=B.Chars+Extra;
    xcronzone* Z=xrtCalloc(1,Bytes);
    if(!Z){xcronZoneRelease(Footer);return xcronZoneFail(XCRON_ZONE_MEMORY,B.End,Issue);}
    xrtAtomic32Init(&Z->Refs,1);Z->Magic=XCRON_ZONE_MAGIC;Z->Bytes=Bytes;Z->History=(xcronzonehistory*)((char*)Z+sizeof(*Z));
    xcronzonehistory* H=Z->History;H->Footer=Footer;H->Last=Last;H->AllFooter=Footer && !B.Count;H->FooterUnknown=FooterUnknown;H->TypesCount=B.TypesCount;H->OffsetsCount=OffsetCount;memcpy(H->Unknown,"-00",4);
    H->Times=(int64*)((char*)Z+TimesAt);H->Indices=(uint16*)((char*)Z+IndicesAt);H->Types=(xcrontziftype*)((char*)Z+TypesAt);
    H->Offsets=(int32*)((char*)Z+OffsetsAt);H->Ranges=(xcrontzifrange*)((char*)Z+RangesAt);H->Names=(char*)Z+NamesAt;
    memcpy(H->Names,B.Names,B.Chars);memcpy(H->Offsets,Offsets,OffsetCount*sizeof(int32));
    for(uint32 I=0;I<B.TypesCount;++I){const xcrontzifparsed* T=Parsed+I;
        H->Types[I]=(xcrontziftype){T->Offset,T->NameOffset,(uint16)T->Name.Size,(uint8)((T->DST?1:0)|(T->Known?2:0))};
        if(T->Numeric)memcpy(H->Names+T->NameOffset,T->Name.Data,T->Name.Size+1);
    }
    Previous=0;
    for(uint32 I=0;I<B.Count;++I){uint32 Next=I+1==B.Count && !Footer?B.TypesCount:B.Indices[I];
        if(!xcronTzifEqualParsed(Parsed+Previous,Parsed+Next)){H->Times[H->Count]=xcronTzifSecond(B.Times+(size_t)I*B.Width,B.Width);H->Indices[H->Count++]=(uint16)Next;}
        Previous=Next;
    }
    if(!H->AllFooter){bool Unknown=!Parsed[0].Known;int64 Begin=INT64_MIN;
        for(uint32 I=0;I<H->Count;++I){int64 End=H->Times[I];if(Footer && End>=Last)break;bool Next=(H->Types[H->Indices[I]].Flags&2)==0;
            if(Unknown && !Next && Begin<End)H->Ranges[H->RangesCount++]=(xcrontzifrange){Begin,End};
            if(!Unknown && Next)Begin=End;
            Unknown=Next;
        }
        int64 End=Footer?Last:INT64_MAX;if(Unknown && Begin<End)H->Ranges[H->RangesCount++]=(xcrontzifrange){Begin,End};
    }
    if(Issue)*Issue=(xcronzoneissue){XCRON_ZONE_OK,Data.Size,0};
    *Out=Z;return true;
}
xcronzonestate xcronZoneHistoryAt(const xcronzone* Z,xtime When)
{return xcronTzifAt(Z->History,xcronTzifUnix(When));}
bool xcronZoneHistoryOverlap(const xcronzone* Z,const void* Output,size_t Size)
{const xcronzone* F=Z->History->Footer;return F && xrtMemRangesOverlap(Output,Size,F,F->Bytes);}
void xcronZoneHistoryRelease(const xcronzone* Z)
{xcronZoneRelease(Z->History->Footer);}
#if defined(XCRON_FEATURE_XCRON_TZDB)
size_t xcronZoneOwnedBytes(const xcronzone* Z)
{return Z->Bytes+(Z->History && Z->History->Footer?Z->History->Footer->Bytes:0);}
#endif
bool xcronZoneHistoryRepeat(const xcronzone* Z,xtime Begin,xtime* First)
{
    const xcronzonehistory* H=Z->History;
    if((!H->Footer && !H->Count) || H->AllFooter || (H->Footer && xcronTzifUnix(Begin)>=H->Last)){*First=Begin;return true;}
    if(!H->Footer || H->Last>INT64_MAX/1000-XCRON_TZIF_EPOCH_SECONDS)return false;
    return xcronZoneSecondValue(H->Last+XCRON_TZIF_EPOCH_SECONDS,0,First);
}
xcronzonestatus xcronZoneHistoryNext(const xcronzone* Z,xtime After,xcronzonetransition* Transition,xcronzoneissue* Issue)
{
    const xcronzonehistory* H=Z->History;int64 Second=xcronTzifUnix(After);uint32 I=xcronTzifUpper(H->Times,H->Count,Second);
    if(I<H->Count){int64 S=H->Times[I];
        if(S>INT64_MAX/1000-XCRON_TZIF_EPOCH_SECONDS)return XCRON_ZONE_NONE;
        xtime T;if(!xcronZoneSecondValue(S+XCRON_TZIF_EPOCH_SECONDS,0,&T))return XCRON_ZONE_NONE;
        *Transition=(xcronzonetransition){T,xcronTzifAt(H,S-1),xcronTzifAt(H,S)};return XCRON_ZONE_FOUND;
    }
    if(!H->Footer)return XCRON_ZONE_NONE;
    xtime Start=After;
    if(!H->AllFooter && Second<H->Last){
        if(H->Last>INT64_MAX/1000-XCRON_TZIF_EPOCH_SECONDS || !xcronZoneSecondValue(H->Last+XCRON_TZIF_EPOCH_SECONDS,0,&Start))return XCRON_ZONE_NONE;
    }
    xcronzonetransition N;xcronzonestatus Status=xcronZoneNextTransition(H->Footer,Start,&N,Issue);
    if(Status==XCRON_ZONE_FOUND){
        if(!xcronTzifNameKnown(N.Before.Abbreviation))N.Before=xcronTzifUnknown(H);
        if(!xcronTzifNameKnown(N.After.Abbreviation))N.After=xcronTzifUnknown(H);
        if(!xcronTzifStateEqual(&N.Before,&N.After))*Transition=N;
        else return XCRON_ZONE_NONE;
    }
    return Status;
}
void xcronZoneHistoryWalkInit(const xcronzone* Z,xtime After,xcronzonewalk* W)
{
    const xcronzonehistory* H=Z->History;int64 Second=xcronTzifUnix(After);
    uint32 I=xcronTzifUpper(H->Times,H->Count,Second);
    if(I<H->Count){int64 S=H->Times[I];xtime T;
        if(S>INT64_MAX/1000-XCRON_TZIF_EPOCH_SECONDS || !xcronZoneSecondValue(S+XCRON_TZIF_EPOCH_SECONDS,0,&T))return;
        W->Result=(xcronzonetransition){T,xcronTzifAt(H,S-1),xcronTzifAt(H,S)};W->Status=XCRON_ZONE_FOUND;return;
    }
    if(!H->Footer)return;
    xtime Start=After;
    if(!H->AllFooter && Second<H->Last &&
        (H->Last>INT64_MAX/1000-XCRON_TZIF_EPOCH_SECONDS || !xcronZoneSecondValue(H->Last+XCRON_TZIF_EPOCH_SECONDS,0,&Start)))return;
    xcronZoneWalkInit(H->Footer,Start,W);W->History=Z;
}
void xcronZoneHistoryWalkResult(const xcronzone* Z,xcronzonewalk* W)
{
    const xcronzonehistory* H=Z->History;
    if(!xcronTzifNameKnown(W->Result.Before.Abbreviation))W->Result.Before=xcronTzifUnknown(H);
    if(!xcronTzifNameKnown(W->Result.After.Abbreviation))W->Result.After=xcronTzifUnknown(H);
    if(xcronTzifStateEqual(&W->Result.Before,&W->Result.After))W->Status=XCRON_ZONE_NONE;
}
static bool xcronTzifUncertain(const xcronzonehistory* H,int64 Low,int64 High,int Milli)
{
    uint32 A=0,B=H->RangesCount;
    while(A<B){uint32 M=A+(B-A)/2;if(H->Ranges[M].End<=Low)A=M+1;else B=M;}
    if(A<H->RangesCount && H->Ranges[A].Begin<=High)return true;
    if(!H->FooterUnknown || (!H->AllFooter && High<H->Last))return false;
    if(!H->AllFooter && Low<H->Last)Low=H->Last;
    xcronzonestate State=xcronTzifFooterAt(H->Footer,Low);if(!xcronTzifNameKnown(State.Abbreviation))return true;
    xtime Start,End;if(!xcronZoneSecondValue(Low+XCRON_TZIF_EPOCH_SECONDS,Milli,&Start) || !xcronZoneSecondValue(High+XCRON_TZIF_EPOCH_SECONDS,Milli,&End))return false;
    xcronzonetransition N;
    while(xcronZoneNextTransition(H->Footer,Start,&N,NULL)==XCRON_ZONE_FOUND && N.When<=End){
        if(!xcronTzifNameKnown(N.After.Abbreviation))return true;
        Start=N.When;
    }
    return false;
}
xcronzonestatus xcronZoneHistoryResolve(const xcronzone* Z,const xdatetime* D,xtime* Candidates,size_t Capacity,size_t* Count,xcronzoneissue* Issue)
{
    const xcronzonehistory* H=Z->History;int64 Local;
    if(!xcronZoneCivilSeconds(D,&Local)){*Count=0;if(Issue)*Issue=(xcronzoneissue){XCRON_ZONE_OK,0,0};return XCRON_ZONE_NONE;}
    int64 Low=Local-INT32_MAX-XCRON_TZIF_EPOCH_SECONDS,High=Local+INT32_MAX-XCRON_TZIF_EPOCH_SECONDS;
    int64 Minimum=INT64_MIN/1000-1+(D->Millisecond<(1000+INT64_MIN%1000)?1:0)-XCRON_TZIF_EPOCH_SECONDS;
    int64 Maximum=INT64_MAX/1000-(D->Millisecond>INT64_MAX%1000?1:0)-XCRON_TZIF_EPOCH_SECONDS;
    if(Low<Minimum)Low=Minimum;
    if(High>Maximum)High=Maximum;
    if(Low<=High && xcronTzifUncertain(H,Low,High,D->Millisecond)){xcronZoneFail(XCRON_ZONE_UNKNOWN,0,Issue);return XCRON_ZONE_ERROR;}
    xtime Values[258];size_t N=0;
    for(uint32 I=0;I<H->OffsetsCount;++I){xtime T;int32 Offset=H->Offsets[I];
        if(xcronZoneSecondValue(Local-Offset,D->Millisecond,&T) && xcronTzifAt(H,xcronTzifUnix(T)).Offset==Offset)Values[N++]=T;
    }
    *Count=N;if(Issue)*Issue=(xcronzoneissue){N>Capacity?XCRON_ZONE_LIMIT:XCRON_ZONE_OK,0,0};
    if(N>Capacity)return XCRON_ZONE_CAPACITY;
    if(N)memcpy(Candidates,Values,N*sizeof(*Candidates));
    return N?XCRON_ZONE_FOUND:XCRON_ZONE_NONE;
}
#endif
