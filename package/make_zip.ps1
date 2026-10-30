# ============================================================
#  把 <Dir> 資料夾壓成 <Zip>，zip 內最上層是資料夾本身（解壓後不會散落一地）
#  由 make_package.bat 呼叫。
#  不用 Compress-Archive／ZipFile.CreateFromDirectory：Windows PowerShell 5.1 前者用系統字碼頁
#  寫中文檔名（解壓變亂碼），後者在 .NET Framework 上用反斜線當路徑分隔（違反 ZIP 規格）。
# ============================================================
param(
    [Parameter(Mandatory)] [string] $Dir,
    [Parameter(Mandatory)] [string] $Zip
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem

$src  = (Resolve-Path $Dir).Path
$root = Split-Path $src -Leaf
$dst  = Join-Path (Resolve-Path (Split-Path $Zip -Parent)).Path (Split-Path $Zip -Leaf)
if (Test-Path $dst) { Remove-Item $dst }

$archive = [IO.Compression.ZipFile]::Open($dst, 'Create', [Text.Encoding]::UTF8)
try {
    foreach ($f in Get-ChildItem $src -File) {
        [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
            $archive, $f.FullName, "$root/$($f.Name)", 'Optimal')
    }
} finally {
    $archive.Dispose()
}
