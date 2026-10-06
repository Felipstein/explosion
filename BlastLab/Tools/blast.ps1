# BlastLab: atalhos do simulador (rodar de qualquer pasta).
#
#   blast.ps1 setup                      # compila o C++ e monta tudo do zero (campo de teste + explosão)
#   blast.ps1 compile                    # compila os módulos C++ (BlastLab, BlastLabEditor)
#   blast.ps1 build-scene                # monta o campo de teste (sem abrir o editor)
#   blast.ps1 build-explosion            # converte a explosão gravada e põe no campo (depois de build-scene);
#                                        # e os materiais do avião/míssil/disparo (build_movers.py)
#   blast.ps1 render Seq_Explosion       # renderiza uma sequência em captures\ue\ (visual máximo)
#   blast.ps1 view [baixa|media|alta|ultra]      # abre o simulador (padrão: alta)
#   blast.ps1 editor                     # abre o editor no projeto
#   blast.ps1 py Tools\x.py              # roda um script Python da Unreal sem abrir o editor
#
# Teste A/B no render: -Cvars muda variáveis de console só nesta renderização
# e -Folder guarda o resultado com outro nome (captures\ue\<Folder>):
#   blast.ps1 render Seq_Explosion -Cvars 'r.X=0,r.Y=1' -Folder Seq_Explosion_B
# -ProfileGpu: qualidade de jogo + tempos de GPU por quadro (depois: python Tools\gpu_profile.py).
# view -Folder <nome>: teste automático do simulador — grava telas em
# captures\ue\<nome>\, mede o tempo de quadro no log e fecha sozinho.
param(
  [Parameter(Mandatory = $true)][string]$Command,
  [string]$Target = '',
  [string]$Cvars = '',
  [string]$Folder = '',
  [switch]$ProfileGpu,
  [string]$Hour = '',     # view: começa nessa hora do dia (ex.: 22.5)
  [string]$Cam = '',      # view: câmera fixa dist,azim,elev,alvoY,fovV (ex.: a das referências do WebGL)
  [string]$GameArgs = ''  # view: argumentos extras do jogo (ex.: '-AELow=60 -AEHigh=98 -AEBias=1')
)
$ErrorActionPreference = 'Stop'
$UE = 'C:\Program Files\Epic Games\UE_5.8\Engine'
$Engine = Join-Path $UE 'Binaries\Win64'
$Project = (Resolve-Path (Join-Path $PSScriptRoot '..\BlastLab.uproject')).Path
$Root = Split-Path $Project
$Repo = Split-Path $Root
$Cmd = Join-Path $Engine 'UnrealEditor-Cmd.exe'
$Captures = Join-Path $Repo 'captures\ue'

function RunUnrealPython([string]$script) {
  $s = if ([IO.Path]::IsPathRooted($script)) { $script } else { Join-Path $Root $script }
  & $Cmd $Project -run=pythonscript "-script=$s" -nullrhi -unattended -nosplash -stdout -FullStdOutLogOutput |
    Where-Object { $_ -match 'LogPython|Error|Warning: .*Python|Success -|Failure -' } | ForEach-Object { $_ }
  if ($LASTEXITCODE -ne 0) { Write-Error "Unreal Python script failed (code $LASTEXITCODE)"; exit 1 }
}

# pré-requisitos gerados pelo Python do sistema (numpy, pillow): só se faltarem
function Prepare([string]$output, [string]$tool) {
  if (-not (Test-Path (Join-Path $Root $output))) {
    python (Join-Path $PSScriptRoot $tool)
    if ($LASTEXITCODE -ne 0) { Write-Error "$tool failed"; exit 1 }
  }
}

function Compile {
  # -NoHotReloadFromIDE: o Live Coding de QUALQUER UnrealEditor.exe aberto (outro
  # projeto também) trava o build; as DLLs do BlastLab não estão carregadas nele
  & (Join-Path $UE 'Build\BatchFiles\Build.bat') BlastLabEditor Win64 Development "-Project=$Project" -WaitMutex -NoHotReload -NoHotReloadFromIDE |
    Select-String 'error|Result' | ForEach-Object { $_.Line }
  if ($LASTEXITCODE -ne 0) { Write-Error 'compile failed'; exit 1 }
}

function BuildScene {
  Prepare 'Intermediate\Ground\ground_A.png' 'make_ground.py'
  RunUnrealPython 'Tools\build_scene.py'
}

