# Semana 7 - Inicio del indice B+ Tree

## Objetivo

Implementar la primera version del modulo de indices con un B+ Tree conectado al
Buffer Manager. Esta semana se concentra en crear la estructura base del indice,
almacenar sus nodos dentro de paginas y soportar inserciones y busquedas
puntuales.

## Alcance implementado

- Se agrega el modulo `index` al sistema de compilacion.
- Se define `BPlusTree` como interfaz principal del indice.
- Se representa cada entrada del indice como par `key -> RID`.
- Se crea una pagina raiz inicial de tipo hoja usando `BufferManager::new_page`.
- Se implementa insercion ordenada dentro de una hoja.
- Se implementa busqueda puntual por clave.
- Se actualiza una clave existente reemplazando su `RID`.

## Relacion con capas anteriores

El B+ Tree no escribe directamente en disco. Todas las paginas del indice se
crean, leen y modifican mediante el Buffer Manager, manteniendo la separacion de
responsabilidades:

- Storage Manager administra paginas persistentes.
- Buffer Manager controla el cache, pinning, dirty pages y flush.
- Index usa esas paginas como contenedores de nodos.

## Pruebas

Se agrega una prueba automatizada para validar:

- Insercion de claves en el indice.
- Recuperacion del `RID` asociado a una clave existente.
- Resultado vacio cuando la clave no existe.
- Actualizacion del `RID` cuando se inserta una clave repetida.

## Pendiente para semanas posteriores

- Split de hojas.
- Creacion de raiz interna.
- Busqueda por rango.
- Splits internos y mayor cobertura de pruebas.
