#ifndef XLLM_CHUNK_INTERNAL_H
#define XLLM_CHUNK_INTERNAL_H

typedef struct {
    char *sText;
    uint32 uChunkIndex;
    size_t iStartByte;
    size_t iEndByte;
} xllm__chunk_entry;

static int xllm__chunk_text(
    const char *sText,
    uint32 uChunkChars,
    uint32 uChunkOverlapChars,
    xllm__chunk_entry **ppChunks,
    size_t *piChunkCount,
    size_t *piChunkCapacity
);

static void xllm__chunk_entries_free(
    xllm__chunk_entry *pChunks,
    size_t iChunkCount
);

#endif
