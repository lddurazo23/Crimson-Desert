La prueba incluye `src/planeo_nativo.cpp` real. Los dobles de Windows y MinHook sólo
permiten ejecutar validaciones con memoria sintética; no abren el juego.

```sh
g++ -std=c++20 -Wall -Wextra -Werror -pthread -Ipruebas/planeo_nativo_portatil/stubs -Isrc pruebas/planeo_nativo_portatil/prueba.cpp -o /tmp/prueba_planeo
/tmp/prueba_planeo
```

El clasificador de prueba acepta exclusivamente sus propios registros ficticios.
El clasificador de producción conserva las huellas públicas y rechaza esos datos.
La fixture comprueba contexto, selección de capa, cada byte de la huella,
publicación de X/Y/Z, controles verticales, compensación horizontal, vuelta a la
ruta nativa, fallo parcial y rechazo del EXE. No demuestra estabilidad de vuelo o
colisiones reales ni compatibilidad de otro parche.
