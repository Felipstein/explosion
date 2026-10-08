# Compila e roda os testes da simulação (C++ puro, sem a Unreal), em três
# compilações diferentes. As três têm de imprimir o mesmo FINAL_HASH: é a prova
# de que o resultado não depende de otimização nem de flag de ponto flutuante.
#
#   powershell -File Game\Tools\simtest.ps1            # testes
#   powershell -File Game\Tools\simtest.ps1 -Scale     # + 800 tanques (tempo por passo)
#   powershell -File Game\Tools\simtest.ps1 -Inspect <replay> -Id 10 [-At 1800]
#       repete uma partida gravada e conta a história de uma unidade (e o mapa em volta dela no passo -At)
param([switch]$Scale, [string]$Inspect, [int]$Id = 1, [int]$At = 0)
$ErrorActionPreference = 'Stop'

$Game = Split-Path -Parent $PSScriptRoot
$Sim = Join-Path $Game 'Source\SimCore'
$Out = Join-Path $Game 'Intermediate\SimTests'
# o compilador do Visual Studio: pelo vswhere quando existe; senão, a instalação mais nova
# (o vswhere some quando se desinstala uma versão antiga do VS)
$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
$vcvars = $null
if (Test-Path $vswhere) {
    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($vs) { $vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat' }
}
if (-not $vcvars -or -not (Test-Path $vcvars)) {
    $vcvars = Get-ChildItem 'C:\Program Files\Microsoft Visual Studio\*\*\VC\Auxiliary\Build\vcvars64.bat' -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
}
if (-not $vcvars -or -not (Test-Path $vcvars)) { throw "vcvars64.bat não encontrado" }

# o módulo da Unreal (SimCoreModule.cpp) fica de fora: o resto é C++ puro
$sources = @(Get-ChildItem (Join-Path $Sim 'Private') -Filter *.cpp | Where-Object { $_.Name -ne 'SimCoreModule.cpp' } | ForEach-Object { "`"$($_.FullName)`"" })

if ($Inspect) {
    $dir = Join-Path $Game 'Intermediate\ReplayInspect'
    New-Item -ItemType Directory -Force $dir | Out-Null
    $exe = Join-Path $dir 'replayinspect.exe'
    $all = $sources + "`"$(Join-Path $Game 'Tests\Sim\ReplayInspect.cpp')`""
    $cl = "cl /nologo /std:c++20 /EHsc /W4 /WX /permissive- /O2 /I`"$Sim\Public`" /Fo`"$dir\\`" /Fe`"$exe`" $($all -join ' ')"
    $log = cmd /c "call `"$vcvars`" >nul && $cl 2>&1"
    if ($LASTEXITCODE -ne 0) {
        $log | Where-Object { $_ -match 'error|warning' } | Select-Object -First 30 | ForEach-Object { Write-Host $_ }
        throw 'compilação do inspetor falhou'
    }
    & $exe (Resolve-Path $Inspect).Path $Id $At
    exit $LASTEXITCODE
}
$sources += "`"$(Join-Path $Game 'Tests\Sim\SimTests.cpp')`""

$variants = [ordered]@{
    'o2_fast'        = '/O2 /fp:fast'
    'od_precise'     = '/Od /fp:precise'
    'o2_strict_avx2' = '/O2 /fp:strict /arch:AVX2'
}

$hashes = @{}
$failed = $false
foreach ($name in $variants.Keys) {
    $dir = Join-Path $Out $name
    New-Item -ItemType Directory -Force $dir | Out-Null
    $exe = Join-Path $dir 'simtests.exe'
    $flags = $variants[$name]
    $cl = "cl /nologo /std:c++20 /EHsc /W4 /WX /permissive- $flags /I`"$Sim\Public`" /Fo`"$dir\\`" /Fe`"$exe`" $($sources -join ' ')"
    $log = cmd /c "call `"$vcvars`" >nul && $cl 2>&1"
    if ($LASTEXITCODE -ne 0) {
        $log | Where-Object { $_ -match 'error|warning' } | Select-Object -First 30 | ForEach-Object { Write-Host $_ }
        throw "compilação $name falhou"
    }
    Write-Host "=== $name ($flags)"
    $args = @()
    if ($Scale -and $name -eq 'o2_fast') { $args += '--scale' }
    $run = & $exe @args
    $run | ForEach-Object { Write-Host $_ }
    if ($LASTEXITCODE -ne 0) { $failed = $true }
    $h = ($run | Where-Object { $_ -match '^FINAL_HASH' }) -replace 'FINAL_HASH ', ''
    $hashes[$name] = $h
}

$distinct = @($hashes.Values | Sort-Object -Unique)
Write-Host ''
if ($distinct.Count -eq 1 -and -not $failed) {
    Write-Host "DETERMINISMO OK: as $($hashes.Count) compilações dão $($distinct[0])"
    exit 0
}
$hashes.GetEnumerator() | ForEach-Object { Write-Host "  $($_.Key): $($_.Value)" }
Write-Host 'FALHOU: compilações diferentes deram resultados diferentes, ou algum teste falhou'
exit 1
