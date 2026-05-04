/**
 * @file    disk_manager.h
 * @brief   Gestor de disco — Lectura y escritura de páginas en disco
 *
 * TEMA: Storage Manager I — Lectura/escritura de páginas + atomicidad
 * ──────────────────────────────────────────────────────────────────────
 *
 * El DiskManager es la capa que abstrae el acceso al archivo binario
 * del disco. Sus responsabilidades son:
 *
 *   1. LECTURA de páginas:
 *      Dado un page_id, calcula el offset en el archivo y lee
 *      exactamente PAGE_SIZE bytes.
 *
 *   2. ESCRITURA ATÓMICA de páginas:
 *      Usa el protocolo WAL (Write-Ahead Log) para garantizar que
 *      nunca quede una página en estado corrupto en disco.
 *
 *   3. RECOVERY al arrancar:
 *      Si existe un .wal con entradas pendientes de un crash anterior,
 *      las aplica al .db antes de continuar.
 *
 *   4. DURABILIDAD vía fsync():
 *      Garantiza que los datos llegaron al hardware físico y no
 *      solo al caché del sistema operativo.
 *
 * LAYOUT DEL ARCHIVO .db:
 *
 *   Offset 0        4096       8192      12288
 *          ├─────────┼─────────┼─────────┼─────── ...
 *          │ Page 0  │ Page 1  │ Page 2  │
 *          │ 4096 B  │ 4096 B  │ 4096 B  │
 *          └─────────┴─────────┴─────────┴─────── ...
 *
 *   Offset de Page N = N × PAGE_SIZE
 */

#pragma once
#include <fstream>
#include <string>
#include "page.h"
#include "wal.h"

class DiskManager {
public:
    /**
     * Abre (o crea) el archivo de base de datos.
     * Si existe un .wal de un crash anterior, hace recovery automático.
     * @param filename  Ruta al archivo .db
     */
    explicit DiskManager(const std::string& filename);

    ~DiskManager();

    /**
     * Lee una página del archivo .db.
     *
     * Calcula el offset: page_id × PAGE_SIZE
     * Lee exactamente PAGE_SIZE bytes en page.data[]
     *
     * @param page_id  Identificador de la página a leer.
     * @param page     Objeto Page donde se cargarán los datos.
     * @throws std::out_of_range si page_id >= num_pages
     */
    void readPage(uint32_t page_id, Page& page);

    /**
     * Escribe una página en disco de forma ATÓMICA usando WAL.
     *
     * Protocolo garantizado:
     *   1. Escribir la página en .wal  + fsync
     *   2. Escribir COMMIT en .wal     + fsync
     *   3. Escribir la página en .db   + fsync
     *   4. Truncar el .wal
     *
     * @param page_id  Identificador de la página a escribir.
     * @param page     Página con los datos a persistir.
     */
    void writePage(uint32_t page_id, const Page& page);

    /**
     * Fuerza fsync() sobre el archivo .db.
     * Garantiza que todos los cambios llegaron al hardware.
     */
    void syncAll();

    /**
     * Ejecuta un checkpoint: sincroniza y vacía el WAL.
     */
    void checkpoint();

    // Número total de páginas en el archivo .db
    uint32_t getNumPages() const { return num_pages_; }

private:
    std::string  filename_;
    std::string  wal_filename_;
    std::fstream file_;       // Archivo .db principal
    uint32_t     num_pages_;  // Páginas existentes en el archivo
    WAL          wal_;        // Write-Ahead Log

    // Abrir o crear el archivo .db
    void openFile();

    // Replay del WAL al arrancar (recovery de crash)
    void recover();

    // Escritura directa al .db sin pasar por WAL
    // Solo para uso interno (recovery, checkpoint)
    void writePageDirect(uint32_t page_id, const Page& page);
};
