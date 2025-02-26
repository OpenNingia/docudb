#include "docudb.hpp"
#include <stdexcept>
#include <random>
#include <sstream>
#include <iostream>

using namespace std::string_literals;
using namespace std::string_view_literals;

namespace docudb
{
    namespace details
    {

        namespace sqlite
        {

            struct statement_scope
            {
                statement_scope(sqlite3_stmt *stmt) : stmt_(stmt)
                {
                }

                ~statement_scope()
                {
                    sqlite3_finalize(stmt_);
                }

            private:
                sqlite3_stmt *stmt_;
            };

            template <typename T>
            int bind(sqlite3_stmt *stmt, int index, T value)
            {
                return SQLITE_ERROR;
            }

            template <>
            int bind(sqlite3_stmt *stmt, int index, std::int16_t value)
            {
                return sqlite3_bind_int(stmt, index, value);
            }

            template <>
            int bind(sqlite3_stmt *stmt, int index, std::int32_t value)
            {
                return sqlite3_bind_int(stmt, index, value);
            }

            template <>
            int bind(sqlite3_stmt *stmt, int index, std::int64_t value)
            {
                return sqlite3_bind_int64(stmt, index, value);
            }

            template <>
            int bind(sqlite3_stmt *stmt, int index, std::nullptr_t value)
            {
                return sqlite3_bind_null(stmt, index);
            }

            template <>
            int bind(sqlite3_stmt *stmt, int index, std::string const &value)
            {
                return sqlite3_bind_text(stmt, index, value.data(), -1, SQLITE_TRANSIENT);
            }

            template <>
            int bind(sqlite3_stmt *stmt, int index, std::string_view value)
            {
                return sqlite3_bind_text(stmt, index, value.data(), -1, SQLITE_TRANSIENT);
            }

            template <>
            int bind(sqlite3_stmt *stmt, int index, double value)
            {
                return sqlite3_bind_double(stmt, index, value);
            }

            template <>
            int bind(sqlite3_stmt *stmt, int index, float value)
            {
                return sqlite3_bind_double(stmt, index, value);
            }
        }

        // courtesy of https://stackoverflow.com/questions/24365331/how-can-i-generate-uuid-in-c-without-using-boost-library
        namespace uuid
        {
            static std::random_device rd;
            static std::mt19937 gen(rd());
            static std::uniform_int_distribution<> dis(0, 15);
            static std::uniform_int_distribution<> dis2(8, 11);

            std::string generate_uuid_v4()
            {
                std::stringstream ss;
                int i;
                ss << std::hex;
                for (i = 0; i < 8; i++)
                {
                    ss << dis(gen);
                }
                ss << "-";
                for (i = 0; i < 4; i++)
                {
                    ss << dis(gen);
                }
                ss << "-4";
                for (i = 0; i < 3; i++)
                {
                    ss << dis(gen);
                }
                ss << "-";
                ss << dis2(gen);
                for (i = 0; i < 3; i++)
                {
                    ss << dis(gen);
                }
                ss << "-";
                for (i = 0; i < 12; i++)
                {
                    ss << dis(gen);
                };
                return ss.str();
            }
        }
    }

    db_exception::db_exception(sqlite3 *db_handle, std::string_view msg) : std::runtime_error(std::string(msg) + ": " + sqlite3_errmsg(db_handle)) {}

    database::database(std::string_view path) : db_handle(nullptr)
    {
        sqlite3 *db;
        int rc = sqlite3_open(path.data(), &db);
        if (rc)
        {
            // Handle error
            sqlite3_close(db);
            throw db_exception{db_handle, "Can't open database"};
        }
        // Store the handle
        db_handle = db;
    }

    database::~database()
    {
        if (db_handle)
        {
            sqlite3_close(db_handle);
        }
    }

