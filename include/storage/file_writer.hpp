#pragma once

#include <string>
#include <cstdint>
#include <stdexcept>
/**
 * @brief Manejador de bajo nivel para un archivo binario.
 *
 * Encapsula las llamadas al sistema (open, read, write, fsync,
 * lseek) detrás de una interfaz limpia. Es la capa más baja
 * del Storage Manager; el DiskManager la usa para leer/escribir
 * páginas completas.
 *
 * Uso típico:
 * @code
 *   FileWriter fw("datos.db");
 *   fw.write_at(offset, buffer, PAGE_SIZE);
 *   fw.flush();   // fsync — garantiza que llegó a disco
 * @endcode
 */
class FileWriter {
public:

    /**
     * @brief Abre (o crea) el archivo binario en la ruta indicada.
     * @param path  Ruta del archivo .db
     * @throws std::runtime_error si no se puede abrir/crear.
     */
    explicit FileWriter(const std::string& path);

    /// Cierra el archivo al destruir el objeto (RAII)
    ~FileWriter();

    // Sin copia — un archivo lo maneja un único dueño
    FileWriter(const FileWriter&)            = delete;
    FileWriter& operator=(const FileWriter&) = delete;

    /**
     * @brief Escribe `size` bytes desde `data` en la posición `offset`.
     *
     * La escritura es posicionada (pwrite): no mueve el cursor
     * del archivo, lo que permite escrituras concurrentes futuras.
     *
     * @param offset  Byte exacto donde comienza la escritura
     * @param data    Puntero al buffer de datos a escribir
     * @param size    Cantidad de bytes a escribir
     * @throws std::runtime_error si la escritura falla o es parcial.
     */
    void write_at(uint64_t offset, const char* data, size_t size);

    /**
     * @brief Lee `size` bytes desde la posición `offset` hacia `data`.
     * @param offset  Byte de inicio de la lectura
     * @param data    Buffer destino (debe tener al menos `size` bytes)
     * @param size    Cantidad de bytes a leer
     * @throws std::runtime_error si la lectura falla o es parcial.
     */
    void read_at(uint64_t offset, char* data, size_t size) const;

    /**
     * @brief Fuerza la escritura a disco con fsync.
     *
     * Sin esta llamada, los datos pueden quedar en el caché del
     * sistema operativo y perderse si hay un corte de energía.
     * Se debe llamar después de writes críticos.
     *
     * @throws std::runtime_error si fsync falla.
     */
    void flush();

    /**
     * @brief Retorna el tamaño actual del archivo en bytes.
     */
    uint64_t file_size() const;

private:
    int         fd_;    ///< File descriptor del sistema operativo
    std::string path_;  ///< Ruta guardada para mensajes de error
};