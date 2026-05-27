#include <gtest/gtest.h>
#include "storage/disk_manager.hpp"
#include "storage/file_writer.hpp"
#include "storage/page.hpp"
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace {

std::filesystem::path test_path(const std::string& name) {
    return std::filesystem::temp_directory_path() / name;
}

void cleanup(const std::filesystem::path& path) {
    if (std::filesystem::exists(path)) {
        std::filesystem::remove(path);
    }
}

std::string read_string(const Page& page, slot_id_t slot_id) {
    offset_t len = 0;
    const char* data = page.read(slot_id, len);
    return std::string(data, len);
}

} // namespace

TEST(FileWriterTest, CrearArchivoNuevo) {
    auto path = test_path("dinodb_file_new.db");
    cleanup(path);

    FileWriter writer(path.string());

    EXPECT_EQ(writer.file_size(), 0u);
    cleanup(path);
}

TEST(FileWriterTest, EscrituraPosicionadaYLectura) {
    auto path = test_path("dinodb_file_roundtrip.db");
    cleanup(path);

    FileWriter writer(path.string());
    const char* data = "abc123";
    writer.write_at(0, data, 6);
    writer.flush();

    char buffer[6] {};
    writer.read_at(0, buffer, 6);

    EXPECT_EQ(std::string(buffer, 6), "abc123");
    cleanup(path);
}

TEST(FileWriterTest, EscrituraEnOffset) {
    auto path = test_path("dinodb_file_offset.db");
    cleanup(path);

    FileWriter writer(path.string());
    writer.write_at(0, "AAAA", 4);
    writer.write_at(8, "BBBB", 4);

    char left[4] {};
    char right[4] {};
    writer.read_at(0, left, 4);
    writer.read_at(8, right, 4);

    EXPECT_EQ(std::string(left, 4), "AAAA");
    EXPECT_EQ(std::string(right, 4), "BBBB");
    cleanup(path);
}

TEST(FileWriterTest, TamanioArchivoCorrecto) {
    auto path = test_path("dinodb_file_size.db");
    cleanup(path);

    FileWriter writer(path.string());
    writer.write_at(128, "X", 1);

    EXPECT_EQ(writer.file_size(), 129u);
    cleanup(path);
}

TEST(PageTest, InsertYLeer) {
    Page page(0);
    slot_id_t slot = page.insert("Hola SGBD", 9);

    EXPECT_EQ(read_string(page, slot), "Hola SGBD");
}

TEST(PageTest, VariosRegistrosConsecutivos) {
    Page page(0);
    std::vector<std::string> values { "alfa", "beta", "gamma", "delta" };
    std::vector<slot_id_t> slots;

    for (const auto& value : values) {
        slots.push_back(page.insert(value.c_str(), static_cast<offset_t>(value.size())));
    }

    for (size_t i = 0; i < values.size(); ++i) {
        EXPECT_EQ(read_string(page, slots[i]), values[i]);
    }
}

TEST(PageTest, EliminarMarcaTombstone) {
    Page page(0);
    slot_id_t slot = page.insert("borrar", 6);

    page.remove(slot);

    offset_t len = 0;
    EXPECT_THROW(page.read(slot, len), std::runtime_error);
}

TEST(PageTest, EspacioLibreDecreceAlInsertar) {
    Page page(0);
    size_t before = page.free_space();

    page.insert("registro", 8);

    EXPECT_LT(page.free_space(), before);
}

TEST(PageTest, SlotFueraDeRangoLanza) {
    Page page(0);
    offset_t len = 0;

    EXPECT_THROW(page.read(7, len), std::runtime_error);
}

TEST(PageTest, CompactEliminaTombstones) {
    Page page(0);
    page.insert("uno", 3);
    slot_id_t removed = page.insert("dos", 3);
    page.insert("tres", 4);
    page.remove(removed);

    page.compact();

    EXPECT_EQ(page.slot_count(), 2);
    EXPECT_EQ(read_string(page, 0), "uno");
    EXPECT_EQ(read_string(page, 1), "tres");
}

