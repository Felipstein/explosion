# As telas novas no jogo: espera a vez na fila, liga tudo de uma vez (apply_shared.py), compila e, se falhar, desfaz
# (pra ninguém ficar com o jogo quebrado). O log fica em compile.log ao lado.
$Q = 'C:\Users\luisf\.gpu_queue\gpu_queue.ps1'
$Owner = 'Telas | compilar o jogo'
$Stage = Split-Path -Parent $MyInvocation.MyCommand.Path
$Log = Join-Path $Stage 'compile.log'
$Py = Join-Path $env:LOCALAPPDATA 'Programs\Python\Python312\python.exe'
"inicio $(Get-Date -Format s)" | Out-File $Log -Encoding utf8
do { powershell -NoProfile -File $Q enter -Owner $Owner -MaxWaitMin 8 | Out-Null; $code = $LASTEXITCODE } while ($code -ne 0)
"a vez e minha $(Get-Date -Format s)" | Add-Content $Log
$job = Start-Job { param($Q, $O) while ($true) { Start-Sleep 240; powershell -NoProfile -File $Q touch -Owner $O | Out-Null } } -ArgumentList $Q, $Owner
$applied = $false
try {
  # nenhuma compilação de outro rodando (o UBT roda dentro do dotnet)
  for ($i = 0; $i -lt 180; $i++) {
    $ubt = Get-CimInstance Win32_Process -Filter "Name='dotnet.exe'" | Where-Object { $_.CommandLine -like '*UnrealBuildTool*' }
    if (-not $ubt) { break }
    Start-Sleep 5
  }
  $env:PYTHONIOENCODING = 'utf-8'
  & $Py (Join-Path $Stage 'apply_shared.py') apply 2>&1 | Add-Content $Log
  $applied = $LASTEXITCODE -eq 0
  if ($applied) {
    & powershell -NoProfile -File 'C:\Users\luisf\explosion\Game\Tools\game.ps1' compile *>&1 | Add-Content $Log
    $ok = ($LASTEXITCODE -eq 0) -and -not (Select-String -Path $Log -Pattern 'Result: Failed|error C\d+|error LNK|BUILD FAILED' -Quiet)
    "compilou: $ok $(Get-Date -Format s)" | Add-Content $Log
    if (-not $ok) { & $Py (Join-Path $Stage 'apply_shared.py') revert 2>&1 | Add-Content $Log }
  }
} finally {
  Stop-Job $job -ErrorAction SilentlyContinue; Remove-Job $job -Force -ErrorAction SilentlyContinue
  powershell -NoProfile -File $Q leave -Owner $Owner | Out-Null
  "fim $(Get-Date -Format s)" | Add-Content $Log
}
