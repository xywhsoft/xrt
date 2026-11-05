#include "../internal/xwork_internal.h"

/* Immutable tool catalog generations.
 *
 * Builders deep-copy descriptor strings and publish only complete catalogs.
 * Callback functions and user data may be paired with an explicit owner pin.
 * Every copied entry owns one pin, so a catalog keeps dynamic code alive for
 * every run and snapshot that retains that catalog.
 */

xwork_tool_catalog *xwork__tool_catalog_allocate(uint64_t uGeneration, size_t iToolCount);

static bool xwork__tool_definition_valid(const xwork_tool_definition *pDefinition)
{
    return pDefinition && pDefinition->sName && pDefinition->sName[0] &&
           !strcmp(pDefinition->sName, xwork__canonical_tool_name(pDefinition->sName)) &&
           pDefinition->sDescription && pDefinition->sParametersJson &&
           pDefinition->uEffects != 0u && (pDefinition->uEffects & ~XWORK_TOOL_EFFECT_ALL) == 0u &&
           ((pDefinition->uEffects & ~((xwork_tool_effects)XWORK_TOOL_EFFECT_READ)) == 0u ||
            pDefinition->OnDescribePermissions != NULL) &&
           ((pDefinition->OnExecute != NULL) != (pDefinition->OnExecuteV2 != NULL)) &&
           ((pDefinition->OnOwnerRetain != NULL) == (pDefinition->OnOwnerRelease != NULL));
}

void xwork__tool_entry_unit(xwork_tool_entry *pTool)
{
    if (!pTool)
        return;
    free(pTool->sName);
    free(pTool->sDescription);
    free(pTool->sParametersJson);
    free(pTool->sSource);
    free(pTool->sSerialGroup);
    if (pTool->bOwnerRetained)
    {
        pTool->OnOwnerRelease(pTool->pUserData);
    }
    memset(pTool, 0, sizeof(*pTool));
}

bool xwork__tool_entry_copy(xwork_tool_entry *pTarget, const xwork_tool_entry *pSource,
                            xwork_error *pError)
{
    memset(pTarget, 0, sizeof(*pTarget));
    pTarget->sName = xwork__strdup(pSource->sName);
    pTarget->sDescription = xwork__strdup(pSource->sDescription);
    pTarget->sParametersJson = xwork__strdup(pSource->sParametersJson);
    pTarget->sSource = xwork__strdup(pSource->sSource);
    pTarget->sSerialGroup = pSource->sSerialGroup ? xwork__strdup(pSource->sSerialGroup) : NULL;
    if (!pTarget->sName || !pTarget->sDescription || !pTarget->sParametersJson ||
        !pTarget->sSource || (pSource->sSerialGroup && !pTarget->sSerialGroup))
    {
        xwork__tool_entry_unit(pTarget);
        xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to copy tool catalog entry");
        return false;
    }
    pTarget->bStrict = pSource->bStrict;
    pTarget->uEffects = pSource->uEffects;
    pTarget->bRetrySafeRead = pSource->bRetrySafeRead;
    pTarget->OnExecute = pSource->OnExecute;
    pTarget->OnDescribePermissions = pSource->OnDescribePermissions;
    pTarget->OnExecuteV2 = pSource->OnExecuteV2;
    pTarget->pUserData = pSource->pUserData;
    pTarget->OnOwnerRetain = pSource->OnOwnerRetain;
    pTarget->OnOwnerRelease = pSource->OnOwnerRelease;
    pTarget->iMaxResultBytes = pSource->iMaxResultBytes;
    pTarget->bParallelSafe = pSource->bParallelSafe;
    if (pTarget->OnOwnerRetain)
    {
        if (!pTarget->OnOwnerRetain(pTarget->pUserData))
        {
            xwork__tool_entry_unit(pTarget);
            xwork__set_error(pError, XWORK_ERROR_CONTEXT,
                             "tool callback owner is no longer retainable");
            return false;
        }
        pTarget->bOwnerRetained = true;
    }
    return true;
}

