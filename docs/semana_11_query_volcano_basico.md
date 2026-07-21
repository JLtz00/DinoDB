# Semana 11 - Query Processor basico con modelo Volcano

## Objetivo

Iniciar la capa de procesamiento de consultas con operadores iteradores usando
el modelo Volcano.

## Alcance implementado

- Se define `Tuple` como contenedor simple de valores enteros.
- Se define `Table` como coleccion de tuplas en memoria.
- Se agrega la interfaz abstracta `Operator`.
- Se implementa `SeqScan` para recorrer una tabla.
- `SeqScan` tambien puede recorrer una `PersistentTable` almacenada en paginas
  con Slot Directory.
- Se implementa `Selection` para filtrar tuplas con un predicado.
- Se implementa `Projection` para seleccionar columnas.

## Modelo Volcano

Cada operador expone tres metodos:

- `open()`: inicializa el operador.
- `next()`: produce la siguiente tupla disponible.
- `close()`: libera estado temporal.

Este patron permite encadenar operadores y ejecutar consultas de forma
incremental.

## Pruebas

Se valida una tuberia `SeqScan -> Selection -> Projection`, verificando que el
filtro y la proyeccion produzcan las filas esperadas. Tambien se valida el
recorrido secuencial desde paginas persistentes.

## Pendiente

- Operadores binarios.
- Joins.
- Ordenamiento externo.
- Planner SQL/catalogo general.
