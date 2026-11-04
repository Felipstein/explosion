# (04/11, item 22 T0 com o PDO) o teste do recorte com o PDO no mestre do World_TestHills, uma fase por vez de fila.
# O mestre só fica mudado DENTRO da minha vez da placa (nenhuma outra Unreal aberta): troco o .uasset pelo estado do teste, abro o
# jogo eu mesmo (os mesmos argumentos do game.ps1 view, sem ele entrar na fila de novo) e devolvo o original por cópia de arquivo
# antes de sair da vez. Os testes do World_TestHills das outras sessões nunca veem o chão mexido.
#   -Phase build   probe (só lê + cópia de segurança), monta os estados zero e corte, devolve o original e confere o desfazer
#   -Phase zero    o recorte zerado (Nanite, sem Nanite, Nanite de novo)
#   -Phase corte   o recorte do Arms Dealer
#   -Phase custo   A/B do custo: corte, original, corte, original (o original fica)
#   -Phase final   confere o desfazer (Python + hash) e a foto do t0_corte de 03/11
param([Parameter(Mandatory = $true)][string]$Phase)
$ErrorActionPreference = 'Continue'
$s = 'C:\Users\luisf\AppData\Local\Temp\claude\C--Users-luisf-explosion\12f1e755-ddf7-4e41-a834-371a3dfa6f7e\scratchpad'
$snap = Join-Path $s 't0pdo'
$game = 'C:\Users\luisf\explosion\Game\Tools\game.ps1'
$Q = 'C:\Users\luisf\.gpu_queue\gpu_queue.ps1'
$Owner = 'Elenco | T0 PDO do recorte'
$Tag = 'T0PDOTEST'
$Master = 'C:\Users\luisf\explosion\Game\Content\Slice\Materials\M_SliceGround_world_desert_sa452615_hills.uasset'
$Project = 'C:\Users\luisf\explosion\Game\RTSGame.uproject'
$Exe = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$Captures = 'C:\Users\luisf\explosion\captures\game'
Set-Location C:\Users\luisf\explosion
$env:PYTHONIOENCODING = 'utf-8'

function Hash($p) { (Get-FileHash $p -Algorithm SHA256).Hash.Substring(0, 16).ToLower() }
function Touch { & powershell -NoProfile -ExecutionPolicy Bypass -File $Q touch -Owner $Owner | Out-Null }
function Swap($name) {
  $src = Join-Path $snap "$name.uasset"
  for ($k = 0; $k -lt 10; $k++) {
    try { Copy-Item $src $Master -Force -ErrorAction Stop; break } catch { "copia falhou ($($_.Exception.Message)), de novo"; Start-Sleep 3 }
  }
  (Get-Item $Master).LastWriteTime = Get-Date   # (a cópia mantém a data da origem: data nova pra quem olha a data)
  $h = Hash $Master
  "mestre = $name ($h; $(if ($h -eq (Hash $src)) { 'igual ao retrato' } else { 'DIFERENTE do retrato' }))"
}
function Py($mode) {
  $env:T0_MODE = $mode
  & powershell -NoProfile -ExecutionPolicy Bypass -File $game py "$s\t0_pdo.py" 2>&1 | Select-String 'T0P |Error|error C|Traceback' | ForEach-Object { $_.Line -replace '^\[[^\]]+\]\[\s*\d+\]', '' }
}
function RunGame($scenario, $folder, $cv) {
  # os argumentos do game.ps1 view -Folder (alta, 1600x900), sem a fila (a vez já é minha)
  $exec = 'Scalability 3, rts.Record.Fast 1'
  if ($cv) { $exec += ", $cv" }
  $dest = Join-Path $Captures $folder
  if (Test-Path $dest) { Remove-Item $dest -Recurse -Force }
  New-Item -ItemType Directory -Force $dest | Out-Null
  $log = Join-Path $Captures "$folder.log"
  $a = @("`"$Project`"", '-game', "-ExecCmds=`"$exec`"", '-nosplash', '-windowed', '-ResX=1600', '-ResY=900',
         '-RTSAutoTest=base', "-RTSCapture=$dest", "-abslog=$log", '-unattended', "-RTSScenario=$scenario", "-$Tag")
  "=== $folder ($scenario) $(Get-Date -Format HH:mm:ss)"
  $p = Start-Process $Exe -ArgumentList $a -PassThru
  $t0 = Get-Date; $touched = Get-Date
  while (-not $p.HasExited) {
    Start-Sleep 5
    if (((Get-Date) - $touched).TotalMinutes -ge 4) { Touch; $touched = Get-Date }
    if (((Get-Date) - $t0).TotalMinutes -ge 14) { "tempo esgotado: fecho o jogo pelo PID $($p.Id)"; Stop-Process -Id $p.Id -Force; break }
  }
  if (Test-Path $log) {
    Select-String -Path $log -Pattern 'RTS TEST (quadros|fim|FALHOU)|recorte do ch|RTS ch.o: peda|landscape.RenderNanite|SliceGround.*(rror|arning)|Failed to compile' |
      ForEach-Object { $_.Line -replace '^\[[^\]]+\]\[\s*\d+\]', '' } | Select-Object -First 25
  }
}

