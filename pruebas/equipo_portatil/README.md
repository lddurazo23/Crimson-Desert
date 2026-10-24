Ejecuta `python3 pruebas/equipo_portatil/ejecutar.py` desde la raíz del proyecto,
con Python 3 y un compilador C++20 (`--compilador g++`, por ejemplo).

La fixture compila `src/equipo.cpp` real con memoria sintética y adaptadores
locales de Windows. Comprueba el máximo por tipo, identidad de ambos mirrors,
planificación sin escrituras, restauración, cancelación y bloqueo cuando el
layout C0 no tiene testigos verificados. No abre el juego ni valida su ABI.

Se eliminó la simulación del esquema POD de guardado de ranuras: un contrato
ficticio que pasa pruebas no demuestra cómo guarda el juego. La alternativa de
creación nativa tiene su propia prueba en `pruebas/equipo_nativo_portatil`.
