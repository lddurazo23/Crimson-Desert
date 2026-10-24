# Registro de la revisión 0.3

El resultado compilado de esta revisión es **DesiertoSeguro 0.3**, DLL Windows **PE32+ x64** con extensión `.asi`. La huella del archivo concreto está en **`SHA256SUMS.txt`**, junto al ASI del paquete. El informe `VERIFICACION_BINARIO.json` y las huellas de fuentes deben corresponder a esta compilación; no deben conservar cifras o hashes de 0.2 como si describieran el resultado nuevo.

El ASI local tiene **2,837,504 bytes** y SHA-256 `6885621665cdf45bbf8db3d8204c4b923e5d038118a9d6f9c51ad4415c946e24`. Pasaron 11 grupos de pruebas portátiles y el ensamblador real del gateway de monturas. Se compilaron y enlazaron 30 unidades.

La compilación cruzada usa **LLVM-MinGW 20261020 UCRT**, Clang 23.1.4, en Linux con destino Windows x64. [Distribución oficial](https://github.com/mstorsjo/llvm-mingw/releases/tag/20261020). El archivo de herramientas se verificó contra el digest publicado: `55064b60b991a1e1e8090b8c2f84e82b22cfe5903d12c159d65064cf5e8ae335`.

Las comprobaciones y su alcance se registran en [PRUEBAS](PRUEBAS.md). Compilar comprueba sintaxis y enlace; el examen PE comprueba formato y arquitectura. Las simulaciones prueban rutas del código con memoria sintética. Ninguna demuestra que las firmas correspondan al EXE del usuario, ni la persistencia o estabilidad de una partida.

**No se ejecutó este ASI dentro de Crimson Desert, no se probó la importación concreta en DMM y no se ejecutó el workflow MSVC en una cuenta del usuario.** El proyecto 0.3 tampoco se presenta como cumplimiento del 100 % de todas las funciones originales: la tabla del README identifica capacidades parciales y alternativas.

El workflow compila de forma nativa con MSVC en Windows y producirá otro binario cuyo hash será distinto. Sigue [COMPILAR](COMPILAR.md) para generarlo desde el navegador. El empaquetador registra su SHA-256.

Una persona técnica puede repetir la compilación cruzada así:

```sh
python3 scripts/compilar_cruzado.py /ruta/a/llvm-mingw-20261020-ucrt-ubuntu-22.04-x86_64
```

El script no descarga fuentes. Las dependencias vienen incluidas y fijadas por revisión. Las licencias de las adaptaciones y del runtime se conservan. `FUENTES_COMPILADAS_SHA256.txt` identifica los archivos del resultado cuando se regenera después del último cambio; un archivo histórico de otra revisión no sirve como prueba del código actual.
