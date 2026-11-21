def notifySlack(String status, String color) {
    slackSend channel: '#jenkins-build-alert', color: color, failOnError: false,
        message: "*${status}*\nJob: ${env.JOB_NAME}\nBuild: #${env.BUILD_NUMBER}\nURL: ${env.BUILD_URL}"
}

def configured = params.containsKey('LOG_QUEUE_CAPACITY')
properties([disableConcurrentBuilds(), parameters([
    string(name: 'PORT', defaultValue: '6000', description: '서버 포트 (1~65535). 더미 잡과 같은 값.', trim: true),
    string(name: 'LOG_POLL_INTERVAL_MS', defaultValue: '10', description: '로그 스레드 확인 주기 (1~1000 ms).', trim: true),
    string(name: 'MAX_SESSION_COUNT', defaultValue: '256', description: '최대 동시 세션 슬롯 수 (1~65535).', trim: true),
    string(name: 'ACCEPTEX_COUNT', defaultValue: '4', description: '미리 걸어둘 접속 수락 요청 수 (1~65535).', trim: true),
    string(name: 'IOCP_WORKER_COUNT', defaultValue: '2', description: '네트워크 IOCP 워커 수 (1~64).', trim: true),
    string(name: 'IOCP_CONCURRENCY', defaultValue: '2', description: 'IOCP 완료 처리 동시성 (1~IOCP_WORKER_COUNT).', trim: true),
    string(name: 'CONTENTS_WORKER_COUNT', defaultValue: '2', description: '콘텐츠 워커 수 (1~64).', trim: true),
    string(name: 'LOG_QUEUE_CAPACITY', defaultValue: '1024', description: '로그 생산자 스레드당 큐 용량 (1~1048576건).', trim: true)
])])
if (!configured) {
    echo 'Parameters registered. No executable was started.'
    return
}
try {
    timeout(time: 2, unit: 'MINUTES') {
        node('windows-build-server-01') {
            stage('Start Release ChatServer') {
                powershell '''
$ErrorActionPreference = 'Stop'
function Read-Number($name, [long]$min, [long]$max) {
    $raw = [Environment]::GetEnvironmentVariable($name)
    $value = 0L
    if ($raw -cnotmatch '^[0-9]+$' -or -not [long]::TryParse($raw, [ref]$value) -or $value -lt $min -or $value -gt $max) {
        throw "$name must be an integer between $min and $max."
    }
    return $value
}
function Set-ConfigNumber([string]$text, [string]$name, [long]$value) {
    $pattern = '(?m)^([ \t]*-[ \t]*' + $name + '[ \t]*=[ \t]*)[^;]+(?=;)'
    if ([regex]::Matches($text, $pattern).Count -ne 1) { throw "Expected exactly one $name config entry." }
    return [regex]::Replace($text, $pattern, ('${1}' + $value))
}
$port = Read-Number PORT 1 65535
$poll = Read-Number LOG_POLL_INTERVAL_MS 1 1000
$maxSessions = Read-Number MAX_SESSION_COUNT 1 65535
$acceptEx = Read-Number ACCEPTEX_COUNT 1 65535
$iocpWorkers = Read-Number IOCP_WORKER_COUNT 1 64
$iocpConcurrency = Read-Number IOCP_CONCURRENCY 1 $iocpWorkers
$contentsWorkers = Read-Number CONTENTS_WORKER_COUNT 1 64
$logQueueCapacity = Read-Number LOG_QUEUE_CAPACITY 1 1048576
$source = 'C:/MyServer/Portfolio/13_MMO_Project/Build/Release/ChatServer'
$run = Join-Path $env:WORKSPACE ('server-' + $env:BUILD_NUMBER)
if (Get-NetTCPConnection -State Listen -LocalPort $port -ErrorAction SilentlyContinue) { throw "Port $port already has a listener. No process was stopped." }
New-Item -Path $run -ItemType Directory | Out-Null
Copy-Item -LiteralPath (Join-Path $source 'ChatServer.exe'), (Join-Path $source 'ServerConfig.conf') -Destination $run
$configPath = Join-Path $run 'ServerConfig.conf'
$config = [IO.File]::ReadAllText($configPath, [Text.Encoding]::Unicode)
if ($config -notmatch '(?m)^[ \t]*-[ \t]*IP[ \t]*=[ \t]*127[.]0[.]0[.]1[ \t]*;') { throw 'Expected 127.0.0.1 in ServerConfig.conf.' }
$config = Set-ConfigNumber $config PORT $port
$config = Set-ConfigNumber $config LOG_POLL_INTERVAL_MS $poll
$config = Set-ConfigNumber $config MAX_SESSION_COUNT $maxSessions
$config = Set-ConfigNumber $config ACCEPTEX_COUNT $acceptEx
$config = Set-ConfigNumber $config IOCP_WORKER_COUNT $iocpWorkers
$config = Set-ConfigNumber $config IOCP_CONCURRENCY $iocpConcurrency
$config = Set-ConfigNumber $config CONTENTS_WORKER_COUNT $contentsWorkers
$config = Set-ConfigNumber $config LOG_QUEUE_CAPACITY $logQueueCapacity
[IO.File]::WriteAllText($configPath, $config, [Text.Encoding]::Unicode)
$supervisor = Join-Path $run 'run-server.ps1'
@'
$ErrorActionPreference = 'Stop'
$server = New-Object System.Diagnostics.Process
$server.StartInfo.FileName = Join-Path $PSScriptRoot 'ChatServer.exe'
$server.StartInfo.WorkingDirectory = $PSScriptRoot
$server.StartInfo.UseShellExecute = $false
$server.StartInfo.CreateNoWindow = $true
$server.StartInfo.RedirectStandardInput = $true
if (-not $server.Start()) { throw 'ChatServer could not start.' }
try {
    Set-Content -LiteralPath (Join-Path $PSScriptRoot 'server.pid') -Value $server.Id
    $stopFile = Join-Path $PSScriptRoot 'stop.request'
    while (-not $server.HasExited -and -not (Test-Path -LiteralPath $stopFile)) { Start-Sleep -Milliseconds 250 }
} finally {
    if (-not $server.HasExited) {
        try { $server.StandardInput.WriteLine('/exit'); $server.StandardInput.Flush() } catch { Write-Warning $_ }
        if (-not $server.WaitForExit(5000)) { $server.Kill(); $server.WaitForExit() }
    }
    Write-Output ('ChatServer exit code: ' + $server.ExitCode)
    $server.Dispose()
}
'@ | Set-Content -LiteralPath $supervisor -Encoding UTF8

# Start through local WMI so the supervisor does not inherit Jenkins' handles.
$ps = Join-Path $PSHOME 'powershell.exe'
$cmd = '"{0}" /d /s /c ""{1}" -NoLogo -NoProfile -NonInteractive -ExecutionPolicy RemoteSigned -File run-server.ps1 > stdout.log 2> stderr.log < NUL"' -f $env:ComSpec, $ps
$started = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{CommandLine=$cmd; CurrentDirectory=$run}
if ($started.ReturnValue -ne 0) { throw ('Win32_Process.Create returned ' + $started.ReturnValue) }
$pidFile = Join-Path $run 'server.pid'
$clock = [System.Diagnostics.Stopwatch]::StartNew()
try {
    do {
        if (-not (Get-Process -Id $started.ProcessId -ErrorAction SilentlyContinue)) { throw ('ChatServer launcher exited; inspect ' + $run) }
        if (Test-Path -LiteralPath $pidFile) {
            $serverPid = 0
            if ([int]::TryParse((Get-Content -LiteralPath $pidFile -Raw), [ref]$serverPid) -and (Get-NetTCPConnection -State Listen -LocalAddress 127.0.0.1 -LocalPort $port -OwningProcess $serverPid -ErrorAction SilentlyContinue)) {
                Write-Output ('ChatServer listening on 127.0.0.1:' + $port + '; PID=' + $serverPid + '; stop with Stop_ChatServer; logs: ' + $run)
                return
            }
        }
        Start-Sleep -Milliseconds 300
    } while ($clock.Elapsed.TotalSeconds -lt 20)
    throw ('ChatServer did not listen within 20 seconds; inspect ' + $run)
} catch {
    New-Item -Path (Join-Path $run 'stop.request') -ItemType File -Force | Out-Null
    $stderr = Join-Path $run 'stderr.log'
    if (Test-Path -LiteralPath $stderr) { Get-Content -LiteralPath $stderr -Tail 10 | Write-Output }
    throw
}
'''
            }
        }
    }
} catch (org.jenkinsci.plugins.workflow.steps.FlowInterruptedException interrupted) {
    throw interrupted
} catch (Exception failure) {
    notifySlack('SERVER START FAILED', 'danger')
    throw failure
}
notifySlack('SERVER START SUCCESS', 'good')
