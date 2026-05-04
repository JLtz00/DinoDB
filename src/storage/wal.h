/**
 * @file    wal.h
 * @brief   Write-Ahead Log (WAL) para escritura atómica y durabilidad
 *
 * TEMA: Persistencia básica — Escritura atómica + fsync
 * ──────────────────────────────────────────────────────────────────────
 *
 * PROBLEMA: Sin mecanismos de protección, un crash del sistema a mitad
 * de una escritura puede dejar una página en estado corrupto (mitad
 * datos nuevos, mitad datos viejos).
 *
 * SOLUCIÓN — Write-Ahead Log (WAL):
 *   Antes de modificar el archivo principal (.db), se registra la
 *   intención de escritura en un archivo de log (.wal). Si el sistema
 *   falla, al reiniciar se puede reconstruir el estado correcto.
 *
 * PROTOCOLO:
 *   ┌─────────────────────────────────────────────────────────┐
 *   │  1. Escribir entrada WRITE en .wal + fsync              │
 *   │  2. Escribir entrada COMMIT en .wal + fsync             │
 *   │  3. Aplicar cambio en .db + fsync                       │
 *   │  4. Truncar el .wal (ya no se necesita)                 │
 *   └─────────────────────────────────────────────────────────┘
 *
 *   Crash entre paso 1 y 2 → sin COMMIT → recovery descarta
 *   Crash entre paso 2 y 4 → con COMMIT → recovery aplica el cambio
 *
 * FORMATO DE ENTRADA EN DISCO:
 *   [WALEntryHeader: 13 bytes][datos de la página: PAGE_SIZE bytes]
 *
 * Referencia: "From Files To Databases", Cap. 2 — Durability & WAL
 */

#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "page.h"

// ── Tipos de entrada en el WAL ─────────────────────────────────────────
enum class WALEntryType : uint8_t {
    WRITE      = 1,  // Intención de escribir una página
    COMMIT     = 2,  // Confirmación de la transacción
    CHECKPOINT = 3   // Marca de sincronización con el .db
};

// ── Cabecera de cada entrada del WAL ──────────────────────────────────
// Esta estructura se escribe literalmente en el archivo .wal.
struct WALEntryHeader {
    WALEntryType type;       // Tipo de entrada
    uint32_t     page_id;    // Página afectada
    uint32_t     data_size;  // Tamaño de los datos (PAGE_SIZE o 0 para COMMIT)
    uint32_t     checksum;   // CRC32 de los datos (detecta corrupción)
};

// ═══════════════════════════════════════════════════════════════════════
//  WAL — Write-Ahead Log
//
//  Garantiza que las escrituras sean atómicas y durables mediante:
//    - fsync() después de cada entrada para forzar bajada al hardware
//    - CRC32 por entrada para detectar entradas corruptas
//    - Protocolo WRITE → COMMIT antes de modificar el .db
// ═══════════════════════════════════════════════════════════════════════
class WAL {
public:
    explicit WAL(const std::string& wal_filename);
    ~WAL();

    /**
     * Registra la intención de escribir una página.
     * Incluye fsync() — los datos llegan al hardware antes de continuar.
     */
    void logWrite(uint32_t page_id, const Page& page);

    /**
     * Confirma la transacción actual.
     * Sin este COMMIT, el recovery descartará las entradas previas.
     */
    void logCommit();

    /**
     * Lee todas las entradas pendientes del .wal.
     * Usado al arrancar para hacer recovery si hubo un crash anterior.
     */
    struct WALEntry {
        WALEntryType type;
        uint32_t     page_id;
        Page         page;
    };
    std::vector<WALEntry> readPendingEntries();

    /**
     * Vacía el .wal (trunca a 0 bytes).
     * Se llama después de confirmar que los cambios están en el .db.
     */
    void truncate();

    bool isOpen() const { return fd_ != -1; }

private:
    std::string wal_filename_;
    int         fd_;  // File descriptor POSIX (para usar fsync directamente)

    // CRC32 para verificar integridad de los datos en el log
    uint32_t computeChecksum(const uint8_t* data, uint32_t size) const;

    void ensureOpen();
};
