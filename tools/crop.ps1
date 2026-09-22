# Cuts the manual images out of a full standalone capture (tools/screenshot.ps1 output).
# The standalone adds a title bar and the "audio input is muted" banner above the 1100x720 editor;
# -Top is the y of the editor's first row inside the capture.
param(
    [string]$Source = "docs\img\ui_full_raw.png",
    [int]$Top = 57
)
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$src = [System.Drawing.Image]::FromFile((Join-Path $root $Source))
function Crop($x, $y, $w, $h, $name) {
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.DrawImage($src, (New-Object System.Drawing.Rectangle 0, 0, $w, $h), (New-Object System.Drawing.Rectangle $x, $y, $w, $h), [System.Drawing.GraphicsUnit]::Pixel)
    $g.Dispose()
    $out = Join-Path $root "docs\img\$name"
    $bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Host "wrote $out ($w x $h)"
}
Crop 0    $Top         1100 720 "ui_full.png"
Crop 0    $Top         1100 56  "ui_header.png"
Crop 24   ($Top + 74)  300  598 "ui_slots.png"
Crop 339  ($Top + 74)  497  598 "ui_editor.png"
Crop 851  ($Top + 74)  225  598 "ui_controls.png"
Crop 0    ($Top + 692) 1100 28  "ui_footer.png"
$src.Dispose()
