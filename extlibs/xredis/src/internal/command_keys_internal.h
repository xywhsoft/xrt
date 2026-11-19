#ifndef XREDIS_INTERNAL_COMMAND_KEYS_H
#define XREDIS_INTERNAL_COMMAND_KEYS_H
#include <xredis/command_keys.h>
#if defined(XREDIS_FEATURE_XREDIS_COMMAND_KEYS) && defined(XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND)
bool __xredisCommandCatalogOutput(const xrediscommandcatalog* Catalogue,const void* Output,size_t Bytes);
#endif
#endif
