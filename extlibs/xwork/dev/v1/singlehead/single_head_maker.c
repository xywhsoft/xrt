#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct xwork_file_entry {
    const char *sPath;
} xwork_file_entry;

static const xwork_file_entry g_arrImplFile[] = {
    { "src/xwork_core/xwork_internal.h" },
    { "src/xwork_core/xwork_core.c" },
    { "src/xwork_workspace/xwork_workspace.c" },
    { "src/xwork_tools/xwork_tools.c" },
    { "src/xwork_agents/xwork_agents.c" },
    { "src/xwork_remote/xwork_remote.c" },
    { "src/xwork_replay/xwork_replay.c" },
    { "src/xwork_orchestrator/xwork_orchestrator.c" },
    { "src/xwork_policy/xwork_policy.c" },
    { "src/xwork_persistence/xwork_persistence.c" },
    { "src/xwork_artifacts/xwork_artifacts.c" },
    { "src/xwork_host/xwork_host.c" },
    { "src/xwork_profiles/xwork_profiles.c" }
};

static int xwork_write_text(FILE *hDst, const char *sText)
{
    size_t iSize = strlen(sText);
    return fwrite(sText, 1, iSize, hDst) == iSize;
}

static int xwork_copy_stream(FILE *hDst, FILE *hSrc)
{
    char arrBuf[8192];
    size_t iRead;

    for ( ;; ) {
        iRead = fread(arrBuf, 1, sizeof(arrBuf), hSrc);
        if ( iRead > 0 ) {
            if ( fwrite(arrBuf, 1, iRead, hDst) != iRead ) {
                return 0;
            }
        }
        if ( iRead < sizeof(arrBuf) ) {
            return ferror(hSrc) ? 0 : 1;
        }
    }
}

static int xwork_line_has_any(const char *sLine, const char *const *arrNeedle, size_t iCount)
{
    size_t iIndex;

    for ( iIndex = 0; iIndex < iCount; ++iIndex ) {
        if ( strstr(sLine, arrNeedle[iIndex]) != NULL ) {
            return 1;
        }
    }

    return 0;
}

static int xwork_should_skip_impl_include(const char *sLine)
{
    static const char *const arrNeedle[] = {
        "#include \"xwork_internal.h\"",
        "#include \"../xwork_core/xwork_internal.h\"",
        "#include \"../../xwork.h\"",
        "#include \"../../lib/xrt.h\"",
        "#include \"../../lib/xllm.h\"",
        "#include \"../../lib/xllm-session.h\"",
        "#include \"../../lib/xllm-memory.h\""
    };

    return xwork_line_has_any(sLine, arrNeedle, sizeof(arrNeedle) / sizeof(arrNeedle[0]));
}

static int xwork_write_filtered_file_block(FILE *hDst, const char *sPath)
{
    FILE *hSrc = fopen(sPath, "rb");
    char arrLine[8192];

    if ( hSrc == NULL ) {
        fprintf(stderr, "failed to open %s\n", sPath);
        return 0;
    }

    if ( !xwork_write_text(hDst, "\n/* ===== File: ") ||
        !xwork_write_text(hDst, sPath) ||
        !xwork_write_text(hDst, " ===== */\n\n") ) {
        fclose(hSrc);
        return 0;
    }

    while ( fgets(arrLine, sizeof(arrLine), hSrc) != NULL ) {
        if ( xwork_should_skip_impl_include(arrLine) ) {
            continue;
        }
        if ( !xwork_write_text(hDst, arrLine) ) {
            fclose(hSrc);
            return 0;
        }
    }

    if ( ferror(hSrc) || !xwork_write_text(hDst, "\n") ) {
        fclose(hSrc);
        fprintf(stderr, "failed to copy %s\n", sPath);
        return 0;
    }

    fclose(hSrc);
    return 1;
}

static int xwork_write_file_block(FILE *hDst, const char *sPath)
{
    FILE *hSrc = fopen(sPath, "rb");

    if ( hSrc == NULL ) {
        fprintf(stderr, "failed to open %s\n", sPath);
        return 0;
    }

    if ( !xwork_write_text(hDst, "\n/* ===== File: ") ||
        !xwork_write_text(hDst, sPath) ||
        !xwork_write_text(hDst, " ===== */\n\n") ||
        !xwork_copy_stream(hDst, hSrc) ||
        !xwork_write_text(hDst, "\n") ) {
        fclose(hSrc);
        fprintf(stderr, "failed to copy %s\n", sPath);
        return 0;
    }

    fclose(hSrc);
    return 1;
}

