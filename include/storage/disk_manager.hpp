#pragma once

#include "common/types.hpp"
#include "storage/page.hpp"
#include "storage/file_writer.hpp"
#include <string>
#include <memory>

// ============================================================
//  disk_manager.hpp  —  Semana 3: Page Manager
//
//  Responsabilidad: traducir page_id ↔ offset en disco y
//  delegar la I/O al FileWriter.
//
//  Fórmula: offset_en_disco = page_id * PAGE_SIZE
// ============================================================

/**
 * @brief Gestor de páginas en disco.
 *
 * Es la única clase que conoce el archivo .db y calcula
 * en qué byte exacto vive cada página. El Buffer Manager
 * lo usa para traer páginas a RAM y devolverlas al disco.
 *
 * Interfaz mínima:
 *   - read_page(page_id)         → Page
 *   - write_page(page_id, page)  → void
 *   - allocate_page()            → page_id_t (nueva página)
 *   - page_count()               → cuántas páginas existen
 */
class DiskManager {
public:

    /**
     * @brief Abre o crea el archivo de base de datos.
     * @param db_path Ruta del archivo .db (ej. "data/sgbd.db")
     */
    explicit DiskManager(const std::string& db_path);
    ~DiskManager() = default;

    // Sin copia
    DiskManager(const DiskManager&)            = delete;
    DiskManager& operator=(const DiskManager&) = delete;

    /**
     * @brief Lee la página `page_id` desde disco y la retorna.
     * @throws std::runtime_error si page_id >= page_count()
     */
    Page read_page(page_id_t page_id) const;

    /**
     * @brief Escribe la página en disco en su posición correcta.
     *
     * Llama a FileWriter::write_at(page_id * PAGE_SIZE, ...).
     * No hace fsync; el llamador decide cuándo hacer flush.
     */
    void write_page(page_id_t page_id, const Page& page);

    /**
     * @brief Reserva una nueva página al final del archivo.
     *
     * Extiende el archivo en PAGE_SIZE bytes y retorna el
     * page_id asignado. La página se inicializa en ceros.
     *
     * @return page_id_t de la nueva página
     */
    page_id_t allocate_page();

    /**
     * @brief Fuerza escritura de todos los datos a disco (fsync).
     */
    void flush();

    /**
     * @brief Número de páginas actualmente en el archivo.
     */
    page_id_t page_count() const;

private:
    std::unique_ptr<FileWriter> writer_;  ///< Capa de I/O binaria
};