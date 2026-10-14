# Opera a janela do EmberGen de fora (ele não tem linha de comando nem script): abre um .ember, fotografa SÓ a
# janela dele (PrintWindow, nunca a área de trabalho), clica e digita com o SendInput, como uma pessoa.
#
#   eg.ps1 open  -File <arquivo.ember>        # abre o EmberGen com o arquivo (anota o PID em captures/embergen/_eg_pid.txt)
#   eg.ps1 shot  -Out <png>                   # foto da janela do EmberGen
#   eg.ps1 click -X <px> -Y <px> [-Right] [-Double]   # clique em pixels da área da janela (os da foto); mexe no mouse de verdade
#   eg.ps1 pclick -X <px> -Y <px> [-Double]   # o mesmo clique por mensagem pra janela: não mexe no mouse nem rouba o foco
#   eg.ps1 drag  -X <px> -Y <px> -X2 <px> -Y2 <px>
#   eg.ps1 type  -Text <texto>                # digita (teclado)
#   eg.ps1 key   -Text <ENTER|ESC|TAB|CTRL+S|...>  # tecla (SendKeys)
#   eg.ps1 close                              # fecha pelo PID que este script abriu
param(
  [Parameter(Mandatory = $true)][string]$Command,
  [string]$File = '', [string]$Out = '', [string]$Text = '',
  [int]$X = 0, [int]$Y = 0, [int]$X2 = 0, [int]$Y2 = 0,
  [switch]$Right, [switch]$Double
)
$ErrorActionPreference = 'Stop'
$Exe = 'C:\Program Files\JangaFX\EmberGen\EmberGen.exe'
$PidFile = Join-Path (Split-Path (Split-Path $PSScriptRoot)) 'captures\embergen\_eg_pid.txt'
Add-Type -AssemblyName System.Drawing, System.Windows.Forms
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class EgWin {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
}
'@

function Get-EgProcess {
  if (-not (Test-Path $PidFile)) { throw 'o EmberGen não foi aberto por este script (sem _eg_pid.txt)' }
  $id = [int](Get-Content $PidFile)
  $p = Get-Process -Id $id -ErrorAction SilentlyContinue
  if (-not $p) { throw "o EmberGen do PID $id não está aberto" }
  $p.Refresh()
  return $p
}
function Focus($h) { [EgWin]::ShowWindow($h, 9) | Out-Null; [EgWin]::SetForegroundWindow($h) | Out-Null; Start-Sleep -Milliseconds 250 }
function ToScreen($h, [int]$cx, [int]$cy) {
  $pt = New-Object EgWin+POINT; $pt.X = $cx; $pt.Y = $cy
  [EgWin]::ClientToScreen($h, [ref]$pt) | Out-Null
  return $pt
}

