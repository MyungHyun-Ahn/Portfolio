pipeline {
    agent { label 'windows-build-server-01' }
    stages {
        stage('git pull') {
            steps {
                dir('C:\\MyServer\\Portfolio\\') {
                    bat 'git pull --ff-only'
                }
            }
        }
        stage('Build') {
            steps {
                dir('C:\\MyServer\\Portfolio\\13_MMO_Project\\Tests\\Chatting') {
                    powershell '''
$vswhere = "${env:ProgramFiles(x86)}\\Microsoft Visual Studio\\Installer\\vswhere.exe"
$msbuild = & $vswhere -latest -products * `
    -requires Microsoft.Component.MSBuild `
    -find "MSBuild\\**\\Bin\\MSBuild.exe" | Select-Object -First 1
if (-not $msbuild) { throw "MSBuild not found" }
& $msbuild "Chatting.sln" `
    /m `
    /p:Configuration=Release `
    /p:Platform=x64
if ($LASTEXITCODE -ne 0) { throw "MSBuild failed: $LASTEXITCODE" }
'''
                }
            }
        }
    }
    post {
        success {
            slackSend(
                channel: '#jenkins-build-alert',
                color: 'good',
                message: """
                    *BUILD SUCCESS*
                    Job: ${env.JOB_NAME}
                    Build: #${env.BUILD_NUMBER}
                    URL: ${env.BUILD_URL}
                """.stripIndent()
            )
        }
        failure {
            slackSend(
                channel: '#jenkins-build-alert',
                color: 'danger',
                message: """
                    *BUILD FAILED*
                    Job: ${env.JOB_NAME}
                    Build: #${env.BUILD_NUMBER}
                    URL: ${env.BUILD_URL}
                """.stripIndent()
            )
        }
    }
}
