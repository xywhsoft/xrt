# LZMA SDK decoder in XRT

This directory contains the public-domain 7-Zip LZMA SDK decoder files needed
by the VFS pack provider. The source file dates identify the imported snapshot:

- `LzmaDec.c`: 2023-04-07
- `LzmaDec.h`: 2023-04-02
- `7zTypes.h`: 2024-01-24
- `Compiler.h`: Igor Pavlov public-domain SDK support header
- `Precomp.h`: 2024-01-25

XRT does not expose SDK types in its public API. `LzmaDec.c` maps every external
decoder function to an `__xrtLzma*` private symbol so applications may link
another LZMA SDK copy without duplicate symbols. `vfs_pack.c` supplies the XRT
allocator and validates exact input consumption, output size, decoder completion
status, and the archive entry CRC before publishing a blob.

The imported decoder uses many generic implementation macros. XRT explicitly
undefines every macro introduced by `LzmaDec.c` at the end of that source so
the amalgamated `XRT_MODULE_ALL` translation unit cannot rewrite later XRT
identifiers. Keep the cleanup list synchronized when updating the SDK.

`LzmaDec_InitDicAndState` is an SDK source-only helper and is kept `static`;
only the explicitly mapped `__xrtLzma*` functions may leave the decoder
object. This permits a host to link its own unmodified LZMA decoder.
