# Envelope decode contracts

This leaf implements JSON-RPC 2.0 document/envelope decoding on the public
`xrtJsonScan` API. The [full product scope](../../../docs/development/service-extensions-plan.md)
remains in force; no runtime/encoder/transport completion is implied.

| Contract | Verification |
| --- | --- |
| Calls/notifications/responses | Handwritten descriptor assertions in request/response/duplex modes |
| Lossless ids and payloads | Huge integers/exponents, fractional ids, explicit null vs missing, NUL string escapes, complete array/object slices |
| Protocol header interpretation | Escaped key/version names, semantic duplicate aliases, case-sensitive comparisons and reserved method prefix |
| Batch shape | Mixed valid/invalid items, scalar roots, nested arrays, empty-batch single error |
| Error objects | Required integer code/string message, nested data, duplicate error fields and huge exponent handling |
| Publication boundary | Malformed tail/batch discards all descriptors; Document.Count is zero on parse failure |
| Local errors | Independent capacity/message/byte/depth/value/item/string/id/method limits and API span/alias rejection |
| Allocation and lifetime | 10,000 repeated decodes with unchanged allocation counters/error identity; borrowed views; one-byte quota and diagnostic OOM classification |
| Property/adversarial coverage | 10,000 generated mixed batches, 10,000 exact rational integer-code cases, 20,000 raw/mutated documents with bounded views and publication checks |
| Distribution | Modular/single tests, example, independent static/shared consumer and dedicated performance/size profiles |

Document is unchanged on LOCAL_ERROR. Scratch is deliberately not an atomic
output: it may contain partial descriptors on any failure and must be discarded.
DONE may contain INVALID entries; runtimes must not execute them. Valid
notifications must never produce responses, including handler errors.

The benchmark observes decoded fields and checks a completion checksum. It is
a workload smoke until a pinned baseline and end-to-end runtime exist. Current
platform evidence and remaining gates are in the [progress ledger](../../../docs/development/service-extensions-progress.md).
