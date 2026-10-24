# (23/10, noite da v4) atalho pros agentes: o cc_check dos plugins e do RTSShaders (pela .rsp da última compilação do arquivo)
#   cc_check_rsp.ps1 -Rsp <o .cpp.obj.rsp da última compilação> [-Src <o .cpp>]
param([Parameter(Mandatory = $true)][string]$Rsp, [string]$Src = '')
$a = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', 'C:\Users\luisf\AppData\Local\Temp\claude\C--Users-luisf-explosion\3735152e-6003-42f7-b4b0-4c40bb937acb\scratchpad\hitlife\cc_check_rsp.ps1', '-Rsp', $Rsp)
if ($Src) { $a += '-Src', $Src }
& powershell @a
exit $LASTEXITCODE
