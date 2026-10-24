# Vuelo y monturas en la versión 0.2

El backend ahora incluye un hook real al integrador nativo, reconocimiento del personaje, consultas de montura, perfiles por tipo y controles. Ya no se limita a guardar botones sin modificar el movimiento.

Faltan datos concretos consumidos por ese backend: la ruta y los bytes del estado real de planeo, y la ruta al integrador físico de la montura. Sin ellos las funciones permanecen desactivadas. No se presenta una caída lenta como planeo ni se supone que todas las especies utilicen el mismo integrador.

Los marcadores y las fuentes se explican en **[MOVIMIENTO](MOVIMIENTO.md)**. Después de pegar valores verificados en `src/datos_juego.hpp`, sigue **[COMPILAR](COMPILAR.md)**; no necesitas escribir otro adaptador si el contrato comprobado coincide con ese backend. Si la física real requiere otro contrato, los marcadores no resuelven esa diferencia.

# Tiempo: alcance de esta implementación

El control de tiempo usa los contratos publicados por Trinity para el reloj lógico en ambos dominios y los límites del administrador del sol. Los datos están centralizados en `src/datos_juego.hpp`. No hay offsets absolutos ni versiones inventadas.

La casilla activa el control de tiempo; la opción de congelar detiene el delta del reloj y mantiene el sol en su hora. La solicitud de hora y minuto se procesa dentro del hook del reloj, después de ejecutar la rutina original. Cambiar hora con congelación es la forma más consistente de sostener juntos el reloj y el sol. Al desactivar, se restauran los límites anteriores del sol si el mismo administrador sigue vigente y los campos siguen siendo los valores escritos por este mod. Cambiar la hora es una acción: desactivar no regresa automáticamente el reloj al momento anterior.

Las firmas provienen de [Trinity, revisión fijada](https://github.com/XeTrinityz/Trinity/blob/70c9a00dd6e10b2081d706a837756844c11f5c2b/src/game/offsets.h), con investigación fechada en julio de 2026. No fueron probadas contra tu ejecutable. Si falta alguna firma, hay varias coincidencias o un layout no pasa las validaciones, se bloquea o se detiene esa función. El juego conserva su rutina original. Esto reduce errores previsibles, pero no prueba estabilidad de una versión nueva sin ensayarla en el juego.
