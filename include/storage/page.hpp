#pragma once

#include "common/types.hpp"
#include <cstring>
#include <stdexcept>
/**
 * @brief Metadatos que encabezan cada página de 4 KB.
 *
 * Campos:
 *  - page_id         : identificador único de esta página
 *  - free_space_end  : offset donde termina el espacio libre
 *                      (los datos se insertan desde aquí)
 *  - slot_count      : cuántos slots existen (incluye eliminados)
 *  - free_slot_count : slots marcados como eliminados (reutilizables)
 */
#pragma pack(push, 1)
struct PageHeader {
    page_id_t page_id        { INVALID_PAGE_ID };
    offset_t  free_space_end { sizeof(PageHeader) }; ///< inicio de datos libres
    slot_id_t slot_count     { 0 };
    slot_id_t free_slot_count{ 0 };
    uint32_t  _reserved      { 0 }; ///< padding hasta 16 bytes
};
#pragma pack(pop)

static_assert(sizeof(PageHeader) == 16, "PageHeader debe ser 16 bytes exactos");

// ------------------------------------------------------------
//  SlotEntry — entrada en el Slot Directory (4 bytes)
// ------------------------------------------------------------
/**
 * @brief Describe la ubicación de un registro dentro de la página.
 *
 * Si offset == 0 el slot fue eliminado (tombstone).
 * El Slot Directory vive al final de la página y crece hacia atrás.
 */
#pragma pack(push, 1)
struct SlotEntry {
    offset_t offset { 0 };  ///< Offset del registro desde inicio de página (0 = borrado)
    offset_t length { 0 };  ///< Longitud del registro en bytes
};
#pragma pack(pop)

static_assert(sizeof(SlotEntry) == 4, "SlotEntry debe ser 4 bytes exactos");

// ------------------------------------------------------------
//  Page — página de tamaño fijo con Slot Directory
// ------------------------------------------------------------
/**
 * @brief Unidad fundamental de almacenamiento del Mini SGBD.
 *
 * Encapsula un bloque de PAGE_SIZE bytes e implementa el
 * Slot Directory para insertar, leer y eliminar registros
 * de longitud variable de forma eficiente.
 *
 * El espacio libre queda entre el último dato insertado y
 * el primer SlotEntry del directorio:
 *
 *   free_space = slot_dir_start - free_space_end
 *
 * Donde slot_dir_start = PAGE_SIZE - slot_count * sizeof(SlotEntry)
 */
class Page {
public:
    // --------------------------------------------------------
    //  Constructor / destructor
    // --------------------------------------------------------

    /// Crea una página limpia con el page_id dado
    explicit Page(page_id_t pid = INVALID_PAGE_ID);

    /// Reconstruye una Page a partir de bytes crudos leídos de disco
    static Page from_bytes(const char* raw);

    // --------------------------------------------------------
    //  Serialización
    // --------------------------------------------------------

    /**
     * @brief Copia los PAGE_SIZE bytes de la página hacia `dest`.
     * @param dest Buffer destino de al menos PAGE_SIZE bytes.
     */
    void to_bytes(char* dest) const;

    // --------------------------------------------------------
    //  Operaciones sobre registros
    // --------------------------------------------------------

    /**
     * @brief Inserta un registro en la página.
     *
     * El dato se copia al área de datos libre; se reserva un
     * SlotEntry al final que apunta a él.
     *
     * @param data   Puntero al dato a insertar
     * @param length Longitud en bytes del dato
     * @return slot_id del registro insertado
     * @throws std::runtime_error si no hay espacio suficiente
     */
    slot_id_t insert(const char* data, offset_t length);

    /**
     * @brief Lee un registro dado su slot_id.
     * @param slot_id  Slot a leer
     * @param out_len  [salida] longitud del registro leído
     * @return Puntero interno a los datos (válido mientras no se modifique la página)
     * @throws std::runtime_error si el slot es inválido o fue eliminado
     */
    const char* read(slot_id_t slot_id, offset_t& out_len) const;

    /**
     * @brief Marca un slot como eliminado (tombstone).
     *
     * No compacta el espacio; para eso existe compact().
     * Incrementa free_slot_count para tracking.
     *
     * @throws std::runtime_error si el slot es inválido o ya eliminado
     */
    void remove(slot_id_t slot_id);
    /// Espacio libre disponible en bytes (entre datos y slots)
    size_t free_space() const;

    /// ¿Cabe un registro de `length` bytes en esta página?
    bool can_fit(offset_t length) const;

    /// Compacta la página eliminando huecos de registros borrados
    void compact();

    // --------------------------------------------------------
    //  Getters
    // --------------------------------------------------------
    page_id_t  page_id()     const { return header_.page_id; }
    slot_id_t  slot_count()  const { return header_.slot_count; }

private:
    char data_[PAGE_SIZE];  ///< Bloque crudo de PAGE_SIZE bytes

    // Acceso tipado a la cabecera (vive al inicio de data_)
    PageHeader& header() {
        return *reinterpret_cast<PageHeader*>(data_);
    }
    const PageHeader& header() const {
        return *reinterpret_cast<const PageHeader*>(data_);
    }

    // Acceso al Slot Directory (vive al final de data_, crece hacia atrás)
    SlotEntry* slot_dir() {
        return reinterpret_cast<SlotEntry*>(data_ + PAGE_SIZE) - header().slot_count;
    }
    const SlotEntry* slot_dir() const {
        return reinterpret_cast<const SlotEntry*>(data_ + PAGE_SIZE) - header().slot_count;
    }

    // Offset donde comienza el directorio de slots
    size_t slot_dir_start() const {
        return PAGE_SIZE - header().slot_count * sizeof(SlotEntry);
    }

    // Alias legible al header cacheado
    PageHeader& header_{ *reinterpret_cast<PageHeader*>(data_) };
};