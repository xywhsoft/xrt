#include "../examples/support/game_loopback.h"
int main(void)
{const unsigned losses[]={0,1,5,10},rtts[]={20,80,200};for(size_t i=0;i<4;++i)for(size_t j=0;j<3;++j)game_run(losses[i],rtts[j]);return 0;}
