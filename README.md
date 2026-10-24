# DesiertoSeguro 0.3 para Crimson Desert

Proyecto Windows x64 con menú dentro del juego, código fuente, dependencias incluidas, compilación automática y paquete ASI para Definitive Mod Manager. **INICIO (Home)** abre y cierra el menú; las teclas y el botón XInput se reasignan y se guardan.

**Esta revisión no cumple al 100 % todas las funciones originales.** Incluye implementaciones reales y alternativas con alcance explícito. Impedir cualquier registro de delito, revelar toda la niebla por región y activar todos los nexos por región siguen siendo capacidades parciales; también se distinguen la velocidad por tipo y las ranuras del equipo existente de sus alternativas. No se afirma que cambiar un offset complete un contrato que todavía se desconoce.

Para instalar el binario usa [INSTALAR](docs/INSTALAR.md). Para generar otro desde el navegador sigue [COMPILAR](docs/COMPILAR.md). **No se ejecutó este ASI dentro de Crimson Desert ni se probó su importación concreta en DMM.**

## Descargar e instalar

**[Descargar el ZIP compilado para DMM](distribucion/DesiertoSeguro-0.3-DMM.zip)**. Abre el archivo en GitHub y pulsa **Download raw file**. Ese paquete se conserva en el repositorio y no depende de la caducidad de Artifacts.

