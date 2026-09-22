param(
    [string]$MsysRoot = "C:\msys64",
    [string]$BuildDir = "build-release",
    [switch]$SkipBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$UcrtBin = Join-Path $MsysRoot "ucrt64\bin"

$CMake = Join-Path $UcrtBin "cmake.exe"
$NtLdd = Join-Path $UcrtBin "ntldd.exe"

$WinDeployQt = Join-Path $UcrtBin "windeployqt6.exe"

if (-not (Test-Path $WinDeployQt)) {
    $WinDeployQt = Join-Path $UcrtBin "windeployqt.exe"
}

foreach ($tool in @($CMake, $NtLdd, $WinDeployQt)) {
    if (-not (Test-Path $tool)) {
        throw "Required tool not found: $tool"
    }
}

$env:PATH = "$UcrtBin;$env:PATH"

Push-Location $RepoRoot

try {
    if (-not $SkipBuild) {
        Write-Host ""
        Write-Host "=== Configuring Release build ==="

        & $CMake --preset ucrt64-release

        if ($LASTEXITCODE -ne 0) {
            throw "CMake configuration failed."
        }

        Write-Host ""
        Write-Host "=== Building Release ==="

        & $CMake --build --preset ucrt64-release

        if ($LASTEXITCODE -ne 0) {
            throw "Release build failed."
        }
    }

    $BuildPath = Join-Path $RepoRoot $BuildDir
    $SourceExe = Join-Path $BuildPath "VidView.exe"

    if (-not (Test-Path $SourceExe)) {
        throw "VidView.exe not found: $SourceExe"
    }

    $DistRoot = Join-Path $RepoRoot "dist"
    $PackageDir = Join-Path $DistRoot "VidView"
    $ZipPath = Join-Path $DistRoot "VidView-Windows-x64.zip"

    if (Test-Path $PackageDir) {
        Remove-Item $PackageDir -Recurse -Force
    }

    if (Test-Path $ZipPath) {
        Remove-Item $ZipPath -Force
    }

    New-Item -ItemType Directory -Path $PackageDir -Force | Out-Null

    $PackageExe = Join-Path $PackageDir "VidView.exe"

    Copy-Item $SourceExe $PackageExe -Force

    Write-Host ""
    Write-Host "=== Deploying Qt dependencies ==="

    & $WinDeployQt `
        --release `
        --no-translations `
        --compiler-runtime `
        --dir $PackageDir `
        $PackageExe

    if ($LASTEXITCODE -ne 0) {
        throw "windeployqt failed."
    }

    Write-Host ""
    Write-Host "=== Resolving FFmpeg / MSYS2 dependencies ==="

    $CopiedDependencies =
        [System.Collections.Generic.HashSet[string]]::new(
            [System.StringComparer]::OrdinalIgnoreCase
        )

    function Convert-NtLddPath {
        param(
            [string]$DependencyPath
        )

        $DependencyPath = $DependencyPath.Trim()

        if ($DependencyPath.StartsWith("/ucrt64/")) {
            $relative = $DependencyPath.Substring("/ucrt64/".Length)
            $relative = $relative.Replace("/", "\")

            return Join-Path (Join-Path $MsysRoot "ucrt64") $relative
        }

        if ($DependencyPath.StartsWith("/")) {
            $relative = $DependencyPath.TrimStart("/").Replace("/", "\")

            return Join-Path $MsysRoot $relative
        }

        return $DependencyPath
    }

    #
    # Run several dependency passes because newly copied DLLs
    # may themselves depend on additional MSYS2 DLLs.
    #
    for ($pass = 1; $pass -le 8; $pass++) {
        Write-Host "Dependency pass $pass..."

        $copiedThisPass = 0

        $binaries = Get-ChildItem `
            -Path $PackageDir `
            -Recurse `
            -File |
            Where-Object {
                $_.Extension -ieq ".exe" -or
                $_.Extension -ieq ".dll"
            }

        foreach ($binary in $binaries) {
            $dependencyLines = & $NtLdd -R $binary.FullName 2>$null

            foreach ($line in $dependencyLines) {
                $dependencyPath = $null

                if ($line -match '=>\s+(.+?)\s+\(0x[0-9A-Fa-f]+\)') {
                    $dependencyPath = $Matches[1]
                }
                elseif ($line -match '^\s*(/[^\s]+\.dll)\s+\(0x[0-9A-Fa-f]+\)') {
                    $dependencyPath = $Matches[1]
                }

                if (-not $dependencyPath) {
                    continue
                }

                $dependencyPath = Convert-NtLddPath $dependencyPath

                if (-not (Test-Path -LiteralPath $dependencyPath)) {
                    continue
                }

                $fullPath = (Resolve-Path -LiteralPath $dependencyPath).Path

                #
                # Only bundle DLLs from our MSYS2 installation.
                # Windows system DLLs are intentionally ignored.
                #
                if (
                    -not $fullPath.StartsWith(
                        $MsysRoot,
                        [System.StringComparison]::OrdinalIgnoreCase
                    )
                ) {
                    continue
                }

                if ($CopiedDependencies.Add($fullPath)) {
                    $destination = Join-Path $PackageDir (Split-Path $fullPath -Leaf)

                    Copy-Item `
                        -LiteralPath $fullPath `
                        -Destination $destination `
                        -Force

                    $copiedThisPass++
                }
            }
        }

        if ($copiedThisPass -eq 0) {
            break
        }
    }

    Write-Host ""
    Write-Host "Copied $($CopiedDependencies.Count) MSYS2 runtime dependencies."

    Write-Host ""
    Write-Host "=== Running isolated smoke test ==="

    #
    # Temporarily remove MSYS2 from PATH.
    #
    # This is important: otherwise the packaged EXE could silently
    # load missing DLLs from C:\msys64\ucrt64\bin and give us a
    # false-positive packaging test.
    #
    $OldPath = $env:PATH

    try {
        $WindowsRoot = $env:SystemRoot

        $env:PATH = (
            (Join-Path $WindowsRoot "System32") +
            ";" +
            $WindowsRoot
        )

        & $PackageExe --smoke-test

        if ($LASTEXITCODE -ne 0) {
            throw "Packaged VidView smoke test failed with exit code $LASTEXITCODE."
        }
    }
    finally {
        $env:PATH = $OldPath
    }

    Write-Host ""
    Write-Host "=== Creating ZIP ==="

    Compress-Archive `
        -Path $PackageDir `
        -DestinationPath $ZipPath `
        -CompressionLevel Optimal

    Write-Host ""
    Write-Host "====================================="
    Write-Host " VidView package created successfully"
    Write-Host "====================================="
    Write-Host ""
    Write-Host "Folder:"
    Write-Host $PackageDir
    Write-Host ""
    Write-Host "ZIP:"
    Write-Host $ZipPath
    Write-Host ""
}
finally {
    Pop-Location
}