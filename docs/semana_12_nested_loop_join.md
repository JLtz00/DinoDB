# Semana 12 - Nested Loop Join

## Objetivo

Agregar el primer operador binario del Query Processor: `NestedLoopJoin`.

## Alcance implementado

- Se implementa `NestedLoopJoin` como operador Volcano.
- Se reciben dos operadores hijos (`left` y `right`).
- Se recibe un predicado de join configurable.
- Se materializa el lado derecho para poder compararlo con cada tupla izquierda.
- Se concatenan los valores de ambas tuplas cuando el predicado coincide.

## Flujo

1. `open()` carga las tuplas del lado derecho.
2. `next()` recorre cada tupla izquierda contra las tuplas derechas.
3. Cuando el predicado es verdadero, retorna una tupla combinada.
4. `close()` limpia el estado interno.

## Pruebas

Se valida un join entre empleados y departamentos usando una igualdad entre
columnas.

## Pendiente

- Costos estimados.
- Joins indexados.
- Joins por bloques.
