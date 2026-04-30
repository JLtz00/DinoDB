🦖 Mini SGBD en C++ — Proyecto Base de Datos II

Construcción de un motor de base de datos desde cero para entender cómo funcionan realmente los sistemas como PostgreSQL o MySQL.

📌 Descripción

Este proyecto tiene como objetivo desarrollar un Mini Sistema Gestor de Bases de Datos (SGBD) en C++17, implementando desde cero los componentes fundamentales que normalmente están ocultos en sistemas comerciales.

La idea es responder a una pregunta clave:

¿Cómo un sistema puede almacenar, recuperar y consultar datos de manera eficiente minimizando el costo de acceso a disco?

🎯 Objetivos
🧠 Objetivo General

Desarrollar un motor de base de datos didáctico que implemente:

Gestión de almacenamiento
Gestión de buffer
Indexación con B+ Tree
Procesamiento de consultas (Modelo Volcano)
⚙️ Objetivos Específicos
📦 Implementar un Storage Manager con páginas de 4KB
🧠 Diseñar un Buffer Manager con política LRU
🌳 Construir un índice B+ Tree
🔍 Crear un Query Processor con operadores relacionales
📚 Documentar el código con Doxygen
🏗️ Arquitectura del Sistema

El sistema se divide en los siguientes módulos:

MiniSGBD
│
├── Storage Manager   → Manejo de páginas en disco
├── Buffer Manager    → Gestión de memoria (LRU)
├── Index (B+ Tree)   → Búsquedas eficientes
└── Query Processor   → Ejecución de consultas
🚀 Funcionalidades (En desarrollo)

✔ Persistencia en disco mediante archivos .db
✔ Manejo de páginas con Slot Directory
✔ Buffer Pool configurable
✔ Árbol B+ con búsquedas por rango
✔ Operadores relacionales básicos:

Selection (σ)
Projection (π)
Nested Loop Join
❌ Fuera de Alcance (por ahora)
Transacciones ACID
Control de concurrencia
Optimización de consultas
Soporte SQL completo
Cliente/Servidor
🛠️ Tecnologías
Categoría	Tecnología
Lenguaje	C++17
Compilador	GCC / Clang
Build System	CMake
Testing	Google Test
Documentación	Doxygen
Control de versiones	Git + GitHub
📂 Estructura del Proyecto (Sugerida)
/project-root
│
├── src/                # Código fuente
├── include/            # Headers
├── tests/              # Pruebas unitarias
├── docs/               # Documentación
├── CMakeLists.txt
└── README.md
👥 Integrantes
Carlos Enrique Gutierrez Castilla
Fernando Antonio Gama Llicahua
Job Lorenzo Quispe Torrez
Diego Mauricio Villanueva Flores
🏫 Información Académica
Universidad: Universidad Nacional de San Agustín de Arequipa
Curso: Base de Datos II
Docente: Maria Vilma Escobar Castillo
Año: 2026
📈 Estado del Proyecto

🚧 En desarrollo — Fase inicial

🤝 Contribución

Este es un proyecto académico, pero puedes:

Reportar errores
Proponer mejoras
Revisar el código
📜 Licencia

Este proyecto es de uso académico.

⭐ Nota

Este proyecto está diseñado con fines educativos para comprender a bajo nivel cómo funcionan los sistemas gestores de bases de datos.
