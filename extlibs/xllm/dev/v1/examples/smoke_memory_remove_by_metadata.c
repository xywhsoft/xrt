#include <stdio.h>
#include <string.h>

#include "xllm-memory.h"

XLLM_API void xllm_turn_init(xllm_turn *pTurn);
XLLM_API void xllm_turn_reset(xllm_turn *pTurn);
XLLM_API int xllm_turn_add_user_text(xllm_turn *pTurn, const char *sText);

static int require_true(int condition, const char *sMessage)
{
    if ( !condition ) {
        fprintf(stderr, "%s\n", sMessage);
        return 1;
    }
    return 0;
}

static int ingest_text_with_metadata(
    xllm_memory *pMemory,
    xllm_memory_scope eScope,
    const char *sRecordId,
    const char *sSourceUri,
    const char *sKey,
    const char *sValue,
    xllm_error *pError
)
{
    xllm_memory_ingest_options tOptions;
    xvalue tMetadata = xvoCreateTable();
    int iStatus;

    if ( !tMetadata ||
         !xvoTableSetText(tMetadata, (str)sKey, 0u, (str)sValue, 0u, FALSE) ) {
        if ( tMetadata ) {
            xvoUnref(tMetadata);
        }
        return XRT_NET_ERROR;
    }

    xllm_memory_ingest_options_init(&tOptions);
    tOptions.eScope = eScope;
    tOptions.sRecordId = sRecordId;
    tOptions.sTitle = sRecordId;
    tOptions.sSourceUri = sSourceUri;
    tOptions.sText = "metadata cleanup candidate";
    tOptions.tMetadata = tMetadata;
    iStatus = xllm_memory_ingest_text(pMemory, &tOptions, pError);
    xvoUnref(tMetadata);
    return iStatus;
}

