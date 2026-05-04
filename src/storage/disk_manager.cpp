/**
 * @file    disk_manager.cpp
 * @brief   Implementación: lectura/escritura de páginas + escritura atómica
 *
 * TEMA: Storage Manager I + Persistencia básica
 * ──────────────────────────────────────────────────────────────────────
 *
 * Este archivo implementa los tres pilares solicitados:
 *
 *  ① Estructura de páginas de tamaño fijo (4 KB)
 *     → readPage() y writePage() operan siempre en bloques de PAGE_SIZE.
 *
 *  ② Lectura y escritura de páginas en disco
 *     → readPage()      : seek al offset correcto + read binario.
 *     → writePageDirect(): seek al offset correcto + write binario.
 *
 *  ③ Escritura atómica + fsync para durabilidad
 *     → writePage()     : protocolo WAL completo con fsync en cada paso.
 *     → recover()       : replay del WAL si hubo crash anterior.
 */

#include "disk_manager.h"
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

// ═══════════════════════════════════════════════════════════════════════
//  Constructor — Inicializar el gestor de disco
// ═══════════════════════════════════════════════════════════════════════
DiskManager::DiskManager(const std::string& filename)
    : filename_(filename),
      wal_filename_(filename + ".wal"),
      num_pages_(0),
      wal_(wal_filename_)
{
    openFile();

    // Calcular cuántas páginas ya existen en el archivo:
    // tamaño del archivo / PAGE_SIZE = número de páginas
    file_.seekg(0, std::ios::end);
    auto size = file_.tellg();
    num_pages_ = (size <= 0) ? 0
               : static_cast<uint32_t>(size / PAGE_SIZE);

    // Recovery: verificar si hay un .wal pendiente de un crash anterior
    recover();
}

DiskManager::~DiskManager() {
    if (file_.is_open()) {
        syncAll();   // Asegurar que todo llegó al disco antes de cerrar
        file_.close();
    }
}

