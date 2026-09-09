# Context Packer Eval

This deterministic eval exercises the memory search-result to `xllm_context_block`
packing path without opening a memory database.

Covered scenarios:

- Full packing retains all synthetic evidence tokens.
- Total character budget reduces output size.
- Distinct-by-record removes duplicate chunks from the same record.
- Min-score filtering removes low-confidence hits.

Run:

```powershell
cmd /c .\build.bat context-packer-eval
```

The eval writes `context_packer_eval_report.json` and
`context_packer_eval_report.txt` under the selected output directory.
