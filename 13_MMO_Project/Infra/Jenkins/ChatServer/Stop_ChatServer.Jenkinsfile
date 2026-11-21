properties([disableConcurrentBuilds()])
timeout(time: 5, unit: 'MINUTES') {
    node('windows-build-server-01') {
        stage('Stop Jenkins ChatServer') {
            powershell '''
$ErrorActionPreference = 'Stop'
$root = Join-Path (Split-Path -Parent $env:WORKSPACE) 'Run_ChatServer'
$targets = @(Get-Process -Name ChatServer -ErrorAction SilentlyContinue | Where-Object {
    if ($_.Path) {
        $folder = Split-Path -Parent $_.Path
        (Split-Path -Parent $folder) -eq $root -and (Split-Path -Leaf $folder) -match '^server-[0-9]+$'
    }
})
if ($targets.Count -eq 0) { Write-Output 'No ChatServer in the Jenkins run folders.'; return }
foreach ($process in $targets) {
    try {
        if ($process.HasExited) { continue }
        $folder = Split-Path -Parent $process.Path
        New-Item -Path (Join-Path $folder 'stop.request') -ItemType File -Force | Out-Null
        if (-not $process.WaitForExit(10000)) {
            if (-not $process.HasExited) { $process.Kill() }
            if (-not $process.WaitForExit(5000)) { throw ('ChatServer did not stop: ' + $process.Id) }
        }
        Write-Output ('Stopped ChatServer PID ' + $process.Id)
    } finally {
        $process.Dispose()
    }
}
'''
        }
    }
}
