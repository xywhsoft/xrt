#ifndef EMBEDBENCH_COMMON_H
#define EMBEDBENCH_COMMON_H

#define _CRT_SECURE_NO_WARNINGS

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct bench_case {
    char *pName;
    char *pGroup;
    char *pLeft;
    char *pRight;
} bench_case;

typedef struct bench_case_list {
    bench_case *pItems;
    size_t uCount;
} bench_case_list;

static char *
bench_strdup(const char *pText)
{
    size_t uLength;
    char *pCopy;

    if (pText == NULL) {
        return NULL;
    }

    uLength = strlen(pText);
    pCopy = (char *)malloc(uLength + 1);
    if (pCopy == NULL) {
        return NULL;
    }

    memcpy(pCopy, pText, uLength + 1);
    return pCopy;
}

static void
bench_trim_newline(char *pLine)
{
    size_t uLength;

    if (pLine == NULL) {
        return;
    }

    uLength = strcspn(pLine, "\r\n");
    pLine[uLength] = '\0';
}

static int
bench_file_exists(const char *pPath)
{
    DWORD dwAttributes = GetFileAttributesA(pPath);
    return dwAttributes != INVALID_FILE_ATTRIBUTES &&
           (dwAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static long long
bench_file_size_bytes(const char *pPath)
{
    FILE *pFile;
    long long iSize;

    pFile = fopen(pPath, "rb");
    if (pFile == NULL) {
        return -1;
    }

    if (fseek(pFile, 0, SEEK_END) != 0) {
        fclose(pFile);
        return -1;
    }

    iSize = _ftelli64(pFile);
    fclose(pFile);
    return iSize;
}

static double
bench_now_ms(void)
{
    static LARGE_INTEGER sFrequency = {0};
    LARGE_INTEGER sCounter;

    if (sFrequency.QuadPart == 0) {
        QueryPerformanceFrequency(&sFrequency);
    }

    QueryPerformanceCounter(&sCounter);
    return (double)sCounter.QuadPart * 1000.0 / (double)sFrequency.QuadPart;
}

static void
bench_free_cases(bench_case_list *pCases)
{
    size_t i;

    if (pCases == NULL) {
        return;
    }

    for (i = 0; i < pCases->uCount; ++i) {
        free(pCases->pItems[i].pName);
        free(pCases->pItems[i].pGroup);
        free(pCases->pItems[i].pLeft);
        free(pCases->pItems[i].pRight);
    }

    free(pCases->pItems);
    pCases->pItems = NULL;
    pCases->uCount = 0;
}

static int
bench_add_case(
    bench_case_list *pCases,
    const char *pName,
    const char *pGroup,
    const char *pLeft,
    const char *pRight)
{
    bench_case *pNewItems;
    bench_case *pItem;

    pNewItems = (bench_case *)realloc(pCases->pItems, (pCases->uCount + 1) * sizeof(bench_case));
    if (pNewItems == NULL) {
        return -1;
    }

    pCases->pItems = pNewItems;
    pItem = &pCases->pItems[pCases->uCount];
    memset(pItem, 0, sizeof(*pItem));

    pItem->pName = bench_strdup(pName);
    pItem->pGroup = bench_strdup(pGroup);
    pItem->pLeft = bench_strdup(pLeft);
    pItem->pRight = bench_strdup(pRight);
    if (pItem->pName == NULL || pItem->pGroup == NULL || pItem->pLeft == NULL || pItem->pRight == NULL) {
        free(pItem->pName);
        free(pItem->pGroup);
        free(pItem->pLeft);
        free(pItem->pRight);
        memset(pItem, 0, sizeof(*pItem));
        return -1;
    }

    ++pCases->uCount;
    return 0;
}

static int
bench_load_cases(const char *pPath, bench_case_list *pCases)
{
    FILE *pFile;
    char szLine[8192];
    int iIsFirstLine = 1;

    memset(pCases, 0, sizeof(*pCases));
    pFile = fopen(pPath, "rb");
    if (pFile == NULL) {
        return -1;
    }

    while (fgets(szLine, sizeof(szLine), pFile) != NULL) {
        char *pName;
        char *pGroup;
        char *pLeft;
        char *pRight;

        bench_trim_newline(szLine);
        if (szLine[0] == '\0' || szLine[0] == '#') {
            continue;
        }

        if (iIsFirstLine && strncmp(szLine, "name\tgroup\tleft\tright", 22) == 0) {
            iIsFirstLine = 0;
            continue;
        }
        iIsFirstLine = 0;

        pName = strtok(szLine, "\t");
        pGroup = strtok(NULL, "\t");
        pLeft = strtok(NULL, "\t");
        pRight = strtok(NULL, "\t");
        if (pName == NULL || pGroup == NULL || pLeft == NULL || pRight == NULL) {
            fclose(pFile);
            bench_free_cases(pCases);
            return -1;
        }

        if (bench_add_case(pCases, pName, pGroup, pLeft, pRight) != 0) {
            fclose(pFile);
            bench_free_cases(pCases);
            return -1;
        }
    }

    fclose(pFile);
    return pCases->uCount > 0 ? 0 : -1;
}

static int
bench_is_related_case(const bench_case *pCase)
{
    return pCase != NULL && pCase->pGroup != NULL && strcmp(pCase->pGroup, "related") == 0;
}

#endif
