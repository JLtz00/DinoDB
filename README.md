# DinoDB - Mini SGBD en C++

### Proyecto - Base de Datos II

![C++](https://img.shields.io/badge/C++-17-blue.svg)
![CMake](https://img.shields.io/badge/CMake-3.20+-green.svg)
![Estado](https://img.shields.io/badge/Estado-Avance%20funcional-yellow)
![Licencia](https://img.shields.io/badge/Licencia-Academica-lightgrey)

DinoDB es un Mini Sistema Gestor de Bases de Datos desarrollado en C++17 con fines academicos. El objetivo del proyecto es construir desde cero los componentes internos basicos de un motor de almacenamiento y consultas: paginas persistentes, Buffer Manager, indice B+ Tree y operadores relacionales bajo el modelo Volcano.

---

## Estado Actual

El proyecto ya cuenta con una base funcional y probada para la sustentacion practica. La suite automatizada actual valida Storage, Buffer, B+ Tree, metricas, operadores de consulta, `IndexScan` y la demo final.

| Modulo | Estado | Alcance actual |
|--------|--------|----------------|
| Storage Manager | Implementado y probado | Archivos binarios, paginas fijas de 4 KB, Slot Directory, insercion, lectura, eliminacion, compactacion y persistencia. |
| Buffer Manager | Implementado y probado | Buffer Pool configurable, Page Table, LRU, pin/unpin, dirty bit, flush y metricas de hit rate. |
| B+ Tree Index | Implementado y probado | Nodos almacenados como paginas mediante Buffer Manager, busqueda puntual, rango, split de hojas, split de nodos internos, altura mayor a 2 y metadatos persistentes. |
| Query Processor | Avance funcional | Modelo Volcano con `SeqScan` en memoria o sobre paginas persistentes, `Selection`, `Projection`, `NestedLoopJoin`, `ExternalMergeSort` con runs temporales e `IndexScan`. |
| Demo final | Implementada | Ejecutable `dinodb_demo` que carga 10,000 registros, construye indice, compara scan secuencial vs indice y muestra metricas del Buffer Manager. |

---

## Arquitectura

El sistema esta organizado en capas. Cada capa superior depende de los servicios de la capa inferior.

```text
+-------------------------------------+
| Query Processor  (Volcano)          |
| SeqScan, Selection, Projection,     |
| NestedLoopJoin, ExternalMergeSort,  |
| IndexScan                           |
+-------------------------------------+
| B+ Tree Index                       |
| insert, search, range_scan, splits, |
| metadata persistente                |
+-------------------------------------+
| Buffer Manager                      |
| Buffer Pool, Page Table, LRU,       |
| pin/unpin, dirty bit, hit rate      |
+-------------------------------------+
| Storage Manager                     |
| FileWriter, DiskManager, Page,      |
| Slot Directory, fsync               |
+-------------------------------------+
```

---

## Estructura del Repositorio

```text
DinoDB/
├── include/
│   ├── buffer/          # BufferManager, LRUReplacer
│   ├── common/          # Types, RID, constantes comunes
│   ├── index/           # BPlusTree
│   ├── query/           # Operadores Volcano e IndexScan
│   └── storage/         # Page, DiskManager, FileWriter
├── src/
│   ├── buffer/
│   ├── demo/            # dinodb_demo.cpp
│   ├── index/
│   ├── query/
│   └── storage/
├── tests/               # Pruebas con Google Test
├── docs/                # Avances/documentacion semanal
├── data/                # Archivos .db generados en ejecucion
├── CMakeLists.txt
└── README.md
```

---

## Compilacion

Requisitos:

- Compilador C++17: GCC 12+ o Clang 15+.
- CMake 3.20 o superior.
- Git.

Se recomienda usar un directorio de build nuevo. Si existe un `build/` antiguo generado desde otra ruta, puede fallar por cache de CMake.

```bash
cd /home/lorenzo/UNSA/2026A/BDII/DinoDB
cmake -S . -B build-local
cmake --build build-local
```

Ejecutar pruebas:

```bash
cd build-local
ctest --output-on-failure
```

Ejecutar benchmarks reproducibles:

```bash
cd /home/lorenzo/UNSA/2026A/BDII/DinoDB
./build-local/bench_scan_vs_index 10000 7777
./build-local/bench_buffer_hit_rate 256
```

Ejecutar la demo final:

```bash
cd /home/lorenzo/UNSA/2026A/BDII/DinoDB
./build-local/dinodb_demo
```

Salida esperada aproximada:

```text
DinoDB demo final
Registros cargados: 10000
Altura B+ Tree: 3
Busqueda secuencial key=7777: encontrado en ... us
Busqueda por indice key=7777: encontrado en ... us
Buffer hits: ...
Range IndexScan [100,109]: 10 tuplas
```

---

## Pruebas Automatizadas

La suite cubre los modulos principales:

- `test_storage_full`: `FileWriter`, `Page`, `DiskManager`, serializacion y persistencia.
- `test_buffer_manager`: Buffer Pool, LRU, pin/unpin, dirty bit, flush y delete.
- `test_bplus_tree`: insercion, busqueda, actualizacion, rangos, split de hojas, split interno, altura mayor a 2 y reapertura del indice desde disco.
- `test_buffer_metrics`: aciertos, fallos, `hit_rate` y reinicio de metricas.
- `test_query_basic`: `SeqScan` en memoria, `SeqScan` sobre paginas persistentes, `Selection` y `Projection`.
- `test_query_join`: `NestedLoopJoin`.
- `test_query_sort`: `ExternalMergeSort` con runs temporales persistidos.
- `test_query_index`: `IndexScan` puntual y por rango usando B+ Tree.

Ultima verificacion realizada en build temporal: 50/50 pruebas exitosas.

---

## DinoDB CLI - Semana 13

`dinodb_cli` es una demo por comandos enfocada en el avance hasta Semana 13: Storage Manager, Buffer Manager y B+ Tree persistente. No intenta ser SQL ni cubrir los operadores de Semana 14/15.

Comandos principales:

```bash
./build-local/dinodb_cli init
./build-local/dinodb_cli insert 10 100
./build-local/dinodb_cli insert-bulk 10000
./build-local/dinodb_cli find 7777
./build-local/dinodb_cli range 100 109
./build-local/dinodb_cli stats
./build-local/dinodb_cli reopen-check 7777
```

La CLI usa por defecto:

```text
data/dinodb_cli_table.db
data/dinodb_cli_index.db
```

Tambien se puede indicar otro directorio al final de cada comando:

```bash
./build-local/dinodb_cli init data/semana13
./build-local/dinodb_cli insert-bulk 10000 data/semana13
./build-local/dinodb_cli find 7777 data/semana13
```

Esta demo permite mostrar que los registros se guardan en paginas/slots reales, que el indice B+ persiste en disco, que se puede reabrir el arbol y que las busquedas/rangos pasan por el Buffer Manager.

---

## Demo Practica

`dinodb_demo` esta pensado para la sustentacion, no como una consola interactiva. La demo realiza automaticamente los pasos minimos solicitados por la consigna:

1. Crea un archivo de indice en `data/dinodb_demo_index.db`.
2. Carga 10,000 registros de prueba.
3. Construye un B+ Tree persistido mediante Buffer Manager.
4. Ejecuta una busqueda secuencial sobre la tabla.
5. Ejecuta una busqueda por indice usando `IndexScan`.
6. Ejecuta un rango `[100,109]`.
7. Muestra altura del arbol, tiempos aproximados y metricas del Buffer Manager.

Limitacion importante: `SeqScan` ya puede leer desde paginas persistentes mediante `PersistentTable`; la demo principal conserva una tabla en memoria para comparar facilmente con `IndexScan`.

---

## Roadmap de Prioridad Intermedia

Estas mejoras aumentarian la fidelidad del proyecto como Mini SGBD, pero no son estrictamente necesarias para demostrar los modulos obligatorios ya implementados.

| Mejora | Estado | Objetivo |
|--------|--------|----------|
| `SeqScan` sobre paginas reales | Implementado | `PersistentTable` serializa tuplas en slots reales y `SeqScan` puede recorrerlas desde disco. |
| Tabla persistente | Implementado basico | `PersistentTable` inserta tuplas en paginas y devuelve `RID`; falta catalogo/esquema general. |
| `ExternalMergeSort` con runs temporales | Implementado | Escribe runs binarios temporales y los consume durante el merge. |
| Benchmarks reproducibles | Implementado | `bench_scan_vs_index` y `bench_buffer_hit_rate` generan salida CSV. |
| CLI basico | Pendiente | Exponer comandos como `load`, `find`, `range`, `stats` y `demo` desde terminal. |
| Build local recomendado | Documentado | `build-local/` esta ignorado por Git; regenerar con `cmake -S . -B build-local`. |

---

## Roadmap de Prioridad Baja / Documentacion

Estas tareas mejoran la presentacion final, mantenibilidad y claridad del repositorio.

| Mejora | Estado | Objetivo |
|--------|--------|----------|
| Informe final en PDF | Pendiente | Consolidar arquitectura, snippets clave, resultados y conclusiones. |
| Diagramas renderizados | Pendiente | Incluir diagramas de arquitectura, flujo de Buffer Manager y estructura del B+ Tree. |
| Capturas de consola | Pendiente | Agregar evidencia visual de compilacion, pruebas y ejecucion de `dinodb_demo`. |
| Tabla de resultados | Pendiente | Documentar tiempos de scan vs indice y metricas de hit rate bajo varias cargas. |
| Presentacion de sustentacion | Pendiente | Preparar diapositivas con decisiones de diseno y demostracion practica. |
| Comentarios tecnicos finales | Parcial | Revisar que las funciones criticas tengan comentarios utiles sin sobrecomentar codigo evidente. |

---

## Limitaciones Conocidas

- No hay parser SQL. Las consultas se construyen directamente con operadores C++.
- No hay transacciones, WAL, rollback ni control de concurrencia.
- No hay catalogo general de tablas e indices; el B+ Tree tiene metadatos propios, pero no existe un catalogo global del SGBD.
- `IndexScan` todavia consume una `Table` en memoria; `SeqScan` ya soporta `PersistentTable`.
- `ExternalMergeSort` usa runs temporales persistidos, pero el resultado final se materializa como `Table` en memoria.
- No existe eliminacion completa en B+ Tree con redistribucion/fusion; la consigna la considera opcional o bonus.
- `dinodb_demo` es reproducible y util para sustentacion, pero no es una aplicacion interactiva.

---

## Entregables Relacionados con la Consigna

Estado frente a los puntos principales del trabajo final:

| Requisito | Estado | Evidencia |
|-----------|--------|-----------|
| Storage Manager con archivos binarios | Cumplido | `FileWriter`, `DiskManager`, `Page`. |
| Paginas fijas de 4 KB | Cumplido | `PAGE_SIZE = 4096`. |
| Slot Directory | Cumplido | `PageHeader`, `SlotEntry`, operaciones de slot. |
| Buffer Pool configurable | Cumplido | `BufferManager(pool_size, disk)`. |
| LRU obligatorio | Cumplido | `LRUReplacer`. |
| Pin, unpin y dirty bit | Cumplido | `BufferManager::fetch_page`, `unpin_page`, `flush_page`. |
| B+ Tree integrado con Buffer Manager | Cumplido | Nodos leidos/escritos mediante `BufferManager`. |
| Split de hojas e internos | Cumplido | Insercion recursiva y propagacion de splits. |
| Metadatos persistentes del indice | Cumplido | Pagina header del B+ Tree. |
| Volcano Model | Cumplido parcialmente | Operadores `open`, `next`, `close`. |
| Selection y Projection | Cumplido | `Selection`, `Projection`. |
| Join | Cumplido | `NestedLoopJoin`. |
| External Merge Sort | Cumplido parcialmente | Runs temporales en disco y merge; resultado final materializado en memoria. |
| Uso de indice en consultas | Cumplido parcialmente | `IndexScan` y benchmark scan vs indice; falta planner/CLI. |
| Demo con 10,000 registros | Cumplido | `dinodb_demo`. |
| Informe PDF, diagramas y capturas | Pendiente | Debe prepararse como entregable final. |

---

## Convencion de Ramas Git

```text
main
├── feature/storage-manager
├── feature/buffer-pool-lru
├── feature/bplus-tree
├── feature/query-processor
├── feature/demo-final
└── fix/nombre-del-bug
```

Prefijos recomendados:

| Prefijo | Uso |
|---------|-----|
| `feat/` | Funcionalidad nueva. |
| `fix/` | Correccion de bugs. |
| `docs/` | Cambios de documentacion. |
| `test/` | Nuevas pruebas o correcciones de pruebas. |
| `refactor/` | Reorganizacion interna sin cambiar comportamiento. |

Ejemplos:

```text
feat(index): implementar split de nodos internos
test(query): agregar pruebas de IndexScan
docs(readme): actualizar estado y roadmap
```

---

## Integrantes

| Nombre |
|--------|
| Carlos Enrique Gutierrez Castilla |
| Fernando Antonio Gama Llicahua |
| Job Lorenzo Quispe Torrez |
| Diego Mauricio Villanueva Flores |

---

## Informacion Academica

- Universidad: Universidad Nacional de San Agustin de Arequipa.
- Curso: Base de Datos II.
- Docente: Maria Vilma Escobar Castillo.
- Año: 2026.

---

## Referencias Bibliograficas

1. Ramakrishnan, R. & Gehrke, J. *Database Management Systems*, 3rd ed. McGraw-Hill, 2003.
2. Graefe, G. *Volcano - An Extensible and Parallel Query Evaluation System*. IEEE TKDE, 1994.
3. Comer, D. *The Ubiquitous B-Tree*. ACM Computing Surveys, 11(2), 1979.
4. Silberschatz, A. et al. *Database System Concepts*, 7th ed. McGraw-Hill, 2019.
