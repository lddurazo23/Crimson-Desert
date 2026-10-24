param([Parameter(Mandatory=$true)][string]$Archivo)
$ErrorActionPreference = 'Stop'
$raiz = Split-Path $PSScriptRoot -Parent
$salida = Join-Path $raiz 'paquete'
$zip = Join-Path $raiz 'DesiertoSeguro-0.3-DMM.zip'
$origen = (Resolve-Path $Archivo).Path
if (Test-Path $salida) { Remove-Item -Recurse -Force $salida }
New-Item -ItemType Directory -Path $salida | Out-Null
Copy-Item $origen (Join-Path $salida 'DesiertoSeguro.asi')
Copy-Item (Join-Path $raiz 'README.md') $salida
if (Test-Path (Join-Path $raiz 'LICENSE')) { Copy-Item (Join-Path $raiz 'LICENSE') $salida }
Copy-Item -Recurse (Join-Path $raiz 'docs') $salida
$licencias = Join-Path $salida 'licencias'
New-Item -ItemType Directory -Path $licencias | Out-Null
Copy-Item (Join-Path $raiz 'terceros/imgui/LICENSE.txt') (Join-Path $licencias 'Dear-ImGui.txt')
Copy-Item (Join-Path $raiz 'terceros/minhook/LICENSE.txt') (Join-Path $licencias 'MinHook.txt')
Copy-Item (Join-Path $raiz 'terceros/trinity/LICENSE') (Join-Path $licencias 'Trinity.txt')
Copy-Item (Join-Path $raiz 'terceros/LEEME.md') $licencias
if (Test-Path (Join-Path $raiz 'licencias')) { Copy-Item (Join-Path $raiz 'licencias/*') $licencias }
$suma = Get-FileHash (Join-Path $salida 'DesiertoSeguro.asi') -Algorithm SHA256
($suma.Hash.ToLowerInvariant() + '  DesiertoSeguro.asi') | Set-Content -Encoding ASCII (Join-Path $salida 'SHA256SUMS.txt')
'DesiertoSeguro 0.3' | Set-Content -Encoding ASCII (Join-Path $salida 'VERSION.txt')
# ZIP plano: el ASI queda en la raiz, sin carpeta contenedora ni manifiesto supuesto.
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path (Join-Path $salida '*') -DestinationPath $zip -CompressionLevel Optimal
Write-Host "Listo: $zip. Importa ese ZIP desde DMM con el juego cerrado."