static bool xwork__tool_entry_from_definition(xwork_tool_entry *pTarget,
                                              const xwork_tool_definition *pDefinition,
                                              const char *sForcedSource, xwork_error *pError)
{
    const char *sSource =
        sForcedSource && sForcedSource[0]
            ? sForcedSource
            : (pDefinition->sSource && pDefinition->sSource[0] ? pDefinition->sSource
                                                               : "application");
    memset(pTarget, 0, sizeof(*pTarget));
    pTarget->sName = xwork__strdup(pDefinition->sName);
    pTarget->sDescription = xwork__strdup(pDefinition->sDescription);
    pTarget->sParametersJson = xwork__strdup(pDefinition->sParametersJson);
    pTarget->sSource = xwork__strdup(sSource);
    if (!pTarget->sName || !pTarget->sDescription || !pTarget->sParametersJson || !pTarget->sSource)
    {
        xwork__tool_entry_unit(pTarget);
        xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to copy tool definition");
        return false;
    }
    pTarget->bStrict = pDefinition->bStrict;
    pTarget->uEffects = pDefinition->uEffects;
    pTarget->OnExecute = pDefinition->OnExecute;
    pTarget->OnDescribePermissions = pDefinition->OnDescribePermissions;
    pTarget->OnExecuteV2 = pDefinition->OnExecuteV2;
    pTarget->pUserData = pDefinition->pUserData;
    pTarget->OnOwnerRetain = pDefinition->OnOwnerRetain;
    pTarget->OnOwnerRelease = pDefinition->OnOwnerRelease;
    pTarget->iMaxResultBytes = pDefinition->iMaxResultBytes;
    pTarget->bParallelSafe = pDefinition->bParallelSafe;
    if (pDefinition->sSerialGroup && pDefinition->sSerialGroup[0])
    {
        pTarget->sSerialGroup = xwork__strdup(pDefinition->sSerialGroup);
    }
    else if (!pDefinition->bParallelSafe)
    {
        pTarget->sSerialGroup = xwork__strdup(sSource);
    }
    if (((pDefinition->sSerialGroup && pDefinition->sSerialGroup[0]) ||
         !pDefinition->bParallelSafe) &&
        !pTarget->sSerialGroup)
    {
        xwork__tool_entry_unit(pTarget);
        xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to copy tool source");
        return false;
    }
    if (pTarget->OnOwnerRetain)
    {
        if (!pTarget->OnOwnerRetain(pTarget->pUserData))
        {
            xwork__tool_entry_unit(pTarget);
            xwork__set_error(pError, XWORK_ERROR_CONTEXT,
                             "tool callback owner is no longer retainable");
            return false;
        }
        pTarget->bOwnerRetained = true;
    }
    return true;
}

static bool xwork__tool_entry_rebind_user_data(xwork_tool_entry *pTool, void *pUserData,
                                               xwork_error *pError)
{
    void *pOldUserData;
    if (pTool->pUserData == pUserData)
        return true;
    if (!pTool->OnOwnerRetain)
    {
        pTool->pUserData = pUserData;
        return true;
    }
    if (!pTool->OnOwnerRetain(pUserData))
    {
        xwork__set_error(pError, XWORK_ERROR_CONTEXT,
                         "rebound tool callback owner is no longer retainable");
        return false;
    }
    pOldUserData = pTool->pUserData;
    pTool->pUserData = pUserData;
    pTool->OnOwnerRelease(pOldUserData);
    return true;
}

xwork_tool_catalog *
xwork__tool_catalog_configure_concurrency(const xwork_tool_catalog *pBase, uint64_t uGeneration,
                                          const xwork_tool_concurrency_config *pConfig,
                                          xwork_error *pError)
{
    xwork_tool_catalog *pNext;
    size_t i;
    char *sGroup = NULL;
    if (!pBase || !pConfig || pConfig->uAbiVersion != XWORK_ABI_VERSION ||
        pConfig->uSize < sizeof(*pConfig) || !pConfig->sToolName || !pConfig->sToolName[0])
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                         "invalid tool concurrency configuration");
        return NULL;
    }
    for (i = 0u; i < pBase->iToolCount; ++i)
    {
        if (strcmp(pBase->pTools[i].sName, xwork__canonical_tool_name(pConfig->sToolName)) == 0)
            break;
    }
    if (i == pBase->iToolCount)
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                         "tool concurrency configuration names an unknown tool");
        return NULL;
    }
    if (pConfig->sSerialGroup && pConfig->sSerialGroup[0])
    {
        sGroup = xwork__strdup(pConfig->sSerialGroup);
        if (!sGroup)
            goto oom;
    }
    else if (!pConfig->bParallelSafe)
    {
        sGroup = xwork__strdup(pBase->pTools[i].sSource);
        if (!sGroup)
            goto oom;
    }
    pNext = xwork__tool_catalog_allocate(uGeneration, pBase->iToolCount);
    if (!pNext)
        goto oom;
    {
        size_t j;
        for (j = 0u; j < pBase->iToolCount; ++j)
        {
            if (!xwork__tool_entry_copy(&pNext->pTools[j], &pBase->pTools[j], pError))
            {
                xworkToolCatalogRelease(pNext);
                free(sGroup);
                return NULL;
            }
        }
    }
    free(pNext->pTools[i].sSerialGroup);
    pNext->pTools[i].sSerialGroup = sGroup;
    pNext->pTools[i].bParallelSafe = pConfig->bParallelSafe;
    return pNext;
