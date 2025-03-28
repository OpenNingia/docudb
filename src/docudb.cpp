#include "docudb.hpp"
#include <stdexcept>
#include <random>
#include <string>
#include <string_view>
#include <format>
#include <thread>
#include <algorithm>

#include "version.h"

using namespace std::string_literals;
using namespace std::string_view_literals;

namespace docudb
{
    std::string get_version() noexcept
    {
        return DOCUDB_VERSION;
    }

    std::string get_build_timestamp() noexcept
    {
        return DOCUDB_VERSION_DATE;
    }

    namespace details
    {
        namespace sqlite
        {
            struct statement
            {
                statement(sqlite3 *db_handle, std::string_view query) : db_handle_(db_handle)
                {
                    rc = sqlite3_prepare_v2(db_handle, query.data(), -1, &stmt_, nullptr);
                    if (rc != SQLITE_OK)
                    {
                        throw db_exception{db_handle_, "Failed to prepare statement"};
                    }
                }

                ~statement()
                {
                    sqlite3_finalize(stmt_);
                }

                sqlite3_stmt *data() const noexcept { return stmt_; }

                statement &bind(int index, std::int16_t value)
                {
                    rc = sqlite3_bind_int(stmt_, index, value);
                    if (rc != SQLITE_OK)
                    {
                        throw db_exception{db_handle_, "Failed to bind value"};
                    }
                    return *this;
                }

                statement &bind(int index, std::int32_t value)
                {
                    rc = sqlite3_bind_int(stmt_, index, value);
                    if (rc != SQLITE_OK)
                    {
                        throw db_exception{db_handle_, "Failed to bind value"};
                    }
                    return *this;
                }

                statement &bind(int index, std::int64_t value)
                {
                    rc = sqlite3_bind_int64(stmt_, index, value);
                    if (rc != SQLITE_OK)
                    {
                        throw db_exception{db_handle_, "Failed to bind value"};
                    }
                    return *this;
                }

                statement &bind(int index, std::nullptr_t value)
                {
                    rc = sqlite3_bind_null(stmt_, index);
                    if (rc != SQLITE_OK)
                    {
                        throw db_exception{db_handle_, "Failed to bind value"};
                    }
                    return *this;
                }

                statement &bind(int index, std::string_view value)
                {
                    rc = sqlite3_bind_text(stmt_, index, value.data(), -1, SQLITE_TRANSIENT);
                    if (rc != SQLITE_OK)
                    {
                        throw db_exception{db_handle_, "Failed to bind value"};
                    }
                    return *this;
                }

                statement &bind(int index, std::double_t value)
                {
                    rc = sqlite3_bind_double(stmt_, index, value);
                    if (rc != SQLITE_OK)
                    {
                        throw db_exception{db_handle_, "Failed to bind value"};
                    }
                    return *this;
                }

                statement &bind(int index, std::float_t value)
                {
                    rc = sqlite3_bind_double(stmt_, index, value);
                    if (rc != SQLITE_OK)
                    {
                        throw db_exception{db_handle_, "Failed to bind value"};
                    }
                    return *this;
                }

                // step
                statement &step() noexcept
                {
                    rc = sqlite3_step(stmt_);
                    return *this;
                }

                // read value
                template <typename T>
                T get(int index) const noexcept
                {
                    static_assert(std::false_type::value, "Unsupported type for get method");
                }

                int result_code() const noexcept
                {
                    return rc;
                }

            private:
                sqlite3 *db_handle_;
                sqlite3_stmt *stmt_;
                int rc;
            };

            template <>
            std::string_view statement::get(int index) const noexcept
            {
                return reinterpret_cast<const char *>(sqlite3_column_text(stmt_, index));
            }

            template <>
            std::string statement::get(int index) const noexcept
            {
                return reinterpret_cast<const char *>(sqlite3_column_text(stmt_, index));
            }

            template <>
            std::double_t statement::get(int index) const noexcept
            {
                return sqlite3_column_double(stmt_, index);
            }

            template <>
            std::int64_t statement::get(int index) const noexcept
            {
                return sqlite3_column_int64(stmt_, index);
            }

            template <>
            std::int32_t statement::get(int index) const noexcept
            {
                return sqlite3_column_int(stmt_, index);
            }
        }

