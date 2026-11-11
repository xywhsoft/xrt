#include <xredis/role.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void roleProbeRequire(bool OK,const char* Text){if(!OK){fprintf(stderr,"[FAIL] ROLE corpus: %s\n",Text);exit(1);}}
int main(int Argc,char** Argv)
{
    if(Argc!=6)return 2;
    unsigned Kind=(unsigned)strtoul(Argv[2],NULL,10);size_t Entries=(size_t)strtoull(Argv[3],NULL,10);unsigned Port=(unsigned)strtoul(Argv[4],NULL,10),Link=(unsigned)strtoul(Argv[5],NULL,10);
    FILE* F=fopen(Argv[1],"rb");roleProbeRequire(F!=NULL,"open fixture");roleProbeRequire(fseek(F,0,SEEK_END)==0,"seek");long Length=ftell(F);roleProbeRequire(Length>0 && Length<=1048576 && fseek(F,0,SEEK_SET)==0,"bounded file");char* Wire=malloc((size_t)Length);roleProbeRequire(Wire && fread(Wire,1,(size_t)Length,F)==(size_t)Length,"read fixture");fclose(F);
    xredisreplyreader* Reader=NULL;xredisreply* R=NULL;size_t Used=0;roleProbeRequire(xredisReplyReaderCreate(NULL,&Reader),"reader");roleProbeRequire(xredisReplyReaderFeed(Reader,(xbytesview){(cbytes)Wire,(size_t)Length},&Used,&R)==XREDIS_REPLY_READY && Used==(size_t)Length,"actual server reply");xredisReplyReaderDestroy(Reader);free(Wire);
    xredisrolelimits K;xredisroleinfo I;roleProbeRequire(xredisRoleLimitsInit(&K) && xredisRoleRead(R,&K,&I)==XREDIS_ROLE_VALUE && (unsigned)I.Kind==Kind,"role identity");
    size_t Count=0;if(Kind==XREDIS_ROLE_MASTER){xredisrolereplica P;while(xredisRoleReplicaNext(&I.Replicas,&P)){roleProbeRequire(P.Port==Port && P.Offset>=0 && P.Host.Size==9 && !memcmp(P.Host.Data,"127.0.0.1",9),"actual connected replica endpoint");++Count;}}
    else if(Kind==XREDIS_ROLE_REPLICA){roleProbeRequire(I.MasterPort==Port && (unsigned)I.Link==Link && I.Offset>= -1 && I.MasterHost.Size==9 && !memcmp(I.MasterHost.Data,"127.0.0.1",9),"replica link/offset");}
    else{xbytesview Name;while(xredisRoleMasterNext(&I.Monitored,&Name)){roleProbeRequire(Name.Size==5 && !memcmp(Name.Data,"cache",5),"monitored name");++Count;}}
    roleProbeRequire(Count==Entries,"all actual entries");xredisReplyRelease(R);printf("[PASS] ROLE corpus kind=%u entries=%zu\n",Kind,Count);return 0;
}
