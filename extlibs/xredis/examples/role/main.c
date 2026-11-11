#include <xredis/role.h>
#include <stdio.h>
#include <string.h>
static int roleExampleInspect(const char* Wire,xredisrolekind Kind)
{
    xredisreplyreader* Reader=NULL;xredisreply* R=NULL;size_t Used=0;
    if(!xredisReplyReaderCreate(NULL,&Reader))return 1;
    xredisreplystatus S=xredisReplyReaderFeed(Reader,(xbytesview){(cbytes)Wire,strlen(Wire)},&Used,&R);
    xredisReplyReaderDestroy(Reader);if(S!=XREDIS_REPLY_READY || Used!=strlen(Wire)){xredisReplyRelease(R);return 1;}
    xredisrolelimits K;xredisroleinfo I;if(!xredisRoleLimitsInit(&K) || xredisRoleRead(R,&K,&I)!=XREDIS_ROLE_VALUE || I.Kind!=Kind){xredisReplyRelease(R);return 1;}
    size_t Count=0;if(Kind==XREDIS_ROLE_MASTER){xredisrolereplica Peer;while(xredisRoleReplicaNext(&I.Replicas,&Peer)){if(Peer.Port!=6380 || Peer.Offset!=99){xredisReplyRelease(R);return 1;}++Count;}}
    else{xbytesview Name;while(xredisRoleMasterNext(&I.Monitored,&Name)){if(Name.Size!=5 || memcmp(Name.Data,"cache",5)){xredisReplyRelease(R);return 1;}++Count;}}
    xredisReplyRelease(R);return Count!=1;
}
int main(void)
{
    if(roleExampleInspect("*3\r\n+master\r\n:100\r\n*1\r\n*3\r\n+127.0.0.1\r\n+6380\r\n+99\r\n",XREDIS_ROLE_MASTER) || roleExampleInspect("*2\r\n+sentinel\r\n*1\r\n+cache\r\n",XREDIS_ROLE_SENTINEL))return 1;
    puts("ROLE: borrowed replica endpoints and monitored names verified");return 0;
}