TEST(PageTest, ReutilizacionDeSlot) {
    Page page(0);
    slot_id_t first = page.insert("primero", 7);
    slot_id_t second = page.insert("segundo", 7);
    page.remove(first);

    slot_id_t reused = page.insert("nuevo", 5);

    EXPECT_EQ(reused, first);
    EXPECT_EQ(second, 1);
    EXPECT_EQ(read_string(page, reused), "nuevo");
}

TEST(PageTest, PaginaLlenaLanzaExcepcion) {
    Page page(0);
    std::string large(PAGE_SIZE, 'x');

    EXPECT_THROW(page.insert(large.c_str(), static_cast<offset_t>(large.size())), std::runtime_error);
}

TEST(PageTest, SerializacionRoundTrip) {
    Page original(42);
    slot_id_t slot = original.insert("persistencia", 12);

    char buffer[PAGE_SIZE];
    original.to_bytes(buffer);
    Page restored = Page::from_bytes(buffer);

    EXPECT_EQ(restored.page_id(), 42u);
    EXPECT_EQ(read_string(restored, slot), "persistencia");
}

TEST(PageTest, SerializacionConVariosRegistros) {
    Page original(10);
    original.insert("uno", 3);
    original.insert("dos", 3);
    original.insert("tres", 4);

    char buffer[PAGE_SIZE];
    original.to_bytes(buffer);
    Page restored = Page::from_bytes(buffer);

    EXPECT_EQ(restored.slot_count(), 3);
    EXPECT_EQ(read_string(restored, 0), "uno");
    EXPECT_EQ(read_string(restored, 1), "dos");
    EXPECT_EQ(read_string(restored, 2), "tres");
}

TEST(DiskManagerTest, AllocarYLeer) {
    auto path = test_path("dinodb_disk_alloc.db");
    cleanup(path);
    DiskManager disk(path.string());

    page_id_t page_id = disk.allocate_page();
    Page page = disk.read_page(page_id);

    EXPECT_EQ(page_id, 0u);
    EXPECT_EQ(page.page_id(), page_id);
    EXPECT_EQ(disk.page_count(), 1u);
    cleanup(path);
}

TEST(DiskManagerTest, EscribirLeerDatos) {
    auto path = test_path("dinodb_disk_write.db");
    cleanup(path);
    DiskManager disk(path.string());

    page_id_t page_id = disk.allocate_page();
    Page page = disk.read_page(page_id);
    slot_id_t slot = page.insert("dato en disco", 13);
    disk.write_page(page_id, page);
    disk.flush();

    DiskManager reopened(path.string());
    Page restored = reopened.read_page(page_id);

    EXPECT_EQ(read_string(restored, slot), "dato en disco");
    cleanup(path);
}

TEST(DiskManagerTest, MultiplesPaginas) {
    auto path = test_path("dinodb_disk_many.db");
    cleanup(path);
    DiskManager disk(path.string());

    for (int i = 0; i < 5; ++i) {
        disk.allocate_page();
    }

    EXPECT_EQ(disk.page_count(), 5u);
    cleanup(path);
}

TEST(DiskManagerTest, PaginaIdsConsecutivos) {
    auto path = test_path("dinodb_disk_ids.db");
    cleanup(path);
    DiskManager disk(path.string());

    EXPECT_EQ(disk.allocate_page(), 0u);
    EXPECT_EQ(disk.allocate_page(), 1u);
    EXPECT_EQ(disk.allocate_page(), 2u);
    cleanup(path);
}

TEST(DiskManagerTest, LeerPaginaInexistenteLanza) {
    auto path = test_path("dinodb_disk_missing.db");
    cleanup(path);
    DiskManager disk(path.string());

    EXPECT_THROW(disk.read_page(0), std::runtime_error);
    cleanup(path);
}

TEST(DiskManagerTest, PersistenciaTrasReinicio) {
    auto path = test_path("dinodb_disk_restart.db");
    cleanup(path);
    page_id_t page_id = 0;
    slot_id_t slot = 0;

    {
        DiskManager disk(path.string());
        page_id = disk.allocate_page();
        Page page(page_id);
        slot = page.insert("sobrevive", 9);
        disk.write_page(page_id, page);
        disk.flush();
    }

    DiskManager reopened(path.string());
    Page restored = reopened.read_page(page_id);

    EXPECT_EQ(read_string(restored, slot), "sobrevive");
    cleanup(path);
}
