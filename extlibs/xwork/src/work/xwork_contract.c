#include "../internal/xwork_internal.h"

/* Host-owned writers are the ABI boundary for TCC/native tool modules. The
 * callback may supply transient memory; these functions copy it immediately. */

#define XWORK_PERMISSION_RESOURCE_MAX 64u
#define XWORK_PERMISSION_TEXT_MAX (16u * 1024u)
#define XWORK_RESOURCE_ACCESS_ALL ((xwork_resource_access)0x3fu)

static void xwork__permission_writer_fail(xwork_permission_resource_writer *pWriter,
                                          xwork_error_code eCode, const char *sMessage)
{
    if (!pWriter || pWriter->bFailed)
        return;
    pWriter->bFailed = true;
    pWriter->eFailure = eCode;
    (void)snprintf(pWriter->sFailure, sizeof(pWriter->sFailure), "%s",
                   sMessage ? sMessage : "permission resource writer failed");
}

void xwork__permission_writer_init(xwork_permission_resource_writer *pWriter)
{
    if (!pWriter)
        return;
    memset(pWriter, 0, sizeof(*pWriter));
    pWriter->uMagic = XWORK_PERMISSION_WRITER_MAGIC;
}

void xwork__permission_writer_unit(xwork_permission_resource_writer *pWriter)
{
    size_t i;
    if (!pWriter)
        return;
    for (i = 0u; i < pWriter->iResourceCount; ++i)
    {
        free((char *)pWriter->pResources[i].sResource);
    }
    free(pWriter->pResources);
    memset(pWriter, 0, sizeof(*pWriter));
}

bool xworkPermissionResourceWriterAdd(xwork_permission_resource_writer *pWriter,
                                      xwork_resource_kind eKind, xwork_resource_access uAccess,
                                      const char *sResource)
{
    xwork_permission_resource *pNew;
    char *sCopy;
    size_t iLength;
    size_t iCap;
    if (!pWriter || pWriter->uMagic != XWORK_PERMISSION_WRITER_MAGIC || pWriter->bFailed)
        return false;
    if (eKind <= XWORK_RESOURCE_NONE || eKind > XWORK_RESOURCE_AGENT || !uAccess ||
        (uAccess & ~XWORK_RESOURCE_ACCESS_ALL) != 0u || !sResource || !sResource[0])
    {
        xwork__permission_writer_fail(pWriter, XWORK_ERROR_INVALID_ARGUMENT,
                                      "invalid permission resource descriptor");
        return false;
    }
    iLength = strlen(sResource);
    if (pWriter->iResourceCount >= XWORK_PERMISSION_RESOURCE_MAX ||
        iLength > XWORK_PERMISSION_TEXT_MAX - pWriter->iTextBytes)
    {
        xwork__permission_writer_fail(pWriter, XWORK_ERROR_LIMIT,
                                      "permission resource descriptor limit was exceeded");
        return false;
    }
    if (pWriter->iResourceCount == pWriter->iResourceCap)
    {
        iCap = pWriter->iResourceCap ? pWriter->iResourceCap * 2u : 4u;
        if (iCap > XWORK_PERMISSION_RESOURCE_MAX)
        {
            iCap = XWORK_PERMISSION_RESOURCE_MAX;
        }
        pNew = (xwork_permission_resource *)realloc(pWriter->pResources, iCap * sizeof(*pNew));
        if (!pNew)
        {
            xwork__permission_writer_fail(pWriter, XWORK_ERROR_OUT_OF_MEMORY,
                                          "failed to copy permission resources");
            return false;
        }
        pWriter->pResources = pNew;
        pWriter->iResourceCap = iCap;
    }
    sCopy = xwork__strdup(sResource);
    if (!sCopy)
    {
        xwork__permission_writer_fail(pWriter, XWORK_ERROR_OUT_OF_MEMORY,
                                      "failed to copy a permission resource");
        return false;
    }
    pNew = &pWriter->pResources[pWriter->iResourceCount++];
    pNew->eKind = eKind;
    pNew->uAccess = uAccess;
    pNew->sResource = sCopy;
    pWriter->iTextBytes += iLength;
    return true;
}

static void xwork__result_writer_fail(xwork_tool_result_writer *pWriter, xwork_error_code eCode,
                                      const char *sMessage)
{
    if (!pWriter || pWriter->bFailed)
        return;
    pWriter->bFailed = true;
    pWriter->eFailure = eCode;
    (void)snprintf(pWriter->sFailure, sizeof(pWriter->sFailure), "%s",
                   sMessage ? sMessage : "tool result writer failed");
}

void xwork__result_writer_init(xwork_tool_result_writer *pWriter, xwork_tool_output *pOutput,
                               size_t iMaxBytes)
{
    if (!pWriter)
        return;
    memset(pWriter, 0, sizeof(*pWriter));
    pWriter->uMagic = XWORK_RESULT_WRITER_MAGIC;
    pWriter->pOutput = pOutput;
    pWriter->iMaxBytes = iMaxBytes;
}

