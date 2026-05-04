// ============================================================
//  test_storage.cpp  —  Pruebas del Storage Manager
//  Semanas 2 y 3: FileWriter, Page, DiskManager
// ============================================================
//
//  Para ejecutar:
//    cd build && cmake .. && cmake --build . && ctest -v
//
//  Requiere Google Test (gtest). Si no lo tienes:
//    sudo apt install libgtest-dev   (Ubuntu/Debian)
//    brew install googletest         (macOS)
// ============================================================

#include <gtest/gtest.h>
#include "storage/page.hpp"
#include "storage/disk_manager.hpp"
#include <cstring>
#include <filesystem>

// ── Helpers ──────────────────────────────────────────────────
static const std::string TEST_DB = "/Users/carlosgc/Desktop/DINO_DB/DinoDB/data/test_storage.db";
void cleanup() {
    if (std::filesystem::exists(TEST_DB))
        std::filesystem::remove(TEST_DB);
}

// ============================================================
//  Tests de Page
// ============================================================

TEST(PageTest, InsertYLeerRegistro) {
    Page p(0);
    const char* dato = "Hola SGBD";
    slot_id_t sid = p.insert(dato, static_cast<offset_t>(strlen(dato)));

    offset_t len = 0;
    const char* leido = p.read(sid, len);

    EXPECT_EQ(len, strlen(dato));
    EXPECT_EQ(std::string(leido, len), "Hola SGBD");
}

TEST(PageTest, InsertarVariosRegistros) {
    Page p(0);
    std::vector<std::string> datos = {"alfa", "beta", "gamma", "delta"};
    std::vector<slot_id_t>   slots;

    for (auto& d : datos)
        slots.push_back(p.insert(d.c_str(), static_cast<offset_t>(d.size())));

    for (size_t i = 0; i < datos.size(); ++i) {
        offset_t len = 0;
        const char* leido = p.read(slots[i], len);
        EXPECT_EQ(std::string(leido, len), datos[i]);
    }
}

TEST(PageTest, EliminarSlotMarcaTombstone) {
    Page p(0);
    const char* dato = "borrar";
    slot_id_t sid = p.insert(dato, static_cast<offset_t>(strlen(dato)));

    p.remove(sid);

    offset_t len = 0;
    EXPECT_THROW(p.read(sid, len), std::runtime_error);
}

TEST(PageTest, EspacioLibreDisminuyeAlInsertar) {
    Page p(0);
    size_t libre_antes = p.free_space();

    const char* dato = "registro";
    p.insert(dato, static_cast<offset_t>(strlen(dato)));

    EXPECT_LT(p.free_space(), libre_antes);
}

TEST(PageTest, SerializacionRoundTrip) {
    Page original(42);
    const char* dato = "persistencia";
    slot_id_t sid = original.insert(dato, static_cast<offset_t>(strlen(dato)));

    // Serializar → deserializar
    char buffer[PAGE_SIZE];
    original.to_bytes(buffer);
    Page restaurada = Page::from_bytes(buffer);

    offset_t len = 0;
    const char* leido = restaurada.read(sid, len);
    EXPECT_EQ(std::string(leido, len), "persistencia");
    EXPECT_EQ(restaurada.page_id(), 42u);
}

// ============================================================
//  Tests de DiskManager
// ============================================================

TEST(DiskManagerTest, AllocarYLeerPagina) {
    cleanup();
    DiskManager dm(TEST_DB);

    page_id_t pid = dm.allocate_page();
    EXPECT_EQ(pid, 0u);
    EXPECT_EQ(dm.page_count(), 1u);

    Page leida = dm.read_page(pid);
    EXPECT_EQ(leida.page_id(), pid);

    cleanup();
}

TEST(DiskManagerTest, EscribirYLeerDatos) {
    cleanup();
    DiskManager dm(TEST_DB);

    page_id_t pid = dm.allocate_page();
    Page p = dm.read_page(pid);

    const char* dato = "dato en disco";
    slot_id_t sid = p.insert(dato, static_cast<offset_t>(strlen(dato)));
    dm.write_page(pid, p);
    dm.flush();

    // Nueva instancia para simular reinicio
    DiskManager dm2(TEST_DB);
    Page p2 = dm2.read_page(pid);
    offset_t len = 0;
    const char* leido = p2.read(sid, len);

    EXPECT_EQ(std::string(leido, len), "dato en disco");
    cleanup();
}

TEST(DiskManagerTest, MultiplesPaginas) {
    cleanup();
    DiskManager dm(TEST_DB);

    for (int i = 0; i < 5; ++i) dm.allocate_page();
    EXPECT_EQ(dm.page_count(), 5u);

    cleanup();
}

// ── main ─────────────────────────────────────────────────────
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}