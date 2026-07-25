# Lenguaje de consultas de DinoDB

## Objetivo

DinoDB incluye una capa SQL pequena para usar los componentes del proyecto como
un Mini SGBD persistente. El lenguaje crea esquemas, inserta tuplas en paginas
con Slot Directory y transforma cada consulta `SELECT` en una tuberia de
operadores Volcano.

El alcance es intencionalmente reducido: se admiten `INT`, `TEXT`, `DATE` y
`HOUR`, y cada `SELECT` consulta una tabla.

## Inicio rapido

Compilar:

```bash
cmake -S . -B build
cmake --build build
```

Abrir la consola interactiva:

```bash
./build/dinodb_cli shell data/mi_base
```

Ejemplo completo:

```sql
CREATE TABLE eventos (
    id INT,
    titulo TEXT,
    fecha DATE,
    inicio HOUR
);

INSERT INTO eventos
VALUES (1, 'Inicio de clases', '2026-03-30', '08:00');

INSERT INTO eventos
VALUES (2, 'Examen de O''Connor', DATE '2026-07-24', TIME '14:30:15');

SELECT id, titulo, inicio
FROM eventos
WHERE fecha >= '2026-07-01' AND inicio < '18:00';

SHOW TABLES;
DESCRIBE eventos;
```

Cada sentencia de la consola interactiva termina en `;`. Tambien existen los
comandos `.tables`, `.schema alumnos`, `.help` y `.exit`.

Una sentencia se puede ejecutar sin abrir la consola:

```bash
./build/dinodb_cli sql \
  "SELECT * FROM eventos WHERE fecha >= '2026-07-01';" \
  data/mi_base
```

## Sintaxis soportada

### Crear una tabla

```sql
CREATE TABLE nombre (
    columna1 INT,
    columna2 TEXT,
    columna3 DATE,
    columna4 HOUR
);
```

Los nombres no distinguen mayusculas de minusculas y se normalizan a
minusculas. Deben comenzar con una letra o `_` y solo pueden contener letras,
digitos y `_`.

Tipos:

- `INT` o `INTEGER`: entero con signo de 32 bits.
- `TEXT`: cadena de longitud variable, almacenada en UTF-8.
- `DATE`: fecha valida con formato `YYYY-MM-DD`.
- `HOUR` o `TIME`: hora valida en formato `HH:MM` o `HH:MM:SS`. La salida se
  normaliza a `HH:MM:SS`.

### Insertar un registro

```sql
INSERT INTO nombre VALUES (10, 'texto', '2026-07-24', '14:30');
```

Los valores son posicionales: la cantidad y el orden deben coincidir con el
esquema creado.

El texto se escribe entre comillas simples. Una comilla dentro del texto se
duplica:

```sql
INSERT INTO autores VALUES (1, 'Flannery O''Connor');
```

Para fechas y horas tambien se aceptan literales tipados:

```sql
INSERT INTO agenda VALUES (1, DATE '2026-07-24', TIME '09:30');
```

### Consultar

```sql
SELECT * FROM nombre;

SELECT columna1, columna3
FROM nombre
WHERE columna2 >= 'm' AND
      (columna3 >= '2026-01-01' OR columna4 < '08:30');
```

Comparadores:

- `=`
- `!=` o `<>`
- `<` y `<=`
- `>` y `>=`

Las condiciones admiten `AND`, `OR` y parentesis. `AND` tiene mayor precedencia
que `OR`.

### Inspeccionar el catalogo

```sql
SHOW TABLES;
DESCRIBE nombre;
```

## Ejecucion interna

El parser genera una representacion estructurada de la sentencia. Para un
`SELECT`, el motor resuelve los nombres de columnas contra el catalogo y crea
este plan:

```text
SeqScan(tabla) -> Selection -> Projection
```

- `SeqScan` lee incrementalmente los registros de `PersistentTable`.
- `Selection` evalua el arbol de condiciones por cada tupla.
- `Projection` entrega solo las columnas pedidas.
- La CLI consume el plan mediante `open()`, llamadas sucesivas a `next()` y
  finalmente `close()`.

Si no existe `WHERE`, se omite `Selection`. La CLI imprime el plan ejecutado
despues del resultado para hacerlo visible durante la demostracion.

## Persistencia

Dentro del directorio elegido se crean:

```text
catalog.meta
eventos.table.db
otra_tabla.table.db
```

`catalog.meta` conserva nombres y tipos de columnas. Cada archivo `*.table.db`
almacena tuplas tipadas de longitud variable en paginas fijas de 4 KB y slots
reales. Cada valor lleva una etiqueta de tipo; `TEXT` incluye su longitud,
`DATE` se almacena como `YYYYMMDD` y `HOUR` como segundos desde medianoche.
Cerrar y volver a abrir `dinodb_cli` no pierde esquemas ni registros.

El lector conserva compatibilidad con el catalogo V1 y las tuplas antiguas que
solo contenian enteros.

## Limites actuales

- `INSERT` usa todas las columnas y un registro por sentencia.
- El SQL todavia no expone `JOIN`, `ORDER BY`, indices, actualizaciones,
  eliminaciones ni agregaciones.
- No hay transacciones, concurrencia, WAL ni recuperacion ante fallos.
- Los operadores `NestedLoopJoin`, `ExternalMergeSort` e `IndexScan` siguen
  disponibles desde la API C++ y las demos existentes.
