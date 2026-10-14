# O jogo: atalhos (rodar de qualquer pasta).
#
#   game.ps1 setup            # compila, traz mapa e tanque, monta os materiais (primeira vez)
#   game.ps1 compile          # compila o C++ (SimCore + RTSGame)
#   game.ps1 worlds           # traz o mapa do deserto (WorldForge) e o tanque (BlastLab) por link físico
#   game.ps1 content          # monta os materiais do jogo (Tools/build_content.py)
#   game.ps1 models           # os modelos provisórios da base: Blender (blender_provisional.py) + Unreal (import_provisional.py)
#   game.ps1 village          # o kit civil da vila (GLBs do Look em captures/look/civil_glb) pra /Game/Village
#   game.ps1 debris           # os destroços que ficam em volta da carcaça (Blender + Unreal, só eles)
#   game.ps1 view [baixa|media|alta|ultra]   # abre o jogo no menu (padrão: alta), em tela cheia sem borda
#   game.ps1 editor           # abre o editor no projeto
#   game.ps1 simtest          # testes da simulação, sem a Unreal (Tools/simtest.ps1)
#
# Teste automático (sem clicar: abre, joga sozinho, tira fotos e fecha):
#   game.ps1 looktank                 (o tanque da maquete do Look no jogo, com a cor do jogador)
#   game.ps1 dozer                    (o Dozer provisório por script, Modelos 3D 13/10, pra /Game/Units/USA_Dozer)
#   game.ps1 package                  (o jogo empacotado pro Windows em captures/dist/Windows; o Linux roda pelo Proton)
#   game.ps1 view -Folder teste1 [-Script base|skirmish|lanhost|lanclient] [-Players 4] [-NoFog]
#       base = a partida com base do ZH (o padrão); lanhost = a LAN: este jogo cria a sala, fica
#       vermelho e os outros (sem placa, o robô jogando) entram; lanclient = este jogo entra na sala
#       de um anfitrião sem placa; -Players = quantos na sala (com as IAs); -AI = IAs que o
#       anfitrião põe (lanhost); -Teams = 2 contra 2 (a gente e as IAs de um lado); -NoFog = sem névoa
# Fotos em captures\game\<Folder>\, log em captures\game\<Folder>.log
param(
  [Parameter(Mandatory = $true)][string]$Command,
  [string]$Target = '',
  [string]$Folder = '',
  [string]$Script = 'base',
  [int]$Players = 2,
  [int]$AI = 0,
  [switch]$Teams,
  [switch]$NoFog,
  [switch]$Packaged,
  [switch]$Record,
  [string]$Res = '1600x900',
  [string]$Extra = '',
  # o teste da LAN: argumentos a mais pra cada robô, na ordem (ex.: -BotExtra '-RTSNetBotQuitAt=600','-RTSNetTestVote')
  [string[]]$BotExtra = @(),
  [string]$Scenario = ''
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

# A placa é uma só (docs/GPU.md): entra na fila comum das sessões (quem chega primeiro usa
# primeiro; com só a trava, quem confere mais vezes ganhava e o teste esperava pra sempre),
# abre o jogo, renova a cada 5 min enquanto ele roda e sai da fila no fim, também em erro.
function RunWithGpu([string]$Owner, [string[]]$GameArgs, [string]$Exe = '') {
  if (-not $Exe) { $Exe = Join-Path $Engine 'UnrealEditor.exe' }
  $queue = 'C:\Users\luisf\.gpu_queue\gpu_queue.ps1'
  $lock = Join-Path $Repo 'captures\.gpu_lock'
  if (Test-Path $queue) {
    do { & powershell -NoProfile -ExecutionPolicy Bypass -File $queue enter -Owner $Owner -Tag 'RTSGame.uproject' | Out-Null } while ($LASTEXITCODE -ne 0)
  } else {
    while ((Test-Path $lock) -and (((Get-Date) - (Get-Item $lock).LastWriteTime).TotalMinutes -lt 20)) { Start-Sleep 15 }
    Set-Content $lock "$Owner | $(Get-Date -Format s)"
  }
  try {
    $p = Start-Process $Exe -ArgumentList $GameArgs -PassThru
    $touched = Get-Date
    while (-not $p.HasExited) {
      Start-Sleep 5
      if (((Get-Date) - $touched).TotalMinutes -ge 5) {
        if (Test-Path $queue) { & powershell -NoProfile -ExecutionPolicy Bypass -File $queue touch -Owner $Owner | Out-Null }
        else { try { (Get-Item $lock).LastWriteTime = Get-Date } catch { Set-Content $lock "$Owner | $(Get-Date -Format s)" } }
        $touched = Get-Date
      }
    }
  } finally {
    if (Test-Path $queue) { & powershell -NoProfile -ExecutionPolicy Bypass -File $queue leave -Owner $Owner | Out-Null }
    else { Remove-Item $lock -ErrorAction SilentlyContinue }
  }
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
  # a vila (o WorldForge regrava o JSON a cada montagem: cópia, não link; o jogo confere o hash com o mapa)
  $village = Join-Path $Repo 'WorldForge\Saved\WorldForge\exports\World_TestSalt2\village.json'
  if (Test-Path $village) {
    $to = Join-Path $Root 'Config\RTS\Villages\World_TestSalt2.json'
    New-Item -ItemType Directory -Force (Split-Path $to) | Out-Null
    Copy-Item $village $to -Force
    Write-Host "  $village -> $to (cópia)"
  }
  # o tipo de chão do WorldForge (13/10: R asfalto, G terra batida, B rocha, a 1 m; a poeira e o sulco leem)
  # a grade da simulação do WorldForge (13/10: a água por célula de 4 m e os corpos, com o hash dele)
  foreach ($ext in 'png', 'json') {
    $grid = Join-Path $Repo "WorldForge\Saved\WorldForge\exports\World_TestSalt2\grid.$ext"
    if (Test-Path $grid) {
      $to = Join-Path $Root "Config\RTS\Grid\World_TestSalt2.$ext"
      New-Item -ItemType Directory -Force (Split-Path $to) | Out-Null
      Copy-Item $grid $to -Force
      Write-Host "  $grid -> $to (cópia)"
    }
  }
  foreach ($ext in 'png', 'json') {
    $surf = Join-Path $Repo "WorldForge\Saved\WorldForge\exports\World_TestSalt2\surface.$ext"
    if (Test-Path $surf) {
      $to = Join-Path $Root "Config\RTS\Surface\World_TestSalt2.$ext"
      New-Item -ItemType Directory -Force (Split-Path $to) | Out-Null
      Copy-Item $surf $to -Force
      Write-Host "  $surf -> $to (cópia)"
    }
  }
  # o map.json do WorldForge (o vento único do mapa: a areia v2 é assada com ele; o Maps.json aponta pra cá)
  $info = Join-Path $Repo 'WorldForge\Saved\WorldForge\exports\World_TestSalt2\map.json'
  if (Test-Path $info) {
    $to = Join-Path $Root 'Config\RTS\Villages\World_TestSalt2.map.json'
    Copy-Item $info $to -Force
    Write-Host "  $info -> $to (cópia)"
  }
  Write-Host 'tanque USA (BlastLab):'
  LinkTree (Join-Path $bl 'Units') (Join-Path $gc 'Units')
  Write-Host 'explosões (BlastLab: gravações, material do volume, clarão e ofuscamento):'
  LinkTree (Join-Path $bl 'Explosions') (Join-Path $gc 'Explosions')
  LinkFile (Join-Path $bl 'Materials\M_FireGlare.uasset') (Join-Path $gc 'Materials\M_FireGlare.uasset')
  LinkFile (Join-Path $bl 'Materials\M_FlashStar.uasset') (Join-Path $gc 'Materials\M_FlashStar.uasset')
  # a morte de tanque da v2 (13–14/10, o BlastLab): os destroços com rastro de fumaça e a sombra da explosão no chão
  LinkFile (Join-Path $bl 'Materials\M_Base.uasset') (Join-Path $gc 'Materials\M_Base.uasset')
  LinkFile (Join-Path $bl 'Materials\M_DebrisGlow.uasset') (Join-Path $gc 'Materials\M_DebrisGlow.uasset')
  LinkFile (Join-Path $bl 'Materials\M_TrailSmoke.uasset') (Join-Path $gc 'Materials\M_TrailSmoke.uasset')
  LinkFile (Join-Path $bl 'Materials\M_SmokeShadowLF.uasset') (Join-Path $gc 'Materials\M_SmokeShadowLF.uasset')
  Write-Host 'gravações da Carga Pesada pro raymarch (BlastLab Intermediate/Raymarch; só as r96, as outras são comparação):'
  foreach ($v in 0..2) {
    LinkFile (Join-Path $Repo "BlastLab\Intermediate\Raymarch\r96_v$v.brm2") (Join-Path $Root "Intermediate\Raymarch\r96_v$v.brm2")
  }
  # a morte de tanque da v2 (13/10, o vídeo aprovado do BlastLab): a gravação nova, que vai no pacote junto das r96
  LinkFile (Join-Path $Repo 'BlastLab\Intermediate\Raymarch\eg_tq_v2.brm2') (Join-Path $Root 'Intermediate\Raymarch\eg_tq_v2.brm2')
}

switch ($Command) {
  'setup' { Compile; Worlds; RunUnrealPython 'Tools\build_content.py' }
  'compile' { Compile }
  'worlds' { Worlds }
  'content' { RunUnrealPython 'Tools\build_content.py' }
  'models' {
    # o Blender gera os GLB (na CPU, não usa a placa) e a Unreal importa sem janela
    # (sem 2>&1: no PowerShell 5.1 o aviso que o Blender escreve no stderr vira erro e para o script)
    $blender = 'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe'
    $out = Join-Path $Captures 'prov'
    & $blender -b --factory-startup -P (Join-Path $PSScriptRoot 'blender_provisional.py') -- $out |
      Where-Object { $_ -match '^PROV|Error|Traceback' } | ForEach-Object { $_ }
    if ($LASTEXITCODE -ne 0) { Write-Error 'Blender falhou'; exit 1 }
    RunUnrealPython 'Tools\import_provisional.py'
  }
  'debris' {
    # os destroços que ficam no chão em volta da carcaça (13/10): só eles, sem refazer os prédios
    $blender = 'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe'
    $out = Join-Path $Captures 'prov'
    $env:PROV_BUILD = 'debris'
    $env:PROV_FOLDER = 'debris'
    try {
      & $blender -b --factory-startup -P (Join-Path $PSScriptRoot 'blender_provisional.py') -- $out |
        Where-Object { $_ -match '^PROV|Error|Traceback' } | ForEach-Object { $_ }
      if ($LASTEXITCODE -ne 0) { Write-Error 'Blender falhou'; exit 1 }
      RunUnrealPython 'Tools\import_provisional.py'
    } finally { Remove-Item Env:PROV_BUILD, Env:PROV_FOLDER -ErrorAction SilentlyContinue }
  }
  'looktank' {
    # o tanque da maquete do Look no lugar do borrado (pedido do Felipe, 11/10): importa só os GLBs
    # dela (captures/look/tank_glb, que o look.ps1 tank gera) com os nossos materiais, sem refazer os prédios
    $env:PROV_TANK_ONLY = '1'
    try { RunUnrealPython 'Tools\import_provisional.py' } finally { Remove-Item Env:PROV_TANK_ONLY -ErrorAction SilentlyContinue }
  }
  'dozer' {
    # o Dozer provisório por script (Modelos 3D, 13/10, aprovado): importa só os GLBs de captures/game/models/usa_dozer
    # pra /Game/Units/USA_Dozer com os nossos materiais (sem a placa: -nullrhi)
    $env:PROV_DOZER_ONLY = '1'
    try { RunUnrealPython 'Tools\import_provisional.py' } finally { Remove-Item Env:PROV_DOZER_ONLY -ErrorAction SilentlyContinue }
  }
  'truck' {
    # o caminhão de suprimento provisório por script (Modelos 3D, 14/10, aprovado): só os GLBs de
    # captures/game/models/usa_supply_truck pra /Game/Units/USA_SupplyTruck, com os nossos materiais (sem a placa)
    $env:PROV_TRUCK_ONLY = '1'
    try { RunUnrealPython 'Tools\import_provisional.py' } finally { Remove-Item Env:PROV_TRUCK_ONLY -ErrorAction SilentlyContinue }
  }
  'village' {
    # o kit civil da vila (12/10): os GLBs da sessão do Look (captures/look/civil_glb) com os nossos
    # materiais, pra /Game/Village; o jogo põe cada peça pelo village.json do WorldForge
    $env:PROV_CIVIL_ONLY = '1'
    try { RunUnrealPython 'Tools\import_provisional.py' } finally { Remove-Item Env:PROV_CIVIL_ONLY -ErrorAction SilentlyContinue }
  }
  'py' { RunUnrealPython $Target }
  'simtest' { & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'simtest.ps1') }
  'editor' { Start-Process (Join-Path $Engine 'UnrealEditor.exe') -ArgumentList "`"$Project`"" }
  'package' {
    # o jogo empacotado pro Windows (e pro Linux pelo Proton da Steam: o mesmo programa nos dois,
    # a mesma conta da simulação): captures/dist/Windows, fora do git. Cozinha na CPU, sem a placa.
    # (os links do mapa e das gravações do raymarch vêm do 'game.ps1 worlds', já feitos; refazer aqui
    # falha com um jogo aberto segurando os arquivos)
    $uat = Join-Path $UE 'Build\BatchFiles\RunUAT.bat'
    $dist = Join-Path $Repo 'captures\dist'
    $log = Join-Path $Repo 'captures\dist_package.log'
    New-Item -ItemType Directory -Force $dist | Out-Null
    & $uat BuildCookRun "-project=$Project" -noP4 -platform=Win64 -clientconfig=Development -build -cook -stage -pak -iostore -compressed -prereqs `
      -map=/Game/Maps/World_TestSalt2 "-stagingdirectory=$dist" -nocompileeditor -skipbuildeditor -unattended -utf8output *> $log
    $code = $LASTEXITCODE
    Select-String -Path $log -Pattern 'BUILD SUCCESSFUL|BUILD FAILED|Error:|error C|AutomationTool exiting' | Select-Object -Last 15 | ForEach-Object { $_.Line }
    if ($code -ne 0) { Write-Error "o pacote falhou (código $code): $log"; exit 1 }
    $win = Join-Path $dist 'Windows'
    # o HUD novo (-NewHud / F4) lê as imagens e as fontes do disco, ao lado do projeto (ProjectDir/..):
    # vão junto do pacote, no mesmo lugar relativo, pra rodar em outro PC (~10 MB)
    foreach ($rel in 'docs\concept\hud\kit', 'HUDLab\LabAssets', 'HUDLab\Fonts') {
      $to = Join-Path $win $rel
      if (Test-Path $to) { Remove-Item $to -Recurse -Force }
      New-Item -ItemType Directory -Force (Split-Path $to) | Out-Null
      Copy-Item (Join-Path $Repo $rel) $to -Recurse
    }
    "pacote: $win ({0:N1} GB)" -f ((Get-ChildItem $win -Recurse -File | Measure-Object Length -Sum).Sum / 1GB)
  }
  'view' {
    # baixa/média/alta/ultra = Médio/Alto/Épico/Cinematic do motor (como no BlastLab)
    $levels = @{ baixa = '1'; media = '2'; alta = '3'; ultra = 'cine' }
    $q = if ($Target) { $levels[$Target] } else { '3' }
    $rx, $ry = $Res -split 'x'
    $a = @("`"$Project`"", '-game', "-ExecCmds=`"Scalability $q`"", '-nosplash')
    if ($Folder) {
      $a += '-windowed', "-ResX=$rx", "-ResY=$ry"   # o teste: janela de tamanho fixo, fotos comparáveis
    } else {
      # o Felipe joga em tela cheia sem borda, na resolução do monitor (09/10: "tela cheia sempre").
      # A resolução vem da placa (pixels de verdade, sem a escala do Windows); "wf" = janela cheia.
      $vc = Get-CimInstance Win32_VideoController | Where-Object { $_.CurrentHorizontalResolution } | Select-Object -First 1
      $a += if ($vc) { "-Res=$($vc.CurrentHorizontalResolution)x$($vc.CurrentVerticalResolution)wf" } else { '-Res=1920x1080wf' }
    }
    if ($Extra) { $a += $Extra }
    if ($Folder) {
      $dest = Join-Path $Captures $Folder
      if (Test-Path $dest) { Remove-Item $dest -Recurse -Force }
      New-Item -ItemType Directory -Force $dest | Out-Null
      $log = Join-Path $Captures "$Folder.log"
      $a += "-RTSAutoTest=$Script", "-RTSCapture=$dest", "-abslog=$log", '-unattended'
      # o teste rápido (12/10): a partida pronta do Config/RTS/Scenarios.json, fotos em segundos fixos
      if ($Scenario) { $a += "-RTSScenario=$Scenario" }
      # gravação: passo fixo de 1/30 s (cada quadro = um passo da simulação), uma foto por quadro do combate
      if ($Record) { $a += '-RTSRecord', '-benchmark', '-fps=30' }
      # o teste da LAN com uma placa só: os outros jogadores sem placa (-nullrhi), com o robô jogando
      # por eles pela rede; este jogo com janela é jogado pelo teste automático (anfitrião ou cliente)
      $bots = @()
      if ($Script -eq 'lanhost' -or $Script -eq 'lanclient') {
        $a += "-RTSNetPlayers=$Players"
        if ($NoFog) { $a += '-RTSTestNoFog' }
        if ($AI -gt 0) { $a += "-RTSNetAI=$AI" }
        if ($Teams) { $a += '-RTSTestTeams' }
        $gate = Join-Path $Captures "$Folder.lan_gate"
        if (Test-Path $gate) { Remove-Item $gate -Force }
        $botArgs = @()
        if ($Script -eq 'lanhost') {
          # os robôs só entram depois que o anfitrião escolheu a cor (o arquivo que o teste cria)
          $a += "-RTSNetGate=$gate"
          for ($k = 1; $k -lt ($Players - $AI); $k++) { $botArgs += , @('-RTSNetJoin=127.0.0.1', "-RTSNetJoinGate=$gate") }
        } else {
          $hostArgs = @('-RTSNetHost', "-RTSNetAutoStart=$Players")
          if ($NoFog) { $hostArgs += '-RTSNetNoFog' }
          $botArgs += , $hostArgs
          for ($k = 2; $k -lt $Players; $k++) { $botArgs += , @('-RTSNetJoin=127.0.0.1') }
        }
        for ($k = 0; $k -lt $botArgs.Count; $k++) {
          $botLog = Join-Path $Captures "$Folder.bot$k.log"
          $args1 = @("`"$Project`"", '-game', '-nullrhi', '-nosound', '-RTSNetBot', "-abslog=$botLog", '-unattended') + $botArgs[$k]
          if ($k -lt $BotExtra.Count -and $BotExtra[$k]) { $args1 += $BotExtra[$k] -split ' ' }
          $p = Start-Process -FilePath (Join-Path $Engine 'UnrealEditor.exe') -ArgumentList $args1 -PassThru -WindowStyle Minimized
          $bots += [pscustomobject]@{ Proc = $p; Log = $botLog; Name = "bot $k" }
          Write-Host "bot $k sem placa ($($botArgs[$k] -join ' ')): pid $($p.Id), log $botLog"
        }
      }
      try {
        if ($Packaged) {
          # o jogo empacotado (game.ps1 package): o mesmo teste, sem o projeto nem o -game
          $exe = Join-Path $Repo 'captures\dist\Windows\RTSGame.exe'
          RunWithGpu "RTSGame | teste $Folder (pacote)" @($a | Select-Object -Skip 2) $exe
        } else {
          RunWithGpu "RTSGame | teste $Folder" $a
        }
      } finally {
        if ($bots.Count) {
          # os robôs saem sozinhos 10 s depois de voltar pra sala; quem ficou, só pelo PID que abri
          $deadline = (Get-Date).AddSeconds(25)
          while ((Get-Date) -lt $deadline -and ($bots | Where-Object { -not $_.Proc.HasExited })) { Start-Sleep -Seconds 1 }
          foreach ($b in $bots) { if (-not $b.Proc.HasExited) { Stop-Process -Id $b.Proc.Id -Force } }
        }
        if ($gate -and (Test-Path $gate)) { Remove-Item $gate -Force }
      }
      foreach ($b in $bots) {
        if (Test-Path $b.Log) {
          Select-String -Path $b.Log -Pattern 'RTS LAN|RTS sem placa|DESSINCRONIA|Fatal error|Unhandled Exception' | ForEach-Object { "$($b.Name): " + ($_.Line -replace '^\[[^\]]+\]\[\s*\d+\]', '') }
        }
      }
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
      # o Felipe jogando: a placa é dele enquanto o jogo estiver aberto (o script espera o jogo fechar)
      RunWithGpu 'RTSGame | Felipe testando o jogo' $a
    }
  }
  default { throw "comando desconhecido: $Command" }
}
