param(
    [Parameter(Mandatory=$true)][string]$Raiz,
    [Parameter(Mandatory=$true)][string]$Archivo
)
$ErrorActionPreference = 'Stop'

# Compara los bytes de terceros con el identificador Git de la fuente publicada.
$manifest = Get-Content -Raw -Encoding UTF8 (Join-Path $Raiz 'terceros/manifest.json') | ConvertFrom-Json
$sha1 = [System.Security.Cryptography.SHA1]::Create()
try {
    foreach ($f in $manifest) {
        $ruta = Join-Path (Join-Path (Join-Path $Raiz 'terceros') $f.dir) $f.path
        [byte[]]$bytes = [System.IO.File]::ReadAllBytes($ruta)
        [byte[]]$cabecera = [System.Text.Encoding]::UTF8.GetBytes(('blob ' + $bytes.Length + [char]0))
        [byte[]]$entrada = New-Object byte[] ($cabecera.Length + $bytes.Length)
        [Array]::Copy($cabecera, 0, $entrada, 0, $cabecera.Length)
        [Array]::Copy($bytes, 0, $entrada, $cabecera.Length, $bytes.Length)
        $actual = -join ($sha1.ComputeHash($entrada) | ForEach-Object { $_.ToString('x2') })
        if ($actual -ne $f.blob_sha) { throw "Dependencia modificada o dañada: $($f.dir)/$($f.path)" }
    }
} finally { $sha1.Dispose() }

# Un ASI es una DLL PE; revisamos arquitectura, encabezados y marca DLL.
[byte[]]$binario = [System.IO.File]::ReadAllBytes((Resolve-Path $Archivo))
if ($binario.Length -lt 256 -or $binario[0] -ne 0x4d -or $binario[1] -ne 0x5a) {
    throw 'El resultado no tiene encabezado MZ de Windows.'
}
$pe = [BitConverter]::ToInt32($binario, 0x3c)
if ($pe -lt 0 -or $pe -gt ($binario.Length - 26)) { throw 'Encabezado PE fuera del archivo.' }
if ([BitConverter]::ToUInt32($binario, $pe) -ne 0x4550) { throw 'Falta la firma PE.' }
if ([BitConverter]::ToUInt16($binario, $pe + 4) -ne 0x8664) { throw 'El ASI no es x64.' }
if (([BitConverter]::ToUInt16($binario, $pe + 22) -band 0x2000) -eq 0) { throw 'El ASI no está marcado como DLL.' }
if ([BitConverter]::ToUInt16($binario, $pe + 24) -ne 0x20b) { throw 'El ASI no tiene formato PE32+.' }
Write-Host "Correcto: $($manifest.Count) archivos de terceros íntegros y DLL x64 válida. Esto no es una prueba dentro del juego."
