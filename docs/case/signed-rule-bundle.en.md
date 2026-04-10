# XRT Case Study: Signed Rule Bundle Import with Charset, Regex, and Crypto

> A practical import chain for signed text rule bundles: read unknown-encoding text, normalize it into UTF-8, extract fields line by line, verify the HMAC, and compile runtime route rules.

[中文](signed-rule-bundle.md) | [Back to Case Studies](README.en.md)

---

## 1. Scenario

Assume you need to import a text rule bundle delivered by an external team with these constraints:

1. the file encoding is not stable and may be UTF-8 or UTF-16 with BOM
2. the format must stay human-editable instead of becoming a binary blob
3. you need to extract several `route=` rules from the text
4. the import must reject tampered bundles
5. after import, runtime code wants compiled rules instead of raw strings
6. the same flow should behave the same way on Windows and Linux

Without one clear mainline, this usually degrades into:

- guessing the file encoding ad hoc
- mixing `strchr()` scans with handwritten `key=value` parsing
- scattering signature logic across several string-assembly steps
- importing raw strings first and compiling them again later

This case solves the exact problem of how a signed text rule bundle should be imported on top of the current XRT mainline.

---

## 2. Why This Mainline

### Why not read with "local encoding" and hope it works?

Because the real problem is not "can this machine display the text right now".

The real problems are:

- the bundle source is not stable
- the business mainline should converge into UTF-8
- the same bundle must import with the same semantics on Windows and Linux

So this page intentionally starts from:

- `xrtFileReadAll(..., XRT_CP_AUTO, ...)`

instead of letting business code gamble on the local code page.

### Why not just use JSON or XSON for the whole file?

That is possible, but it is not the point of this case.

The goal here is:

- keep the file human-editable like `.env` or `.ini`
- keep the format minimal
- make the teaching focus land on `charset + regex + crypto`

So this page deliberately uses line-oriented `key=value` text.

### Why HMAC instead of "encrypt the file"?

Because the real need here is:

- detect tampering
- confirm the bundle came from a peer that holds the shared secret

That is exactly what:

- `xrtHMAC_SHA256()`

solves.

If you only encrypt without authenticating the source, you still cannot clearly tell:

- whether the bundle was modified
- whether the content came from the real publisher

### Why does regex only handle line extraction here?

Because that is the safest current regex responsibility:

- validate whether one line looks like `key=value`
- extract `key` and `value`
- compile repeated `route=` values into a runtime rule set

It should not take over:

- a full syntax tree
- nested object parsing
- state-machine protocol parsing

---

## 3. What Each Layer Does

| Layer | Role in this case | What it really gives you |
|------|-------------------|--------------------------|
| `file + charset` | read unknown-encoding text and normalize into UTF-8 | the import path does not get confused by BOM or UTF-16 |
| line-level `xregex` | extract `key=value` pairs | every line has a clear field boundary |
| `xrtHMAC_SHA256()` | verify the body before the signature line | source authentication and tamper detection |
| `xrtHexDecode()` | turn the hex signature text into binary MAC bytes | bridge text format and crypto primitive |
| `xregexset` | compile several `route=` rules into one runtime rule set | imported rules become directly matchable |

One-sentence version:

> `charset` reads the input correctly, `regex` splits the text correctly, `HMAC` verifies the source correctly, and `RegexSet` turns text rules into runtime objects.

---

## 4. Bundle Format

This case intentionally keeps the bundle format minimal:

```text
bundle=partner-acl
route=^/admin/.+$
route=^/ops/(restart|metrics)$
hmac=4d2f... (64 hex chars)
```

There are 3 explicit rules:

1. `hmac=` must be the last effective config line
2. the HMAC covers the UTF-8 text bytes before `hmac=`
3. `route=` may appear several times and is finally compiled into one `xregexset`

That is why this page does not use a more elaborate format. It wants the text shape, the signature boundary, and the runtime compile target to stay obvious.

