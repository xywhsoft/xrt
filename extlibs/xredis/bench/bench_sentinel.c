#define main sentinelOfflineExample
#include "../examples/sentinel/main.c"
#undef main
int main(int Argc,char** Argv)
{
    size_t Count=Argc>1?(size_t)strtoul(Argv[1],NULL,10):20;if(!Count || Count>1000 || Argc>2)return 2;
    double Start=xrtTimer();for(size_t N=0;N<Count;++N)if(sentinelOfflineExample())return 1;
    double Seconds=xrtTimer()-Start;printf("discovery_ms: %.6f\ndiscoveries_per_sec: %.3f\n",Seconds*1000/Count,Count/Seconds);return 0;
}
