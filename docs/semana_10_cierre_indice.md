# Semana 10 - Cierre de fase del indice

## Objetivo

Consolidar la fase del B+ Tree antes de avanzar al Query Processor, dejando
documentado el estado funcional y las limitaciones conocidas.

## Estado funcional

- Insercion de claves enteras asociadas a `RID`.
- Actualizacion de claves repetidas.
- Busqueda puntual.
- Split de hojas.
- Raiz interna de altura 2.
- Escaneo por rango recorriendo hojas enlazadas.
- Integracion con Buffer Manager para lectura y escritura de paginas.

## Validacion adicional

Se agregan pruebas de borde para confirmar que un rango invertido no devuelve
resultados y que el indice mantiene una respuesta consistente ante consultas sin
coincidencias.

## Limitaciones conocidas

- Aun no se implementa split recursivo de nodos internos.
- La raiz del indice vive en memoria durante la vida del objeto `BPlusTree`.
- No existe un catalogo de indices ni persistencia de metadatos del arbol.

## Siguiente fase

La semana 11 inicia el Query Processor con el modelo Volcano, usando operadores
iteradores sobre tablas en memoria y preparando la integracion posterior con
Storage, Buffer e Index.
