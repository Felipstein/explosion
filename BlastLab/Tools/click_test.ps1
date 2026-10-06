# Teste do fluxo real do simulador: abre a janela, espera ficar pronta e dá um
# clique de mouse DE VERDADE no terreno (Windows SendInput), como o usuário.
# O simulador (-AutoWatch) fotografa a explosão que o clique detonou e fecha.
#   powershell -File BlastLab\Tools\click_test.ps1 [pasta] [x] [y]
# x, y: ponto do clique em frações da área da janela (padrão: terreno à esquerda do painel).
param([string]$Folder = 'click_test', [double]$X = 0.36, [double]$Y = 0.62)
$ErrorActionPreference = 'Stop'
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class Win {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
}
'@
$UE = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$Project = (Resolve-Path (Join-Path $PSScriptRoot '..\BlastLab.uproject')).Path
$Captures = Join-Path (Split-Path (Split-Path $Project)) 'captures\ue'
$dest = Join-Path $Captures $Folder
if (Test-Path $dest) { Remove-Item $dest -Recurse -Force }
New-Item -ItemType Directory -Force $dest | Out-Null
$log = Join-Path $Captures "$Folder.log"
if (Test-Path $log) { Remove-Item -LiteralPath $log -Force }
$p = Start-Process $UE -PassThru -ArgumentList @("`"$Project`"", '/Game/Maps/TestRange', '-game', '-windowed', '-ResX=1600', '-ResY=900',
  '-ExecCmds="Scalability cine"', '-nosplash', "-AutoWatch=$dest", "-abslog=$log", '-unattended')
# espera o simulador ficar pronto (shaders + aquecimento)
$t0 = Get-Date
while (-not ((Test-Path $log) -and (Select-String -Path $log -Pattern 'AUTOTEST ready for click' -Quiet))) {
  if ($p.HasExited -or ((Get-Date) - $t0).TotalSeconds -gt 240) { throw 'o simulador não ficou pronto' }
  Start-Sleep -Milliseconds 500
}
$p.Refresh()
$h = $p.MainWindowHandle
[Win]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 400
$r = New-Object Win+RECT
[Win]::GetClientRect($h, [ref]$r) | Out-Null
$pt = New-Object Win+POINT
$pt.X = [int](($r.R - $r.L) * $X); $pt.Y = [int](($r.B - $r.T) * $Y)
[Win]::ClientToScreen($h, [ref]$pt) | Out-Null
[Win]::SetCursorPos($pt.X, $pt.Y) | Out-Null
Start-Sleep -Milliseconds 300
[Win]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)   # botão esquerdo pressionado
Start-Sleep -Milliseconds 80
[Win]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)   # solto
"clique em ($($pt.X), $($pt.Y)) da tela"
$p.WaitForExit(240000) | Out-Null
Select-String -Path $log -Pattern 'AUTOTEST (click|shot|mean|no click)' | ForEach-Object { $_.Line -replace '^\[[^\]]+\]\[\s*\d+\]', '' }
