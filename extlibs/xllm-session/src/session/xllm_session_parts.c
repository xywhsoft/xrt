#include "../internal/xllm_session_internal.h"

/* v4 owns the complete message, not merely its text fast path. Binary data
 * stays inline so byte restore never fetches a URL or opens an external file.
 * Standard padded base64 is canonical; decoded size is validated before any
 * allocation and cannot exceed the encoded input's byte budget. */
static bool xllm_session__write_nullable(xllm_session_buf *pJson, const char *sText)
{
    return sText ? xllm_session__json_string(pJson, sText) : xllm_session__buf_cstr(pJson, "null");
}

bool xllm_session__write_parts(xllm_session_buf *pJson, const xllm_message *pMessage)
{
    size_t i;
    if (!xllm_session__buf_cstr(pJson, ",\"native\":") ||
        !xllm_session__write_nullable(pJson, pMessage->sNative) ||
        !xllm_session__buf_cstr(pJson, ",\"parts\":["))
        return false;
    for (i = 0u; i < pMessage->iPartCount; ++i)
    {
        const xllm_part *pPart = &pMessage->pParts[i];
        size_t iBytes = 0u;
        char *sData = NULL;
        bool bOk;
        if ((i && !xllm_session__buf_char(pJson, ',')) ||
            !xllm_session__buf_cstr(pJson, "{\"kind\":") ||
            !xllm_session__buf_u64(pJson, pPart->eKind) ||
            !xllm_session__buf_cstr(pJson, ",\"text\":") ||
            !xllm_session__write_nullable(pJson, pPart->sText) ||
            !xllm_session__buf_cstr(pJson, ",\"native_type\":") ||
            !xllm_session__write_nullable(pJson, pPart->sNativeType) ||
            !xllm_session__buf_cstr(pJson, ",\"media_type\":") ||
            !xllm_session__write_nullable(pJson, pPart->sMediaType) ||
            !xllm_session__buf_cstr(pJson, ",\"source_url\":") ||
            !xllm_session__write_nullable(pJson, pPart->sSourceUrl) ||
            !xllm_session__buf_cstr(pJson, ",\"detail\":") ||
            !xllm_session__write_nullable(pJson, pPart->sDetail) ||
            !xllm_session__buf_cstr(pJson, ",\"data_bytes\":") ||
            !xllm_session__buf_u64(pJson, pPart->iDataSize) ||
            !xllm_session__buf_cstr(pJson, ",\"data\":"))
            return false;
        if (pPart->iDataSize == 0u)
        {
            if (!xllm_session__buf_cstr(pJson, "null}"))
                return false;
            continue;
        }
        if (!xrtBase64Encode(pPart->pData, pPart->iDataSize, NULL, 0u, &iBytes, NULL) ||
            iBytes == SIZE_MAX)
            return false;
        sData = (char *)malloc(iBytes + 1u);
        if (!sData)
            return false;
        bOk = xrtBase64Encode(pPart->pData, pPart->iDataSize, sData, iBytes + 1u, &iBytes, NULL) &&
              xllm_session__json_string(pJson, sData) && xllm_session__buf_char(pJson, '}');
        free(sData);
        if (!bOk)
            return false;
    }
    return xllm_session__buf_char(pJson, ']');
}

static xvalue *xllm_session__part_field(xvalue *pObject, const char *sKey)
{
    return xrtValueObjectGet(pObject, (xstrview){sKey, strlen(sKey)});
}

static bool xllm_session__part_text(xvalue *pObject, const char *sKey, char **psText)
{
    xvalue *pValue = xllm_session__part_field(pObject, sKey);
    xstrview tText;
    *psText = NULL;
    if (!pValue)
        return false;
    if (xrtValueIs(pValue, XVALUE_NULL))
        return true;
    if (!xrtValueGetString(pValue, &tText) || memchr(tText.Data, 0, tText.Size))
        return false;
    *psText = (char *)tText.Data;
    return true;
}

static bool xllm_session__part_uint(xvalue *pObject, const char *sKey, uint64_t *puValue)
{
    xvalue *pValue = xllm_session__part_field(pObject, sKey);
    int64 iSigned;
    if (xrtValueIs(pValue, XVALUE_UINT))
        return xrtValueGetUInt(pValue, puValue);
    if (!xrtValueIs(pValue, XVALUE_INT) || !xrtValueGetInt(pValue, &iSigned) || iSigned < 0)
        return false;
    *puValue = (uint64_t)iSigned;
    return true;
}

