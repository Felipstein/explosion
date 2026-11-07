# A prova do teste automático com o menu nativo (06/11 à noite: o menu antigo do HUD saiu). Abre o jogo com os argumentos do
# "game.ps1 view -Folder <pasta> -Scenario resultado" (o roteiro base, a cena curta: um tanque morre em segundos), mas direto, pra
# rodar dentro de uma vez da fila já segurada (o view entra na fila sozinho e travaria). Confere no log que o teste passou do menu à
# partida pela função dos botões, que a tela de carregar e o resultado foram os nativos e que voltou pelo "menu" do resultado novo.
# Na vez da fila; sai pelo PID. O log em captures/game/telas_auto_menu.log.
param([int]$Seconds = 240, [string]$Scene = 'resultado', [string]$Script = 'base')
$Q = 'C:\Users\luisf\.gpu_queue\gpu_queue.ps1'
$Owner = if ($env:RTS_QUEUE_OWNER) { $env:RTS_QUEUE_OWNER } else { 'Telas | teste automatico no menu nativo' }
$Captures = 'C:\Users\luisf\explosion\captures\game'
$Dest = Join-Path $Captures 'telas_auto_menu'
$Log = Join-Path $Captures 'telas_auto_menu.log'
if (Test-Path $Dest) { Remove-Item $Dest -Recurse -Force }
New-Item -ItemType Directory -Force $Dest | Out-Null
Remove-Item $Log -ErrorAction SilentlyContinue
$UE = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$a = @('"C:\Users\luisf\explosion\Game\RTSGame.uproject"', '-game', '-ExecCmds="Scalability 1, rts.Record.Fast 1"', '-nosplash', '-windowed', '-ResX=1600', '-ResY=900',
  '-RenderOffScreen', "-RTSAutoTest=$Script", "-RTSCapture=$Dest", "-abslog=$Log", '-unattended', "-RTSScenario=$Scene", '-RTSMuteOutput')
if (-not $env:RTS_QUEUE_OWNER) { do { powershell -NoProfile -File $Q enter -Owner $Owner -Tag 'RTSGame.uproject' | Out-Null } while ($LASTEXITCODE -ne 0) }
$p = $null
try {
  $p = Start-Process $UE -ArgumentList $a -PassThru
  "jogo $($p.Id)"
  $t0 = Get-Date
  while (((Get-Date) - $t0).TotalSeconds -lt $Seconds -and -not $p.HasExited) {
    Start-Sleep 5
    if (-not $env:RTS_QUEUE_OWNER) { powershell -NoProfile -File $Q touch -Owner $Owner | Out-Null }
  }
  if (-not $p.HasExited) { 'o teste não terminou no prazo' }
}
finally {
  if ($p -and -not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
  if (-not $env:RTS_QUEUE_OWNER) { powershell -NoProfile -File $Q leave -Owner $Owner | Out-Null }
}
'--- o teste automático no menu nativo'
Get-Content $Log -ErrorAction SilentlyContinue | Select-String 'RTS TEST|pela função do botão|RTS telas: (tela da partida|menu\.html|o menu antigo)|LOADING|IsInGameThread|Critical error|Assertion' | Select-Object -Last 45 | ForEach-Object { $_.Line.Substring(0, [Math]::Min(240, $_.Line.Length)) }
