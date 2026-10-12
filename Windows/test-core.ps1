param(
    [string]$WinDivertPath = "C:\WinDivert-2.2.2-A",
    [string[]]$Tests = @("relay_peer", "packet_batch", "tcp_half_close", "worker_lifecycle", "loopback_bypass")
)
$ErrorActionPreference = "Stop"
$testOutput = Join-Path ([IO.Path]::GetTempPath()) "proxybridge-core-tests"
New-Item -ItemType Directory -Path $testOutput -Force | Out-Null
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) { throw "Visual Studio C++ x64 tools are unavailable." }
$vcVars = Join-Path $vsPath "VC\Auxiliary\Build\vcvarsall.bat"
$coreSources = @("ProxyBridge", "pb_util", "pb_process", "pb_rules", "pb_proxy", "pb_dns", "pb_socks5", "pb_http", "pb_conntrack", "pb_relay")
foreach ($test in $Tests) {
    if ($test -notmatch '^[a-z_]+$') { throw "Invalid test name" }
    $testSource = Join-Path $PSScriptRoot "tests\test_$test.c"
    $testText = Get-Content -LiteralPath $testSource -Raw
    $sources = @($coreSources | Where-Object { $testText -notmatch ('#include "../src/' + $_ + '\.c"') } | ForEach-Object { '"' + (Join-Path $PSScriptRoot "src\$_.c") + '"' })
    $sources += '"' + $testSource + '"'
    $testPath = Join-Path $testOutput "test_$test.exe"
    $compileArgs = "/nologo /utf-8 /O2 /W4 /wd4100 /wd4189 /wd4267 /wd4244 /wd4996 /D_CRT_SECURE_NO_WARNINGS /D_WINSOCK_DEPRECATED_NO_WARNINGS /DPROXYBRIDGE_EXPORTS " +
        "/I`"$WinDivertPath\include`" " + ($sources -join ' ') + " /link /LIBPATH:`"$WinDivertPath\x64`" WinDivert.lib ws2_32.lib iphlpapi.lib /OUT:`"$testPath`""
    cmd /c "`"$vcVars`" x64 >nul && cd /d `"$testOutput`" && cl.exe $compileArgs"
    if ($LASTEXITCODE -ne 0) { throw "Test $test build failed." }
    Copy-Item -LiteralPath "$WinDivertPath\x64\WinDivert.dll" -Destination $testOutput -Force
    & $testPath
    if ($LASTEXITCODE -ne 0) { throw "Test $test failed." }
}
