#include <gtest/gtest.h>
#include "query/database.hpp"
#include "query/sql_parser.hpp"
#include "storage/disk_manager.hpp"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <variant>

namespace {

std::filesystem::path database_path(const std::string& name) {
    auto path = std::filesystem::temp_directory_path() / name;
    std::filesystem::remove_all(path);
    return path;
}

} // namespace

TEST(SqlParserTest, ReconoceSentenciasSinImportarMayusculas) {
    SqlStatement parsed = SqlParser::parse(
        "SeLeCt id, nota FROM alumnos WHERE nota >= 11 AND (id != 4 OR id = -2);");

    ASSERT_TRUE(std::holds_alternative<SelectStatement>(parsed));
    const auto& select = std::get<SelectStatement>(parsed);
    EXPECT_EQ(select.table, "alumnos");
    EXPECT_EQ(select.columns, std::vector<std::string>({ "id", "nota" }));
    ASSERT_NE(select.where, nullptr);
    EXPECT_EQ(select.where->kind, WhereExpression::Kind::logical_and);
}

TEST(SqlParserTest, RechazaTiposNoSoportados) {
    EXPECT_THROW(
        SqlParser::parse("CREATE TABLE personas (id INT, nombre VARCHAR);"),
        std::runtime_error);
}

TEST(SqlParserTest, ReconoceTiposYCadenasConComillasEscapadas) {
    SqlStatement create = SqlParser::parse(
        "CREATE TABLE eventos (id INT, titulo TEXT, fecha DATE, inicio TIME);");
    ASSERT_TRUE(std::holds_alternative<CreateTableStatement>(create));
    const auto& columns = std::get<CreateTableStatement>(create).columns;
    ASSERT_EQ(columns.size(), 4u);
    EXPECT_EQ(columns[0].type, ValueType::integer);
    EXPECT_EQ(columns[1].type, ValueType::text);
    EXPECT_EQ(columns[2].type, ValueType::date);
    EXPECT_EQ(columns[3].type, ValueType::hour);

    SqlStatement insert = SqlParser::parse(
        "INSERT INTO eventos VALUES (1, 'Charla de O''Connor', "
        "DATE '2026-07-24', TIME '09:30');");
    ASSERT_TRUE(std::holds_alternative<InsertStatement>(insert));
    const auto& values = std::get<InsertStatement>(insert).values;
    ASSERT_EQ(values.size(), 4u);
    EXPECT_EQ(values[1].as_text(), "Charla de O'Connor");
    EXPECT_EQ(values[2].to_string(), "2026-07-24");
    EXPECT_EQ(values[3].to_string(), "09:30:00");
}

TEST(SqlEngineTest, CreaInsertaYConsultaConPlanVolcano) {
    auto path = database_path("dinodb_sql_engine_e2e");
    Database database(path);

    QueryResult created = database.execute(
        "CREATE TABLE alumnos (id INT, edad INT, nota INT);");
    EXPECT_EQ(created.message, "Tabla 'alumnos' creada");

    database.execute("INSERT INTO alumnos VALUES (1, 17, 14);");
    database.execute("INSERT INTO alumnos VALUES (2, 20, 18);");
    database.execute("INSERT INTO alumnos VALUES (3, 21, 9);");
    database.execute("INSERT INTO alumnos VALUES (4, 25, 19);");

    QueryResult selected = database.execute(
        "SELECT id, nota FROM alumnos "
        "WHERE edad >= 18 AND (nota >= 11 OR id = 3);");

    EXPECT_EQ(selected.columns, std::vector<std::string>({ "id", "nota" }));
    ASSERT_EQ(selected.rows.size(), 3u);
    EXPECT_EQ(selected.rows[0].values, std::vector<int32_t>({ 2, 18 }));
    EXPECT_EQ(selected.rows[1].values, std::vector<int32_t>({ 3, 9 }));
    EXPECT_EQ(selected.rows[2].values, std::vector<int32_t>({ 4, 19 }));
    EXPECT_EQ(selected.plan, "SeqScan(alumnos) -> Selection -> Projection");

    std::filesystem::remove_all(path);
}

TEST(SqlEngineTest, CatalogoYRegistrosPersistenTrasReabrir) {
    auto path = database_path("dinodb_sql_engine_reopen");
    {
        Database database(path);
        database.execute("CREATE TABLE productos (codigo INT, stock INT);");
        database.execute("INSERT INTO productos VALUES (10, 7);");
        database.execute("INSERT INTO productos VALUES (20, 0);");
    }

    Database reopened(path);
    EXPECT_EQ(reopened.list_tables(), std::vector<std::string>({ "productos" }));
    EXPECT_EQ(
        reopened.describe("PRODUCTOS").columns,
        std::vector<std::string>({ "codigo", "stock" }));

    QueryResult selected = reopened.execute(
        "SELECT * FROM productos WHERE stock = 0;");
    ASSERT_EQ(selected.rows.size(), 1u);
    EXPECT_EQ(selected.rows[0].values, std::vector<int32_t>({ 20, 0 }));

    QueryResult tables = reopened.execute("SHOW TABLES;");
    ASSERT_EQ(tables.text_rows.size(), 1u);
    EXPECT_EQ(tables.text_rows[0], std::vector<std::string>({ "productos" }));

    QueryResult schema = reopened.execute("DESCRIBE productos;");
    ASSERT_EQ(schema.text_rows.size(), 2u);
    EXPECT_EQ(schema.text_rows[0], std::vector<std::string>({ "codigo", "INT" }));

    std::filesystem::remove_all(path);
}

