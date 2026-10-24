Desde la raíz del proyecto, en Linux con un compilador C++20:

```sh
g++ -std=c++20 -Wall -Wextra -Werror -pthread -Ipruebas/equipo_nativo_portatil/stubs -Isrc pruebas/equipo_nativo_portatil/prueba.cpp -o /tmp/prueba_equipo_nativo
/tmp/prueba_equipo_nativo
```

La fixture incluye `src/equipo_nativo.cpp` real. Los destinos nativos, buffers,
handles, identidades y estados son sintéticos. Nunca abre el juego ni usa un ASI.
Comprueba rechazo de perfil ausente, selección y contexto con referencia prestada,
realm incorrecto, caller incorrecto, la llamada original, catálogo, cancelación,
vencimiento, contexto vencido, cinco registros, conversión, UID nuevo, cantidad,
rechazo previo al alta y bloqueo ante errores o efectos inciertos de la transacción.
Los valores de prueba no son datos que se deban pegar al juego.
