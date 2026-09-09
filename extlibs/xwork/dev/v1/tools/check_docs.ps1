$ErrorActionPreference = "Stop"

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$DocsDir = Join-Path $Root "docs"
$DevDocsDir = Join-Path $Root "dev\docs"
$HeaderPath = Join-Path $Root "xwork.h"

function Fail($Message) {
    throw "docs check failed: $Message"
}

function Require-Path($RelativePath) {
    $Path = Join-Path $Root $RelativePath
    if (-not (Test-Path -LiteralPath $Path)) {
        Fail "missing required path: $RelativePath"
    }
}

function Read-Text($RelativePath) {
    return Get-Content -LiteralPath (Join-Path $Root $RelativePath) -Raw
}

function Normalize-LinkTarget($Target) {
    $Clean = $Target.Trim()
    if ($Clean.StartsWith("<") -and $Clean.EndsWith(">")) {
        $Clean = $Clean.Substring(1, $Clean.Length - 2)
    }
    $HashIndex = $Clean.IndexOf("#")
    if ($HashIndex -ge 0) {
        $Clean = $Clean.Substring(0, $HashIndex)
    }
    return [Uri]::UnescapeDataString($Clean.Trim())
}

function Is-ExternalLink($Target) {
    return $Target -match "^(https?|mailto):" -or $Target.StartsWith("#")
}

