# Internal verification decisions

Verification and diagnostic construction are separate operations. Internal
`xrt_crypto_check` values identify a valid signature, a signature/key rejection,
invalid API arguments, or an operation that could not complete. Decisions do not
derive their meaning from the current thread error, its text, or its cause chain.

RSA PKCS#1/PSS, raw/DER ECDSA, and Ed25519 keep one verification algorithm each.
The existing public `bool` entry points call that algorithm and materialize the
existing diagnostic category only on failure. Mathematical verification and
secure temporary cleanup are not duplicated. The decision entry points are
private native implementation details, not new public cryptographic APIs.

This is the prerequisite for typed X.509 path decisions: candidate rejection may
be backtracked; allocation, parser/normalization, and explicit callback failure
must stop the operation. `xrtX509PathCheck` and `xrtX509PathBuildCheck` now consume
these values directly; their legacy bool entry points share the same algorithm.
The search does not save, classify, or discard errors from failed operations.

`x509_verification_decision_tests` runs five existing normal, fixed signatures
(PKCS#1, PSS, P256, P384, Ed25519), zero-allocation API argument decisions,
pending-error preservation, old diagnostic categories, and resource balance.
No signature mutation, malformed-input campaign, network activity, or original
cryptographic security test executable is part of this acceptance.

The prerequisite acceptance was Windows/Linux GCC O0/O2 and TCC, modular and single
header, plus Linux ASan/UBSan: 28 build/run stages and 14 independent binary
replays. The five normal signatures were independently verified. Thirteen API
argument decisions, pre-existing thread errors, old diagnostic categories, and
zero-allocation operation were checked. The DER helper checks its scratch bound
before decoding; Ed25519 hash initialization and verification share one context
argument predicate.

That prerequisite updated twelve affected source/header sections of `single/xrt.h`.
These sections match fresh amalgamation from the scoped native source. Unrelated
generated sections already differ from full regeneration at the pre-change HEAD;
this batch preserves them and does not claim whole-header freshness or release
acceptance. It left public declarations unchanged. The subsequent path phase adds
explicit checked path/build declarations, preserving both projects' time ABIs.

The path phase reuses this evaluator and keeps legacy key failures classified as
Value in their diagnostic cause, rather than accidentally converting them to
Protocol. That metadata is only for diagnostic materialization, never for path
control flow. Its three routine groups and the two original normal-profile probes
passed 138 Windows/Linux stages including independent replays. See `x509.md` for
the exact scope and the remaining xlang snapshot integration boundary.