TEST(SqlEngineTest, ValidaEsquemaYConsultas) {
    auto path = database_path("dinodb_sql_engine_errors");
    Database database(path);
    database.execute("CREATE TABLE medidas (id INT, valor INT);");

    EXPECT_THROW(
        database.execute("CREATE TABLE medidas (otro INT);"),
        std::runtime_error);
    EXPECT_THROW(
        database.execute("CREATE TABLE repetida (id INT, id INT);"),
        std::runtime_error);
    EXPECT_THROW(
        database.execute("INSERT INTO medidas VALUES (1);"),
        std::runtime_error);
    EXPECT_THROW(
        database.execute("SELECT inexistente FROM medidas;"),
        std::runtime_error);
    EXPECT_THROW(
        database.execute("SELECT * FROM desconocida;"),
        std::runtime_error);

    std::filesystem::remove_all(path);
}

TEST(SqlEngineTest, PersisteYConsultaIntTextDateHour) {
    auto path = database_path("dinodb_sql_engine_typed");
    {
        Database database(path);
        database.execute(
            "CREATE TABLE eventos "
            "(id INT, titulo TEXT, fecha DATE, inicio HOUR);");
        database.execute(
            "INSERT INTO eventos VALUES "
            "(1, 'Inicio de clases', '2026-03-30', '08:00');");
        database.execute(
            "INSERT INTO eventos VALUES "
            "(2, 'Examen de O''Connor', '2026-07-24', '14:30:15');");
        database.execute(
            "INSERT INTO eventos VALUES "
            "(3, 'Cierre', DATE '2026-07-31', HOUR '18:00');");
    }

    Database reopened(path);
    TableSchema schema = reopened.describe("eventos");
    EXPECT_EQ(schema.types, std::vector<ValueType>({
        ValueType::integer, ValueType::text, ValueType::date, ValueType::hour
    }));

    QueryResult selected = reopened.execute(
        "SELECT titulo, fecha, inicio FROM eventos "
        "WHERE fecha >= '2026-07-01' AND inicio < '18:00:00';");
    ASSERT_EQ(selected.rows.size(), 1u);
    EXPECT_EQ(selected.rows[0].value(0).as_text(), "Examen de O'Connor");
    EXPECT_EQ(selected.rows[0].value(1).to_string(), "2026-07-24");
    EXPECT_EQ(selected.rows[0].value(2).to_string(), "14:30:15");
    EXPECT_EQ(selected.plan, "SeqScan(eventos) -> Selection -> Projection");

    QueryResult described = reopened.execute("DESCRIBE eventos;");
    EXPECT_EQ(described.text_rows[1], std::vector<std::string>({ "titulo", "TEXT" }));
    EXPECT_EQ(described.text_rows[2], std::vector<std::string>({ "fecha", "DATE" }));
    EXPECT_EQ(described.text_rows[3], std::vector<std::string>({ "inicio", "HOUR" }));

    std::filesystem::remove_all(path);
}

TEST(SqlEngineTest, RechazaValoresConTipoOFormatoIncorrecto) {
    auto path = database_path("dinodb_sql_engine_type_errors");
    Database database(path);
    database.execute(
        "CREATE TABLE eventos (id INT, titulo TEXT, fecha DATE, inicio HOUR);");

    EXPECT_THROW(
        database.execute(
            "INSERT INTO eventos VALUES ('uno', 'Demo', '2026-07-24', '10:00');"),
        std::runtime_error);
    EXPECT_THROW(
        database.execute(
            "INSERT INTO eventos VALUES (1, 99, '2026-07-24', '10:00');"),
        std::runtime_error);
    EXPECT_THROW(
        database.execute(
            "INSERT INTO eventos VALUES (1, 'Demo', '2025-02-29', '10:00');"),
        std::runtime_error);
    EXPECT_THROW(
        database.execute(
            "INSERT INTO eventos VALUES (1, 'Demo', '2026-07-24', '24:00');"),
        std::runtime_error);
    EXPECT_THROW(
        database.execute("SELECT * FROM eventos WHERE fecha >= 20260724;"),
        std::runtime_error);

    std::filesystem::remove_all(path);
}

TEST(SqlEngineTest, LeeCatalogoYRegistrosIntDelFormatoAnterior) {
    auto path = database_path("dinodb_sql_engine_legacy");
    std::filesystem::create_directories(path);
    {
        std::ofstream catalog(path / "catalog.meta");
        catalog << "DINODB_CATALOG_V1\nlegacy|id,valor\n";
    }

    DiskManager disk((path / "legacy.table.db").string());
    page_id_t page_id = disk.allocate_page();
    Page page = disk.read_page(page_id);
    std::string legacy(sizeof(uint16_t) + 2 * sizeof(int32_t), '\0');
    uint16_t columns = 2;
    int32_t id = 7;
    int32_t value = 70;
    std::memcpy(legacy.data(), &columns, sizeof(columns));
    std::memcpy(legacy.data() + sizeof(columns), &id, sizeof(id));
    std::memcpy(
        legacy.data() + sizeof(columns) + sizeof(id), &value, sizeof(value));
    page.insert(legacy.data(), static_cast<offset_t>(legacy.size()));
    disk.write_page(page_id, page);
    disk.flush();

    Database database(path);
    EXPECT_EQ(database.describe("legacy").types, std::vector<ValueType>({
        ValueType::integer, ValueType::integer
    }));
    QueryResult selected = database.execute(
        "SELECT * FROM legacy WHERE valor >= 50;");
    ASSERT_EQ(selected.rows.size(), 1u);
    EXPECT_EQ(selected.rows[0].values, std::vector<int32_t>({ 7, 70 }));

    std::filesystem::remove_all(path);
}
