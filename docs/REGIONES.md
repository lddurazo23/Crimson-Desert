# Descubrimientos, mapa y nexos: alcance real

La solicitud de **quitar toda la niebla y activar todos los nexos por región sigue parcial**. No se presenta como una operación que pueda completarse pegando una dirección. Se retiraron los descriptores de contenedores y el escritor de dos estados de la versión anterior: no había una fuente que acreditara esas estructuras ni que sus escrituras representaran la operación del juego.

La alternativa implementada es el **editor nativo de descubrimientos individuales**. Lee el catálogo vivo y permite aprender una entrada de las categorías publicadas `Node` (lugares) o `AbyssRuins` (conocimientos del abismo). No incluye IDs inventados ni una lista de tipos que el usuario tenga que programar. El nombre interno real y el ID leído del juego se muestran para seleccionar el registro exacto.

| Operación | Estado real | Lo que falta |
|---|---|---|
| Aprender un lugar `Node` elegido del catálogo | Lógica completa; firma/ABI/layout publicados | Verificar vigencia de las firmas y perfil en el ejecutable del usuario. Una firma alternativa del global se pega en el encabezado central si hace falta. |
| Aprender una entrada `AbyssRuins` elegida | Lógica completa de conocimiento, mismo contrato nativo | No acredita activar una puerta ni completar su acertijo. |
| Quitar toda la niebla por región | Parcial en el ASI | Transacción real de revelado y correspondencia regional; aprender conocimientos no demuestra eliminar toda la niebla. |
| Activar todos los nexos por región | Parcial en el ASI | Correspondencia de puertas, descubrimiento, estados y acertijos. No hay un contrato publicado completo para esa operación. |

Los nombres Hernand, Demeniss, Delesyia, Pailune, Kweiden y Desierto Carmesí son etiquetas de selección del menú. **No se usan como IDs internos ni filtran el catálogo mediante una correspondencia regional supuesta.** El editor de conocimientos muestra sus registros reales y ofrece búsqueda individual; la agrupación regional completa no está acreditada.

## Evidencia de tres capas para las puertas

La investigación pública de `crimson-rs` distingue:

1. **Descubrimiento:** `KnowledgeSaveData._list`; aparece el icono del mapa.
2. **Estado de puerta:** `_initStateNameHash`; puede extender un puente o cambiar una puerta.
3. **Estado del acertijo:** variantes adicionales del mismo campo; requiere resolver la mecánica correspondiente.

El paquete de 398 conocimientos descrito en esa investigación sólo corresponde a descubrimiento. El documento declara pendientes los hashes de acertijos y el cruce completo `(gimmick_info_key, knowledge_key, owner_level_name, display_name)`. Por tanto, ni otorgar conocimientos ni escribir un solo hash acredita la activación completa de todos los nexos. Tampoco se deduce el layout en memoria a partir del formato de un archivo de guardado.

