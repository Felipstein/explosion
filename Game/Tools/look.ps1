# O look do jogo (sessão de gráficos): a maquete da arte e as comparações.
#
#   look.ps1 houses                # gera as casas de adobe no Blender (Tools/look_houses.py, só CPU)
#   look.ps1 civil                 # o kit civil da vila (SM_civ_<tipo>.glb em captures\look\civil_glb, convenção do WorldForge)
#   look.ps1 tank                  # o tanque USA provisório detalhado, a carcaça e o paraquedas (Tools/look_tank.py, só CPU)
#   look.ps1 ground                # o asfalto rachado (look_textures.py), o fogo e a fumaça (look_fx_textures.py) e o chão em relevo (look_ground.py), só CPU
#   look.ps1 build                 # monta a maquete (Tools/look_lab.py), sem janela e sem a placa
#   look.ps1 recipes               # as receitas viram os DataAssets do plugin RTSLook (o jogo lê de lá)
#   look.ps1 render [Before|Golden] [-Quick]   # as câmeras (arte, jogo padrão, jogo perto) em captures\look\;
#                                  -Quick: só a câmera da arte (pra iterar a luz)
#   look.ps1 compare               # folhas lado a lado com a arte e os números (Tools/look_compare.py)
#   look.ps1 iterate Golden        # a volta de ajuste da luz: monta só essa receita, render rápido e números
#
# A maquete vive em Game/Content/LookLab (fora do git: o script refaz). Só lê o que
# vem por link físico (Slice, Units): nunca grava lá (docs/GRAFICOS.md, combinado
# com a sessão do jogo em 10/10).
param(
  [Parameter(Mandatory = $true)][string]$Command,
  [string]$Target = '',
  [switch]$Quick,
  # a cena (Tools/look_scene.py): desert_golden_hour (padrão), desert_night ou desert_ground (a de perto)
  [string]$Scene = 'desert_golden_hour',
  [string]$Preset = ''
)
$ErrorActionPreference = 'Stop'
$UE = 'C:\Program Files\Epic Games\UE_5.8\Engine'
$Engine = Join-Path $UE 'Binaries\Win64'
$Project = (Resolve-Path (Join-Path $PSScriptRoot '..\RTSGame.uproject')).Path
$Root = Split-Path $Project
$Repo = Split-Path $Root
$Cmd = Join-Path $Engine 'UnrealEditor-Cmd.exe'
$Captures = Join-Path $Repo 'captures\look'
$env:LOOK_SCENE = $Scene
$Sub = if ($Scene -eq 'desert_golden_hour') { '' } else { $Scene }
$MapPrefix = if ($Scene -eq 'desert_night') { 'LL_DesertNight' } elseif ($Scene -eq 'desert_ground') { 'LL_DesertGround' } elseif ($Scene -eq 'desert_tanks') { 'LL_DesertTanks' } elseif ($Scene -eq 'desert_crater') { 'LL_DesertCrater' } else { 'LL_DesertDay' }
$Variants = if ($Scene -eq 'desert_night') { @('Night') } elseif ($Scene -eq 'desert_ground') { @('Ground') } elseif ($Scene -eq 'desert_tanks') { @('Tanks') } elseif ($Scene -eq 'desert_crater') { @('Crater') } else { @('Before', 'Golden') }

function RunUnrealPython([string]$script) {
  $s = Join-Path $Root $script
  & $Cmd $Project -run=pythonscript "-script=$s" -nullrhi -unattended -nosplash -stdout -FullStdOutLogOutput |
    Where-Object { $_ -match 'LOOK|Error|Warning: .*Python|Success -|Failure -' } | ForEach-Object { $_ }
  if ($LASTEXITCODE -ne 0) { Write-Error "Unreal Python script failed (code $LASTEXITCODE)"; exit 1 }
}

# A placa é uma só (docs/GPU.md): a fila comum das sessões, renovada enquanto renderiza.
function WithGpu([string]$Owner, [scriptblock]$Work) {
  $queue = 'C:\Users\luisf\.gpu_queue\gpu_queue.ps1'
  do { & powershell -NoProfile -ExecutionPolicy Bypass -File $queue enter -Owner $Owner -Tag 'LookLab' | Out-Null } while ($LASTEXITCODE -ne 0)
  try { & $Work } finally { & powershell -NoProfile -ExecutionPolicy Bypass -File $queue leave -Owner $Owner | Out-Null }
}