---

## 5. Code Skeleton

The Chinese page contains the longer version. The skeleton below keeps the same import, verification, and runtime-compile boundaries:

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

#define DEMO_MAX_ROUTE	8

typedef struct
{
	char sBundleId[64];
	char* arrRoute[DEMO_MAX_ROUTE];
	uint32 iRouteCount;
	xregexset* pRouteSet;
} DemoSignedRuleBundle;

static bool procAsciiEqualsIgnoreCase(const char* sA, const char* sB)
{
	char cA;
	char cB;

	if ( (sA == NULL) || (sB == NULL) ) {
		return FALSE;
	}

	for ( ;; ) {
		cA = *sA++;
		cB = *sB++;

		if ( (cA >= 'A') && (cA <= 'Z') ) {
			cA = (char)(cA - 'A' + 'a');
		}
		if ( (cB >= 'A') && (cB <= 'Z') ) {
			cB = (char)(cB - 'A' + 'a');
		}

		if ( cA != cB ) {
			return FALSE;
		}
		if ( cA == '\0' ) {
			return TRUE;
		}
	}
}

static char* procCopySpan(const char* sText, xregexspan tSpan)
{
	size_t iLen;
	char* sOut;

	if ( (sText == NULL) || (tSpan.iEnd < tSpan.iBegin) ) {
		return NULL;
	}

	iLen = tSpan.iEnd - tSpan.iBegin;
	sOut = (char*)xrtMalloc(iLen + 1u);
	if ( sOut == NULL ) {
		return NULL;
	}

	memcpy(sOut, sText + tSpan.iBegin, iLen);
	sOut[iLen] = '\0';
	return sOut;
}

static void procFreeBundle(DemoSignedRuleBundle* pBundle)
{
	uint32 i;

	if ( pBundle == NULL ) {
		return;
	}

	if ( pBundle->pRouteSet != NULL ) {
		xrtRegexSetDestroy(pBundle->pRouteSet);
		pBundle->pRouteSet = NULL;
	}

	for ( i = 0; i < pBundle->iRouteCount; i++ ) {
		if ( pBundle->arrRoute[i] != NULL ) {
			xrtFree(pBundle->arrRoute[i]);
			pBundle->arrRoute[i] = NULL;
		}
	}

	pBundle->iRouteCount = 0;
	memset(pBundle->sBundleId, 0, sizeof(pBundle->sBundleId));
}

static bool procVerifyBundleHMAC(const char* sText, size_t iSignedSize, const char* sHexMAC, const char* sSecret)
{
	uint8 aucActual[32];
	uint8* pExpect = NULL;
	bool bOk = FALSE;

	pExpect = (uint8*)xrtHexDecode((str)sHexMAC, strlen(sHexMAC));
	if ( pExpect == NULL ) {
		return FALSE;
	}

	xrtHMAC_SHA256(
		(const uint8*)sSecret,
		strlen(sSecret),
		(const uint8*)sText,
		iSignedSize,
		aucActual
	);

	bOk = memcmp(pExpect, aucActual, sizeof(aucActual)) == 0;
	xrtFree(pExpect);
	return bOk;
}

static bool procCompileRouteSet(DemoSignedRuleBundle* pBundle)
{
	pBundle->pRouteSet = xrtRegexSetCreate((const char* const*)pBundle->arrRoute, pBundle->iRouteCount);
	return pBundle->pRouteSet != NULL;
}

