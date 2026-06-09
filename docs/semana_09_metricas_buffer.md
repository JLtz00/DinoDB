# Semana 9 - Metricas del Buffer Manager

## Objetivo

Agregar instrumentacion al Buffer Manager para medir el comportamiento del cache
durante operaciones de indice y pruebas de rendimiento.

## Alcance implementado

- Se contabilizan aciertos de cache (`cache_hits`).
- Se contabilizan fallos de cache (`cache_misses`).
- Se expone `hit_rate()` como proporcion de aciertos sobre accesos totales.
- Se agrega `reset_metrics()` para reiniciar mediciones entre escenarios.

## Criterio de medicion

Un `fetch_page` cuenta como hit cuando la pagina ya esta cargada en el Buffer
Pool. Cuenta como miss cuando debe buscarse una victima o frame libre para traer
la pagina desde disco.

## Pruebas

Se valida:

- Primer acceso como miss.
- Segundo acceso a la misma pagina como hit.
- Calculo correcto del `hit_rate`.
- Reinicio de contadores con `reset_metrics`.

## Utilidad

Estas metricas permiten comparar patrones de acceso del B+ Tree, verificar que
los rangos reutilizan paginas ya cargadas y preparar mediciones mas completas
para las semanas de consultas.
