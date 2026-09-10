# Builda o projeto (via CMake/Ninja, mesmo mecanismo da extensao MPLAB) e depois
# copia o .hex gerado para um arquivo com a versao (variavel "versao" do main.c) no nome.
# Uso: rodar como Task do VS Code ("Build").

$ErrorActionPreference = "Stop"

$RootDir = Split-Path -Parent $PSScriptRoot
$MainC = Join-Path $RootDir "main.c"

if (-not (Test-Path $MainC)) {
    Write-Error "main.c nao encontrado em $RootDir"
    exit 1
}

# ===== 1) Descobrir a config de build (preset) a usar =====
# Ajuste $NomeProjeto/$Config se a variante ativa mudar (ex: outro nome de projeto ou config Debug).
$NomeProjeto = "Carregador_4T_ANTIGA"
$Config = "default.production"

$CmakeDir = Join-Path $RootDir "cmake\$NomeProjeto\$Config"
$PresetsFile = Join-Path $CmakeDir "CMakePresets.json"

if (-not (Test-Path $PresetsFile)) {
    Write-Error "CMakePresets.json nao encontrado em $CmakeDir - confira NomeProjeto/Config no script"
    exit 1
}

$presetName = (Get-Content $PresetsFile | ConvertFrom-Json).configurePresets[0].name
$BinaryDir = Join-Path $RootDir "_build\$NomeProjeto\$Config"

# ===== 2) Configurar (se necessario) e buildar =====
Push-Location $CmakeDir
try {
    if (-not (Test-Path (Join-Path $BinaryDir "build.ninja"))) {
        Write-Host "Configurando ($presetName)..."
        cmake --preset $presetName
        if ($LASTEXITCODE -ne 0) { throw "Falha ao configurar o CMake" }
    }
} finally {
    Pop-Location
}

Write-Host "Buildando..."
cmake --build $BinaryDir
if ($LASTEXITCODE -ne 0) {
    Write-Error "Build falhou - veja o log acima"
    exit 1
}

# ===== 3) Extrair a versao de main.c =====
$match = Select-String -Path $MainC -Pattern 'versao\[\d*\]\s*=\s*"([^"]*)"' | Select-Object -First 1
if (-not $match) {
    Write-Error "Nao foi possivel encontrar a variavel 'versao' em main.c"
    exit 1
}
$Versao = $match.Matches[0].Groups[1].Value

# ===== 4) Copiar o .hex gerado com a versao no nome =====
$OutDir = Join-Path $RootDir "out"
$hex = Get-ChildItem -Path $OutDir -Filter "*.hex" -Recurse |
    Where-Object { $_.Name -notmatch [regex]::Escape($Versao) } |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1

if (-not $hex) {
    Write-Error "Build terminou mas nenhum .hex foi encontrado em $OutDir"
    exit 1
}

$novoNome = "{0}_{1}{2}" -f $hex.BaseName, $Versao, $hex.Extension
$destino = Join-Path $hex.DirectoryName $novoNome

Copy-Item -Path $hex.FullName -Destination $destino -Force

Write-Host "Build versionado gerado: $destino"
