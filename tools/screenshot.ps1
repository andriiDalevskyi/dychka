# Launches the standalone (if not running), waits for its window and saves the client area as PNG.
# Usage: powershell -File tools/screenshot.ps1 -Out docs/img/ui_full.png [-Exe build\Dychka_artefacts\Release\Standalone\Dychka.exe] [-Wait 4] [-Keep] [-NoLaunch]
# Note: run it from an interactive shell. From a sandboxed / non-interactive shell the default WASAPI
# device never opens and the window is not created; a %APPDATA%\Dychka\Dychka.settings file that
# selects DirectSound ("Primary Sound Driver") works around that.
param(
    [string]$Exe = "build\Dychka_artefacts\Release\Standalone\Dychka.exe",
    [string]$Out = "docs\img\ui_full.png",
    [int]$Wait = 4,
    [switch]$Keep,
    [switch]$NoLaunch
)
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class Win32 {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hWnd, out RECT rect);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr hWnd, ref POINT pt);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hWnd, IntPtr after, int x, int y, int cx, int cy, uint flags);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  // The first visible top-level window of the process whose title matches (Process.MainWindowHandle is unreliable here).
  public static IntPtr Find(uint pid, string title) {
    IntPtr found = IntPtr.Zero;
    EnumWindows((h, l) => {
      uint p; GetWindowThreadProcessId(h, out p);
      if (p != pid || !IsWindowVisible(h)) return true;
      var t = new StringBuilder(256); GetWindowText(h, t, 256);
      if (t.ToString() == title) { found = h; return false; }
      return true;
    }, IntPtr.Zero);
    return found;
  }
}
"@
[Win32]::SetProcessDPIAware() | Out-Null   # physical pixels, so a 150 % desktop is captured whole
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$exePath = Join-Path $root $Exe
$outPath = Join-Path $root $Out
$proc = Get-Process -Name "Dychka" -ErrorAction SilentlyContinue | Select-Object -First 1
$launched = $false
if (-not $proc -and -not $NoLaunch) {
    $proc = Start-Process -FilePath $exePath -PassThru
    $launched = $true
    Start-Sleep -Seconds $Wait
}
if (-not $proc) { throw "Dychka is not running" }
$h = [IntPtr]::Zero
for ($i = 0; $i -lt 20 -and $h -eq [IntPtr]::Zero; $i++) {
    $h = [Win32]::Find([uint32]$proc.Id, "Dychka")
    if ($h -eq [IntPtr]::Zero) { Start-Sleep -Milliseconds 500 }
}
if ($h -eq [IntPtr]::Zero) { throw "Dychka window not found" }
[Win32]::ShowWindow($h, 9) | Out-Null
[Win32]::SetWindowPos($h, [IntPtr]::Zero, 40, 40, 0, 0, 0x0001 -bor 0x0040) | Out-Null   # SWP_NOSIZE | SWP_SHOWWINDOW
[Win32]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 900
$rect = New-Object Win32+RECT
[Win32]::GetClientRect($h, [ref]$rect) | Out-Null
$pt = New-Object Win32+POINT
$pt.X = 0; $pt.Y = 0
[Win32]::ClientToScreen($h, [ref]$pt) | Out-Null
$w = $rect.Right - $rect.Left; $hgt = $rect.Bottom - $rect.Top
if ($w -le 0 -or $hgt -le 0) { throw "empty client rect" }
$bmp = New-Object System.Drawing.Bitmap $w, $hgt
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($pt.X, $pt.Y, 0, 0, (New-Object System.Drawing.Size $w, $hgt))
$g.Dispose()
New-Item -ItemType Directory -Force (Split-Path -Parent $outPath) | Out-Null
# The editor is 1100 px wide in logical pixels; a scaled desktop renders it larger. Bring the
# capture back to the logical size so every manual image has the same geometry.
if ($w -gt 1100) {
    $scale = 1100 / $w
    $tw = 1100; $th = [int][Math]::Round($hgt * $scale)
    $small = New-Object System.Drawing.Bitmap $tw, $th
    $g2 = [System.Drawing.Graphics]::FromImage($small)
    $g2.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g2.DrawImage($bmp, 0, 0, $tw, $th)
    $g2.Dispose()
    $bmp.Dispose()
    $bmp = $small
    $w = $tw; $hgt = $th
}
$bmp.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Host "saved $outPath ($w x $hgt)"
if ($launched -and -not $Keep) { Stop-Process -Id $proc.Id -Force }
