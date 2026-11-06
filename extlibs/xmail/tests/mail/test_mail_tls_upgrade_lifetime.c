#include "../test.h"
#include "../../../../tests/fixtures/mail_tls_upgrade_lifetime.h"
#define xrtPromiseReject testMailUpgradeReject
#define __xrtFutureWaitUntil testMailUpgradeWait
#include "../test_tls.h"
#undef __xrtFutureWaitUntil
#undef xrtPromiseReject

int main(void)
{
    return testMailUpgradeLifetime();
}