// ── Abrir o crear el archivo binario .db ──────────────────────────────
void DiskManager::openFile() {
    // Intentar abrir en modo lectura/escritura binario
    file_.open(filename_, std::ios::in | std::ios::out | std::ios::binary);

    if (!file_.is_open()) {
        // El archivo no existía — crearlo primero
        file_.open(filename_, std::ios::out | std::ios::binary);
        file_.close();
        file_.open(filename_, std::ios::in | std::ios::out | std::ios::binary);
    }

    if (!file_.is_open()) {
        throw std::runtime_error("No se pudo abrir: " + filename_);
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  recover() — Replay del WAL al iniciar el sistema
//
//  Al arrancar, si el .wal tiene entradas, significa que hubo un crash
//  durante una escritura anterior. Se analiza el .wal y se decide:
//
//    Sin COMMIT → transacción incompleta → descartar (equivale a rollback)
//    Con COMMIT → transacción confirmada → aplicar al .db (equivale a redo)
// ═══════════════════════════════════════════════════════════════════════
void DiskManager::recover() {
    auto entries = wal_.readPendingEntries();
    if (entries.empty()) return;  // No hubo crash — continuar normal

    std::cout << "[WAL Recovery] " << entries.size()
              << " entradas encontradas\n";

    // Verificar si existe un COMMIT en el log
    bool has_commit = false;
    for (auto& e : entries)
        if (e.type == WALEntryType::COMMIT) { has_commit = true; break; }

    if (!has_commit) {
        // Sin COMMIT: la transacción fue interrumpida antes de confirmarse
        // → descartar todas las entradas (rollback implícito)
        std::cout << "[WAL Recovery] Sin COMMIT — descartando (rollback)\n";
        wal_.truncate();
        return;
    }

    // Con COMMIT: aplicar todas las páginas escritas al archivo .db
    for (auto& e : entries) {
        if (e.type == WALEntryType::WRITE) {
            std::cout << "[WAL Recovery] Aplicando página " << e.page_id << "\n";
            writePageDirect(e.page_id, e.page);
        }
    }

    syncAll();        // fsync del .db con los cambios aplicados
    wal_.truncate();  // El .wal ya no se necesita
    std::cout << "[WAL Recovery] Completado ✓\n";
}

// ═══════════════════════════════════════════════════════════════════════
//  ② ESCRITURA DIRECTA DE PÁGINA EN DISCO (sin WAL)
//
//  Calcula el offset de la página y escribe PAGE_SIZE bytes.
//  Offset = page_id × PAGE_SIZE
//
//  Solo para uso interno: recovery y checkpoint.
//  Las escrituras normales usan writePage() con protocolo WAL.
// ═══════════════════════════════════════════════════════════════════════
void DiskManager::writePageDirect(uint32_t page_id, const Page& page) {
    file_.clear();  // Limpiar flags de error del fstream antes de seek

    // Calcular offset exacto: cada página ocupa exactamente PAGE_SIZE bytes
    std::streamoff offset = static_cast<std::streamoff>(page_id) * PAGE_SIZE;
    file_.seekp(offset, std::ios::beg);

    // Escribir el bloque completo de PAGE_SIZE bytes
    file_.write(reinterpret_cast<const char*>(page.data), PAGE_SIZE);

    if (file_.fail()) {
        file_.clear();
        throw std::runtime_error("Error al escribir página " +
                                  std::to_string(page_id));
    }

    if (page_id >= num_pages_) {
        num_pages_ = page_id + 1;
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  ③ ESCRITURA ATÓMICA CON WAL + fsync
//
//  Protocolo de escritura segura:
//
//  ┌─────────────────────────────────────────────────────────────────┐
//  │ PASO 1: wal.logWrite(page)   → escribe página en .wal + fsync  │
//  │ PASO 2: wal.logCommit()      → escribe COMMIT en .wal + fsync  │
//  │                               ← punto de no retorno            │
//  │ PASO 3: writePageDirect()    → escribe página en .db           │
//  │ PASO 4: fsync(.db)           → datos en hardware físico        │
//  │ PASO 5: wal.truncate()       → .wal ya no es necesario         │
//  └─────────────────────────────────────────────────────────────────┘
//
//  Escenarios de crash:
//    Crash antes PASO 2 → .wal sin COMMIT → recovery hace rollback
//    Crash entre PASO 2 y PASO 5 → .wal con COMMIT → recovery hace redo
// ═══════════════════════════════════════════════════════════════════════
void DiskManager::writePage(uint32_t page_id, const Page& page) {
    // PASO 1: Registrar la página en el WAL (incluye fsync interno)
    wal_.logWrite(page_id, page);

    // PASO 2: Confirmar la transacción en el WAL (incluye fsync interno)
    // A partir de aquí, aunque falle el sistema, el recovery puede
    // reconstruir el estado correcto.
    wal_.logCommit();

    // PASO 3: Escribir la página en el archivo principal .db
    writePageDirect(page_id, page);

    // PASO 4: fsync del archivo .db
    // Fuerza que los datos bajen del caché del OS al hardware físico.
    // Garantiza que los datos sobrevivan un corte de energía.
    file_.flush();
    int fd = open(filename_.c_str(), O_WRONLY);
    if (fd != -1) {
        fsync(fd);   // ← durabilidad garantizada
        close(fd);
    }

    // PASO 5: El .wal ya no es necesario — truncar
    wal_.truncate();
}

// ═══════════════════════════════════════════════════════════════════════
//  ② LECTURA DE PÁGINA DEL DISCO
//
//  Calcula el offset: page_id × PAGE_SIZE
//  Lee exactamente PAGE_SIZE bytes en page.data[]
// ═══════════════════════════════════════════════════════════════════════
void DiskManager::readPage(uint32_t page_id, Page& page) {
    if (page_id >= num_pages_) {
        throw std::out_of_range(
            "page_id " + std::to_string(page_id) +
            " fuera de rango (total: " + std::to_string(num_pages_) + ")"
        );
    }

    file_.clear();  // Limpiar flags de error/eof antes del seek

    // Calcular offset exacto de la página en el archivo
    std::streamoff offset = static_cast<std::streamoff>(page_id) * PAGE_SIZE;
    file_.seekg(offset, std::ios::beg);

    // Leer el bloque completo de PAGE_SIZE bytes
    file_.read(reinterpret_cast<char*>(page.data), PAGE_SIZE);

    if (file_.fail()) {
        file_.clear();
        throw std::runtime_error("Error al leer página " +
                                  std::to_string(page_id));
    }
}

// ── syncAll() — fsync explícito del archivo completo ─────────────────
void DiskManager::syncAll() {
    file_.flush();  // Primero vaciar buffers de C++ al kernel

    // Luego fsync para bajar del kernel al hardware
    int fd = open(filename_.c_str(), O_WRONLY);
    if (fd == -1) return;

    if (fsync(fd) == -1) {
        close(fd);
        throw std::runtime_error("fsync falló en " + filename_);
    }

    close(fd);
}

// ── checkpoint() — Sincronizar y vaciar el WAL ───────────────────────
void DiskManager::checkpoint() {
    std::cout << "[Checkpoint] Iniciando...\n";
    syncAll();
    wal_.truncate();
    std::cout << "[Checkpoint] Completado ✓\n";
}
