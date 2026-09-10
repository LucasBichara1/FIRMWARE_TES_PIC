# Remove as pastas de artefatos de build (mesmas ignoradas no .gitignore).
# Uso: rodar como Task do VS Code para limpar antes de um build novo.

$ErrorActionPreference = "Stop"

$RootDir = Split-Path -Parent $PSScriptRoot
$Pastas = @("out", "build", "dist", "debug", "_build")

foreach ($nome in $Pastas) {
    $caminho = Join-Path $RootDir $nome
    if (Test-Path $caminho) {
        Remove-Item -Path $caminho -Recurse -Force
        Write-Host "Removido: $caminho"
    }
}

Write-Host "Limpeza concluida."
