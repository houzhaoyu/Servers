
$extensions = "*.h", "*.cpp", "*.cc"


$files = Get-ChildItem -Recurse -Include $extensions

foreach ($file in $files) {
    Write-Host "converting: $($file.FullName)"
    
    $content = Get-Content -Path $file.FullName -Raw
    
    $utf8WithBom = New-Object System.Text.UTF8Encoding($true)
    [System.IO.File]::WriteAllLines($file.FullName, $content, $utf8WithBom)
}

Write-Host "finished" -ForegroundColor Green