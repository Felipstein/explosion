# O custo da poeira das lagartas (docs/POEIRA_DAS_LAGARTAS.md, F): o mesmo mapa, a mesma luz e a câmera do jogo, com
# 0 (sem poeira), 20 e 40 tanques em formação levantando poeira; 10 s de amostras em 1920×1080, sem quadro fixo.
# Pela fila da placa; o resultado de cada um vai pra captures\fxlab\measure_tread_<nome>.txt.
# Não vale com o EmberGen de outra sessão aberto (docs/GPU.md): o script confere e avisa.
# -AB chave1,chave2 -Grid 8x5: em vez de rodadas separadas, A/B no mesmo processo (a chave troca a cada 2 s, 12 pares;
# chaves: shadow, shadowmod, clip, k2), com a formação em grade toda no quadro; o resultado vai pra ab_tread_<nome>.txt
param(
  [string]$Profile = 'tread_v11_ember.json',
  [int[]]$Tanks = @(0, 20, 40),
  [string[]]$AB = @(),
  [string]$Grid = '8x5',
  [string]$Tag = '',          # vai no nome do resultado (ex.: 'plugin')
  [string]$More = ''          # argumentos a mais (ex.: '-TreadPlugin -TreadQuality=0': mede a cópia do jogo)
)
$ErrorActionPreference = 'Stop'
$UE = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$Project = (Resolve-Path (Join-Path $PSScriptRoot '..\FXLab.uproject')).Path
$Repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$Queue = 'C:\Users\luisf\.gpu_queue\gpu_queue.ps1'
foreach ($key in $AB) {
  $name = "$($Profile -replace '\.json$','')$Tag`_$($key)_$Grid"
  $owner = "FXLab | A/B $name"
  do { & powershell -NoProfile -File $Queue enter -Owner $owner -Tag 'FXLab' | Out-Null } while ($LASTEXITCODE -ne 0)
  try {
    if (Get-Process EmberGen -ErrorAction SilentlyContinue) { "AVISO: EmberGen aberto durante o A/B $name (medida suja)" }
    $p = Start-Process $UE -PassThru -ArgumentList "`"$Project`" /Game/Maps/World_TestSalt2 -game -windowed -ResX=1920 -ResY=1080 -nosplash -unattended -Cam=game -Fx=tread -Load=0 -GameLight -GameTank -LookRecipe=desert_golden_hour_v2_aprovada -TreadPath=line -TreadSpeed=7.9 -TreadProfile=$Profile -TreadGrid=$Grid -TreadAB=$key -TreadMeasure=$name $More"
    # vigia: a medida dura ~1-2 min; se passar de 4 (tela preta, travou), encerra pelo PID e libera a placa
    if (-not $p.WaitForExit(240000)) { Stop-Process -Id $p.Id -Force; "TRAVOU: $name encerrado pelo PID $($p.Id) depois de 4 min" }
  } finally { & powershell -NoProfile -File $Queue leave -Owner $owner | Out-Null }
  Get-Content (Join-Path $Repo "captures\fxlab\ab_tread_$name.txt") -ErrorAction SilentlyContinue
}
if ($AB.Count) { return }
foreach ($n in $Tanks) {
  $name = "$($Profile -replace '\.json$','')$Tag`_$n"
  $owner = "FXLab | medida $name"
  do { & powershell -NoProfile -File $Queue enter -Owner $owner -Tag 'FXLab' | Out-Null } while ($LASTEXITCODE -ne 0)
  try {
    if (Get-Process EmberGen -ErrorAction SilentlyContinue) { "AVISO: EmberGen aberto durante a medida $name (medida suja)" }
    $p = Start-Process $UE -PassThru -ArgumentList "`"$Project`" /Game/Maps/World_TestSalt2 -game -windowed -ResX=1920 -ResY=1080 -nosplash -unattended -Cam=game -Fx=tread -Load=0 -GameLight -GameTank -LookRecipe=desert_golden_hour_v2_aprovada -TreadPath=line -TreadSpeed=7.9 -TreadProfile=$Profile -TreadTanks=$n -TreadMeasure=$name $More"
    # vigia: a medida dura ~1-2 min; se passar de 4 (tela preta, travou), encerra pelo PID e libera a placa
    if (-not $p.WaitForExit(240000)) { Stop-Process -Id $p.Id -Force; "TRAVOU: $name encerrado pelo PID $($p.Id) depois de 4 min" }
  } finally { & powershell -NoProfile -File $Queue leave -Owner $owner | Out-Null }
  Get-Content (Join-Path $Repo "captures\fxlab\measure_tread_$name.txt") -ErrorAction SilentlyContinue
}
