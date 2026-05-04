/**
 * @file    page.cpp
 * @brief   Implementación del layout de páginas de tamaño fijo (Slotted Page)
 *
 * TEMA: Storage Manager I — Lectura y escritura de páginas en disco
 * ──────────────────────────────────────────────────────────────────────
 *
 * Implementa el modelo "Slotted Page" donde:
 *   - Los registros se insertan desde el FINAL de la página hacia el inicio.
 *   - El array de slots crece desde el inicio (tras el header) hacia el final.
 *   - La página está llena cuando ambos extremos se tocan.
 *
 * Esta técnica permite registros de longitud variable sin fragmentación
 * externa, y es usada por PostgreSQL, SQLite y la mayoría de SGBD modernos.
 */

#include "page.h"

// ── Acceso interno al header (zero-copy) ───────────────────────────────
// Se superpone el struct PageHeader sobre los primeros bytes de data[].
// No se copia memoria — se trabaja directamente sobre el bloque de 4KB.
PageHeader* Page::header() {
    return reinterpret_cast<PageHeader*>(data);
}
const PageHeader* Page::header() const {
    return reinterpret_cast<const PageHeader*>(data);
}

// ── Acceso interno al slot array ───────────────────────────────────────
// Los slots viven justo después del header, también dentro de data[].
Slot* Page::slots() {
    return reinterpret_cast<Slot*>(data + sizeof(PageHeader));
}
const Slot* Page::slots() const {
    return reinterpret_cast<const Slot*>(data + sizeof(PageHeader));
}

// ═══════════════════════════════════════════════════════════════════════
//  init() — Inicializar una página vacía
// ═══════════════════════════════════════════════════════════════════════
void Page::init(uint32_t page_id) {
    // Poner a cero los 4096 bytes para evitar datos basura en disco
    std::memset(data, 0, PAGE_SIZE);

    // Configurar el header
    header()->page_id     = page_id;
    header()->num_slots   = 0;
    // free_offset apunta al FINAL de la página:
    // los registros se irán insertando hacia el inicio desde aquí.
    header()->free_offset = PAGE_SIZE;
}

// ═══════════════════════════════════════════════════════════════════════
//  insert() — Insertar un registro en la página
//
//  Proceso:
//    1. Verificar que hay espacio suficiente (slot nuevo + datos).
//    2. Escribir el registro al final del espacio libre (crece hacia atrás).
//    3. Registrar el slot que apunta al nuevo registro.
//    4. Retornar el slot_id asignado.
// ═══════════════════════════════════════════════════════════════════════
int Page::insert(const void* record, uint16_t length) {
    // Fin del slot array si se agrega un slot más
    uint16_t slot_array_size = sizeof(PageHeader)
                             + (header()->num_slots + 1) * sizeof(Slot);

    // Nueva posición del registro (crece desde el final hacia el inicio)
    uint16_t new_free_offset = header()->free_offset - length;

    // Verificar colisión: si el espacio libre se agotó, no cabe
    if (new_free_offset < slot_array_size) {
        return -1;  // Página llena
    }

    // Escribir el registro en el espacio libre
    header()->free_offset = new_free_offset;
    std::memcpy(data + new_free_offset, record, length);

    // Registrar el slot con la posición y tamaño del registro
    int slot_id           = header()->num_slots;
    slots()[slot_id]      = { new_free_offset, length };
    header()->num_slots++;

    return slot_id;
}

// ═══════════════════════════════════════════════════════════════════════
//  get() — Leer un registro por slot_id
// ═══════════════════════════════════════════════════════════════════════
const void* Page::get(int slot_id, uint16_t& out_length) const {
    // Validar rango
    if (slot_id < 0 || slot_id >= header()->num_slots) {
        return nullptr;
    }

    const Slot& s = slots()[slot_id];
    out_length = s.length;
    return data + s.offset;  // Puntero directo dentro de data[]
}

// ── Getters de estado ─────────────────────────────────────────────────
uint32_t Page::getPageId()    const { return header()->page_id; }
uint16_t Page::getNumSlots()  const { return header()->num_slots; }

// Espacio libre = distancia entre el fin del slot array y free_offset
uint16_t Page::getFreeSpace() const {
    uint16_t slot_array_end = sizeof(PageHeader)
                            + header()->num_slots * sizeof(Slot);
    return header()->free_offset - slot_array_end;
}
