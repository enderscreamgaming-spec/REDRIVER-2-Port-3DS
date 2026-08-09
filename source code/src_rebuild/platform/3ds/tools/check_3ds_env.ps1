$ErrorActionPreference = 'Continue'

$requiredCommands = @(
    'make',
    'arm-none-eabi-g++',
    '3dsxtool',
    'smdhtool'
)

$optionalCommands = @(
    'makerom'
)

$ok = $true

Write-Host 'REDRIVER2 3DS build environment check'
Write-Host ''

foreach ($name in @('DEVKITPRO', 'DEVKITARM')) {
    $value = [Environment]::GetEnvironmentVariable($name)
    if (-not $value) {
        Write-Host "missing env: $name"
        $ok = $false
        continue
    }

    if (-not (Test-Path -LiteralPath $value)) {
        Write-Host "bad env: $name=$value (path not found)"
        $ok = $false
        continue
    }

    Write-Host "ok env: $name=$value"
}

foreach ($command in $requiredCommands) {
    $found = Get-Command $command -ErrorAction SilentlyContinue
    if (-not $found) {
        Write-Host "missing tool: $command"
        $ok = $false
        continue
    }

    Write-Host "ok tool: $command -> $($found.Source)"
}

foreach ($command in $optionalCommands) {
    $found = Get-Command $command -ErrorAction SilentlyContinue
    if ($found) {
        Write-Host "ok optional tool: $command -> $($found.Source)"
    }
    else {
        Write-Host "optional tool missing: $command (needed only for make cia)"
    }
}

$devkitPro = [Environment]::GetEnvironmentVariable('DEVKITPRO')
if ($devkitPro -and (Test-Path -LiteralPath $devkitPro)) {
    $ctru = Join-Path $devkitPro 'libctru\include\3ds.h'
    if (Test-Path -LiteralPath $ctru) {
        Write-Host "ok libctru: $ctru"
    }
    else {
        Write-Host "missing libctru header: $ctru"
        $ok = $false
    }
}

Write-Host ''
if ($ok) {
    Write-Host 'Environment looks ready for make.'
    exit 0
}

Write-Host 'Environment is not ready. Install devkitPro with 3DS development support, then reopen this shell.'
exit 1
