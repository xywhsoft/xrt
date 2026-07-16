# miniz inflater/deflater vendoring notes

- Upstream: https://github.com/richgel999/miniz
- Imported release: 3.1.2
- Imported files: `miniz_common.h`, `miniz_tinfl.h`, `miniz_tinfl.c`, `miniz_tdef.h`, `miniz_tdef.c`, and `LICENSE`
- Used surface: low-level streaming `tinfl` inflater and `tdefl` compressor

XRT-local changes:

- `miniz_tinfl.c` includes `miniz_tinfl.h` directly because the generated amalgamated `miniz.h` is not vendored.
- `miniz_tdef.c` includes `miniz_tdef.h` directly and supplies the small checksum/constant compatibility surface normally provided by `miniz.h`.
- Unused zlib-compatible allocator declarations were removed from `miniz_common.h`.
- `miniz_export.h` keeps imported symbols translation-unit local and supplies architecture feature macros.

The upstream MIT license is preserved in `LICENSE`.
