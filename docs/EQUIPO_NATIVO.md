# Equipo: alcance real y alternativa nativa

La solicitud original incluye mejorar todo el equipo que ya tienes y abrir todas
sus ranuras. Hay dos rutas distintas en el proyecto. No se presentan como una sola
función terminada.

| Operación | Estado honesto | Condición y límite |
|---|---|---|
| Refinar piezas existentes | Lógica de edición implementada; necesita dos testigos de layout C0 y la firma publicada | Experimental. Escribe ambos mirrors, respeta el máximo por tipo hasta 10, solicita actualización visual solo del cliente y bloquea ante fallos. Estadísticas y persistencia requieren validación real. |
| Abrir todas las ranuras de piezas existentes | Parcial: el contrato persistente no está verificado | Se eliminó el esquema hipotético de sublista POD. Un algoritmo con un formato supuesto no queda completo al pegar offsets. |
| Crear una pieza nueva con cinco ranuras nativas | Alternativa implementada de punta a punta para el perfil publicado 2976 | Catálogo público de 2,455 claves externas; constructor, conversión, alta nativa y destructor reales. No modifica el equipo existente ni conserva sus mejoras, tintes o gemas. Requiere prueba dentro del juego. |

## Qué hace la alternativa

La solicitud nace en el menú y conserva únicamente la clave externa, la identidad
completa del personaje y un vencimiento de 30 segundos. El hook del wrapper nativo
de tareas ejecuta la función original una vez y solo consume trabajo después de
una tarea exitosa con referencia prestada al actor de autoridad. Comprueba los dos
callers publicados, TLS realm 1, el contexto TLS existente, las vtables del perfil,
los vínculos de propietario/controlador/inventario y el handle con generación del
personaje seleccionado. No cambia de realm, no fabrica un contexto y no guarda
punteros a actores entre hilos.

Se resuelve la clave externa a un tipo interno usando el lookup real. La pieza se
construye con cantidad uno. Solo continúa cuando el constructor del motor produjo
cinco registros válidos ya construidos y capacidad suficiente. Conserva sus campos
nativos y abre esos cinco registros. La conversión real debe conservar UID nuevo
`-1`, clave externa, cantidad uno, cinco registros vacíos y el contador inline
cinco; cualquier discrepancia se rechaza antes del alta.

El destructor libera el valor temporal antes de llamar a la transacción nativa de
alta. El juego conserva asignación de UID, colocación, eventos y notificaciones.
Después se exige un incremento exacto de uno, una sola instancia nueva con UID
propio, cantidad uno y cinco ranuras en el inventario de autoridad. Un error después
de entrar a esa ruta deja resultado incierto y bloquea nuevas operaciones durante
la sesión. Nunca se repite la petición automáticamente.

Una respuesta exitosa verifica el estado de autoridad observado, no certifica la
actualización visual ni el archivo de guardado. Prueba una sola pieza en una copia
de partida, abre el inventario, guarda, cierra y recarga. Si un dato no coincide,
el mensaje lo indica sin presentar la operación como confirmada.

## Catálogo e instalación

Las claves vienen de la lista de equipo que publicó Workbench para esta compilación.
Se muestran como **Equipo con clave real …**, porque esta ruta no tiene una fuente
verificada para nombres localizados. No se mezclan con los tipos internos ni los
nombres del catálogo C0 del editor legado. Elige una entrada desde el menú; no se
te pide editar código ni inventar un ID.

Es una ruta nativa dentro del mismo ASI. No requiere CrimsonLiveItems, CrimsonExtraSockets,
un pipe ni otro archivo de configuración. Comparte las preferencias del menú del
proyecto y su instalación ASI mediante DMM. Una pieza ya creada no se borra al apagar
la opción. Apagar cancela solicitudes que todavía esperan.

## Perfil y fuentes

Todos los datos se concentran en `src/datos_juego.hpp`, bloque `equipoNativo`.
El motor requiere SHA-256 exacto del EXE, timestamp PE, tamaño de imagen y los hashes
completos de trece rutinas antes de instalar el hook. En un ejecutable diferente
queda no disponible. No se intenta convertir una coincidencia AOB en prueba de un
ABI o layout distinto.

Fuente MIT fijada: [ArasHueseyin/crimson-workbench, revisión 0c86f9b](https://github.com/ArasHueseyin/crimson-workbench/tree/0c86f9be5afdc007b4e9e8c3c93b12ac46837462).

- [Live-Items Build 2976](https://github.com/ArasHueseyin/crimson-workbench/blob/0c86f9be5afdc007b4e9e8c3c93b12ac46837462/runtime/live-items/BUILD_2976.md): contexto con referencia nativa, rutas y límites.
- [native.h](https://github.com/ArasHueseyin/crimson-workbench/blob/0c86f9be5afdc007b4e9e8c3c93b12ac46837462/runtime/live-items/src/native.h): firmas tipadas, constructor, conversión, alta y destructor.
- [runtime.cpp](https://github.com/ArasHueseyin/crimson-workbench/blob/0c86f9be5afdc007b4e9e8c3c93b12ac46837462/runtime/live-items/src/runtime.cpp): hash del EXE y trece hashes completos de rutinas.
- [Extra Sockets Build 2976](https://github.com/ArasHueseyin/crimson-workbench/blob/0c86f9be5afdc007b4e9e8c3c93b12ac46837462/runtime/extra-sockets/BUILD_2976.md): serialización nativa de cinco entradas y límite de InitData.
- [socket_policy.h](https://github.com/ArasHueseyin/crimson-workbench/blob/0c86f9be5afdc007b4e9e8c3c93b12ac46837462/runtime/extra-sockets/src/socket_policy.h): ItemVal C8, vector nativo y registros de seis bytes.
- [gear.h](https://github.com/ArasHueseyin/crimson-workbench/blob/0c86f9be5afdc007b4e9e8c3c93b12ac46837462/runtime/extra-sockets/src/gear.h): catálogo de claves externas admitidas.

La fuente también declara que falta aceptación real dentro del juego. Esta entrega
no ejecutó Crimson Desert. Sus fixtures y compilaciones no se describen como pruebas
de una partida.
