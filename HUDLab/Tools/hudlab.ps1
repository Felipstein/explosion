# HUDLab: o laboratório do HUD do jogo (sessão de arte), separado do Game/.
#
#   hudlab.ps1 compile               # compila o C++ (o plugin RTSHud e o laboratório)
#   hudlab.ps1 assets                # prepara HUDLab\LabAssets\ (fundo, minimapa, emblemas) com o Python
#   hudlab.ps1 video [-State 2] [-NoQueue]       # 3 s em movimento: captures\hudlab\video_state_N.mp4
#   hudlab.ps1 view [-State 2]       # abre o laboratório numa janela (teclas 1 a 6 trocam o estado)
#   hudlab.ps1 shot [-State 1..6|all] [-Res 2560x1080] [-Faction china|gla] [-NoQueue] # foto 1920×1080 sem tomar a tela: captures\hudlab\state_N.png
#                                    (-NoQueue: sem a fila da placa; a foto do HUD é leve, só Canvas, sem cena 3D)
#
# O HUD lê o kit direto de docs\concept\hud\kit\ (ícones e fotos) e as fontes de HUDLab\Fonts\.
param(
  [Parameter(Mandatory = $true)][string]$Command,
  [string]$State = '1',
  [int]$Cols = 7,
  [string]$Suffix = '',
  [string]$Res = '1920x1080',
  [string]$Faction = 'usa',
  [switch]$NoQueue,
  [switch]$Measure
)
$ErrorActionPreference = 'Stop'
$UE = 'C:\Program Files\Epic Games\UE_5.8\Engine'
$Project = (Resolve-Path (Join-Path $PSScriptRoot '..\HUDLab.uproject')).Path
$Root = Split-Path $Project
$Repo = Split-Path $Root
$Editor = Join-Path $UE 'Binaries\Win64\UnrealEditor.exe'

# a placa é uma só (docs/GPU.md): entra na fila das sessões antes de abrir a Unreal com imagem
$Queue = 'C:\Users\luisf\.gpu_queue\gpu_queue.ps1'
$QueueOwner = ''
function EnterGpu([string]$what) {
  if ($NoQueue) { return }
  if (Test-Path $Queue) {
    $script:QueueOwner = "HUDLab | $what"
    do {
      & powershell -NoProfile -File $Queue enter -Owner $script:QueueOwner -Tag 'HUDLab' | Out-Null
    } while ($LASTEXITCODE -ne 0)
  }
}
function LeaveGpu() {
  if ($script:QueueOwner -and (Test-Path $Queue)) {
    & powershell -NoProfile -File $Queue leave -Owner $script:QueueOwner | Out-Null
  }
}

switch ($Command) {
  'compile' {
    & (Join-Path $UE 'Build\BatchFiles\Build.bat') HUDLabEditor Win64 Development "-Project=$Project" -WaitMutex -NoHotReload -NoHotReloadFromIDE
    exit $LASTEXITCODE
  }
  'assets' {
    & python (Join-Path $PSScriptRoot 'prepare_assets.py')
    exit $LASTEXITCODE
  }
  'view' {
    EnterGpu 'view'
    try {
      $p = Start-Process $Editor -PassThru -ArgumentList "`"$Project`" -game -windowed -ResX=1600 -ResY=900 -nosplash -State=$State"
      $p.WaitForExit()
    } finally { LeaveGpu }
  }
  'shot' {
    $out = Join-Path $Repo 'captures\hudlab'
    New-Item -ItemType Directory -Force $out | Out-Null
    $states = if ($State -eq 'all') { 1..6 } else { @([int]$State) }
    EnterGpu 'shot'
    try {
      foreach ($s in $states) {
        $file = Join-Path $out "state_$s$Suffix.png"
        if (Test-Path $file) { Remove-Item $file }
        $p = Start-Process $Editor -PassThru -ArgumentList "`"$Project`" -game -windowed -ResX=$($Res.Split('x')[0]) -ResY=$($Res.Split('x')[1]) -ForceRes -RenderOffScreen -nosplash -unattended -State=$s -Cols=$Cols -Res=$Res -Faction=$Faction $(if ($Measure) { '-Measure' }) -Shot=`"$file`""
        if (-not $p.WaitForExit(240000)) { Stop-Process -Id $p.Id -Force; Write-Host "estado ${s}: passou do tempo" }
        if (Test-Path $file) { Write-Host "ok $file" } else { Write-Host "FALHOU estado $s (sem foto)" }
      }
    } finally { LeaveGpu }
  }
  'video' {
    # um vídeo curto do estado (o painel subindo, o aviso entrando, o aperto, as pulsações): captures\hudlab\video_state_N.mp4
    $out = Join-Path $Repo 'captures\hudlab'
    $frames = Join-Path $out "frames_$State"
    if (Test-Path $frames) { Remove-Item -Recurse -Force $frames }
    New-Item -ItemType Directory -Force $frames | Out-Null
    EnterGpu 'video'
    try {
      $p = Start-Process $Editor -PassThru -ArgumentList "`"$Project`" -game -windowed -ResX=1920 -ResY=1080 -ForceRes -RenderOffScreen -nosplash -unattended -State=$State -Cols=$Cols -Seq=90 -Shot=`"$(Join-Path $frames 'f.png')`""
      if (-not $p.WaitForExit(600000)) { Stop-Process -Id $p.Id -Force; Write-Host 'passou do tempo' }
    } finally { LeaveGpu }
    $ffmpeg = & python -c "import imageio_ffmpeg; print(imageio_ffmpeg.get_ffmpeg_exe())"
    $mp4 = Join-Path $out "video_state_$State.mp4"
    & $ffmpeg -y -loglevel error -framerate 30 -i (Join-Path $frames 'f_%03d.png') -c:v libx264 -crf 19 -pix_fmt yuv420p $mp4
    if (Test-Path $mp4) { Write-Host "ok $mp4" } else { Write-Host 'FALHOU o vídeo' }
  }
  default { Write-Host "comando desconhecido: $Command" }
}
