#include "storage/page.hpp"
#include <cstring>
#include <stdexcept>
#include <string>

// ============================================================
//  page.cpp  —  Semana 3: Implementación de la página 4 KB
// ============================================================

Page::Page(page_id_t pid) {
    std::memset(data_, 0, PAGE_SIZE);
    header_.page_id         = pid;
    header_.free_space_end  = sizeof(PageHeader);
    header_.slot_count      = 0;
    header_.free_slot_count = 0;
}

Page Page::from_bytes(const char* raw) {
    Page p;
    std::memcpy(p.data_, raw, PAGE_SIZE);
    return p;
}

void Page::to_bytes(char* dest) const {
    std::memcpy(dest, data_, PAGE_SIZE);
}

size_t Page::free_space() const {
    return slot_dir_start() - header_.free_space_end;
}

bool Page::can_fit(offset_t length) const {
    return free_space() >= static_cast<size_t>(length) + sizeof(SlotEntry);
}

// ------------------------------------------------------------
// insert: inserta un registro y devuelve su slot_id
//
// El Slot Directory crece desde el FINAL de la página hacia atrás.
// slot 0 está en: data_ + PAGE_SIZE - 1*sizeof(SlotEntry)
// slot 1 está en: data_ + PAGE_SIZE - 2*sizeof(SlotEntry)
// ...
// slot N está en: data_ + PAGE_SIZE - (N+1)*sizeof(SlotEntry)
//
// Cuando slot_count = N, el directorio tiene N entradas y
// la entrada i se accede como:
//   base = (SlotEntry*)(data_ + PAGE_SIZE) - slot_count
//   base[i]  →  slot i
// ------------------------------------------------------------
slot_id_t Page::insert(const char* data, offset_t length) {
    if (!can_fit(length)) {
        throw std::runtime_error(
            "Page::insert: no hay espacio (libre=" +
            std::to_string(free_space()) +
            ", requerido=" + std::to_string(length) + ")"
        );
    }

    // Escribir dato en área libre
    offset_t data_offset = header_.free_space_end;
    std::memcpy(data_ + data_offset, data, length);
    header_.free_space_end += length;

    // Buscar slot reciclable (tombstone: offset == 0)
    if (header_.free_slot_count > 0) {
        SlotEntry* dir = reinterpret_cast<SlotEntry*>(data_ + PAGE_SIZE)
                         - header_.slot_count;
        for (slot_id_t i = 0; i < header_.slot_count; ++i) {
            if (dir[i].offset == 0) {
                dir[i].offset = data_offset;
                dir[i].length = length;
                --header_.free_slot_count;
                return i;
            }
        }
    }

    // No hay slot reciclable: crear uno nuevo
    // IMPORTANTE: incrementar slot_count ANTES de calcular dir
    // porque el nuevo slot queda en la posición [slot_count-1]
    // del nuevo directorio expandido
    slot_id_t new_sid = header_.slot_count;
    ++header_.slot_count;

    SlotEntry* dir = reinterpret_cast<SlotEntry*>(data_ + PAGE_SIZE)
                     - header_.slot_count;
    // El nuevo slot es dir[0] (el más lejano al final del archivo)
    dir[0].offset = data_offset;
    dir[0].length = length;

    return new_sid;
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

    // slot 0 → dir[slot_count-1], slot N-1 → dir[0]
    // Para acceder al slot_id correcto necesitamos invertir el índice
    slot_id_t idx = header_.slot_count - 1 - slot_id;
    const SlotEntry& entry = dir[idx];

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
// remove: marca slot como tombstone
// ------------------------------------------------------------
void Page::remove(slot_id_t slot_id) {
    if (slot_id >= header_.slot_count) {
        throw std::runtime_error(
            "Page::remove: slot_id " + std::to_string(slot_id) + " inválido"
        );
    }

    SlotEntry* dir = reinterpret_cast<SlotEntry*>(data_ + PAGE_SIZE)
                     - header_.slot_count;
    slot_id_t idx = header_.slot_count - 1 - slot_id;
    SlotEntry& entry = dir[idx];

    if (entry.offset == 0) {
        throw std::runtime_error(
            "Page::remove: slot_id " + std::to_string(slot_id) +
            " ya fue eliminado"
        );
    }

    entry.offset = 0;
    entry.length = 0;
    ++header_.free_slot_count;
}

// ------------------------------------------------------------
// compact: elimina huecos de tombstones
// ------------------------------------------------------------
void Page::compact() {
    char temp[PAGE_SIZE];
    std::memset(temp, 0, PAGE_SIZE);

    PageHeader* new_header = reinterpret_cast<PageHeader*>(temp);
    *new_header = header_;
    new_header->free_space_end  = sizeof(PageHeader);
    new_header->slot_count      = 0;
    new_header->free_slot_count = 0;

    const SlotEntry* old_dir = reinterpret_cast<const SlotEntry*>(data_ + PAGE_SIZE)
                               - header_.slot_count;

    // Recorrer slots en orden original (0..slot_count-1)
    for (slot_id_t i = 0; i < header_.slot_count; ++i) {
        slot_id_t idx = header_.slot_count - 1 - i;
        if (old_dir[idx].offset == 0) continue;

        offset_t new_offset = new_header->free_space_end;
        std::memcpy(temp + new_offset, data_ + old_dir[idx].offset, old_dir[idx].length);
        new_header->free_space_end += old_dir[idx].length;

        slot_id_t new_sid = new_header->slot_count;
        ++new_header->slot_count;
        SlotEntry* new_dir = reinterpret_cast<SlotEntry*>(temp + PAGE_SIZE)
                             - new_header->slot_count;
        new_dir[0].offset = new_offset;
        new_dir[0].length = old_dir[idx].length;
        (void)new_sid;
    }

    std::memcpy(data_, temp, PAGE_SIZE);
}