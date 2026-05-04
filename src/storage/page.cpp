#include "storage/page.hpp"
#include <cstring>
#include <stdexcept>
#include <string>

// ============================================================
//  page.cpp  —  Semana 3: Implementación de la página 4 KB
// ============================================================

// ------------------------------------------------------------
// Constructor: inicializa una página limpia
// ------------------------------------------------------------
Page::Page(page_id_t pid) {
    std::memset(data_, 0, PAGE_SIZE);
    header_.page_id         = pid;
    header_.free_space_end  = sizeof(PageHeader);
    header_.slot_count      = 0;
    header_.free_slot_count = 0;
}

// ------------------------------------------------------------
// from_bytes: reconstruye la Page desde bytes crudos de disco
// ------------------------------------------------------------
Page Page::from_bytes(const char* raw) {
    Page p;
    std::memcpy(p.data_, raw, PAGE_SIZE);
    return p;
}

// ------------------------------------------------------------
// to_bytes: serializa la página hacia un buffer externo
// ------------------------------------------------------------
void Page::to_bytes(char* dest) const {
    std::memcpy(dest, data_, PAGE_SIZE);
}

// ------------------------------------------------------------
// free_space: espacio entre área de datos y Slot Directory
// ------------------------------------------------------------
size_t Page::free_space() const {
    return slot_dir_start() - header_.free_space_end;
}

// ------------------------------------------------------------
// can_fit: ¿cabe un registro de `length` bytes + un SlotEntry?
// ------------------------------------------------------------
bool Page::can_fit(offset_t length) const {
    // Necesitamos espacio para el dato más una nueva SlotEntry
    return free_space() >= static_cast<size_t>(length) + sizeof(SlotEntry);
}

// ------------------------------------------------------------
// insert: inserta un registro y devuelve su slot_id
// ------------------------------------------------------------
/**
 * Estrategia:
 *  1. Si hay slots eliminados reutilizables, reciclar uno.
 *  2. Si no, agregar un nuevo SlotEntry al directorio (crece hacia atrás).
 *  3. Copiar los datos en free_space_end y avanzar el puntero.
 */
slot_id_t Page::insert(const char* data, offset_t length) {
    if (!can_fit(length)) {
        throw std::runtime_error(
            "Page::insert: no hay espacio (libre=" +
            std::to_string(free_space()) +
            ", requerido=" + std::to_string(length) + ")"
        );
    }

    // Buscar slot reciclable (offset == 0 → tombstone)
    slot_id_t sid = header_.slot_count; // por defecto, slot nuevo
    bool recycled = false;

    if (header_.free_slot_count > 0) {
        // El directorio crece hacia atrás; los slots están al final de data_
        SlotEntry* dir = reinterpret_cast<SlotEntry*>(data_ + PAGE_SIZE)
                         - header_.slot_count;
        for (slot_id_t i = 0; i < header_.slot_count; ++i) {
            if (dir[i].offset == 0) {
                sid = i;
                recycled = true;
                break;
            }
        }
    }

    // Escribir el dato en el área libre
    offset_t data_offset = header_.free_space_end;
    std::memcpy(data_ + data_offset, data, length);
    header_.free_space_end += length;

    // Actualizar SlotEntry
    if (recycled) {
        // Reutilizar slot existente
        SlotEntry* dir = reinterpret_cast<SlotEntry*>(data_ + PAGE_SIZE)
                         - header_.slot_count;
        dir[sid].offset = data_offset;
        dir[sid].length = length;
        --header_.free_slot_count;
    } else {
        // Nuevo slot: el directorio crece hacia atrás (slot_count aumenta)
        ++header_.slot_count;
        SlotEntry* dir = reinterpret_cast<SlotEntry*>(data_ + PAGE_SIZE)
                         - header_.slot_count;
        dir[sid].offset = data_offset; // dir[0] es el slot más nuevo
        dir[sid].length = length;
    }

    return sid;
}

// ------------------------------------------------------------
// read: retorna puntero interno al dato del slot indicado
// ------------------------------------------------------------
const char* Page::read(slot_id_t slot_id, offset_t& out_len) const {
    if (slot_id >= header_.slot_count) {
        throw std::runtime_error(
            "Page::read: slot_id " + std::to_string(slot_id) +
            " fuera de rango (total=" + std::to_string(header_.slot_count) + ")"
        );
    }

    const SlotEntry* dir = reinterpret_cast<const SlotEntry*>(data_ + PAGE_SIZE)
                           - header_.slot_count;
    const SlotEntry& entry = dir[slot_id];

    if (entry.offset == 0) {
        throw std::runtime_error(
            "Page::read: slot_id " + std::to_string(slot_id) +
            " fue eliminado (tombstone)"
        );
    }

    out_len = entry.length;
    return data_ + entry.offset;
}

// ------------------------------------------------------------
// remove: marca un slot como eliminado (tombstone)
// ------------------------------------------------------------
/**
 * No libera el espacio físico inmediatamente.
 * El espacio se recupera al llamar compact().
 */
void Page::remove(slot_id_t slot_id) {
    if (slot_id >= header_.slot_count) {
        throw std::runtime_error(
            "Page::remove: slot_id " + std::to_string(slot_id) + " inválido"
        );
    }

    SlotEntry* dir = reinterpret_cast<SlotEntry*>(data_ + PAGE_SIZE)
                     - header_.slot_count;
    SlotEntry& entry = dir[slot_id];

    if (entry.offset == 0) {
        throw std::runtime_error(
            "Page::remove: slot_id " + std::to_string(slot_id) +
            " ya fue eliminado"
        );
    }

    // Tombstone: offset = 0 significa "eliminado"
    entry.offset = 0;
    entry.length = 0;
    ++header_.free_slot_count;
}

// ------------------------------------------------------------
// compact: reorganiza datos para eliminar huecos
// ------------------------------------------------------------
/**
 * Crea un buffer temporal, copia los registros vivos en orden
 * y reconstruye el Slot Directory sin tombstones.
 * Costo: O(n) — se usa con poca frecuencia.
 */
void Page::compact() {
    char temp[PAGE_SIZE];
    std::memset(temp, 0, PAGE_SIZE);

    // Copiar header al buffer temporal
    PageHeader* new_header = reinterpret_cast<PageHeader*>(temp);
    *new_header = header_;
    new_header->free_space_end  = sizeof(PageHeader);
    new_header->slot_count      = 0;
    new_header->free_slot_count = 0;

    const SlotEntry* old_dir = reinterpret_cast<const SlotEntry*>(data_ + PAGE_SIZE)
                               - header_.slot_count;

    for (slot_id_t i = 0; i < header_.slot_count; ++i) {
        if (old_dir[i].offset == 0) continue; // tombstone, saltar

        // Copiar dato al nuevo buffer
        offset_t new_offset = new_header->free_space_end;
        std::memcpy(temp + new_offset, data_ + old_dir[i].offset, old_dir[i].length);
        new_header->free_space_end += old_dir[i].length;

        // Agregar SlotEntry en el nuevo directorio
        ++new_header->slot_count;
        SlotEntry* new_dir = reinterpret_cast<SlotEntry*>(temp + PAGE_SIZE)
                             - new_header->slot_count;
        new_dir[0].offset = new_offset;
        new_dir[0].length = old_dir[i].length;
    }

    std::memcpy(data_, temp, PAGE_SIZE);
}