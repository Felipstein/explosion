# O jogo: atalhos (rodar de qualquer pasta).
#
#   game.ps1 setup            # compila, traz mapa e tanque, monta os materiais (primeira vez)
#   game.ps1 compile          # compila o C++ (SimCore + RTSGame)
#   game.ps1 worlds           # traz o mapa do deserto (WorldForge) e o tanque (BlastLab) por link físico
#   game.ps1 content          # monta os materiais do jogo (Tools/build_content.py)
#   game.ps1 view [baixa|media|alta|ultra]   # abre o jogo no menu (padrão: alta)
#   game.ps1 editor           # abre o editor no projeto
#   game.ps1 simtest          # testes da simulação, sem a Unreal (Tools/simtest.ps1)
#
# Teste automático (sem clicar: abre, joga sozinho, tira fotos e fecha):
#   game.ps1 view -Folder teste1 [-Script skirmish]
# Fotos em captures\game\<Folder>\, log em captures\game\<Folder>.log
param(
  [Parameter(Mandatory = $true)][string]$Command,
  [string]$Target = '',
  [string]$Folder = '',
  [string]$Script = 'skirmish',
  [switch]$Record,
  [string]$Res = '1600x900',
  [string]$Extra = ''
)
$ErrorActionPreference = 'Stop'
$UE = 'C:\Program Files\Epic Games\UE_5.8\Engine'
$Engine = Join-Path $UE 'Binaries\Win64'
$Project = (Resolve-Path (Join-Path $PSScriptRoot '..\RTSGame.uproject')).Path
$Root = Split-Path $Project
$Repo = Split-Path $Root
$Cmd = Join-Path $Engine 'UnrealEditor-Cmd.exe'
$Captures = Join-Path $Repo 'captures\game'

function RunUnrealPython([string]$script) {
  $s = if ([IO.Path]::IsPathRooted($script)) { $script } else { Join-Path $Root $script }
  & $Cmd $Project -run=pythonscript "-script=$s" -nullrhi -unattended -nosplash -stdout -FullStdOutLogOutput |
    Where-Object { $_ -match 'LogPython|Error|Warning: .*Python|Success -|Failure -' } | ForEach-Object { $_ }
  if ($LASTEXITCODE -ne 0) { Write-Error "Unreal Python script failed (code $LASTEXITCODE)"; exit 1 }
}

function Compile {
  # Direto no UnrealBuildTool, sem a trava global do Build.bat (-NoMutex): cada projeto
  # escreve no próprio Intermediate, e a compilação do jogo não fica na fila atrás das
  # outras sessões. Poucas tarefas em paralelo: com Blender e editores abertos a memória
  # acaba e o UBA mata os próprios processos ("Low on memory", 08/10).
  $dotnet = Join-Path $UE 'Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe'
  $ubt = Join-Path $UE 'Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll'
  $log = Join-Path $Root 'Saved\Logs\UBT-RTSGame.txt'
  Push-Location (Join-Path $UE 'Source')
  & $dotnet $ubt RTSGameEditor Win64 Development "-Project=$Project" -NoMutex -NoHotReload -NoHotReloadFromIDE -MaxParallelActions=2 "-Log=$log" |
    Select-String 'error|warning C|Result' | ForEach-Object { $_.Line }
  $code = $LASTEXITCODE
  Pop-Location
  if ($code -ne 0) { Write-Error 'compile failed'; exit 1 }
}

