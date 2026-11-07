<#
    make-icon.ps1 - generate assets/app.ico from original geometry.

    The ICO holds one 32bpp DIB (BITMAPINFOHEADER + BGRA pixels + AND mask)
    per size so the Windows resource compiler accepts it.

    Geometry (original to DriverNest): a rounded "nest" ring with a SoC chip
    inside it and four contact pins.
#>
[CmdletBinding()]
param(
    [string]$OutFile = ''
)

if ([string]::IsNullOrWhiteSpace($OutFile)) {
    $scriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
    $OutFile = Join-Path $scriptDir '..\assets\app.ico'
}

Add-Type -AssemblyName System.Drawing

$sizes = @(16, 24, 32, 48, 256)

# Brand colours: #2F7DE1 accent, white chip.
$brand = [System.Drawing.Color]::FromArgb(255, 0x2F, 0x7D, 0xE1)
$chip = [System.Drawing.Color]::White

function New-IconBitmap([int]$size) {
    $bmp = New-Object System.Drawing.Bitmap($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.Clear([System.Drawing.Color]::Transparent)

    $s = [double]$size
    $pad = $s * 0.06
    $ring = New-Object System.Drawing.RectangleF($pad, $pad, ($s - 2 * $pad), ($s - 2 * $pad))

    # Nest ring: thick rounded outline.
    $penWidth = [Math]::Max(1.0, $s * 0.10)
    $pen = New-Object System.Drawing.Pen($brand, $penWidth)
    $radius = $ring.Width * 0.28
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = 2 * $radius
    $path.AddArc($ring.X, $ring.Y, $d, $d, 180, 90)
    $path.AddArc(($ring.Right - $d), $ring.Y, $d, $d, 270, 90)
    $path.AddArc(($ring.Right - $d), ($ring.Bottom - $d), $d, $d, 0, 90)
    $path.AddArc($ring.X, ($ring.Bottom - $d), $d, $d, 90, 90)
    $path.CloseFigure()
    $g.DrawPath($pen, $path)
    $pen.Dispose()

    # Chip body.
    $chipSize = $s * 0.38
    $chipRect = New-Object System.Drawing.RectangleF((($s - $chipSize) / 2), (($s - $chipSize) / 2), $chipSize, $chipSize)
    $chipBrush = New-Object System.Drawing.SolidBrush($chip)
    $g.FillRectangle($chipBrush, $chipRect)
    $chipBrush.Dispose()

    # Four contact pins on the chip.
    $pinBrush = New-Object System.Drawing.SolidBrush($brand)
    $pinLen = $s * 0.07
    $pinThick = [Math]::Max(1.0, $s * 0.045)
    $cx = $s / 2
    $cy = $s / 2
    $offsets = @((-$chipSize * 0.25), ($chipSize * 0.25))
    foreach ($o in $offsets) {
        $g.FillRectangle($pinBrush, [single]($cx + $o - $pinThick / 2), [single]($chipRect.Top - $pinLen), [single]$pinThick, [single]$pinLen)
        $g.FillRectangle($pinBrush, [single]($cx + $o - $pinThick / 2), [single]$chipRect.Bottom, [single]$pinThick, [single]$pinLen)
        $g.FillRectangle($pinBrush, [single]($chipRect.Left - $pinLen), [single]($cy + $o - $pinThick / 2), [single]$pinLen, [single]$pinThick)
        $g.FillRectangle($pinBrush, [single]($chipRect.Right), [single]($cy + $o - $pinThick / 2), [single]$pinLen, [single]$pinThick)
    }
    $pinBrush.Dispose()

    $g.Dispose()
    return $bmp
}

function Get-IconDib([System.Drawing.Bitmap]$bmp) {
    $w = $bmp.Width
    $h = $bmp.Height

    $ms = New-Object System.IO.MemoryStream
    $bw = New-Object System.IO.BinaryWriter($ms)

    # BITMAPINFOHEADER
    $bw.Write([uint32]40)          # biSize
    $bw.Write([int32]$w)           # biWidth
    $bw.Write([int32]($h * 2))     # biHeight (XOR + AND)
    $bw.Write([uint16]1)           # biPlanes
    $bw.Write([uint16]32)          # biBitCount
    $bw.Write([uint32]0)           # biCompression (BI_RGB)
    $xorBytes = $w * 4 * $h
    $maskRow = [int]([Math]::Ceiling($w / 32.0) * 4)
    $maskBytes = $maskRow * $h
    $bw.Write([uint32]($xorBytes + $maskBytes))
    $bw.Write([int32]0); $bw.Write([int32]0)
    $bw.Write([uint32]0); $bw.Write([uint32]0)

    # XOR pixels: bottom-up rows of BGRA.
    for ($y = $h - 1; $y -ge 0; $y--) {
        for ($x = 0; $x -lt $w; $x++) {
            $c = $bmp.GetPixel($x, $y)
            $bw.Write([byte]$c.B); $bw.Write([byte]$c.G)
            $bw.Write([byte]$c.R); $bw.Write([byte]$c.A)
        }
    }

    # AND mask: all zero (fully determined by the alpha channel).
    $zeros = New-Object byte[] $maskBytes
    $bw.Write($zeros)

    $bw.Flush()
    # Comma prevents PowerShell from enumerating the byte array on return.
    return ,([byte[]]$ms.ToArray())
}

$images = @()
foreach ($size in $sizes) {
    $bmp = New-IconBitmap $size
    $bytes = Get-IconDib $bmp
    $images += [pscustomobject]@{ Size = $size; Bytes = $bytes }
    $bmp.Dispose()
}

$outPath = [System.IO.Path]::GetFullPath($OutFile)
$dir = [System.IO.Path]::GetDirectoryName($outPath)
if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir | Out-Null }

$fs = [System.IO.File]::Create($outPath)
$w = New-Object System.IO.BinaryWriter($fs)
$w.Write([uint16]0)                 # reserved
$w.Write([uint16]1)                 # type = icon
$w.Write([uint16]$images.Count)

$offset = 6 + 16 * $images.Count
foreach ($img in $images) {
    $dim = if ($img.Size -ge 256) { 0 } else { $img.Size }
    $w.Write([byte]$dim)            # width
    $w.Write([byte]$dim)            # height
    $w.Write([byte]0)               # colour count
    $w.Write([byte]0)               # reserved
    $w.Write([uint16]1)             # planes
    $w.Write([uint16]32)            # bit count
    $w.Write([uint32]$img.Bytes.Length)
    $w.Write([uint32]$offset)
    $offset += $img.Bytes.Length
}
foreach ($img in $images) { $w.Write([byte[]]$img.Bytes) }

$w.Flush(); $w.Dispose(); $fs.Dispose()

Write-Host ("Wrote {0} ({1} bytes, {2} sizes)" -f $outPath, (Get-Item $outPath).Length, $images.Count)
