# Auditoría de mapa, nexos y delitos

Esta revisión usa la definición del usuario: un algoritmo genérico con un formato hipotético no cuenta como lógica completa para Crimson Desert. Que una prueba sintética acepte ese formato tampoco demuestra que el motor lo use.

| Solicitud | Clasificación honesta | Alternativa implementada o publicada |
|---|---|---|
| Revelar toda la niebla por región | **Parcial en el ASI.** No se verificó la transacción ni la correspondencia regional. | Editor nativo individual de conocimientos `Node`, con ABI publicado y catálogo real; Map Tweaks anuncia una alternativa global de datos. Ninguna de las dos se presenta como selección regional íntegra. |
| Activar todos los nexos por región | **Parcial en el ASI.** Faltan correspondencias y contratos de estado/acertijos, no solamente offsets. | Editor nativo individual de conocimientos `AbyssRuins` para descubrimientos. CrimsonAtomtic publica edición offline individual de estados de puertas; no se afirma que resuelva todos los acertijos. |
| Cometer cualquier delito sin registrarlo | **Parcial.** Las tablas de recompensa y las exenciones conocidas no acreditan cobertura universal. | Reglas reversibles de recompensa y exención de categorías conocidas. El backend de tablas está implementado; su alcance debe describirse como parcial respecto a la petición universal. |

## Corrección de la entrega anterior

La versión anterior de `regiones.cpp` suponía contenedores runtime/guardado con propietario de actor, claves, identidad u64 y dos estados u32. Esa forma no estaba publicada para esta operación. Se eliminó todo ese escritor y sus descriptores. También se retiraron marcadores que pretendían que completar esos campos bastaría para implementar las puertas.

La revisión actual sólo usa el contrato real publicado de `KnowledgeApply`, y lo llama como editor de conocimiento individual. No contiene un callback universal de puertas vacío ni una función falsa que anuncie haber activado una región. La selección regional original se muestra como capacidad parcial sin ejecución supuesta.

## Evidencia nueva sobre puertas

`bbfox0703/crimson-rs`, revisión `10b137d455408caa4fa7574f73ea062d9987cb9d`, documenta tres capas: descubrimiento, estado y acertijo. Su pack de 398 conocimientos únicamente modifica la primera. Mantiene como investigación pendiente los estados de acertijo y el cruce completo de claves de conocimiento, gimmick, nivel y nombre. Por tanto, descubrir un icono y escribir dos campos no es prueba de una activación completa.

[Fuente fijada](https://github.com/bbfox0703/crimson-rs/blob/10b137d455408caa4fa7574f73ea062d9987cb9d/docs/abyss-gate-map.md).

## Evidencia actual sobre delitos

Se revisó además `ArasHueseyin/crimson-workbench`, revisión actual consultada `d8f7df0202d459b43b7d15b00197b2719c6f8fe7`. Su investigación de auto-loot conserva una distinción decisiva:

- `TribeInfo::_wantedCrimeType == 7` representa una negativa independiente para las categorías nativas `0/4/5/6`.
- Para las demás categorías interviene un predicado criminal personalizado del actor objetivo. Su propietario, sus condiciones y su bandera incondicional se verifican en el editor de auto-loot; no se publica allí un interruptor universal que vuelva inocua cualquier acción.
- El auto-loot sólo acepta una fracción de interacciones cuya legalidad demostró por esas condiciones. No altera todos los delitos, ni prueba que modificar tablas elimine todos los registros del mundo.
- Las funciones de preflight/registro que el documento menciona no proporcionan por sí solas una ABI universal acreditada para que este mod las intercepte.

[Investigación fijada de auto-loot](https://github.com/ArasHueseyin/crimson-workbench/blob/d8f7df0202d459b43b7d15b00197b2719c6f8fe7/docs/research/AUTO_LOOT.md), [lector nativo del mismo proyecto](https://github.com/ArasHueseyin/crimson-workbench/blob/d8f7df0202d459b43b7d15b00197b2719c6f8fe7/runtime/marketplace/auto-loot/auto_loot_native.h).

Los datos anteriores confirman una alternativa limitada; no justifican cambiar el texto del menú a «ningún delito se registra». Para ampliar la cobertura hace falta seguir una acción legítima de cada categoría, confirmar sus efectos de testigos/reputación/misiones y verificar la operación nativa correspondiente.

No se ejecutó el juego en esta revisión. La publicación de un dato se distingue de su vigencia y de su aceptación real en una partida.
