param(
    [Parameter(Mandatory = $true)][string]$VcVars,
    [Parameter(Mandatory = $true)][string]$OutputPath
)

$ErrorActionPreference = "Stop"
$SourceRevision = & git -C $PSScriptRoot rev-parse --verify HEAD
if ($LASTEXITCODE -ne 0 -or $SourceRevision -notmatch '^[0-9a-f]{40}$') {
    throw "Cannot identify the ProxyBridge source revision."
}
$SourceStatus = & git -C $PSScriptRoot status --porcelain --untracked-files=normal -- .
if ($LASTEXITCODE -ne 0) { throw "Cannot inspect the ProxyBridge source worktree." }
if ($SourceStatus) { $SourceRevision += "-dirty" }

$OutputPath = [System.IO.Path]::GetFullPath($OutputPath)
$ResourceSource = Join-Path $PSScriptRoot "src/core.rc"
$HeaderDirectory = Join-Path ([System.IO.Path]::GetTempPath()) ("proxybridge-version-" + [guid]::NewGuid())
New-Item -ItemType Directory -Path $HeaderDirectory | Out-Null
try {
    $HeaderPath = Join-Path $HeaderDirectory "pb_build_revision.h"
    Set-Content -Path $HeaderPath -Value ('#define PB_SOURCE_REVISION "' + $SourceRevision + '"') -Encoding ascii
    $Command = "`"$VcVars`" x64 >nul && rc.exe /nologo /i `"$HeaderDirectory`" /fo `"$OutputPath`" `"$ResourceSource`""
    cmd /c $Command
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $OutputPath)) {
        throw "ProxyBridge version resource compilation failed."
    }
} finally {
    Remove-Item $HeaderDirectory -Recurse -Force
}
