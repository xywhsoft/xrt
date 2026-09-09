# Documentation Review Checklist

>Status: First draft in Chinese, awaiting review.

This checklist is for reviewing xwork official documentation changes. When adding new public APIs, examples, persistence formats, remote protocols, profile default strategies, or built-in tools, you must also check the relevant documentation.

## Links and Structure

- The root README has access to `docs/README.md`, API, tutorials and examples index.
- Relative links in `docs/` are not broken.
- `docs/` can link to `dev/docs/` as an internal reference, but the text must stand alone to state the facts that the user needs to understand.
- The new documents have been added to the corresponding indexes: `docs/README.md`, `docs/api/README.md`, `docs/guide/README.md` or `docs/case/README.md`.
- The English `.en.md` will not be generated before the Chinese main manuscript is stable; after it is generated in English, it must maintain the same name, the same level, and the same structure.

## API consistency

- The type names, function names, enumeration names, error codes and field names in the API page come from the current `xwork.h`.
- Each API page explains ownership, lifecycle, reset/destroy rules and thread boundaries.
- Each API page illustrates typical call sequences and recovery boundaries.
- The version number in the persistence document is consistent with `XWORK_PERSISTENCE_FORMAT_VERSION`.
- The protocol version in the remote worker documentation is consistent with `XWORK_REMOTE_PROTOCOL_VERSION_CURRENT`.
- The schema name and built-in tool name are consistent with the constants in `xwork.h`.

## Code and examples

- The complete runnable example is placed in `examples/`, the documentation only retains short snippets or calling sequences.
- Reproducibly executable build commands must be verified locally or in CI.
- Pseudocode snippets must be explicitly labeled `text` or indicate that they are a calling sequence and not pretend to be compilable C.
- Each new example must be supplemented with `examples/README.md` and the corresponding case or guide entry.
- The sample output, artifact, store directory and cleaning method are consistent with the source code behavior.

## Language and Terminology

- Chinese documents give priority to using unified terminology: runtime, workspace, tool, orchestrator, approval, product, persistence, remote worker, playback.
- Retain the original English text of public C identifiers, JSON field names, schema names, and tool names.
- Do not treat internal development plans as a prerequisite for user understanding.
- Do not describe xwork as a full product, cloud service or built-in network service.

## Automatic check

Run before submitting:

```powershell
powershell -ExecutionPolicy Bypass -File tools\check_docs.ps1
```

If you modify the example, you should also compile and run the related program using the commands in `examples/README.md`.
