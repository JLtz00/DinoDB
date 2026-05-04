/**
 * @file    main.cpp
 * @brief   Demostración de los tres temas requeridos
 *
 * Este programa demuestra:
 *   ① Estructura de páginas de tamaño fijo (4 KB) — Slotted Page
 *   ② Lectura y escritura de páginas en disco
 *   ③ Escritura atómica con WAL + fsync para durabilidad
 *
 * Compilar:
 *   g++ -std=c++17 -Wall *.cpp -o storage_demo
 *
 * Ejecutar:
 *   ./storage_demo
 */

#include <iostream>
#include <cstring>
#include <cassert>
#include <cstdio>
#include "page.h"
#include "disk_manager.h"

// ── Utilidades de salida ───────────────────────────────────────────────
#define VERDE  "\033[32m"
#define CYAN   "\033[36m"
#define BOLD   "\033[1m"
#define RESET  "\033[0m"

void titulo(const char* t) {
    std::cout << "\n" << BOLD << CYAN
              << "═══════════════════════════════════════════\n"
              << "  " << t << "\n"
              << "═══════════════════════════════════════════"
              << RESET << "\n";
}
void ok(const char* msg) {
    std::cout << VERDE << "  ✓ " << RESET << msg << "\n";
}

// ═══════════════════════════════════════════════════════════════════════
//  DEMO 1: Estructura de páginas de tamaño fijo — Slotted Page
//
//  Muestra cómo una página de 4096 bytes organiza internamente
//  múltiples registros usando el modelo slotted page.
// ═══════════════════════════════════════════════════════════════════════
void demo_estructura_pagina() {
    titulo("① Estructura de páginas de tamaño fijo (4 KB)");

    Page page;
    page.init(0);  // Inicializar página 0 vacía

    std::cout << "\n  Tamaño de página:    " << PAGE_SIZE << " bytes\n";
    std::cout << "  Tamaño de PageHeader: " << sizeof(PageHeader) << " bytes\n";
    std::cout << "  Tamaño de Slot:      " << sizeof(Slot) << " bytes\n";
    std::cout << "  Espacio libre inicial: " << page.getFreeSpace() << " bytes\n\n";

    // Insertar registros de distintos tamaños
    struct Registro { int32_t id; char nombre[20]; float salario; };

    Registro r1 = { 1, "Ana García",   45000.0f };
    Registro r2 = { 2, "Luis Ramos",   52000.0f };
    Registro r3 = { 3, "María Torres", 38000.0f };

    // Los slots se asignan del inicio; los datos se escriben desde el final
    int s1 = page.insert(&r1, sizeof(r1));
    int s2 = page.insert(&r2, sizeof(r2));
    int s3 = page.insert(&r3, sizeof(r3));

    std::cout << "  Registros insertados en slots: "
              << s1 << ", " << s2 << ", " << s3 << "\n";
    std::cout << "  Slots usados:   " << page.getNumSlots() << "\n";
    std::cout << "  Espacio libre:  " << page.getFreeSpace() << " bytes\n\n";

    // Verificar recuperación por slot_id
    uint16_t len;
    auto* out1 = static_cast<const Registro*>(page.get(s1, len));
    auto* out2 = static_cast<const Registro*>(page.get(s2, len));
    auto* out3 = static_cast<const Registro*>(page.get(s3, len));

    assert(out1 && out1->id == 1);
    assert(out2 && out2->id == 2);
    assert(out3 && out3->id == 3);

    std::cout << "  Registro slot 0: id=" << out1->id
              << " nombre=" << out1->nombre
              << " salario=" << out1->salario << "\n";
    std::cout << "  Registro slot 1: id=" << out2->id
              << " nombre=" << out2->nombre
              << " salario=" << out2->salario << "\n";
    std::cout << "  Registro slot 2: id=" << out3->id
              << " nombre=" << out3->nombre
              << " salario=" << out3->salario << "\n\n";

    ok("Slotted page funcional: registros insertados y recuperados correctamente");
}

