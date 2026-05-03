# 🦖 Mini SGBD en C++
### Proyecto — Base de Datos II

![C++](https://img.shields.io/badge/C++-17-blue.svg)
![CMake](https://img.shields.io/badge/CMake-3.20+-green.svg)
![Estado](https://img.shields.io/badge/Estado-Planeación-orange)
![Licencia](https://img.shields.io/badge/Licencia-Académica-lightgrey)

> Proyecto académico enfocado en la construcción de un motor de base de datos desde cero en C++17, implementando sus componentes internos: Storage Manager, Buffer Manager, B+ Tree e índices, y un procesador de consultas con el modelo Volcano.

---

## 📌 Descripción

Este proyecto tiene como objetivo desarrollar un **Mini Sistema Gestor de Bases de Datos (SGBD)** en **C++17**, con fines educativos.

La intención es comprender cómo funcionan internamente los sistemas gestores de bases de datos, implementando sus componentes principales desde un nivel bajo, en lugar de utilizar soluciones ya existentes.

Se busca responder a la siguiente pregunta:

> *¿Cómo puede un sistema almacenar, organizar y recuperar datos de manera eficiente?*

---

## 🏗️ Arquitectura del Sistema

El sistema está organizado en **4 capas**, construidas de abajo hacia arriba:

```
┌─────────────────────────────────────┐
│     Query Processor  (Volcano)      │  ← Capa 4
├─────────────────────────────────────┤
│     Índice  (B+ Tree)               │  ← Capa 3
├─────────────────────────────────────┤
│     Buffer Manager  (LRU Pool)      │  ← Capa 2
├─────────────────────────────────────┤
│     Storage Manager  (Disco/I-O)    │  ← Capa 1
└─────────────────────────────────────┘
```

### Módulos

| Módulo | Descripción | Estado |
|--------|-------------|--------|
| **Storage Manager** | Persistencia en disco con archivos binarios, páginas fijas de 4 KB y Slot Directory | 🔲 Pendiente |
| **Buffer Manager** | Buffer Pool configurable en RAM, política de reemplazo LRU, estados dirty/pinned | 🔲 Pendiente |
| **B+ Tree Index** | Árbol B+ integrado con el Buffer Manager, búsqueda puntual y por rango | 🔲 Pendiente |
| **Query Processor** | Modelo Volcano: operadores Selection, Projection y Nested Loop Join | 🔲 Pendiente |

---

## 📁 Estructura del Repositorio

```
mini-sgbd/
├── src/
│   ├── storage/          # DiskManager, Page, SlotDirectory
│   ├── buffer/           # BufferPool, Frame, LRUReplacer
│   ├── index/            # BPlusTree, BPlusNode
│   ├── query/            # Operator, SeqScan, Selection, Projection, NLJoin
│   └── common/           # Types, RID, Config
├── include/              # Headers (.h / .hpp)
├── tests/                # Google Test — un archivo por módulo
├── data/                 # Archivos .db generados en ejecución (ignorados por git)
├── docs/                 # Documentación generada por Doxygen
├── CMakeLists.txt
└── README.md
```

---

## 🗺️ Fases de Implementación

El desarrollo sigue un orden estricto: **cada capa depende de la anterior**.

| Fase | Módulo | Contenido principal |
|------|--------|---------------------|
| **Fase 1** | Storage Manager | `Page` (4 KB), `SlotDirectory`, `DiskManager` (read/write binario) |
| **Fase 2** | Buffer Manager | `BufferPool`, `Frame`, `LRUReplacer` (lista enlazada + hashmap) |
| **Fase 3** | B+ Tree | Nodos como páginas del Buffer Manager, insert, search, range scan |
| **Fase 4** | Query Processor | Interfaz `Operator`, `SeqScan`, `Selection`, `Projection`, `NLJoin` |

---

## ⚙️ Compilación y Ejecución

### Requisitos previos

- Compilador con soporte C++17: GCC 12+ o Clang 15+
- CMake 3.20 o superior
- Git

### Pasos

```bash
# 1. Clonar el repositorio
git clone https://github.com/equipo/mini-sgbd.git
cd mini-sgbd

# 2. Crear directorio de compilación
mkdir build && cd build

# 3. Configurar con CMake
cmake ..

# 4. Compilar
cmake --build .

# 5. Ejecutar pruebas
ctest --output-on-failure
```

---

## 🌿 Convención de Ramas Git

```
main              ← rama principal estable, solo recibe Pull Requests aprobados
├── feature/storage-manager
├── feature/buffer-pool-lru
├── feature/bplus-tree
├── feature/query-processor
└── fix/nombre-del-bug
```

| Prefijo | Uso |
|---------|-----|
| `feature/` | Implementación de un módulo o funcionalidad nueva |
| `fix/` | Corrección de un bug |
| `docs/` | Cambios solo de documentación |
| `test/` | Adición o corrección de pruebas |

**Reglas de commits:**
- Mensajes en formato: `tipo(módulo): descripción breve`
- Ejemplos: `feat(storage): implementar SlotDirectory`, `fix(buffer): corregir puntero LRU`
- Commits pequeños y frecuentes para evidenciar trabajo grupal

---

## 👥 Integrantes

| Nombre | Módulo asignado |
|--------|----------------|
| Carlos Enrique Gutierrez Castilla | Storage Manager |
| Fernando Antonio Gama Llicahua | Buffer Manager |
| Job Lorenzo Quispe Torrez | B+ Tree Index |
| Diego Mauricio Villanueva Flores | Query Processor |

---

## 🏫 Información Académica

- **Universidad:** Universidad Nacional de San Agustín de Arequipa
- **Curso:** Base de Datos II
- **Docente:** Maria Vilma Escobar Castillo
- **Año:** 2026

---

## 📚 Referencias Bibliográficas

1. Ramakrishnan, R. & Gehrke, J. *Database Management Systems*, 3rd ed. McGraw-Hill, 2003.
2. Graefe, G. *Volcano — An Extensible and Parallel Query Evaluation System*. IEEE TKDE, 1994.
3. Comer, D. *The Ubiquitous B-Tree*. ACM Computing Surveys, 11(2), 1979.
4. Silberschatz, A. et al. *Database System Concepts*, 7th ed. McGraw-Hill, 2019.

---

## 🔄 Historial de Actualizaciones

Este README será actualizado conforme se implementen los distintos módulos del sistema.