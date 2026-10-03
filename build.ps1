$ErrorActionPreference = "Stop"

$srcFiles = @(
    "src/dllmain.cpp",
    "src/config.cpp",
    "src/controls.cpp",
    "src/memory.cpp"
)

$outDll = "ErFluidControls.dll"

Write-Host "Compiling $outDll with g++..."

& g++ -std=c++20 -O3 -shared -DNDEBUG -fno-exceptions -static -static-libgcc -static-libstdc++ `
    $srcFiles `
    -o $outDll `
    -luser32 -lkernel32

if ($LASTEXITCODE -ne 0) {
    Write-Error "Compilation failed!"
    exit 1
}

Write-Host "Build successful: $outDll"
$dllItem = Get-Item $outDll
Write-Host "Size: $($dllItem.Length) bytes"
