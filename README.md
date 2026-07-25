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
| Query Processor | Implementado y probado | Parser SQL con `INT`, `TEXT`, `DATE` y `HOUR`, catalogo persistente y planes Volcano con `SeqScan`, `Selection`, `Projection`, `NestedLoopJoin`, `ExternalMergeSort` e `IndexScan`. |
| Demo final | Implementada | Ejecutable `dinodb_demo` que carga 10,000 registros, construye indice, compara scan secuencial vs indice y muestra metricas del Buffer Manager. |

---

## Arquitectura

El sistema esta organizado en capas. Cada capa superior depende de los servicios de la capa inferior.

```text
+-------------------------------------+
| SQL + Catalogo + Query Processor    |
| Parser, SeqScan, Selection,         |
| Projection,                         |
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
├── data/                # Se genera en ejecucion; no versionada
├── CMakeLists.txt
└── README.md
```

---

## Compilacion

Requisitos:

- Compilador C++17: GCC 12+ o Clang 15+.
- CMake 3.20 o superior.
- Git.
- Conexion a Internet durante la primera configuracion para descargar Google
  Test mediante CMake.

En Debian o Ubuntu se pueden instalar las herramientas con:

```bash
sudo apt update
sudo apt install build-essential cmake git
```

Los comandos siguientes se ejecutan desde la raiz del repositorio. Se
recomienda usar un directorio de build nuevo. Si existe uno generado desde otra
ruta, puede fallar por cache de CMake.

```bash
cmake -S . -B build-local
cmake --build build-local
```

Los ejecutables quedan dentro de `build-local/`. Para verificar la instalacion:

```bash
./build-local/dinodb_cli --help
```

Ejecutar benchmarks reproducibles:

```bash
./build-local/bench_scan_vs_index 10000 7777
./build-local/bench_buffer_hit_rate 256
```

Ejecutar la demo final:

```bash
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

Ejecutar toda la suite desde la raiz del repositorio:

```bash
ctest --test-dir build-local --output-on-failure
```

La suite cubre los modulos principales:

- `test_storage_full`: `FileWriter`, `Page`, `DiskManager`, serializacion y persistencia.
- `test_buffer_manager`: Buffer Pool, LRU, pin/unpin, dirty bit, flush y delete.
- `test_bplus_tree`: insercion, busqueda, actualizacion, rangos, split de hojas, split interno, altura mayor a 2 y reapertura del indice desde disco.
- `test_buffer_metrics`: aciertos, fallos, `hit_rate` y reinicio de metricas.
- `test_query_basic`: `SeqScan` en memoria, `SeqScan` sobre paginas persistentes, `Selection` y `Projection`.
- `test_query_join`: `NestedLoopJoin`.
- `test_query_sort`: `ExternalMergeSort` con runs temporales persistidos.
- `test_query_index`: `IndexScan` puntual y por rango usando B+ Tree.
- `test_sql_engine`: parser, catalogo, validacion de esquemas, persistencia y
  planes Volcano generados desde SQL.

Ultima verificacion: 63/63 pruebas exitosas.

---

## DinoDB SQL

Esta seccion contiene todo lo necesario para utilizar el lenguaje de consultas.

### Inicio rapido

Abrir una base persistente en `data/mi_base`:

```bash
./build-local/dinodb_cli shell data/mi_base
```

La consola muestra el prompt `dinodb>`. Cada sentencia SQL debe terminar en
`;`. Para salir se usa `.exit`.

Sesion completa:

```sql
CREATE TABLE eventos (
    id INT,
    nombre TEXT,
    fecha DATE,
    inicio HOUR
);

INSERT INTO eventos VALUES (1, 'Examen', '2026-07-24', '14:30');
INSERT INTO eventos VALUES (2, 'Cierre', DATE '2026-07-31', HOUR '18:00');

SELECT id, nombre FROM eventos
WHERE fecha >= '2026-07-01' AND inicio < '18:00';

