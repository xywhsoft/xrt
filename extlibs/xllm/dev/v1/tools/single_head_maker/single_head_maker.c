#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SHM_MAX_PATH 4096
#define SHM_MAX_FILES 1024

typedef struct {
    char szPath[SHM_MAX_PATH];
} shm_file_entry;

static shm_file_entry g_arrProcessed[SHM_MAX_FILES];
static int g_iProcessedCount = 0;

static void shm_normalize_path(char *sPath)
{
    char *p = sPath;
    while ( *p ) {
        if ( *p == '\\' ) {
            *p = '/';
        }
        ++p;
    }
}

static int shm_file_exists(const char *sPath)
{
    FILE *pFile = fopen(sPath, "rb");
    if ( pFile ) {
        fclose(pFile);
        return 1;
    }
    return 0;
}

static const char *shm_basename(const char *sPath)
{
    const char *sSlash1 = strrchr(sPath, '/');
    const char *sSlash2 = strrchr(sPath, '\\');
    const char *sSlash = sSlash1;
    if ( sSlash2 && (!sSlash || sSlash2 > sSlash) ) {
        sSlash = sSlash2;
    }
    return sSlash ? (sSlash + 1) : sPath;
}

static int shm_is_xrt_include(const char *sInclude)
{
    const char *sBase = shm_basename(sInclude);
    if ( strcmp(sBase, "xrt.h") == 0 ) {
        return 1;
    }
    return 0;
}

static int shm_is_processed(const char *sPath)
{
    int i;
    for ( i = 0; i < g_iProcessedCount; ++i ) {
        if ( strcmp(g_arrProcessed[i].szPath, sPath) == 0 ) {
            return 1;
        }
    }
    return 0;
}

static void shm_add_processed(const char *sPath)
{
    if ( g_iProcessedCount >= SHM_MAX_FILES ) {
        return;
    }
    strncpy(g_arrProcessed[g_iProcessedCount].szPath, sPath, SHM_MAX_PATH - 1);
    g_arrProcessed[g_iProcessedCount].szPath[SHM_MAX_PATH - 1] = '\0';
    ++g_iProcessedCount;
}

static void shm_get_dirname(const char *sPath, char *sOut)
{
    const char *sBase;
    size_t iLen;

    strcpy(sOut, sPath);
    shm_normalize_path(sOut);

    sBase = shm_basename(sOut);
    iLen = (size_t)(sBase - sOut);
    if ( iLen == 0 ) {
        strcpy(sOut, ".");
        return;
    }
    sOut[iLen - 1] = '\0';
}

static void shm_resolve_include(const char *sCurrentFile, const char *sInclude, char *sOut)
{
    char szDir[SHM_MAX_PATH];

    if ( !sInclude || !sInclude[0] ) {
        sOut[0] = '\0';
        return;
    }

    if ( sInclude[0] == '/' || (sInclude[0] && sInclude[1] == ':') ) {
        strcpy(sOut, sInclude);
        shm_normalize_path(sOut);
        return;
    }

    shm_get_dirname(sCurrentFile, szDir);
    snprintf(sOut, SHM_MAX_PATH, "%s/%s", szDir, sInclude);
    shm_normalize_path(sOut);
}

static int shm_parse_local_include(const char *sLine, char *sInclude)
{
    const char *sStart;
    const char *sEnd;

    sStart = strstr(sLine, "#include \"");
    if ( !sStart ) {
        return 0;
    }

    sStart += 10;
    sEnd = strchr(sStart, '"');
    if ( !sEnd ) {
        return 0;
    }

    memcpy(sInclude, sStart, (size_t)(sEnd - sStart));
    sInclude[sEnd - sStart] = '\0';
    return 1;
}

static int shm_should_keep_include_external(const char *sInclude)
{
    return shm_is_xrt_include(sInclude);
}

static int shm_process_file(FILE *pOut, const char *sFilePath);

static void shm_write_line(FILE *pOut, const char *sLine)
{
    size_t iLen;

    if ( !sLine ) {
        return;
    }
    iLen = strlen(sLine);
    while ( iLen > 0 && (sLine[iLen - 1] == '\n' || sLine[iLen - 1] == '\r') ) {
        --iLen;
    }
    if ( iLen > 0 ) {
        fwrite(sLine, 1u, iLen, pOut);
    }
    fputc('\n', pOut);
}

static int shm_process_include(FILE *pOut, const char *sCurrentFile, const char *sInclude)
{
    char szResolved[SHM_MAX_PATH];

    if ( shm_should_keep_include_external(sInclude) ) {
        fprintf(pOut, "#include \"xrt.h\"\n");
        return 1;
    }

    shm_resolve_include(sCurrentFile, sInclude, szResolved);
    if ( !shm_file_exists(szResolved) ) {
        fprintf(pOut, "#include \"%s\"\n", sInclude);
        return 1;
    }

    return shm_process_file(pOut, szResolved);
}

static int shm_process_file(FILE *pOut, const char *sFilePath)
{
    FILE *pIn;
    char szPath[SHM_MAX_PATH];
    char szLine[8192];

    strcpy(szPath, sFilePath);
    shm_normalize_path(szPath);

    if ( shm_is_processed(szPath) ) {
        fprintf(pOut, "/* duplicate include skipped: %s */\n", szPath);
        return 1;
    }
    shm_add_processed(szPath);

    pIn = fopen(szPath, "rb");
    if ( !pIn ) {
        fprintf(stderr, "failed to open: %s\n", szPath);
        return 0;
    }

    fprintf(pOut, "\n/* ===== begin: %s ===== */\n\n", szPath);

    while ( fgets(szLine, sizeof(szLine), pIn) ) {
        char szInclude[SHM_MAX_PATH];

        if ( shm_parse_local_include(szLine, szInclude) ) {
            if ( !shm_process_include(pOut, szPath, szInclude) ) {
                fclose(pIn);
                return 0;
            }
            continue;
        }

        shm_write_line(pOut, szLine);
    }

    fprintf(pOut, "\n/* ===== end: %s ===== */\n", szPath);

    fclose(pIn);
    return 1;
}

int main(int argc, char **argv)
{
    const char *sInput = "xllm.h";
    const char *sOutput = "singlehead/xllm.h";
    FILE *pOut;
    int i;

    for ( i = 1; i < argc; ++i ) {
        if ( strcmp(argv[i], "-i") == 0 && i + 1 < argc ) {
            sInput = argv[++i];
        } else if ( strcmp(argv[i], "-o") == 0 && i + 1 < argc ) {
            sOutput = argv[++i];
        } else if ( strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0 ) {
            printf("Usage: single_head_maker [-i input] [-o output]\n");
            return 0;
        }
    }

    pOut = fopen(sOutput, "wb");
    if ( !pOut ) {
        fprintf(stderr, "failed to open output: %s\n", sOutput);
        return 1;
    }

    fprintf(pOut, "/*\n");
    fprintf(pOut, " * xllm single-header distribution\n");
    fprintf(pOut, " *\n");
    fprintf(pOut, " * Note:\n");
    fprintf(pOut, " * - This file intentionally does not embed xrt.\n");
    fprintf(pOut, " * - Distribute xllm together with xrt.h / xrt runtime sources.\n");
    fprintf(pOut, " */\n\n");

    if ( !shm_process_file(pOut, sInput) ) {
        fclose(pOut);
        return 1;
    }

    fclose(pOut);
    return 0;
}
