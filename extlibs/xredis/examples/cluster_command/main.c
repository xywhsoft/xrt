#include <xredis/cluster_command.h>
#include "../../tests/support/cluster_command_peer.h"
int main(void)
{
    rffixture* Fixture=qpStart();xredisclusterclient* Client=cfCreate(Fixture,NULL);
    xrediscommandcatalog* Catalogue=qpCatalogue(1,NULL);
    xfuture* Future=qpGet(Client,Catalogue,5000);
    xredisCommandCatalogRelease(Catalogue);
    qpValue(Future);cfStop(Fixture,Client);qpValue(Future);xrtFutureDestroy(Future);
    puts("Metadata-selected GET completed; catalogue and client were released before retained result cleanup.");return 0;
}
