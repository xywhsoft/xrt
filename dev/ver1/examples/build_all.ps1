$sScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$sRootDir = Split-Path -Parent $sScriptDir
$sBinDir = Join-Path $sScriptDir 'bin'

if ( -not (Test-Path $sBinDir) ) {
	New-Item -ItemType Directory -Path $sBinDir | Out-Null
}

$sCompiler = if ( Get-Command tcc -ErrorAction SilentlyContinue ) { 'tcc' } else { 'gcc' }
$iPass = 0
$iFail = 0
$arrFail = @()

Set-Location $sRootDir

$arrSources = Get-ChildItem -Path $sScriptDir -Recurse -Filter main.c | Where-Object {
	$sRelDir = $_.DirectoryName.Substring($sScriptDir.Length).TrimStart('\', '/')
	$arrParts = $sRelDir -split '[\\/]'
	return $arrParts.Count -eq 2
} | Sort-Object FullName

foreach ( $objFile in $arrSources ) {
	$sRelDir = $objFile.DirectoryName.Substring($sScriptDir.Length).TrimStart('\', '/')
	$arrParts = $sRelDir -split '[\\/]'
	$sModule = $arrParts[0]
	$sName = $arrParts[1]
	$sOut = Join-Path $sBinDir "${sModule}_${sName}.exe"

	Write-Host "[BUILD] $sModule/$sName"
	& $sCompiler -m64 $objFile.FullName -I . -lWs2_32 -lShell32 -lIPHLPAPI -o $sOut

	if ( $LASTEXITCODE -eq 0 ) {
		$iPass++
		Write-Host "[ OK ] $sModule/$sName"
	} else {
		$iFail++
		$arrFail += "$sModule/$sName"
		Write-Host "[FAIL] $sModule/$sName"
	}
}

Write-Host ''
Write-Host 'Build finished.'
Write-Host "Success: $iPass"
Write-Host "Failed : $iFail"

if ( $arrFail.Count -gt 0 ) {
	Write-Host 'Failed list:'
	$arrFail | ForEach-Object {
		Write-Host " - $_"
	}
}

exit 0
