param(
    [Parameter(Mandatory = $true)]
    [string]$SdRoot,

    [string]$DataSource,

    [string]$SupplementalSource,

    [switch]$AllowIncomplete,

    [switch]$SkipBinary
)

$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$platformDir = (Resolve-Path -LiteralPath (Join-Path $scriptDir '..')).Path
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $platformDir '..\..\..')).Path
$packageCache = Join-Path $platformDir '.package-cache'

function Resolve-Driver2Source {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Source,

        [Parameter(Mandatory = $true)]
        [string]$Label
    )

    $resolved = (Resolve-Path -LiteralPath $Source).Path

    if (Test-Path -LiteralPath $resolved -PathType Leaf) {
        if ([System.IO.Path]::GetExtension($resolved).ToLowerInvariant() -ne '.zip') {
            throw "$Label must be a DRIVER2 folder, a project folder containing data\DRIVER2, or a .zip archive."
        }

        Add-Type -AssemblyName System.IO.Compression.FileSystem
        $safeName = [System.IO.Path]::GetFileNameWithoutExtension($resolved) -replace '[^A-Za-z0-9_.-]', '_'
        $safeName = "$safeName-$PID"
        $extractDir = Join-Path $packageCache $safeName
        New-Item -ItemType Directory -Force -Path $extractDir | Out-Null
        $extractRoot = [System.IO.Path]::GetFullPath($extractDir)
        $extractRootWithSeparator = $extractRoot.TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar) + [System.IO.Path]::DirectorySeparatorChar
        $zip = [System.IO.Compression.ZipFile]::OpenRead($resolved)
        try {
            foreach ($entry in $zip.Entries) {
                $destination = [System.IO.Path]::GetFullPath((Join-Path $extractDir $entry.FullName))
                if ($destination -ne $extractRoot -and -not $destination.StartsWith($extractRootWithSeparator, [System.StringComparison]::OrdinalIgnoreCase)) {
                    throw "Blocked unsafe zip entry path: $($entry.FullName)"
                }

                if ([string]::IsNullOrEmpty($entry.Name)) {
                    New-Item -ItemType Directory -Force -Path $destination | Out-Null
                    continue
                }

                $parent = Split-Path -Parent $destination
                New-Item -ItemType Directory -Force -Path $parent | Out-Null

                $inputStream = $entry.Open()
                try {
                    $outputStream = [System.IO.File]::Open($destination, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write)
                    try {
                        $inputStream.CopyTo($outputStream)
                    }
                    finally {
                        $outputStream.Dispose()
                    }
                }
                finally {
                    $inputStream.Dispose()
                }
            }
        }
        finally {
            $zip.Dispose()
        }

        $driver2Dirs = @(Get-ChildItem -LiteralPath $extractDir -Recurse -Directory -Filter 'DRIVER2' |
            Where-Object { (Split-Path -Leaf (Split-Path -Parent $_.FullName)) -ieq 'data' } |
            Sort-Object FullName)

        if ($driver2Dirs.Count -eq 0) {
            throw "Could not find data\DRIVER2 inside $Label zip: $resolved"
        }

        return $driver2Dirs[0].FullName
    }

    $nestedDriver2 = Join-Path $resolved 'data\DRIVER2'
    if (Test-Path -LiteralPath $nestedDriver2 -PathType Container) {
        return (Resolve-Path -LiteralPath $nestedDriver2).Path
    }

    return $resolved
}

if (-not $DataSource) {
    $DataSource = Join-Path $repoRoot 'data\DRIVER2'
}

$dataSource = Resolve-Driver2Source -Source $DataSource -Label 'DataSource'

if (-not $SupplementalSource) {
    $SupplementalSource = Join-Path $repoRoot 'data\DRIVER2'
}

$supplementalSource = Resolve-Driver2Source -Source $SupplementalSource -Label 'SupplementalSource'

$requiredFiles = @(
    'DATA\FEFONT.BNK',
    'GFX\FONT2.FNT',
    'GFX\LOADCHIC.TIM',
    'GFX\LOADHAVA.TIM',
    'GFX\LOADVEGA.TIM',
    'GFX\LOADRIO.TIM',
    'LANG\EN_GAME.LTXT',
    'LANG\EN_MISSION.LTXT',
    'LEVELS\CHICAGO.LEV',
    'LEVELS\HAVANA.LEV',
    'LEVELS\VEGAS.LEV',
    'LEVELS\RIO.LEV'
)

function Test-DataFile {
    param([string]$RelativePath)

    return (Test-Path -LiteralPath (Join-Path $dataSource $RelativePath)) -or
        (Test-Path -LiteralPath (Join-Path $supplementalSource $RelativePath))
}

$missing = @(
    foreach ($file in $requiredFiles) {
        if (-not (Test-DataFile $file)) {
            $file
        }
    }
)

if (-not ((Test-DataFile 'GFX\SPLASH1N.TIM') -or (Test-DataFile 'GFX\SPLASH1P.TIM'))) {
    $missing += 'GFX\SPLASH1N.TIM or GFX\SPLASH1P.TIM'
}

if ($missing.Count -gt 0) {
    Write-Host 'The selected DRIVER2 data folder is incomplete:'
    foreach ($file in $missing) {
        Write-Host "  missing $file"
    }

    if (-not $AllowIncomplete) {
        throw 'Refusing to package incomplete game data. Pass -AllowIncomplete only for boot/debug tests.'
    }
}

New-Item -ItemType Directory -Force -Path $SdRoot | Out-Null
$sdRootPath = (Resolve-Path -LiteralPath $SdRoot).Path

$targetDir = Join-Path $sdRootPath '3ds\redriver2'
$targetData = Join-Path $targetDir 'DRIVER2'
New-Item -ItemType Directory -Force -Path $targetData | Out-Null

Copy-Item -Path (Join-Path $dataSource '*') -Destination $targetData -Recurse -Force

if ($supplementalSource -ne $dataSource) {
    Copy-Item -Path (Join-Path $supplementalSource '*') -Destination $targetData -Recurse -Force
}

if (-not $SkipBinary) {
    $binary = Join-Path $platformDir 'redriver2_3ds.3dsx'
    if (Test-Path -LiteralPath $binary) {
        Copy-Item -LiteralPath $binary -Destination (Join-Path $targetDir 'redriver2_3ds.3dsx') -Force
    }
    else {
        Write-Warning "Built binary not found: $binary"
    }
}

Write-Host "Packaged SD layout at $targetDir"
