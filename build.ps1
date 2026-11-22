# Build TxtAutoWrap.dll (x64) with MinGW-w64.
# Requires g++ and windres on PATH, e.g.: winget install BrechtSanders.WinLibs.POSIX.UCRT
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

windres -c 65001 -O coff -i TxtAutoWrap.rc -o TxtAutoWrap_res.o
if ($LASTEXITCODE) { exit $LASTEXITCODE }

g++ -O2 -Wall -shared -municode -static -static-libgcc -static-libstdc++ -s `
    -o TxtAutoWrap.dll TxtAutoWrap.cpp TxtAutoWrap_res.o -lshlwapi
$code = $LASTEXITCODE
Remove-Item TxtAutoWrap_res.o -ErrorAction SilentlyContinue
exit $code
