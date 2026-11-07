# O teste automático da LAN (o roteiro lanhost do RTSAutoTest) com o menu nativo, numa vez da fila já segurada (06/11 à noite):
# o mesmo que "game.ps1 view -Folder telas_lan_auto -Script lanhost -Players 2", mas abrindo o anfitrião e o robô sem placa juntos,
# dentro da vez (o game.ps1 solta os robôs antes de entrar na fila, e eles prendem a DLL dos outros). Roda uns minutos (os passos do
# menu e o começo da partida são o que muda com o menu nativo) e fecha os dois pelo PID. Os logs em captures/game/telas_lan_auto*.log.
param([int]$Seconds = 200)
$Q = 'C:\Users\luisf\.gpu_queue\gpu_queue.ps1'
$Owner = if ($env:RTS_QUEUE_OWNER) { $env:RTS_QUEUE_OWNER } else { 'Telas | teste automatico da LAN' }
$Captures = 'C:\Users\luisf\explosion\captures\game'
$Folder = 'telas_lan_auto'
$Dest = Join-Path $Captures $Folder
$Log = Join-Path $Captures "$Folder.log"
$BotLog = Join-Path $Captures "$Folder.bot0.log"
$Gate = Join-Path $Captures "$Folder.lan_gate"
if (Test-Path $Dest) { Remove-Item $Dest -Recurse -Force }
New-Item -ItemType Directory -Force $Dest | Out-Null
Remove-Item $Log, $BotLog, $Gate -ErrorAction SilentlyContinue
$UE = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$Proj = '"C:\Users\luisf\explosion\Game\RTSGame.uproject"'
if (-not $env:RTS_QUEUE_OWNER) { do { powershell -NoProfile -File $Q enter -Owner $Owner -Tag 'RTSGame.uproject' | Out-Null } while ($LASTEXITCODE -ne 0) }
$h = $null; $b = $null
try {
  # (07/11) o robô manda a fala dele com 5 s de partida (-RTSTestChat=, o _ vira espaço): o passo chat_receber do roteiro espera por ela
  $b = Start-Process $UE -ArgumentList @($Proj, '-game', '-nullrhi', '-nosound', '-RTSNetBot', "-abslog=`"$BotLog`"", '-unattended', '-RTSNetJoin=127.0.0.1', "-RTSNetJoinGate=`"$Gate`"",
    '-RTSTestChat=roger,_moving_out') -PassThru -WindowStyle Minimized
  $h = Start-Process $UE -ArgumentList @($Proj, '-game', '-ExecCmds="Scalability 1, rts.Record.Fast 1"', '-nosplash', '-windowed', '-ResX=1600', '-ResY=900', '-RenderOffScreen',
    '-RTSAutoTest=lanhost', "-RTSCapture=`"$Dest`"", "-abslog=`"$Log`"", '-unattended', '-RTSNetPlayers=2', "-RTSNetGate=`"$Gate`"", '-RTSMuteOutput') -PassThru
  "anfitrião $($h.Id), robô $($b.Id)"
  $t0 = Get-Date
  while (((Get-Date) - $t0).TotalSeconds -lt $Seconds -and -not $h.HasExited) {
    Start-Sleep 5
    if (-not $env:RTS_QUEUE_OWNER) { powershell -NoProfile -File $Q touch -Owner $Owner | Out-Null }
  }
}
finally {
  foreach ($p in @($h, $b)) { if ($p -and -not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue } }
  if (-not $env:RTS_QUEUE_OWNER) { powershell -NoProfile -File $Q leave -Owner $Owner | Out-Null }
}
'--- o teste automático da LAN no menu nativo'
Get-Content $Log -ErrorAction SilentlyContinue | Select-String 'RTS TEST|pela função do botão|RTS telas: (tela da partida|multiplayer|skirmish\.html)|DESSINCRONIA|IsInGameThread|Critical error' | Select-Object -Last 40 | ForEach-Object { $_.Line.Substring(0, [Math]::Min(220, $_.Line.Length)) }
