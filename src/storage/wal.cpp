/**
 * @file    wal.cpp
 * @brief   Implementación del Write-Ahead Log con fsync para durabilidad
 *
 * TEMA: Persistencia básica — Escritura atómica + manejo de fsync
 * ──────────────────────────────────────────────────────────────────────
 *
 * POR QUÉ fsync() Y NO SOLO fflush():
 *
 *   fflush() vacía el buffer de la biblioteca C al kernel del OS.
 *   El kernel puede aún mantener los datos en su propio page cache
 *   y escribirlos al disco en cualquier momento posterior.
 *
 *   fsync() fuerza al kernel a escribir TODOS los datos pendientes
 *   al dispositivo de almacenamiento físico antes de retornar.
 *   Esto garantiza que los datos sobrevivan un corte de energía.
 *
 *   fflush()  →  buffer C  →  kernel cache  →  (posible pérdida)
 *   fsync()   →  buffer C  →  kernel cache  →  disco físico ✓
 *
 * Usamos la API POSIX (open/write/fsync/close) en lugar de fstream
 * porque fstream no expone el file descriptor necesario para fsync().
 */

#include "wal.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>
#include <cstring>
#include <iostream>

// ── Constructor: abrir o crear el archivo .wal ─────────────────────────
WAL::WAL(const std::string& wal_filename)
    : wal_filename_(wal_filename), fd_(-1)
{
    ensureOpen();
}

WAL::~WAL() {
    if (fd_ != -1) close(fd_);
}

