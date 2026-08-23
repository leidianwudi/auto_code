# Build config (default Debug; for Release use: .trae\build.ps1 Release)
param([string]$Config = "Debug")

# Clear all SAFE_RM_* environment variables
Get-ChildItem Env:SAFE_RM_* | Remove-Item -ErrorAction SilentlyContinue

# Setup VS dev environment (if not loaded yet)
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    # Locate VS install path via vswhere so the script works across machines
    $vsDevCmd = $null
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $installPath = & $vswhere -latest -products * -property installationPath
        if ($installPath) {
            $candidate = Join-Path $installPath "Common7\Tools\VsDevCmd.bat"
            if (Test-Path $candidate) { $vsDevCmd = $candidate }
        }
    }
    if (-not $vsDevCmd) {
        Write-Error "VsDevCmd.bat not found (vswhere: $vswhere)"
        exit 1
    }
    # Run VsDevCmd in a child process, print its env, then copy back line by line.
    # Do NOT use `cmd /c "... && set"` directly: the env would only exist in the
    # child process and the parent still cannot find cmake.
    $envLines = cmd /c "`"$vsDevCmd`" -arch=x64 -host_arch=x64 >nul 2>&1 && set"
    foreach ($line in $envLines) {
        if ($line -match '^([^=]+)=(.*)$') {
            # Env var names may contain spaces (e.g. Program Files); skip invalid names
            if ($matches[1] -notmatch '\s') {
                Set-Item -Path "Env:$($matches[1])" -Value $matches[2]
            }
        }
    }
    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
        Write-Error "VsDevCmd executed but cmake is still not on PATH. Check VsDevCmd path or CMake installation."
        exit 1
    }
}

# Run the build ($PSScriptRoot = script dir, ..\build = build dir under project root)
# Note: VS generator builds ALL_BUILD by default when no --target is given;
# there is no "all" target in VS solutions.
$buildDir = Join-Path $PSScriptRoot '..\build'
cmake --build $buildDir --config $Config
exit $LASTEXITCODE
