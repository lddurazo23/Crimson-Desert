# Obtener tu ASI sin programar

El proyecto compila para **Windows de 64 bits**, con **Visual Studio 2022**. Incluye su propio código, Dear ImGui y MinHook; no necesitas descargar bibliotecas ni cambiar archivos de código para compilarlo como viene. Los datos de memoria pendientes seguirán apareciendo como no disponibles. Compilar no encuentra esas direcciones por sí solo.

## Tu repositorio

El proyecto se entrega en [lddurazo23/Crimson-Desert](https://github.com/lddurazo23/Crimson-Desert). Para recompilar desde ese repositorio, abre **Actions → Compilar ASI de Windows → Run workflow → Run workflow**. Cuando termine con marca verde, abre la ejecución y descarga **DesiertoSeguro-0.3-DMM** en **Artifacts**. Ese ZIP se importa en DMM. Si ya tienes este repositorio, puedes omitir los pasos para crear y subir uno nuevo de abajo.

## Ruta recomendada: GitHub Actions, desde el navegador

Necesitas una cuenta de [GitHub](https://github.com/signup). La computadora de GitHub tiene el compilador; tú solo subes los archivos y descargas el resultado. No necesitas instalar Visual Studio, Git ni un compilador online adicional. Puedes usar un repositorio privado. La disponibilidad y minutos de Actions dependen de tu cuenta; revisa lo que GitHub te muestre antes de activar servicios de pago.

1. Descarga el ZIP **del proyecto completo** entregado en esta conversación. En el Explorador de Windows, haz clic derecho en él y selecciona **Extraer todo**.
2. Abre la carpeta extraída. Debes ver `CMakeLists.txt`, `compilar.bat`, `src`, `terceros`, `docs`, `scripts` y `.github` juntos. Si solo ves otra carpeta, ábrela hasta llegar a esos archivos. **No subas el ZIP cerrado**.
3. En GitHub, pulsa el botón **+**, luego **New repository**. Escribe `DesiertoSeguro`, selecciona **Private**, marca **Add a README file** y pulsa **Create repository**.
4. En la página del repositorio, pulsa **Add file → Upload files**. GitHub limita cada carga desde el navegador a 100 archivos: sube **una carpeta completa por carga**, por ejemplo primero `src`, después `terceros`, luego `pruebas`, `docs`, `scripts`, `licencias` y `.github`. En cada carga pulsa **Commit changes**. Después sube los archivos sueltos de la raíz, como `CMakeLists.txt`, `compilar.bat`, `README.md`, `LICENSE`, `.gitattributes` y `.gitignore`. Arrastra las carpetas mismas para conservar sus nombres; no crees otra carpeta contenedora. Es normal que las primeras compilaciones fallen mientras todavía faltan carpetas por subir.
5. Comprueba que la carga incluya `.github/workflows/compilar.yml`. `.github` es una carpeta real que viene en el ZIP; no la borres. En Windows un nombre que empieza con punto no la oculta automáticamente. Si tu Explorador sí la oculta, activa **Ver → Mostrar → Elementos ocultos**. Pulsa **Commit changes**.
6. Abre la pestaña **Actions**. Si GitHub pide habilitar Actions para ese repositorio, habilítalo. Cuando todos los archivos estén en su lugar, una carga o modificación inicia una compilación automáticamente en las ramas `main` o `master`.
7. También puedes abrir **Compilar ASI de Windows → Run workflow → Run workflow** para iniciarla manualmente. Espera a que termine; normalmente tarda unos minutos. La marca verde significa que el compilador y la verificación del formato terminaron bien.
8. Abre la ejecución terminada. En **Artifacts**, descarga **DesiertoSeguro-0.3-DMM**. GitHub exige iniciar sesión para descargar los artifacts. Este resultado queda disponible durante 30 días; puedes volver a compilarlo después.
9. Conserva ese ZIP como **`DesiertoSeguro-0.3-DMM.zip`**: está preparado para importarse directamente en DMM. Si lo abres para revisar, `DesiertoSeguro.asi` debe estar en la raíz, junto a `README.md`, `docs`, `licencias` y `SHA256SUMS.txt`. Sigue [INSTALAR.md](INSTALAR.md).

Si aparece una cruz roja, abre el paso rojo para leer el primer error. No hay un ASI nuevo válido hasta que termine con marca verde. Los logs y el artifact **Diagnostico-de-compilacion** sirven para pedir ayuda. No descargues un resultado anterior pensando que incluye tus cambios nuevos.

**Límite de la marca verde:** confirma compilación, archivos de terceros íntegros y formato DLL x64. No comprueba direcciones del juego ni estabilidad de la partida. No se ejecuta Crimson Desert en GitHub Actions.

Documentación oficial de los pasos: [subir archivos a un repositorio](https://docs.github.com/en/repositories/working-with-files/managing-files/adding-a-file-to-a-repository), [iniciar un workflow manualmente](https://docs.github.com/en/actions/how-tos/manage-workflow-runs/manually-run-a-workflow) y [descargar artifacts](https://docs.github.com/en/actions/how-tos/manage-workflow-runs/download-workflow-artifacts).

## Pegar datos encontrados y volver a compilar

1. Cierra el juego y conserva un respaldo de tus partidas y de tu ASI anterior.
2. La única ubicación de datos del juego es **`src/datos_juego.hpp`**. Busca el texto **`PEGAR AQUÍ`**. Sigue la explicación que aparece junto a cada marcador: indica exactamente qué valor falta y para qué se utiliza.
3. Una firma de bytes se pega **entre las comillas existentes**. Conserva las comillas y el punto y coma. Los espacios separan bytes y `??` indica un byte variable. Nunca reemplaces `??` por números elegidos al azar. Un offset se pega como el número hexadecimal indicado por quien lo verificó, conservando `0x` cuando corresponda.
4. No cambies un valor para hacerlo coincidir con un ejemplo de otro juego. Una firma publicada para un parche anterior tampoco confirma que funcione con tu parche. Los marcadores vacíos dejan funciones bloqueadas de forma intencional.
5. Para editar desde GitHub: abre `src`, abre `datos_juego.hpp`, pulsa el ícono del lápiz, pega únicamente los valores indicados y pulsa **Commit changes**. Se iniciará otra compilación. Descarga el nuevo artifact cuando termine en verde.
6. Para editar en tu computadora: haz clic derecho en `datos_juego.hpp`, selecciona **Abrir con → Bloc de notas**, pega los valores indicados y guarda. Vuelve a subir ese archivo en la carpeta `src` de GitHub, o utiliza la compilación local de abajo. No guardes una segunda copia llamada `.hpp.txt`.
7. Reemplaza el ASI instalado con el resultado de esa compilación, con el juego cerrado. Prueba una función a la vez en una copia de tu partida. Consulta el aviso específico del menú y el log si algo sigue bloqueado.

**Los marcadores de 0.3 son datos consumidos por rutas concretas.** El editor legado y su refinamiento requieren dos AOB testigo del layout C0 antes de instalarse. Los perfiles nativos 2976 ya incluyen sus datos públicos; no necesitas inventar un estado de planeo ni un formato de sockets. Las capacidades originales clasificadas como parciales no se completan pegando un valor: consulta [DATOS](DATOS.md) y la tabla del README. Una marca verde de compilación no cambia esa clasificación.

## Ruta alternativa: doble clic en tu computadora

Esta ruta sirve si prefieres evitar GitHub. Descarga e instala lo siguiente, en este orden:

1. [Visual Studio Community 2022, instalador oficial](https://aka.ms/vs/17/release/vs_community.exe). Si ya lo tienes, abre **Visual Studio Installer → Modificar**.
2. En **Cargas de trabajo**, marca **Desarrollo para el escritorio con C++**. En los detalles de instalación verifica **MSVC v143**, **Windows 10 SDK o Windows 11 SDK** y **Herramientas de CMake de C++ para Windows**. Acepta la instalación y espera a que termine.
3. Extrae el ZIP del proyecto completo en una carpeta corta, por ejemplo `C:\Mods\DesiertoSeguro`. No lo compiles dentro del ZIP ni dentro de la carpeta del juego.
4. Haz doble clic en **`compilar.bat`**. No necesitas escribir comandos. El archivo encuentra Visual Studio 2022, configura x64, compila y revisa el resultado.
5. Cuando muestre **LISTO**, abre **`build\Release`**: allí está `DesiertoSeguro.asi`. También se crean **`paquete`**, con el ASI, las instrucciones y las licencias, y **`DesiertoSeguro-0.3-DMM.zip`**, listo para importar. No necesitas copiar archivos `.lib`, `.obj` ni carpetas de compilación al juego.
6. Para recompilar tras pegar valores, vuelve a hacer doble clic en `compilar.bat`. Instala el resultado nuevo con el juego cerrado.

Si el archivo indica que falta Visual Studio o CMake, vuelve al paso 2. El proyecto no requiere Python para compilar en Windows. El compilador genera un ASI con las bibliotecas del menú y de hooks enlazadas; no necesitas copiarlas como DLL adicionales.

Para alguien que ya usa una terminal, los comandos equivalentes son:

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

No se recomienda un compilador online genérico: el proyecto requiere Windows SDK, Direct3D 12 y salida DLL x64. El workflow incluido ya prepara ese entorno concreto.