oom:
    free(sGroup);
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to configure tool concurrency");
    return NULL;
}

xwork_tool_catalog *xwork__tool_catalog_allocate(uint64_t uGeneration, size_t iToolCount)
{
    xwork_tool_catalog *pCatalog;
    if (!uGeneration || iToolCount > SIZE_MAX / sizeof(xwork_tool_entry))
        return NULL;
    pCatalog = (xwork_tool_catalog *)calloc(1u, sizeof(*pCatalog));
    if (!pCatalog)
        return NULL;
    if (iToolCount)
    {
        pCatalog->pTools = (xwork_tool_entry *)calloc(iToolCount, sizeof(*pCatalog->pTools));
        if (!pCatalog->pTools)
        {
            free(pCatalog);
            return NULL;
        }
    }
    pCatalog->iRefs = 1;
    pCatalog->uGeneration = uGeneration;
    pCatalog->iToolCount = iToolCount;
    return pCatalog;
}

xwork_tool_catalog *xwork__tool_catalog_empty(uint64_t uGeneration)
{
    return xwork__tool_catalog_allocate(uGeneration, 0u);
}

xwork_tool_catalog *xworkToolCatalogRef(xwork_tool_catalog *pCatalog)
{
    if (!pCatalog || xwork__atomic_inc(&pCatalog->iRefs) <= 1)
        return NULL;
    return pCatalog;
}

void xworkToolCatalogRelease(xwork_tool_catalog *pCatalog)
{
    size_t i;
    if (!pCatalog || xwork__atomic_dec(&pCatalog->iRefs) != 0)
        return;
    for (i = 0u; i < pCatalog->iToolCount; ++i)
    {
        xwork__tool_entry_unit(&pCatalog->pTools[i]);
    }
    free(pCatalog->pTools);
    free(pCatalog);
}

uint64_t xworkToolCatalogGeneration(const xwork_tool_catalog *pCatalog)
{
    return pCatalog ? pCatalog->uGeneration : 0u;
}

size_t xworkToolCatalogCount(const xwork_tool_catalog *pCatalog)
{
    return pCatalog ? pCatalog->iToolCount : 0u;
}

bool xworkToolCatalogToolAt(const xwork_tool_catalog *pCatalog, size_t iIndex,
                            xwork_tool_info *pInfo)
{
    const xwork_tool_entry *pTool;
    if (!pCatalog || !pInfo || iIndex >= pCatalog->iToolCount)
        return false;
    pTool = &pCatalog->pTools[iIndex];
    memset(pInfo, 0, sizeof(*pInfo));
    pInfo->sName = pTool->sName;
    pInfo->sDescription = pTool->sDescription;
    pInfo->sParametersJson = pTool->sParametersJson;
    pInfo->sSource = pTool->sSource;
    pInfo->bStrict = pTool->bStrict;
    pInfo->uEffects = pTool->uEffects;
    pInfo->uCatalogGeneration = pCatalog->uGeneration;
    pInfo->bDescribesPermissions = pTool->OnDescribePermissions != NULL;
    pInfo->bUsesResultWriter = pTool->OnExecuteV2 != NULL;
    pInfo->iMaxResultBytes = pTool->iMaxResultBytes;
    pInfo->bParallelSafe = pTool->bParallelSafe;
    pInfo->sSerialGroup = pTool->sSerialGroup;
    return true;
}

static bool xwork__tool_catalog_has_name(const xwork_tool_catalog *pCatalog, const char *sName)
{
    size_t i;
    for (i = 0u; pCatalog && i < pCatalog->iToolCount; ++i)
    {
        if (strcmp(pCatalog->pTools[i].sName, sName) == 0)
            return true;
    }
    return false;
}