function Get-RepoRelativePath($Path) {
    $RootUri = New-Object System.Uri(($Root.TrimEnd('\') + '\'))
    $PathUri = New-Object System.Uri($Path)
    return [Uri]::UnescapeDataString($RootUri.MakeRelativeUri($PathUri).ToString()).Replace('/', '\')
}

$RequiredFiles = @(
    "README.md",
    "docs\README.md",
    "docs\ARCHITECTURE.md",
    "docs\BEST_PRACTICES.md",
    "docs\FAQ.md",
    "docs\INTEGRATION.md",
    "docs\MIGRATION.md",
    "docs\DOCS_REVIEW_CHECKLIST.md",
    "docs\api\README.md",
    "docs\api\types.md",
    "docs\api\api-runtime.md",
    "docs\api\api-workspace.md",
    "docs\api\api-tools.md",
    "docs\api\api-run.md",
    "docs\api\api-orchestrator.md",
    "docs\api\api-policy-approval.md",
    "docs\api\api-artifacts.md",
    "docs\api\api-persistence.md",
    "docs\api\api-host-tools.md",
    "docs\api\api-profiles.md",
    "docs\api\api-multi-agent.md",
    "docs\api\api-remote-worker.md",
    "docs\api\api-replay.md",
    "docs\api\api-xllm-integration.md",
    "docs\api\api-local-host.md",
    "docs\guide\README.md",
    "docs\guide\first-xwork-program.md",
    "docs\guide\xllm-orchestrator-intro.md",
    "docs\guide\tool-approval-artifact-intro.md",
    "docs\guide\persistence-replay-intro.md",
    "docs\guide\multi-agent-intro.md",
    "docs\guide\remote-worker-intro.md",
    "docs\guide\profile-intro.md",
    "docs\guide\local-host-tools-intro.md",
    "docs\guide\async-run-cancel-intro.md",
    "docs\guide\workspace-memory-intro.md",
    "docs\guide\artifact-query-intro.md",
    "docs\guide\provider-smoke-intro.md",
    "docs\guide\CODE_SNIPPETS.md",
    "docs\case\README.md",
    "docs\case\first-xwork-program.md",
    "docs\case\ai-ide-agent.md",
    "docs\case\claw-autonomous-agent.md",
    "docs\case\multi-agent-claw.md",
    "docs\case\remote-worker-agent.md",
    "docs\case\replay-agent-run.md"
)

foreach ($File in $RequiredFiles) {
    Require-Path $File
}

Require-Path "README.en.md"

$FormalChineseDocs = Get-ChildItem -LiteralPath $DocsDir -Recurse -File -Filter "*.md" |
    Where-Object { $_.Name -notlike "*.en.md" }
foreach ($Doc in $FormalChineseDocs) {
    $ExpectedEnglish = Join-Path $Doc.DirectoryName ($Doc.BaseName + ".en.md")
    if (-not (Test-Path -LiteralPath $ExpectedEnglish)) {
        $RelativeDoc = Get-RepoRelativePath $Doc.FullName
        Fail "missing English counterpart for $RelativeDoc"
    }
}

$MarkdownFiles = @(
    (Join-Path $Root "README.md"),
    (Join-Path $Root "README.en.md"),
    (Join-Path $Root "examples\README.md")
)
$MarkdownFiles += Get-ChildItem -LiteralPath (Join-Path $Root "dev") -File -Filter "*.md" | ForEach-Object { $_.FullName }
$MarkdownFiles += Get-ChildItem -LiteralPath $DocsDir -Recurse -File -Filter "*.md" | ForEach-Object { $_.FullName }
$MarkdownFiles += Get-ChildItem -LiteralPath $DevDocsDir -Recurse -File -Filter "*.md" | ForEach-Object { $_.FullName }

$FormalIndexFiles = @(
    "README.md",
    "README.en.md",
    "docs\README.md",
    "docs\README.en.md",
    "docs\api\README.md",
    "docs\api\README.en.md",
    "docs\guide\README.md",
    "docs\guide\README.en.md",
    "docs\case\README.md",
    "docs\case\README.en.md"
)
foreach ($IndexFile in $FormalIndexFiles) {
    $IndexText = Read-Text $IndexFile
    if ($IndexText -match "TODO|待补|未完成|Chinese source") {
        Fail "formal index contains stale status marker: $IndexFile"
    }
}

$LinkPattern = [regex]'\[[^\]]+\]\(([^)]+)\)'
foreach ($File in $MarkdownFiles) {
    $Text = Get-Content -LiteralPath $File -Raw
    foreach ($Match in $LinkPattern.Matches($Text)) {
        $Target = Normalize-LinkTarget $Match.Groups[1].Value
        if ([string]::IsNullOrWhiteSpace($Target) -or (Is-ExternalLink $Target)) {
            continue
        }
        if ($Target -match '^[A-Za-z]:[\\/]') {
            $Resolved = $Target
        } else {
            $Resolved = Join-Path (Split-Path -Parent $File) $Target
        }
        if (-not (Test-Path -LiteralPath $Resolved)) {
            $RelativeFile = Get-RepoRelativePath $File
            Fail "broken link in $RelativeFile -> $Target"
        }

        $FullResolved = (Resolve-Path -LiteralPath $Resolved).Path
        $IsFormalDoc = $File.StartsWith($DocsDir, [System.StringComparison]::OrdinalIgnoreCase)
        $DevDir = Join-Path $Root "dev"
        $LinksToDev = $FullResolved.StartsWith($DevDir, [System.StringComparison]::OrdinalIgnoreCase)
        if ($IsFormalDoc -and $LinksToDev -and -not $FullResolved.StartsWith($DevDocsDir, [System.StringComparison]::OrdinalIgnoreCase)) {
            $RelativeFile = Get-RepoRelativePath $File
            Fail "formal docs may only link to dev/docs internal references: $RelativeFile -> $Target"
        }
    }
}

$Header = Get-Content -LiteralPath $HeaderPath -Raw

$ApiPagesRequiringC = Get-ChildItem -LiteralPath (Join-Path $DocsDir "api") -File -Filter "*.md" |
    Where-Object { $_.Name -notin @("README.md", "README.en.md", "API_PAGE_TEMPLATE.md") }
foreach ($Page in $ApiPagesRequiringC) {
    $PageText = Get-Content -LiteralPath $Page.FullName -Raw
    if ($PageText -notmatch '```c') {
        Fail "API page should include at least one C snippet: $($Page.Name)"
    }
}

$PersistenceVersionMatch = [regex]::Match($Header, '#define\s+XWORK_PERSISTENCE_FORMAT_VERSION\s+(\d+)')
if (-not $PersistenceVersionMatch.Success) {
    Fail "XWORK_PERSISTENCE_FORMAT_VERSION not found in xwork.h"
}
$PersistenceVersion = $PersistenceVersionMatch.Groups[1].Value
if ((Read-Text "docs\api\api-persistence.md") -notmatch "XWORK_PERSISTENCE_FORMAT_VERSION[^\r\n]+`?$PersistenceVersion`?") {
    Fail "persistence API doc does not mention current persistence format version $PersistenceVersion"
}

$RemoteVersionMatch = [regex]::Match($Header, '#define\s+XWORK_REMOTE_PROTOCOL_VERSION_CURRENT\s+(\d+)')
if (-not $RemoteVersionMatch.Success) {
    Fail "XWORK_REMOTE_PROTOCOL_VERSION_CURRENT not found in xwork.h"
}
$RemoteVersion = $RemoteVersionMatch.Groups[1].Value
if ((Read-Text "docs\api\api-remote-worker.md") -notmatch "XWORK_REMOTE_PROTOCOL_VERSION_CURRENT[^\r\n]+`?$RemoteVersion`?") {
    Fail "remote worker API doc does not mention current remote protocol version $RemoteVersion"
}

$SchemaConstants = @(
    "XWORK_REPORT_SCHEMA_V1",
    "XWORK_DIAGNOSTICS_SCHEMA_V1",
    "XWORK_PATCH_APPLY_RESULT_SCHEMA_V1",
    "XWORK_PATCH_FILE_SUMMARY_SCHEMA_V1",
    "XWORK_TERMINAL_STATE_SCHEMA_V1",
    "XWORK_TERMINAL_INVENTORY_SCHEMA_V1"
)
foreach ($Name in $SchemaConstants) {
    if ($Header -notmatch "\b$Name\b") {
        Fail "schema constant missing from xwork.h: $Name"
    }
    if ((Read-Text "docs\api\api-artifacts.md") -notmatch "\b$Name\b") {
        Fail "schema constant missing from artifact API doc: $Name"
    }
}

$ToolConstants = [regex]::Matches($Header, '#define\s+(XWORK_TOOL_[A-Z0-9_]+)\s+"([^"]+)"')
if ($ToolConstants.Count -lt 10) {
    Fail "expected built-in tool constants in xwork.h"
}
$HostToolDoc = Read-Text "docs\api\api-host-tools.md"
foreach ($Match in $ToolConstants) {
    $ToolName = $Match.Groups[2].Value
    if ($HostToolDoc -notmatch [regex]::Escape($ToolName)) {
        Fail "built-in tool name missing from host tool API doc: $ToolName"
    }
}

$ProfileDocs = @(
    "docs\api\api-profiles.md",
    "docs\api\api-profiles.en.md",
    "docs\guide\profile-intro.md",
    "docs\guide\profile-intro.en.md"
)
$ProfileIdentifiers = @(
    "XWORK_PROFILE_XCODE",
    "XWORK_PROFILE_XCLAW",
    "xwork_profile_get_builtin",
    "xwork_profile_apply_runtime_options",
    "xwork_profile_apply_run_options"
)
foreach ($Name in $ProfileIdentifiers) {
    if ($Header -notmatch "\b$Name\b") {
        Fail "profile identifier missing from xwork.h: $Name"
    }
}
foreach ($DocPath in $ProfileDocs) {
    $ProfileDoc = Read-Text $DocPath
    foreach ($Name in $ProfileIdentifiers) {
        if ($ProfileDoc -notmatch "\b$Name\b") {
            Fail "profile identifier missing from ${DocPath}: $Name"
        }
    }
}
foreach ($DocPath in @("docs\api\api-profiles.en.md", "docs\guide\profile-intro.en.md", "docs\case\ai-ide-agent.en.md", "docs\case\claw-autonomous-agent.en.md")) {
    if ((Read-Text $DocPath) -match "xwork_get_builtin_profile|xwork_runtime_register_profile") {
        Fail "stale profile API name in $DocPath"
    }
}

$Examples = @(
    "examples\first_xwork_program.c",
    "examples\ai_ide_agent.c",
    "examples\claw_autonomous_agent.c",
    "examples\multi_agent_claw.c",
    "examples\remote_worker_agent.c",
    "examples\replay_agent_run.c"
)
foreach ($Example in $Examples) {
    Require-Path $Example
}

$ExamplesReadme = Read-Text "examples\README.md"
$GccCommandCount = ([regex]::Matches($ExamplesReadme, '(?m)^\s*gcc\s+')).Count
if ($GccCommandCount -lt $Examples.Count) {
    Fail "examples/README.md should contain at least $($Examples.Count) gcc build commands"
}
foreach ($Example in $Examples) {
    $Name = Split-Path -Leaf $Example
    if ($ExamplesReadme -notmatch [regex]::Escape($Name)) {
        Fail "examples/README.md does not mention $Name"
    }
}

$Ci = Read-Text ".github\workflows\ci.yml"
if ($Ci -notmatch "tools/check_docs\.ps1") {
    Fail "CI does not run tools/check_docs.ps1"
}
if ($Ci -notmatch "first_xwork_program\.c") {
    Fail "CI examples target does not build first_xwork_program.c"
}

Write-Host "docs check OK"