static bool procLoadSignedRuleBundle(const char* sPath, const char* sSecret, DemoSignedRuleBundle* pOut)
{
	static const char sLinePattern[] = "^[ \\t]*(?P<key>[A-Za-z_]+)[ \\t]*=[ \\t]*(?P<value>.*?)[ \\t]*$";
	xregexbuilder* pBuilder = NULL;
	xregex* pLineRegex = NULL;
	str sText = NULL;
	str sHexMAC = NULL;
	size_t iTextSize = 0;
	size_t iLineStart = 0;
	size_t iSignedSize = 0;
	xregexspan arrCaps[3] = {0};
	bool bSeenHMAC = FALSE;
	bool bOk = FALSE;

	memset(pOut, 0, sizeof(*pOut));

	sText = xrtFileReadAll((str)sPath, XRT_CP_AUTO, &iTextSize);
	if ( (sText == NULL) || !xrtIsUTF8(sText, iTextSize) ) {
		goto cleanup;
	}

	if ( xrtRegexBuilderCreate(&pBuilder, sLinePattern, strlen(sLinePattern), NULL) != 0 ) {
		goto cleanup;
	}
	xrtRegexBuilderSetFlags(pBuilder, XRT_REGEX_FLAG_INSENSITIVE);
	if ( xrtRegexCreateFromBuilder(&pLineRegex, pBuilder, NULL) != 0 || (pLineRegex == NULL) ) {
		goto cleanup;
	}

	while ( iLineStart < iTextSize ) {
		size_t iLineEnd = iLineStart;
		size_t iParseEnd;
		char* sLine = NULL;
		char* sKey = NULL;
		char* sValue = NULL;

		while ( (iLineEnd < iTextSize) && (sText[iLineEnd] != '\n') ) {
			iLineEnd++;
		}

		iParseEnd = iLineEnd;
		if ( (iParseEnd > iLineStart) && (sText[iParseEnd - 1u] == '\r') ) {
			iParseEnd--;
		}
		if ( iParseEnd == iLineStart ) {
			iLineStart = (iLineEnd < iTextSize) ? (iLineEnd + 1u) : iLineEnd;
			continue;
		}

		sLine = procCopySpan(sText, (xregexspan){iLineStart, iParseEnd});
		if ( (sLine == NULL) || (sLine[0] == '#') ) {
			if ( sLine != NULL ) {
				xrtFree(sLine);
			}
			iLineStart = (iLineEnd < iTextSize) ? (iLineEnd + 1u) : iLineEnd;
			continue;
		}
		if ( bSeenHMAC || (xrtRegexCaptures(pLineRegex, sLine, strlen(sLine), arrCaps, 3u) <= 0) ) {
			xrtFree(sLine);
			goto cleanup;
		}

		sKey = procCopySpan(sLine, arrCaps[1]);
		sValue = procCopySpan(sLine, arrCaps[2]);
		xrtFree(sLine);

		if ( procAsciiEqualsIgnoreCase(sKey, "bundle") ) {
			snprintf(pOut->sBundleId, sizeof(pOut->sBundleId), "%s", sValue);
		} else if ( procAsciiEqualsIgnoreCase(sKey, "route") ) {
			if ( pOut->iRouteCount >= DEMO_MAX_ROUTE ) {
				xrtFree(sKey);
				xrtFree(sValue);
				goto cleanup;
			}
			pOut->arrRoute[pOut->iRouteCount++] = sValue;
			sValue = NULL;
		} else if ( procAsciiEqualsIgnoreCase(sKey, "hmac") ) {
			sHexMAC = sValue;
			sValue = NULL;
			iSignedSize = iLineStart;
			bSeenHMAC = TRUE;
		}

		xrtFree(sKey);
		if ( sValue != NULL ) {
			xrtFree(sValue);
		}

		iLineStart = (iLineEnd < iTextSize) ? (iLineEnd + 1u) : iLineEnd;
	}

	if ( !bSeenHMAC || (sHexMAC == NULL) || (pOut->iRouteCount == 0u) ) {
		goto cleanup;
	}
	if ( !procVerifyBundleHMAC(sText, iSignedSize, sHexMAC, sSecret) ) {
		goto cleanup;
	}
	if ( !procCompileRouteSet(pOut) ) {
		goto cleanup;
	}

	bOk = TRUE;

cleanup:
	if ( !bOk ) {
		procFreeBundle(pOut);
	}
	if ( sHexMAC != NULL ) {
		xrtFree(sHexMAC);
	}
	if ( pLineRegex != NULL ) {
		xrtRegexDestroy(pLineRegex);
	}
	if ( pBuilder != NULL ) {
		xrtRegexBuilderDestroy(pBuilder);
	}
	if ( sText != NULL ) {
		xrtFree(sText);
	}
	return bOk;
}
```

---

## 6. The 4 Most Important Boundaries

### 6.1 Encoding boundary

This page does not start by guessing an encoding and hoping the business layer can survive it.

It explicitly begins with:

- `xrtFileReadAll(..., XRT_CP_AUTO, ...)`

so unknown source files first converge into the UTF-8 mainline.

That means:

- `charset` is responsible for reading correctly
- then `regex` and `crypto` can work on stable text

### 6.2 Parsing boundary

Regex here only does:

- decide whether one line looks like `key=value`
- extract the `key` and `value`

It does not:

- own the whole file syntax
- maintain a parser state machine

That is the safest current regex role.

### 6.3 Signature boundary

This page deliberately defines the signature scope as:

- the UTF-8 text bytes before `hmac=`

So the import chain stays clear:

1. normalize the file into UTF-8
2. locate where the `hmac=` line begins
3. run `HMAC-SHA256` over the preceding body

In other words:

- text semantics are stabilized first
- source verification happens second

### 6.4 Runtime boundary

After import, the page does not leave `route=` values as raw strings.

It immediately uses:

- `xrtRegexSetCreate(...)`

to compile them into a runtime rule set.

So later business code receives:

- directly usable matching objects

instead of:

- text that still needs another compile phase

---

## 7. Why This Page Does Not Focus on the Publisher Side

Because the core problem here is:

- how to safely import a bundle delivered by someone else

not:

- how to design a whole publishing platform

The publishing side is still straightforward:

1. build the UTF-8 body without the `hmac=` line
2. run `xrtHMAC_SHA256()` over that body
3. append the result as the final `hmac=...` line using `xrtHexEncode()`

So publishing and importing are two ends of the same chain. This page deliberately stabilizes the import side first.

---

## 8. Where This Model Fits Best

This model fits well for:

- rule or policy bundle import
- human-editable text configs that still need source authentication
- gateways or services that must compile imported rules into runtime objects
- cross-platform tools and services on Windows and Linux

It is not a good fit for:

- confidential secret storage
- large nested data models
- a full DSL or protocol parser

---

## 9. Common Mistakes

### Mistake 1: reading with local encoding and hoping there will be no corruption

That makes the same bundle decode differently on different platforms.

### Mistake 2: reformatting the text before verification

If the signature scope differs from the publisher side, verification will fail.

### Mistake 3: using regex as the parser for the whole document

That goes beyond the safest current regex boundary.

### Mistake 4: using a plain hash instead of HMAC

A plain hash can describe the content, but it cannot prove who produced it.

### Mistake 5: leaving imported rules as uncompiled strings

Then the runtime matching layer must repeat compile-time validation again.

---

## 10. Suggested Reading

- [Charset API](../api/api-charset.en.md)
- [Regex API](../api/api-regex.en.md)
- [Crypto API](../api/api-crypto.en.md)
- [Guide: Charset Intro](../guide/charset-intro.md) (Chinese for now)
- [Guide: Regex Intro](../guide/regex-intro.md) (Chinese for now)
- [Guide: Crypto Intro](../guide/crypto-intro.md) (Chinese for now)
- [Case: Configuration System with `xvalue + json`](json-config-system.en.md)

---

**One-sentence summary:** the key is not merely "read a text file"; the key is to normalize unknown input into UTF-8 first, extract fields with regex, verify the body with HMAC, and finally compile text rules into formal runtime objects.
