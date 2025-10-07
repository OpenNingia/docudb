#include "docudb.hpp"
#include <stdexcept>
#include <random>
#include <string>
#include <string_view>
#include <format>
#include <thread>
#include <algorithm>
#include <utility>
#include <sstream>
#include "sqlite_extensions.h"
#include "docudb_version.h"

using namespace std::string_literals;
using namespace std::string_view_literals;

namespace docudb
{
    namespace query
    {
        thread_local int bind_counter = 0;
    }

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
            statement::statement(sqlite3 *db_handle, std::string_view query) : db_handle_(db_handle)
            {
                rc = sqlite3_prepare_v2(db_handle, query.data(), -1, &stmt_, nullptr);
                if (rc != SQLITE_OK)
                {
                    throw stmt_exception{db_handle_, query};
                }
            }

            statement::~statement()
            {
                sqlite3_finalize(stmt_);
            }

            sqlite3_stmt *statement::data() const noexcept { return stmt_; }

            statement &statement::bind(int index, std::int16_t value)
            {
                rc = sqlite3_bind_int(stmt_, index, value);
                if (rc != SQLITE_OK)
                {
                    throw db_exception{db_handle_, "Failed to bind value"};
                }
                return *this;
            }

            statement &statement::bind(int index, std::int32_t value)
            {
                rc = sqlite3_bind_int(stmt_, index, value);
                if (rc != SQLITE_OK)
                {
                    throw db_exception{db_handle_, "Failed to bind value"};
                }
                return *this;
            }

            statement &statement::bind(int index, std::int64_t value)
            {
                rc = sqlite3_bind_int64(stmt_, index, value);
                if (rc != SQLITE_OK)
                {
                    throw db_exception{db_handle_, "Failed to bind value"};
                }
                return *this;
            }

            statement &statement::bind(int index, std::nullptr_t value)
            {
                rc = sqlite3_bind_null(stmt_, index);
                if (rc != SQLITE_OK)
                {
                    throw db_exception{db_handle_, "Failed to bind value"};
                }
                return *this;
            }

            statement &statement::bind(int index, std::string_view value)
            {
                rc = sqlite3_bind_text(stmt_, index, value.data(), -1, SQLITE_TRANSIENT);
                if (rc != SQLITE_OK)
                {
                    throw db_exception{db_handle_, "Failed to bind value"};
                }
                return *this;
            }

            statement &statement::bind(int index, std::double_t value)
            {
                rc = sqlite3_bind_double(stmt_, index, value);
                if (rc != SQLITE_OK)
                {
                    throw db_exception{db_handle_, "Failed to bind value"};
                }
                return *this;
            }

            statement &statement::bind(int index, std::float_t value)
            {
                rc = sqlite3_bind_double(stmt_, index, value);
                if (rc != SQLITE_OK)
                {
                    throw db_exception{db_handle_, "Failed to bind value"};
                }
                return *this;
            }

            // step
            statement &statement::step() noexcept
            {
                rc = sqlite3_step(stmt_);
                return *this;
            }

            template <>
            std::string statement::get(int index) const
            {
                auto s = sqlite3_column_text(stmt_, index);
                // for now throw an exception if null
                if (!s)
                    throw db_exception{db_handle_, "Field is null"};
                return reinterpret_cast<const char *>(s);
            }

            template <>
            std::double_t statement::get(int index) const
            {
                return sqlite3_column_double(stmt_, index);
            }

            template <>
            std::int64_t statement::get(int index) const
            {
                return sqlite3_column_int64(stmt_, index);
            }

            template <>
            std::int32_t statement::get(int index) const
            {
                return sqlite3_column_int(stmt_, index);
            }

            template <>
            std::int16_t statement::get(int index) const
            {
                return sqlite3_column_int(stmt_, index);
            }  
            
            template <>
            std::uint64_t statement::get(int index) const
            {
                return sqlite3_column_int64(stmt_, index);
            }

            template <>
            std::uint32_t statement::get(int index) const
            {
                return sqlite3_column_int64(stmt_, index);
            }

            template <>
            std::uint16_t statement::get(int index) const
            {
                return sqlite3_column_int(stmt_, index);
            }  

            json_type statement::get_type(int index) const noexcept
            {
                switch(sqlite3_column_type(stmt_, index)) {
                    case SQLITE_INTEGER:
                        return json_type::integer;
                    case SQLITE_FLOAT:
                        return json_type::real;
                    case SQLITE_TEXT:
                        return json_type::string;
                    case SQLITE_BLOB:
                        return json_type::string; // treat blob as string
                    case SQLITE_NULL:
                    default:
                        return json_type::null;
                }
            }

