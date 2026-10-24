# Pegar datos verificados

El proyecto se puede compilar tal como viene. Eso no descubre datos del juego ni convierte en completas las funciones originales parciales. El estado de cada capacidad está en [README](../README.md).

**El único archivo de datos internos del juego es `src/datos_juego.hpp`.** Incluye firmas, offsets, tablas públicas, huellas, hashes y catálogos con su fuente. Las firmas vacías que el programa consume permanecen como marcadores claros.

## Qué puede quedar pendiente

En el perfil legado de inventario C0 faltan dos firmas testigo: `firmaTestigoCtorC0` y `firmaTestigoPasoC0`. La primera debe identificar un bloque real del constructor que demuestre sus límites C0; la segunda, la indexación real C0 de los slots. Ambas deben ser únicas y pertenecer a las funciones resueltas correspondientes, delimitadas por la información unwind pública del ejecutable. No se aceptan instrucciones parecidas de otra función.

Estos testigos impiden entregar un buffer C0 a un constructor C8. **Si el juego usa C8, pegar una firma cualquiera no vuelve compatible el motor legado.** Los perfiles nativos 2976 de planeo, clima y creación de equipo son implementaciones separadas con datos C8 cuando corresponde; no se mezclan sus layouts.

Otros marcadores del adaptador genérico de movimiento son investigación opcional. No sustituyen la clasificación nativa 2976 ya integrada. Para monturas, un puntero por sí solo no demuestra que cada especie use el ABI del integrador propuesto; el alcance por tipo sigue siendo parcial.

El resolvedor del editor individual de conocimientos permite una firma alternativa para su global cuando no coincide el ancla pública. Esa alternativa alimenta una ruta de `KnowledgeApply` publicada. No representa una activación regional de puertas.

Se retiraron los marcadores que pedían inventar un formato POD de guardado de sockets o contenedores equivalentes de ejecución/guardado para nexos. **Esas capacidades originales no se solucionan pegando un offset:** requieren estudiar la operación real. El menú y la tabla las identifican como parciales.

## Instrucciones para quien solo va a pegar valores

1. Pide a quien investigó el dato la versión y el hash del EXE, el **nombre exacto de la constante**, el valor completo y cómo comprobó la operación. Debe confirmar también que el contrato y el layout siguen siendo los publicados.
2. Cierra Crimson Desert. Conserva respaldo de partidas y del ASI anterior.
3. En GitHub abre `src`, luego `datos_juego.hpp` y pulsa el lápiz. En tu computadora puedes abrir ese archivo con **Bloc de notas**.
4. Usa **Ctrl+F** y busca `PEGAR AQUÍ`, o el nombre de la constante que te dieron.
5. Para una firma reemplaza **solo lo que va entre las comillas**. Conserva las comillas y el punto y coma. Los bytes se separan con espacios y `??` significa byte variable. No se proporciona un ejemplo falso para copiar.
6. Para un offset reemplaza solo el número indicado, conservando `0x` si es hexadecimal. No pegues una dirección absoluta de una sesión como firma.
7. Guarda el archivo. No lo renombres a `.hpp.txt`. Si editaste en GitHub, pulsa **Commit changes**.
8. Sigue [COMPILAR](COMPILAR.md). Espera el resultado nuevo correcto; con el juego cerrado, reemplaza el ASI desde DMM y deja activa una sola copia.
9. Prueba una capacidad a la vez en una copia de la partida. Si sigue bloqueada, conserva el aviso y el log; no retires la validación.

Para los dos testigos C0 basta pegar sus firmas cuando la investigación demuestra el mismo layout/ABI publicado. Una investigación que descubra otro layout o otra operación requiere otra implementación; no debe disfrazarse de actualización de datos.

## Qué significa una fuente pública

«Confirmado» acredita que un autor publicó un dato para su compilación analizada. «Inferido» es una conclusión técnica que aún necesita comprobación. «Sin verificar» no autoriza escrituras. Un AOB único puede sobrevivir a un parche que cambie el cuerpo de la función o el tamaño del objeto; por eso se exigen testigos de layout o un perfil exacto cuando corresponde.

Los perfiles 2976 exigen hash y testigos integrados. Cambiar únicamente su hash para aceptar otro EXE evita la protección y no valida sus estructuras. Las instrucciones no piden a una persona sin conocimientos técnicos que adivine esos datos.
