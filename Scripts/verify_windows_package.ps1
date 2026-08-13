[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$PackageRoot,

    [string]$ClientExecutable,

    [string]$ServerExecutable,

    [switch]$RequireServer,

    [switch]$SkipLaunch,

    [int]$ServerStartupSeconds = 20
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Resolve-PackagePath([string]$PathValue) {
    if ([System.IO.Path]::IsPathRooted($PathValue)) {
        return (Resolve-Path -LiteralPath $PathValue).Path
    }
    return (Resolve-Path -LiteralPath (Join-Path $PackageRoot $PathValue)).Path
}

function Find-Executable([string]$ExplicitPath, [string[]]$Names) {
    if ($ExplicitPath) {
        $resolved = Resolve-PackagePath $ExplicitPath
        if (-not (Test-Path -LiteralPath $resolved -PathType Leaf)) {
            throw "Executable not found: $resolved"
        }
        return Get-Item -LiteralPath $resolved
    }

    foreach ($name in $Names) {
        $candidate = Get-ChildItem -LiteralPath $PackageRoot -Filter $name -File -Recurse -ErrorAction SilentlyContinue |
            Sort-Object FullName |
            Select-Object -First 1
        if ($candidate) {
            return $candidate
        }
    }
    return $null
}

$root = (Resolve-Path -LiteralPath $PackageRoot).Path
$reportPath = Join-Path $root "windows-package-verification.txt"
$report = [System.Collections.Generic.List[string]]::new()
$report.Add("Package root: $root")
$report.Add("Verified: $(Get-Date -Format o)")

$client = Find-Executable $ClientExecutable @("NightglassSignalDemo.exe")
if (-not $client) {
    throw "NightglassSignalDemo.exe was not found below $root"
}
$report.Add("Client: $($client.FullName)")

$server = Find-Executable $ServerExecutable @("NightglassSignalDemoServer.exe", "NightglassSignalDemo.exe")
if ($server -and $server.FullName -eq $client.FullName) {
    $server = $null
}
if ($server) {
    $report.Add("Server: $($server.FullName)")
} elseif ($RequireServer) {
    throw "A dedicated server executable was required but was not found below $root"
} else {
    $report.Add("Server: not present (not required for this invocation)")
}

$pakFiles = @(
    Get-ChildItem -LiteralPath $root -File -Recurse -ErrorAction SilentlyContinue |
        Where-Object {
            $_.FullName -match "[\\/]Content[\\/]Paks[\\/]" -and
            $_.Name -match "^(NightglassSignal(-.+)?|global)(\.(pak|utoc|ucas)|-.+\.(pak|utoc|ucas))$"
        }
)
if ($pakFiles.Count -eq 0) {
    throw "No cooked Black Meridian content container was found below a Content\\Paks directory"
}
$report.Add("Content containers: $($pakFiles.Count)")
$pakFiles | ForEach-Object { $report.Add("  $($_.FullName)") }

if (-not $SkipLaunch) {
    $clientLog = Join-Path $root "windows-client-smoke.log"
    $clientArgs = @(
        "-nullrhi",
        "-unattended",
        "-nosplash",
        "-NoSound",
        "-ExecCmds=Quit",
        "-abslog=$clientLog"
    )
    $clientProcess = Start-Process -FilePath $client.FullName -ArgumentList $clientArgs -WorkingDirectory $client.DirectoryName -PassThru -Wait
    if ($clientProcess.ExitCode -ne 0) {
        throw "Packaged client smoke exited with code $($clientProcess.ExitCode); see $clientLog"
    }
    $report.Add("Client smoke: PASS (exit 0)")

    if ($server) {
        $serverLog = Join-Path $root "windows-server-smoke.log"
        $serverArgs = @("-nullrhi", "-unattended", "-NoSound", "-log", "-port=27777", "-abslog=$serverLog")
        $serverProcess = Start-Process -FilePath $server.FullName -ArgumentList $serverArgs -WorkingDirectory $server.DirectoryName -PassThru
        try {
            Start-Sleep -Seconds $ServerStartupSeconds
            if ($serverProcess.HasExited) {
                throw "Packaged server exited with code $($serverProcess.ExitCode); see $serverLog"
            }
            $report.Add("Server startup smoke: PASS (alive after ${ServerStartupSeconds}s)")
        } finally {
            if (-not $serverProcess.HasExited) {
                Stop-Process -Id $serverProcess.Id -Force
                $serverProcess.WaitForExit()
            }
        }
    }
} else {
    $report.Add("Launch smoke: skipped by -SkipLaunch")
}

$report | Set-Content -LiteralPath $reportPath -Encoding UTF8
$report | ForEach-Object { Write-Output $_ }
Write-Output "Verification report: $reportPath"
