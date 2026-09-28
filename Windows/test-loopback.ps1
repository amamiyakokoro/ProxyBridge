param(
    [string]$WinDivertPath = "C:\WinDivert-2.2.2-A",
    [string]$OutputDirectory = "output\loopback-test"
)
$ErrorActionPreference = "Stop"
$OutputDirectory = if ([IO.Path]::IsPathRooted($OutputDirectory)) { $OutputDirectory } else { Join-Path $PSScriptRoot $OutputDirectory }
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) { throw "Visual Studio C++ x64 tools are unavailable." }
$vcVars = Join-Path $vsPath "VC\Auxiliary\Build\vcvarsall.bat"
$Sources = @(
    "src\ProxyBridge.c", "src\pb_util.c", "src\pb_process.c", "tests\test_loopback_bypass.c",
    "src\pb_proxy.c", "src\pb_dns.c", "src\pb_socks5.c", "src\pb_http.c",
    "src\pb_conntrack.c", "src\pb_relay.c"
) | ForEach-Object { '"' + (Join-Path $PSScriptRoot $_) + '"' }
$TestPath = Join-Path $OutputDirectory "test_loopback_bypass.exe"
$CompileArgs = "/nologo /utf-8 /O2 /W4 /wd4100 /wd4189 /wd4267 /wd4244 /wd4996 " +
    "/D_CRT_SECURE_NO_WARNINGS /D_WINSOCK_DEPRECATED_NO_WARNINGS /DPROXYBRIDGE_EXPORTS " +
    "/I`"$WinDivertPath\include`" " + ($Sources -join " ") +
    " /link /LIBPATH:`"$WinDivertPath\x64`" WinDivert.lib ws2_32.lib iphlpapi.lib /OUT:`"$TestPath`""
cmd /c "`"$vcVars`" x64 >nul && cd /d `"$OutputDirectory`" && cl.exe $CompileArgs"
if ($LASTEXITCODE -ne 0) { throw "Loopback regression test build failed." }
Copy-Item "$WinDivertPath\x64\WinDivert.dll" $OutputDirectory -Force
& $TestPath
if ($LASTEXITCODE -ne 0) { throw "Loopback regression test failed." }
