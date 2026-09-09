param(
    [string]$WinDivertPath = "C:\WinDivert-2.2.2-A",
    [string]$OutputDirectory = "output"
)

$ErrorActionPreference = "Stop"
$SourceFiles = @(
    "src\ProxyBridge.c", "src\pb_util.c", "src\pb_process.c", "src\pb_rules.c",
    "src\pb_proxy.c", "src\pb_dns.c", "src\pb_socks5.c", "src\pb_http.c",
    "src\pb_conntrack.c", "src\pb_relay.c"
) -join " "

if (-not [Environment]::Is64BitProcess) {
    throw "KokoroBox supports only the Windows x64 ProxyBridge core."
}
if (-not (Test-Path "$WinDivertPath\include\windivert.h")) {
    throw "WinDivert 2.2.2-A was not found at $WinDivertPath."
}

$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) { throw "Visual Studio C++ x64 tools are unavailable." }
$vcVars = Join-Path $vsPath "VC\Auxiliary\Build\vcvarsall.bat"

if (Test-Path $OutputDirectory) { Remove-Item $OutputDirectory -Recurse -Force }
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null

$dllPath = Join-Path $OutputDirectory "ProxyBridgeCore.dll"
$args = "/nologo /O2 /GL /Gy /W4 /wd4100 /wd4189 /wd4267 /wd4244 /wd4996 " +
    "/D_CRT_SECURE_NO_WARNINGS /D_WINSOCK_DEPRECATED_NO_WARNINGS /DPROXYBRIDGE_EXPORTS /DNDEBUG " +
    "/GS /guard:cf /I`"$WinDivertPath\include`" $SourceFiles /LD /link /LTCG /OPT:REF /OPT:ICF " +
    "/RELEASE /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT /guard:cf /LIBPATH:`"$WinDivertPath\x64`" " +
    "WinDivert.lib ws2_32.lib iphlpapi.lib /OUT:`"$dllPath`""
$command = "`"$vcVars`" x64 >nul && cd /d `"$PSScriptRoot`" && cl.exe $args"
cmd /c $command
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $dllPath)) {
    throw "ProxyBridgeCore.dll build failed."
}

Copy-Item "$WinDivertPath\x64\WinDivert.dll" $OutputDirectory -Force
Copy-Item "$WinDivertPath\x64\WinDivert64.sys" $OutputDirectory -Force
Write-Host "Built the KokoroBox Windows routing core in $OutputDirectory."
