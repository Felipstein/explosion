# O teste da sala nova da LAN de ponta a ponta, sem clicar (06/11): este PC hospeda com as telas nativas (o HOST sozinho pelo
# -RTSMpTestHost) e segue o roteiro da sala (-RTSRoomTest, fixtures/room_test_host.txt: a IA na vaga 2 no difícil, a facção, a cor,
# o time, a vaga fechada, o mapa trocado com alguém dentro, as bases, o chat, o START com a contagem). Um robô sem placa
# (-RTSNetBot -RTSNetJoin=127.0.0.1, o mesmo do lanhost do game.ps1) entra e aperta o READY sozinho. O resultado sai dos logs:
# a sala a cada mudança ("sala da LAN:"), o mapa que o robô recebeu, as largadas da partida e o carregar do mapa nos dois.
# Na vez da fila (o anfitrião desenha na placa); sai por PID no fim. Os logs em captures/concept/ui/shell/shots/native/lan_*.log.
param([int]$Seconds = 150, [string]$Script = '')
$Q = 'C:\Users\luisf\.gpu_queue\gpu_queue.ps1'
$Owner = if ($env:RTS_QUEUE_OWNER) { $env:RTS_QUEUE_OWNER } else { 'Telas | teste da sala da LAN' }
$T = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $Script) { $Script = Join-Path $T 'fixtures\room_test_host.txt' }
$Out = 'C:\Users\luisf\explosion\captures\concept\ui\shell\shots\native'
New-Item -ItemType Directory -Force $Out | Out-Null
$HostLog = Join-Path $Out 'lan_host.log'; $BotLog = Join-Path $Out 'lan_bot.log'
Remove-Item $HostLog, $BotLog -ErrorAction SilentlyContinue
$UE = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$Proj = '"C:\Users\luisf\explosion\Game\RTSGame.uproject"'
if (-not $env:RTS_QUEUE_OWNER) { do { powershell -NoProfile -File $Q enter -Owner $Owner -Tag 'RTSGame.uproject' | Out-Null } while ($LASTEXITCODE -ne 0) }
$h = $null; $b = $null
try {
  $b = Start-Process $UE -ArgumentList @($Proj, '-game', '-nullrhi', '-nosound', '-RTSNetBot', '-RTSNetJoin=127.0.0.1', "-abslog=`"$BotLog`"", '-unattended') -PassThru
  $h = Start-Process $UE -ArgumentList @($Proj, '-game', '-windowed', '-ResX=1920', '-ResY=1080', '-RenderOffScreen', '-nosplash', "-abslog=`"$HostLog`"",
    '-RTSMuteOutput', '-RTSShellStart=multiplayer.html', '-RTSMpTestHost', "-RTSRoomTest=`"$Script`"", '-ExecCmds="Scalability 1"') -PassThru
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
'--- anfitrião'
Get-Content $HostLog -ErrorAction SilentlyContinue | Select-String 'RTS telas: (sala da LAN|teste da sala)|RTS LAN|RTS largadas|Load map complete|RTS chat|error|Fatal' | Select-Object -Last 60 | ForEach-Object { $_.Line.Substring(0, [Math]::Min(400, $_.Line.Length)) }
'--- robô'
Get-Content $BotLog -ErrorAction SilentlyContinue | Select-String 'RTS LAN|RTS largadas|Load map complete|RTS chat|error|Fatal' | Select-Object -Last 40 | ForEach-Object { $_.Line.Substring(0, [Math]::Min(300, $_.Line.Length)) }
