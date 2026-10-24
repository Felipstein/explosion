# (23/10, noite da v4) atalho pros agentes: o cc_check (confere se os .cpp compilam com o mesmo cl.exe e o PCH da última
# compilação, sem tocar no Binaries/Intermediate; o .obj vai pro scratchpad). Sem -Mirror, confere o próprio repositório.
#   cc_check.ps1 -Module RTSGame -Files RTSMatchSubsystem.cpp,RTSHUDNew.cpp
param([string]$Mirror = 'C:\Users\luisf\explosion', [string]$Module = 'RTSGame', [Parameter(Mandatory = $true)][string]$Files, [string]$Extra = '')
$a = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', 'C:\Users\luisf\AppData\Local\Temp\claude\C--Users-luisf-explosion\3735152e-6003-42f7-b4b0-4c40bb937acb\scratchpad\v4\cc_check.ps1',
  '-Mirror', $Mirror, '-Module', $Module, '-Files', $Files)
if ($Extra) { $a += '-Extra', $Extra }
& powershell @a
exit $LASTEXITCODE