    db_collection database::collection(std::string_view name)
    {
        // search table
        std::string check_table_query = "SELECT name FROM sqlite_master WHERE type='table' AND name=?;";
        sqlite3_stmt *stmt{};
        int rc = sqlite3_prepare_v2(db_handle, check_table_query.c_str(), -1, &stmt, nullptr);

        // create a statement scope that finalizes the statement when it goes out of scope
        {
            details::sqlite::statement_scope scope(stmt);
            if (rc != SQLITE_OK)
            {
                throw db_exception{db_handle, "Failed to prepare statement"};
            }
            rc = sqlite3_bind_text(stmt, 1, name.data(), -1, SQLITE_TRANSIENT);

            if (rc != SQLITE_OK)
            {
                throw db_exception{db_handle, "Failed to bind text"};
            }

            rc = sqlite3_step(stmt);
            bool table_exists = (rc == SQLITE_ROW);

            // if table exists, exit
            if (table_exists)
            {
                return db_collection{name, db_handle};
            }
        }

        std::string create_table_query = "CREATE TABLE [" + std::string(name) + "] (body TEXT, docid TEXT GENERATED ALWAYS AS (json_extract(body, '$.docid')) VIRTUAL NOT NULL);";
        sqlite3_stmt *create_stmt{};
        rc = sqlite3_prepare_v2(db_handle, create_table_query.c_str(), -1, &create_stmt, nullptr);

        details::sqlite::statement_scope create_scope(create_stmt);
        if (rc != SQLITE_OK)
        {
            throw db_exception{db_handle, "Failed to prepare statement"};
        }

        rc = sqlite3_step(create_stmt);
        if (rc != SQLITE_DONE)
        {
            throw db_exception{db_handle, "Failed to create table"};
        }

        return db_collection{name, db_handle};
    }

    // COLLECTION
    db_collection::db_collection(std::string_view name, sqlite3 *db_handle) : db_handle(db_handle), table_name(name) {}

    db_document db_collection::doc(std::string_view doc_id)
    {
        std::string get_doc_query = "SELECT body FROM [" + table_name + "] WHERE docid=?;";
        sqlite3_stmt *stmt{};
        int rc = sqlite3_prepare_v2(db_handle, get_doc_query.c_str(), -1, &stmt, nullptr);

        details::sqlite::statement_scope scope{stmt};

        if (rc != SQLITE_OK)
        {
            throw db_exception{db_handle, "Failed to prepare statement"};
        }

        rc = sqlite3_bind_text(stmt, 1, doc_id.data(), -1, SQLITE_TRANSIENT);
        if (rc != SQLITE_OK)
        {
            throw db_exception{db_handle, "Failed to bind text"};
        }

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_ROW)
        {
            throw db_exception{db_handle, "Document not found"};
        }

