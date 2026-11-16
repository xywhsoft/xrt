#include "../examples/support/native_loopback.h"
int main(void){run(0,false);run(1,false);run(2,false);run(1,true);run(3,false);run(4,false);puts("xtp native: real TCP/TLS1.3 ChaCha+AES/WS duplex + rejected identity/TLS1.2 + retirement PASS");return 0;}
