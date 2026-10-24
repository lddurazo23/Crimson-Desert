# Instalar el ASI desde Definitive Mod Manager

**Usa el archivo `DesiertoSeguro-0.3-DMM.zip` compilado, no el ZIP del proyecto de código.** Descarga el paquete que se conserva en [distribucion](../distribucion/DesiertoSeguro-0.3-DMM.zip); en GitHub pulsa **Download raw file**. [GitHub Actions también permite recompilarlo](COMPILAR.md). El mod es experimental. Incluye capacidades implementadas, rutas bloqueadas por datos y alternativas de alcance limitado; revisa la tabla de `README.md` antes de usarlo. No se presenta como cumplimiento de todas las funciones originales al 100 %.

## Qué se verificó de DMM

El autor publica DMM **4.1.0**, del 24 de octubre de 2026, con soporte ASI/DLL e importación de plugins desde ZIP. El gestor conserva la fuente en su biblioteca `mods` y despliega el plugin al `bin64` del juego cuando se activa. Tiene instalación/diagnóstico del ASI loader. Su historial declara Ultimate ASI Loader 9.7.4, selección entre `winmm.dll` y `version.dll`, y diagnóstico de loaders duplicados. El soporte de `.cdmod` fue retirado; este paquete es un ZIP ASI plano. [Página y changelog oficiales del autor](https://www.nexusmods.com/crimsondesert/mods/633?tab=description).

Un precedente abierto para este juego entrega **un ZIP plano con `.asi` en la raíz**, apto para DMM y sin un manifiesto obligatorio de parches: [Desert Tooling, instrucciones de su autor](https://github.com/KiefBC/desert-modding). Este paquete sigue esa estructura.

**Límite de verificación:** no ejecutamos DMM 4.1.0 en este entorno. El antiguo repositorio público enlazado por su autor devolvió 404; no se certifica un esquema interno de metadatos mediante código actual. No se inventó un `manifest.json` ni un `.cdmod` para forzar detección. La compatibilidad de formato se sustenta en la documentación del gestor y el precedente ASI publicado; queda pendiente comprobar esta importación concreta con la aplicación real.

## Paquete de instalación

Al abrir el ZIP debes ver directamente:

```text
DesiertoSeguro.asi
README.md
SHA256SUMS.txt
docs/
licencias/
```

No incluye ejecutable del juego, archivos PAZ, DLL gráficas, loader duplicado ni scripts que necesites ejecutar para instalar. Dear ImGui, MinHook y el runtime de C++ del binario entregado se enlazan al ASI; el loader es la dependencia externa.

## Instalación, paso a paso

1. Cierra Crimson Desert. Haz el respaldo de partidas indicado abajo.
2. Actualiza DMM desde [la página de su autor](https://www.nexusmods.com/crimsondesert/mods/633?tab=files). Extrae el gestor en una carpeta permanente fuera de la carpeta del juego; no lo ejecutes dentro del ZIP. Conserva su configuración habitual y tu biblioteca de mods.
3. Abre DMM y verifica que tenga seleccionada tu instalación correcta de Crimson Desert. Si tu versión pide el directorio de mods, usa una biblioteca fuera de la instalación del juego.
4. **Arrastra `DesiertoSeguro-0.3-DMM.zip` a DMM** para importarlo. No necesitas extraer el ASI ni copiarlo a mano al juego en este flujo. Si tu interfaz ofrece importar/agregar mods en lugar de arrastrar, selecciona ese mismo ZIP.
5. En **ASI Mods**, localiza **DesiertoSeguro.asi** y activa su casilla. Si no aparece, usa **Refresh**. Comprueba que solo haya una copia de ese plugin activa.
6. Revisa el estado **ASI Loader**. Si falta, usa la instalación del loader ofrecida por DMM; desde versiones recientes también se ofrece al usar **Start Game** con un ASI activado. **El `.asi` no se ejecuta por sí solo: necesita Ultimate ASI Loader x64.**
7. Si DMM informa dos loaders o un proxy que no reconoce, usa su diagnóstico para identificar el conflicto antes de iniciar. No copies un segundo loader por tu cuenta para intentar repararlo. Un ReShade existente puede usar `dxgi.dll`; no lo sobrescribas a ciegas.
8. Inicia el juego desde DMM, carga una copia de tu partida y presiona **INICIO (Home)**. En una instalación nueva las funciones disponibles están apagadas. Activa solo una para comprobarla.

No se promete un enlace Nexus de **Download with Manager**: eso requiere publicar un archivo en Nexus y registrar su enlace. Esta entrega es un ZIP local que DMM puede importar; importar y activar son pasos distintos.

## Loader elegido y convivencia

La elección es **Ultimate ASI Loader de 64 bits**, administrado por DMM. Se recomienda dejar que DMM conserve y diagnostique la variante que ya instaló. Su historial incluye varios nombres de proxy en diferentes versiones; no se afirma sin verificar qué hash o nombre exacto trae tu copia del gestor.

Si DMM no consigue instalar ningún loader y su diagnóstico confirma que **no existe otro activo**, la alternativa manual concreta es el **Ultimate ASI Loader x64 `winmm.dll`** del [repositorio oficial de ThirteenAG](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases). Descarga **`winmm-x64.zip`** de la columna **Win64** y extrae únicamente su `winmm.dll` y colócala como `winmm.dll` en **`Crimson Desert\bin64\`**, junto a **`CrimsonDesert.exe`**. Después vuelve a diagnosticar desde DMM y sigue administrando **el ASI** desde el gestor. [Precedente de `winmm.dll` para Crimson Desert](https://github.com/KiefBC/desert-modding).

No reemplaces una DLL existente sin identificarla ni instales simultáneamente otros proxies ASI bajo `version.dll`, `dinput8.dll` o `dxgi.dll`. La biblioteca del loader puede ofrecer varios nombres, pero necesitas una sola cadena de carga. El paquete entregado no añade ninguno para evitar ese conflicto.

## Dónde queda cada archivo

| Archivo | Ubicación |
|---|---|
| Fuente del plugin para DMM | Dentro de la biblioteca de mods seleccionada en DMM. La subcarpeta exacta depende de la importación. |
| Plugin que carga el juego | `Crimson Desert\bin64\DesiertoSeguro.asi`, desplegado por DMM en su configuración normal. No lo mantengas a mano en otro lugar a la vez. |
| Loader | Junto a `CrimsonDesert.exe`, normalmente en `bin64`; lo administra DMM. |
| Ajustes y teclas | `%LOCALAPPDATA%\DesiertoSeguro\DesiertoSeguro.ini`. Se crea al guardar un cambio desde el menú. |
| Diagnóstico del mod | `%LOCALAPPDATA%\DesiertoSeguro\DesiertoSeguro.log`. |
| Datos que se actualizan antes de compilar | `src/datos_juego.hpp`, en el proyecto de fuentes; no necesita instalarse al juego. |

El INI es el archivo único de configuración y persistencia de preferencias. Se guarda fuera de la biblioteca de DMM y de `bin64` para que una actualización o desactivación del plugin no sustituya tus teclas. Esto es una decisión de este mod. El botón de DMM para editar un INI junto al ASI puede no mostrarlo; usa **Configuración** dentro del juego.

## Actualizar o quitar

Con el juego cerrado, importa el ZIP nuevo y reemplaza la copia anterior en tu biblioteca de DMM. Revisa **ASI Mods** y activa solo una copia. DMM reciente puede avisar si el plugin desplegado es más viejo; comprueba también la fecha del resultado que acabas de descargar.

Para desactivar, desmarca DesiertoSeguro en DMM **y reinicia el juego**: un plugin ya cargado sigue en memoria hasta cerrar el proceso. Para quitarlo, usa la eliminación del mod desde DMM con el juego cerrado. Los ajustes de `%LOCALAPPDATA%\DesiertoSeguro` se conservan; puedes borrar esa carpeta manualmente si deseas reiniciar preferencias. Desinstalar el ASI no revierte modificaciones que ya quedaron en una partida.

## Respaldo y problemas comunes

Antes de probar: **Win+R → `%LOCALAPPDATA%\Pearl Abyss\CD\save`**. Con el juego cerrado y la nube sincronizada, copia toda la carpeta `save`, incluidas las subcarpetas numéricas, a una carpeta con fecha. [Ruta oficial de guardados](https://support.pearlabyss.com/crimsondesert/en-US/Faq/Home/Detail?_faqNo=616).

Si Home no abre, revisa primero que DMM haya desplegado el ASI y que haya un solo loader activo. Lee el log del mod. Si dice que no observó la creación de la cadena DX12, el loader llegó tarde: cierra el proceso completo y vuelve a iniciarlo con el loader activo desde el arranque. No se admite la inyección tardía como sustituto de esta instalación.

Si el menú abre pero una función indica **no disponible: faltan datos**, el loader sí cumplió su tarea. El inventario legado y su refinamiento vienen bloqueados hasta acreditar C0 mediante dos testigos AOB. Los perfiles nativos de planeo/clima/creación requieren el hash 2976; otra copia queda bloqueada. Las capacidades regionales completas y el registro universal de delitos son parciales, con alternativas identificadas en el menú. Reinstalarlo no resuelve una firma ausente ni un protocolo incompleto; consulta [DATOS](DATOS.md). Para recuperar Home tras una reasignación que no recuerdas, cierra el juego y renombra el INI a `DesiertoSeguro.ini.respaldo`; el siguiente inicio usa los valores por defecto.

El log de este mod está fuera de `bin64`, por lo que el reporte automático de DMM podría no recogerlo. Si necesitas diagnosticar una falla, conserva ese archivo junto con el reporte del gestor y la versión del juego.
