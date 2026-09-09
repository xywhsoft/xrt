$ErrorActionPreference = "Stop"

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

function Has-Han($Text) {
    return $Text -match '[\p{IsCJKUnifiedIdeographs}]'
}

function Get-EnglishPath($Path) {
    $Dir = Split-Path -Parent $Path
    $Name = Split-Path -Leaf $Path
    if ($Name -like "*.en.md") {
        return $null
    }
    if ($Name -eq "README.md" -and $Dir -eq $Root) {
        return (Join-Path $Root "README.en.md")
    }
    $Base = [System.IO.Path]::GetFileNameWithoutExtension($Name)
    return (Join-Path $Dir ($Base + ".en.md"))
}

function Get-RelativePath($BasePath, $Path) {
    $BaseUri = New-Object System.Uri(($BasePath.TrimEnd('\') + '\'))
    $PathUri = New-Object System.Uri($Path)
    return [Uri]::UnescapeDataString($BaseUri.MakeRelativeUri($PathUri).ToString()).Replace('/', '\')
}

function Protect-InlineMarkdown($Text, [ref]$Placeholders) {
    $Result = $Text
    $Patterns = @(
        '\[[^\]]+\]\([^)]+\)',
        '`[^`]*`'
    )
    foreach ($Pattern in $Patterns) {
        while ($Result -match $Pattern) {
            $Value = $Matches[0]
            $Key = "XWORKPLACEHOLDER$($Placeholders.Value.Count)TOKEN"
            $Placeholders.Value[$Key] = $Value
            $Result = $Result.Remove($Result.IndexOf($Value), $Value.Length).Insert($Result.IndexOf($Value), $Key)
        }
    }
    return $Result
}

function Restore-InlineMarkdown($Text, $Placeholders) {
    $Result = $Text
    foreach ($Key in $Placeholders.Keys) {
        $Result = $Result.Replace($Key, $Placeholders[$Key])
    }
    return $Result
}

function Invoke-GoogleTranslate($Text) {
    if ([string]::IsNullOrWhiteSpace($Text) -or -not (Has-Han $Text)) {
        return $Text
    }

    $Placeholders = @{}
    $Protected = Protect-InlineMarkdown $Text ([ref]$Placeholders)
    if (-not (Has-Han $Protected)) {
        return $Text
    }

    $Uri = "https://translate.googleapis.com/translate_a/single?client=gtx&sl=zh-CN&tl=en&dt=t&q=" + [Uri]::EscapeDataString($Protected)
    $Response = Invoke-RestMethod -Uri $Uri -Method Get -TimeoutSec 30
    $TranslatedParts = @()
    foreach ($Part in $Response[0]) {
        if ($null -ne $Part[0]) {
            $TranslatedParts += [string]$Part[0]
        }
    }
    $Translated = ($TranslatedParts -join "")
    return (Restore-InlineMarkdown $Translated $Placeholders)
}

function Translate-Paragraph($Lines) {
    if ($Lines.Count -eq 0) {
        return @()
    }
    $Text = ($Lines -join "`n")
    if (-not (Has-Han $Text)) {
        return $Lines
    }
    try {
        $Translated = Invoke-GoogleTranslate $Text
        return ($Translated -split "`n", -1)
    } catch {
        Write-Warning "translation failed for paragraph: $($_.Exception.Message)"
        return $Lines
    }
}

function Translate-Markdown($Text) {
    $Lines = $Text -split "`r?`n", -1
    $Out = New-Object System.Collections.Generic.List[string]
    $Paragraph = New-Object System.Collections.Generic.List[string]
    $InFence = $false

    foreach ($Line in $Lines) {
        if ($Line -match '^\s*```') {
            foreach ($TranslatedLine in (Translate-Paragraph $Paragraph)) {
                $Out.Add($TranslatedLine)
            }
            $Paragraph.Clear()
            $InFence = -not $InFence
            $Out.Add($Line)
            continue
        }

        if ($InFence) {
            $Out.Add($Line)
            continue
        }

        if ([string]::IsNullOrWhiteSpace($Line)) {
            foreach ($TranslatedLine in (Translate-Paragraph $Paragraph)) {
                $Out.Add($TranslatedLine)
            }
            $Paragraph.Clear()
            $Out.Add($Line)
            continue
        }

        $Paragraph.Add($Line)
    }

    foreach ($TranslatedLine in (Translate-Paragraph $Paragraph)) {
        $Out.Add($TranslatedLine)
    }

    return ($Out -join "`n")
}

$MarkdownFiles = @()
$MarkdownFiles += Join-Path $Root "README.md"
$MarkdownFiles += Join-Path $Root "examples\README.md"
$MarkdownFiles += Get-ChildItem -LiteralPath (Join-Path $Root "docs") -Recurse -File -Filter "*.md" |
    Where-Object { $_.Name -notlike "*.en.md" } |
    ForEach-Object { $_.FullName }
$MarkdownFiles += Get-ChildItem -LiteralPath (Join-Path $Root "dev") -Recurse -File -Filter "*.md" |
    Where-Object { $_.Name -notlike "*.en.md" } |
    ForEach-Object { $_.FullName }

$MarkdownFiles = $MarkdownFiles | Sort-Object -Unique

foreach ($Path in $MarkdownFiles) {
    $Text = Get-Content -LiteralPath $Path -Raw -Encoding UTF8
    if (-not (Has-Han $Text)) {
        continue
    }
    $EnglishPath = Get-EnglishPath $Path
    if (-not $EnglishPath) {
        continue
    }
    Write-Host "Translating $(Get-RelativePath $Root $Path) -> $(Get-RelativePath $Root $EnglishPath)"
    $Translated = Translate-Markdown $Text
    $Parent = Split-Path -Parent $EnglishPath
    if (-not (Test-Path -LiteralPath $Parent)) {
        New-Item -ItemType Directory -Path $Parent | Out-Null
    }
    [System.IO.File]::WriteAllText(
        $EnglishPath,
        $Translated,
        (New-Object System.Text.UTF8Encoding($false))
    )
}