static int xwork_write_file_list(FILE *hDst, const xwork_file_entry *arrFile, size_t iCount)
{
    size_t iIndex;

    for ( iIndex = 0; iIndex < iCount; ++iIndex ) {
        if ( !xwork_write_filtered_file_block(hDst, arrFile[iIndex].sPath) ) {
            return 0;
        }
    }

    return 1;
}

static int xwork_write_external_include(FILE *hDst, const char *sName, const char *sLibPath)
{
    return
        xwork_write_text(hDst, "#if defined(__has_include)\n") &&
        xwork_write_text(hDst, "#  if __has_include(\"") &&
        xwork_write_text(hDst, sName) &&
        xwork_write_text(hDst, "\")\n") &&
        xwork_write_text(hDst, "#    include \"") &&
        xwork_write_text(hDst, sName) &&
        xwork_write_text(hDst, "\"\n") &&
        xwork_write_text(hDst, "#  elif __has_include(\"") &&
        xwork_write_text(hDst, sLibPath) &&
        xwork_write_text(hDst, "\")\n") &&
        xwork_write_text(hDst, "#    include \"") &&
        xwork_write_text(hDst, sLibPath) &&
        xwork_write_text(hDst, "\"\n") &&
        xwork_write_text(hDst, "#  else\n") &&
        xwork_write_text(hDst, "#    error \"xwork single header requires external dependency: ") &&
        xwork_write_text(hDst, sName) &&
        xwork_write_text(hDst, "\"\n") &&
        xwork_write_text(hDst, "#  endif\n") &&
        xwork_write_text(hDst, "#else\n") &&
        xwork_write_text(hDst, "#  include \"") &&
        xwork_write_text(hDst, sName) &&
        xwork_write_text(hDst, "\"\n") &&
        xwork_write_text(hDst, "#endif\n");
}

static int xwork_write_external_includes(FILE *hDst)
{
    return
        xwork_write_external_include(hDst, "xrt.h", "../lib/xrt.h") &&
        xwork_write_external_include(hDst, "xllm.h", "../lib/xllm.h") &&
        xwork_write_external_include(hDst, "xllm-session.h", "../lib/xllm-session.h") &&
        xwork_write_external_include(hDst, "xllm-memory.h", "../lib/xllm-memory.h");
}

int main(void)
{
    FILE *hDst = fopen("singlehead/xwork.h", "wb");

    if ( hDst == NULL ) {
        fprintf(stderr, "failed to create singlehead/xwork.h\n");
        return 1;
    }

    if ( !xwork_write_text(hDst, "/* Auto-generated single header from xwork source tree. */\n") ||
        !xwork_write_text(hDst, "#ifndef XWORK_SINGLE_HEADER\n") ||
        !xwork_write_text(hDst, "#define XWORK_SINGLE_HEADER\n\n") ||
        !xwork_write_text(hDst, "/* Usage:\n") ||
        !xwork_write_text(hDst, "   #define XRT_IMPLEMENTATION\n") ||
        !xwork_write_text(hDst, "   #include \"xrt.h\"\n") ||
        !xwork_write_text(hDst, "   #define XLLM_SESSION_IMPLEMENTATION\n") ||
        !xwork_write_text(hDst, "   #include \"xllm-session.h\"\n") ||
        !xwork_write_text(hDst, "   #define XLLM_MEMORY_IMPLEMENTATION\n") ||
        !xwork_write_text(hDst, "   #include \"xllm-memory.h\"\n") ||
        !xwork_write_text(hDst, "   #define XWORK_IMPLEMENTATION\n") ||
        !xwork_write_text(hDst, "   #include \"xwork.h\"\n") ||
        !xwork_write_text(hDst, "\n") ||
        !xwork_write_text(hDst, "   xwork embeds only its own implementation. Provide xrt/xllm/sqlite\n") ||
        !xwork_write_text(hDst, "   headers and implementation objects according to your build layout.\n") ||
        !xwork_write_text(hDst, "*/\n") ||
        !xwork_write_file_block(hDst, "xwork.h") ||
        !xwork_write_text(hDst, "\n#if defined(XWORK_IMPLEMENTATION)\n\n") ||
        !xwork_write_external_includes(hDst) ||
        !xwork_write_file_list(hDst, g_arrImplFile, sizeof(g_arrImplFile) / sizeof(g_arrImplFile[0])) ||
        !xwork_write_text(hDst, "\n#endif /* XWORK_IMPLEMENTATION */\n\n") ||
        !xwork_write_text(hDst, "#endif /* XWORK_SINGLE_HEADER */\n") ) {
        fclose(hDst);
        fprintf(stderr, "failed to write singlehead/xwork.h\n");
        return 2;
    }

    fclose(hDst);
    return 0;
}
