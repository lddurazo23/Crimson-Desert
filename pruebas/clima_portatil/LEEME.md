Estas pruebas compilan la implementación real de `src/clima.cpp` con memoria y
destinos nativos sintéticos. No cargan un ASI ni abren un proceso del juego.

Desde la raíz del proyecto:

```sh
c++ -std=c++20 -Wall -Wextra -Werror -pthread -I pruebas/clima_portatil/stubs -I src pruebas/clima_portatil/prueba.cpp -o /tmp/prueba_clima
/tmp/prueba_clima
```

Los casos comprueban identidad y vínculos del propietario, fase y etapa del campo,
límites y valores no finitos, huecos excluidos de la copia, lluvia y nieve,
confirmación por máscara del consumidor, restauración del estado y modo manual
previos, continuación automática, abandono por cambios ajenos y sustitución del
recurso, vencimiento del testigo y revocación inmediata del buffer inactivo.
Incluyen una solicitud anterior al primer campo, contención del mutex desde otro
hilo y cancelación de la espera automática al cambiar el recurso o la etapa.
También comprueban que se conserva el delta de la llamada original y que un archivo
EXE no verificado impide crear hooks.

Los fixtures no prueban aceptación visual en el juego, compatibilidad de un
parche diferente ni la ejecución real de los setters nativos.
