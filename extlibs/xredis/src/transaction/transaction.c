#include <xredis/transaction.h>
#include "../internal/client_connect.h"
#include <xrt/memory.h>

#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
XREDIS_API bool xredisClientTransactionConfigInit(xredistransactionconfig* Config)
{
    if (Config == NULL || !xrtMemRangeValid(Config, sizeof(*Config))) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xrt.redis", XREDIS_CLIENT_ARGUMENT, "invalid transaction config output"); return false;
    }
    *Config = (xredistransactionconfig){sizeof(*Config), XREDIS_TRANSACTION_COMMANDS_DEFAULT}; return true;
}

XREDIS_API xredisadmission xredisClientTransactionAsync(xredisclient* Client,
    const xredistransactioncommand* Commands, size_t Count,
    const xredistransactionconfig* Config, int64 Timeout, xfuture** Future)
{
    return __xredisClientTransactionAsync(Client, Commands, Count, Config, Timeout, xrtTimer(), Future);
}

XREDIS_API xredisadmission xredisClientTransaction(xredisclient* Client,
    const xredistransactioncommand* Commands, size_t Count,
    const xredistransactionconfig* Config, int64 Timeout, xfuture** Future)
{
    return __xredisClientTransaction(Client, Commands, Count, Config, Timeout, xrtTimer(), Future);
}
#endif