function BuildExplosion {
  Prepare 'Intermediate\BakeRaw\r96_v2\meta.json' 'convert_bake.py'    # as 3 variantes
  Prepare 'Intermediate\Noise\curl_noise_64.bgra' 'make_noise.py'
  Prepare 'Intermediate\BakeVelocity\velocity.json' 'export_bake_velocity.py'   # o vento do que passa deforma a explosão
  RunUnrealPython 'Tools\build_explosion.py'
  RunUnrealPython 'Tools\build_movers.py'                              # avião, míssil, disparo
}

switch ($Command) {
  'setup' { Compile; BuildScene; BuildExplosion }
  'compile' { Compile }
  'build-scene' { BuildScene }
  'build-explosion' { BuildExplosion }
  'py' { RunUnrealPython $Target }
  'render' {
    if (-not $Target) { throw 'which sequence? render Seq_Explosion' }
    $out = Join-Path $Captures $Target
    if (Test-Path $out) { Remove-Item $out -Recurse -Force }
    $log = Join-Path $Captures "$Target.log"
    New-Item -ItemType Directory -Force $Captures | Out-Null
    $t = Get-Date
    # -game + Movie Render Queue pela linha de comando: abre uma janela,
    # renderiza a sequência com passo de tempo fixo e fecha
    $extra = @()
    if ($Cvars) { $extra += "-dpcvars=$Cvars" }
    $config = 'Config_Test'
    if ($ProfileGpu) {
      $extra += '-csvGpuStats', '-csvCaptureFrames=300'
      $config = 'Config_Profile'
    }
    & $Cmd $Project '/Game/Maps/TestRange' -game "-LevelSequence=/Game/Sequences/$Target.$Target" `
      "-MoviePipelineConfig=/Game/Render/$config.$config" -windowed -resx=1280 -resy=720 `
      -log -unattended -nosplash -NoLoadingScreen -notexturestreaming "-abslog=$log" @extra | Out-Null
    if ($Folder) {
      $dest = Join-Path $Captures $Folder
      if (Test-Path $dest) { Remove-Item $dest -Recurse -Force }
      Move-Item $out $dest
      $newLog = Join-Path $Captures "$Folder.log"
      Move-Item $log $newLog -Force
      $out, $log = $dest, $newLog
    }
    $n = (Get-ChildItem $out -Include *.jpeg,*.jpg,*.png -Recurse -ErrorAction SilentlyContinue | Measure-Object).Count
    "rendered: $n frames in $out ($([int]((Get-Date) - $t).TotalSeconds) s) · log: $log"
  }
  'editor' { Start-Process (Join-Path $Engine 'UnrealEditor.exe') -ArgumentList "`"$Project`"" }
  'view' {
    # preset do painel: baixa | media | alta | ultra (padrão alta)
    $q = if ($Target) { $Target } else { 'alta' }
    $a = @("`"$Project`"", '/Game/Maps/TestRange', '-game', '-windowed', '-ResX=1600', '-ResY=900',
           "-Quality=$q", '-nosplash')
    if ($Cvars) { $a += "-ExecCmds=`"$Cvars`"" }        # comandos extras: view alta -Cvars 'r.X 1, r.Y 0'
    if ($Hour) { $a += "-TimeOfDay=$Hour" }
    if ($Cam) { $a += "-Cam=$Cam" }
    if ($GameArgs) { $a += $GameArgs -split '\s+' }
    if ($Folder) {
      $dest = Join-Path $Captures $Folder
      if (Test-Path $dest) { Remove-Item $dest -Recurse -Force }
      New-Item -ItemType Directory -Force $dest | Out-Null
      $log = Join-Path $Captures "$Folder.log"
      $a += "-AutoTest=$dest", "-abslog=$log", '-unattended'
      if ($ProfileGpu) { $a += '-csvGpuStats', '-AutoProfile' }   # tempos de GPU durante a explosão → Tools\gpu_profile.py --live
      Start-Process (Join-Path $Engine 'UnrealEditor.exe') -ArgumentList $a -Wait
      Select-String -Path $log -Pattern 'AUTOTEST|Error' | ForEach-Object { $_.Line -replace '^\[[^\]]+\]\[\s*\d+\]', '' }
    } else {
      Start-Process (Join-Path $Engine 'UnrealEditor.exe') -ArgumentList $a
    }
  }
  default { throw "unknown command: $Command" }
}