static bool xllm_session__part_keys(xvalue *pObject)
{
    static const char *const tKeys[] = {"kind",       "text",   "native_type", "media_type",
                                        "source_url", "detail", "data_bytes",  "data"};
    size_t i, j;
    if (!xrtValueIs(pObject, XVALUE_OBJECT) || xrtValueCount(pObject) != 8u)
        return false;
    for (i = 0u; i < 8u; ++i)
    {
        xstrview tKey;
        (void)xrtValueObjectAt(pObject, i, &tKey);
        for (j = 0u; j < 8u; ++j)
            if (tKey.Size == strlen(tKeys[j]) && memcmp(tKey.Data, tKeys[j], tKey.Size) == 0)
                break;
        if (j == 8u)
            return false;
    }
    return true;
}

#if defined(XLLM_SESSION_TEST_FAULTS)
static xcancel *xllm_session__test_parts_cancel;
#endif

bool xllm_session__load_parts(xllm_message *pMessage, xvalue *pEntry, uint64_t uVersion,
                              const xllm_session_restore_options *pOptions, xllm_error *pError)
{
    xvalue *pParts = xllm_session__part_field(pEntry, "parts");
    xvalue *pNative = xllm_session__part_field(pEntry, "native");
    char *sNative;
    size_t i;
    if (uVersion < 4u)
    {
        if (pParts || pNative)
            goto invalid; /* no silent version downgrade */
        return true;
    }
    if (!xrtValueIs(pParts, XVALUE_ARRAY) || !xllm_session__part_text(pEntry, "native", &sNative) ||
        (xrtValueCount(pParts) != 0u && pMessage->sContent != NULL))
        goto invalid;
    if (sNative)
    {
        if (!xrtJsonValid((xstrview){sNative, strlen(sNative)}))
            goto invalid;
        if (!xllmMessageSetNative(pMessage, sNative))
        {
            xllm_session__error(pError, XLLM_ERROR_OUT_OF_MEMORY,
                                "cannot copy persisted native message");
            return false;
        }
    }
    for (i = 0u; i < xrtValueCount(pParts); ++i)
    {
        xvalue *pValue = xrtValueArrayGet(pParts, i);
        xvalue *pData;
        xllm_part tPart = {0}; /* Strings borrow the tree until AddPart deep copies. */
        uint64_t uKind, uBytes;
        xstrview tData;
        size_t iDecoded = 0u;
        bool bOk;
        if (!xllm_session__restore_check(pOptions, pError))
            return false;
        if (!xllm_session__part_keys(pValue) || !xllm_session__part_uint(pValue, "kind", &uKind) ||
            uKind > XLLM_PART_NATIVE || !xllm_session__part_uint(pValue, "data_bytes", &uBytes) ||
            uBytes > SIZE_MAX || !xllm_session__part_text(pValue, "text", &tPart.sText) ||
            !xllm_session__part_text(pValue, "native_type", &tPart.sNativeType) ||
            !xllm_session__part_text(pValue, "media_type", &tPart.sMediaType) ||
            !xllm_session__part_text(pValue, "source_url", &tPart.sSourceUrl) ||
            !xllm_session__part_text(pValue, "detail", &tPart.sDetail))
            goto invalid;
        tPart.eKind = (xllm_part_kind)uKind;
        pData = xllm_session__part_field(pValue, "data");
        if (xrtValueIs(pData, XVALUE_NULL))
        {
            if (uBytes != 0u)
                goto invalid;
        }
        else
        {
            if (!xrtValueGetString(pData, &tData) || uBytes == 0u ||
                !xrtBase64Decode(tData.Data, tData.Size, NULL, 0u, &iDecoded, NULL) ||
                iDecoded != (size_t)uBytes)
                goto invalid;
            tPart.pData = (uint8_t *)malloc(iDecoded);
            if (!tPart.pData)
            {
                xllm_session__error(pError, XLLM_ERROR_OUT_OF_MEMORY,
                                    "cannot allocate persisted part bytes");
                return false;
            }
            tPart.iDataSize = iDecoded;
            if (!xrtBase64Decode(tData.Data, tData.Size, tPart.pData, iDecoded, &iDecoded, NULL))
            {
                free(tPart.pData);
                goto invalid;
            }
        }
        bOk = xllmMessageAddPart(pMessage, &tPart);
        free(tPart.pData);
        if (!bOk)
        {
            xllm_session__error(pError, XLLM_ERROR_OUT_OF_MEMORY,
                                "cannot copy persisted message part");
            return false;
        }
#if defined(XLLM_SESSION_TEST_FAULTS)
        if (xllm_session__test_parts_cancel)
            (void)xrtCancelRequest(xllm_session__test_parts_cancel);
#endif
    }
    return xllm_session__restore_check(pOptions, pError);
invalid:
    xllm_session__error(pError, XLLM_ERROR_PARSE, "invalid persisted message parts");
    return false;
}
