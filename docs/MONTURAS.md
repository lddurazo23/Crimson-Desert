# Velocidad global de monturas, nivel 5

Alternativa implementada: aplica el atributo de velocidad al nivel 5 por el acceso nativo publicado para 2.03.02. No ofrece multiplicadores ni selección por especie. El autor de la investigación declara una comprobación con Rokade; la cobertura de otras monturas es inferida del selector y necesita prueba real.

[Fuente pública y revisión fija](https://github.com/gutzufuss1477/Crimson-Desert-Mods/blob/99855e499d3033c82bd043beea3a326fcebf6e01/mods/real-infinite-horse-stamina-with-optional-lvl5-speed-for-all-hor/variants/all-mounts-level-5-speed/src/AllMountsLvL5Speed_20302.c). Se utilizan hechos de esa investigación y un gateway original; no se copia su manejador VEH ni su mecanismo de single-step.

Se exigen dos AOB únicos y su distancia publicada. El callback reconoce el campo por relación de registros, selector, subtipo, ordinal, máximo, mínimo y paso. La escritura usa CAS sobre datos alineados y escribibles; actualiza también el argumento RDX requerido por la ruta publicada. Una página rechazada desactiva el modo.

El gateway MinHook preserva GPR, banderas y el estado extendido XSAVE/XRSTOR habilitado por el sistema. Rechaza CPU o tamaños no soportados. Hay variantes MASM y GAS del mismo flujo, con marco de unwind en Windows. La prueba usa el ensamblador real y destruye deliberadamente GPR/YMM dentro del callback para comprobar restauración.

Apagar detiene futuras aplicaciones. No reduce valores ya aceptados por el juego; el atributo podría conservarse en la partida. Respalda antes de usarlo. No se probó este mod dentro del juego.