            bool statement::is_result_null(int index) const noexcept
            {
                auto strval = sqlite3_column_text(stmt_, index);
                auto blobval = sqlite3_column_blob(stmt_, index);
                return nullptr == sqlite3_column_blob(stmt_, index);
            }

            struct transaction
            {
                transaction(sqlite3 *db_handle) : db_handle_(db_handle)
                {
                    auto ret = sqlite3_exec(db_handle_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
                    if (ret != SQLITE_OK)
                    {
                        // std::cerr << "sqlite3_exec: " << ret << std::endl;
                        throw db_exception{db_handle_, "Failed to begin transaction"};
                    }
                }

                void commit()
                {
                    auto ret = sqlite3_exec(db_handle_, "COMMIT;", nullptr, nullptr, nullptr);
                    if (ret != SQLITE_OK)
                    {
                        // std::cerr << "sqlite3_exec: " << ret << std::endl;
                        throw db_exception{db_handle_, "Failed to commit transaction"};
                    }
                    db_handle_ = nullptr; // Prevent rollback in destructor
                }

                ~transaction()
                {
                    if (db_handle_)
                        sqlite3_exec(db_handle_, "ROLLBACK;", nullptr, nullptr, nullptr);
                    // ignore errors
                    // cannot throw in destructor
                }

            private:
                sqlite3 *db_handle_;
            };
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
    stmt_exception::stmt_exception(sqlite3 *db_handle, std::string_view sql)
        : db_exception(db_handle, std::format("Failed to prepare statement: `{}", sql)) {}

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

    database::database(std::string_view path, open_mode mode, threading_mode thread_mode)
        : db_handle(nullptr)
    {
        int flags = 0;

        // Set the open mode flags
        switch (mode)
        {
        case open_mode::read_only:
            flags |= SQLITE_OPEN_READONLY;
            break;
        case open_mode::read_write:
            flags |= SQLITE_OPEN_READWRITE;
            break;
        case open_mode::read_write_create:
            flags |= SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
            break;
        }

        // Set the threading mode flags
        switch (thread_mode)
        {
        case threading_mode::default_mode:
            // No flags are added; uses the compile-time or process-start default.
            break;
        case threading_mode::multi_thread:
            flags |= SQLITE_OPEN_NOMUTEX;
            break;
        case threading_mode::serialized:
            flags |= SQLITE_OPEN_FULLMUTEX;
            break;
        }

        sqlite3* db;
        // Use path.data() for compatibility with C-style string APIs
        int rc = sqlite3_open_v2(path.data(), &db, flags, nullptr);

        if (rc != SQLITE_OK)
        {
            sqlite3_close(db);
            throw db_exception{db_handle, "Can't open database"};
        }

        // Store the handle
        db_handle = db;
    }    

