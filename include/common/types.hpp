#pragma once

#include <cstdint>
#include <cstddef>
#include <limits>

/// Tamaño fijo de cada página en bytes (4 KB)
static constexpr size_t PAGE_SIZE = 4096;

/// Identificador inválido para páginas (equivalente a null)
static constexpr uint32_t INVALID_PAGE_ID = std::numeric_limits<uint32_t>::max();

/// Identificador inválido para frames del Buffer Pool
static constexpr int32_t INVALID_FRAME_ID = -1;

/// Identificador único de una página en disco
using page_id_t = uint32_t;

/// Identificador de un frame dentro del Buffer Pool
using frame_id_t = int32_t;

/// Identificador de un slot dentro de una página
using slot_id_t = uint16_t;

/// Tamaño de datos en bytes (dentro de una página)
using offset_t  = uint16_t;
/**
 * @brief Identifica de forma única un registro en disco.
 *
 * Cada registro se localiza con dos valores:
 *   - page_id : en qué página del archivo .db vive
 *   - slot_id : en qué slot del Slot Directory de esa página
 */
struct RID {
    page_id_t page_id { INVALID_PAGE_ID };
    slot_id_t slot_id { 0 };

    bool is_valid() const { return page_id != INVALID_PAGE_ID; }

    bool operator==(const RID& o) const {
        return page_id == o.page_id && slot_id == o.slot_id;
    }
    bool operator!=(const RID& o) const { return !(*this == o); }
};
