#include "support/store_delta_peer.h"
static void deltaBasic(xmqttversion V)
{
    xmqttstoredeltaconfig C=sdConfig(V);sdsource S;sdSource(V,&S);unsigned char B[SD_BUFFER];size_t N=sdWrite(&C,&S,B,sizeof(B));testRequire(!memcmp(B,"XMQDSET\0",8)&&B[8]==0&&B[9]==1&&B[10]==V&&B[15]==SD_COUNT,"literal version/level/count header");sdCheck(&C,(xbytesview){B,N});memset(&S,0xcc,sizeof(S));sdCheck(&C,(xbytesview){B,N});
    size_t Empty=0;sdStatus(xmqttStoreDeltaWrite(&C,NULL,0,NULL,0,B,sizeof(B),&Empty),XMQTT_STORE_DELTA_DONE,"empty transaction");testRequire(Empty==32,"empty format bytes");xmqttstoredeltadocument D;sdStatus(xmqttStoreDeltaRead(&C,(xbytesview){B,Empty},NULL,0,NULL,0,&D),XMQTT_STORE_DELTA_DONE,"empty read with zero scratch");testRequire(!D.Count,"empty batch count");
}
static int deltaFile(int Argc,char** Argv)
{
    testRequire(Argc==4,"delta file CLI");xmqttstoredeltaconfig C=sdConfig(!strcmp(Argv[3],"5")?XMQTT_VERSION_5:XMQTT_VERSION_311);unsigned char B[SD_BUFFER];size_t N;FILE* F;
    if(!strcmp(Argv[1],"--write")){sdsource S;sdSource(C.Version,&S);N=sdWrite(&C,&S,B,sizeof(B));F=fopen(Argv[2],"wb");testRequire(F&&fwrite(B,1,N,F)==N&&fclose(F)==0,"emit own delta bytes");}
    else {testRequire(!strcmp(Argv[1],"--read"),"read fixture mode");F=fopen(Argv[2],"rb");testRequire(F!=NULL,"open independent fixture");N=fread(B,1,sizeof(B),F);testRequire(!ferror(F)&&feof(F)&&fclose(F)==0,"bounded complete fixture");sdCheck(&C,(xbytesview){B,N});}
    puts("[PASS] portable typed delta batch fixture");return 0;
}
int main(int Argc,char** Argv)
{if(Argc>1)return deltaFile(Argc,Argv);deltaBasic(XMQTT_VERSION_5);deltaBasic(XMQTT_VERSION_311);puts("[PASS] all12 typed delta kinds, native canonical MQTT5/311 packets and borrowed batching; no state application claim");return 0;}
