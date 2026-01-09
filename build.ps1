# Build script for CivilizationVI_CommunityExtension
# Usage: .\build.ps1 [-Clean] [-Release] [-Debug] [-BuildDeps] [-Help]

param(
    [switch]$Clean,
    [switch]$Release,
    [switch]$Debug,
    [switch]$BuildDeps,
    [switch]$Help
)

$ErrorActionPreference = "Stop"
$ProjectRoot = $PSScriptRoot
$LibDir = Join-Path $ProjectRoot "lib\Release"

# Default to Release if neither specified
if (-not $Release -and -not $Debug) {
    $Release = $true
}

function Write-Header {
    param([string]$Message)
    Write-Host ""
    Write-Host "===== $Message =====" -ForegroundColor Cyan
    Write-Host ""
}

function Write-Success {
    param([string]$Message)
    Write-Host $Message -ForegroundColor Green
}

function Write-Error {
    param([string]$Message)
    Write-Host $Message -ForegroundColor Red
}

function Show-Help {
    Write-Host @"
CivilizationVI_CommunityExtension Build Script

USAGE:
    .\build.ps1 [options]

OPTIONS:
    -Release    Build Release configuration (default)
    -Debug      Build Debug configuration
    -Clean      Clean build artifacts before building
    -BuildDeps  Force rebuild of dependencies (MinHook, Capstone)
    -Help       Show this help message

EXAMPLES:
    .\build.ps1                  # Build Release
    .\build.ps1 -Clean           # Clean and build Release
    .\build.ps1 -Debug           # Build Debug
    .\build.ps1 -BuildDeps       # Rebuild dependencies and project
    .\build.ps1 -Clean -Debug    # Clean and build Debug

REQUIREMENTS:
    - Visual Studio 2022 with C++ workload
    - CMake (for Capstone)
    - Git (for cloning dependencies)
"@
}

function Find-MSBuild {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\amd64\MSBuild.exe | Select-Object -First 1
        if ($msbuild) {
            return $msbuild
        }
    }

    # Fallback paths
    $fallbacks = @(
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\amd64\MSBuild.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\amd64\MSBuild.exe"
    )

    foreach ($path in $fallbacks) {
        if (Test-Path $path) {
            return $path
        }
    }

    throw "MSBuild not found. Please install Visual Studio 2022 with C++ workload."
}

function Find-CMake {
    $cmake = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmake) {
        return $cmake.Source
    }

    # Check common paths
    $fallbacks = @(
        "${env:ProgramFiles}\CMake\bin\cmake.exe",
        "${env:ProgramFiles(x86)}\CMake\bin\cmake.exe"
    )

    foreach ($path in $fallbacks) {
        if (Test-Path $path) {
            return $path
        }
    }

    throw "CMake not found. Please install CMake and add it to PATH."
}

function Build-MinHook {
    Write-Header "Building MinHook"

    $minhookDir = Join-Path $ProjectRoot "minhook"
    $minhookLib = Join-Path $LibDir "libMinHook.x64.lib"

    if ((Test-Path $minhookLib) -and -not $BuildDeps) {
        Write-Success "MinHook already built, skipping (use -BuildDeps to force rebuild)"
        return
    }

    # Clone if not exists
    if (-not (Test-Path $minhookDir)) {
        Write-Host "Cloning MinHook..."
        git clone https://github.com/TsudaKageyu/minhook.git $minhookDir
    }

    # Build
    Write-Host "Building MinHook..."
    $msbuild = Find-MSBuild
    & $msbuild "$minhookDir\build\VC17\libMinHook.vcxproj" /p:Configuration=Release /p:Platform=x64 /m /v:minimal

    if ($LASTEXITCODE -ne 0) {
        throw "MinHook build failed"
    }

    # Copy library
    New-Item -ItemType Directory -Force -Path $LibDir | Out-Null
    Copy-Item "$minhookDir\build\VC17\lib\Release\libMinHook.x64.lib" $LibDir -Force

    Write-Success "MinHook built successfully"
}

function Build-Capstone {
    Write-Header "Building Capstone"

    $capstoneDir = Join-Path $ProjectRoot "capstone"
    $capstoneLib = Join-Path $LibDir "capstone.lib"

    if ((Test-Path $capstoneLib) -and -not $BuildDeps) {
        Write-Success "Capstone already built, skipping (use -BuildDeps to force rebuild)"
        return
    }

    # Clone if not exists
    if (-not (Test-Path $capstoneDir)) {
        Write-Host "Cloning Capstone..."
        git clone https://github.com/capstone-engine/capstone.git $capstoneDir
    }

    # Build with CMake
    Write-Host "Building Capstone..."
    $cmake = Find-CMake

    Push-Location $capstoneDir
    try {
        & $cmake -B build -A x64 2>&1 | Out-Host
        & $cmake --build build --config Release 2>&1 | Out-Host

        if ($LASTEXITCODE -ne 0) {
            throw "Capstone build failed"
        }
    }
    finally {
        Pop-Location
    }

    # Copy library
    New-Item -ItemType Directory -Force -Path $LibDir | Out-Null
    Copy-Item "$capstoneDir\build\Release\capstone.lib" $LibDir -Force

    Write-Success "Capstone built successfully"
}

function Build-Project {
    param([string]$Configuration)

    Write-Header "Building CivilizationVI_CommunityExtension ($Configuration)"

    $msbuild = Find-MSBuild
    $projectFile = Join-Path $ProjectRoot "CivilizationVI_CommunityExtension.vcxproj"

    $target = if ($Clean) { "Rebuild" } else { "Build" }

    & $msbuild $projectFile /p:Configuration=$Configuration /p:Platform=x64 /m /t:$target /v:minimal

    if ($LASTEXITCODE -ne 0) {
        throw "Build failed"
    }

    Write-Success "Build completed successfully!"

    # Show output location
    $outputDir = "$env:USERPROFILE\Documents\My Games\Sid Meier's Civilization VI\Mods\CivilizationVI_CommunityExtension_Mod\Binaries\Win64"
    Write-Host ""
    Write-Host "Output: $outputDir\GameCore_XP2_CE_FinalRelease.dll" -ForegroundColor Yellow
}

function Clean-Project {
    Write-Header "Cleaning build artifacts"

    $dirsToClean = @(
        "Civiliza.42f0ba07",
        "x64",
        "Debug",
        "Release"
    )

    foreach ($dir in $dirsToClean) {
        $path = Join-Path $ProjectRoot $dir
        if (Test-Path $path) {
            Write-Host "Removing $dir..."
            Remove-Item -Recurse -Force $path
        }
    }

    Write-Success "Clean completed"
}

# Main
if ($Help) {
    Show-Help
    exit 0
}

try {
    Write-Host "CivilizationVI_CommunityExtension Build Script" -ForegroundColor Magenta
    Write-Host "=============================================" -ForegroundColor Magenta

    # Clean if requested
    if ($Clean) {
        Clean-Project
    }

    # Build dependencies
    Build-MinHook
    Build-Capstone

    # Build project
    $config = if ($Debug) { "Debug" } else { "Release" }
    Build-Project -Configuration $config

    Write-Host ""
    Write-Success "All done!"
}
catch {
    Write-Error "Build failed: $_"
    exit 1
}
