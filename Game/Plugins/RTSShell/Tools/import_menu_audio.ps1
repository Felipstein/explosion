# Roda o import_menu_audio.py (os sons das telas como assets do jogo) na Unreal sem janela, na vez da fila. Log em import_menu_audio.log.
$Q = 'C:\Users\luisf\.gpu_queue\gpu_queue.ps1'
$Owner = 'Telas | importar os sons do menu'
$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
$Log = Join-Path $Here 'import_menu_audio.log'
$Cmd = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
do { powershell -NoProfile -File $Q enter -Owner $Owner -Tag 'RTSGame.uproject' | Out-Null } while ($LASTEXITCODE -ne 0)
try {
  $p = Start-Process $Cmd -ArgumentList @('"C:\Users\luisf\explosion\Game\RTSGame.uproject"', '-run=pythonscript', "-script=`"$Here\import_menu_audio.py`"",
    '-nullrhi', '-unattended', '-nosplash', "-abslog=`"$Log`"") -PassThru
  if (-not $p.WaitForExit(600000)) { Stop-Process -Id $p.Id -Force; 'FALHA: passou de 10 min' }
} finally {
  powershell -NoProfile -File $Q leave -Owner $Owner | Out-Null
}
Select-String -Path $Log -Pattern 'som importado|som das telas|Error|Traceback' | Select-Object -First 12 | ForEach-Object { $_.Line -replace '^\[[^\]]+\]\[\s*\d+\]', '' }
