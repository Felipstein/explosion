# O menu nativo no jogo: na vez da fila, sem compilação de outra sessão e sem Unreal nenhuma segurando os arquivos do jogo (o link
# falha com o .dll aberto e a UBT já apagou o velho: foi o "missing module" de 03/11), guarda a versão que está no plugin em prev\,
# põe os arquivos do rascunho, compila e, se falhar, volta a de prev\. Log em deploy.log.
# -Src: a pasta com os fontes editados (o padrão é a pasta deste script)
param([string]$Src = '')
$Q = 'C:\Users\luisf\.gpu_queue\gpu_queue.ps1'
$Owner = if ($env:RTS_QUEUE_OWNER) { $env:RTS_QUEUE_OWNER } else { 'Telas | compilar o menu nativo' }   # RTS_QUEUE_OWNER: a corrente que já está com a vez
$NS = if ($Src) { $Src } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
$Log = Join-Path $NS 'deploy.log'
$P = 'C:\Users\luisf\explosion\Game\Plugins\RTSShell\Source\RTSShell'
$G = 'C:\Users\luisf\explosion\Game'
$Files = [ordered]@{ 'SRTSMenu.h' = "$P\Private\SRTSMenu.h"; 'SRTSMenu.cpp' = "$P\Private\SRTSMenu.cpp"; 'RTSShell.cpp' = "$P\Private\RTSShell.cpp";
  'RTSShellUI.h' = "$P\Private\RTSShellUI.h"; 'RTSShellUI.cpp' = "$P\Private\RTSShellUI.cpp";
  'RTSShellAudio.h' = "$P\Private\RTSShellAudio.h"; 'RTSShellAudio.cpp' = "$P\Private\RTSShellAudio.cpp";
  'SRTSSkirmish.h' = "$P\Private\SRTSSkirmish.h"; 'SRTSSkirmish.cpp' = "$P\Private\SRTSSkirmish.cpp"; 'RTSShell.Build.cs' = "$P\RTSShell.Build.cs";
  'RTSShellBridge.h' = "$P\Public\RTSShellBridge.h"; 'SRTSMultiplayer.h' = "$P\Private\SRTSMultiplayer.h"; 'SRTSMultiplayer.cpp' = "$P\Private\SRTSMultiplayer.cpp";
  'SRTSOptions.h' = "$P\Private\SRTSOptions.h"; 'SRTSOptions.cpp' = "$P\Private\SRTSOptions.cpp";
  'SRTSScript.h' = "$P\Private\SRTSScript.h"; 'SRTSScript.cpp' = "$P\Private\SRTSScript.cpp";
  'SRTSMatch.h' = "$P\Private\SRTSMatch.h"; 'SRTSMatch.cpp' = "$P\Private\SRTSMatch.cpp"; 'RTSShell.h' = "$P\Public\RTSShell.h" }   # (06/11) as telas da partida
$Prev = Join-Path $NS 'prev'
New-Item -ItemType Directory -Force $Prev | Out-Null
"inicio $(Get-Date -Format s)" | Out-File $Log -Encoding utf8
do { powershell -NoProfile -File $Q enter -Owner $Owner -MaxWaitMin 8 | Out-Null } while ($LASTEXITCODE -ne 0)
"a vez e minha $(Get-Date -Format s)" | Add-Content $Log
$job = Start-Job { param($Q, $O) while ($true) { Start-Sleep 240; powershell -NoProfile -File $Q touch -Owner $O | Out-Null } } -ArgumentList $Q, $Owner
try {
  $busy = $true
  for ($i = 0; $i -lt 60; $i++) {
    $ubt = Get-CimInstance Win32_Process -Filter "Name='dotnet.exe'" | Where-Object { $_.CommandLine -like '*UnrealBuildTool*RTSGame*' }
    $ue = Get-CimInstance Win32_Process | Where-Object { $_.Name -like 'UnrealEditor*' -and $_.CommandLine -like '*RTSGame.uproject*' }
    if (-not $ubt -and -not $ue) { $busy = $false; break }
    if ($i -eq 0) { "esperando: $((@($ubt) + @($ue) | ForEach-Object { "$($_.Name) $($_.ProcessId)" }) -join ', ')" | Add-Content $Log }
    Start-Sleep 10
  }
  if ($busy) { "desisti: o jogo ficou aberto 10 min (nada mudou no plugin)" | Add-Content $Log; return }
  foreach ($k in $Files.Keys) { if (Test-Path $Files[$k]) { Copy-Item $Files[$k] (Join-Path $Prev $k) -Force } else { Remove-Item (Join-Path $Prev $k) -ErrorAction SilentlyContinue } }
  foreach ($k in $Files.Keys) { Copy-Item (Join-Path $NS $k) $Files[$k] -Force; (Get-Item $Files[$k]).LastWriteTime = Get-Date }
  # as linhas das Telas nos arquivos compartilhados (o mapa do menu, a tela de carregamento dele): troca exata, só agora
  python 'C:\Users\luisf\explosion\Game\Plugins\RTSShell\Tools\patch_shared.py' apply 2>&1 | Add-Content $Log
  "aplicado $(Get-Date -Format s)" | Add-Content $Log
  $UE = 'C:\Program Files\Epic Games\UE_5.8\Engine'
  Push-Location (Join-Path $UE 'Source')
  & (Join-Path $UE 'Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe') (Join-Path $UE 'Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll') RTSGameEditor Win64 Development "-Project=$G\RTSGame.uproject" -NoMutex -NoHotReload -NoHotReloadFromIDE -MaxParallelActions=1 "-Log=$G\Saved\Logs\UBT-RTSGame.txt" 2>&1 | Add-Content $Log
  $code = $LASTEXITCODE
  Pop-Location
  $ok = ($code -eq 0) -and -not (Select-String -Path $Log -Pattern 'Result: Failed|error C\d+|error LNK' -Quiet)
  "compilou: $ok $(Get-Date -Format s)" | Add-Content $Log
  if (-not $ok) {
    # o que não existia antes (arquivo novo) sai; o resto volta
    foreach ($k in $Files.Keys) { if (Test-Path (Join-Path $Prev $k)) { Copy-Item (Join-Path $Prev $k) $Files[$k] -Force; (Get-Item $Files[$k]).LastWriteTime = Get-Date } else { Remove-Item $Files[$k] -ErrorAction SilentlyContinue } }
    python 'C:\Users\luisf\explosion\Game\Plugins\RTSShell\Tools\patch_shared.py' revert 2>&1 | Add-Content $Log
    "voltei o plugin pra versao de antes (prev); compilo de novo pra o jogo nao ficar sem o modulo" | Add-Content $Log
    Push-Location (Join-Path $UE 'Source')
    & (Join-Path $UE 'Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe') (Join-Path $UE 'Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll') RTSGameEditor Win64 Development "-Project=$G\RTSGame.uproject" -NoMutex -NoHotReload -NoHotReloadFromIDE -MaxParallelActions=1 "-Log=$G\Saved\Logs\UBT-RTSGame.txt" 2>&1 | Add-Content $Log
    "recompilou a de antes: codigo $LASTEXITCODE" | Add-Content $Log
    Pop-Location
  }
} finally {
  Stop-Job $job -ErrorAction SilentlyContinue; Remove-Job $job -Force -ErrorAction SilentlyContinue
  if (-not $env:RTS_QUEUE_OWNER) { powershell -NoProfile -File $Q leave -Owner $Owner | Out-Null }
  "fim $(Get-Date -Format s)" | Add-Content $Log
}
