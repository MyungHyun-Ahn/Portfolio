def notifySlack(String status, String color) {
    slackSend channel: '#jenkins-build-alert', color: color, failOnError: false,
        message: "*${status}*\nJob: ${env.JOB_NAME}\nBuild: #${env.BUILD_NUMBER}\nURL: ${env.BUILD_URL}"
}

def configured = params.containsKey('PORT')
properties([disableConcurrentBuilds(), parameters([
    string(name: 'PORT', defaultValue: '6000', description: '서버 잡과 같은 포트 (1~65535).', trim: true),
    string(name: 'CLIENT_COUNT', defaultValue: '2', description: '더미 클라이언트 수.', trim: true),
    string(name: 'ACCOUNT_BASE', defaultValue: '710001', description: '첫 번째 더미의 계정 번호.', trim: true),
    string(name: 'RUN_DURATION_MS', defaultValue: '60000', description: '모든 더미가 Ready가 된 뒤 실행 시간 (ms). Jenkins 별도 상한 없음; DummyClient 범위: 1~4294967295.', trim: true),
    string(name: 'WORKER_COUNT', defaultValue: '1', description: 'IOCP 워커 수 (1~16).', trim: true),
    string(name: 'APP_WORKER_COUNT', defaultValue: '1', description: '앱 워커 수 (1~16). 실제 수는 더미 수 이하.', trim: true),
    string(name: 'ACTION_INTERVAL_MS', defaultValue: '1000', description: '더미별 행동 간격 (ms).', trim: true),
    string(name: 'SECTOR_MOVE_WEIGHT', defaultValue: '1', description: '섹터 이동 가중치 (0이면 꺼짐).', trim: true),
    string(name: 'MESSAGE_WEIGHT', defaultValue: '1', description: '채팅 가중치 (0이면 꺼짐).', trim: true),
    string(name: 'RECONNECT_PROBABILITY_BPS', defaultValue: '0', description: '행동 성공 후 재접속 확률 (0~10000, 10000=100%).', trim: true)
])])
if (!configured) {
    echo 'Parameters registered. No executable was started.'
    return
}
try {
    def durationText = params.RUN_DURATION_MS
    if (!(durationText ==~ /[0-9]+/) || !durationText.isLong()) {
        error('RUN_DURATION_MS must be an integer between 1 and 4294967295.')
    }
    long durationMs = durationText.toLong()
    if (durationMs < 1L || durationMs > 4294967295L) {
        error('RUN_DURATION_MS must be an integer between 1 and 4294967295.')
    }
    int timeoutSeconds = (int)((durationMs + 999L).intdiv(1000L) + 300L)
    node('windows-build-server-01') {
        timeout(time: timeoutSeconds, unit: 'SECONDS') {
            try {
                stage('Run Release DummyClient') {
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
$values = [ordered]@{
    PORT = (Read-Number PORT 1 65535)
    CLIENT_COUNT = (Read-Number CLIENT_COUNT 1 ([long]::MaxValue))
    ACCOUNT_BASE = (Read-Number ACCOUNT_BASE 1 ([long]::MaxValue))
    RUN_DURATION_MS = (Read-Number RUN_DURATION_MS 1 ([uint32]::MaxValue))
    WORKER_COUNT = (Read-Number WORKER_COUNT 1 16)
    APP_WORKER_COUNT = (Read-Number APP_WORKER_COUNT 1 16)
    ACTION_INTERVAL_MS = (Read-Number ACTION_INTERVAL_MS 1 4294967295L)
    SECTOR_MOVE_WEIGHT = (Read-Number SECTOR_MOVE_WEIGHT 0 4294967295L)
    MESSAGE_WEIGHT = (Read-Number MESSAGE_WEIGHT 0 4294967295L)
    RECONNECT_PROBABILITY_BPS = (Read-Number RECONNECT_PROBABILITY_BPS 0 10000)
}
if ($values.ACCOUNT_BASE -gt ([long]::MaxValue - ($values.CLIENT_COUNT - 1))) { throw 'The client account range exceeds Int64.' }
if ($values.SECTOR_MOVE_WEIGHT -eq 0 -and $values.MESSAGE_WEIGHT -eq 0) { throw 'At least one action weight must be positive.' }
$port = $values.PORT
$listeners = @(Get-NetTCPConnection -State Listen -LocalAddress 127.0.0.1 -LocalPort $port -ErrorAction SilentlyContinue)
$expected = @($listeners | Where-Object { (Get-Process -Id $_.OwningProcess -ErrorAction SilentlyContinue).ProcessName -eq 'ChatServer' })
if ($expected.Count -eq 0) { throw "ChatServer is not listening on 127.0.0.1:$port. Run Run_ChatServer with the same port first." }
$source = 'C:/MyServer/Portfolio/13_MMO_Project/Build/Release/DummyClient'
$run = Join-Path $env:WORKSPACE ('dummy-' + $env:BUILD_NUMBER)
New-Item -Path $run -ItemType Directory | Out-Null
Copy-Item -LiteralPath (Join-Path $source 'DummyClient.exe'), (Join-Path $source 'DummyClientConfig.conf') -Destination $run
$configPath = Join-Path $run 'DummyClientConfig.conf'
$config = [IO.File]::ReadAllText($configPath, [Text.Encoding]::Unicode)
if ($config -notmatch '(?m)^[ \t]*-[ \t]*IP[ \t]*=[ \t]*127[.]0[.]0[.]1[ \t]*;') { throw 'Expected 127.0.0.1 in DummyClientConfig.conf.' }
foreach ($name in $values.Keys) { $config = Set-ConfigNumber $config $name $values[$name] }
[IO.File]::WriteAllText($configPath, $config, [Text.Encoding]::Unicode)
Push-Location -LiteralPath $run
try {
    & (Join-Path $run 'DummyClient.exe')
    $code = $LASTEXITCODE
} finally {
    Pop-Location
}
exit $code
'''
                }
            } finally {
                archiveArtifacts artifacts: "dummy-${env.BUILD_NUMBER}/**/RunSummary.json", allowEmptyArchive: true
            }
        }
    }
} catch (org.jenkinsci.plugins.workflow.steps.FlowInterruptedException interrupted) {
    throw interrupted
} catch (Exception failure) {
    notifySlack('DUMMY RUN FAILED', 'danger')
    throw failure
}
notifySlack('DUMMY RUN SUCCESS', 'good')
