# split_rom_secret.ps1
# Splits the Apple IIgs ROM into base64 chunks small enough for GitHub Secrets (max 65,536 chars each).
# Run from the project root:
#   powershell -ExecutionPolicy Bypass -File split_rom_secret.ps1
#
# Output: saves chunks to rom_secret_part1.txt ... rom_secret_part6.txt (in project root)
# Then add each file's content as a GitHub Secret:
#   IIGS_ROM_V3_BASE64_1, IIGS_ROM_V3_BASE64_2, ... IIGS_ROM_V3_BASE64_6

param(
    [string]$RomPath = "$PSScriptRoot\rom\rom.v3",
    [int]$ChunkSize = 60000   # safely under the 65,536 char GitHub limit
)

$RomPath = (Resolve-Path $RomPath).Path
Write-Host "Reading ROM from: $RomPath"

$bytes = [IO.File]::ReadAllBytes($RomPath)
$b64full = [Convert]::ToBase64String($bytes)
$total = $b64full.Length
$numParts = [Math]::Ceiling($total / $ChunkSize)

Write-Host "ROM size : $($bytes.Length) bytes"
Write-Host "Base64   : $total chars"
Write-Host "Splitting: $numParts parts (chunk size = $ChunkSize)"
Write-Host ""

$outDir = $PSScriptRoot
for ($i = 0; $i -lt $numParts; $i++) {
    $start = $i * $ChunkSize
    $len = [Math]::Min($ChunkSize, $total - $start)
    $chunk = $b64full.Substring($start, $len)
    $idx = $i + 1
    $file = Join-Path $outDir "rom_secret_part$idx.txt"
    [IO.File]::WriteAllText($file, $chunk, [Text.Encoding]::ASCII)
    Write-Host "Part $idx : $($chunk.Length) chars  -->  $file"
}

Write-Host ""
Write-Host "Done! Add each file's content as a GitHub Secret:"
for ($i = 1; $i -le $numParts; $i++) {
    Write-Host "  IIGS_ROM_V3_BASE64_$i  <-- rom_secret_part$i.txt"
}
Write-Host ""
Write-Host "Note: rom_secret_part*.txt files are listed in .gitignore - they will NOT be committed."
