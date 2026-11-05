#include "../../../xws-server/include/xws-server/features.h"
#include "../../../xhttp-server/include/xhttp-server/features.h"
#include "../../../xhttp/include/xhttp/features.h"
#include "../../../xws/include/xws/features.h"
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#define XWS_IMPLEMENTATION
#include "../../../../single/extlibs/xws.h"
#define XHTTP_IMPLEMENTATION
#include "../../../../single/extlibs/xhttp.h"
#define XHTTP_SERVER_IMPLEMENTATION
#include "../../../../single/extlibs/xhttp-server.h"
#define XWS_SERVER_IMPLEMENTATION
#include "../../../../single/extlibs/xws-server.h"

/* Reuse the same snapshot across modular and amalgamated feature closures. */
#include "../websocket/test_server_optional_deflate.c"
