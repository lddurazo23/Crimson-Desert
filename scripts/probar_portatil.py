#!/usr/bin/env python3
"""Ejecuta pruebas portátiles del código real; no carga ni modifica el juego."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

opciones = argparse.ArgumentParser(description=__doc__)
opciones.add_argument("--compilador", default="c++", help="Compilador C++20 del sistema")
args = opciones.parse_args()
raiz = Path(__file__).resolve().parent.parent

def ejecutar(comando):
    subprocess.run([str(x) for x in comando], cwd=raiz, check=True)

with tempfile.TemporaryDirectory(prefix="desierto-pruebas-") as temporal:
    salida = Path(temporal)
    casos = [
        ("patron", [], ["pruebas/patron.cpp"]),
        ("mundo", ["pruebas/mundo_portatil/stubs"],
            ["src/mundo.cpp", "pruebas/mundo_portatil/prueba.cpp"]),
        ("teclas", [], ["pruebas/teclas.cpp"]),
        ("monturas_callback", ["pruebas/monturas5_portatil/stubs"], ["pruebas/monturas5_portatil/callback.cpp"]),
        ("monturas_gateway", [], ["pruebas/monturas5_portatil/gateway.cpp", "pruebas/monturas5_portatil/prueba_gateway.S", "src/monturas5_gateway.S"]),
        ("planeo_nativo", ["pruebas/planeo_nativo_portatil/stubs"], ["pruebas/planeo_nativo_portatil/prueba.cpp"]),
        ("equipo_nativo", ["pruebas/equipo_nativo_portatil/stubs"], ["pruebas/equipo_nativo_portatil/prueba.cpp"]),
        ("clima", ["pruebas/clima_portatil/stubs"],
            ["pruebas/clima_portatil/prueba.cpp"]),
        ("regiones", ["pruebas/regiones_portatil/stubs"],
            ["pruebas/regiones_portatil/prueba.cpp"]),
    ]
    for nombre, inclusiones, fuentes in casos:
        print("Verificando " + nombre, flush=True)
        ejecutable = salida / nombre
        comando = [args.compilador, "-std=c++20", "-Wall", "-Wextra", "-Werror", "-pthread", "-Isrc"]
        comando += ["-I" + x for x in inclusiones]
        ejecutar(comando + fuentes + ["-o", ejecutable])
        ejecutar([ejecutable])
    ejecutar([sys.executable, "pruebas/equipo_portatil/ejecutar.py",
        "--compilador", args.compilador])
    print("Verificando delitos", flush=True)
    delitos = salida / "delitos"
    ejecutar([args.compilador, "-std=c++20", "-Wall", "-Wextra", "-Werror", "-pthread", "-Isrc",
        "src/delitos.cpp", "pruebas/delitos_portatil/prueba.cpp", "-o", delitos])
    for caso in ("espera", "rechazo", "externo", "recarga", "fila_tardia", "reintento", "restauracion"):
        ejecutar([delitos, caso])
print("Pruebas portátiles terminadas. Esto no valida el motor real ni una partida.")

