/* Separate modular link-wrap unit: injected Core IO outcomes, not product hooks.
 * Every wrapper performs real IO unless its explicitly selected fault applies.
 * Native single/header tests run the ordinary real-file paths without wrapping. */
#if defined(XCRON_TZDB_FILE_FAULTS)
static unsigned TzdbFileFault,TzdbFileReadCalls,TzdbFileStatCalls,TzdbFileFaultHits;
bool __real_xrtReadFull(xfile,ptr,size_t,size_t*);
bool __real_xrtRead(xfile,ptr,size_t,size_t*);
bool __real_xrtFileStat(xfile,xfileinfo*);
bool __real_xrtClose(xfile);
static bool tzdbFileInjected(void)
{++TzdbFileFaultHits;xrtSetErrorInfo(XERR_IO,"test.tzdb.io",1,"injected IO failure");return false;}
bool __wrap_xrtReadFull(xfile F,ptr P,size_t N,size_t* Got)
{++TzdbFileReadCalls;if(TzdbFileFault==1)return tzdbFileInjected();return __real_xrtReadFull(F,P,N,Got);}
bool __wrap_xrtRead(xfile F,ptr P,size_t N,size_t* Got)
{
    if(TzdbFileReadCalls && TzdbFileFault==3)return tzdbFileInjected();
    if(TzdbFileReadCalls && TzdbFileFault==4 && N==1){++TzdbFileFaultHits;*(uint8*)P=0;*Got=1;return true;}
    return __real_xrtRead(F,P,N,Got);
}
bool __wrap_xrtFileStat(xfile F,xfileinfo* Info)
{
    ++TzdbFileStatCalls;if(TzdbFileStatCalls>1 && TzdbFileFault==6)return tzdbFileInjected();
    bool OK=__real_xrtFileStat(F,Info);
    if(OK && TzdbFileStatCalls>1 && TzdbFileFault==2){++TzdbFileFaultHits;++Info->Size;}
    return OK;
}
bool __wrap_xrtClose(xfile F)
{bool OK=__real_xrtClose(F);return TzdbFileReadCalls && TzdbFileFault==5?tzdbFileInjected():OK;}
static void testTzdbFileFaults(xcrontzdb* DB,xroot Root)
{
    xstrview Name=XRT_STR_LITERAL("UTC");xcrontzdbstats Before,After;testRequire(xcronTzdbInspect(DB,&Before,NULL),"fault baseline aggregate bytes");
    for(unsigned Mode=1;Mode<=6;++Mode){TzdbFileReadCalls=TzdbFileStatCalls=TzdbFileFaultHits=0;TzdbFileFault=Mode;
        uint64 R=777;xcrontzdbissue I;testRequire(!xcronTzdbLoadRoot(DB,Root,XRT_STR_LITERAL("fault"),&Name,1,Before.Revision,&R,&I) && I.Code==XCRON_TZDB_IO && I.Index==0 && R==777 && TzdbFileFaultHits,"IO faults/changed metadata/growth/close error reject actual reload");
        TzdbFileFault=0;xrtClearError();testRequire(xcronTzdbInspect(DB,&After,NULL) && After.Revision==Before.Revision && After.LiveBytes==Before.LiveBytes && !After.Loading,"IO failure leaves old publication and no partial resources");
    }
    puts("[PASS] tzdb modular IO fault injection: read/metadata/growth/trailer/stat/consumed close error");
}
#endif
static void tzdbFileWrite(xroot Root,cstr Name,const void* Bytes,size_t Size)
{xfileoptions O={XFILE_WRITE|XFILE_CREATE|XFILE_EXCLUSIVE,0600,XFILE_SHARE_ALL};xfile F=xrtRootFileOpen(Root,Name,&O);testRequire(F && xrtWriteFull(F,Bytes,Size,NULL) && xrtClose(F),"write owned release fixture");}
static void testTzdbFileRejections(xcrontzdb* DB,xroot Root,xroot Parent,cstr Directory)
{
    xcrontzdbstats Before;testRequire(xcronTzdbInspect(DB,&Before,NULL),"root-load failure baseline");xstrview Names[2]={XRT_STR_LITERAL("UTC"),XRT_STR_LITERAL("Missing")};xcrontzdbissue I;uint64 R=777;
    testRequire(!xcronTzdbLoadRoot(DB,Root,XRT_STR_LITERAL("missing"),Names,2,Before.Revision,&R,&I) && I.Code==XCRON_TZDB_IO && I.Index==1 && R==777,"missing member rejects whole release");xrtClearError();
    tzdbFileWrite(Root,"Bad","bad",3);Names[1]=XRT_STR_LITERAL("Bad");testRequire(!xcronTzdbLoadRoot(DB,Root,XRT_STR_LITERAL("bad"),Names,2,Before.Revision,&R,&I) && I.Code==XCRON_TZDB_ZONE && I.Index==1 && I.Zone.Code==XCRON_ZONE_TZIF && R==777,"malformed member returns its original manifest index");xrtClearError();
    testRequire(xrtRootDirCreate(Root,"Directory",0700),"nonregular member fixture");Names[1]=XRT_STR_LITERAL("Directory");testRequire(!xcronTzdbLoadRoot(DB,Root,XRT_STR_LITERAL("directory"),Names,2,Before.Revision,&R,&I) && I.Code==XCRON_TZDB_IO,"directory is never a TZif member");xrtClearError();
    xfileoptions O={XFILE_WRITE|XFILE_CREATE|XFILE_EXCLUSIVE,0600,XFILE_SHARE_ALL};xfile Large=xrtRootFileOpen(Root,"Large",&O);testRequire(Large && xrtFileResize(Large,(uint64)XCRON_ZONE_TZIF_MAX+1) && xrtClose(Large),"bounded sparse oversize fixture");Names[1]=XRT_STR_LITERAL("Large");
    testRequire(!xcronTzdbLoadRoot(DB,Root,XRT_STR_LITERAL("large"),Names,2,Before.Revision,&R,&I) && I.Code==XCRON_TZDB_LIMIT && I.Index==1,"oversize rejected before allocating file buffer");xrtClearError();
    char Outside[128],Target[136],AliasPath[128],EscapePath[128];
    int N=snprintf(Outside,sizeof(Outside),"%s-outside",Directory);testRequire(N>0 && (size_t)N<sizeof(Outside),"outside name bound");tzdbFileWrite(Parent,Outside,TzdbFileUTC,sizeof(TzdbFileUTC));
    testRequire(snprintf(Target,sizeof(Target),"../%s",Outside)>0 && snprintf(AliasPath,sizeof(AliasPath),"%s/Alias",Directory)>0 && snprintf(EscapePath,sizeof(EscapePath),"%s/Escape",Directory)>0,"link fixture paths");
    if(xrtLinkCreate("UTC",AliasPath,false)){
        Names[1]=XRT_STR_LITERAL("Alias");testRequire(xcronTzdbLoadRoot(DB,Root,XRT_STR_LITERAL("alias"),Names,2,Before.Revision,&R,NULL),"contained relative IANA alias loads");
        testRequire(xrtLinkCreate(Target,EscapePath,false),"create escaping relative alias to existing valid data");Names[1]=XRT_STR_LITERAL("Escape");uint64 Keep=R;
        testRequire(!xcronTzdbLoadRoot(DB,Root,XRT_STR_LITERAL("escape"),Names,2,R,&Keep,&I) && I.Code==XCRON_TZDB_IO && Keep==R,"anchored loader cannot follow an alias outside its root");xrtClearError();
        testRequire(xrtRootRemove(Root,"Escape") && xrtRootRemove(Root,"Alias"),"remove known link fixtures");
    }else {
#if defined(_WIN32) || defined(_WIN64)
        testRequire(xrtErrorKind(xrtGetError())==XERR_PERMISSION || xrtErrorKind(xrtGetError())==XERR_UNSUPPORTED,"only explicit platform link limitations may skip fixture");printf("[SKIP] native symbolic link fixture: system=%d\n",(int)xrtErrorSystemCode(xrtGetError()));xrtClearError();
#else
        testRequire(false,"POSIX link fixture required");
#endif
    }
    testRequire(xrtRootRemove(Parent,Outside) && xrtRootRemove(Root,"Bad") && xrtRootRemove(Root,"Large") && xrtRootRemove(Root,"Directory"),"remove all isolated rejection fixtures");
}