SHOW TABLES;
DESCRIBE eventos;

.exit
```

La base se crea si no existe. Si se vuelve a ejecutar el mismo comando con el
mismo directorio, las tablas y registros anteriores se recuperan del disco.

### Comandos de la consola

| Comando | Funcion |
|---------|---------|
| `.help` | Muestra la ayuda resumida. |
| `.tables` | Lista las tablas; equivale a `SHOW TABLES;`. |
| `.schema nombre` | Muestra columnas y tipos; equivale a `DESCRIBE nombre;`. |
| `.exit` o `.quit` | Cierra la consola. |

La consola acepta sentencias de varias lineas y no interpreta un `;` dentro de
un valor `TEXT` como fin de sentencia.

### Tipos de datos

| Tipo | Formato | Ejemplos |
|------|---------|----------|
| `INT` o `INTEGER` | Entero con signo de 32 bits. | `10`, `0`, `-45` |
| `TEXT` | Texto entre comillas simples. | `'Ana'`, `'Base de Datos II'` |
| `DATE` | Fecha valida `YYYY-MM-DD`. | `'2026-07-25'`, `DATE '2026-07-25'` |
| `HOUR` o `TIME` | Hora `HH:MM` o `HH:MM:SS`. | `'08:30'`, `TIME '14:05:20'` |

Las horas se muestran normalizadas como `HH:MM:SS`. Las fechas se validan,
incluyendo dias por mes y anos bisiestos. Para guardar una comilla simple en
`TEXT`, se escribe dos veces:

```sql
INSERT INTO autores VALUES (1, 'Flannery O''Connor');
```

### Crear tablas

```sql
CREATE TABLE personas (
    id INT,
    nombre TEXT,
    nacimiento DATE,
    ingreso HOUR
);
```

Los nombres de tablas y columnas no distinguen mayusculas de minusculas, se
guardan en minusculas y deben comenzar con una letra o `_`. Solo pueden
contener letras, digitos y `_`.

No se puede crear dos veces la misma tabla ni repetir nombres de columnas.

### Insertar registros

```sql
INSERT INTO personas
VALUES (1, 'Ana Torres', '2001-04-12', '08:30');

INSERT INTO personas
VALUES (2, 'Luis O''Connor', DATE '1999-11-03', TIME '14:15:30');
```

`INSERT` agrega un registro por sentencia. Los valores son posicionales y deben
tener la misma cantidad, orden y tipo que las columnas declaradas.

### Consultar registros

Consultar todas las columnas y filas:

```sql
SELECT * FROM personas;
```

Proyectar columnas concretas:

```sql
SELECT id, nombre FROM personas;
```

Filtrar registros:

```sql
SELECT nombre, nacimiento
FROM personas
WHERE nacimiento >= '2000-01-01';
```

Comparadores soportados:

| Operador | Significado |
|----------|-------------|
| `=` | Igual |
| `!=` o `<>` | Diferente |
| `<` y `<=` | Menor y menor o igual |
| `>` y `>=` | Mayor y mayor o igual |

Las condiciones se pueden combinar con `AND`, `OR` y parentesis. `AND` tiene
mayor precedencia que `OR`:

```sql
SELECT id, nombre
FROM personas
WHERE nacimiento >= '2000-01-01'
  AND (ingreso < '12:00' OR nombre = 'Ana Torres');
```

Las comparaciones deben usar valores compatibles con el tipo de la columna.
Los textos se comparan de forma lexicografica y distinguen mayusculas.

### Consultar el catalogo

```sql
SHOW TABLES;
DESCRIBE personas;
```

`SHOW TABLES` lista las tablas de la base actual. `DESCRIBE` muestra el nombre y
tipo de cada columna.

### Ejecutar una sentencia sin abrir la consola

El subcomando `sql` recibe la sentencia completa y, opcionalmente, el
directorio de la base:

```bash
./build-local/dinodb_cli sql \
  "SELECT * FROM personas WHERE nacimiento >= '2000-01-01';" \
  data/mi_base