function Render([string]$variant) {
  $seq = if ($Quick) { "Seq_$($variant)_Q" } else { "Seq_$variant" }
  $out = Join-Path $Captures $seq
  if (Test-Path $out) { Remove-Item $out -Recurse -Force }
  New-Item -ItemType Directory -Force $Captures | Out-Null
  $log = Join-Path $Captures "$seq.log"
  # não abrir o jogo por cima de um compile da sessão do jogo (o UBT trava as DLLs)
  while (Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -match 'UnrealBuildTool' -and $_.CommandLine -match 'RTSGame' }) {
    Write-Host 'esperando o compile do jogo terminar...'; Start-Sleep 15
  }
  $t = Get-Date
  # -game + Movie Render Queue pela linha de comando (como o blast.ps1 render)
  $p = Start-Process $Cmd -PassThru -NoNewWindow -ArgumentList @(
    "`"$Project`"", "/Game/LookLab/Maps/$($MapPrefix)_$variant", '-game',
    "-LevelSequence=/Game/LookLab/Sequences/$seq.$seq",
    '-MoviePipelineConfig=/Game/LookLab/Render/Config_Look.Config_Look',
    '-windowed', '-resx=1280', '-resy=720', '-log', '-unattended', '-nosplash', '-NoLoadingScreen',
    '-notexturestreaming', "-abslog=`"$log`"")
  $queue = 'C:\Users\luisf\.gpu_queue\gpu_queue.ps1'
  $touched = Get-Date
  while (-not $p.HasExited) {
    Start-Sleep 5
    if (((Get-Date) - $touched).TotalMinutes -ge 5) {
      & powershell -NoProfile -ExecutionPolicy Bypass -File $queue touch -Owner 'Look visual do RTS | render da maquete' | Out-Null
      $touched = Get-Date
    }
  }
  $n = (Get-ChildItem $out -Filter *.png -ErrorAction SilentlyContinue | Measure-Object).Count
  "$variant`: $n quadros em $out ($([int]((Get-Date) - $t).TotalSeconds) s) · log: $log"
}