switch ($Command) {
  'open' {
    $running = Get-Process EmberGen -ErrorAction SilentlyContinue
    if ($running) { throw "já há um EmberGen aberto (PID $($running.Id -join ', ')): pode ser de outra sessão; não mexo" }
    $p = Start-Process $Exe -PassThru -ArgumentList "`"$File`"" -WorkingDirectory (Split-Path $Exe)
    New-Item -ItemType Directory -Force (Split-Path $PidFile) | Out-Null
    Set-Content $PidFile $p.Id
    $t0 = Get-Date
    while ($true) {
      $p.Refresh()
      if ($p.MainWindowHandle -ne 0) { break }
      if (((Get-Date) - $t0).TotalSeconds -gt 90) { throw 'a janela do EmberGen não apareceu' }
      Start-Sleep -Milliseconds 500
    }
    Write-Host "ok PID $($p.Id)"
  }
  'shot' {
    $p = Get-EgProcess; $h = $p.MainWindowHandle
    $r = New-Object EgWin+RECT
    [EgWin]::GetClientRect($h, [ref]$r) | Out-Null
    $w = $r.R - $r.L; $hh = $r.B - $r.T
    $bmp = New-Object System.Drawing.Bitmap $w, $hh
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    # 3 = PW_CLIENTONLY | PW_RENDERFULLCONTENT (janela de GPU)
    [EgWin]::PrintWindow($h, $hdc, 3) | Out-Null
    $g.ReleaseHdc($hdc); $g.Dispose()
    $bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
    Write-Host "ok $Out ${w}x$hh"
  }
  'click' {
    $p = Get-EgProcess; $h = $p.MainWindowHandle; Focus $h
    $pt = ToScreen $h $X $Y
    [EgWin]::SetCursorPos($pt.X, $pt.Y) | Out-Null; Start-Sleep -Milliseconds 120
    $down = if ($Right) { 0x0008 } else { 0x0002 }; $up = if ($Right) { 0x0010 } else { 0x0004 }
    $n = if ($Double) { 2 } else { 1 }
    for ($i = 0; $i -lt $n; $i++) {
      [EgWin]::mouse_event($down, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 60
      [EgWin]::mouse_event($up, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 80
    }
    Write-Host "ok click $X,$Y"
  }
  'pclick' {
    # o clique por mensagem pra janela (não mexe no mouse de verdade nem rouba o foco)
    $p = Get-EgProcess; $h = $p.MainWindowHandle
    $lp = [IntPtr](($Y -shl 16) -bor ($X -band 0xFFFF))
    [EgWin]::PostMessage($h, 0x0200, [IntPtr]::Zero, $lp) | Out-Null; Start-Sleep -Milliseconds 80     # WM_MOUSEMOVE
    $n = if ($Double) { 2 } else { 1 }
    for ($i = 0; $i -lt $n; $i++) {
      [EgWin]::PostMessage($h, 0x0201, [IntPtr]1, $lp) | Out-Null; Start-Sleep -Milliseconds 60    # WM_LBUTTONDOWN
      [EgWin]::PostMessage($h, 0x0202, [IntPtr]::Zero, $lp) | Out-Null; Start-Sleep -Milliseconds 80 # WM_LBUTTONUP
    }
    Write-Host "ok pclick $X,$Y"
  }
  'drag' {
    $p = Get-EgProcess; $h = $p.MainWindowHandle; Focus $h
    $a = ToScreen $h $X $Y; $b = ToScreen $h $X2 $Y2
    [EgWin]::SetCursorPos($a.X, $a.Y) | Out-Null; Start-Sleep -Milliseconds 120
    [EgWin]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 100
    for ($i = 1; $i -le 20; $i++) {
      [EgWin]::SetCursorPos([int]($a.X + ($b.X - $a.X) * $i / 20), [int]($a.Y + ($b.Y - $a.Y) * $i / 20)) | Out-Null
      Start-Sleep -Milliseconds 15
    }
    [EgWin]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
    Write-Host "ok drag"
  }
  'type' {
    $p = Get-EgProcess; Focus $p.MainWindowHandle
    [System.Windows.Forms.SendKeys]::SendWait(($Text -replace '([+^%~(){}\[\]])', '{$1}'))
    Write-Host 'ok type'
  }
  'key' {
    $p = Get-EgProcess; Focus $p.MainWindowHandle
    $map = @{ 'ENTER' = '{ENTER}'; 'ESC' = '{ESC}'; 'TAB' = '{TAB}'; 'DEL' = '{DEL}'; 'BKSP' = '{BACKSPACE}'; 'SPACE' = ' '; 'CTRL+S' = '^s'; 'CTRL+A' = '^a'; 'CTRL+O' = '^o' }
    $k = if ($map.ContainsKey($Text)) { $map[$Text] } else { $Text }
    [System.Windows.Forms.SendKeys]::SendWait($k)
    Write-Host "ok key $Text"
  }
  'close' {
    $p = Get-EgProcess
    Stop-Process -Id $p.Id
    Remove-Item $PidFile -ErrorAction SilentlyContinue
    Write-Host "fechado PID $($p.Id)"
  }
  default { throw "comando desconhecido: $Command" }
}
