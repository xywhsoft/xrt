#include "xllm_memory_internal.h"

char* xllm_memory__strdup(const char* sText)
{
    size_t iLen;
    char* sCopy;
    if ( !sText ) return NULL;
    iLen = strlen(sText);
    sCopy = (char*)malloc(iLen + 1u);
    if ( !sCopy ) return NULL;
    memcpy(sCopy, sText, iLen + 1u);
    return sCopy;
}

char* xllm_memory__strndup(const char* sText, size_t iLen)
{
    char* sCopy;
    if ( !sText || iLen == SIZE_MAX ) return NULL;
    sCopy = (char*)malloc(iLen + 1u);
    if ( !sCopy ) return NULL;
    memcpy(sCopy, sText, iLen);
    sCopy[iLen] = '\0';
    return sCopy;
}

void xllm_memory__error(xllm_error* pError, xllm_error_code eCode, const char* sMessage)
{
    size_t iLen;
    if ( !pError ) return;
    xllmErrorInit(pError);
    pError->eCode = eCode;
    if ( !sMessage ) return;
    iLen = strlen(sMessage);
    if ( iLen >= sizeof(pError->sMessage) ) iLen = sizeof(pError->sMessage) - 1u;
    memcpy(pError->sMessage, sMessage, iLen);
    pError->sMessage[iLen] = '\0';
}

int64_t xllm_memory__now_unix(void)
{
    return (int64_t)xrtTimeUnix(xrtNow());
}

bool xllm_memory__buf_append(xllm_memory_buf* pBuf, const void* pData, size_t iLen)
{
    size_t iNeed;
    size_t iCap;
    char* pNew;
    if ( !pBuf || (!pData && iLen) || pBuf->iLen > SIZE_MAX - iLen - 1u ) return false;
    iNeed = pBuf->iLen + iLen + 1u;
    if ( iNeed > pBuf->iCap ) {
        iCap = pBuf->iCap ? pBuf->iCap : 512u;
        while ( iCap < iNeed ) {
            if ( iCap > SIZE_MAX / 2u ) { iCap = iNeed; break; }
            iCap *= 2u;
        }
        pNew = (char*)realloc(pBuf->pData, iCap);
        if ( !pNew ) return false;
        pBuf->pData = pNew;
        pBuf->iCap = iCap;
    }
    if ( iLen ) memcpy(pBuf->pData + pBuf->iLen, pData, iLen);
    pBuf->iLen += iLen;
    pBuf->pData[pBuf->iLen] = '\0';
    return true;
}

bool xllm_memory__buf_cstr(xllm_memory_buf* pBuf, const char* sText)
{
    return xllm_memory__buf_append(pBuf, sText ? sText : "", sText ? strlen(sText) : 0u);
}

bool xllm_memory__buf_char(xllm_memory_buf* pBuf, char ch)
{
    return xllm_memory__buf_append(pBuf, &ch, 1u);
}

bool xllm_memory__buf_u64(xllm_memory_buf* pBuf, uint64_t uValue)
{
    char sValue[32];
    (void)snprintf(sValue, sizeof(sValue), "%llu", (unsigned long long)uValue);
    return xllm_memory__buf_cstr(pBuf, sValue);
}

bool xllm_memory__buf_i64(xllm_memory_buf* pBuf, int64_t iValue)
{
    char sValue[32];
    (void)snprintf(sValue, sizeof(sValue), "%lld", (long long)iValue);
    return xllm_memory__buf_cstr(pBuf, sValue);
}

bool xllm_memory__buf_double(xllm_memory_buf* pBuf, double fValue)
{
    char sValue[64];
    (void)snprintf(sValue, sizeof(sValue), "%.17g", fValue);
    return xllm_memory__buf_cstr(pBuf, sValue);
}

bool xllm_memory__json_string(xllm_memory_buf* pBuf, const char* sText)
{
    const unsigned char* p = (const unsigned char*)(sText ? sText : "");
    char sEscape[7];
    if ( !xllm_memory__buf_char(pBuf, '"') ) return false;
    while ( *p ) {
        switch ( *p ) {
            case '"': if ( !xllm_memory__buf_cstr(pBuf, "\\\"") ) return false; break;
            case '\\': if ( !xllm_memory__buf_cstr(pBuf, "\\\\") ) return false; break;
            case '\b': if ( !xllm_memory__buf_cstr(pBuf, "\\b") ) return false; break;
            case '\f': if ( !xllm_memory__buf_cstr(pBuf, "\\f") ) return false; break;
            case '\n': if ( !xllm_memory__buf_cstr(pBuf, "\\n") ) return false; break;
            case '\r': if ( !xllm_memory__buf_cstr(pBuf, "\\r") ) return false; break;
            case '\t': if ( !xllm_memory__buf_cstr(pBuf, "\\t") ) return false; break;
            default:
                if ( *p < 0x20u ) {
                    (void)snprintf(sEscape, sizeof(sEscape), "\\u%04x", (unsigned)*p);
                    if ( !xllm_memory__buf_cstr(pBuf, sEscape) ) return false;
                } else if ( !xllm_memory__buf_append(pBuf, p, 1u) ) return false;
                break;
        }
        ++p;
    }
    return xllm_memory__buf_char(pBuf, '"');
}

