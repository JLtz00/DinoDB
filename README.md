# 🦖 Mini SGBD en C++  
### Proyecto — Base de Datos II

![C++](https://img.shields.io/badge/C++-17-blue.svg)
![Estado](https://img.shields.io/badge/Estado-En%20Desarrollo-yellow)
![Build](https://img.shields.io/badge/Build-CMake-green)
![Licencia](https://img.shields.io/badge/Licencia-Académica-lightgrey)

> Construyendo un motor de base de datos desde cero para entender cómo funcionan realmente los SGBD modernos.

---

## 📌 Descripción

Este proyecto consiste en el desarrollo de un **Mini Sistema Gestor de Bases de Datos (SGBD)** implementado en **C++17**.

El objetivo es exponer de forma práctica y transparente los mecanismos internos de sistemas como PostgreSQL o MySQL:

- Persistencia en disco  
- Manejo eficiente de memoria  
- Indexación  
- Ejecución de consultas  

---

## 🎯 Objetivo

Desarrollar un motor que implemente los **4 pilares fundamentales** de un SGBD:

- 📦 Storage Manager  
- 🧠 Buffer Manager  
- 🌳 Índices (B+ Tree)  
- 🔍 Query Processor (Modelo Volcano)  

---

## 🏗️ Arquitectura
MiniSGBD
│
├── Storage Manager → Manejo de páginas en disco
├── Buffer Manager → Gestión de memoria (LRU)
├── Index (B+ Tree) → Búsqueda eficiente
└── Query Processor → Ejecución de consultas


---

## 🚀 Funcionalidades

✔ Persistencia en archivo `.db`  
✔ Páginas de 4KB con Slot Directory  
✔ Buffer Pool configurable con LRU  
✔ Índice B+ Tree  
✔ Operadores relacionales básicos:

- Selection (σ)  
- Projection (π)  
- Nested Loop Join  

---

## ❌ Fuera de Alcance

- Transacciones ACID  
- Control de concurrencia  
- Optimizador de consultas  
- SQL completo  
- Cliente/Servidor  

---

## 🛠️ Tecnologías

| Categoría | Herramienta |
|----------|------------|
| Lenguaje | C++17 |
| Compilador | GCC / Clang |
| Build | CMake |
| Testing | Google Test |
| Documentación | Doxygen |
| Control de versiones | Git + GitHub |

---

## 📂 Estructura del Proyecto

/project-root
│
├── src/ # Código fuente
├── include/ # Headers
├── tests/ # Pruebas unitarias
├── docs/ # Documentación
├── CMakeLists.txt
└── README.md


---

## ⚙️ Cómo compilar

```bash
# Clonar repositorio
git clone https://github.com/tu-usuario/tu-repo.git
cd tu-repo

# Crear carpeta de build
mkdir build
cd build

# Compilar
cmake ..
make

# Ejecutar (ejemplo)
./mini_sgbd
