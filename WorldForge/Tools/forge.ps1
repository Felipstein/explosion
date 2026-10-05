# WorldForge: atalhos do gerador de mundos (rodar de qualquer pasta).
#
#   forge.ps1 setup                  # compila o C++ e monta tudo do zero (texturas, materiais, mapa)
#   forge.ps1 compile                # compila o módulo C++
#   forge.ps1 build                  # texturas do chão + materiais + mapa (sem abrir o editor)
#   forge.ps1 view [low|medium|high|epic|cine]   # abre o gerador (padrão: cine, o máximo)
#   forge.ps1 editor                 # abre o editor no projeto
#
# Teste automático (gera, fotografa vistas fixas, salva os dados e fecha):
#   forge.ps1 view -Folder teste1 -Seeds 1,2,3 [-Players 4] [-Size 1536] [-Biome 1]
# Fotos e dados em captures\worldforge\<Folder>\, log em captures\worldforge\<Folder>.log
# Teste da interface (cliques, arrastes, teclado e roda sintéticos; confere cada passo no log):
#   forge.ps1 view -Folder ui -UITest
param(
  [Parameter(Mandatory = $true)][string]$Command,
  [string]$Target = '',
  [string]$Folder = '',
  [string[]]$Seeds = @(),
  [int]$Players = 0,
  [int]$Size = 0,
  [int]$Biome = -1,
  [string]$Cvars = '',
  [switch]$UITest,
  [string]$Extra = ''      # argumentos extras pro app (ex.: '-WaterScatter=0.01,0.02,0.02')
)
$ErrorActionPreference = 'Stop'
$UE = 'C:\Program Files\Epic Games\UE_5.8\Engine'
$Engine = Join-Path $UE 'Binaries\Win64'
$Project = (Resolve-Path (Join-Path $PSScriptRoot '..\WorldForge.uproject')).Path
$Root = Split-Path $Project
$Repo = Split-Path $Root
$Cmd = Join-Path $Engine 'UnrealEditor-Cmd.exe'
$Captures = Join-Path $Repo 'captures\worldforge'

function RunUnrealPython([string]$script) {
  $s = if ([IO.Path]::IsPathRooted($script)) { $script } else { Join-Path $Root $script }
  & $Cmd $Project -run=pythonscript "-script=$s" -nullrhi -unattended -nosplash -stdout -FullStdOutLogOutput |
    Where-Object { $_ -match 'LogPython|Error|Warning: .*Python|Success -|Failure -' } | ForEach-Object { $_ }
  if ($LASTEXITCODE -ne 0) { Write-Error "Unreal Python script failed (code $LASTEXITCODE)"; exit 1 }
}

function Compile {
  & (Join-Path $UE 'Build\BatchFiles\Build.bat') WorldForgeEditor Win64 Development "-Project=$Project" -WaitMutex -NoHotReload -NoHotReloadFromIDE |
    Select-String 'error|warning C|Result' | ForEach-Object { $_.Line }
  if ($LASTEXITCODE -ne 0) { Write-Error 'compile failed'; exit 1 }
}

function Build {
  python (Join-Path $PSScriptRoot 'make_textures.py')
  if ($LASTEXITCODE -ne 0) { Write-Error 'make_textures.py failed'; exit 1 }
  RunUnrealPython 'Tools\build_world.py'
}

switch ($Command) {
  'setup' { Compile; Build }
  'compile' { Compile }
  'build' { Build }
  'py' { RunUnrealPython $Target }
  'editor' { Start-Process (Join-Path $Engine 'UnrealEditor.exe') -ArgumentList "`"$Project`"" }
  'view' {
    # o comando Scalability só aceita número (0–3) ou cine
    $levels = @{ low = '0'; medium = '1'; high = '2'; epic = '3'; cine = 'cine' }
    $q = if ($Target) { $levels[$Target] } else { 'cine' }
    if ($Cvars) { $q += ', ' + $Cvars }
    $a = @("`"$Project`"", '/Game/Maps/Forge', '-game', '-windowed', '-ResX=1600', '-ResY=900',
           "-ExecCmds=`"Scalability $q`"", '-nosplash')
    if ($Players) { $a += "-Players=$Players" }
    if ($Size) { $a += "-Size=$Size" }
    if ($Biome -ge 0) { $a += "-Biome=$Biome" }
    if ($Extra) { $a += $Extra }
    if ($Folder) {
      $dest = Join-Path $Captures $Folder
      if (Test-Path $dest) { Remove-Item $dest -Recurse -Force }
      New-Item -ItemType Directory -Force $dest | Out-Null
      $log = Join-Path $Captures "$Folder.log"
      $a += "-ForgeCapture=$dest", "-abslog=$log", '-unattended'
      if ($Seeds) { $a += "-ForgeSeeds=$($Seeds -join ',')" }
      if ($UITest) { $a += '-ForgeUITest' }
      Start-Process (Join-Path $Engine 'UnrealEditor.exe') -ArgumentList $a -Wait
      Select-String -Path $log -Pattern 'FORGE' | ForEach-Object { $_.Line -replace '^\[[^\]]+\]\[\s*\d+\]', '' }
    } else {
      if ($Seeds) { $a += "-Seed=$($Seeds[0])" }
      Start-Process (Join-Path $Engine 'UnrealEditor.exe') -ArgumentList $a
    }
  }
  default { throw "unknown command: $Command" }
}