        db_document doc{table_name, doc_id, reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)), db_handle};
        return doc;
    }

    db_document db_collection::doc()
    {
        auto new_doc_id = details::uuid::generate_uuid_v4();
        auto new_doc_body = "{\"docid\":\"" + new_doc_id + "\"}";

        std::string insert_doc_query = "INSERT INTO [" + table_name + "] (body) VALUES (?);";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(db_handle, insert_doc_query.c_str(), -1, &stmt, nullptr);

        details::sqlite::statement_scope scope{stmt};

        if (rc != SQLITE_OK)
        {
            throw db_exception{db_handle, "Failed to prepare statement"};
        }

        rc = sqlite3_bind_text(stmt, 1, new_doc_body.c_str(), -1, SQLITE_TRANSIENT);
        if (rc != SQLITE_OK)
        {
            throw db_exception{db_handle, "Failed to bind body text"};
        }

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE)
        {
            throw db_exception{db_handle, "Failed to insert document"};
        }

        return db_document{table_name, new_doc_id, new_doc_body, db_handle};
    }

    db_document::db_document(std::string_view table, std::string_view doc_id, std::string_view body, sqlite3 *db_handle) : table_name(table), doc_id(doc_id), body_data(body), db_handle(db_handle) {}

    std::string db_document::id() const
    {
        return doc_id;
    }

    std::string db_document::body() const
    {
        return body_data;
    }

    db_document &db_document::body(std::string_view body)
    {
        std::string update_doc_query = "UPDATE [" + table_name + "] SET body=json_set(?1, '$.docid', ?2) WHERE docid=?2;";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(db_handle, update_doc_query.c_str(), -1, &stmt, nullptr);

        details::sqlite::statement_scope scope{stmt};

        if (rc != SQLITE_OK)
        {
            throw db_exception{db_handle, "Failed to prepare statement"};
        }

        rc = sqlite3_bind_text(stmt, 1, body.data(), -1, SQLITE_TRANSIENT);
        if (rc != SQLITE_OK)
        {
            throw db_exception{db_handle, "Failed to bind body text"};
        }

        rc = sqlite3_bind_text(stmt, 2, doc_id.data(), -1, SQLITE_TRANSIENT);
        if (rc != SQLITE_OK)
        {
            throw db_exception{db_handle, "Failed to bind document id"};
        }

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE)
        {
            throw db_exception{db_handle, "Failed to update document"};
        }

        body_data = body;
        return *this;
    }

    template <typename T>
    void db_document_json_patch_impl(sqlite3 *db_handle, std::string_view table_name, std::string_view doc_id, T value)
    {
        std::string update_doc_query = std::string("UPDATE [") + std::string(table_name) + std::string("] SET body=json_patch(body, ?1), WHERE docid=?2;");
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(db_handle, update_doc_query.c_str(), -1, &stmt, nullptr);

        details::sqlite::statement_scope scope{stmt};

        if (rc != SQLITE_OK)
        {
            throw db_exception{db_handle, "Failed to prepare statement"};
        }

        rc = details::sqlite::bind(stmt, 1, value);
        if (rc != SQLITE_OK)
        {
            throw db_exception{db_handle, "Failed to bind replacement value"};
        }

        rc = details::sqlite::bind(stmt, 2, doc_id);
        if (rc != SQLITE_OK)
        {
            throw db_exception{db_handle, "Failed to bind document id"};
        }

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE)
        {
            throw db_exception{db_handle, "Failed to update document"};
        }
    }

    template <typename T>
    void db_document_json_ins_set_repl_impl(sqlite3 *db_handle, std::string_view table_name, std::string_view func, std::string_view query, std::string_view doc_id, T value)
    {
        std::string update_doc_query = "UPDATE ["s + std::string(table_name) + "] SET body="s + std::string(func) + "(body, ?1, ?2), WHERE docid=?3;"s;
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(db_handle, update_doc_query.c_str(), -1, &stmt, nullptr);

        details::sqlite::statement_scope scope{stmt};

        if (rc != SQLITE_OK)
        {
            throw db_exception{db_handle, "Failed to prepare statement"};
        }

        rc = details::sqlite::bind(stmt, 1, query);
        if (rc != SQLITE_OK)
        {
            throw db_exception{db_handle, "Failed to bind query text"};
        }

        rc = details::sqlite::bind(stmt, 2, value);
        if (rc != SQLITE_OK)
        {
            throw db_exception{db_handle, "Failed to bind replacement value"};
        }

        rc = details::sqlite::bind(stmt, 3, doc_id);
        if (rc != SQLITE_OK)
        {
            throw db_exception{db_handle, "Failed to bind document id"};
        }

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE)
        {
            throw db_exception{db_handle, "Failed to update document"};
        }
    }

    template <typename T>
    void db_document_replace_impl(sqlite3 *db_handle, std::string_view table_name, std::string_view query, std::string_view doc_id, T value)
    {
        db_document_json_ins_set_repl_impl(db_handle, table_name, "json_replace", query, doc_id, value);
    }
    template <typename T>
    void db_document_set_impl(sqlite3 *db_handle, std::string_view table_name, std::string_view query, std::string_view doc_id, T value)
    {
        db_document_json_ins_set_repl_impl(db_handle, table_name, "json_set", query, doc_id, value);
    }
    template <typename T>
    void db_document_insert_impl(sqlite3 *db_handle, std::string_view table_name, std::string_view query, std::string_view doc_id, T value)
    {
        db_document_json_ins_set_repl_impl(db_handle, table_name, "json_insert", query, doc_id, value);
    }

    // replace

    db_document &db_document::replace(std::string_view query, float value)
    {
        db_document_replace_impl(db_handle, table_name, query, doc_id, value);
        return *this;
    }
    db_document &db_document::replace(std::string_view query, double value)
    {
        db_document_replace_impl(db_handle, table_name, query, doc_id, value);
        return *this;
    }
    db_document &db_document::replace(std::string_view query, std::int32_t value)
    {
        db_document_replace_impl(db_handle, table_name, query, doc_id, value);
        return *this;
    }
    db_document &db_document::replace(std::string_view query, std::int64_t value)
    {
        db_document_replace_impl(db_handle, table_name, query, doc_id, value);
        return *this;
    }
    db_document &db_document::replace(std::string_view query, std::nullptr_t value)
    {
        db_document_replace_impl(db_handle, table_name, query, doc_id, value);
        return *this;
    }
    db_document &db_document::replace(std::string_view query, std::string const &value)
    {
        db_document_replace_impl(db_handle, table_name, query, doc_id, value);
        return *this;
    }
    db_document &db_document::replace(std::string_view query, std::string_view value)
    {
        db_document_replace_impl(db_handle, table_name, query, doc_id, value);
        return *this;
    }
    // set
    db_document &db_document::set(std::string_view query, float value)
    {
        db_document_set_impl(db_handle, table_name, query, doc_id, value);
        return *this;
    }
    db_document &db_document::set(std::string_view query, double value)
    {
        db_document_set_impl(db_handle, table_name, query, doc_id, value);
        return *this;
    }
    db_document &db_document::set(std::string_view query, std::int32_t value)
    {
        db_document_set_impl(db_handle, table_name, query, doc_id, value);
        return *this;
    }
    db_document &db_document::set(std::string_view query, std::int64_t value)
    {
        db_document_set_impl(db_handle, table_name, query, doc_id, value);
        return *this;
    }
    db_document &db_document::set(std::string_view query, std::nullptr_t value)
    {
        db_document_set_impl(db_handle, table_name, query, doc_id, value);
        return *this;
    }
    db_document &db_document::set(std::string_view query, std::string const &value)
    {
        db_document_set_impl(db_handle, table_name, query, doc_id, value);
        return *this;
    }
    db_document &db_document::set(std::string_view query, std::string_view value)
    {
        db_document_set_impl(db_handle, table_name, query, doc_id, value);
        return *this;
    }
    // insert
    db_document &db_document::insert(std::string_view query, float value)
    {
        db_document_insert_impl(db_handle, table_name, query, doc_id, value);
        return *this;
    }
    db_document &db_document::insert(std::string_view query, double value)
    {
        db_document_insert_impl(db_handle, table_name, query, doc_id, value);
        return *this;
    }
    db_document &db_document::insert(std::string_view query, std::int32_t value)
    {
        db_document_insert_impl(db_handle, table_name, query, doc_id, value);
        return *this;
    }
    db_document &db_document::insert(std::string_view query, std::int64_t value)
    {
        db_document_insert_impl(db_handle, table_name, query, doc_id, value);
        return *this;
    }
    db_document &db_document::insert(std::string_view query, std::nullptr_t value)
    {
        db_document_insert_impl(db_handle, table_name, query, doc_id, value);
        return *this;
    }
    db_document &db_document::insert(std::string_view query, std::string const &value)
    {
        db_document_insert_impl(db_handle, table_name, query, doc_id, value);
        return *this;
    }
    db_document &db_document::insert(std::string_view query, std::string_view value)
    {
        db_document_insert_impl(db_handle, table_name, query, doc_id, value);
        return *this;
    }

    // patch
    db_document &db_document::patch(std::string const &json)
    {
        db_document_json_patch_impl(db_handle, table_name, doc_id, json);
        return *this;
    }
    db_document &db_document::patch(std::string_view json)
    {
        db_document_json_patch_impl(db_handle, table_name, doc_id, json);
        return *this;
    }

    // document ref
    db_document_ref::db_document_ref(std::string_view table_name, std::string_view doc_id, sqlite3 *db_handle) : table_name(table_name), doc_id(doc_id), db_handle(db_handle) {}

    std::string db_document_ref::id() const
    {
        return doc_id;
    }

    db_document db_document_ref::doc() const
    {
        return db_collection{table_name, db_handle}.doc(doc_id);
    }

    // where
    std::vector<db_document_ref> where_impl(sqlite3 *db_handle, std::string_view table_name, std::string_view query, std::string_view op, std::string_view criteria)
    {
        std::string get_doc_query = "SELECT docid FROM ["s + std::string(table_name) + "] WHERE json_extract(body, ?1) " + std::string(op) + " ?2;"s;
        sqlite3_stmt *stmt{};
        int rc = sqlite3_prepare_v2(db_handle, get_doc_query.c_str(), -1, &stmt, nullptr);

        details::sqlite::statement_scope scope{stmt};

        if (rc != SQLITE_OK)
        {
            throw db_exception{db_handle, "Failed to prepare statement"};
        }

        rc = sqlite3_bind_text(stmt, 1, query.data(), -1, SQLITE_TRANSIENT);
        if (rc != SQLITE_OK)
        {
            throw db_exception{db_handle, "Failed to bind query text"};
        }

        rc = sqlite3_bind_text(stmt, 2, criteria.data(), -1, SQLITE_TRANSIENT);
        if (rc != SQLITE_OK)
        {
            throw db_exception{db_handle, "Failed to bind criteria text"};
        }

        std::vector<db_document_ref> refs;
        do
        {
            rc = sqlite3_step(stmt);
            if (rc == SQLITE_ERROR)
            {
                throw db_exception{db_handle, "Failed to enumerate documents"};
            }
            if (rc != SQLITE_ROW)
            {
                break;
            }
            refs.push_back(db_document_ref{table_name, reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)), db_handle});
        } while (true);

        return refs;
    }
    std::vector<db_document_ref> db_collection::where(std::string_view query, ops::like value)
    {
        return where_impl(db_handle, table_name, query, "LIKE", value.value);
    }
    std::vector<db_document_ref> db_collection::where(std::string_view query, ops::eq value)
    {
        return where_impl(db_handle, table_name, query, "=", value.value);
    }
    std::vector<db_document_ref> db_collection::where(std::string_view query, ops::neq value)
    {
        return where_impl(db_handle, table_name, query, "!=", value.value);
    }
}