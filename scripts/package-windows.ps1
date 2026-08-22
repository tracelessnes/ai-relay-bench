param(
    [string]$QtRoot = "C:\Qt\6.11.1\mingw_64",
    [string]$CompilerRoot = "C:\Qt\Tools\mingw1310_64",
    [string]$BuildDir = "build-release",
    [string]$DistDir = "dist"
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
$cmake = (Get-Command cmake).Source
$ninja = (Get-Command ninja).Source
$env:PATH = "$QtRoot\bin;$CompilerRoot\bin;$env:PATH"
$cmakeListsPath = Join-Path $root "CMakeLists.txt"
$cmakeLists = [IO.File]::ReadAllText($cmakeListsPath, [Text.UTF8Encoding]::new($false))
$versionMatch = [regex]::Match(
    $cmakeLists,
    '(?im)^\s*project\s*\(\s*[^\s\)]+\s+VERSION\s+(?<version>\d+(?:\.\d+){2})(?:\s|\))')
if (-not $versionMatch.Success) {
    throw "Unable to read a semantic project version from $cmakeListsPath"
}
$version = $versionMatch.Groups["version"].Value
& $cmake -S . -B $BuildDir -G Ninja -DCMAKE_PREFIX_PATH=$QtRoot -DCMAKE_CXX_COMPILER="$CompilerRoot\bin\g++.exe" -DCMAKE_BUILD_TYPE=Release
& $cmake --build $BuildDir -j 4
& ctest --test-dir $BuildDir --output-on-failure
$packageName = "AI-Relay-Bench-$version-win64"
$target = Join-Path $DistDir $packageName
if (Test-Path -LiteralPath $target) {
    $resolved = [IO.Path]::GetFullPath($target)
    $distResolved = [IO.Path]::GetFullPath($DistDir)
    if (-not $resolved.StartsWith($distResolved, [StringComparison]::OrdinalIgnoreCase)) { throw "Unsafe target path: $resolved" }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
New-Item -ItemType Directory -Force $target | Out-Null
Copy-Item -LiteralPath (Join-Path $BuildDir "ai-relay-bench.exe") -Destination $target
Copy-Item -LiteralPath "README.md" -Destination $target
& "$QtRoot\bin\windeployqt.exe" --release --compiler-runtime --no-translations --dir $target (Join-Path $target "ai-relay-bench.exe")
$zip = Join-Path $DistDir "$packageName.zip"
if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
Compress-Archive -Path (Join-Path $target "*") -DestinationPath $zip -CompressionLevel Optimal
Write-Host "Package ready: $target"
Write-Host "ZIP ready: $zip"
