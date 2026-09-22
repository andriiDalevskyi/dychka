# Rebuilds both manuals from the HTML sources with headless Edge (no other tools needed).
$edge = "C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
$docs = $PSScriptRoot
foreach ($pair in @(@("manual_en.html", "Dychka_User_Manual_EN.pdf"), @("manual_uk.html", "Dychka_Instrukciya_UA.pdf"))) {
    # -replace takes a REGEX pattern, so a literal backslash must be doubled.
    $src = "file:///" + ($docs -replace '\\', '/') + "/" + $pair[0]
    $out = Join-Path $docs $pair[1]
    Start-Process -FilePath $edge -ArgumentList "--headless=new", "--disable-gpu", "--no-pdf-header-footer", "--user-data-dir=$env:TEMP\edge-pdf", "--print-to-pdf=`"$out`"", "--virtual-time-budget=8000", $src -Wait
    Write-Host "wrote $out"
}
