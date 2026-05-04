#include "storage/file_writer.hpp"
#include <fcntl.h>      // open, O_RDWR, O_CREAT
#include <unistd.h>     // pread, pwrite, fsync, close, lseek
#include <sys/stat.h>   // fstat
#include <stdexcept>
#include <string>
#include <cerrno>
#include <cstring>      // strerror
// ------------------------------------------------------------
// Constructor: abre o crea el archivo en modo lectura/escritura
// ------------------------------------------------------------
FileWriter::FileWriter(const std::string& path)
    : path_(path)
{
    // O_RDWR   → abrir para leer y escribir
    // O_CREAT  → crear si no existe
    // S_IRUSR | S_IWUSR → permisos 600 (solo el dueño)
    fd_ = ::open(path.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);

    if (fd_ == -1) {
        throw std::runtime_error(
            "FileWriter: no se pudo abrir '" + path +
            "': " + std::strerror(errno)
        );
    }
}

// ------------------------------------------------------------
// Destructor: cierra el file descriptor (RAII)
// ------------------------------------------------------------
FileWriter::~FileWriter() {
    if (fd_ != -1) {
        ::close(fd_);
    }
}

// ------------------------------------------------------------
// write_at — escritura posicionada (no mueve el cursor global)
// ------------------------------------------------------------
/**
 * Usa pwrite() en lugar de lseek + write para que la operación
 * sea atómica respecto a la posición del archivo. Esto evita
 * condiciones de carrera si en el futuro se agrega concurrencia.
 */
void FileWriter::write_at(uint64_t offset, const char* data, size_t size) {
    ssize_t written = ::pwrite(fd_, data, size, static_cast<off_t>(offset));

    if (written == -1) {
        throw std::runtime_error(
            "FileWriter::write_at: error en '" + path_ +
            "' offset=" + std::to_string(offset) +
            ": " + std::strerror(errno)
        );
    }
    // Verificar escritura parcial (disco lleno, etc.)
    if (static_cast<size_t>(written) != size) {
        throw std::runtime_error(
            "FileWriter::write_at: escritura parcial en '" + path_ +
            "' (esperado=" + std::to_string(size) +
            " escrito=" + std::to_string(written) + ")"
        );
    }
}

// ------------------------------------------------------------
// read_at — lectura posicionada
// ------------------------------------------------------------
void FileWriter::read_at(uint64_t offset, char* data, size_t size) const {
    ssize_t bytes_read = ::pread(fd_, data, size, static_cast<off_t>(offset));

    if (bytes_read == -1) {
        throw std::runtime_error(
            "FileWriter::read_at: error en '" + path_ +
            "' offset=" + std::to_string(offset) +
            ": " + std::strerror(errno)
        );
    }
    if (static_cast<size_t>(bytes_read) != size) {
        throw std::runtime_error(
            "FileWriter::read_at: lectura parcial en '" + path_ +
            "' (esperado=" + std::to_string(size) +
            " leído=" + std::to_string(bytes_read) + ")"
        );
    }
}

// ------------------------------------------------------------
// flush — fuerza escritura a disco físico mediante fsync
// ------------------------------------------------------------
/**
 * fsync() garantiza que todos los datos escritos con write_at()
 * pasen del buffer del SO al disco físico. Sin esta llamada,
 * un corte de energía puede provocar pérdida de datos.
 *
 * Es costosa en rendimiento (puede tardar ms), por eso se
 * llama solo después de operaciones críticas, no en cada write.
 */
void FileWriter::flush() {
    if (::fsync(fd_) == -1) {
        throw std::runtime_error(
            "FileWriter::flush: fsync falló en '" + path_ +
            "': " + std::strerror(errno)
        );
    }
}

// ------------------------------------------------------------
// file_size — tamaño actual del archivo
// ------------------------------------------------------------
uint64_t FileWriter::file_size() const {
    struct stat st;
    if (::fstat(fd_, &st) == -1) {
        throw std::runtime_error(
            "FileWriter::file_size: fstat falló: " +
            std::string(std::strerror(errno))
        );
    }
    return static_cast<uint64_t>(st.st_size);
}