xwork_tool_catalog *xwork__tool_catalog_add(const xwork_tool_catalog *pBase, uint64_t uGeneration,
                                            const xwork_tool_definition *pDefinition,
                                            xwork_error *pError)
{
    xwork_tool_catalog *pNext;
    size_t i;
    if (!pBase || !xwork__tool_definition_valid(pDefinition))
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                         "invalid tool definition or catalog");
        return NULL;
    }
    if (xwork__tool_catalog_has_name(pBase, pDefinition->sName))
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "tool name is already registered");
        return NULL;
    }
    if (pBase->iToolCount == SIZE_MAX)
        goto oom;
    pNext = xwork__tool_catalog_allocate(uGeneration, pBase->iToolCount + 1u);
    if (!pNext)
        goto oom;
    for (i = 0u; i < pBase->iToolCount; ++i)
    {
        if (!xwork__tool_entry_copy(&pNext->pTools[i], &pBase->pTools[i], pError))
            goto copy_failed;
    }
    if (!xwork__tool_entry_from_definition(&pNext->pTools[pBase->iToolCount], pDefinition, NULL,
                                           pError))
    {
        goto copy_failed;
    }
    return pNext;
copy_failed:
    xworkToolCatalogRelease(pNext);
    return NULL;
oom:
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to build tool catalog generation");
    return NULL;
}

xwork_tool_catalog *xwork__tool_catalog_remove_name(const xwork_tool_catalog *pBase,
                                                    uint64_t uGeneration, const char *sName,
                                                    xwork_error *pError)
{
    xwork_tool_catalog *pNext;
    size_t i;
    size_t iOut = 0u;
    if (!pBase || !sName || !sName[0])
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                         "catalog and tool name are required");
        return NULL;
    }
    if (!xwork__tool_catalog_has_name(pBase, sName))
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "tool name is not registered");
        return NULL;
    }
    pNext = xwork__tool_catalog_allocate(uGeneration, pBase->iToolCount - 1u);
    if (!pNext)
        goto oom;
    for (i = 0u; i < pBase->iToolCount; ++i)
    {
        if (strcmp(pBase->pTools[i].sName, sName) == 0)
            continue;
        if (!xwork__tool_entry_copy(&pNext->pTools[iOut++], &pBase->pTools[i], pError))
        {
            xworkToolCatalogRelease(pNext);
            return NULL;
        }
    }
    return pNext;
oom:
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to build tool catalog generation");
    return NULL;
}

xwork_tool_catalog *xwork__tool_catalog_replace_source(const xwork_tool_catalog *pBase,
                                                       uint64_t uGeneration, const char *sSource,
                                                       const xwork_tool_definition *pDefinitions,
                                                       size_t iCount, size_t *piRemoved,
                                                       xwork_error *pError)
{
    xwork_tool_catalog *pNext;
    size_t i;
    size_t j;
    size_t iRetained = 0u;
    size_t iRemoved = 0u;
    size_t iOut = 0u;
    if (piRemoved)
        *piRemoved = 0u;
    if (!pBase || !sSource || !sSource[0] || (iCount && !pDefinitions))
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                         "catalog, source, and replacement definitions are required");
        return NULL;
    }
    for (i = 0u; i < iCount; ++i)
    {
        if (!xwork__tool_definition_valid(&pDefinitions[i]) ||
            (pDefinitions[i].sSource && pDefinitions[i].sSource[0] &&
             strcmp(pDefinitions[i].sSource, sSource) != 0))
        {
            xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                             "replacement tool definition or source is invalid");
            return NULL;
        }
        for (j = 0u; j < i; ++j)
        {
            if (strcmp(pDefinitions[i].sName, pDefinitions[j].sName) == 0)
            {
                xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                                 "replacement contains duplicate tool names");
                return NULL;
            }
        }
    }
    for (i = 0u; i < pBase->iToolCount; ++i)
    {
        if (strcmp(pBase->pTools[i].sSource, sSource) == 0)
            ++iRemoved;
        else
            ++iRetained;
    }
    if (iCount > SIZE_MAX - iRetained)
        goto oom;
    for (i = 0u; i < iCount; ++i)
    {
        for (j = 0u; j < pBase->iToolCount; ++j)
        {
            if (strcmp(pBase->pTools[j].sSource, sSource) != 0 &&
                strcmp(pBase->pTools[j].sName, pDefinitions[i].sName) == 0)
            {
                xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                                 "replacement tool name collides with another source");
                return NULL;
            }
        }
    }
    pNext = xwork__tool_catalog_allocate(uGeneration, iRetained + iCount);
    if (!pNext)
        goto oom;
    for (i = 0u; i < pBase->iToolCount; ++i)
    {
        if (strcmp(pBase->pTools[i].sSource, sSource) == 0)
            continue;
        if (!xwork__tool_entry_copy(&pNext->pTools[iOut++], &pBase->pTools[i], pError))
        {
            goto copy_failed;
        }
    }
    for (i = 0u; i < iCount; ++i)
    {
        if (!xwork__tool_entry_from_definition(&pNext->pTools[iOut++], &pDefinitions[i], sSource,
                                               pError))
        {
            goto copy_failed;
        }
    }
    if (piRemoved)
        *piRemoved = iRemoved;
    return pNext;
