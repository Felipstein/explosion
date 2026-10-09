# FXLab: o laboratório de explosões na Unreal (separado do BlastLab).
#
#   fxlab.ps1 compile                 # compila o C++ (FXWreckFire, câmera e painel)
#   fxlab.ps1 setup                   # importa o cozimento do preset.json, o tanque e a areia; monta materiais e mapa
#   fxlab.ps1 view                    # abre o laboratório numa janela (câmera RTS/baixa, pausar, 0.25×)
#   fxlab.ps1 shot -Name x -At 2,4,6 [-Cam low]
#                                     # fotos em captures\fxlab\shots_ue\ com passo fixo de 1/30 s:
#                                     # a mesma história do laboratório no navegador (?shot=x&at=2,4,6&cam=low)
#   fxlab.ps1 worlds                  # copia a fatia do deserto do WorldForge (só lê de lá) e põe os efeitos nela,
#                                     # com a luz do jogo: /Game/Maps/World_Desert (setup_world.py)
#   fxlab.ps1 view -Map World_Desert  # o deserto na câmera do jogo
#   fxlab.ps1 measure -Name x -Map World_Desert
#                                     # mede o quadro (mediana e p95, CPU e GPU) sem efeito, com 1+1 e com 3+3
#                                     # (captures\fxlab\measure_x.txt)
# O ajuste vem do FXLab\preset.json (o mesmo do navegador; a Unreal recarrega sozinha quando ele muda).
param(
  [Parameter(Mandatory = $true)][string]$Command,
  [string]$Name = 'ue',
  [string]$At = '2,4,6',
  [string]$Cam = 'rts',
  [string]$Size = '1280x720',
  [string]$Extra = '',       # shot: argumentos a mais (ex.: '-Fx=impact')
  [string]$Map = 'Lookdev_Fire'
)
$ErrorActionPreference = 'Stop'
$UE = 'C:\Program Files\Epic Games\UE_5.8\Engine'
$Project = (Resolve-Path (Join-Path $PSScriptRoot '..\FXLab.uproject')).Path
$Root = Split-Path $Project
$Editor = Join-Path $UE 'Binaries\Win64\UnrealEditor.exe'
$Cmd = Join-Path $UE 'Binaries\Win64\UnrealEditor-Cmd.exe'

# trava da placa: duas Unreal ao mesmo tempo derrubaram a 1660 (07/10). Antes de abrir com janela,
# espera captures\.gpu_lock sumir (e a janela do BlastLab fechar); cria a trava e apaga no fim.
$Lock = Join-Path (Split-Path $Root) 'captures\.gpu_lock'
# A placa é uma só (docs/GPU.md): entra na FILA das sessões (C:\Users\luisf\.gpu_queue\gpu_queue.ps1: quem chega
# primeiro usa primeiro; ela escreve o captures\.gpu_lock e espera qualquer Unreal com janela de fora). Sem a fila
# instalada, espera a trava e as outras Unreal do jeito antigo.
$Queue = 'C:\Users\luisf\.gpu_queue\gpu_queue.ps1'
$QueueOwner = ''
function WaitGpu([string]$what) {
  if (Test-Path $Queue) {
    $script:QueueOwner = "FXLab | $what"
    do {
      & powershell -NoProfile -File $Queue enter -Owner $script:QueueOwner -Tag 'FXLab' | Out-Null
    } while ($LASTEXITCODE -ne 0)
    return
  }
  while ($true) {
    $busy = $false
    if (Test-Path $Lock) {
      $age = (Get-Date) - (Get-Item $Lock).LastWriteTime
      if ($age.TotalMinutes -lt 20) { $busy = $true }
    }
    # qualquer outra Unreal com janela (BlastLab, WorldForge, RTSGame...) também ocupa a placa
    if (Get-Process UnrealEditor -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowTitle -ne '' -and $_.MainWindowTitle -notlike 'FXLab*' }) { $busy = $true }
    if (-not $busy) { break }
    Start-Sleep 15
  }
  Set-Content $Lock "fxlab $what $(Get-Date -Format s)"
}
function ReleaseGpu {
  if ($script:QueueOwner) { & powershell -NoProfile -File $Queue leave -Owner $script:QueueOwner | Out-Null }
  Remove-Item $Lock -ErrorAction SilentlyContinue
}

