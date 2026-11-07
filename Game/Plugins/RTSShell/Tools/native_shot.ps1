# A foto do menu nativo dentro do jogo (a foto interna, -RTSMenuShot), curta e fora da tela dele: na vez da fila, abre o jogo, espera
# o "foto do menu nativo" no log e fecha pelo PID. Desiste se o log parar 4 min ou se as telas não vierem 30 s depois do mapa.
# -Boot: em vez da foto, a abertura quadro a quadro (-RTSBootFrames) em <Name>_frames, pro boot_video.py montar o vídeo com o som
# -Page <página> [-Extra '<args>']: as outras telas nativas (o multiplayer, a sala da LAN com -Extra '-RTSLobbyFake=...')
# -Skirmish: a sala do skirmish nativa (-SkHover <vaga> e -SkPop '<vaga>,<campo>' pros estados das fotos do mockup; -SkBrowser o navegador de mapas aberto, com -SkQuery <busca> e -SkCard <cartão> em hover)
param([string]$Name, [int]$Hover = -1, [string]$T = '', [switch]$Window, [switch]$Boot, [switch]$Skirmish, [int]$SkHover = -1, [string]$SkPop = '', [switch]$SkBrowser, [string]$SkQuery = '', [int]$SkCard = -1, [int]$SkPopHover = -1, [string]$Page = '', [string]$Extra = '')
$Q = 'C:\Users\luisf\.gpu_queue\gpu_queue.ps1'
# RTS_QUEUE_OWNER: uma corrente que já está com a vez da fila (várias fotos numa vez só); aí a foto não sai da fila no fim
$Owner = if ($env:RTS_QUEUE_OWNER) { $env:RTS_QUEUE_OWNER } else { "Telas | foto do menu $Name" }
$Out = 'C:\Users\luisf\explosion\captures\concept\ui\shell\shots\native'
New-Item -ItemType Directory -Force $Out | Out-Null
$Png = Join-Path $Out "$Name.png"
$GameLog = Join-Path $Out "$Name.log"
Remove-Item $Png, $GameLog -ErrorAction SilentlyContinue
$UE = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
do { powershell -NoProfile -File $Q enter -Owner $Owner -Tag 'RTSGame.uproject' | Out-Null } while ($LASTEXITCODE -ne 0)
$p = $null
try {
  $a = @('"C:\Users\luisf\explosion\Game\RTSGame.uproject"', '-game', '-windowed', '-ResX=1920', '-ResY=1080', '-RenderOffScreen', '-nosplash',
    "-abslog=`"$GameLog`"", '-ExecCmds="Scalability 3"', "-RTSMenuShot=`"$Png`"", '-RTSMuteOutput')   # o som do teste nunca sai nos alto-falantes dele
  if ($Hover -ge 0) { $a += "-RTSMenuHover=$Hover" }
  if ($T) { $a += "-RTSMenuT=$T" }
  if ($Skirmish) {
    $a = $a | Where-Object { $_ -notlike '-RTSMenuShot=*' }
    $a += '-RTSShellStart=skirmish.html', "-RTSSkirmishShot=`"$Png`""
    if ($SkHover -ge 0) { $a += "-RTSSkHover=$SkHover" }
    if ($SkPop) { $a += "-RTSSkPop=$SkPop" }
    if ($SkPopHover -ge 0) { $a += "-RTSSkPopHover=$SkPopHover" }
    if ($SkBrowser) { $a += '-RTSSkBrowser' }
    if ($SkQuery) { $a += "-RTSSkQuery=$SkQuery" }
    if ($SkCard -ge 0) { $a += "-RTSSkCardHover=$SkCard" }
  }
  # -Page <página>: uma tela nativa qualquer pela foto genérica (-RTSScreenShot), com -Extra pros dados de mentira (-RTSLanFake=..., -RTSLobbyFake=...)
  if ($Page) {
    $a = $a | Where-Object { $_ -notlike '-RTSMenuShot=*' }
    $a += "-RTSShellStart=`"$Page`"", "-RTSScreenShot=`"$Png`""
    if ($Extra) { $a += ($Extra -split ' ' | Where-Object { $_ }) }
  }
  if ($Boot) {
    $Frames = Join-Path $Out "$($Name)_frames"
    if (Test-Path $Frames) { Remove-Item $Frames -Recurse -Force }
    $a = $a | Where-Object { $_ -notlike '-RTSMenuShot=*' }
    $a += "-RTSBootFrames=`"$Frames`""
  }
  # -Window: a janela de verdade (a captura dela, _window.png, precisa da imagem da janela), posta fora do monitor
  if ($Window) { $a = $a | Where-Object { $_ -ne '-RenderOffScreen' }; $a += '-WinX=-4000', '-WinY=0' }
  $p = Start-Process $UE -ArgumentList $a -PassThru
  $t0 = Get-Date; $loaded = $null; $lastLen = 0; $lastGrow = Get-Date
  while (-not $p.HasExited) {
    Start-Sleep 2
    $txt = if (Test-Path $GameLog) { Get-Content $GameLog -Raw -ErrorAction SilentlyContinue } else { '' }
    if ($txt -match 'foto do \w+ nativo|quadros da abertura') { 'foto pronta'; Start-Sleep 5; break }   # mais 5 s: as medidas de custo (a cada 2 s)
    if ($txt -match 'RTS telas: o menu antigo') { 'FALHA: caiu no menu antigo'; break }
    if (-not $loaded -and $txt -match 'Load map complete') { $loaded = Get-Date }
    if ($loaded -and ((Get-Date) - $loaded).TotalSeconds -gt 40) { 'FALHA: o mapa carregou e a foto não veio em 40 s'; break }
    if ($txt.Length -ne $lastLen) { $lastLen = $txt.Length; $lastGrow = Get-Date }
    if (((Get-Date) - $lastGrow).TotalSeconds -gt 240) { 'FALHA: o jogo parou (o log não anda há 4 min)'; break }
    if (((Get-Date) - $t0).TotalMinutes -gt 8) { 'FALHA: não abriu em 8 min'; break }
  }
  "em {0:N0} s" -f ((Get-Date) - $t0).TotalSeconds
} finally {
  if ($p -and -not $p.HasExited) { Stop-Process -Id $p.Id -Force }
  if (-not $env:RTS_QUEUE_OWNER) { powershell -NoProfile -File $Q leave -Owner $Owner | Out-Null }
  if (Test-Path $GameLog) { Select-String -Path $GameLog -Pattern 'RTS telas|Fatal error|Unhandled Exception|Ensure condition' | Select-Object -First 16 | ForEach-Object { $_.Line -replace '^\[[^\]]+\]\[\s*\d+\]', '' } }
}
