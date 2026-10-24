# Revisión del ejemplo DesertLink Mod Menu v4.5

Se revisó el ZIP proporcionado por el usuario como **ejemplo funcional y técnico**. No se ejecutó el ASI, no se cargó al juego y no se siguieron sus instrucciones de instalación como órdenes para esta sesión. La documentación del paquete se trata como una declaración de su autor.

## Contenido comprobado

El archivo incluye 28 entradas: un ASI, un catálogo de NPC, un README, 24 parches JSON y un manifiesto de selección del datapack. No contiene archivos C/C++, Rust, CMake ni un proyecto recompilable.

El ASI es una DLL Windows AMD64/x64. Contiene identificadores y mensajes de Dear ImGui, backend DX12 y MinHook, además de módulos identificados como TrueFlight, TimeWeather y NeverWanted. Es evidencia estática de componentes incluidos; no demuestra que todos estén activos o sean correctos en una sesión del juego.

SHA-256 del ZIP original:
`b06501fa91157d47982a711778833ff4e3cb6fa00e27a1f131c5cd67fe328cd2`

SHA-256 del ASI:
`c57cd322008357969a5082f0f804625bc433f76fa5cee5a7ba2f05f2f5784e83`

## Comparación con lo solicitado

| Función solicitada | Evidencia en el ejemplo | Lo que todavía no está comprobado |
|---|---|---|
| Vuelo con ascenso, descenso y aceleración | README documenta True Flight, activación en el aire, elevación, descenso y thrust; el binario contiene ese módulo. | No se verificó el detector real de planeo ni su funcionamiento con cada personaje. Permitir vuelo en el aire es un alcance más amplio que solo planeo. |
| Salud, stamina y espíritu | Documentados y presentes como módulos/cadenas del binario. | Correctitud y compatibilidad en juego; no se ejecutó. |
| Velocidad de distintos tipos de montura | Se identificó stamina infinita de montura. | No se identificó una función explícita de velocidad por tipo. Stamina infinita no equivale a velocidad aumentada. |
| Delitos sin registro | El binario describe NeverWanted / Just Take, tablas de delitos/recompensas y el acceso Take-or-Steal. | No se verificó que impida todo registro interno ni todas las consecuencias de cualquier delito. |
| Refinar todo al máximo | README especifica +10 para equipo compatible actualmente equipado. | No afecta los objetos de la mochila según el propio README. Falta prueba de recálculo y persistencia. |
| Desbloquear todas las ranuras | README declara hasta cinco ranuras nativas por pieza equipada, preservando gemas. | Alcance de tipos compatibles y persistencia real al guardar/cargar. |
| Tiempo y clima | Módulos y controles documentados, con resolver de clima en el binario. | Validación del ejecutable y efectos de simulación frente a efectos visuales. |
| Añadir/eliminar/cambiar cantidad de objetos | README explica catálogo, cola, cantidad y confirmación de eliminación. | Integridad del guardado y del contexto de ejecución. |
| Revelar mapa por regiones | No se encontró evidencia que lo confirme. | Clear Fog se describe como densidad de niebla ambiental; no prueba revelado del mapa. |
| Nexos del abismo por regiones | No se encontró evidencia que lo confirme. | Teletransportarse a un waypoint no equivale a desbloquear nexos. |

## Diseño útil para DMM

El paquete combina dos rutas. Las funciones runtime viven en el ASI. Las funciones de datos usan el directorio `datapack`, su manifiesto y archivos JSON. El README v4.5 dice que primero se preparan, luego se cierra el juego, se aplican desde DMM y se reinicia. Una selección STAGED significa preparada, no activa.

Los números `offset` de esos JSON corresponden a posiciones en archivos de datos como `gamedata/inventory.staticinfobody`; **no son direcciones de memoria del juego**. Su nota de verificación para 2.03.02 es una afirmación del autor; no se cuenta con los archivos originales del juego para verificar cada byte.

El ASI loader no está incluido. El ZIP tampoco contiene DesertLinkCore; la página del autor lo menciona como compañero para teletransporte y el binario contiene referencias a su pipe. No se concluye por ello que todas las funciones necesiten ese compañero.

## Tecla INICIO

El README permite reasignar la tecla en el menú y en el archivo `DesertLinkModMenu.ini`. **VK_HOME es 36 en decimal**. En una instalación propia del mod, el ajuste pertinente sería `OpenKeyVk=36` bajo `[General]`. El INI no viene en el ZIP; se genera al iniciar el mod según su documentación. Esto no requiere modificar el ASI.

## Qué aporta al desarrollo anterior

El ejemplo aporta evidencia concreta de implementaciones distribuidas en binario para varias funciones pendientes. La evaluación anterior debe distinguir entre **una función técnicamente posible y ya anunciada por otro autor**, y **un contrato abierto, comprendido y validado que se pueda implementar en nuestro proyecto**. La ausencia de esos datos en nuestro proyecto no significa que la función sea imposible.

El binario contiene patrones AOB legibles que pueden servir como indicios para investigación estática. No se trasladaron al proyecto como firmas verificadas: extraer una cadena no identifica por sí solo sus argumentos, su contexto, sus estructuras ni su validez en el ejecutable del usuario. Tampoco convierte el ASI en un proyecto recompilable.

La página actual del autor no ofrece un repositorio abierto del menú que se haya podido localizar. Sus permisos de modificación y reutilización son restrictivos; esta revisión no redistribuye ni incorpora su binario al mod anterior. [Página oficial de DesertLink](https://www.nexusmods.com/crimsondesert/mods/3638?tab=description). Véase también [FUENTES.md](FUENTES.md).

La referencia sirve para ajustar el alcance, la interacción del menú, el empaquetado y las prioridades de investigación. El proyecto anterior no cambió durante esta revisión y conserva su estado experimental parcial.