switch ($Command) {
  'compile' {
    & (Join-Path $UE 'Build\BatchFiles\Build.bat') FXLabEditor Win64 Development "-Project=$Project" -WaitMutex -NoHotReload -NoHotReloadFromIDE |
      Select-String 'error|Result' | ForEach-Object { $_.Line }
  }
  'setup' {
    & $Cmd $Project -run=pythonscript "-script=$(Join-Path $Root 'Tools\setup_fxlab.py')" -nullrhi -unattended -nosplash -stdout -FullStdOutLogOutput |
      Where-Object { $_ -match 'FXLAB|Error|Traceback|LogPython: Error' } | ForEach-Object { $_ }
  }
  'worlds' {
    # a fatia como está no WorldForge (terreno, rochas, plantas, estrada e a LUZ dela), copiada: lá não se mexe
    $from = Join-Path (Split-Path $Root) 'WorldForge\Content'
    $to = Join-Path $Root 'Content'
    if (-not (Test-Path (Join-Path $from 'Maps\Slice_Desert.umap'))) { Write-Error 'sem WorldForge\Content\Maps\Slice_Desert.umap'; exit 1 }
    robocopy (Join-Path $from 'Slice') (Join-Path $to 'Slice') /MIR /NJH /NJS /NP /NFL /NDL | Out-Null
    if ($LASTEXITCODE -ge 8) { Write-Error "robocopy falhou ($LASTEXITCODE)"; exit 1 }
    New-Item -ItemType Directory -Force (Join-Path $to 'Maps') | Out-Null
    Copy-Item (Join-Path $from 'Maps\Slice_Desert.umap') (Join-Path $to 'Maps\World_Desert.umap') -Force
    # o chão e o mar da fatia usam a normal da água de fora da pasta Slice (sem ela: chão em xadrez, 08/10)
    foreach ($f in 'Textures\T_WaterN.uasset', 'Materials\M_Water.uasset') {
      $s = Join-Path $from $f
      if (Test-Path $s) { New-Item -ItemType Directory -Force (Split-Path (Join-Path $to $f)) | Out-Null; Copy-Item $s (Join-Path $to $f) -Force }
    }
    & $Cmd $Project -run=pythonscript "-script=$(Join-Path $Root 'Tools\setup_world.py')" -nullrhi -unattended -nosplash -stdout -FullStdOutLogOutput |
      Where-Object { $_ -match 'FXLAB|Error|Traceback|LogPython: Error' } | ForEach-Object { $_ }
  }
  'view' {
    Start-Process $Editor -ArgumentList "`"$Project`" /Game/Maps/$Map -game -windowed -ResX=1600 -ResY=900 -nosplash $Extra"
  }
  'measure' {
    # sem -benchmark: o tempo de verdade de cada quadro (a placa livre, nada mais aberto)
    WaitGpu "measure $Name"
    try {
      $p = Start-Process $Editor -PassThru -ArgumentList "`"$Project`" /Game/Maps/$Map -game -windowed -ResX=1920 -ResY=1080 -nosplash -unattended -Measure=$Name $Extra"
      $p.WaitForExit()
    } finally { ReleaseGpu }
    Get-Content (Join-Path (Split-Path $Root) "capturesxlab\measure_$Name.txt") -ErrorAction SilentlyContinue
  }
  'shot' {
    $w, $h = $Size -split 'x'
    WaitGpu "shot $Name"
    try {
      $p = Start-Process $Editor -PassThru -ArgumentList "`"$Project`" /Game/Maps/$Map -game -windowed -ResX=$w -ResY=$h -nosplash -benchmark -fps=30 -unattended -Shot=$Name -At=$At -Cam=$Cam $Extra"
      $p.WaitForExit()
    } finally { ReleaseGpu }
    Get-ChildItem (Join-Path (Split-Path $Root) "captures\fxlab\shots_ue\$Name*") | ForEach-Object { $_.FullName }
  }
  default { Write-Error "comando desconhecido: $Command" }
}
