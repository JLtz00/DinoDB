# Semana 13 - External Merge Sort

## Objetivo

Implementar un ordenamiento por corridas como base para operaciones de sort y
para futuros operadores que requieran orden.

## Alcance implementado

- Se agrega `ExternalMergeSort`.
- Se divide la entrada en corridas con un limite de memoria expresado en filas.
- Se ordena cada corrida por una columna clave.
- Se fusionan las corridas tomando el menor elemento disponible.
- Se normaliza `memory_limit_rows = 0` a una fila para evitar corridas vacias.
- Las corridas temporales se escriben como archivos binarios y se eliminan al
  finalizar el merge.

## Uso esperado

La entrada y salida final siguen usando `Table`, pero las corridas intermedias
se materializan en disco para replicar el flujo de un external merge sort:

1. Crear corridas.
2. Ordenar cada corrida.
3. Fusionar corridas.

## Pruebas

Se valida que una tabla desordenada quede ordenada por la columna seleccionada
y que las corridas temporales no queden como basura tras finalizar.

## Pendiente

- Fusion multiway con Buffer Manager.
- Integracion con `ORDER BY`.
