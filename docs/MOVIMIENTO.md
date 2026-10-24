# Planeo real y movimiento de monturas

El proyecto incluye un perfil nativo de planeo para **EXE 1.0.0.2976**, SHA-256 `57da440d72f4db974f25fef047cf84c4dadd999a88cb2a3c5af4c9bd67fde1e7`. Todos sus offsets, huellas de animación y testigos de código publicados están integrados en `src/datos_juego.hpp`. Este perfil no necesita pegar un detector hipotético de planeo. Rechaza otro ejecutable o código modificado antes de instalar su hook.

## Cómo reconoce el planeo

La investigación pública de crimson-workbench documenta el método de movimiento nativo, sus dos llamadas consumidoras y la selección de la capa de animación activa. Se verifica la identidad del protagonista, que esté vivo, la posesión y los vínculos entre actor, componentes, controlador, estado y método. Luego se comprueba el paquete y la huella completa de 156 bytes de la animación seleccionada.

Hay cinco registros publicados de impulso de planeo en la capa inferior y quince registros publicados de planeo ordinario/viraje en la capa superior. Una animación antigua que quede en la capa inactiva no autoriza el cambio. Las animaciones de inicio, aterrizaje y otros movimientos no se aceptan por parecido.

**Confirmado con fuente:** los datos de ambas familias de animaciones, la selección de capa, el ABI y el consumo de X/Y/Z. La fuente informa una prueba de su aceleración horizontal durante el impulso al 250 %. Esa es una prueba ajena del módulo original, no una prueba de este proyecto.

**Adaptación nueva:** aquí también se aceptan los quince registros ordinarios documentados y se añade control vertical. La clasificación tiene evidencia pública; el comportamiento de estos controles nuevos requiere validación dentro del juego. Los protagonistas que no usen las animaciones y los vínculos auditados permanecen sin modificación. No se afirma haber probado todos los personajes.

## Qué modifica el botón

Mientras la opción esté habilitada y mantengas el botón asignado, el vector efímero que producirá el movimiento recibe el multiplicador horizontal elegido y la orden de ascenso o descenso. No cambia el delta de tiempo ni teletransporta al personaje. Sin orden vertical se conserva la Y producida por el juego. Al soltar el botón, perder el foco, abrir el menú o desactivar la opción se deja de solicitar el cambio; continúan los cálculos y comprobaciones nativos.

El motor lee la velocidad horizontal entrante antes de calcular su salida. Se aplica la compensación publicada antes de la llamada y el multiplicador después, para evitar multiplicación exponencial. La función original se llama una sola vez y se conserva íntegro su resultado. Se vuelven a comprobar el propietario, la animación y el delta antes de publicar los tres valores.

La escritura usa comparación e intercambio de cada componente y restaura únicamente componentes que todavía pertenecen a esa invocación si falla una publicación. Un cambio ajeno se conserva y una restauración fallida bloquea el perfil durante la sesión. Esto reduce los fallos previsibles; no demuestra seguridad de colisiones a velocidades altas.

## Otros parches

Se retiró el adaptador anterior que intentaba reconocer planeo con una ruta hipotética y modificar un integrador documentado sólo para movimiento a pie. Tampoco se usa ese integrador como si fuera un controlador de todas las monturas. Esos enfoques no satisfacían la definición de implementación completa.

Una firma puede tolerar que se mueva código. No demuestra que un parche conserve el ABI, la propiedad ni las animaciones. Para el perfil nativo se exige identidad exacta del ejecutable y sus testigos: cambiar su hash por el de otro archivo sin comprobar el contrato completo no es una actualización válida.

## Monturas

La velocidad por tipo basada en el integrador a pie se retiró: la fuente publica expresamente que su módulo excluye monturas y movimientos aéreos. Un offset supuesto no completa esa mecánica. Existe una alternativa pública distinta que eleva el atributo de velocidad de monturas al nivel 5 en 2.03.02. Tiene un alcance global; no identifica cada tipo de montura. Sus datos no permiten afirmar un multiplicador físico arbitrario por especie.

## Fuentes

- [Planeo: investigación, llamadas, velocidad y alcance de pruebas](https://github.com/ArasHueseyin/crimson-workbench/blob/0c86f9be5afdc007b4e9e8c3c93b12ac46837462/docs/research/GLIDE_SPEED.md).
- [Captura de identidad y capa activa](https://github.com/ArasHueseyin/crimson-workbench/blob/0c86f9be5afdc007b4e9e8c3c93b12ac46837462/runtime/marketplace/glide/glide_scope.h).
- [Huellas y tamaños de paquetes publicados](https://github.com/ArasHueseyin/crimson-workbench/blob/0c86f9be5afdc007b4e9e8c3c93b12ac46837462/runtime/marketplace/glide/glide_data.h).
- [ABI y compensación de velocidad entrante](https://github.com/ArasHueseyin/crimson-workbench/blob/0c86f9be5afdc007b4e9e8c3c93b12ac46837462/runtime/marketplace/glide/glide_policy.h).
- [Testigos del EXE](https://github.com/ArasHueseyin/crimson-workbench/blob/0c86f9be5afdc007b4e9e8c3c93b12ac46837462/runtime/marketplace/glide/glide_targets.h).
- [Integrador a pie: excluye expresamente monturas y movimiento aéreo](https://github.com/ArasHueseyin/crimson-workbench/blob/0c86f9be5afdc007b4e9e8c3c93b12ac46837462/docs/research/PLAYER_MOVEMENT_SPEED.md).
- [Consulta de montura y liberación de referencia](https://github.com/ArasHueseyin/crimson-workbench/blob/0c86f9be5afdc007b4e9e8c3c93b12ac46837462/runtime/marketplace/blackstar/blackstar_flight.h).
- [Alternativa de nivel 5 para monturas, código público](https://github.com/gutzufuss1477/Crimson-Desert-Mods/blob/99855e499d3033c82bd043beea3a326fcebf6e01/mods/real-infinite-horse-stamina-with-optional-lvl5-speed-for-all-hor/variants/all-mounts-level-5-speed/src/AllMountsLvL5Speed_20302.c). Su disponibilidad pública no implica licencia general para copiarlo; se citaron sus hechos técnicos sin incorporar su implementación.

## Pruebas reproducibles

```sh
g++ -std=c++20 -Wall -Wextra -Werror -pthread -Ipruebas/planeo_nativo_portatil/stubs -Isrc pruebas/planeo_nativo_portatil/prueba.cpp -o /tmp/prueba_planeo
/tmp/prueba_planeo
```

Las fixtures compilan el código real con memoria sintética. El clasificador de prueba se proporciona explícitamente; el clasificador de producción rechaza esos registros ficticios. Se verifican identidad, capas, mutación de cada uno de los 156 bytes, multiplicadores, ascenso/descenso, argumento y resultado nativos, original invocado una vez, delta inválido, restauración por escritura rechazada, estabilidad de 600 ciclos y rechazo de ejecutable no verificado. Los valores sintéticos no deben pegarse como datos del juego.

También se comprobó la compilación para Windows x64 con LLVM-MinGW y advertencias estrictas. **No se ejecutó este ASI en Crimson Desert.**