copy_failed:
    xworkToolCatalogRelease(pNext);
    return NULL;
oom:
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to build tool catalog generation");
    return NULL;
}

xwork_tool_catalog *
xwork__tool_catalog_filter_subagent(const xwork_tool_catalog *pBase, uint64_t uGeneration,
                                    const char *const *psNames, size_t iNameCount,
                                    xwork_tool_effects uAllowedEffects, const void *pOldUserData,
                                    void *pNewUserData, xwork_error *pError)
{
    xwork_tool_catalog *pNext;
    size_t i;
    size_t j;
    size_t iCount = 0u;
    size_t iOut = 0u;
    if (!pBase || !uGeneration || (iNameCount && !psNames) ||
        (uAllowedEffects & ~XWORK_TOOL_EFFECT_ALL) != 0u)
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                         "invalid subagent tool-catalog filter");
        return NULL;
    }
    for (i = 0u; i < iNameCount; ++i)
    {
        if (!psNames[i] || !psNames[i][0])
        {
            xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                             "subagent tool whitelist contains an empty name");
            return NULL;
        }
        for (j = 0u; j < i; ++j)
        {
            if (strcmp(psNames[i], psNames[j]) == 0)
            {
                xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                                 "subagent tool whitelist contains a duplicate name");
                return NULL;
            }
        }
        if (!xwork__tool_catalog_has_name(pBase, psNames[i]))
        {
            xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
                             "subagent tool whitelist names an unavailable parent tool");
            return NULL;
        }
    }
    for (i = 0u; i < pBase->iToolCount; ++i)
    {
        bool bSelected = psNames == NULL;
        const xwork_tool_entry *pTool = &pBase->pTools[i];
        if (strcmp(pTool->sName, "agent") == 0)
            continue;
        for (j = 0u; !bSelected && j < iNameCount; ++j)
        {
            if (strcmp(pTool->sName, psNames[j]) == 0)
                bSelected = true;
        }
        if (bSelected && (pTool->uEffects & ~uAllowedEffects) == 0u)
            ++iCount;
    }
    pNext = xwork__tool_catalog_allocate(uGeneration, iCount);
    if (!pNext)
        goto oom;
    for (i = 0u; i < pBase->iToolCount; ++i)
    {
        bool bSelected = psNames == NULL;
        const xwork_tool_entry *pTool = &pBase->pTools[i];
        if (strcmp(pTool->sName, "agent") == 0)
            continue;
        for (j = 0u; !bSelected && j < iNameCount; ++j)
        {
            if (strcmp(pTool->sName, psNames[j]) == 0)
                bSelected = true;
        }
        if (!bSelected || (pTool->uEffects & ~uAllowedEffects) != 0u)
            continue;
        if (!xwork__tool_entry_copy(&pNext->pTools[iOut], pTool, pError))
        {
            xworkToolCatalogRelease(pNext);
            return NULL;
        }
        if (pNext->pTools[iOut].pUserData == pOldUserData)
        {
            if (!xwork__tool_entry_rebind_user_data(&pNext->pTools[iOut], pNewUserData, pError))
            {
                xworkToolCatalogRelease(pNext);
                return NULL;
            }
        }
        ++iOut;
    }
    return pNext;
oom:
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY,
                     "failed to build the subagent tool catalog");
    return NULL;
}
