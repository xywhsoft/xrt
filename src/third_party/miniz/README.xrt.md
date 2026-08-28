# miniz inflater/deflater vendoring notes

- Upstream: https://github.com/richgel999/miniz
- Imported release: 3.1.2
- Imported files: `miniz_common.h`, `miniz_tinfl.h`, `miniz_tinfl.c`, `miniz_tdef.h`, `miniz_tdef.c`, and `LICENSE`
- Used surface: low-level streaming `tinfl` inflater and `tdefl` compressor

XRT-local changes:

- `miniz_tinfl.c` includes `miniz_tinfl.h` directly because the generated amalgamated `miniz.h` is not vendored.
- `miniz_tdef.c` includes `miniz_tdef.h` directly and supplies the small checksum/constant compatibility surface normally provided by `miniz.h`.
- `miniz_tinfl.h`, `miniz_tinfl.c`, `miniz_tdef.h`, and `miniz_tdef.c` retain
  only the allocation-free streaming codecs and parameter conversion used by
  XRT. The unused heap, fixed-buffer, callback wrapper, PNG, status getter, and
  allocator helpers are intentionally excluded from the maintained source and
  generated single header.
- `TDEFL_LESS_MEMORY=1` reduces each compressor state from roughly 312 KiB to
  164 KiB. The XRT benchmark found equivalent throughput and only a negligible
  compressed-size difference for the representative mixed payload.
- `tinfl_set_window_bits()` and `tdefl_set_window_bits()` are XRT-local
  low-level extensions. They enforce negotiated 8..15-bit history distances;
  the compressor also writes the matching zlib CINFO value. The default remains
  the upstream-compatible 15-bit window.
- `tinfl` rejects both a zlib CINFO wider than configured and any actual
  back-reference beyond the configured history. It also tracks the amount of
  history produced by the current stream, so a malformed early back-reference
  cannot read retained dictionary bytes after reset. `tdefl` limits normal and
  fast match searches to the configured distance.
- Unused zlib-compatible allocator declarations were removed from `miniz_common.h`.
- `miniz_export.h` supplies architecture feature macros and exports only the
  selected codec object inside the XRT library; no miniz declarations enter the
  public XRT headers.

The upstream MIT license is preserved in `LICENSE`.