int main(void)
{
    xllm_runtime_options tRuntimeOptions;
    xllm_runtime *pRuntime = NULL;
    xllm_memory_options tMemoryOptions;
    xllm_memory_ingest_turn_response_options tTurnResponseOptions;
    xllm_memory_remove_by_metadata_options tRemoveOptions;
    xllm_turn tTurn;
    xllm_error tError;
    xllm_memory *pMemory = NULL;
    char sNamespace[96];
    uint32 uRemoved = 0u;
    int iStatus;
    int iRc = 1;

    memset(&tRuntimeOptions, 0, sizeof(tRuntimeOptions));
    xllm_memory_options_init(&tMemoryOptions);
    xllm_memory_ingest_turn_response_options_init(&tTurnResponseOptions);
    xllm_memory_remove_by_metadata_options_init(&tRemoveOptions);
    xllm_turn_init(&tTurn);
    xllm_error_init(&tError);

    iStatus = xllm_runtime_create(&tRuntimeOptions, &pRuntime);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "runtime create failed: %d\n", iStatus);
        goto cleanup;
    }

    snprintf(sNamespace, sizeof(sNamespace), "smoke-remove-by-metadata-%llu", (unsigned long long)xrtRand64());
    tMemoryOptions.sNamespace = sNamespace;
    iStatus = xllm_memory_create(pRuntime, &tMemoryOptions, &pMemory);
    if ( iStatus != XRT_NET_OK || !pMemory ) {
        fprintf(stderr, "memory create failed: %d\n", iStatus);
        goto cleanup;
    }

    iStatus = ingest_text_with_metadata(
        pMemory,
        XLLM_MEMORY_SCOPE_KNOWLEDGE,
        "knowledge-cleanup",
        "workspace://cleanup/knowledge",
        "cleanup_group",
        "stale",
        &tError
    );
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "knowledge cleanup ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    iStatus = ingest_text_with_metadata(
        pMemory,
        XLLM_MEMORY_SCOPE_MEMORY,
        "memory-cleanup",
        "conversation://cleanup/memory",
        "cleanup_group",
        "stale",
        &tError
    );
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory cleanup ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    iStatus = ingest_text_with_metadata(
        pMemory,
        XLLM_MEMORY_SCOPE_MEMORY,
        "memory-keep",
        "conversation://cleanup/keep",
        "cleanup_group",
        "active",
        &tError
    );
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "memory keep ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    xllm_memory_remove_by_metadata_options_init(&tRemoveOptions);
    tRemoveOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tRemoveOptions.sMetadataKey = "cleanup_group";
    tRemoveOptions.sMetadataValue = "stale";
    iStatus = xllm_memory_remove_by_metadata(pMemory, &tRemoveOptions, &uRemoved, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "remove memory stale failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(uRemoved == 1u, "expected one stale memory record removed") != 0 ||
         require_true(xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_KNOWLEDGE) == 1u, "knowledge stale record should remain") != 0 ||
         require_true(xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_MEMORY) == 1u, "only active memory record should remain") != 0 ) {
        goto cleanup;
    }

    xllm_memory_remove_by_metadata_options_init(&tRemoveOptions);
    tRemoveOptions.eScope = XLLM_MEMORY_SCOPE_ANY;
    tRemoveOptions.sMetadataKey = "cleanup_group";
    tRemoveOptions.sMetadataValue = "stale";
    iStatus = xllm_memory_remove_by_metadata(pMemory, &tRemoveOptions, &uRemoved, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "remove any stale failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(uRemoved == 1u, "expected one stale any-scope record removed") != 0 ||
         require_true(xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_KNOWLEDGE) == 0u, "knowledge stale record should be removed") != 0 ||
         require_true(xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_MEMORY) == 1u, "active memory record should remain") != 0 ) {
        goto cleanup;
    }

    if ( xllm_turn_add_user_text(&tTurn, "Typed turn_response should be removable by memory_type.") != XRT_NET_OK ) {
        fprintf(stderr, "failed to build turn\n");
        goto cleanup;
    }

    xllm_memory_ingest_turn_response_options_init(&tTurnResponseOptions);
    tTurnResponseOptions.pTurn = &tTurn;
    tTurnResponseOptions.sRecordId = "typed-turn";
    tTurnResponseOptions.sConversationId = "typed-conv";
    tTurnResponseOptions.sTurnId = "turn-001";
    iStatus = xllm_memory_ingest_turn_response(pMemory, &tTurnResponseOptions, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "typed turn ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    xllm_memory_remove_by_metadata_options_init(&tRemoveOptions);
    tRemoveOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tRemoveOptions.sMetadataKey = "memory_type";
    tRemoveOptions.sMetadataValue = "conversation.turn_response.v1";
    iStatus = xllm_memory_remove_by_metadata(pMemory, &tRemoveOptions, &uRemoved, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "remove typed turn failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(uRemoved == 1u, "expected typed turn_response record removed") != 0 ||
         require_true(xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_MEMORY) == 1u, "active memory record should still remain") != 0 ) {
        goto cleanup;
    }

    xllm_memory_remove_by_metadata_options_init(&tRemoveOptions);
    tRemoveOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tRemoveOptions.sMetadataKey = "cleanup_marker";
    iStatus = xllm_memory_remove_by_metadata(pMemory, &tRemoveOptions, &uRemoved, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "pre-clean marker-only failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_MEMORY) == 1u, "pre-clean should not remove active memory record") != 0 ) {
        goto cleanup;
    }

    iStatus = ingest_text_with_metadata(
        pMemory,
        XLLM_MEMORY_SCOPE_MEMORY,
        "memory-marker-only",
        "conversation://cleanup/marker-only",
        "cleanup_marker",
        "present",
        &tError
    );
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "marker-only ingest failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }

    xllm_memory_remove_by_metadata_options_init(&tRemoveOptions);
    tRemoveOptions.eScope = XLLM_MEMORY_SCOPE_MEMORY;
    tRemoveOptions.sMetadataKey = "cleanup_marker";
    iStatus = xllm_memory_remove_by_metadata(pMemory, &tRemoveOptions, &uRemoved, &tError);
    if ( iStatus != XRT_NET_OK ) {
        fprintf(stderr, "remove marker-only failed: %s\n", tError.sMessage ? tError.sMessage : "(null)");
        goto cleanup;
    }
    if ( require_true(uRemoved == 1u, "expected one key-only metadata record removed") != 0 ||
         require_true(xllm_memory_record_count(pMemory, XLLM_MEMORY_SCOPE_MEMORY) == 1u, "active memory record should remain after key-only removal") != 0 ) {
        goto cleanup;
    }

    xllm_memory_remove_by_metadata_options_init(&tRemoveOptions);
    tRemoveOptions.eScope = XLLM_MEMORY_SCOPE_ANY;
    iStatus = xllm_memory_remove_by_metadata(pMemory, &tRemoveOptions, &uRemoved, &tError);
    if ( require_true(iStatus != XRT_NET_OK, "missing metadata key should fail") != 0 ||
         require_true(uRemoved == 0u, "failed remove should report zero removals") != 0 ) {
        goto cleanup;
    }

    printf("smoke_memory_remove_by_metadata ok\n");
    iRc = 0;

cleanup:
    xllm_turn_reset(&tTurn);
    xllm_memory_destroy(pMemory);
    xllm_runtime_destroy(pRuntime);
    xllm_error_reset(&tError);
    return iRc;
}
