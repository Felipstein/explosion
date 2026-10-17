# A linha de base da poeira das lagartas (docs/POEIRA_DAS_LAGARTAS.md, E1): as duas versões de 13/10 (EmberGen e
# plano B) no mapa do jogo, com o tanque, a luz e o asfalto de hoje, nos enquadramentos dos alvos:
#   r1      = a r1_raid_slabs (câmera a 28°, o casco com 16% da largura, o sol 41° à esquerda e 25° de altura, 7,9 m/s)
#   v2close = o gameplay_v2 de perto (a pose do v2 com o campo fechado 3,5×, 2,5 m/s)
# Cada gravação entra na fila da placa (fxlab.ps1 shot) e vira mp4 em captures\fxlab\tread_base\.
param(
  [string[]]$Cams = @('r1', 'v2close'),
  [string[]]$Profiles = @('tread_v1_ember.json', 'tread_v1_handmade.json'),
  [double]$Seconds = 5,
  [string]$Tag = 'base',
  [string]$ShotSize = '',          # o tamanho da foto (ex.: 1024x572, o do game shot r1); vazio = o da câmera
  [string]$Recipe = 'desert_golden_hour_v2_aprovada',   # a luz: a do jogo é desert_golden_dust (Game/Config/RTS/Maps.json)
  [string]$More = ''          # argumentos a mais pra Unreal (ex.: '-TreadPath=toasphalt -TreadLog')
)
$ErrorActionPreference = 'Stop'
$Fx = Join-Path $PSScriptRoot 'fxlab.ps1'
$Repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$Shots = Join-Path $Repo 'captures\fxlab\shots_ue'
$Out = Join-Path $Repo 'captures\fxlab\tread_base'
New-Item -ItemType Directory -Force $Out | Out-Null
$Ffmpeg = & python -c "import imageio_ffmpeg;print(imageio_ffmpeg.get_ffmpeg_exe())"
$N = [int]($Seconds * 30)
$At = (0..($N - 1) | ForEach-Object { '{0:0.0000}' -f ($_ / 30.0) }) -join ','
foreach ($cam in $Cams) {
  $size = if ($cam -eq 'r1' -or $cam -eq 'game') { '1600x900' } else { '1280x720' }
  if ($ShotSize) { $size = $ShotSize }   # o PowerShell não distingue $Size de $size   # game: a câmera do jogo (o teste dele é em 1600×900)
  foreach ($prof in $Profiles) {
    $short = ($prof -replace '^tread_', '' -replace '\.json$', '')
    $name = "${Tag}_${cam}_${short}"
    Get-ChildItem $Shots -Filter "${name}_*.png" -ErrorAction SilentlyContinue | Remove-Item
    & $Fx shot -Name $name -Map World_TestSalt2 -Cam $cam -At $At -Size $size `
      -Extra "-Fx=tread -Load=0 -GameLight -GameTank -LookRecipe=$Recipe -Cam=$cam -TreadProfile=$prof $More" | Out-Null
    Copy-Item (Join-Path $Repo 'FXLab\Saved\Logs\FXLab.log') (Join-Path $Out "$name.log") -Force
    # as fotos saem com o tempo no nome (x_0.0333s.png): em ordem, viram o vídeo a 30 qps
    $list = Join-Path $Out "$name.txt"
    Get-ChildItem $Shots -Filter "${name}_*.png" | Sort-Object { [double](($_.BaseName -replace "^${name}_", '') -replace 's$', '') } |
      ForEach-Object { "file '$($_.FullName -replace '\\', '/')'"; 'duration 0.0333333' } | Set-Content -Encoding ascii $list
    & $Ffmpeg -y -loglevel error -f concat -safe 0 -i $list -r 30 -c:v libx264 -pix_fmt yuv420p -crf 18 (Join-Path $Out "$name.mp4")
    "$name : $((Get-ChildItem $Shots -Filter "${name}_*.png").Count) fotos -> $name.mp4"
    # o disco é apertado: as fotos viram vídeo e vão pra Lixeira (a 1ª fica, pra folha)
    if (Test-Path (Join-Path $Out "$name.mp4")) {
      Add-Type -AssemblyName Microsoft.VisualBasic
      Get-ChildItem $Shots -Filter "${name}_*.png" | Sort-Object Name | Select-Object -Skip 1 | ForEach-Object {
        [Microsoft.VisualBasic.FileIO.FileSystem]::DeleteFile($_.FullName, 'OnlyErrorDialogs', 'SendToRecycleBin') }
    }
  }
}