```

Si no se indica el directorio, se utiliza `data/`.

### Plan Volcano

Cada `SELECT` se traduce a operadores Volcano reales y la CLI imprime el plan
utilizado:

```text
SeqScan(personas) -> Selection -> Projection
```

Sin una clausula `WHERE`, se omite `Selection`. La ejecucion llama a `open()`,
consume filas mediante `next()` y termina con `close()`.

### Archivos persistentes

Para el ejemplo `data/mi_base`, DinoDB crea:

```text
data/mi_base/
├── catalog.meta
├── eventos.table.db
└── personas.table.db
```

`catalog.meta` almacena esquemas y tipos. Cada archivo `*.table.db` guarda
registros tipados dentro de paginas de 4 KB con Slot Directory. El formato
actual puede leer los catalogos y registros antiguos que solo contenian
enteros.

### Errores que valida el motor

- Tabla inexistente o duplicada.
- Columna inexistente o repetida.
- Cantidad incorrecta de valores en `INSERT`.
- Incompatibilidad de tipos.
- Enteros fuera del rango de 32 bits.
- Fechas y horas invalidas.
- Cadenas sin comilla de cierre.
- Sintaxis incompleta o texto adicional despues de una sentencia.

La documentacion tecnica ampliada del formato interno tambien esta disponible
en [`docs/lenguaje_consultas_sql.md`](docs/lenguaje_consultas_sql.md), pero no
es necesaria para utilizar la CLI.

---

## DinoDB CLI - Demo B+ Tree

`dinodb_cli` es una demo por comandos enfocada en Storage Manager, Buffer Manager, B+ Tree persistente y consultas por indice mediante `IndexScan`. No intenta ser SQL.

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

La demo principal usa una tabla persistente (`PersistentTable`) y un indice B+ separado. La busqueda secuencial pasa por `SeqScan`; la busqueda puntual y el rango pasan por `IndexScan`.

---

## Roadmap de Prioridad Intermedia

Estas mejoras aumentarian la fidelidad del proyecto como Mini SGBD, pero no son estrictamente necesarias para demostrar los modulos obligatorios ya implementados.

| Mejora | Estado | Objetivo |
|--------|--------|----------|
| `SeqScan` sobre paginas reales | Implementado | `PersistentTable` serializa tuplas en slots reales y `SeqScan` puede recorrerlas desde disco. |
| Tabla persistente | Implementado | `PersistentTable` inserta tuplas en paginas, lee por `RID` y permite recorrido secuencial; el catalogo SQL conserva nombres y columnas. |
| `ExternalMergeSort` con runs temporales | Implementado | Escribe runs binarios temporales y los consume durante el merge. |
| Benchmarks reproducibles | Implementado | `bench_scan_vs_index` y `bench_buffer_hit_rate` generan salida CSV. |
| CLI SQL | Implementado | Expone consola interactiva, parser, catalogo, creacion de tablas, insercion y consultas con condiciones. |
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

- El SQL es deliberadamente pequeno: una tabla por `SELECT` y sin `UPDATE`,
  `DELETE`, agregaciones, `JOIN` u `ORDER BY`.
- No hay transacciones, WAL, rollback ni control de concurrencia.
- Hay catalogo de tablas y columnas, pero aun no administra indices. `IndexScan`
  se construye manualmente desde la API C++ y la demo B+ Tree.
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
| Volcano Model | Cumplido | Operadores `open`, `next`, `close` en scans, filtros, proyecciones, join e `IndexScan`. |
| Selection y Projection | Cumplido | `Selection`, `Projection`. |
| Join | Cumplido | `NestedLoopJoin`. |
| External Merge Sort | Cumplido parcialmente | Runs temporales en disco y merge; resultado final materializado en memoria. |
| Uso de indice en consultas | Cumplido | `IndexScan` puntual y por rango sobre tablas en memoria o persistentes; CLI y demo lo ejercitan. |
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