// Abre el .wal en modo append — nuevas entradas se añaden al final.
void WAL::ensureOpen() {
    if (fd_ != -1) return;

    fd_ = open(wal_filename_.c_str(),
               O_WRONLY | O_CREAT | O_APPEND,  // crear si no existe, append
               S_IRUSR | S_IWUSR);             // permisos rw-------

    if (fd_ == -1) {
        throw std::runtime_error("No se pudo abrir WAL: " + wal_filename_);
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  computeChecksum() — CRC32 para detección de corrupción
//
//  Genera un hash de 32 bits de los datos de la página.
//  Al leer el WAL, se recalcula el checksum y se compara con el
//  almacenado. Si difieren, la entrada está corrupta y se descarta.
// ═══════════════════════════════════════════════════════════════════════
uint32_t WAL::computeChecksum(const uint8_t* data, uint32_t size) const {
    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < size; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            // Polinomio estándar CRC32 (IEEE 802.3)
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}

// ═══════════════════════════════════════════════════════════════════════
//  logWrite() — Registrar escritura de página en el WAL
//
//  Escribe en disco:
//    [WALEntryHeader (13 bytes)] [datos de la página (4096 bytes)]
//
//  Seguido de fsync() para garantizar durabilidad ANTES de que
//  el llamador modifique el archivo .db principal.
// ═══════════════════════════════════════════════════════════════════════
void WAL::logWrite(uint32_t page_id, const Page& page) {
    // Preparar el header de la entrada
    WALEntryHeader hdr;
    hdr.type      = WALEntryType::WRITE;
    hdr.page_id   = page_id;
    hdr.data_size = PAGE_SIZE;
    hdr.checksum  = computeChecksum(page.data, PAGE_SIZE);

    // Escribir el header al .wal
    ssize_t w = write(fd_, &hdr, sizeof(hdr));
    if (w != static_cast<ssize_t>(sizeof(hdr))) {
        throw std::runtime_error("WAL: error escribiendo header");
    }

    // Escribir los datos completos de la página al .wal
    w = write(fd_, page.data, PAGE_SIZE);
    if (w != static_cast<ssize_t>(PAGE_SIZE)) {
        throw std::runtime_error("WAL: error escribiendo página");
    }

    // ── fsync crítico ──────────────────────────────────────────────────
    // Forzar que esta entrada llegue al hardware AHORA.
    // Si el sistema falla después de este punto, el .wal tiene la entrada
    // y el recovery puede reconstruir el estado correcto.
    if (fsync(fd_) == -1) {
        throw std::runtime_error("WAL: fsync falló");
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  logCommit() — Marcar fin de transacción
//
//  Sin este COMMIT en el .wal, el recovery al reiniciar descartará
//  todas las entradas previas (transacción incompleta = rollback).
// ═══════════════════════════════════════════════════════════════════════
void WAL::logCommit() {
    WALEntryHeader hdr;
    hdr.type      = WALEntryType::COMMIT;
    hdr.page_id   = 0;
    hdr.data_size = 0;
    hdr.checksum  = 0;

    ssize_t wc = write(fd_, &hdr, sizeof(hdr));
    (void)wc;  // El commit es best-effort — si falla, recovery descarta

    // fsync del COMMIT — garantiza que quede en disco antes de tocar .db
    fsync(fd_);
}

// ═══════════════════════════════════════════════════════════════════════
//  readPendingEntries() — Leer el WAL para recovery
//
//  Al arrancar el sistema, se lee el .wal para detectar si hubo un
//  crash anterior con cambios no aplicados al .db.
//
//  Reglas de recovery:
//    - Si hay entradas WRITE sin COMMIT → descartar todo (rollback)
//    - Si hay entradas WRITE con COMMIT → aplicar al .db (redo)
//    - Si el checksum falla → entrada corrupta, descartar
// ═══════════════════════════════════════════════════════════════════════
std::vector<WAL::WALEntry> WAL::readPendingEntries() {
    std::vector<WALEntry> entries;

    // Abrir en modo solo lectura para no modificar el WAL durante recovery
    int fd_read = open(wal_filename_.c_str(), O_RDONLY);
    if (fd_read == -1) return entries;  // .wal vacío o no existe — OK

    while (true) {
        WALEntryHeader hdr;
        ssize_t r = read(fd_read, &hdr, sizeof(hdr));
        if (r == 0) break;            // EOF — fin del log
        if (r != sizeof(hdr)) break;  // Entrada incompleta (crash mid-write)

        if (hdr.type == WALEntryType::COMMIT ||
            hdr.type == WALEntryType::CHECKPOINT) {
            entries.push_back({ hdr.type, 0, Page{} });
            continue;
        }

        if (hdr.type == WALEntryType::WRITE && hdr.data_size == PAGE_SIZE) {
            WALEntry entry;
            entry.type    = hdr.type;
            entry.page_id = hdr.page_id;

            r = read(fd_read, entry.page.data, PAGE_SIZE);
            if (r != static_cast<ssize_t>(PAGE_SIZE)) break;  // Dato corrupto

            // Verificar integridad con CRC32
            uint32_t computed = computeChecksum(entry.page.data, PAGE_SIZE);
            if (computed != hdr.checksum) {
                std::cerr << "WAL: checksum inválido en página "
                          << hdr.page_id << " — descartada\n";
                continue;
            }

            entries.push_back(std::move(entry));
        }
    }

    close(fd_read);
    return entries;
}

// ═══════════════════════════════════════════════════════════════════════
//  truncate() — Vaciar el WAL tras aplicar los cambios al .db
//
//  Una vez que los cambios están seguros en el .db (con fsync),
//  el .wal ya no es necesario y se trunca a 0 bytes.
// ═══════════════════════════════════════════════════════════════════════
void WAL::truncate() {
    close(fd_);
    fd_ = -1;

    // O_TRUNC trunca el archivo a 0 bytes al abrirlo
    fd_ = open(wal_filename_.c_str(),
               O_WRONLY | O_CREAT | O_TRUNC,
               S_IRUSR | S_IWUSR);

    if (fd_ == -1) {
        throw std::runtime_error("WAL: no se pudo truncar");
    }

    // fsync para confirmar que el truncado llegó al disco
    fsync(fd_);
}
