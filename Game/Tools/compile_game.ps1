# (23/10, noite da v4) atalho pros agentes: compila o jogo (game.ps1 compile) pela fila da placa, renovando o bilhete;
# o script de verdade e o log ficam no scratchpad da sessão
& powershell -NoProfile -ExecutionPolicy Bypass -File 'C:\Users\luisf\AppData\Local\Temp\claude\C--Users-luisf-explosion\3735152e-6003-42f7-b4b0-4c40bb937acb\scratchpad\compile_game.ps1'
exit $LASTEXITCODE
