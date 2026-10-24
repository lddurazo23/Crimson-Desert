# Recompensa y delitos

El motor propio de `src/delitos.cpp` modifica registros de tablas en el punto de ejecución del juego. El renderizador solo solicita activar/desactivar. No se instala un hook en la IA ni en la supuesta rutina `RegisterCrimeEvent`.

Los datos públicos permiten reducir la recompensa de `WantedInfo` y aplicar una exención de categorías de `TribeInfo`. **Esto no demuestra impedir el registro de todos los delitos**, ni cambia la recompensa ya acumulada. El menú debe distinguir “Sin recompensa por delitos” de “No registrar delitos”; la segunda capacidad necesita un contrato adicional comprobado.

## Fuentes y alcance

- [Trinity fork MIT, revisión 96e72fe53a387989f6c8d18a32f5692dd6b7cdaa, offsets.h:1316](https://github.com/patoangolano/trinity-crimson-desert-mounts/blob/96e72fe53a387989f6c8d18a32f5692dd6b7cdaa/src/game/offsets.h#L1316): `WantedInfo`, incremento de recompensa por delito `i64 +0x18`, bloqueada `u8 +0x10`, límite 4096 filas.
- [La misma fuente, inventory.cpp:260](https://github.com/patoangolano/trinity-crimson-desert-mounts/blob/96e72fe53a387989f6c8d18a32f5692dd6b7cdaa/src/game/inventory.cpp#L260): global → tabla, cantidad `u32 +0x08`, array de punteros de fila `+0x58` moderno, `+0x50` antiguo, ID como índice de lookup. El motor elige el layout configurado; no prueba ambos buscando algo que parezca un puntero.
- [La misma fuente, SetNoBounty:5971](https://github.com/patoangolano/trinity-crimson-desert-mounts/blob/96e72fe53a387989f6c8d18a32f5692dd6b7cdaa/src/game/inventory.cpp#L5971): demuestra poner el precio a cero. No implementa TribeInfo a pesar del comentario del header.
- [Workbench MIT, revisión 0c86f9be5afdc007b4e9e8c3c93b12ac46837462, auto_loot_native.h:170](https://github.com/ArasHueseyin/crimson-workbench/blob/0c86f9be5afdc007b4e9e8c3c93b12ac46837462/runtime/marketplace/auto-loot/auto_loot_native.h#L170): `TribeInfo` bloqueada `u8 +0x10`, `_wantedCrimeType u8 +0x50`, y valor 7 exento en la comprobación estudiada. [Su investigación](https://github.com/ArasHueseyin/crimson-workbench/blob/0c86f9be5afdc007b4e9e8c3c93b12ac46837462/docs/research/AUTO_LOOT.md) limita lo demostrado a categorías 0/4/5/6. Es una aplicación propia de esos datos, no una prueba en el ejecutable del usuario.
- [Nexus 3252, actualización del autor](https://www.nexusmods.com/crimsondesert/mods/3252?BH=0): confirma que se retiraron hooks amplios de IA y se sustituyeron por cambios en tablas; advierte de dos rutas de precio (robo y agresión). No publica el contrato adicional necesario para afirmar cobertura universal.

La referencia binaria aportada no se ejecuta ni se incorpora. Su texto `TribeInfo::_wantedCrimeType=+0x50` no acredita por sí solo el valor objetivo 0 ni un ABI del oráculo de propiedad. La fuente abierta refuta la firma histórica de `RegisterCrimeEvent`, que confundía una cadena de 64 bits con un identificador de 32 bits; se excluye por completo.

## Validación y restauración

Antes de escribir se valida el esquema y se capturan todas las filas vivas no bloqueadas. Cada copia conserva tabla, array, cantidad, índice/ID de lookup, dirección de fila y bytes originales por campo. Cada escritura vuelve a comprobar esos vínculos y verifica el valor leído. Si cambia la tabla o aparecen filas, se restauran únicamente las filas originales aún identificables y se captura el nuevo conjunto. Nunca se escribe al objeto antiguo después de cambiar su identidad.

Desactivar restaura solo valores que siguen siendo los escritos por este motor. Un cambio externo se conserva; una página ilegible o una restauración rechazada queda pendiente de reintento. Un fallo al aplicar detiene la opción y revierte las escrituras anteriores. Las pruebas portátiles cubren restauración, carga tardía, cambio de tabla, conflicto externo, fallo parcial y reintento. No sustituyen una prueba en el juego.

Los únicos datos editables son `datos_juego.hpp`: tablas, offsets, tipos y valores objetivo. Una `firmaGlobal` AOB única con operandos RIP permite sustituir un resolver por nombre que no coincida. `sinOffset` es un marcador real: cualquier campo con ese valor impide activar el motor antes de la primera escritura. Añadir otra tabla/campo no requiere cambiar el algoritmo, pero sí demostrar su significado, tipo, layout y efecto.
