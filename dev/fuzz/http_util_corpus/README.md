# HTTP Util Corpus

This directory stores tracked malformed or high-risk inputs for the public HTTP utility fuzz harness.

The goal is not protocol conformance coverage alone. The goal is to preserve known edge cases that are useful for:

- parser hardening
- malformed input regression checks
- sanitizer-backed fuzz smoke runs
- future libFuzzer seed corpora

Current seed categories:

- URL authority and IPv6 edge cases
- duplicate / empty query parameters
- malformed header blocks
- cookie and Set-Cookie attribute edge cases
- multipart boundary and filename* cases
- multipart header explosion / malformed boundary collision samples
