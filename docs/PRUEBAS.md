# Pruebas y límites de la revisión 0.3

El runner `python3 scripts/probar_portatil.py --compilador g++` compila el código de producción con memoria sintética y se detiene ante cualquier error. No abre el juego ni modifica guardados.

| Grupo | Evidencia comprobada |
|---|---|
| Patrones | Gramática, comodines, unicidad, límites y desbordes. |
| Tiempo | Dos relojes, delta, sol, congelación, restauración y escritura rechazada. |
| Teclas | Conservación de Home/personalización y eliminación de colisiones al cargar. |
| Planeo nativo | Identidad y capa activas, huellas completas, 600 ciclos, 156 mutaciones, controles verticales, compensación, rollback y rechazo de EXE. |
| Monturas: callback | Selector, límites, alineación, direcciones y desactivación en el callback real. |
| Monturas: gateway | Ensamblador real; 200 ciclos preservan 15 GPR, banderas y los 16 YMM tras un callback que los destruye. Solo cambia RDX. Si CPU/sistema no ofrecen XSAVE/AVX, la prueba lo identifica; el mod tampoco instala su hook cuando no puede preservar el contexto. |
| Equipo nuevo | Contrato de tarea/autoridad, constructor, cinco registros nativos, conversión, otorgamiento, UID nuevo, cancelación y resultado incierto. |
| Equipo legado | Curva por tipo, identidad de ambos mirrors, planificación, rollback, cancelación y bloqueo de diseño C0 sin verificar. |
| Clima | Identidad/fases, lluvia/nieve, escalas, restauración, cambios ajenos, contención y solicitud temprana. |
| Descubrimientos | Catálogo real Node/AbyssRuins, autoridad temporal 509, confirmación asíncrona, cancelación e identidad. |
| Delitos parciales | Restauración, carga tardía, cambio de tablas, conflictos, escritura rechazada y reintento. |

Los valores de las fixtures son exclusivamente de prueba. No son offsets ni plantillas autorizadas del juego. Se retiraron los escritores hipotéticos de nexos y sublistas de sockets; no se mantiene una prueba sintética como evidencia de que esas mecánicas existen.

La compilación cruzada también ensambla el gateway GAS para Windows. Su equivalente MASM se revisó con LLVM ML, omitiendo únicamente la directiva CASEMAP que ese ensamblador no soporta. GitHub Actions usa el ensamblador MASM de Visual Studio para el resultado nativo.

El workflow ejecuta las pruebas en Ubuntu y compila Windows x64 con MSVC. CTest verifica el parser, las 33 dependencias fijadas y el formato PE32+/DLL/AMD64. El artifact solo se produce si esos controles pasan.

No se han probado dentro de Crimson Desert la aceptación visual, persistencia, cobertura de personajes/monturas, ABI del parche instalado, estabilidad DX12 ni la importación concreta en DMM. Una compilación o una simulación no representa esa sesión. La pérdida de GPU puede pausar la redimensión mientras se sincronizan recursos; el renderer conserva el resultado real de DXGI.
