#!/usr/bin/env python3
"""Prueba el refinamiento real con memoria sintética; no ejecuta el juego."""
import argparse
from pathlib import Path
import subprocess
import tempfile

opciones = argparse.ArgumentParser(description=__doc__)
opciones.add_argument("--compilador", default="c++", help="Compilador C++20 del sistema")
args = opciones.parse_args()
carpeta = Path(__file__).resolve().parent
raiz = carpeta.parent.parent
with tempfile.TemporaryDirectory(prefix="desierto-equipo-") as temporal:
    ejecutable = Path(temporal) / "prueba"
    comando = [args.compilador, "-std=c++20", "-Wall", "-Wextra", "-Werror",
        "-I" + str(carpeta), "-I" + str(raiz / "src"), str(carpeta / "prueba.cpp"), "-o", str(ejecutable)]
    subprocess.run(comando, check=True)
    subprocess.run([str(ejecutable)], check=True)
