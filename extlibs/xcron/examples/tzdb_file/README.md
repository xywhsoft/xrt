# Anchored TZif release directory

Run the offline example from the repository root:

```console
python tools/build.py --compiler gcc --manifest extlibs/xcron/config/modules.json --suite xcron_tzdb_file
```

Without arguments it creates an isolated directory and real UTC TZif file,
loads the explicit manifest, removes its file/closes the root, then queries the
owned snapshot and releases every duty. It removes only its own known fixtures.
The executable also accepts `immutable-release-directory zone-name` to import
an explicitly selected installed or packaged release. Version labels and source
immutability remain application responsibilities; no default database is guessed.
See [atomic publication and source boundaries](../../docs/tzdb.md).