switch ($Command) {
  'houses' {
    $blender = 'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe'
    $out = Join-Path (Join-Path $Captures $Sub) 'houses_glb'
    New-Item -ItemType Directory -Force $out | Out-Null
    & $blender -b --factory-startup --python (Join-Path $PSScriptRoot 'look_houses.py') -- $out 2>&1 |
      Where-Object { $_ -match 'LOOKHOUSE|Error|Traceback' } | ForEach-Object { $_ }
  }
  'tank' {
    $blender = 'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe'
    $out = Join-Path $Captures 'tank_glb'
    New-Item -ItemType Directory -Force $out | Out-Null
    & $blender -b --factory-startup --python (Join-Path $PSScriptRoot 'look_tank.py') -- $out 2>&1 |
      Where-Object { $_ -match 'LOOKTANK|Error|Traceback' } | ForEach-Object { $_ }
  }
  'civil' {
    # o kit civil da vila pro WorldForge e o jogo: as casas/muros/poste (look_layout_civil.py) e os carros e miudezas
    # com a frente em +Y, em captures\look\civil_glb\SM_civ_<tipo>.glb
    $blender = 'C:\Program Files\Blender Foundation\Blender 5.2lender.exe'
    $env:LOOK_SCENE = 'civil_kit'
    $out = Join-Path (Join-Path $Captures 'civil_kit') 'houses_glb'
    New-Item -ItemType Directory -Force $out | Out-Null
    & $blender -b --factory-startup --python (Join-Path $PSScriptRoot 'look_houses.py') -- $out 2>&1 |
      Where-Object { $_ -match 'LOOKHOUSE|Error|Traceback' } | ForEach-Object { $_ }
    & $blender -b --factory-startup --python (Join-Path $PSScriptRoot 'look_civil_props.py') -- $Repo 2>&1 |
      Where-Object { $_ -match 'LOOKCIVIL|Error|Traceback' } | ForEach-Object { $_ }
  }
  'ground' {
    $py = Join-Path $env:LOCALAPPDATA 'Programs\Python\Python312\python.exe'
    if (-not (Test-Path $py)) { $py = 'python' }
    & $py (Join-Path $PSScriptRoot 'look_textures.py') (Join-Path $Captures 'textures')
    & $py (Join-Path $PSScriptRoot 'look_fx_textures.py') (Join-Path $Captures 'textures')
    & $py (Join-Path $PSScriptRoot 'look_ground.py') (Join-Path (Join-Path $Captures $Sub) 'ground')
  }
  'build' { RunUnrealPython 'Tools\look_lab.py' }
  # as receitas (look_recipes/*.json) viram os DataAssets do plugin RTSLook (/RTSLook/Recipes/DA_Look_<cena>)
  'recipes' { RunUnrealPython 'Tools\look_recipe_import.py' }
  # a função de material do chão com memória (o plugin RTSLook: /RTSLook/Materials/MF_LookGroundMemory)
  'mf' { RunUnrealPython 'Tools\look_mf.py' }
  'render' {
    $list = if ($Target) { @($Target) } else { $Variants }
    WithGpu 'Look visual do RTS | render da maquete' { foreach ($v in $list) { Render $v } }
  }
  'iterate' {
    $env:LOOK_VARIANTS = $Target
    RunUnrealPython 'Tools\look_lab.py' | Where-Object { $_ -match 'environment|Traceback|Error:|propriedade|pós' }
    Remove-Item Env:LOOK_VARIANTS
    $script:Quick = $true
    WithGpu 'Look visual do RTS | render da maquete' { Render $Target }
    $py = Join-Path $env:LOCALAPPDATA 'Programs\Python\Python312\python.exe'
    if (-not (Test-Path $py)) { $py = 'python' }
    & $py (Join-Path $PSScriptRoot 'look_compare.py') $Repo | Select-Object -Last 3
  }
  'perf' {
    # o tempo de GPU da luz no jogo (não no render offline): o mapa em -game, a câmera do jogo
    # ativa desde o começo (CAM_GameDefault), 1920×1080, 600 quadros de perfil e fecha sozinho.
    # look.ps1 perf Golden [-Preset baixa|media|alta|ultra] [-Scene ...]
    $levels = @{ baixa = '1'; media = '2'; alta = '3'; ultra = 'cine' }
    $q = if ($Preset) { $levels[$Preset] } else { '3' }
    $log = Join-Path $Captures "perf_$($Target)_$($Preset).log"
    $csvDir = Join-Path $env:LOCALAPPDATA 'UnrealEngine\5.8\Saved\Profiling\CSV'
    $before = Get-Date
    WithGpu 'Look visual do RTS | desempenho da luz' {
      $p = Start-Process (Join-Path $Engine 'UnrealEditor.exe') -PassThru -ArgumentList @(
        "`"$Project`"", "/Game/LookLab/Maps/$($MapPrefix)_$Target", '-game', '-windowed', '-ResX=1920', '-ResY=1080',
        '-nosplash', '-unattended', '-NoLoadingScreen', "-abslog=`"$log`"", '-csvGpuStats', '-csvCaptureFrames=600',
        '-ExitAfterCsvProfiling', "-ExecCmds=`"Scalability $q`"")
      $p.WaitForExit()
    }
    $csv = Get-ChildItem $csvDir -Filter *.csv | Where-Object { $_.LastWriteTime -gt $before } | Sort-Object LastWriteTime | Select-Object -Last 1
    $py = Join-Path $env:LOCALAPPDATA 'Programs\Python\Python312\python.exe'
    if (-not (Test-Path $py)) { $py = 'python' }
    if ($csv) { & $py (Join-Path $PSScriptRoot 'look_perf.py') $csv.FullName } else { 'sem CSV (ver o log ' + $log + ')' }
  }
  'compare' {
    $py = Join-Path $env:LOCALAPPDATA 'Programs\Python\Python312\python.exe'
    if (-not (Test-Path $py)) { $py = 'python' }
    & $py (Join-Path $PSScriptRoot 'look_compare.py') $Repo
  }
  default { throw "comando desconhecido: $Command" }
}
