[CmdletBinding()]
param(
    [switch]$Program,
    [switch]$ProbeOnly,
    [string]$JLinkExe = 'D:\Keil5\Core\ARM\Segger\JLink.exe',
    [string]$HexFile = '',
    [string]$Device = 'MSPM0G3507',
    [ValidateSet('SWD')]
    [string]$Interface = 'SWD',
    [ValidateRange(100, 15000)]
    [int]$SpeedKHz = 4000
)

$ErrorActionPreference = 'Stop'

function Resolve-ExistingFile([string]$Path, [string]$Description) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Description 不存在：$Path"
    }
    return (Resolve-Path -LiteralPath $Path).Path
}

if (-not $Program -and -not $ProbeOnly) {
    Write-Host '安全模式：未执行下载。请使用 -ProbeOnly 检查调试器，或使用 -Program 写入固件。' -ForegroundColor Yellow
    exit 2
}

$jlink = Resolve-ExistingFile $JLinkExe 'J-Link Commander'
if (-not $ProbeOnly) {
    if ([string]::IsNullOrWhiteSpace($HexFile)) {
        $HexFile = Join-Path $PSScriptRoot 'build\MSPM0G3507_FreeRTOS.hex'
    }
    $hex = Resolve-ExistingFile $HexFile 'GCC HEX 固件'
}

$commandFile = Join-Path ([System.IO.Path]::GetTempPath()) ("mspm0-jlink-{0}.jlink" -f ([guid]::NewGuid().ToString('N')))
try {
    if ($ProbeOnly) {
        @(
            'connect',
            'showhwstatus',
            'q'
        ) | Set-Content -LiteralPath $commandFile -Encoding ascii
        Write-Host "J-Link 探测：Device=$Device Interface=$Interface Speed=${SpeedKHz}kHz"
    } else {
        # 只执行复位、下载和校验；不发送 erase 命令，避免误擦除整片 Flash。
        @(
            'connect',
            'r',
            ('loadfile "{0}"' -f $hex),
            ('verify "{0}"' -f $hex),
            'r',
            'g',
            'q'
        ) | Set-Content -LiteralPath $commandFile -Encoding ascii
        Write-Host "J-Link 下载：$hex"
    }

    $jlinkOutput = (& $jlink -NoGui 1 -Device $Device -If $Interface -Speed $SpeedKHz -AutoConnect 1 -CommanderScript $commandFile 2>&1 | Out-String)
    $jlinkExit = $LASTEXITCODE
    Write-Host $jlinkOutput
    if ($jlinkExit -ne 0 -or $jlinkOutput -match '(?i)(FAILED|Cannot connect|No J-Link|cannot connect)') {
        throw "J-Link 未连接或目标不可访问。请检查 J-Link 是否连接、目标板供电和 SWD 接线。退出码=$jlinkExit"
    }
    Write-Host 'J-Link 操作完成。'
}
finally {
    Remove-Item -LiteralPath $commandFile -Force -ErrorAction SilentlyContinue
}
