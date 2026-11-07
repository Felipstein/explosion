# Roda o import_music.py (a trilha da partida como assets do jogo) na Unreal sem janela, na vez da fila. Log em captures/musica/import_music.log.
#   powershell -File Game/Tools/music/import_music.ps1 [-Ids a,b]
param([string]$Ids = '')
$Q = 'C:\Users\luisf\.gpu_queue\gpu_queue.ps1'
# (06/11) o dono leva as faixas: duas importações com o mesmo nome dividiam o mesmo bilhete e uma passava na vaga da outra
$Owner = if ($Ids) { 'Trilha | importar ' + ((($Ids -split ',') | ForEach-Object { $_ -replace '[^A-Za-z0-9_]', '' }) -join ' ') } else { 'Trilha | importar as faixas da partida' }
if ($Owner.Length -gt 120) { $Owner = $Owner.Substring(0, 120) }
$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
$Root = (Resolve-Path (Join-Path $Here '..\..\..')).Path
$Log = Join-Path $Root 'captures\musica\import_music.log'
$Cmd = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
# (o bilhete que some no meio da leitura de outra sessão não derruba a espera)
do { powershell -NoProfile -File $Q enter -Owner $Owner -Tag 'RTSGame.uproject' 2>$null | Out-Null; $code = $LASTEXITCODE } while ($code -ne 0)
$touch = Start-Job -ScriptBlock { param($q, $o) while ($true) { Start-Sleep 200; powershell -NoProfile -File $q touch -Owner $o | Out-Null } } -ArgumentList $Q, $Owner
try {
  $script = "$Here\import_music.py"
  $a = @("`"$Root\Game\RTSGame.uproject`"", '-run=pythonscript', "-script=`"$script$(if ($Ids) { " -ids=$Ids" })`"", '-nullrhi', '-unattended', '-nosplash', "-abslog=`"$Log`"")
  $p = Start-Process $Cmd -ArgumentList $a -PassThru
  if (-not $p.WaitForExit(1200000)) { Stop-Process -Id $p.Id -Force; 'FALHA: passou de 20 min' }
} finally {
  Stop-Job $touch; Remove-Job $touch
  powershell -NoProfile -File $Q leave -Owner $Owner | Out-Null
}
Select-String -Path $Log -Pattern 'trilha|Error|Traceback' | Select-Object -Last 50 | ForEach-Object { $_.Line -replace '^\[[^\]]+\]\[\s*\d+\]', '' }
