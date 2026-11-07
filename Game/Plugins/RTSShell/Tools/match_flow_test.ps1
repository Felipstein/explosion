# O teste de ponta a ponta das telas da partida numa partida de verdade, sem clicar (06/11):
# o jogo abre no SCRIPT com a cena "resultado" armada (1 tanque contra 1: um morre em segundos) e roda ela sozinho (-RTSScriptAutoRun);
# o -RTSMatchTest (RTSShell.cpp, RTSShellMatchTest) escreve no log cada tela da partida, tira uma foto do jogo inteiro (a interface junto)
# em cada uma, aperta o Esc de verdade aos 3 s de partida e o RESUME no Esc. No resultado, acha o botão pela ponte do teste automático
# (FRTSShell::MatchButtonCenter) e clica de verdade pela Slate, como o RTSAutoTest. A janela fica fora do monitor (a foto com a
# interface precisa da janela de verdade). Na vez da fila; sai pelo PID.
# -OldMenu: o modo do teste automático e do -RTSOldMenu (o menu antigo do HUD; a partida começa pelo botão "skirmish" do controle):
# prova que lá também só aparecem as telas novas.
# As fotos e o log em captures/concept/ui/shell/shots/native/partida_real[_menu_antigo]/.
# -Tag <nome>: a pasta ganha o sufixo (06/11 à noite: "-Scene nao_existe -OldMenu -Tag erro" prova o aviso da partida que não monta e
# o MAIN MENU dele).
param([int]$Seconds = 170, [string]$Scene = 'resultado', [switch]$OldMenu, [string]$Tag = '')
$Q = 'C:\Users\luisf\.gpu_queue\gpu_queue.ps1'
$Owner = if ($env:RTS_QUEUE_OWNER) { $env:RTS_QUEUE_OWNER } else { 'Telas | teste das telas da partida' }
$Out = 'C:\Users\luisf\explosion\captures\concept\ui\shell\shots\native\partida_real'
if ($OldMenu) { $Out += '_menu_antigo' }
if ($Tag) { $Out += "_$Tag" }
if (Test-Path $Out) { Remove-Item $Out -Recurse -Force }
New-Item -ItemType Directory -Force $Out | Out-Null
$Log = Join-Path $Out 'jogo.log'
$UE = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$a = @('"C:\Users\luisf\explosion\Game\RTSGame.uproject"', '-game', '-windowed', '-ResX=1920', '-ResY=1080', '-WinX=-4000', '-WinY=0',
  '-nosplash', "-abslog=`"$Log`"", '-RTSMuteOutput', "-RTSScenario=$Scene", "-RTSMatchTest=`"$Out`"", '-ExecCmds="Scalability 1"')
if ($OldMenu) { $a += '-RTSOldMenu', '-RTSMatchTestStart' } else { $a += '-RTSShellStart=script.html', '-RTSScriptAutoRun' }
if (-not $env:RTS_QUEUE_OWNER) { do { powershell -NoProfile -File $Q enter -Owner $Owner -Tag 'RTSGame.uproject' | Out-Null } while ($LASTEXITCODE -ne 0) }
$p = $null
try {
  $p = Start-Process $UE -ArgumentList $a -PassThru
  "jogo $($p.Id)"
  $t0 = Get-Date
  while (((Get-Date) - $t0).TotalSeconds -lt $Seconds -and -not $p.HasExited) {
    Start-Sleep 5
    $txt = if (Test-Path $Log) { Get-Content $Log -Raw -ErrorAction SilentlyContinue } else { '' }
    # o clique do resultado levou de volta ao menu: fim
    if ($txt -match 'clique no' -and ($txt -split 'clique no')[-1] -match 'Load map complete') { Start-Sleep 4; break }
    if (-not $env:RTS_QUEUE_OWNER) { powershell -NoProfile -File $Q touch -Owner $Owner | Out-Null }
  }
}
finally {
  if ($p -and -not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
  if (-not $env:RTS_QUEUE_OWNER) { powershell -NoProfile -File $Q leave -Owner $Owner | Out-Null }
}
'--- a partida' + $(if ($OldMenu) { ' (o menu antigo)' } else { '' })
Get-Content $Log -ErrorAction SilentlyContinue | Select-String 'RTS telas: (tela da partida|teste da partida|SCRIPT|skirmish\.html|menu\.html)|Load map complete|LOADING|IsInGameThread|Critical error|Assertion' | Select-Object -Last 40 | ForEach-Object { $_.Line.Substring(0, [Math]::Min(260, $_.Line.Length)) }
Get-ChildItem $Out -Filter *.png | ForEach-Object { "foto $($_.Name) $([int]($_.Length / 1024)) KB" }