while ($true) { & powershell -NoProfile -ExecutionPolicy Bypass -File $Q enter -Owner $Owner -Tag $Tag | Out-Null; if ($LASTEXITCODE -eq 0) { break }; Start-Sleep 30 }
"vez da placa: $(Get-Date -Format HH:mm:ss)"
try {
  while (Get-Process -Name UnrealEditor -ErrorAction SilentlyContinue) { Start-Sleep -Seconds 15 }
  $orig = Join-Path $snap 'orig.uasset'
  if ((Hash $Master) -ne (Hash $orig)) {
    # o mestre tem de começar como o original (o retrato de 04/11 00:14); se não, paro sem mexer
    "o mestre NAO esta igual ao original ($(Hash $Master) x $(Hash $orig)): paro"
    return
  }
  switch ($Phase) {
    'build' {
      "=== probe"; Py 'probe'
      "=== build"; Py 'build'
      Swap 'orig'
      "=== check (o desfazer)"; Py 'check'
    }
    'zero' { Swap 'zero'; RunGame 't0p_zero' 't0p_zero' '' }
    'corte' { Swap 'cut'; RunGame 't0p_corte' 't0p_corte' '' }
    'custo' {
      Swap 'cut'; RunGame 't0p_custo' 't0p_custo_B1' ''; Touch
      Swap 'orig'; RunGame 't0p_custo' 't0p_custo_A1' ''; Touch
      Swap 'cut'; RunGame 't0p_custo' 't0p_custo_B2' ''; Touch
      Swap 'orig'; RunGame 't0p_custo' 't0p_custo_A2' ''
    }
    'final' {
      "=== check (o desfazer)"; Py 'check'
      RunGame 't0_corte' 't0p_desfeito' ''
    }
    'custo2' {
      # (a 2ª rodada do custo, 90 s cada, na ordem contrária: A, B, A, B; o original fica)
      Swap 'orig'; RunGame 't0p_custo_longo' 't0p_custo2_A1' ''; Touch
      Swap 'cut'; RunGame 't0p_custo_longo' 't0p_custo2_B1' ''; Touch
      Swap 'orig'; RunGame 't0p_custo_longo' 't0p_custo2_A2' ''; Touch
      Swap 'cut'; RunGame 't0p_custo_longo' 't0p_custo2_B2' ''; Touch
      Swap 'orig'
      "=== check (o desfazer)"; Py 'check'
    }
    'resto' {
      # (a fila anda devagar: o controle sem o PDO, o corte, o A/B do custo e o desfazer numa vez só, ~20 min)
      "=== build_nopdo (o controle: a máscara sem o PDO)"; Py 'build_nopdo'; Swap 'orig'; Touch
      if (Test-Path (Join-Path $snap 'zeronopdo.uasset')) { Swap 'zeronopdo'; RunGame 't0p_zero' 't0p_zero_sempdo' ''; Touch }
      Swap 'cut'; RunGame 't0p_corte' 't0p_corte' ''; Touch
      Swap 'cut'; RunGame 't0p_custo' 't0p_custo_B1' ''; Touch
      Swap 'orig'; RunGame 't0p_custo' 't0p_custo_A1' ''; Touch
      Swap 'cut'; RunGame 't0p_custo' 't0p_custo_B2' ''; Touch
      Swap 'orig'; RunGame 't0p_custo' 't0p_custo_A2' ''; Touch
      "=== check (o desfazer)"; Py 'check'
      RunGame 't0_corte' 't0p_desfeito' ''
    }
  }
} finally {
  if ((Test-Path (Join-Path $snap 'orig.uasset')) -and ((Hash $Master) -ne (Hash (Join-Path $snap 'orig.uasset')))) {
    while (Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe'" | Where-Object { $_.CommandLine -match $Tag }) { Start-Sleep 5 }
    Swap 'orig'
  }
  "mestre no fim da vez: $(Hash $Master) (original $(Hash (Join-Path $snap 'orig.uasset')))"
  & powershell -NoProfile -ExecutionPolicy Bypass -File $Q leave -Owner $Owner | Out-Null
  "saiu da fila $(Get-Date -Format HH:mm:ss)"
}
