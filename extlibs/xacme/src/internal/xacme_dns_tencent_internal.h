#ifndef XACME_DNS_TENCENT_INTERNAL_H
#define XACME_DNS_TENCENT_INTERNAL_H

#include <xrt/core.h>

/* API 3.0 在 HTTP 2xx 内仍可能返回 Response.Error。 */
bool xacmeDnsTencentResponseSuccess(xstrview sBody);

#endif
