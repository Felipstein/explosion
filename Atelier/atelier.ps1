# Abre o Atelier: liga o serviço da fila (se ainda não estiver rodando) e abre a janela do app
# (Edge em modo app, sem barra de navegador). O serviço continua rodando com a janela fechada,
# pra as sessões poderem pedir imagem a qualquer hora.
$port = 8140
function Test-Atelier { try { Invoke-RestMethod "http://127.0.0.1:$port/api/health" -TimeoutSec 2 | Out-Null; $true } catch { $false } }

if (-not (Test-Atelier)) {
    Start-Process node -ArgumentList "`"$PSScriptRoot\core\atelier.mjs`"" -WindowStyle Hidden
    for ($i = 0; $i -lt 40 -and -not (Test-Atelier); $i++) { Start-Sleep -Milliseconds 250 }
}
if (-not (Test-Atelier)) { Write-Host "O serviço do Atelier não subiu; veja captures\atelier\atelier.log"; exit 1 }

$edge = "${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe"
Start-Process $edge "--app=http://127.0.0.1:$port/ --window-size=1600,960"