char* xllm_memory__buf_detach(xllm_memory_buf* pBuf)
{
    char* pData;
    if ( !pBuf ) return NULL;
    if ( !pBuf->pData ) {
        pBuf->pData = (char*)calloc(1u, 1u);
        if ( !pBuf->pData ) return NULL;
    }
    pData = pBuf->pData;
    memset(pBuf, 0, sizeof(*pBuf));
    return pData;
}

void xllm_memory__buf_unit(xllm_memory_buf* pBuf)
{
    if ( !pBuf ) return;
    free(pBuf->pData);
    memset(pBuf, 0, sizeof(*pBuf));
}

static uint64_t xllm_memory__hash_bytes(uint64_t uHash, const void* pData, size_t iLen)
{
    const unsigned char* p = (const unsigned char*)pData;
    while ( iLen-- ) { uHash ^= *p++; uHash *= 1099511628211ull; }
    return uHash;
}

static uint64_t xllm_memory__hash_cstr(uint64_t uHash, const char* sText)
{
    static const unsigned char uSeparator = 0xffu;
    if ( sText ) uHash = xllm_memory__hash_bytes(uHash, sText, strlen(sText));
    return xllm_memory__hash_bytes(uHash, &uSeparator, 1u);
}

static uint64_t xllm_memory__record_hash(const xllm_memory_record_internal* pRecord)
{
    uint64_t uHash = 1469598103934665603ull;
    char sNumbers[160];
    uHash = xllm_memory__hash_cstr(uHash, pRecord->sRecordId);
    uHash = xllm_memory__hash_cstr(uHash, pRecord->sTitle);
    uHash = xllm_memory__hash_cstr(uHash, pRecord->sSourceUri);
    uHash = xllm_memory__hash_cstr(uHash, pRecord->sText);
    uHash = xllm_memory__hash_cstr(uHash, pRecord->sActor);
    uHash = xllm_memory__hash_cstr(uHash, pRecord->sReason);
    (void)snprintf(sNumbers, sizeof(sNumbers), "%d:%d:%d:%d:%d:%lld",
        (int)pRecord->eScope, (int)pRecord->eKind, (int)pRecord->eTrust,
        (int)pRecord->eSensitivity, (int)pRecord->iPriority,
        (long long)pRecord->iExpiresAtUnix);
    return xllm_memory__hash_cstr(uHash, sNumbers);
}

static void xllm_memory__record_unit(xllm_memory_record_internal* pRecord)
{
    if ( !pRecord ) return;
    free(pRecord->sRecordId);
    free(pRecord->sTitle);
    free(pRecord->sSourceUri);
    free(pRecord->sText);
    free(pRecord->sActor);
    free(pRecord->sReason);
    memset(pRecord, 0, sizeof(*pRecord));
}

static bool xllm_memory__valid_record_id(const char* sRecordId)
{
    size_t i;
    size_t iLen = sRecordId ? strlen(sRecordId) : 0u;
    if ( iLen == 0u || iLen >= 128u ) return false;
    for ( i = 0u; i < iLen; ++i ) {
        unsigned char ch = (unsigned char)sRecordId[i];
        if ( ch <= 0x20u || ch == 0x7fu ) return false;
    }
    return true;
}

