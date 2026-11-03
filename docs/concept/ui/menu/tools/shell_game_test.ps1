# Um teste das telas no jogo, curto e fora da tela dele: na vez da fila, abre o jogo (sem o teste automático, então com as telas
# novas) com a porta de depuração do navegador, espera o sinal "RTS telas: menu.html" no log e DESISTE se ele não vier (o menu
# antigo no log = na hora; 30 s depois do mapa carregado; 4 min no total). Dirige as páginas pelo shell_cdp.mjs e fecha pelo PID.
param([string]$Name, [string]$Steps, [string]$Extra = '', [int]$AfterSec = 20)
$Q = 'C:\Users\luisf\.gpu_queue\gpu_queue.ps1'
$Owner = "Telas | teste $Name"
$Stage = Split-Path -Parent $MyInvocation.MyCommand.Path
$Out = Join-Path 'C:\Users\luisf\explosion\captures\concept\ui\shell\shots\game' $Name
if (Test-Path $Out) { Remove-Item $Out -Recurse -Force }
New-Item -ItemType Directory -Force $Out | Out-Null
$GameLog = Join-Path $Out 'game.log'
$Node = 'C:\Users\luisf\AppData\Local\Volta\tools\image\node\24.15.0\node.exe'
$UE = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
do { powershell -NoProfile -File $Q enter -Owner $Owner -Tag 'RTSGame.uproject' | Out-Null } while ($LASTEXITCODE -ne 0)
$p = $null
try {
  $gameArgs = @('"C:\Users\luisf\explosion\Game\RTSGame.uproject"', '-game', '-windowed', '-ResX=1920', '-ResY=1080', '-RenderOffScreen', '-nosplash', '-cefdebug=9333', "-abslog=`"$GameLog`"", '-ExecCmds="Scalability 3"')
  if ($Extra) { $gameArgs += $Extra -split ' ' }
  $p = Start-Process $UE -ArgumentList $gameArgs -PassThru
  "jogo PID $($p.Id)"
  $t0 = Get-Date; $loaded = $null; $ready = $false; $lastLen = 0; $lastGrow = Get-Date
  while (-not $p.HasExited) {
    Start-Sleep 2
    $txt = if (Test-Path $GameLog) { Get-Content $GameLog -Raw -ErrorAction SilentlyContinue } else { '' }
    if ($txt -match 'RTS telas: menu\.html') { $ready = $true; break }
    if ($txt -match 'RTS telas: o menu antigo') { 'FALHA: o jogo caiu no menu antigo'; break }
    if (-not $loaded -and $txt -match 'Load map complete') { $loaded = Get-Date }
    if ($loaded -and ((Get-Date) - $loaded).TotalSeconds -gt 30) { 'FALHA: o mapa carregou e as telas não apareceram em 30 s'; break }
    # parado = o log não cresce há 4 min (os shaders globais compilam sem escrever no log) (compilar shader faz o log andar: isso não é parado); teto de 8 min
    if ($txt.Length -ne $lastLen) { $lastLen = $txt.Length; $lastGrow = Get-Date }
    if (((Get-Date) - $lastGrow).TotalSeconds -gt 240) { 'FALHA: o jogo parou (o log não anda há 4 min)'; break }
    if (((Get-Date) - $t0).TotalMinutes -gt 8) { 'FALHA: o jogo não abriu em 8 min'; break }
  }
  "pronto em {0:N0} s: $ready" -f ((Get-Date) - $t0).TotalSeconds
  if ($ready) {
    & $Node (Join-Path $Stage 'shell_cdp.mjs') 9333 $Out $Steps
    $t1 = Get-Date
    while (-not $p.HasExited -and ((Get-Date) - $t1).TotalSeconds -lt $AfterSec) { Start-Sleep 2 }
  }
} finally {
  if ($p -and -not $p.HasExited) { Stop-Process -Id $p.Id -Force }
  powershell -NoProfile -File $Q leave -Owner $Owner | Out-Null
  Start-Sleep 2
  if (Test-Path $GameLog) { Select-String -Path $GameLog -CaseSensitive -Pattern 'RTS telas|RTS facção do jogador|RTS qualidade|RTS laboratório|RTS mapa da cena|RTS largadas|RTS cenário|Fatal error|Unhandled Exception' | Select-Object -First 60 | ForEach-Object { $_.Line -replace '^\[[^\]]+\]\[\s*\d+\]', '' } }
}
