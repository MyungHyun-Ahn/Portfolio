properties([disableConcurrentBuilds()])
timeout(time: 6, unit: 'MINUTES') {
    node('windows-build-server-01') {
        stage('Clean up leftover Jenkins DummyClient') {
            powershell '''
$ErrorActionPreference = 'Stop'
$root = Join-Path (Split-Path -Parent $env:WORKSPACE) 'Run_ChatDummy'
$targets = @(Get-Process -Name DummyClient -ErrorAction SilentlyContinue | Where-Object {
    if ($_.Path) {
        $folder = Split-Path -Parent $_.Path
        (Split-Path -Parent $folder) -eq $root -and (Split-Path -Leaf $folder) -match '^dummy-[0-9]+$'
    }
})
if ($targets.Count -eq 0) { Write-Output 'No DummyClient in the Jenkins run folders.'; return }
foreach ($process in $targets) {
    try {
        if ($process.HasExited) { continue }
        $process.Kill()
        if (-not $process.WaitForExit(5000)) { throw ('DummyClient did not stop: ' + $process.Id) }
        Write-Output ('Stopped DummyClient PID ' + $process.Id)
    } finally {
        $process.Dispose()
    }
}
'''
        }
    }
}