    database::database(database&& other) : db_handle(std::exchange(other.db_handle, nullptr)) {}
    database& database::operator=(database&& other) {
        if (this != &other)
        {
            db_handle = std::exchange(other.db_handle, nullptr);
        }
        return *this;
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

    void database::load_extensions() const
    {
        sqlite3_create_function_v2(
            db_handle,
            "REGEXP",             // Function name in SQL
            2,                    // Number of arguments
            SQLITE_UTF8,          // Preferred text encoding
            nullptr,              // No application context pointer needed
            ::sqlite_regexp_func, // Pointer to our C++ implementation
            nullptr,              // No step function (not aggregate)
            nullptr,              // No final function (not aggregate)
            nullptr               // No destroy function
        );
    }

    void database::backup_to(database &dest, std::function<void(int, int)> progress) const
    {
        const auto PAGES_PER_STEP = 1000;
        auto bak = sqlite3_backup_init(dest.db_handle, "main", db_handle, "main");
        if (bak)
        {
            auto pagecount = 0;
            auto remaining = 1;  

            while (true)
            {
                auto pages = std::min(PAGES_PER_STEP, remaining);
                sqlite3_backup_step(bak, pages);

                pagecount = sqlite3_backup_pagecount(bak);
                remaining = sqlite3_backup_remaining(bak);

                progress(remaining, pagecount);

                if (remaining <= 0)
                {
                    break;
                }
            }

            sqlite3_backup_finish(bak);
        }

        auto rc = sqlite3_errcode(dest.db_handle);
        if (rc != SQLITE_OK)
            throw db_exception{dest.db_handle, "Backup failed"};
    }

    std::string database::filename_database() const noexcept
    {
        auto f = sqlite3_db_filename(db_handle, "main");
        auto r = sqlite3_filename_database(f);
        if (r)
            return r;
        return {};
    }

    std::string database::filename_journal() const noexcept
    {
        auto f = sqlite3_db_filename(db_handle, "main");
        auto r = sqlite3_filename_journal(f);
        if (r)
            return r;
        return {};
    }

    std::string database::filename_wal() const noexcept
    {
        auto f = sqlite3_db_filename(db_handle, "main");
        auto r = sqlite3_filename_wal(f);
        if (r)
            return r;
        return {};
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
        return db_document{table_name, doc_id, db_handle};
    }

    db_document db_collection::doc()
    {
        auto new_doc_id = details::uuid::generate_uuid_v4();
        return create(new_doc_id);
    }

    db_document db_collection::create(std::string_view doc_id)
    {
        auto new_doc_body = std::format(R"({{"docid":"{}"}})", doc_id);

        auto insert_doc_query = std::format("INSERT INTO [{}] (body) VALUES (?);", table_name);
        details::sqlite::statement stmt{db_handle, insert_doc_query};

        stmt
            .bind(1, new_doc_body)
            .step();

        if (stmt.result_code() != SQLITE_DONE)
        {
            throw db_exception{db_handle, "Failed to insert document"};
        }

        return db_document{table_name, doc_id, new_doc_body, db_handle};
    }

    // get the count
    std::size_t db_collection::count() const
    {
        auto query = std::format("SELECT count(*) FROM [{}];", table_name);
        details::sqlite::statement stmt{db_handle, query};

        stmt.step();

        if (stmt.result_code() != SQLITE_ROW)
        {
            throw db_exception{db_handle, "Failed to count collection"};
        }

        return stmt.get<std::int64_t>(0);
    }

    std::size_t db_collection::count(query::queryable_type_eraser q) const
    {
        auto query_string = std::format("SELECT COUNT(*) FROM [{}] WHERE {}", table_name, q.to_query_string());

        details::sqlite::statement stmt{db_handle, query_string};

        auto binder = q.get_binder();

        for (const auto &[key, value] : binder.get_parameters())
        {
            std::visit([&](auto &&val)
                        { stmt.bind(key, val); }, value);
        }

        stmt.step();

        if (stmt.result_code() != SQLITE_ROW)
        {
            throw db_exception{db_handle, "Failed to count collection"};
        }

        return stmt.get<std::int64_t>(0);
    }

    // get all documents
    std::vector<db_document_ref> db_collection::docs() const
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

    std::vector<db_document_ref> db_collection::find(query::queryable_type_eraser q, std::optional<query::order_by> order_by, std::optional<int> limit) const
    {
        std::vector<std::string> select_fields = {"docid"};
        if (order_by) {
            auto json_query = order_by->field().size() > 0 && order_by->field().at(0) == '$';
            if (json_query) {
                select_fields.push_back(std::format("json_extract(body, '{}') AS __order_by", order_by->field()));
            } else {
                select_fields.push_back(std::format("{} AS __order_by", order_by->field()));
            }
        }

        auto select_fields_str = std::accumulate(
            std::next(select_fields.begin()), select_fields.end(), select_fields[0],
            [](std::string const &a, std::string const &b)
            {
                return a + "," + b;
            });

        auto query_string = std::format("SELECT {} FROM [{}] WHERE {}", select_fields_str, table_name, q.to_query_string());
        if (order_by)
            query_string += std::format(" ORDER BY __order_by {}", order_by->direction());
        if (limit)
            query_string += std::format(" LIMIT {}", *limit);

        details::sqlite::statement stmt{db_handle, query_string};

        auto binder = q.get_binder();

        for (const auto &[key, value] : binder.get_parameters())
        {
            std::visit([&](auto &&val)
                        { stmt.bind(key, val); }, value);
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

    bool column_exists(sqlite3 *db_handle, std::string_view table_name, std::string_view column_name)
    {
        auto doc_query = std::format("SELECT COUNT(*) FROM pragma_table_xinfo('{0}') WHERE name=?1", table_name);
        details::sqlite::statement stmt{db_handle, doc_query};

        stmt.bind(1, column_name)
            .step();

        if (stmt.result_code() != SQLITE_ROW)
        {
            throw db_exception{db_handle, "Failed to count collection"};
        }

        return stmt.get<std::int64_t>(0) > 0;
    }

    db_collection &db_collection::index(std::string_view column_name, std::string_view query, bool unique)
    {
        // create virtual table if not exists
        if (!column_exists(db_handle, table_name, column_name))
        {
            auto alter_table = std::format("ALTER TABLE [{}] ADD COLUMN [{}] GENERATED ALWAYS AS (json_extract(body, '{}')) VIRTUAL;", table_name, column_name, query);
            details::sqlite::statement stmt{db_handle, alter_table};

            stmt.step();

            if (stmt.result_code() != SQLITE_DONE)
            {
                throw db_exception{db_handle, "Failed to alter table"};
            }
        }

        // create index
        {
            auto create_index = std::format("CREATE {0} INDEX IF NOT EXISTS [Idx_{1}_{2}] on [{1}]({2});", unique ? "UNIQUE" : "", table_name, column_name);
            details::sqlite::statement stmt{db_handle, create_index};

            stmt.step();

            if (stmt.result_code() != SQLITE_DONE)
            {
                throw db_exception{db_handle, "Failed to create index"};
            }
        }

        return *this;
    }

    db_collection &db_collection::index(
        std::string name,
        std::vector<std::pair<std::string, std::string>> const &columns,
        bool unique)
    {
        details::sqlite::transaction transaction{db_handle};

        // add relevant columns
        for (auto &&[column_name, query] : columns)
        {
            if (!column_exists(db_handle, table_name, column_name))
            {
                auto alter_table = std::format("ALTER TABLE [{}] ADD COLUMN [{}] GENERATED ALWAYS AS (json_extract(body, '{}')) VIRTUAL;", table_name, column_name, query);
                auto ret = sqlite3_exec(db_handle, alter_table.c_str(), nullptr, nullptr, nullptr);

                if (ret != SQLITE_OK)
                {
                    throw db_exception{db_handle, "Failed to alter table"};
                }
            }
        }

        // create index
        {
            auto columns_joined_comma = std::accumulate(
                std::next(columns.begin()), columns.end(), columns[0].first,
                [](std::string const &a, std::pair<std::string, std::string> const &b)
                {
                    return a + "," + b.first;
                });
            auto create_index = std::format("CREATE {0} INDEX IF NOT EXISTS [{1}] on [{2}]({3});", unique ? "UNIQUE" : "", name, table_name, columns_joined_comma);
            auto ret = sqlite3_exec(db_handle, create_index.c_str(), nullptr, nullptr, nullptr);

            if (ret != SQLITE_OK)
            {
                throw db_exception{db_handle, "Failed to create index"};
            }
        }

        transaction.commit();

        return *this;
    }

    void db_collection::remove(std::string_view doc_id)
    {
        auto delete_doc_query = std::format("DELETE FROM [{}] WHERE docid=?1;", table_name);
        details::sqlite::statement stmt{db_handle, delete_doc_query};

        stmt
            .bind(1, doc_id)
            .step();

        if (stmt.result_code() != SQLITE_DONE)
        {
            throw db_exception{db_handle, "Failed to delete document"};
        }
    }    

    // DOCUMENT

    db_document::db_document(std::string_view table, std::string_view doc_id, std::string_view body, sqlite3 *db_handle) : table_name(table), doc_id(doc_id), body_data(body), invalid_body(false), db_handle(db_handle) {}
    db_document::db_document(std::string_view table, std::string_view doc_id, sqlite3 *db_handle) : table_name(table), doc_id(doc_id), invalid_body(true), db_handle(db_handle) {}

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

    // remove
    void db_document::erase()
    {
        return db_collection{table_name, db_handle}.remove(doc_id);
    }     

    template <class R>
    R get_value_impl(sqlite3 *db_handle, std::string_view table_name, std::string const &doc_id, std::string_view query)
    {
        auto get_doc_query = std::format("SELECT json_extract(body, ?1) FROM [{}] WHERE docid=?2 AND json_type(body, ?1) IS NOT NULL;", table_name);
        details::sqlite::statement stmt{db_handle, get_doc_query};

        stmt
            .bind(1, query)
            .bind(2, doc_id)
            .step();

        if (stmt.result_code() != SQLITE_ROW)
        {
            throw db_exception{db_handle, "Document or field not found"};
        }

        return stmt.get<R>(0);
    }

    std::string get_value_gen_sql_query(const std::vector<std::string> &fields, const std::string &tableName)
    {
        std::ostringstream sql;
        sql << "SELECT ";
        for (std::size_t i = 0; i < fields.size(); ++i)
        {
            sql << "json_extract(body, ?" << (i + 2) << ")";
            if (i < fields.size() - 1)
            {
                sql << ", ";
            }
        }
        sql << " FROM [" << tableName << "] WHERE docid=?1;";
        return sql.str();
    }

    details::sqlite::statement db_document::get_value_stmt_impl(const std::vector<std::string> &fields) const
    {
        auto get_doc_query = get_value_gen_sql_query(fields, table_name);
        auto stmt = details::sqlite::statement{db_handle, get_doc_query};

        for (std::size_t i = 0; i < fields.size(); i++)
        {
            stmt.bind(i + 2, fields[i]);
        }

        stmt.bind(1, doc_id);
        stmt.step();

        if (stmt.result_code() != SQLITE_ROW)
        {
            throw db_exception{db_handle, "Document not found"};
        }

        return stmt;
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

    // get type
    docudb::json_type db_document::get_type(std::string_view query) const
    {
        std::string get_type_query = std::format(
            "SELECT json_type(body, ?1) FROM [{}] WHERE docid = ?2",
            table_name
        );

        details::sqlite::statement stmt{db_handle, get_type_query};

        stmt
            .bind(1, query)
            .bind(2, doc_id)
            .step();

        docudb::json_type result = docudb::json_type::not_found;
        if (stmt.result_code() == SQLITE_ROW) {
            if (stmt.is_result_null(0)) {
                return docudb::json_type::not_found;
            }
            auto type = stmt.get<std::string>(0);
            if ("null"sv == type) {
                result = docudb::json_type::null;
            } else if ("integer"sv == type) {
                result = docudb::json_type::integer;
            } else if ("real"sv == type) {
                result = docudb::json_type::real;
            } else if ("text"sv == type) {
                result = docudb::json_type::string;
            } else if ("object"sv == type) {
                result = docudb::json_type::object;
            } else if ("array"sv == type) {
                result = docudb::json_type::array;
            } else if ("true"sv == type) {
                result = docudb::json_type::boolean_true;
            } else if ("false"sv == type) {
                result = docudb::json_type::boolean_false;
            }
        }
        return result;
    }    

    // get array length
    std::size_t db_document::get_array_length(std::string_view query) const
    {
        auto get_doc_query = std::format("SELECT json_array_length(body, ?1) FROM [{}] WHERE docid=?2 AND json_type(body, ?1) = 'array';", table_name);
        details::sqlite::statement stmt{db_handle, get_doc_query};

        stmt
            .bind(1, query)
            .bind(2, doc_id)
            .step();

        if (stmt.result_code() != SQLITE_ROW)
        {
            throw db_exception{db_handle, "Document or array not found"};
        }

        return stmt.get<std::int32_t>(0);
    }

    std::vector<std::string> db_document::get_object_keys(std::string_view query) const
    {
        auto enum_query = std::format("SELECT DISTINCT json_each.key FROM [{}], json_each(body, ?1) WHERE docid=?2;", table_name);
        details::sqlite::statement stmt{db_handle, enum_query};

        stmt
            .bind(1, query)
            .bind(2, doc_id);

        std::vector<std::string> ret;
        do
        {
            stmt.step();
            if (stmt.result_code() == SQLITE_ERROR)
            {
                throw db_exception{db_handle, "Failed to enumerate keys"};
            }
            else if (stmt.result_code() != SQLITE_ROW)
            {
                break;
            }
            else
            {
                ret.push_back(stmt.get<std::string>(0));
            }
        } while (true);

        return ret;     
    }

    // DOCUMENT_REF

    db_document_ref::db_document_ref(db_document const &doc) : table_name(doc.table_name), doc_id(doc.doc_id), db_handle(doc.db_handle) {}
    db_document_ref::db_document_ref(std::string_view table_name, std::string_view doc_id, sqlite3 *db_handle) : table_name(table_name), doc_id(doc_id), db_handle(db_handle) {}

    std::string db_document_ref::id() const
    {
        return doc_id;
    }

    db_document db_document_ref::doc() const
    {
        return db_collection{table_name, db_handle}.doc(doc_id);
    }

    void db_document_ref::erase()
    {
        return db_collection{table_name, db_handle}.remove(doc_id);
    }    
}