La [ejecución nativa de Windows y sus pruebas terminó correctamente](https://github.com/lddurazo23/Crimson-Desert/actions/runs/40127752982). El ZIP contiene directamente `DesiertoSeguro.asi`, instrucciones, licencias y `SHA256SUMS.txt`. Impórtalo en DMM con el juego cerrado y usa el loader administrado por DMM. Presiona Home dentro del juego.

## Base elegida

**C++20 x64, CMake, MinHook 1.3.4, Dear ImGui 1.91.9b, Direct3D 12 y Ultimate ASI Loader x64 administrado por DMM.** DirectX 12 figura en los requisitos oficiales; los mods abiertos revisados documentan esta base ASI y menús DX12. Se observa la swapchain y su command queue mediante interfaces COM públicas, y se interceptan presentación y redimensión. Las bibliotecas del menú y de hooks vienen incluidas y fijadas por revisión. [Fuentes](docs/FUENTES.md).

DMM **4.1.0**, actualizado el 24 de octubre de 2026 según su autor, admite ASI/DLL y administra el loader. El ZIP usa el precedente abierto de ASI en la raíz. No se inventó un manifiesto ni se usa `.cdmod`, cuyo soporte fue retirado del gestor.

## Estado por función

«Datos confirmados» significa publicados con fuente; **no significa probados en tu ejecutable**. La viabilidad presupone que el perfil y su contexto pasan las validaciones.

| Función y alcance real | Estado | Viabilidad y condición |
|---|---|---|
| Menú, teclas y preferencias | lógica completa + datos confirmados | Alta. Win32/XInput/DXGI públicos; Home por defecto. Requiere carga desde el arranque. |
| Free flight durante planeo | lógica completa + datos confirmados | Perfil nativo EXE 1.0.0.2976: identifica la capa y las animaciones publicadas, con ascenso/descenso y aceleración. Otro ejecutable o actor/animación no auditados quedan sin modificar. No se probó el vuelo de este proyecto en juego. |
| Salud infinita | lógica completa + datos confirmados | Hook y estadísticas publicados por Trinity; identidad fresca del personaje. Probar combate y mecánicas especiales. |
| Stamina infinita | lógica completa + datos confirmados | Interruptor independiente y tipos públicos; perfil compatible obligatorio. |
| Espíritu infinito | lógica completa + datos confirmados | Interruptor independiente y tipos públicos; perfil compatible obligatorio. |
| Velocidad de montura por tipo | parcial o inviable | No se verificó el integrador físico de cada especie. No se presenta un offset del jugador como si fuera de montura. |
| Alternativa: atributo de velocidad de monturas al nivel 5 | lógica completa + datos confirmados | Cambia el atributo global por la ruta publicada para 2.03.02; no selecciona especies ni aplica un multiplicador físico por tipo. Se conserva su alcance separado. |
| Ningún delito se registra | parcial o inviable | Falta un contrato universal verificado. |
| Alternativa: reglas de recompensa y exenciones conocidas | lógica completa + datos confirmados | Tablas publicadas, captura previa, restauración y validación de identidad. No cubre todos los delitos ni todas las reacciones de testigos/misiones. |
| Refinar absolutamente todo el equipo y todos los almacenes | parcial o inviable | No hay un contrato verificado para todos los tipos, recálculo y persistencia de cualquier almacén. La alternativa inferior tiene un alcance acotado. |
| Alternativa: editar nivel del equipo compatible de mochila y vestido | lógica completa + faltan solo marcadores | Perfil legado C0 bloqueado hasta dos AOB testigo reales. Usa el máximo de cada curva, hasta 10. Excluye tipos incompatibles; no se promete modificar todos los almacenes. Persistencia y atributos requieren prueba real. |
| Abrir todas las ranuras del equipo que ya tienes | parcial o inviable | La operación persistente sobre todas las piezas existentes no tiene un contrato completo demostrado en este proyecto. Se retiró el escritor de guardado hipotético. |
| Alternativa: crear una pieza nueva con cinco ranuras | lógica completa + datos confirmados | Perfil nativo 2976: constructor, conversión y otorgamiento reales, UID nuevo y comprobación de autoridad. No modifica tus piezas existentes. Catálogo de 2,455 claves reales con etiqueta numérica. |
| Hora y congelación | lógica completa + datos confirmados | Reloj lógico y sol publicados. Apagar libera el control; no deshace eventos ni tiempo transcurrido. |
| Clima | lógica completa + datos confirmados | Perfil nativo completo 2976, con hash y testigos exactos. Precipitación, nubes, humedad, viento, niebla atmosférica y nieve. |
| Inventario: añadir, eliminar y cantidad | lógica completa + faltan solo marcadores | Perfil legado C0 bloqueado hasta sus dos AOB testigo, además de las firmas públicas de cada ruta. Añadir tiene disponibilidad propia. Edición experimental de ambos mirrors; no se garantiza atomicidad del juego. |
| Quitar toda la niebla por región | parcial o inviable | No se verificaron la operación completa ni la correspondencia regional. Aprender un lugar no demuestra revelado cartográfico íntegro. |
| Activar todos los nexos por región | parcial o inviable | Descubrimiento, estado y acertijo son capas distintas; faltan contratos y correspondencias. No es una carencia de solo offsets. |
| Alternativa: editor individual de descubrimientos | lógica completa + datos confirmados | Catálogo real de conocimientos `Node` y `AbyssRuins`; solicitud individual por clave y confirmación del motor. No afirma revelar regiones ni habilitar viaje/acertijos. |

Las regiones del menú son etiquetas de selección y diagnóstico. **No ejecutan un desbloqueo regional supuesto.** Las alternativas tienen sus propios controles; desactivarlas cancela solicitudes pendientes, sin borrar cambios que ya quedaron en la partida.

## Qué cambió desde 0.2

Se integró un detector nativo real de planeo 2976; se añadió la creación nativa de una pieza con cinco ranuras y el editor de descubrimientos individuales; se retiraron el protocolo POD hipotético de sockets y los contenedores inventados de nexos. El editor C0 y su refinamiento quedan bloqueados antes de instalar sus hooks si faltan los dos testigos de layout.

También se corrigieron la disponibilidad parcial del alta de inventario, las colisiones al cargar teclas y el estado de fallo de inicialización. El renderer espera sus cercas con comprobación de pérdida real del dispositivo; no devuelve un error artificial de redimensión solo porque pasaron 100 ms. Estas correcciones no sustituyen una prueba de GPU y juego reales.

## Uso, datos y respaldo

- **Home** abre/cierra; **Esc** cierra. Cerrar el menú no desactiva funciones.
- Las opciones nuevas vienen apagadas. Una firma ausente, inválida o ambigua bloquea su ruta.
- Preferencias: **`%LOCALAPPDATA%\DesiertoSeguro\DesiertoSeguro.ini`**. Diagnóstico: `DesiertoSeguro.log`, en la misma carpeta.
- El único registro de offsets, firmas y perfiles es **[src/datos_juego.hpp](src/datos_juego.hpp)**. Busca **`PEGAR AQUÍ`**. [Cómo pegar datos](docs/DATOS.md).
- Los perfiles nativos 2976 exigen su hash exacto. Cambiar el hash por el de otro EXE sin estudiar su contrato no es actualizar el mod.

Antes de probar, cierra el juego y espera la sincronización de Steam Cloud. Presiona **Win+R**, pega **`%LOCALAPPDATA%\Pearl Abyss\CD\save`** y copia **toda la carpeta `save`**, incluidas las subcarpetas numéricas, a otra carpeta con fecha. [Ruta oficial](https://support.pearlabyss.com/crimsondesert/en-US/Faq/Home/Detail?_faqNo=616).

El parche comercial más reciente verificable en la consulta del 24 de octubre de 2026 sigue siendo **2.03.02**, del 23 de septiembre. No hay equivalencia pública comprobada entre ese número y el FILEVERSION `1.0.0.2976`; el hash decide la aceptación de los perfiles nativos.

[Pruebas y límites](docs/PRUEBAS.md), [registro del binario](docs/COMPILACION_ENTREGADA.md), [auditoría](docs/AUDITORIA_V3.md), [movimiento](docs/MOVIMIENTO.md) y [mapa/delitos](docs/AUDITORIA_MAPA_DELITOS_V3.md). Se revisó el DesertLink proporcionado sin ejecutarlo ni incorporar su implementación cerrada.
