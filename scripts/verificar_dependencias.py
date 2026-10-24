"""Comprueba integridad de dependencias sin instalar paquetes ni compilar."""
import hashlib
import json
from pathlib import Path

raiz = Path(__file__).resolve().parents[1] / "terceros"
manifest = json.loads((raiz / "manifest.json").read_text(encoding="utf-8"))
for archivo in manifest:
    ruta = raiz / archivo["dir"] / archivo["path"]
    datos = ruta.read_bytes()
    actual = hashlib.sha1(b"blob " + str(len(datos)).encode("ascii") + b"\0" + datos).hexdigest()
    if actual != archivo["blob_sha"]:
        raise SystemExit(f"ERROR: dependencia modificada o dañada: {ruta}")
print(f"Correcto: {len(manifest)} archivos idénticos a los commits originales.")
