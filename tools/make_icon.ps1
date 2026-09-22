# Renders resources/icon_512.png and icon_64.png: the Dychka mark (three saw-tooth time ramps
# with teal drop lines) on the plug-in's dark rounded square. No external tools needed.
# Usage: powershell -File tools/make_icon.ps1
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$outDir = Join-Path $root "resources"
New-Item -ItemType Directory -Force $outDir | Out-Null

function RoundedRect([float]$x, [float]$y, [float]$w, [float]$h, [float]$r) {
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = $r * 2
    $p.AddArc($x, $y, $d, $d, 180, 90)
    $p.AddArc($x + $w - $d, $y, $d, $d, 270, 90)
    $p.AddArc($x + $w - $d, $y + $h - $d, $d, $d, 0, 90)
    $p.AddArc($x, $y + $h - $d, $d, $d, 90, 90)
    $p.CloseFigure()
    return $p
}

$size = 512
$bmp = New-Object System.Drawing.Bitmap $size, $size
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.Clear([System.Drawing.Color]::Transparent)

# body: rounded square with the panel gradient and the outer border
$body = RoundedRect 8 8 ($size - 16) ($size - 16) 84
$grad = New-Object System.Drawing.Drawing2D.LinearGradientBrush ([System.Drawing.Point]::new(0, 0)), ([System.Drawing.Point]::new(0, $size)), ([System.Drawing.ColorTranslator]::FromHtml("#2a2e34")), ([System.Drawing.ColorTranslator]::FromHtml("#15171a"))
$g.FillPath($grad, $body)
$border = New-Object System.Drawing.Pen ([System.Drawing.ColorTranslator]::FromHtml("#3d424a")), 4
$g.DrawPath($border, $body)

# mark: three teeth. Each ramps from "live" (top) down to two bars back (bottom) and snaps back.
$amber = [System.Drawing.ColorTranslator]::FromHtml("#f2a33a")
$amberLight = [System.Drawing.ColorTranslator]::FromHtml("#ffd08a")
$teal = [System.Drawing.ColorTranslator]::FromHtml("#38c7c1")
$left = 92; $right = $size - 92; $top = 150; $bottom = 372
$teeth = 3
$w = ($right - $left) / $teeth
for ($t = 0; $t -lt $teeth; $t++) {
    $x0 = $left + $t * $w + 6
    $x1 = $x0 + $w - 24
    $col = if ($t % 2 -eq 0) { $amber } else { $amberLight }
    $pen = New-Object System.Drawing.Pen $col, 26
    $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $g.DrawLine($pen, [float]$x0, [float]$top, [float]$x1, [float]$bottom)
    $drop = New-Object System.Drawing.Pen $teal, 14
    $drop.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $drop.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $g.DrawLine($drop, [float]($x1 + 8), [float]($top - 6), [float]($x1 + 8), [float]($bottom + 6))
}
$g.Dispose()
$out512 = Join-Path $outDir "icon_512.png"
$bmp.Save($out512, [System.Drawing.Imaging.ImageFormat]::Png)

$small = New-Object System.Drawing.Bitmap 64, 64
$g2 = [System.Drawing.Graphics]::FromImage($small)
$g2.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g2.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g2.DrawImage($bmp, 0, 0, 64, 64)
$g2.Dispose()
$out64 = Join-Path $outDir "icon_64.png"
$small.Save($out64, [System.Drawing.Imaging.ImageFormat]::Png)
$small.Dispose()
$bmp.Dispose()
Write-Host "wrote $out512 and $out64"
