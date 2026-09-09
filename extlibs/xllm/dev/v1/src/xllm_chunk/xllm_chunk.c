static size_t xllm__chunk_find_preferred_break(const char *sText, size_t iStart, size_t iSoftEnd)
{
    size_t i;

    if ( !sText ) {
        return iSoftEnd;
    }

    for ( i = iSoftEnd; i > iStart + 1u; --i ) {
        if ( sText[i - 1u] == '\n' && sText[i - 2u] == '\n' ) {
            return i;
        }
    }

    for ( i = iSoftEnd; i > iStart; --i ) {
        unsigned char c = (unsigned char)sText[i - 1u];
        if ( c == '\n' || c == '.' || c == '!' || c == '?' || c == ';' ) {
            return i;
        }
    }

    for ( i = iSoftEnd; i > iStart; --i ) {
        unsigned char c = (unsigned char)sText[i - 1u];
        if ( c <= 0x20u ) {
            return i;
        }
    }

    return iSoftEnd;
}

static void xllm__chunk_entry_free(xllm__chunk_entry *pChunk)
{
    if ( !pChunk ) {
        return;
    }

    xllm__free_cstr(&pChunk->sText);
    memset(pChunk, 0, sizeof(*pChunk));
}

static void xllm__chunk_entries_free(
    xllm__chunk_entry *pChunks,
    size_t iChunkCount
)
{
    size_t i;

    if ( !pChunks ) {
        return;
    }

    for ( i = 0u; i < iChunkCount; ++i ) {
        xllm__chunk_entry_free(&pChunks[i]);
    }
    xrtFree(pChunks);
}

static int xllm__chunk_text(
    const char *sText,
    uint32 uChunkChars,
    uint32 uChunkOverlapChars,
    xllm__chunk_entry **ppChunks,
    size_t *piChunkCount,
    size_t *piChunkCapacity
)
{
    size_t iStart = 0u;
    size_t iTextLength;
    size_t iChunkSize;
    size_t iOverlap;
    uint32 uChunkIndex = 0u;

    if ( !sText || !sText[0] || !ppChunks || !piChunkCount || !piChunkCapacity ) {
        return XRT_NET_ERROR;
    }

    *ppChunks = NULL;
    *piChunkCount = 0u;
    *piChunkCapacity = 0u;

    iChunkSize = uChunkChars > 0u ? (size_t)uChunkChars : 800u;
    iOverlap = (uChunkOverlapChars > 0u) ? (size_t)uChunkOverlapChars : 120u;
    if ( iOverlap >= iChunkSize ) {
        iOverlap = iChunkSize / 4u;
    }

    iTextLength = strlen(sText);
    while ( iStart < iTextLength ) {
        size_t iChunkStart = iStart;
        size_t iSoftEnd = iStart + iChunkSize;
        size_t iEnd;
        size_t iLen;
        xllm__chunk_entry tChunk;

        if ( iSoftEnd > iTextLength ) {
            iSoftEnd = iTextLength;
        }
        if ( iSoftEnd >= iTextLength ) {
            iEnd = iTextLength;
        } else {
            iEnd = xllm__chunk_find_preferred_break(sText, iStart, iSoftEnd);
            if ( iEnd <= iStart ) {
                iEnd = iSoftEnd;
            }
        }

        while ( iStart < iEnd && ((unsigned char)sText[iStart]) <= 0x20u ) {
            ++iStart;
        }
        while ( iEnd > iStart && ((unsigned char)sText[iEnd - 1u]) <= 0x20u ) {
            --iEnd;
        }
        if ( iEnd <= iStart ) {
            iStart = iSoftEnd;
            continue;
        }

        memset(&tChunk, 0, sizeof(tChunk));
        iLen = iEnd - iStart;
        tChunk.sText = (char *)xrtCalloc(iLen + 1u, sizeof(char));
        if ( !tChunk.sText ) {
            xllm__chunk_entries_free(*ppChunks, *piChunkCount);
            *ppChunks = NULL;
            *piChunkCount = 0u;
            *piChunkCapacity = 0u;
            return XRT_NET_ERROR;
        }
        memcpy(tChunk.sText, sText + iStart, iLen);
        tChunk.sText[iLen] = '\0';
        tChunk.uChunkIndex = uChunkIndex++;
        tChunk.iStartByte = iStart;
        tChunk.iEndByte = iEnd;

        if ( xllm__append_buffer(
                (void **)ppChunks,
                sizeof(tChunk),
                piChunkCount,
                piChunkCapacity,
                &tChunk
             ) != XRT_NET_OK ) {
            xllm__chunk_entry_free(&tChunk);
            xllm__chunk_entries_free(*ppChunks, *piChunkCount);
            *ppChunks = NULL;
            *piChunkCount = 0u;
            *piChunkCapacity = 0u;
            return XRT_NET_ERROR;
        }

        if ( iEnd >= iTextLength ) {
            break;
        }
        iStart = (iOverlap > 0u && iEnd > iOverlap) ? (iEnd - iOverlap) : iEnd;
        if ( iStart <= iChunkStart ) {
            iStart = iEnd;
        }
    }

    return *piChunkCount > 0u ? XRT_NET_OK : XRT_NET_ERROR;
}
