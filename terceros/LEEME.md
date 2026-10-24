# Dependencias incluidas

El ZIP del proyecto contiene todos los archivos de estas bibliotecas que requiere la compilación. CMake no descarga código y no necesitas instalar Git. Los archivos conservan sus comentarios y licencias originales en inglés para mantener su atribución e integridad; el código propio y la documentación están en español.

| Biblioteca | Versión | Commit exacto | Licencia |
|---|---|---|---|
| [Dear ImGui](https://github.com/ocornut/imgui/tree/f5befd2d29e66809cd1110a152e375a7f1981f06) | v1.91.9b | `f5befd2d29e66809cd1110a152e375a7f1981f06` | MIT, incluido en `imgui/LICENSE.txt` |
| [MinHook](https://github.com/TsudaKageyu/minhook/tree/c3fcafdc10146beb5919319d0683e44e3c30d537) | v1.3.4 | `c3fcafdc10146beb5919319d0683e44e3c30d537` | BSD, incluido en `minhook/LICENSE.txt` |

`manifest.json` registra el identificador Git de cada archivo original. La verificación compara todos los bytes de las 33 dependencias antes de empacar el ASI. El número interno de versión en el CMake de MinHook sigue indicando 1.3.3 en el tag público 1.3.4; aquí se conserva tal como lo publicó su autor.

La compilación enlaza estas bibliotecas de forma estática. El paquete de instalación conserva sus licencias. No incluye un ASI loader: el gestor o el usuario instala Ultimate ASI Loader por separado.