        // inspired by https://stackoverflow.com/questions/24365331/how-can-i-generate-uuid-in-c-without-using-boost-library
        namespace uuid
        {
            thread_local std::random_device rd;
            thread_local std::size_t threadIdHash = std::hash<std::thread::id>{}(std::this_thread::get_id());
            thread_local uint64_t mixedSeed = static_cast<uint64_t>(rd()) ^ static_cast<uint64_t>(threadIdHash);
            thread_local std::mt19937 gen(static_cast<unsigned int>(mixedSeed));
            thread_local std::uniform_int_distribution<> dis(0, 15);
            thread_local std::uniform_int_distribution<> dis2(8, 11);

            std::string generate_uuid_v4()
            {
                const char *hex_chars = "0123456789abcdef";
                char uuid[37]; // 36 characters + null terminator

                for (int i = 0; i < 8; ++i)
                {
                    uuid[i] = hex_chars[dis(gen)];
                }
                uuid[8] = '-';
                for (int i = 9; i < 13; ++i)
                {
                    uuid[i] = hex_chars[dis(gen)];
                }
                uuid[13] = '-';
                uuid[14] = '4'; // UUID version 4
                for (int i = 15; i < 18; ++i)
                {
                    uuid[i] = hex_chars[dis(gen)];
                }
                uuid[18] = '-';
                uuid[19] = hex_chars[dis2(gen)];
                for (int i = 20; i < 23; ++i)
                {
                    uuid[i] = hex_chars[dis(gen)];
                }
                uuid[23] = '-';
                for (int i = 24; i < 36; ++i)
                {
                    uuid[i] = hex_chars[dis(gen)];
                }
                uuid[36] = '\0';

                return std::string(uuid);
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

    db_collection database::collection(std::string_view name) const
    {
        // create a statement scope that finalizes the statement when it goes out of scope
        {
            auto check_table_query = "SELECT name FROM sqlite_master WHERE type='table' AND name=?;"sv;
            details::sqlite::statement stmt{db_handle, check_table_query};

            stmt
                .bind(1, name)
                .step();

            bool table_exists = (stmt.result_code() == SQLITE_ROW);

            // if table exists, exit
            if (table_exists)
            {
                return db_collection{name, db_handle};
            }
        }

        {
            auto create_table_query = std::format("CREATE TABLE [{}] (body TEXT, docid TEXT GENERATED ALWAYS AS (json_extract(body, '$.docid')) VIRTUAL NOT NULL UNIQUE);", name);
            details::sqlite::statement stmt{db_handle, create_table_query};

            if (stmt.step().result_code() != SQLITE_DONE)
            {
                throw db_exception{db_handle, "Failed to create table"};
            }
        }

        // create index on docid
        {
            auto create_index = std::format("CREATE UNIQUE INDEX Idx_{0}_docid on [{0}](docid);", name);
            details::sqlite::statement stmt{db_handle, create_index};

            stmt.step();

            if (stmt.result_code() != SQLITE_DONE)
            {
                throw db_exception{db_handle, "Failed to create index"};
            }
        }

        return db_collection{name, db_handle};
    }

    std::vector<db_collection> database::collections() const
    {
        {
            auto check_table_query = "SELECT name FROM sqlite_master WHERE type='table';"sv;
            details::sqlite::statement stmt{db_handle, check_table_query};

            std::vector<db_collection> collections;
            do
            {
                stmt.step();
                if (stmt.result_code() == SQLITE_ERROR)
                {
                    throw db_exception{db_handle, "Failed to enumerate collections"};
                }
                else if (stmt.result_code() != SQLITE_ROW)
                {
                    break;
                }
                else
                {
                    collections.push_back(db_collection{stmt.get<std::string>(0), db_handle});
                }
            } while (true);
    
            return collections;
        }
    }

    std::string read_doc_body(sqlite3 *db_handle, std::string_view table_name, std::string_view doc_id)
    {
        auto get_doc_query = std::format("SELECT body FROM [{}] WHERE docid=?;", table_name);
        details::sqlite::statement stmt(db_handle, get_doc_query);

        stmt
            .bind(1, doc_id)
            .step();

        if (stmt.result_code() != SQLITE_ROW)
        {
            throw db_exception{db_handle, "Document not found"};
        }

        return stmt.get<std::string>(0);
    }

    // COLLECTION
    db_collection::db_collection(std::string_view name, sqlite3 *db_handle) : db_handle(db_handle), table_name(name) {}

    std::string db_collection::name() const noexcept
    {
        return table_name;
    }

    db_document db_collection::doc(std::string_view doc_id) const
    {
        return db_document{table_name, doc_id, read_doc_body(db_handle, table_name, doc_id), db_handle};
    }

    db_document db_collection::doc()
    {
        auto new_doc_id = details::uuid::generate_uuid_v4();
        auto new_doc_body = std::format(R"({{"docid":"{}"}})", new_doc_id);

        auto insert_doc_query = std::format("INSERT INTO [{}] (body) VALUES (?);", table_name);
        details::sqlite::statement stmt{db_handle, insert_doc_query};

        stmt
            .bind(1, new_doc_body)
            .step();

        if (stmt.result_code() != SQLITE_DONE)
        {
            throw db_exception{db_handle, "Failed to insert document"};
        }

        return db_document{table_name, new_doc_id, new_doc_body, db_handle};
    }

    // get all documents
    std::vector<db_document_ref> db_collection::docs()
    {
        std::string get_doc_query = std::format("SELECT docid FROM [{}];", table_name);
        details::sqlite::statement stmt{db_handle, get_doc_query};

        std::vector<db_document_ref> refs;
        do
        {
            stmt.step();
            if (stmt.result_code() == SQLITE_ERROR)
            {
                throw db_exception{db_handle, "Failed to enumerate documents"};
            }
            else if (stmt.result_code() != SQLITE_ROW)
            {
                break;
            }
            else
            {
                refs.push_back(db_document_ref{table_name, stmt.get<std::string>(0), db_handle});
            }
        } while (true);

        return refs;
    }    

    // where
    std::vector<db_document_ref> where_impl(sqlite3 *db_handle, std::string_view table_name, std::string_view query, std::string_view op, std::string_view criteria)
    {
        auto json_query = query.size() > 0 && query[0] == '$';
        std::string get_doc_query;
        if (json_query)
            get_doc_query = std::format("SELECT docid FROM [{}] WHERE json_extract(body, ?1) {} ?2;", table_name, op);
        else
            get_doc_query = std::format("SELECT docid FROM [{}] WHERE [{}] {} ?1;", table_name, query, op);

        details::sqlite::statement stmt{db_handle, get_doc_query};

        if (json_query)
        {
            stmt
                .bind(1, query)
                .bind(2, criteria);
        }
        else
        {
            stmt
                .bind(1, criteria);
        }

        std::vector<db_document_ref> refs;
        do
        {
            stmt.step();
            if (stmt.result_code() == SQLITE_ERROR)
            {
                throw db_exception{db_handle, "Failed to enumerate documents"};
            }
            else if (stmt.result_code() != SQLITE_ROW)
            {
                break;
            }
            else
            {
                refs.push_back(db_document_ref{table_name, stmt.get<std::string>(0), db_handle});
            }
        } while (true);

        return refs;
    }

    std::vector<db_document_ref> db_collection::where(std::string_view query, ops::like value) const
    {
        return where_impl(db_handle, table_name, query, "LIKE", value.value);
    }

    std::vector<db_document_ref> db_collection::where(std::string_view query, ops::eq value) const
    {
        return where_impl(db_handle, table_name, query, "=", value.value);
    }

    std::vector<db_document_ref> db_collection::where(std::string_view query, ops::neq value) const
    {
        return where_impl(db_handle, table_name, query, "!=", value.value);
    }

    db_collection &db_collection::index(std::string_view column_name, std::string_view query, bool unique)
    {
        {
            auto alter_table = std::format("ALTER TABLE [{}] ADD COLUMN [{}] GENERATED ALWAYS AS (json_extract(body, '{}')) VIRTUAL;", table_name, column_name, query);
            details::sqlite::statement stmt{db_handle, alter_table};

            stmt.step();

            if (stmt.result_code() != SQLITE_DONE)
            {
                throw db_exception{db_handle, "Failed to alter table"};
            }
        }

        {
            auto create_index = std::format("CREATE {0} INDEX Idx_{1}_{2} on [{1}]({2});", unique ? "UNIQUE" : "", table_name, column_name);
            details::sqlite::statement stmt{db_handle, create_index};

            stmt.step();

            if (stmt.result_code() != SQLITE_DONE)
            {
                throw db_exception{db_handle, "Failed to create index"};
            }
        }

        return *this;
    }

    // DOCUMENT

    db_document::db_document(std::string_view table, std::string_view doc_id, std::string_view body, sqlite3 *db_handle) : table_name(table), doc_id(doc_id), body_data(body), invalid_body(false), db_handle(db_handle) {}

    std::string db_document::id() const
    {
        return doc_id;
    }

    std::string db_document::body() const
    {
        if (invalid_body)
            body_data = read_doc_body(db_handle, table_name, doc_id);
        return body_data;
    }

    db_document &db_document::body(std::string_view body)
    {
        auto update_doc_query = std::format("UPDATE [{}] SET body=json_set(?1, '$.docid', ?2) WHERE docid=?2;", table_name);
        details::sqlite::statement stmt{db_handle, update_doc_query};

        stmt
            .bind(1, body)
            .bind(2, doc_id)
            .step();

        if (stmt.result_code() != SQLITE_DONE)
        {
            throw db_exception{db_handle, "Failed to update document"};
        }

        body_data = body;
        invalid_body = false;
        return *this;
    }

    template <typename T>
    void db_document_json_patch_impl(sqlite3 *db_handle, std::string_view table_name, std::string_view doc_id, T value)
    {
        auto update_doc_query = std::format("UPDATE [{}] SET body=json_patch(body, ?1) WHERE docid=?2;", table_name);
        details::sqlite::statement stmt{db_handle, update_doc_query};

        stmt
            .bind(1, value)
            .bind(2, doc_id)
            .step();

        if (stmt.result_code() != SQLITE_DONE)
        {
            throw db_exception{db_handle, "Failed to update document"};
        }
    }

    template <typename T>
    void db_document_json_ins_set_repl_impl(sqlite3 *db_handle, std::string_view table_name, std::string_view func, std::string_view query, std::string_view doc_id, T value)
    {
        auto update_doc_query = std::format("UPDATE [{}] SET body={}(body, ?1, ?2) WHERE docid=?3;", table_name, func);
        details::sqlite::statement stmt{db_handle, update_doc_query};

        stmt
            .bind(1, query)
            .bind(2, value)
            .bind(3, doc_id)
            .step();

        if (stmt.result_code() != SQLITE_DONE)
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

    db_document &db_document::replace(std::string_view query, std::float_t value)
    {
        db_document_replace_impl(db_handle, table_name, query, doc_id, value);
        invalid_body = true;
        return *this;
    }
    db_document &db_document::replace(std::string_view query, std::double_t value)
    {
        db_document_replace_impl(db_handle, table_name, query, doc_id, value);
        invalid_body = true;
        return *this;
    }
    db_document &db_document::replace(std::string_view query, std::int32_t value)
    {
        db_document_replace_impl(db_handle, table_name, query, doc_id, value);
        invalid_body = true;
        return *this;
    }
    db_document &db_document::replace(std::string_view query, std::int64_t value)
    {
        db_document_replace_impl(db_handle, table_name, query, doc_id, value);
        invalid_body = true;
        return *this;
    }
    db_document &db_document::replace(std::string_view query, std::nullptr_t value)
    {
        db_document_replace_impl(db_handle, table_name, query, doc_id, value);
        invalid_body = true;
        return *this;
    }
    db_document &db_document::replace(std::string_view query, std::string const &value)
    {
        db_document_replace_impl(db_handle, table_name, query, doc_id, value);
        invalid_body = true;
        return *this;
    }
    db_document &db_document::replace(std::string_view query, std::string_view value)
    {
        db_document_replace_impl(db_handle, table_name, query, doc_id, value);
        invalid_body = true;
        return *this;
    }
    // set
    db_document &db_document::set(std::string_view query, std::float_t value)
    {
        db_document_set_impl(db_handle, table_name, query, doc_id, value);
        invalid_body = true;
        return *this;
    }
    db_document &db_document::set(std::string_view query, std::double_t value)
    {
        db_document_set_impl(db_handle, table_name, query, doc_id, value);
        invalid_body = true;
        return *this;
    }
    db_document &db_document::set(std::string_view query, std::int32_t value)
    {
        db_document_set_impl(db_handle, table_name, query, doc_id, value);
        invalid_body = true;
        return *this;
    }
    db_document &db_document::set(std::string_view query, std::int64_t value)
    {
        db_document_set_impl(db_handle, table_name, query, doc_id, value);
        invalid_body = true;
        return *this;
    }
    db_document &db_document::set(std::string_view query, std::nullptr_t value)
    {
        db_document_set_impl(db_handle, table_name, query, doc_id, value);
        invalid_body = true;
        return *this;
    }
    db_document &db_document::set(std::string_view query, std::string const &value)
    {
        db_document_set_impl(db_handle, table_name, query, doc_id, value);
        invalid_body = true;
        return *this;
    }
    db_document &db_document::set(std::string_view query, std::string_view value)
    {
        db_document_set_impl(db_handle, table_name, query, doc_id, value);
        invalid_body = true;
        return *this;
    }
    // insert
    db_document &db_document::insert(std::string_view query, std::float_t value)
    {
        db_document_insert_impl(db_handle, table_name, query, doc_id, value);
        invalid_body = true;
        return *this;
    }
    db_document &db_document::insert(std::string_view query, std::double_t value)
    {
        db_document_insert_impl(db_handle, table_name, query, doc_id, value);
        invalid_body = true;
        return *this;
    }
    db_document &db_document::insert(std::string_view query, std::int32_t value)
    {
        db_document_insert_impl(db_handle, table_name, query, doc_id, value);
        invalid_body = true;
        return *this;
    }
    db_document &db_document::insert(std::string_view query, std::int64_t value)
    {
        db_document_insert_impl(db_handle, table_name, query, doc_id, value);
        invalid_body = true;
        return *this;
    }
    db_document &db_document::insert(std::string_view query, std::nullptr_t value)
    {
        db_document_insert_impl(db_handle, table_name, query, doc_id, value);
        invalid_body = true;
        return *this;
    }
    db_document &db_document::insert(std::string_view query, std::string const &value)
    {
        db_document_insert_impl(db_handle, table_name, query, doc_id, value);
        invalid_body = true;
        return *this;
    }
    db_document &db_document::insert(std::string_view query, std::string_view value)
    {
        db_document_insert_impl(db_handle, table_name, query, doc_id, value);
        invalid_body = true;
        return *this;
    }

    // patch
    db_document &db_document::patch(std::string_view json)
    {
        db_document_json_patch_impl(db_handle, table_name, doc_id, json);
        invalid_body = true;
        return *this;
    }

    template <class R>
    R get_value_impl(sqlite3 *db_handle, std::string_view table_name, std::string const &doc_id, std::string_view query)
    {
        auto get_doc_query = std::format("SELECT json_extract(body, ?1) FROM [{}] WHERE docid=?2;", table_name);
        details::sqlite::statement stmt{db_handle, get_doc_query};

        stmt
            .bind(1, query)
            .bind(2, doc_id)
            .step();

        if (stmt.result_code() != SQLITE_ROW)
        {
            throw db_exception{db_handle, "Document not found"};
        }

        return stmt.get<R>(0);
    }

    // get string
    std::string db_document::get_string(std::string_view query) const
    {
        return get_value_impl<std::string>(db_handle, table_name, doc_id, query);
    }

    // get number
    std::int64_t db_document::get_number(std::string_view query) const
    {
        return get_value_impl<std::int64_t>(db_handle, table_name, doc_id, query);
    }

    // get real
    std::double_t db_document::get_real(std::string_view query) const
    {
        return get_value_impl<std::double_t>(db_handle, table_name, doc_id, query);
    }

    // DOCUMENT_REF

    db_document_ref::db_document_ref(db_document const& doc) : table_name(doc.table_name), doc_id(doc.doc_id), db_handle(doc.db_handle) {}
    db_document_ref::db_document_ref(std::string_view table_name, std::string_view doc_id, sqlite3 *db_handle) : table_name(table_name), doc_id(doc_id), db_handle(db_handle) {}

    std::string db_document_ref::id() const
    {
        return doc_id;
    }

    db_document db_document_ref::doc() const
    {
        return db_collection{table_name, db_handle}.doc(doc_id);
    }
}
