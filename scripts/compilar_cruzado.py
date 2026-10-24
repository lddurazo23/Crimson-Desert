#!/usr/bin/env python3
"""Reproduce el binario entregado con LLVM-MinGW en Linux; la ruta sencilla es Actions."""
import argparse
import concurrent.futures
from pathlib import Path
import subprocess
import sys

opciones = argparse.ArgumentParser(description=__doc__)
opciones.add_argument("toolchain", type=Path, help="Carpeta extraída de LLVM-MinGW")
opciones.add_argument("--salida", type=Path, default=Path("build-cruzada"))
args = opciones.parse_args()
raiz = Path(__file__).resolve().parent.parent
salida = args.salida.resolve()
salida.mkdir(parents=True, exist_ok=True)
binarios = args.toolchain.resolve() / "bin"
auxiliar = salida / "includes"
auxiliar.mkdir(exist_ok=True)
# Windows SDK tolera mayúsculas; estos puentes preservan los bytes de las dependencias.
(auxiliar / "Windows.h").write_text("#include <windows.h>\n")
(auxiliar / "Xinput.h").write_text("#include <xinput.h>\n")
includes = ["-I" + str(auxiliar)] + ["-I" + str(raiz / ruta) for ruta in (
    "src", "terceros/imgui", "terceros/imgui/backends", "terceros/imgui/misc/cpp",
    "terceros/minhook/include", "terceros/minhook/src")]
archivos = sorted((raiz / "src").glob("*.cpp"))
archivos += sorted((raiz / "src").glob("*.S"))
archivos += [raiz / "terceros/imgui" / ruta for ruta in (
    "imgui.cpp", "imgui_draw.cpp", "imgui_tables.cpp", "imgui_widgets.cpp",
    "misc/cpp/imgui_stdlib.cpp", "backends/imgui_impl_win32.cpp", "backends/imgui_impl_dx12.cpp")]
archivos += [raiz / "terceros/minhook/src" / ruta for ruta in (
    "buffer.c", "hook.c", "trampoline.c", "hde/hde64.c")]

def compilar(archivo):
    objeto = salida / (str(archivo.relative_to(raiz)).replace("/", "_") + ".o")
    compilador = binarios / ("x86_64-w64-mingw32-clang++" if archivo.suffix == ".cpp" else "x86_64-w64-mingw32-clang")
    comando = [str(compilador), "-O2", "-g0", "-fms-extensions", "-DWIN32_LEAN_AND_MEAN",
        "-DNOMINMAX", "-DUNICODE", "-D_UNICODE", "-Wno-macro-redefined"]
    if archivo.suffix == ".cpp":
        comando += ["-std=c++20"]
    resultado = subprocess.run(comando + includes + ["-c", str(archivo), "-o", str(objeto)],
        text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    return archivo, objeto, resultado.returncode, resultado.stdout

with concurrent.futures.ThreadPoolExecutor(max_workers=4) as ejecutor:
    resultados = list(ejecutor.map(compilar, archivos))
with (salida / "compilar.log").open("w", encoding="utf-8") as registro:
    for archivo, _, codigo, texto in resultados:
        registro.write(f"{archivo.name}: estado {codigo}\n{texto}\n")
        print(f"{archivo.name}: {'correcto' if codigo == 0 else 'ERROR'}")
        if codigo:
            print(texto)
if any(codigo for _, _, codigo, _ in resultados):
    sys.exit(1)
bibliotecas = ["d3d12", "dxgi", "dxguid", "d3dcompiler", "xinput9_1_0", "psapi", "bcrypt",
    "dwmapi", "gdi32", "user32", "imm32"]
comando = [str(binarios / "x86_64-w64-mingw32-clang++"), "-shared", "-static", "-o", str(salida / "DesiertoSeguro.asi")]
comando += [str(objeto) for _, objeto, _, _ in resultados]
resultado = subprocess.run(comando + ["-l" + nombre for nombre in bibliotecas],
    text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
(salida / "enlazar.log").write_text(resultado.stdout, encoding="utf-8")
if resultado.returncode:
    print(resultado.stdout)
    sys.exit(resultado.returncode)
print("ASI generado:", salida / "DesiertoSeguro.asi")
