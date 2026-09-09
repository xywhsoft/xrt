#define XRT_IMPLEMENTATION
#include "lib/xrt.h"
#include <stdio.h>
int main(void){
    const char *s = "{\"command\":\"echo %XWORK_TEST_PROCESS_ENV%\",\"env\":[\"XWORK_TEST_PROCESS_ENV=xwork-process-env\"]}";
    xvalue v = xrtParseJSON((str)s, strlen(s));
    if(!v){ puts("parse null"); return 1; }
    printf("root=%d\n", xvoType(v));
    xvalue env = xvoTableGetValue(v, "env", 3);
    printf("env=%d count_arr=%u count_list=%u\n", xvoType(env), env && xvoType(env)==XVO_DT_ARRAY ? xvoArrayItemCount(env) : 0u, env && xvoType(env)==XVO_DT_LIST ? xvoListItemCount(env) : 0u);
    if(env && xvoType(env)==XVO_DT_ARRAY){
        xvalue item = xvoArrayGetValue(env,0);
        printf("item=%d text=%s\n", xvoType(item), xvoGetText(item));
    }
    xvoUnref(v);
    return 0;
}