Fuente fijada: [crimson-rs, MIT, revisión `10b137d455408caa4fa7574f73ea062d9987cb9d`, investigación de puertas](https://github.com/bbfox0703/crimson-rs/blob/10b137d455408caa4fa7574f73ea062d9987cb9d/docs/abyss-gate-map.md).

## Cómo funciona la alternativa nativa

El menú habilita el editor, muestra una copia del catálogo y encola una clave elegida. El tick del juego obtiene otra vez el personaje controlado, verifica la clase RTTI `ServerKnowledgeActorComponent`, su tabla, estados y cantidades. Sólo admite nombres cuyo token de categoría sea exactamente `Node` o `AbyssRuins`. Antes de cada llamada vuelve a resolver la clave y el contexto; no reutiliza un puntero de actor obtenido desde el menú.

La llamada `KnowledgeApply` usa **el ABI publicado de siete argumentos**. El editor obtiene el TEB del hilo actual mediante `NtQueryInformationThread`, activa temporalmente su realm de autoridad y restaura el byte anterior después de la llamada. Ese resolvedor es autónomo: **no instala hooks de inventario ni recorre slots C0/C8**. El perfil moderno publicado usa realm `509`; no intenta el byte legado `498` porque parezca una bandera.

Después espera hasta cinco segundos a que el motor confirme el estado aprendido. No repite automáticamente una llamada sin confirmar. Un cambio de personaje, tabla o sesión cancela el trabajo. Un resultado negativo del motor cancela la solicitud; una excepción nativa o restauración fallida del realm bloquea el editor para esa sesión. Deshabilitar el editor cancela las solicitudes pendientes, pero no borra conocimientos ya otorgados.

No hay un lector/escritor de puertas basado en contenedores hipotéticos ni callbacks de activación con argumentos desconocidos. Tampoco existen marcadores para pegar formatos inventados de guardado.

## Actualizar datos

La única ubicación editable es `src/datos_juego.hpp`, bloque `ds::datos::regiones`. Incluye los offsets y la firma publicados. El marcador opcional `firmaTablaConocimiento` reemplaza el resolvedor por nombre si éste no encuentra el global en el ejecutable instalado; requiere la firma AOB única de una lectura RIP, el desplazamiento de su `disp32` y la longitud de esa instrucción. La explicación junto al marcador indica qué seguir en Cheat Engine.

Eso actualiza la vinculación del **editor de conocimientos ya implementado**. No implementa por sí mismo las operaciones regionales que permanecen parciales; esas operaciones requieren investigación adicional del motor y podrían requerir nuevo código. No se le pide al usuario completar contratos, argumentos, constructores ni estructuras.

## Alternativas existentes que sí tienen implementación pública

**Para estados individuales de puertas, con el juego cerrado:** [CrimsonAtomtic, revisión `21aa544eb12f42a17622172112e7e4b844ad4ec0`](https://github.com/bbfox0703/CrimsonAtomtic/tree/21aa544eb12f42a17622172112e7e4b844ad4ec0) tiene editor de guardados y un diálogo de puertas. Separa la operación **Unlock All Abyss Gates (Map Discovery)** del editor de estados individuales. Conserva estados desconocidos como sólo lectura. Su codec Rust lee el esquema que contiene cada partida; no requiere que el usuario escriba offsets de archivo. Esto es una herramienta externa y no convierte el ASI en un desbloqueador regional completo.

Para usarla: respalda toda tu carpeta de partidas; cierra el juego; abre una copia de la partida en CrimsonAtomtic; configura la instalación del juego para que lea nombres; usa el editor de conocimiento para descubrimientos o **Edit Abyss Gates** para una puerta individual; guarda en otra copia y comprueba el resultado al cargar. No actives masivamente registros de escenografía ni estados desconocidos. La disponibilidad de binarios y los pasos de instalación deben consultarse en la página de ese proyecto.

**Para ver el mapa global sin niebla:** [Map Tweaks de Caites](https://www.nexusmods.com/crimsondesert/mods/203?tab=description) anuncia una variante **No Fog of War** administrable como mod de datos. Es una alternativa global; no ofrece aquí selección regional desde este menú. Sus archivos y permisos pertenecen a su autor y no se redistribuyen ni se inventa un parche equivalente. Hay que revisar su compatibilidad actual y los pasos de DMM en su propia página.

## Fuentes y pruebas

- [Trinity, revisión MIT `96e72fe53a387989f6c8d18a32f5692dd6b7cdaa`, skills.cpp](https://github.com/patoangolano/trinity-crimson-desert-mounts/blob/96e72fe53a387989f6c8d18a32f5692dd6b7cdaa/src/game/skills.cpp): ABI, componente, tablas y aplicación nativa de conocimientos; el propietario sale del resolvedor del personaje, no de un inventario clonado.
- [Misma revisión, offsets.h:1130](https://github.com/patoangolano/trinity-crimson-desert-mounts/blob/96e72fe53a387989f6c8d18a32f5692dd6b7cdaa/src/game/offsets.h#L1130): TEB y realm moderno/legado; `inventory.cpp:1476` publica la consulta del hilo.
- [CrimsonAtomtic, KnowledgeEditorViewModel.cs, revisión fijada](https://github.com/bbfox0703/CrimsonAtomtic/blob/21aa544eb12f42a17622172112e7e4b844ad4ec0/src/CrimsonAtomtic.Ui/ViewModels/KnowledgeEditorViewModel.cs): categorías y advertencia del alcance de `Node`.
- [CrimsonAtomtic, AbyssGatesViewModel.cs, revisión fijada](https://github.com/bbfox0703/CrimsonAtomtic/blob/21aa544eb12f42a17622172112e7e4b844ad4ec0/src/CrimsonAtomtic.Ui/ViewModels/AbyssGatesViewModel.cs): descubrimiento, estado individual y tratamiento de escenografía/estados desconocidos.

`pruebas/regiones_portatil/prueba.cpp` compila el motor real con memoria sintética. Verifica las categorías, el catálogo, selección de claves, autoridad temporal, confirmación asíncrona, cancelación, cambio de propietario, rechazo nativo y bloqueo tras fallo de restauración. Sus IDs/nombres son datos de prueba, no datos para el juego. Estas pruebas no ejecutan Crimson Desert, no validan las firmas del usuario y no demuestran persistencia en una partida real.
