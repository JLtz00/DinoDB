/**
 * @file    page.h
 * @brief   Definición y diseño de la estructura de páginas de tamaño fijo
 *
 * TEMA: Storage Manager I — Estructura de páginas
 * ──────────────────────────────────────────────────────────────────────
 *
 * Un gestor de base de datos no almacena registros de forma contigua
 * en disco. En cambio, organiza el archivo en bloques de tamaño fijo
 * llamados PÁGINAS. Esto permite:
 *
 *   1. Leer y escribir exactamente un bloque del disco por operación
 *      (alineado con el tamaño de bloque del sistema operativo).
 *   2. Gestionar el espacio libre de forma eficiente.
 *   3. Identificar cualquier página por su número (page_id).
 *
 * LAYOUT DE UNA PÁGINA (Slotted Page):
 *
 *   Byte 0                                        Byte 4095
 *   ┌───────────┬──────────────┬────────────┬──────────────────────┐
 *   │  Header   │  Slot[0..N]  │  espacio   │  rec[N]..rec[1]rec[0]│
 *   │ (8 bytes) │  (4B c/u)    │   libre    │  (crecen desde atrás)│
 *   └───────────┴──────────────┴────────────┴──────────────────────┘
 *                               ↑                         ↑
 *                        slot_array_end             free_offset
 *
 * Referencia: "From Files To Databases", Cap. 2 — Page Layout
 */

#pragma once
#include <cstdint>
#include <cstring>

// ── Tamaño de página: 4 KB ─────────────────────────────────────────────
// Coincide con el tamaño de bloque estándar del sistema operativo.
// Esto evita lecturas/escrituras parciales y maximiza la eficiencia de I/O.
static constexpr uint32_t PAGE_SIZE = 4096;  // bytes

// ═══════════════════════════════════════════════════════════════════════
//  PageHeader — Metadatos de control al inicio de cada página
//
//  Ocupa los primeros 8 bytes de page.data[].
//  Permite al gestor conocer el estado de la página sin leer todo su
//  contenido.
// ═══════════════════════════════════════════════════════════════════════
struct PageHeader {
    uint32_t page_id;      // Identificador único de esta página en el archivo
    uint16_t num_slots;    // Cantidad de registros actualmente almacenados
    uint16_t free_offset;  // Posición donde empieza el espacio libre
                           // (los registros crecen desde el final hacia aquí)
};

// ═══════════════════════════════════════════════════════════════════════
//  Slot — Descriptor de posición de un registro dentro de la página
//
//  Cada slot ocupa 4 bytes y vive en el "slot array", justo después
//  del header. El slot[i] describe dónde está el i-ésimo registro.
// ═══════════════════════════════════════════════════════════════════════
struct Slot {
    uint16_t offset;  // Posición del registro dentro de page.data[]
    uint16_t length;  // Tamaño en bytes del registro
};

// ═══════════════════════════════════════════════════════════════════════
//  Page — Bloque de datos de tamaño fijo (4 KB)
//
//  Toda la información de la página vive en el array data[PAGE_SIZE].
//  El PageHeader y los Slots se leen/escriben mediante reinterpret_cast
//  sobre las primeras posiciones de ese array (zero-copy).
// ═══════════════════════════════════════════════════════════════════════
class Page {
public:
    // Bloque crudo de 4096 bytes — esto es exactamente lo que se
    // escribe y lee del archivo binario en disco.
    uint8_t data[PAGE_SIZE];

    /**
     * Inicializa la página como vacía.
     * Pone a cero todo el bloque y configura el header.
     * @param page_id  Identificador único de esta página.
     */
    void init(uint32_t page_id);

    /**
     * Inserta un registro en la página.
     * El registro se escribe al final del espacio libre (crece desde atrás).
     * El slot correspondiente se agrega al slot array (crece desde el frente).
     *
     * @param record  Puntero a los bytes del registro.
     * @param length  Tamaño del registro en bytes.
     * @return        slot_id asignado, o -1 si no hay espacio suficiente.
     */
    int insert(const void* record, uint16_t length);

    /**
     * Lee un registro por su slot_id.
     * @param slot_id     Índice del slot (0-based).
     * @param out_length  Se rellena con el tamaño del registro leído.
     * @return            Puntero al inicio del registro, o nullptr si inválido.
     */
    const void* get(int slot_id, uint16_t& out_length) const;

    // ── Consultas de estado ─────────────────────────────
    uint32_t getPageId()    const;  // ID de esta página
    uint16_t getNumSlots()  const;  // Número de registros almacenados
    uint16_t getFreeSpace() const;  // Bytes disponibles para nuevos registros

private:
    // Accesores internos al header y al slot array.
    // Usan reinterpret_cast sobre data[] — sin copias de memoria.
    PageHeader*       header();
    const PageHeader* header() const;
    Slot*             slots();
    const Slot*       slots() const;
};
