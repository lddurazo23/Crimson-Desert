# Fuentes y alcance de la verificación

Consulta actualizada al **24 de octubre de 2026**. Se distingue lo publicado de lo probado por este proyecto. **No hubo ejecución de este ASI dentro de Crimson Desert ni prueba de su importación en DMM.**

## Juego y carga desde DMM

- **Confirmado:** Pearl Abyss exige DirectX 12 en PC. [Requisitos oficiales](https://crimsondesert.pearlabyss.com/es-mx/News/Notice/Detail?_boardNo=62).
- **Confirmado:** la lista oficial consultada muestra **2.03.02**, del 23 de septiembre, como parche más reciente verificable. No conocemos el hash del usuario. [Lista oficial](https://crimsondesert.pearlabyss.com/en-US/News/Notice), [notas 2.03.02](https://crimsondesert.pearlabyss.com/en-US/News/Notice/Detail?_boardNo=133).
- **Confirmado:** ruta Windows de guardados **`%LOCALAPPDATA%\Pearl Abyss\CD\save`**, con subcarpetas numéricas. [Soporte oficial](https://support.pearlabyss.com/crimsondesert/en-US/Faq/Home/Detail?_faqNo=616).
- **Confirmado según el autor:** DMM **4.1.0**, actualizado el 24 de octubre, admite ASI/DLL, origen en su biblioteca y despliegue en `bin64`. El historial menciona Ultimate ASI Loader **9.7.4**, elección entre `winmm.dll` y `version.dll`, companion files y diagnóstico de duplicados. También registra retirada de `.cdmod`. No se verificó el hash del loader incluido ni se ejecutó la aplicación. [Página y changelog de DMM](https://www.nexusmods.com/crimsondesert/mods/633?tab=description).
- **Confirmado por precedente abierto:** Desert Tooling documenta un ZIP plano con ASI en la raíz para DMM y `winmm.dll` en `bin64`. No publica un manifiesto obligatorio que deba inventarse. [Instrucciones de KiefBC](https://github.com/KiefBC/desert-modding#install).
- **Confirmado:** Ultimate ASI Loader ofrece `winmm-x64.zip` para Win64. Se elige el loader administrado por DMM y se evita añadir proxies en paralelo. [Proyecto oficial](https://github.com/ThirteenAG/Ultimate-ASI-Loader), [descargas](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases).

## Revisiones fijadas utilizadas

| Fuente | Revisión | Evidencia usada |
|---|---|---|
| [XeTrinityz/Trinity](https://github.com/XeTrinityz/Trinity/tree/70c9a00dd6e10b2081d706a837756844c11f5c2b) | `70c9a00dd6e10b2081d706a837756844c11f5c2b` | Stats, reloj, inventario legado y equipo. No hay hash inequívoco de su EXE. El perfil C0 se bloquea hasta testigos estructurales. |
| [crimson-workbench](https://github.com/ArasHueseyin/crimson-workbench/tree/0c86f9be5afdc007b4e9e8c3c93b12ac46837462) | `0c86f9be5afdc007b4e9e8c3c93b12ac46837462` | Planeo nativo, clima y creación de equipo; ABI, animaciones, constructor/conversión/otorgamiento y testigos para 2976. |
| [Trinity con monturas](https://github.com/patoangolano/trinity-crimson-desert-mounts/tree/96e72fe53a387989f6c8d18a32f5692dd6b7cdaa) | `96e72fe53a387989f6c8d18a32f5692dd6b7cdaa` | Tick de trabajo, límites de refinamiento, tablas de delitos y aplicación de conocimientos. |
| [Investigación de auto-loot](https://github.com/ArasHueseyin/crimson-workbench/blob/d8f7df0202d459b43b7d15b00197b2719c6f8fe7/docs/research/AUTO_LOOT.md) | `d8f7df0202d459b43b7d15b00197b2719c6f8fe7` | Exenciones parciales de delitos; no acredita un bloqueo universal del registro. |
| [Mapa de puertas de crimson-rs](https://github.com/bbfox0703/crimson-rs/blob/10b137d455408caa4fa7574f73ea062d9987cb9d/docs/abyss-gate-map.md) | `10b137d455408caa4fa7574f73ea062d9987cb9d` | Distingue descubrimiento, estado y acertijo; mantiene cruces y capas sin resolver. |
| [CrimsonAtomtic](https://github.com/bbfox0703/CrimsonAtomtic) | Referencia del formato guardado | Editor offline publicado; no se copia su layout de archivo como estructura de memoria. |

Los nombres técnicos, bytes y offsets están citados junto a los bloques correspondientes en `src/datos_juego.hpp`. Las licencias aplicables de las adaptaciones se conservan. Una referencia a un hecho técnico de otro proyecto no otorga por sí sola permiso para copiar toda su implementación.

## Hallazgos que cambian la clasificación

**Planeo:** [GLIDE_SPEED.md](https://github.com/ArasHueseyin/crimson-workbench/blob/0c86f9be5afdc007b4e9e8c3c93b12ac46837462/docs/research/GLIDE_SPEED.md) publica un clasificador nativo y las capas de animación. La revisión 0.3 integra esos datos, con control vertical nuevo. La prueba del autor de su aceleración horizontal no es una prueba de este ASI ni de todos los protagonistas. Véase [MOVIMIENTO](MOVIMIENTO.md).

**Equipo nuevo:** Workbench publica [live-items](https://github.com/ArasHueseyin/crimson-workbench/tree/0c86f9be5afdc007b4e9e8c3c93b12ac46837462/runtime/live-items) y [extra-sockets](https://github.com/ArasHueseyin/crimson-workbench/tree/0c86f9be5afdc007b4e9e8c3c93b12ac46837462/runtime/extra-sockets). Se utiliza una ruta nativa acotada de creación de una pieza nueva con cinco ranuras. Eso no demuestra desbloquear todas las piezas existentes ni valida el perfil legado C0.

**Monturas:** hay hechos técnicos públicos para elevar globalmente su atributo de velocidad al nivel 5 en 2.03.02. Es una alternativa distinta de acelerar físicamente cada especie por separado. [Fuente consultada](https://github.com/gutzufuss1477/Crimson-Desert-Mods/blob/99855e499d3033c82bd043beea3a326fcebf6e01/mods/real-infinite-horse-stamina-with-optional-lvl5-speed-for-all-hor/variants/all-mounts-level-5-speed/src/AllMountsLvL5Speed_20302.c).

**Datos C0/C8:** un constructor que escribe C8 no puede recibir un buffer C0. Se requieren testigos reales del layout antes de activar el inventario legado. Que un prólogo coincida no demuestra que el tamaño del objeto siga igual.

**Delitos:** WantedInfo y TribeInfo soportan reglas limitadas de recompensa/exención; no demuestran impedir cualquier registro. **Mapa y nexos:** aprender conocimientos no acredita revelado completo ni solución de estado/acertijo. Véase [auditoría de mapa y delitos](AUDITORIA_MAPA_DELITOS_V3.md).

**Inferido:** los AOB pueden resistir desplazamientos de código; ningún patrón garantiza que un parche conserve semántica y ABI. **No verificado:** equivalencia entre la versión comercial 2.03.02 y el FILEVERSION 1.0.0.2976. No se llama actual u obsoleto al perfil solo por comparar esos números: decide su hash exacto.

## Referencias adicionales

La tabla bbfox de [Open Cheat Tables](https://opencheattables.com/viewtopic.php?t=1836), Enhanced Flight de [Ronibuilds](https://github.com/Ronibuilds/CrimsonDesertMods) y [Otis_Inf Photomode](https://opm.fransbouma.com/Cameras/crimsondesert.htm) permiten contrastar viabilidad y alcance. No se importaron firmas de adjuntos sin contrato verificable ni se tomó una heurística de caída como prueba de planeo.

El DesertLink V4.5 proporcionado fue inspeccionado estáticamente; no se ejecutó ni se incorporó su implementación cerrada. Sus anuncios no se usan para equiparar teletransporte con desbloqueo de nexos ni niebla atmosférica con niebla cartográfica. [Revisión del ejemplo](REVISION_DESERTLINK.md).
