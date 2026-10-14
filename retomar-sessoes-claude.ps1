param(
    [ValidateSet('explosao', 'cnc', 'mundo')]
    [string]$Sessao = 'explosao'
)

$ErrorActionPreference = 'Stop'
$ids = @{
    explosao = '7fab7fca-b804-45c4-8302-972a750d5b27'
    cnc = '12f1e755-ddf7-4e41-a834-371a3dfa6f7e'
    mundo = '9ef5f6ac-f2c6-4d28-8379-73bed6f11a46'
}
$claude = Join-Path $env:USERPROFILE '.local\bin\claude.exe'
if (-not (Test-Path -LiteralPath $claude)) { throw 'Claude Code nao encontrado.' }
Set-Location -LiteralPath $PSScriptRoot
$statusOutput = & $claude auth status --json
$status = ($statusOutput -join "`n") | ConvertFrom-Json
if (-not $status.loggedIn) {
    Write-Host 'Entre na conta nova do Claude no navegador que sera aberto.'
    & $claude auth login
    if ($LASTEXITCODE -ne 0) { throw 'Login nao concluido. Os historicos nao foram alterados.' }
}
Write-Host 'O Desktop deve estar conectado a conta desejada. Abrindo a sessao preservada...'
& $claude --desktop --resume $ids[$Sessao]
if ($LASTEXITCODE -ne 0) { throw 'O Claude nao confirmou a abertura. Guarde a mensagem acima para diagnostico.' }
