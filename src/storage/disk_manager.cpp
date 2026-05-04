#include "storage/disk_manager.hpp"
#include <stdexcept>
#include <string>
#include <cstring>

// ============================================================
//  disk_manager.cpp  —  Semana 3: Implementación Page Manager
// ============================================================

// ------------------------------------------------------------
// Constructor: abre el archivo .db
// ------------------------------------------------------------
DiskManager::DiskManager(const std::string& db_path)
    : writer_(std::make_unique<FileWriter>(db_path))
{}

// ------------------------------------------------------------
// page_count: cuántas páginas completas hay en el archivo
// ------------------------------------------------------------
page_id_t DiskManager::page_count() const {
    uint64_t size = writer_->file_size();
    return static_cast<page_id_t>(size / PAGE_SIZE);
}

// ------------------------------------------------------------
// read_page: lee PAGE_SIZE bytes y reconstruye la Page
// ------------------------------------------------------------
Page DiskManager::read_page(page_id_t page_id) const {
    if (page_id >= page_count()) {
        throw std::runtime_error(
            "DiskManager::read_page: page_id " +
            std::to_string(page_id) + " no existe (total=" +
            std::to_string(page_count()) + ")"
        );
    }

    char buffer[PAGE_SIZE];
    uint64_t offset = static_cast<uint64_t>(page_id) * PAGE_SIZE;
    writer_->read_at(offset, buffer, PAGE_SIZE);

    return Page::from_bytes(buffer);
}

// ------------------------------------------------------------
// write_page: serializa la Page y la escribe en disco
// ------------------------------------------------------------
void DiskManager::write_page(page_id_t page_id, const Page& page) {
    char buffer[PAGE_SIZE];
    page.to_bytes(buffer);

    uint64_t offset = static_cast<uint64_t>(page_id) * PAGE_SIZE;
    writer_->write_at(offset, buffer, PAGE_SIZE);
}

// ------------------------------------------------------------
// allocate_page: extiende el archivo con una página vacía
// ------------------------------------------------------------
/**
 * La nueva página se inicializa en ceros y se escribe en disco
 * inmediatamente para que el archivo crezca. El page_id es
 * simplemente el índice de la nueva página al final.
 */
page_id_t DiskManager::allocate_page() {
    page_id_t new_id = page_count();

    // Crear página limpia y escribirla al final del archivo
    Page new_page(new_id);
    write_page(new_id, new_page);

    return new_id;
}

// ------------------------------------------------------------
// flush: fuerza todos los writes a disco físico
// ------------------------------------------------------------
void DiskManager::flush() {
    writer_->flush();
}