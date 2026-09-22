param(
    [switch]$SkipPackage
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = (
    Resolve-Path (
        Join-Path $PSScriptRoot ".."
    )
).Path

Push-Location $RepoRoot

try {
    $IconPath = Join-Path $RepoRoot "resources\VidView.ico"

    if (-not (Test-Path $IconPath)) {
        Write-Host ""
        Write-Host "=== Generating application icon ==="

        & python ".\scripts\generate_icon.py"

        if ($LASTEXITCODE -ne 0) {
            throw "Icon generation failed."
        }
    }

    if (-not $SkipPackage) {
        Write-Host ""
        Write-Host "=== Building packaged application ==="

        & ".\scripts\package.ps1"

        if ($LASTEXITCODE -ne 0) {
            throw "VidView packaging failed."
        }
    }

    $CMakeText = Get-Content ".\CMakeLists.txt" -Raw

    $VersionMatch = [regex]::Match(
        $CMakeText,
        'VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)'
    )

    if (-not $VersionMatch.Success) {
        throw "Could not determine VidView version from CMakeLists.txt."
    }

    $Version = $VersionMatch.Groups[1].Value

    Write-Host ""
    Write-Host "VidView version: $Version"

    $Candidates = @()

    if ($env:INNO_SETUP_PATH) {
        $Candidates += $env:INNO_SETUP_PATH
    }

    if (${env:ProgramFiles(x86)}) {
        $Candidates += (
            Join-Path `
                ${env:ProgramFiles(x86)} `
                "Inno Setup 6\ISCC.exe"
        )
    }

    if ($env:ProgramFiles) {
        $Candidates += (
            Join-Path `
                $env:ProgramFiles `
                "Inno Setup 6\ISCC.exe"
        )
    }

    if ($env:LOCALAPPDATA) {
        $Candidates += (
            Join-Path `
                $env:LOCALAPPDATA `
                "Programs\Inno Setup 6\ISCC.exe"
        )
    }

    $Iscc = $Candidates |
        Where-Object {
            $_ -and (Test-Path $_)
        } |
        Select-Object -First 1

    if (-not $Iscc) {
        throw @"
Inno Setup compiler was not found.

Install it with:

winget install --id JRSoftware.InnoSetup -e --source winget
"@
    }

    Write-Host ""
    Write-Host "=== Building Windows installer ==="
    Write-Host "Using: $Iscc"

    & $Iscc `
        "/DMyAppVersion=$Version" `
        ".\installer\VidView.iss"

    if ($LASTEXITCODE -ne 0) {
        throw "Inno Setup compilation failed."
    }

    $Installer = Join-Path `
        $RepoRoot `
        "dist\VidView-Setup-$Version.exe"

    if (-not (Test-Path $Installer)) {
        throw "Installer was not produced at $Installer"
    }

    Write-Host ""
    Write-Host "====================================="
    Write-Host " VidView installer built successfully"
    Write-Host "====================================="
    Write-Host ""
    Write-Host $Installer
}
finally {
    Pop-Location
}