# Elenco delle cartelle da escludere
$excludeDirs = @(".angular", ".git", ".vscode", "node_modules", "dist")

# Elenco dei file da escludere (nuovo)
$excludeFiles = @("package-lock.json",  "tsconfig.json")

# Percorso del file di output
$outputFile = "mergedFile.txt"

# Cancella il file di output se esiste
if (Test-Path $outputFile) {
    Remove-Item $outputFile
}

# Funzione per controllare se un file è in una delle cartelle da escludere
function IsExcludedDir {
    param (
        [string]$filePath,
        [array]$excludeDirs
    )
    foreach ($dir in $excludeDirs) {
        if ($filePath -like "*\$dir\*") {
            return $true
        }
    }
    return $false
}

# Funzione per controllare se un file deve essere escluso in base al nome
function IsExcludedFile {
    param (
        [string]$fileName,
        [array]$excludeFiles
    )
    return $excludeFiles -contains $fileName
}

# Cerca tutti i file .html, .ts e .css ricorsivamente
Get-ChildItem -Recurse -Include  *.c, *.h | ForEach-Object {
    $filePath = $_.FullName
    $fileName = $_.Name
    
    # Verifica se il file è in una delle cartelle da escludere o se il file stesso è nella lista dei file da escludere
    if (-not (IsExcludedDir -filePath $filePath -excludeDirs $excludeDirs) -and -not (IsExcludedFile -fileName $fileName -excludeFiles $excludeFiles)) {
        # Aggiungi l'intestazione del file al file di output
        Add-Content -Path $outputFile -Value "---- File: $filePath ----"
        # Aggiungi il contenuto del file al file di output
        Get-Content $filePath | Add-Content -Path $outputFile
    }
}

Write-Host "Completato. Tutti i file sono stati uniti in $outputFile escludendo le cartelle specificate e i file nella lista di esclusione."