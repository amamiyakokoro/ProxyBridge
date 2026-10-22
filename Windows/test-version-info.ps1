param(
    [Parameter(Mandatory = $true)][string]$DllPath
)

$ErrorActionPreference = "Stop"
$Header = Get-Content (Join-Path $PSScriptRoot "src/pb_version.h") -Raw
$Numbers = foreach ($Component in @("MAJOR", "MINOR", "PATCH", "BUILD")) {
    if ($Header -notmatch "(?m)^#define PB_VERSION_$Component (\d+)\s*$") {
        throw "Missing version component: $Component"
    }
    [int]$Matches[1]
}
$ExpectedVersion = $Numbers[0..2] -join "."
$Info = [System.Diagnostics.FileVersionInfo]::GetVersionInfo((Resolve-Path $DllPath).Path)
$ActualNumbers = @($Info.FileMajorPart, $Info.FileMinorPart, $Info.FileBuildPart, $Info.FilePrivatePart)
if (($ActualNumbers -join ".") -ne ($Numbers -join ".") -or
    $Info.FileVersion -ne $ExpectedVersion -or $Info.ProductVersion -ne $ExpectedVersion) {
    throw "ProxyBridge DLL version does not match pb_version.h."
}
if ($Info.ProductName -ne "ProxyBridge for KokoroBox" -or
    $Info.OriginalFilename -ne "ProxyBridgeCore.dll" -or -not $Info.IsPrivateBuild) {
    throw "ProxyBridge DLL is missing KokoroBox version metadata."
}
$Revision = & git -C $PSScriptRoot rev-parse --verify HEAD
if ($LASTEXITCODE -ne 0 -or $Info.PrivateBuild -notin @($Revision, "$Revision-dirty")) {
    throw "ProxyBridge DLL source revision does not match this checkout."
}
Write-Host "Verified ProxyBridgeCore.dll $ExpectedVersion ($($Info.PrivateBuild))."