static bool xllm_memory__record_from_input(xllm_memory_record_internal* pRecord,
    const xllm_memory_record_input* pInput, const xllm_memory_record_internal* pPrevious,
    xllm_error* pError)
{
    int64_t iNow = xllm_memory__now_unix();
    if ( !pRecord || !pInput || pInput->eScope < XLLM_MEMORY_SCOPE_MEMORY ||
         pInput->eScope > XLLM_MEMORY_SCOPE_KNOWLEDGE ||
         pInput->eKind < XLLM_MEMORY_KIND_FACT || pInput->eKind > XLLM_MEMORY_KIND_KNOWLEDGE ||
         pInput->eTrust < XLLM_MEMORY_TRUST_UNTRUSTED || pInput->eTrust > XLLM_MEMORY_TRUST_USER_APPROVED ||
         pInput->eSensitivity < XLLM_MEMORY_SENSITIVITY_PUBLIC ||
         pInput->eSensitivity > XLLM_MEMORY_SENSITIVITY_SECRET ||
         !xllm_memory__valid_record_id(pInput->sRecordId) || !pInput->sSourceUri || !pInput->sSourceUri[0] ||
         !pInput->sText || !pInput->sText[0] || !pInput->sActor || !pInput->sActor[0] ) {
        xllm_memory__error(pError, XLLM_ERROR_INVALID_ARGUMENT,
            "memory record requires valid scope, kind, id, source URI, text, actor, trust, and sensitivity");
        return false;
    }
    if ( pPrevious && pPrevious->uRecordRevision == UINT64_MAX ) {
        xllm_memory__error(pError, XLLM_ERROR_PROTOCOL, "memory record revision exhausted");
        return false;
    }
    memset(pRecord, 0, sizeof(*pRecord));
    pRecord->eScope = pInput->eScope;
    pRecord->eKind = pInput->eKind;
    pRecord->eTrust = pInput->eTrust;
    pRecord->eSensitivity = pInput->eSensitivity;
    pRecord->iPriority = pInput->iPriority;
    pRecord->iCreatedAtUnix = pInput->iCreatedAtUnix ? pInput->iCreatedAtUnix :
        (pPrevious ? pPrevious->iCreatedAtUnix : iNow);
    pRecord->iUpdatedAtUnix = pInput->iUpdatedAtUnix ? pInput->iUpdatedAtUnix : iNow;
    pRecord->iExpiresAtUnix = pInput->iExpiresAtUnix;
    pRecord->uRecordRevision = pPrevious ? pPrevious->uRecordRevision + 1u : 1u;
    pRecord->sRecordId = xllm_memory__strdup(pInput->sRecordId);
    pRecord->sTitle = xllm_memory__strdup(pInput->sTitle ? pInput->sTitle : "");
    pRecord->sSourceUri = xllm_memory__strdup(pInput->sSourceUri);
    pRecord->sText = xllm_memory__strdup(pInput->sText);
    pRecord->sActor = xllm_memory__strdup(pInput->sActor);
    pRecord->sReason = xllm_memory__strdup(pInput->sReason ? pInput->sReason : "");
    if ( !pRecord->sRecordId || !pRecord->sTitle || !pRecord->sSourceUri ||
         !pRecord->sText || !pRecord->sActor || !pRecord->sReason ) {
        xllm_memory__record_unit(pRecord);
        xllm_memory__error(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to allocate memory record");
        return false;
    }
    pRecord->uContentHash = xllm_memory__record_hash(pRecord);
    return true;
}

static bool xllm_memory__same_content(const xllm_memory_record_internal* pA,
    const xllm_memory_record_internal* pB)
{
    return pA->uContentHash == pB->uContentHash && pA->eScope == pB->eScope &&
        pA->eKind == pB->eKind && pA->eTrust == pB->eTrust &&
        pA->eSensitivity == pB->eSensitivity && pA->iPriority == pB->iPriority &&
        pA->iExpiresAtUnix == pB->iExpiresAtUnix && strcmp(pA->sRecordId, pB->sRecordId) == 0 &&
        strcmp(pA->sTitle, pB->sTitle) == 0 && strcmp(pA->sSourceUri, pB->sSourceUri) == 0 &&
        strcmp(pA->sText, pB->sText) == 0 && strcmp(pA->sActor, pB->sActor) == 0 &&
        strcmp(pA->sReason, pB->sReason) == 0;
}

const xllm_memory_record_internal* xllm_memory__find(const xllm_memory* pMemory,
    const char* sRecordId, size_t* pIndex)
{
    size_t i;
    if ( !pMemory || !sRecordId ) return NULL;
    for ( i = 0u; i < pMemory->iRecordCount; ++i ) {
        if ( strcmp(pMemory->pRecords[i].sRecordId, sRecordId) == 0 ) {
            if ( pIndex ) *pIndex = i;
            return &pMemory->pRecords[i];
        }
    }
    return NULL;
}

static bool xllm_memory__record_json(xllm_memory_buf* pJson,
    const xllm_memory_record_internal* pRecord)
{
    char sHash[17];
    (void)snprintf(sHash, sizeof(sHash), "%016llx", (unsigned long long)pRecord->uContentHash);
    return xllm_memory__buf_cstr(pJson, "{\"scope\":") &&
        xllm_memory__buf_u64(pJson, (uint64_t)pRecord->eScope) &&
        xllm_memory__buf_cstr(pJson, ",\"kind\":") &&
        xllm_memory__buf_u64(pJson, (uint64_t)pRecord->eKind) &&
        xllm_memory__buf_cstr(pJson, ",\"trust\":") &&
        xllm_memory__buf_u64(pJson, (uint64_t)pRecord->eTrust) &&
        xllm_memory__buf_cstr(pJson, ",\"sensitivity\":") &&
        xllm_memory__buf_u64(pJson, (uint64_t)pRecord->eSensitivity) &&
        xllm_memory__buf_cstr(pJson, ",\"record_id\":") && xllm_memory__json_string(pJson, pRecord->sRecordId) &&
        xllm_memory__buf_cstr(pJson, ",\"title\":") && xllm_memory__json_string(pJson, pRecord->sTitle) &&
        xllm_memory__buf_cstr(pJson, ",\"source_uri\":") && xllm_memory__json_string(pJson, pRecord->sSourceUri) &&
        xllm_memory__buf_cstr(pJson, ",\"text\":") && xllm_memory__json_string(pJson, pRecord->sText) &&
        xllm_memory__buf_cstr(pJson, ",\"actor\":") && xllm_memory__json_string(pJson, pRecord->sActor) &&
        xllm_memory__buf_cstr(pJson, ",\"reason\":") && xllm_memory__json_string(pJson, pRecord->sReason) &&
        xllm_memory__buf_cstr(pJson, ",\"priority\":") && xllm_memory__buf_i64(pJson, pRecord->iPriority) &&
        xllm_memory__buf_cstr(pJson, ",\"created_at_unix\":") && xllm_memory__buf_i64(pJson, pRecord->iCreatedAtUnix) &&
        xllm_memory__buf_cstr(pJson, ",\"updated_at_unix\":") && xllm_memory__buf_i64(pJson, pRecord->iUpdatedAtUnix) &&
        xllm_memory__buf_cstr(pJson, ",\"expires_at_unix\":") && xllm_memory__buf_i64(pJson, pRecord->iExpiresAtUnix) &&
        xllm_memory__buf_cstr(pJson, ",\"record_revision\":") && xllm_memory__buf_u64(pJson, pRecord->uRecordRevision) &&
        xllm_memory__buf_cstr(pJson, ",\"content_hash\":") && xllm_memory__json_string(pJson, sHash) &&
        xllm_memory__buf_char(pJson, '}');
}

static bool xllm_memory__save(const xllm_memory* pMemory, xllm_error* pError)
{
    xllm_memory_buf tJson = {0};
    char* sJson = NULL;
    size_t i;
    bool bOk;
    if ( !xllm_memory__buf_cstr(&tJson, "{\"format\":\"xllm-memory\",\"version\":1,\"namespace\":") ||
         !xllm_memory__json_string(&tJson, pMemory->sNamespace) ||
         !xllm_memory__buf_cstr(&tJson, ",\"store_revision\":") ||
         !xllm_memory__buf_u64(&tJson, pMemory->uStoreRevision) ||
         !xllm_memory__buf_cstr(&tJson, ",\"records\":[") ) goto oom;
    for ( i = 0u; i < pMemory->iRecordCount; ++i ) {
        if ( i && !xllm_memory__buf_char(&tJson, ',') ) goto oom;
        if ( !xllm_memory__record_json(&tJson, &pMemory->pRecords[i]) ) goto oom;
    }
    if ( !xllm_memory__buf_cstr(&tJson, "]}") ) goto oom;
    sJson = xllm_memory__buf_detach(&tJson);
    if ( !sJson ) goto oom;
    bOk = xrtFileWriteAtomic(pMemory->sPath,
        (xbytesview){ (const uint8*)sJson, strlen(sJson) });
    free(sJson);
    if ( !bOk ) xllm_memory__error(pError, XLLM_ERROR_NETWORK, "failed to atomically persist memory store");
    return bOk;
oom:
    free(sJson);
    xllm_memory__buf_unit(&tJson);
    xllm_memory__error(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to serialize memory store");
    return false;
}

static xvalue* xllm_memory__json_get(xvalue* pObject, const char* sKey)
{
    xvalue* pValue = pObject && xrtValueIs(pObject, XVALUE_OBJECT) ?
        xrtValueObjectGet(pObject, (xstrview){ sKey, strlen(sKey) }) : NULL;
    return pValue && !xrtValueIs(pValue, XVALUE_NULL) ? pValue : NULL;
}

static const char* xllm_memory__json_text(xvalue* pObject, const char* sKey)
{
    xvalue* pValue = xllm_memory__json_get(pObject, sKey);
    xstrview tText;
    return pValue && xrtValueGetString(pValue, &tText) ? tText.Data : NULL;
}

static uint64_t xllm_memory__json_u64(xvalue* pObject, const char* sKey, uint64_t uDefault)
{
    xvalue* pValue = xllm_memory__json_get(pObject, sKey);
    int64 iValue;
    return pValue && xrtValueGetInt(pValue, &iValue) && iValue >= 0 ?
        (uint64_t)iValue : uDefault;
}

static int64_t xllm_memory__json_i64(xvalue* pObject, const char* sKey, int64_t iDefault)
{
    xvalue* pValue = xllm_memory__json_get(pObject, sKey);
    int64 iValue;
    return pValue && xrtValueGetInt(pValue, &iValue) ? (int64_t)iValue : iDefault;
}

static bool xllm_memory__parse_hash(const char* sText, uint64_t* pHash)
{
    uint64_t uValue = 0u;
    size_t i;
    if ( !sText || strlen(sText) != 16u || !pHash ) return false;
    for ( i = 0u; i < 16u; ++i ) {
        unsigned char ch = (unsigned char)sText[i];
        unsigned uDigit;
        if ( ch >= '0' && ch <= '9' ) uDigit = ch - '0';
        else if ( ch >= 'a' && ch <= 'f' ) uDigit = ch - 'a' + 10u;
        else if ( ch >= 'A' && ch <= 'F' ) uDigit = ch - 'A' + 10u;
        else return false;
        uValue = (uValue << 4u) | uDigit;
    }
    *pHash = uValue;
    return true;
}

static bool xllm_memory__reserve(xllm_memory* pMemory, size_t iNeed)
{
    size_t iCap;
    xllm_memory_record_internal* pNew;
    if ( iNeed <= pMemory->iRecordCap ) return true;
    iCap = pMemory->iRecordCap ? pMemory->iRecordCap * 2u : 16u;
    while ( iCap < iNeed ) {
        if ( iCap > SIZE_MAX / 2u ) { iCap = iNeed; break; }
        iCap *= 2u;
    }
    if ( iCap > SIZE_MAX / sizeof(*pNew) ) return false;
    pNew = (xllm_memory_record_internal*)realloc(pMemory->pRecords, iCap * sizeof(*pNew));
    if ( !pNew ) return false;
    memset(pNew + pMemory->iRecordCap, 0, (iCap - pMemory->iRecordCap) * sizeof(*pNew));
    pMemory->pRecords = pNew;
    pMemory->iRecordCap = iCap;
    return true;
}

static bool xllm_memory__load_record(xllm_memory* pMemory, xvalue* pItem)
{
    xllm_memory_record_internal tRecord;
    const char* sRecordId = xllm_memory__json_text(pItem, "record_id");
    const char* sTitle = xllm_memory__json_text(pItem, "title");
    const char* sSourceUri = xllm_memory__json_text(pItem, "source_uri");
    const char* sText = xllm_memory__json_text(pItem, "text");
    const char* sActor = xllm_memory__json_text(pItem, "actor");
    const char* sReason = xllm_memory__json_text(pItem, "reason");
    const char* sHash = xllm_memory__json_text(pItem, "content_hash");
    memset(&tRecord, 0, sizeof(tRecord));
    tRecord.eScope = (xllm_memory_scope)xllm_memory__json_u64(pItem, "scope", 0u);
    tRecord.eKind = (xllm_memory_kind)xllm_memory__json_u64(pItem, "kind", 0u);
    tRecord.eTrust = (xllm_memory_trust)xllm_memory__json_u64(pItem, "trust", UINT64_MAX);
    tRecord.eSensitivity = (xllm_memory_sensitivity)xllm_memory__json_u64(pItem, "sensitivity", UINT64_MAX);
    tRecord.iPriority = (int32_t)xllm_memory__json_i64(pItem, "priority", 0);
    tRecord.iCreatedAtUnix = xllm_memory__json_i64(pItem, "created_at_unix", 0);
    tRecord.iUpdatedAtUnix = xllm_memory__json_i64(pItem, "updated_at_unix", 0);
    tRecord.iExpiresAtUnix = xllm_memory__json_i64(pItem, "expires_at_unix", 0);
    tRecord.uRecordRevision = xllm_memory__json_u64(pItem, "record_revision", 0u);
    if ( !xllm_memory__parse_hash(sHash, &tRecord.uContentHash) ) return false;
    if ( tRecord.eScope < XLLM_MEMORY_SCOPE_MEMORY || tRecord.eScope > XLLM_MEMORY_SCOPE_KNOWLEDGE ||
         tRecord.eKind < XLLM_MEMORY_KIND_FACT || tRecord.eKind > XLLM_MEMORY_KIND_KNOWLEDGE ||
         tRecord.eTrust < XLLM_MEMORY_TRUST_UNTRUSTED || tRecord.eTrust > XLLM_MEMORY_TRUST_USER_APPROVED ||
         tRecord.eSensitivity < XLLM_MEMORY_SENSITIVITY_PUBLIC ||
         tRecord.eSensitivity > XLLM_MEMORY_SENSITIVITY_SECRET ||
         !xllm_memory__valid_record_id(sRecordId) || !sSourceUri || !sSourceUri[0] ||
         !sText || !sText[0] || !sActor || !sActor[0] || !tRecord.uRecordRevision ||
         xllm_memory__find(pMemory, sRecordId, NULL) ) return false;
    tRecord.sRecordId = xllm_memory__strdup(sRecordId);
    tRecord.sTitle = xllm_memory__strdup(sTitle ? sTitle : "");
    tRecord.sSourceUri = xllm_memory__strdup(sSourceUri);
    tRecord.sText = xllm_memory__strdup(sText);
    tRecord.sActor = xllm_memory__strdup(sActor);
    tRecord.sReason = xllm_memory__strdup(sReason ? sReason : "");
    if ( !tRecord.sRecordId || !tRecord.sTitle || !tRecord.sSourceUri || !tRecord.sText ||
         !tRecord.sActor || !tRecord.sReason || tRecord.uContentHash != xllm_memory__record_hash(&tRecord) ||
         !xllm_memory__reserve(pMemory, pMemory->iRecordCount + 1u) ) {
        xllm_memory__record_unit(&tRecord);
        return false;
    }
    pMemory->pRecords[pMemory->iRecordCount++] = tRecord;
    return true;
}

static bool xllm_memory__load(xllm_memory* pMemory, xllm_error* pError)
{
    char* sJson;
    size_t iJsonLen = 0u;
    xvalue* pRoot;
    xvalue* pRecords;
    const char* sFormat;
    const char* sNamespace;
    size_t i;
    sJson = (char*)xrtFileReadAll(pMemory->sPath, &iJsonLen);
    if ( !sJson ) {
        xllm_memory__error(pError, XLLM_ERROR_NETWORK, "failed to read memory store");
        return false;
    }
    pRoot = xrtJsonParse((xstrview){ sJson, iJsonLen });
    xrtFree(sJson);
    sFormat = xllm_memory__json_text(pRoot, "format");
    sNamespace = xllm_memory__json_text(pRoot, "namespace");
    pRecords = xllm_memory__json_get(pRoot, "records");
    if ( !pRoot || !xrtValueIs(pRoot, XVALUE_OBJECT) || !sFormat || strcmp(sFormat, "xllm-memory") != 0 ||
         xllm_memory__json_u64(pRoot, "version", 0u) != 1u || !sNamespace ||
         strcmp(sNamespace, pMemory->sNamespace) != 0 || !pRecords ||
         !xrtValueIs(pRecords, XVALUE_ARRAY) ) goto invalid;
    pMemory->uStoreRevision = xllm_memory__json_u64(pRoot, "store_revision", 0u);
    for ( i = 0u; i < xrtValueCount(pRecords); ++i ) {
        if ( !xllm_memory__load_record(pMemory, xrtValueArrayGet(pRecords, i)) ) goto invalid;
    }
    xrtValueRelease(pRoot);
    return true;
invalid:
    xrtValueRelease(pRoot);
    xllm_memory__error(pError, XLLM_ERROR_PARSE,
        "invalid memory store, namespace mismatch, duplicate id, or content-hash mismatch");
    return false;
}

static bool xllm_memory__ensure_parent(const char* sPath)
{
    char* sDirectory = xrtPathParent(sPath);
    bool bOk;
    if ( !sDirectory || !sDirectory[0] ) {
        if ( sDirectory ) xrtFree(sDirectory);
        return true;
    }
    bOk = xrtDirExists(sDirectory) || xrtDirCreateAll(sDirectory);
    xrtFree(sDirectory);
    return bOk;
}

void xllmMemoryConfigInit(xllm_memory_config* pConfig)
{
    if ( !pConfig ) return;
    memset(pConfig, 0, sizeof(*pConfig));
    pConfig->sNamespace = "default";
    pConfig->bCreateIfMissing = true;
}

void xllmMemoryRecordInputInit(xllm_memory_record_input* pInput)
{
    if ( !pInput ) return;
    memset(pInput, 0, sizeof(*pInput));
    pInput->eScope = XLLM_MEMORY_SCOPE_MEMORY;
    pInput->eKind = XLLM_MEMORY_KIND_FACT;
    pInput->eTrust = XLLM_MEMORY_TRUST_UNTRUSTED;
    pInput->eSensitivity = XLLM_MEMORY_SENSITIVITY_INTERNAL;
}

const char* xllmMemoryScopeName(xllm_memory_scope eScope)
{
    switch ( eScope ) {
        case XLLM_MEMORY_SCOPE_ANY: return "any";
        case XLLM_MEMORY_SCOPE_MEMORY: return "memory";
        case XLLM_MEMORY_SCOPE_KNOWLEDGE: return "knowledge";
        default: return "unknown";
    }
}

const char* xllmMemoryKindName(xllm_memory_kind eKind)
{
    switch ( eKind ) {
        case XLLM_MEMORY_KIND_ANY: return "any";
        case XLLM_MEMORY_KIND_FACT: return "fact";
        case XLLM_MEMORY_KIND_PREFERENCE: return "preference";
        case XLLM_MEMORY_KIND_TASK: return "task";
        case XLLM_MEMORY_KIND_SUMMARY: return "summary";
        case XLLM_MEMORY_KIND_KNOWLEDGE: return "knowledge";
        default: return "unknown";
    }
}

const char* xllmMemoryTrustName(xllm_memory_trust eTrust)
{
    switch ( eTrust ) {
        case XLLM_MEMORY_TRUST_UNTRUSTED: return "untrusted";
        case XLLM_MEMORY_TRUST_LOCAL: return "local";
        case XLLM_MEMORY_TRUST_USER_APPROVED: return "user-approved";
        default: return "unknown";
    }
}

const char* xllmMemorySensitivityName(xllm_memory_sensitivity eSensitivity)
{
    switch ( eSensitivity ) {
        case XLLM_MEMORY_SENSITIVITY_PUBLIC: return "public";
        case XLLM_MEMORY_SENSITIVITY_INTERNAL: return "internal";
        case XLLM_MEMORY_SENSITIVITY_SENSITIVE: return "sensitive";
        case XLLM_MEMORY_SENSITIVITY_SECRET: return "secret";
        default: return "unknown";
    }
}

const char* xllmMemoryActionName(xllm_memory_action eAction)
{
    switch ( eAction ) {
        case XLLM_MEMORY_ACTION_NONE: return "none";
        case XLLM_MEMORY_ACTION_CREATED: return "created";
        case XLLM_MEMORY_ACTION_REPLACED: return "replaced";
        case XLLM_MEMORY_ACTION_UNCHANGED: return "unchanged";
        case XLLM_MEMORY_ACTION_REMOVED: return "removed";
        default: return "unknown";
    }
}

xllm_memory* xllmMemoryOpen(const xllm_memory_config* pConfig, xllm_error* pError)
{
    xllm_memory* pMemory;
    const char* sNamespace;
    if ( pError ) xllmErrorInit(pError);
    if ( !pConfig || !pConfig->sPath || !pConfig->sPath[0] ) {
        xllm_memory__error(pError, XLLM_ERROR_INVALID_ARGUMENT, "memory store path is required");
        return NULL;
    }
    sNamespace = (pConfig->sNamespace && pConfig->sNamespace[0]) ? pConfig->sNamespace : "default";
    pMemory = (xllm_memory*)calloc(1u, sizeof(*pMemory));
    if ( !pMemory ) {
        xllm_memory__error(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to allocate memory store");
        return NULL;
    }
    pMemory->sPath = xllm_memory__strdup(pConfig->sPath);
    pMemory->sNamespace = xllm_memory__strdup(sNamespace);
    if ( !pMemory->sPath || !pMemory->sNamespace || !xllm_memory__ensure_parent(pMemory->sPath) ) {
        xllmMemoryClose(pMemory);
        xllm_memory__error(pError, XLLM_ERROR_NETWORK, "failed to initialize memory store path");
        return NULL;
    }
    if ( xrtFileExists((str)pMemory->sPath) ) {
        if ( !xllm_memory__load(pMemory, pError) ) { xllmMemoryClose(pMemory); return NULL; }
    } else if ( !pConfig->bCreateIfMissing ) {
        xllmMemoryClose(pMemory);
        xllm_memory__error(pError, XLLM_ERROR_NETWORK, "memory store does not exist");
        return NULL;
    } else if ( !xllm_memory__save(pMemory, pError) ) {
        xllmMemoryClose(pMemory);
        return NULL;
    }
    return pMemory;
}

void xllmMemoryClose(xllm_memory* pMemory)
{
    size_t i;
    if ( !pMemory ) return;
    for ( i = 0u; i < pMemory->iRecordCount; ++i ) xllm_memory__record_unit(&pMemory->pRecords[i]);
    free(pMemory->pRecords);
    free(pMemory->sPath);
    free(pMemory->sNamespace);
    free(pMemory);
}

const char* xllmMemoryNamespace(const xllm_memory* pMemory)
{
    return pMemory ? pMemory->sNamespace : NULL;
}

const char* xllmMemoryPath(const xllm_memory* pMemory)
{
    return pMemory ? pMemory->sPath : NULL;
}

bool xllmMemoryGetStats(const xllm_memory* pMemory, int64_t iNowUnix, xllm_memory_stats* pStats)
{
    size_t i;
    if ( !pMemory || !pStats ) return false;
    if ( !iNowUnix ) iNowUnix = xllm_memory__now_unix();
    memset(pStats, 0, sizeof(*pStats));
    pStats->uStoreRevision = pMemory->uStoreRevision;
    pStats->uRecordCount = pMemory->iRecordCount;
    for ( i = 0u; i < pMemory->iRecordCount; ++i ) {
        const xllm_memory_record_internal* pRecord = &pMemory->pRecords[i];
        if ( pRecord->eScope == XLLM_MEMORY_SCOPE_MEMORY ) ++pStats->uMemoryRecordCount;
        if ( pRecord->eScope == XLLM_MEMORY_SCOPE_KNOWLEDGE ) ++pStats->uKnowledgeRecordCount;
        if ( pRecord->iExpiresAtUnix > 0 && pRecord->iExpiresAtUnix <= iNowUnix ) ++pStats->uExpiredRecordCount;
        if ( pRecord->eSensitivity >= XLLM_MEMORY_SENSITIVITY_SENSITIVE ) ++pStats->uSensitiveRecordCount;
        pStats->uTotalTextBytes += strlen(pRecord->sText);
    }
    return true;
}

static void xllm_memory__receipt(xllm_memory_receipt* pReceipt, xllm_memory_action eAction,
    const xllm_memory* pMemory, const xllm_memory_record_internal* pRecord, uint64_t uPreviousHash)
{
    if ( !pReceipt ) return;
    memset(pReceipt, 0, sizeof(*pReceipt));
    pReceipt->eAction = eAction;
    pReceipt->uStoreRevision = pMemory->uStoreRevision;
    pReceipt->uRecordRevision = pRecord ? pRecord->uRecordRevision : 0u;
    pReceipt->uPreviousContentHash = uPreviousHash;
    pReceipt->uContentHash = pRecord ? pRecord->uContentHash : 0u;
    pReceipt->iUpdatedAtUnix = pRecord ? pRecord->iUpdatedAtUnix : xllm_memory__now_unix();
    if ( pRecord && pRecord->sRecordId ) {
        size_t iLen = strlen(pRecord->sRecordId);
        if ( iLen >= sizeof(pReceipt->sRecordId) ) iLen = sizeof(pReceipt->sRecordId) - 1u;
        memcpy(pReceipt->sRecordId, pRecord->sRecordId, iLen);
        pReceipt->sRecordId[iLen] = '\0';
    }
}

bool xllmMemoryPut(xllm_memory* pMemory, const xllm_memory_record_input* pInput,
    xllm_memory_receipt* pReceipt, xllm_error* pError)
{
    xllm_memory_record_internal tNew;
    xllm_memory_record_internal tOld;
    const xllm_memory_record_internal* pExisting;
    size_t iIndex = 0u;
    uint64_t uOldStoreRevision;
    if ( pError ) xllmErrorInit(pError);
    if ( pReceipt ) memset(pReceipt, 0, sizeof(*pReceipt));
    if ( !pMemory || !pInput ) {
        xllm_memory__error(pError, XLLM_ERROR_INVALID_ARGUMENT, "memory store and record are required");
        return false;
    }
    pExisting = xllm_memory__find(pMemory, pInput->sRecordId, &iIndex);
    if ( pExisting && !pInput->bReplaceExisting ) {
        xllm_memory__error(pError, XLLM_ERROR_INVALID_ARGUMENT,
            "memory record already exists; replacement must be explicit");
        return false;
    }
    if ( !xllm_memory__record_from_input(&tNew, pInput, pExisting, pError) ) return false;
    if ( pExisting && xllm_memory__same_content(pExisting, &tNew) ) {
        xllm_memory__receipt(pReceipt, XLLM_MEMORY_ACTION_UNCHANGED, pMemory, pExisting, pExisting->uContentHash);
        xllm_memory__record_unit(&tNew);
        return true;
    }
    if ( pMemory->uStoreRevision == UINT64_MAX ) {
        xllm_memory__record_unit(&tNew);
        xllm_memory__error(pError, XLLM_ERROR_PROTOCOL, "memory store revision exhausted");
        return false;
    }
    uOldStoreRevision = pMemory->uStoreRevision;
    ++pMemory->uStoreRevision;
    if ( pExisting ) {
        tOld = pMemory->pRecords[iIndex];
        pMemory->pRecords[iIndex] = tNew;
        if ( !xllm_memory__save(pMemory, pError) ) {
            pMemory->pRecords[iIndex] = tOld;
            pMemory->uStoreRevision = uOldStoreRevision;
            xllm_memory__record_unit(&tNew);
            return false;
        }
        xllm_memory__receipt(pReceipt, XLLM_MEMORY_ACTION_REPLACED, pMemory,
            &pMemory->pRecords[iIndex], tOld.uContentHash);
        xllm_memory__record_unit(&tOld);
        return true;
    }
    if ( !xllm_memory__reserve(pMemory, pMemory->iRecordCount + 1u) ) {
        pMemory->uStoreRevision = uOldStoreRevision;
        xllm_memory__record_unit(&tNew);
        xllm_memory__error(pError, XLLM_ERROR_OUT_OF_MEMORY, "failed to grow memory record store");
        return false;
    }
    pMemory->pRecords[pMemory->iRecordCount++] = tNew;
    if ( !xllm_memory__save(pMemory, pError) ) {
        --pMemory->iRecordCount;
        pMemory->uStoreRevision = uOldStoreRevision;
        xllm_memory__record_unit(&pMemory->pRecords[pMemory->iRecordCount]);
        return false;
    }
    xllm_memory__receipt(pReceipt, XLLM_MEMORY_ACTION_CREATED, pMemory,
        &pMemory->pRecords[pMemory->iRecordCount - 1u], 0u);
    return true;
}

bool xllmMemoryRemove(xllm_memory* pMemory, const char* sRecordId,
    xllm_memory_receipt* pReceipt, xllm_error* pError)
{
    xllm_memory_record_internal tRemoved;
    const xllm_memory_record_internal* pExisting;
    size_t iIndex;
    uint64_t uOldStoreRevision;
    if ( pError ) xllmErrorInit(pError);
    if ( pReceipt ) memset(pReceipt, 0, sizeof(*pReceipt));
    if ( !pMemory || !sRecordId || !sRecordId[0] ) {
        xllm_memory__error(pError, XLLM_ERROR_INVALID_ARGUMENT, "memory store and record id are required");
        return false;
    }
    pExisting = xllm_memory__find(pMemory, sRecordId, &iIndex);
    if ( !pExisting ) {
        xllm_memory__error(pError, XLLM_ERROR_INVALID_ARGUMENT, "memory record does not exist");
        return false;
    }
    if ( pMemory->uStoreRevision == UINT64_MAX ) {
        xllm_memory__error(pError, XLLM_ERROR_PROTOCOL, "memory store revision exhausted");
        return false;
    }
    tRemoved = pMemory->pRecords[iIndex];
    if ( iIndex + 1u < pMemory->iRecordCount ) {
        memmove(&pMemory->pRecords[iIndex], &pMemory->pRecords[iIndex + 1u],
            (pMemory->iRecordCount - iIndex - 1u) * sizeof(*pMemory->pRecords));
    }
    --pMemory->iRecordCount;
    uOldStoreRevision = pMemory->uStoreRevision++;
    if ( !xllm_memory__save(pMemory, pError) ) {
        if ( iIndex < pMemory->iRecordCount ) {
            memmove(&pMemory->pRecords[iIndex + 1u], &pMemory->pRecords[iIndex],
                (pMemory->iRecordCount - iIndex) * sizeof(*pMemory->pRecords));
        }
        pMemory->pRecords[iIndex] = tRemoved;
        ++pMemory->iRecordCount;
        pMemory->uStoreRevision = uOldStoreRevision;
        return false;
    }
    xllm_memory__receipt(pReceipt, XLLM_MEMORY_ACTION_REMOVED, pMemory, &tRemoved, tRemoved.uContentHash);
    if ( pReceipt ) pReceipt->uContentHash = 0u;
    xllm_memory__record_unit(&tRemoved);
    return true;
}