bool xworkToolResultWriterSetSuccess(xwork_tool_result_writer *pWriter, bool bSuccess)
{
    if (!pWriter || pWriter->uMagic != XWORK_RESULT_WRITER_MAGIC || pWriter->bFailed)
        return false;
    if (pWriter->bStatusSet)
    {
        xwork__result_writer_fail(pWriter, XWORK_ERROR_INVALID_ARGUMENT,
                                  "tool result success was set more than once");
        return false;
    }
    pWriter->pOutput->bSuccess = bSuccess;
    pWriter->bStatusSet = true;
    return true;
}

bool xworkToolResultWriterWrite(xwork_tool_result_writer *pWriter, const char *pText,
                                size_t iLength)
{
    if (!pWriter || pWriter->uMagic != XWORK_RESULT_WRITER_MAGIC || pWriter->bFailed)
        return false;
    if ((!pText && iLength) || (iLength && memchr(pText, '\0', iLength)))
    {
        xwork__result_writer_fail(pWriter, XWORK_ERROR_INVALID_ARGUMENT,
                                  "tool result text must not contain embedded NUL bytes");
        return false;
    }
    if (iLength > pWriter->iMaxBytes - pWriter->iImageBytes ||
        pWriter->tText.iLen > pWriter->iMaxBytes - pWriter->iImageBytes - iLength)
    {
        xwork__result_writer_fail(pWriter, XWORK_ERROR_LIMIT,
                                  "tool result writer byte limit was exceeded");
        return false;
    }
    if (iLength && !xwork__buf_append(&pWriter->tText, pText, iLength))
    {
        xwork__result_writer_fail(pWriter, XWORK_ERROR_OUT_OF_MEMORY,
                                  "failed to copy tool result text");
        return false;
    }
    return true;
}

bool xworkToolResultWriterWriteText(xwork_tool_result_writer *pWriter, const char *sText)
{
    return sText && xworkToolResultWriterWrite(pWriter, sText, strlen(sText));
}

bool xworkToolResultWriterSetImage(xwork_tool_result_writer *pWriter, const unsigned char *pData,
                                   size_t iSize, const char *sMime)
{
    if (!pWriter || pWriter->uMagic != XWORK_RESULT_WRITER_MAGIC || pWriter->bFailed)
        return false;
    if (!pData || !iSize || !sMime || !sMime[0] || strlen(sMime) >= 32u)
    {
        xwork__result_writer_fail(pWriter, XWORK_ERROR_INVALID_ARGUMENT,
                                  "invalid tool result image");
        return false;
    }
    if (iSize > pWriter->iMaxBytes || pWriter->tText.iLen > pWriter->iMaxBytes - iSize)
    {
        xwork__result_writer_fail(pWriter, XWORK_ERROR_LIMIT,
                                  "tool result writer byte limit was exceeded");
        return false;
    }
    if (!xworkToolOutputSetImage(pWriter->pOutput, pData, iSize, sMime))
    {
        xwork__result_writer_fail(pWriter, XWORK_ERROR_OUT_OF_MEMORY,
                                  "failed to copy tool result image");
        return false;
    }
    pWriter->iImageBytes = iSize;
    return true;
}

bool xwork__result_writer_finish(xwork_tool_result_writer *pWriter, xwork_error *pError)
{
    if (!pWriter || pWriter->uMagic != XWORK_RESULT_WRITER_MAGIC)
    {
        xwork__set_error(pError, XWORK_ERROR_CONTEXT, "tool result writer is unavailable");
        return false;
    }
    if (pWriter->bFailed)
    {
        xwork__set_error(pError, pWriter->eFailure ? pWriter->eFailure : XWORK_ERROR_TOOL,
                         pWriter->sFailure[0] ? pWriter->sFailure : "tool result writer failed");
        return false;
    }
    if (!pWriter->bStatusSet || !pWriter->tText.iLen)
    {
        xwork__set_error(pError, XWORK_ERROR_TOOL,
                         !pWriter->bStatusSet ? "tool result writer did not set success"
                                              : "tool result writer produced no text");
        return false;
    }
    if (!xrtUtf8Valid(xrtStrViewN(pWriter->tText.pData, pWriter->tText.iLen), NULL))
    {
        xwork__set_error(pError, XWORK_ERROR_TOOL, "tool result is not valid UTF-8 text");
        return false;
    }
    pWriter->pOutput->sContent = xwork__buf_detach(&pWriter->tText);
    if (!pWriter->pOutput->sContent)
    {
        xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to finalize tool result text");
        return false;
    }
    pWriter->uMagic = 0u;
    return true;
}

void xwork__result_writer_unit(xwork_tool_result_writer *pWriter)
{
    if (!pWriter)
        return;
    pWriter->uMagic = 0u;
    free(pWriter->sArtifactPath);
    xwork__buf_unit(&pWriter->tText);
    memset(pWriter, 0, sizeof(*pWriter));
}