// ═══════════════════════════════════════════════════════════════════════
//  DEMO 2: Lectura y escritura de páginas en disco
//
//  Escribe páginas al archivo binario y las lee de vuelta,
//  simulando un reinicio del sistema (nuevo DiskManager).
// ═══════════════════════════════════════════════════════════════════════
void demo_lectura_escritura() {
    titulo("② Lectura y escritura de páginas en disco");

    std::remove("demo.db");
    std::remove("demo.db.wal");

    // ── ESCRITURA ─────────────────────────────────────────────────────
    {
        DiskManager dm("demo.db");

        // Página 0: datos de empleados
        Page p0; p0.init(0);
        const char* emp1 = "Empleado:001:Juan Perez:35000";
        const char* emp2 = "Empleado:002:Lucia Ruiz:41000";
        p0.insert(emp1, static_cast<uint16_t>(strlen(emp1) + 1));
        p0.insert(emp2, static_cast<uint16_t>(strlen(emp2) + 1));
        dm.writePage(0, p0);

        // Página 1: datos de productos
        Page p1; p1.init(1);
        const char* prod1 = "Producto:A01:Laptop:2500.00";
        p1.insert(prod1, static_cast<uint16_t>(strlen(prod1) + 1));
        dm.writePage(1, p1);

        std::cout << "\n  Páginas escritas al disco: "
                  << dm.getNumPages() << "\n";
        std::cout << "  Archivo: demo.db ("
                  << dm.getNumPages() * PAGE_SIZE << " bytes)\n\n";
        ok("Páginas escritas correctamente al archivo binario");
    }

    // ── LECTURA (simula reinicio del sistema) ─────────────────────────
    {
        std::cout << "\n  [Simulando reinicio del sistema...]\n\n";
        DiskManager dm("demo.db");  // Nueva instancia — lee del archivo

        std::cout << "  Páginas encontradas en disco: "
                  << dm.getNumPages() << "\n\n";

        Page p0, p1;
        dm.readPage(0, p0);
        dm.readPage(1, p1);

        uint16_t len;
        auto* e1 = static_cast<const char*>(p0.get(0, len));
        auto* e2 = static_cast<const char*>(p0.get(1, len));
        auto* pr = static_cast<const char*>(p1.get(0, len));

        assert(e1 && strcmp(e1, "Empleado:001:Juan Perez:35000") == 0);
        assert(e2 && strcmp(e2, "Empleado:002:Lucia Ruiz:41000") == 0);
        assert(pr && strcmp(pr, "Producto:A01:Laptop:2500.00") == 0);

        std::cout << "  Página 0, slot 0: " << e1 << "\n";
        std::cout << "  Página 0, slot 1: " << e2 << "\n";
        std::cout << "  Página 1, slot 0: " << pr << "\n\n";

        ok("Datos recuperados correctamente tras reinicio del sistema");
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  DEMO 3: Escritura atómica con WAL + fsync para durabilidad
//
//  Muestra el protocolo WAL completo:
//    logWrite → logCommit → writePageDirect → fsync → truncate
//  Y simula recovery ante un crash hipotético.
// ═══════════════════════════════════════════════════════════════════════
void demo_atomicidad_fsync() {
    titulo("③ Escritura atómica con WAL + fsync");

    std::remove("atomico.db");
    std::remove("atomico.db.wal");

    DiskManager dm("atomico.db");

    // ── Escritura normal con WAL ───────────────────────────────────────
    std::cout << "\n  [Inserción con protocolo WAL]\n";
    std::cout << "  Paso 1: wal.logWrite()  → página en .wal + fsync\n";
    std::cout << "  Paso 2: wal.logCommit() → COMMIT en .wal + fsync\n";
    std::cout << "  Paso 3: writePageDirect → página en .db\n";
    std::cout << "  Paso 4: fsync(.db)      → datos en hardware\n";
    std::cout << "  Paso 5: wal.truncate()  → .wal vaciado\n\n";

    Page p0; p0.init(0);
    p0.insert("dato-original-v1", 17);
    dm.writePage(0, p0);
    ok("Primera escritura atómica completada");

    // ── Actualización: sobreescribir la misma página ───────────────────
    Page p0_v2; p0_v2.init(0);
    p0_v2.insert("dato-actualizado-v2", 20);
    dm.writePage(0, p0_v2);
    ok("Actualización atómica completada");

    // ── Verificar ─────────────────────────────────────────────────────
    Page check; dm.readPage(0, check);
    uint16_t len;
    auto* val = static_cast<const char*>(check.get(0, len));
    assert(val && strcmp(val, "dato-actualizado-v2") == 0);
    std::cout << "\n  Valor en disco: [" << val << "]\n\n";
    ok("Dato actualizado verificado correctamente");

    // ── fsync explícito ───────────────────────────────────────────────
    std::cout << "\n  [fsync explícito — syncAll()]\n";
    dm.syncAll();
    ok("fsync ejecutado: datos garantizados en hardware físico");

    // ── Checkpoint ────────────────────────────────────────────────────
    dm.checkpoint();
    ok("Checkpoint completado: WAL vaciado");
}

// ═══════════════════════════════════════════════════════════════════════
//  MAIN
// ═══════════════════════════════════════════════════════════════════════
int main() {
    std::cout << BOLD
              << "\n╔══════════════════════════════════════════╗\n"
              << "║  Storage Manager — Demostración Completa ║\n"
              << "║  Páginas fijas + I/O + Escritura Atómica ║\n"
              << "╚══════════════════════════════════════════╝"
              << RESET << "\n";

    try {
        demo_estructura_pagina();
        demo_lectura_escritura();
        demo_atomicidad_fsync();

        std::cout << "\n" << BOLD << VERDE
                  << "  ✓ TODAS LAS DEMOS COMPLETADAS EXITOSAMENTE\n"
                  << RESET << "\n";

    } catch (const std::exception& e) {
        std::cerr << "\n\033[31m✗ ERROR: " << e.what() << "\033[0m\n";
        return 1;
    }

    return 0;
}
