/* Normal public fixed certificates, exact data views, transaction/OOM and
 * caller-owned local files only. No malformed crypto or network tests. */
#include "../test.h"
#include "../fixtures/x509_path_legacy.h"

static size_t Budgets,Failures;
static const xbytesview Root={X509_PATH_ROOT,sizeof(X509_PATH_ROOT)};

static void balanced(xmemdebugsnapshot before)
{
	xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
	testRequire(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes &&
		after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount &&
		before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount &&
		before.UseAfterFreeCount==after.UseAfterFreeCount,"store input memory balance");
}

static void sweep(xbytesview input,cstr path)
{
	bool finished=false;
	for(size_t budget=0;budget<128u;budget++){
		xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
		xx509store* store=xrtX509StoreCreate();testRequire(store!=NULL,"create store before fault");
		testRequire(xrtX509StoreAddData(store,Root,NULL) && xrtX509StoreCount(store)==1,"existing trust before fault");
		size_t added=73u;
		testRequire(xrtMemDebugFailAfter(budget),"arm ordinary allocation fault");
		bool ok=path?xrtX509StoreAddFile(store,path,&added):xrtX509StoreAddData(store,input,&added);
		bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();Budgets++;
		if(hit){
			testRequire(!ok && added==73u && xrtX509StoreCount(store)==1,"failed import publishes no count or entries");
			Failures++;
		}else{
			testRequire(ok && added==2u && xrtX509StoreCount(store)==3,"successful complete import");finished=true;
		}
		const xx509cert* first=xrtX509StoreCertificate(store,0);
		testRequire(first && first->Raw.Size==Root.Size && !memcmp(first->Raw.Data,Root.Data,Root.Size),"existing DER preserved");
		xrtClearError();xrtX509StoreFree(store);balanced(before);
		if(finished)break;
	}
	testRequire(finished,"all import allocation points reached");
}

int main(int argc,char** argv)
{
	testRequire(argc==2 && strlen(argv[1])<3900,"owned output path prefix required");
	xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
	str mid=xrtPemEncodeNew("CERTIFICATE",X509_PATH_INTERMEDIATE,sizeof(X509_PATH_INTERMEDIATE));
	str leaf=xrtPemEncodeNew("CERTIFICATE",X509_PATH_LEAF,sizeof(X509_PATH_LEAF));
	testRequire(mid && leaf,"encode normal PEM fixtures");
	xbuffer pem={0};
	testRequire(xrtBufferAppend(&pem,(xbytesview){(const uint8*)"normal trust bundle\r\n",21}) &&
		xrtBufferAppend(&pem,(xbytesview){(const uint8*)mid,strlen(mid)}) &&
		xrtBufferAppend(&pem,(xbytesview){(const uint8*)leaf,strlen(leaf)}),"assemble normal two-certificate PEM");
	xrtFree(mid);xrtFree(leaf);
	/* An exact byte buffer with no trailing C terminator is a valid input. */
	bytes exact=xrtMalloc(pem.Size);testRequire(exact!=NULL,"allocate exact input view");memcpy(exact,pem.Data,pem.Size);
	xbytesview input={exact,pem.Size};
	xx509store* store=xrtX509StoreCreate();testRequire(store!=NULL,"empty store");size_t added=73;
	testRequire(xrtX509StoreAddData(store,Root,&added) && added==1 && xrtX509StoreCount(store)==1,"DER auto input");
	added=73;testRequire(xrtX509StoreAddData(store,Root,&added) && added==0,"DER duplicate is successful zero");
	testRequire(xrtX509StoreAddData(store,input,&added) && added==2 && xrtX509StoreCount(store)==3,"PEM auto input");
	added=73;testRequire(xrtX509StoreAddData(store,input,&added) && added==0 && xrtX509StoreAddData(store,input,NULL),"PEM duplicate and optional count");
	added=73;testRequire(!xrtX509StoreAddData(store,(xbytesview){NULL,1},&added) && added==73 &&
		xrtErrorKind(xrtGetError())==XERR_ARGUMENT && xrtX509StoreCount(store)==3,"invalid span ABI leaves outputs unchanged");xrtClearError();
	testRequire(!xrtX509StoreAddData(store,(xbytesview){NULL,0},&added) && added==73 &&
		xrtErrorKind(xrtGetError())==XERR_PROTOCOL && xrtX509StoreCount(store)==3,"empty encoded input is a format failure");xrtClearError();
	xrtX509StoreFree(store);
	char derPath[4096],pemPath[4096];
	testRequire(snprintf(derPath,sizeof(derPath),"%s.der",argv[1])>0 && snprintf(pemPath,sizeof(pemPath),"%s.pem",argv[1])>0,"format owned paths");
	testRequire(xrtFileWriteAll(derPath,Root) && xrtFileWriteAll(pemPath,input),"write normal owned files");
	store=xrtX509StoreCreate();testRequire(store!=NULL,"file store");
	testRequire(xrtX509StoreAddFile(store,derPath,&added) && added==1 &&
		xrtX509StoreAddFile(store,pemPath,&added) && added==2 &&
		xrtX509StoreAddFile(store,pemPath,&added) && added==0 && xrtX509StoreCount(store)==3,"file and byte input share format and duplicate rules");
	xrtX509StoreFree(store);
	sweep(input,NULL);sweep(input,pemPath);
	xrtFree(exact);xrtBufferUnit(&pem);balanced(before);testRequire(xrtMemDebugReset(),"store input memory drain");
	printf("[PASS] x509_store_input: 3 normal certificates; %zu allocation budgets; %zu failures; data/file parity; balanced\n",Budgets,Failures);
	return 0;
}