# Link físico (mesmo disco: não ocupa espaço). A Unreal salva num arquivo novo e troca,
# então salvar do lado do jogo nunca escreve no WorldForge/BlastLab.
function LinkTree([string]$From, [string]$To) {
  if (-not (Test-Path $From)) { throw "não existe: $From" }
  $n = 0
  Get-ChildItem $From -Recurse -File | ForEach-Object {
    $rel = $_.FullName.Substring($From.Length).TrimStart('\')
    $dst = Join-Path $To $rel
    New-Item -ItemType Directory -Force (Split-Path $dst) | Out-Null
    if (Test-Path $dst) { Remove-Item $dst -Force }
    New-Item -ItemType HardLink -Path $dst -Target $_.FullName | Out-Null
    $n++
  }
  Write-Host "  $n arquivos: $From -> $To"
}

function LinkFile([string]$From, [string]$To) {
  New-Item -ItemType Directory -Force (Split-Path $To) | Out-Null
  if (Test-Path $To) { Remove-Item $To -Force }
  New-Item -ItemType HardLink -Path $To -Target $From | Out-Null
  Write-Host "  $From -> $To"
}

function Worlds {
  $wf = Join-Path $Repo 'WorldForge\Content'
  $bl = Join-Path $Repo 'BlastLab\Content'
  $gc = Join-Path $Root 'Content'
  Write-Host 'mapa do deserto (WorldForge, World_TestSalt2) e o que ele usa:'
  LinkTree (Join-Path $wf 'Slice') (Join-Path $gc 'Slice')
  LinkFile (Join-Path $wf 'Textures\T_WaterN.uasset') (Join-Path $gc 'Textures\T_WaterN.uasset')
  LinkFile (Join-Path $wf 'Maps\World_TestSalt2.umap') (Join-Path $gc 'Maps\World_TestSalt2.umap')
  Write-Host 'tanque USA (BlastLab):'
  LinkTree (Join-Path $bl 'Units') (Join-Path $gc 'Units')
  Write-Host 'explosões (BlastLab: gravações, material do volume, clarão e ofuscamento):'
  LinkTree (Join-Path $bl 'Explosions') (Join-Path $gc 'Explosions')
  LinkFile (Join-Path $bl 'Materials\M_FireGlare.uasset') (Join-Path $gc 'Materials\M_FireGlare.uasset')
  LinkFile (Join-Path $bl 'Materials\M_FlashStar.uasset') (Join-Path $gc 'Materials\M_FlashStar.uasset')
}

switch ($Command) {
  'setup' { Compile; Worlds; RunUnrealPython 'Tools\build_content.py' }
  'compile' { Compile }
  'worlds' { Worlds }
  'content' { RunUnrealPython 'Tools\build_content.py' }
  'py' { RunUnrealPython $Target }
  'simtest' { & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'simtest.ps1') }
  'editor' { Start-Process (Join-Path $Engine 'UnrealEditor.exe') -ArgumentList "`"$Project`"" }
  'view' {
    # baixa/média/alta/ultra = Médio/Alto/Épico/Cinematic do motor (como no BlastLab)
    $levels = @{ baixa = '1'; media = '2'; alta = '3'; ultra = 'cine' }
    $q = if ($Target) { $levels[$Target] } else { '3' }
    $rx, $ry = $Res -split 'x'
    $a = @("`"$Project`"", '-game', '-windowed', "-ResX=$rx", "-ResY=$ry", "-ExecCmds=`"Scalability $q`"", '-nosplash')
    if ($Extra) { $a += $Extra }
    if ($Folder) {
      $dest = Join-Path $Captures $Folder
      if (Test-Path $dest) { Remove-Item $dest -Recurse -Force }
      New-Item -ItemType Directory -Force $dest | Out-Null
      $log = Join-Path $Captures "$Folder.log"
      $a += "-RTSAutoTest=$Script", "-RTSCapture=$dest", "-abslog=$log", '-unattended'
      # gravação: passo fixo de 1/30 s (cada quadro = um passo da simulação), uma foto por quadro do combate
      if ($Record) { $a += '-RTSRecord', '-benchmark', '-fps=30' }
      # trava da placa (docs/GPU.md): espera a das outras sessões, segura a nossa enquanto o
      # jogo roda (renovada a cada 5 min: a partida gravada passa de 20 min) e solta no fim
      $lock = Join-Path $Repo 'captures\.gpu_lock'
      while ($true) {
        $busy = (Test-Path $lock) -and (((Get-Date) - (Get-Item $lock).LastWriteTime).TotalMinutes -lt 20)
        if (Get-Process UnrealEditor -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowTitle -ne '' }) { $busy = $true }
        if (-not $busy) { break }
        Start-Sleep 15
      }
      Set-Content $lock "RTSGame | teste $Folder | $(Get-Date -Format s)"
      try {
        $p = Start-Process (Join-Path $Engine 'UnrealEditor.exe') -ArgumentList $a -PassThru
        $touched = Get-Date
        while (-not $p.HasExited) {
          Start-Sleep 5
          if (((Get-Date) - $touched).TotalMinutes -ge 5) {
            try { (Get-Item $lock).LastWriteTime = Get-Date } catch { Set-Content $lock "RTSGame | teste $Folder | $(Get-Date -Format s)" }
            $touched = Get-Date
          }
        }
      } finally { Remove-Item $lock -ErrorAction SilentlyContinue }
      Select-String -Path $log -Pattern 'RTS' | ForEach-Object { $_.Line -replace '^\[[^\]]+\]\[\s*\d+\]', '' }
      $frames = Join-Path $dest 'frames'
      if ($Record -and (Test-Path $frames)) {
        $py = Join-Path $env:LOCALAPPDATA 'Programs\Python\Python312\python.exe'
        $ffmpeg = & $py -c "import imageio_ffmpeg; print(imageio_ffmpeg.get_ffmpeg_exe())"
        $mp4 = Join-Path $dest 'batalha.mp4'
        # o ffmpeg para no primeiro número que falta: renumera em sequência antes
        $i = 0
        Get-ChildItem $frames -Filter 'f_*.png' | Sort-Object Name | ForEach-Object {
          Rename-Item $_.FullName (Join-Path $frames ('s_{0:D5}.png' -f $i)); $i++
        }
        & $ffmpeg -y -loglevel error -framerate 30 -i (Join-Path $frames 's_%05d.png') -c:v libx264 -crf 19 -preset slow -pix_fmt yuv420p $mp4
        if ($LASTEXITCODE -eq 0) { Remove-Item $frames -Recurse -Force; Write-Host "vídeo: $mp4" }
      }
    } else {
      Start-Process (Join-Path $Engine 'UnrealEditor.exe') -ArgumentList $a
    }
  }
  default { throw "comando desconhecido: $Command" }
}
