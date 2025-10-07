#pragma once

#include <string_view>
#include <string>
#include <vector>
#include <stdexcept>
#include <functional>

#include <sqlite3.h>
#include <cstdint>
#include <cmath>
#include <variant>
#include <unordered_map>
#include <memory>
#include <format>

// sqlite3 forward declarations
struct sqlite3;
struct sqlite3_stmt;

namespace docudb
{
    /**
     * \brief Types supported.
     */
    using db_value = std::variant<
        std::float_t,
        std::double_t,
        std::int32_t,
        std::int64_t,
        std::nullptr_t,
        std::string>;

    /**
     * \brief JSON types.
     */
    enum class json_type {
        null,
        integer,
        real,
        string,
        object,
        array,
        boolean_true,
        boolean_false,
        not_found
    };

    /**
     * \brief Specifies the database file open mode.
     */
    enum class open_mode {
        /**
         * \brief Open the database for read-only access. The database must already exist.
         */
        read_only,
        /**
         * \brief Open the database for reading and writing. The database must already exist.
         */
        read_write,
        /**
         * \brief Open the database for reading and writing, and create it if it does not exist.
         */
        read_write_create
    };

    /**
     * \brief Specifies the threading mode for the database connection.
     * \note The threading mode specified by these flags overrides the compile-time default.
     *       Refer to SQLite's documentation on sqlite3_open_v2 for details.
     */
    enum class threading_mode {
        /**
         * \brief Use the default threading mode. If you need single-thread mode, SQLite
         *        must be compiled accordingly. This option adds no specific threading flags.
         */
        default_mode,
        /**
         * \brief The new database connection will use the multi-thread threading mode.
         */
        multi_thread,
        /**
         * \brief The new database connection will use the serialized threading mode.
         */
        serialized
    };


    namespace details::sqlite {

        struct statement
        {
            statement(sqlite3 *db_handle, std::string_view query);
            ~statement();
            sqlite3_stmt *data() const noexcept;
            statement &bind(int index, std::int16_t value);
            statement &bind(int index, std::int32_t value);
            statement &bind(int index, std::int64_t value);           
            statement &bind(int index, std::nullptr_t value);
            statement &bind(int index, std::string_view value);
            statement &bind(int index, std::double_t value);
            statement &bind(int index, std::float_t value);
            statement &step() noexcept;
            template <typename T>
            T get(int index) const
            {
                static_assert(std::false_type::value, "Unsupported type for get method");
            }

            json_type get_type(int index) const noexcept;

            bool is_result_null(int index) const noexcept;

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
        std::string statement::get(int index) const;
        template <>
        std::double_t statement::get(int index) const;
        template <>
        std::int64_t statement::get(int index) const;
        template <>
        std::int32_t statement::get(int index) const;
        template <>
        std::int16_t statement::get(int index) const;        
        template <>
        std::uint64_t statement::get(int index) const;
        template <>
        std::uint32_t statement::get(int index) const;        
        template <>
        std::uint16_t statement::get(int index) const;   
    }

    namespace query
    {
        /**
         * \brief Represents a binder for query parameters.
         */
        struct binder
        {
            const std::unordered_map<int, db_value> &get_parameters() const
            {
                return bind_map;
            }

            void merge(binder &b)
            {
                bind_map.merge(b.bind_map);
            }

            void add(int index, db_value &&v)
            {
                bind_map.emplace(index, std::move(v));
            }

        private:
            std::unordered_map<int, db_value> bind_map;
        };

        /**
         * \brief Represents a queryable object.
         */
        template <typename T>
        concept Queryable = requires(T x) {
            { x.to_query_string() } -> std::same_as<std::string>;
            { x.get_binder() } -> std::same_as<binder &>;
        };

        /**
         * \brief Represents a logic gate for combining two queryable objects.
         */
        template <Queryable A, Queryable B>
        struct logic_gate
        {

            explicit logic_gate(A &&a, B &&b, std::string const &gate) : a_(std::move(a)), b_(std::move(b)), gate_(gate)
            {
                binder_.merge(a_.get_binder());
                binder_.merge(b_.get_binder());
            }

            std::string to_query_string() const
            {
                return std::format("{} {} {}", a_.to_query_string(), gate_, b_.to_query_string());
            }

            binder const &get_binder() const
            {
                return binder_;
            }

            binder &get_binder()
            {
                return binder_;
            }

        private:
            A a_;
            B b_;
            std::string gate_;
            binder binder_;
        };

        /**
         * \brief Represents a LOGIC AND gate.
         */
        template <Queryable A, Queryable B>
        struct logic_and : logic_gate<A, B>
        {

            explicit logic_and(A &&a, B &&b) : logic_gate<A, B>(std::move(a), std::move(b), "AND")
            {
            }
        };

        /**
         * \brief Represents a LOGIC OR gate.
         */
        template <Queryable A, Queryable B>
        struct logic_or : logic_gate<A, B>
        {

            explicit logic_or(A &&a, B &&b) : logic_gate<A, B>(std::move(a), std::move(b), "OR")
            {
            }
        };

        extern thread_local int bind_counter;
        const int MAX_VAR_NUM = 250000;

        /**
         * \brief Represents a binary operation for querying.
         */
        struct binary_op
        {
            explicit binary_op(
                std::string const &json_query,
                std::string const &op,
                db_value &&value) : var_(json_query), op_(op), index_(1 + (++bind_counter % MAX_VAR_NUM))
            {
                binder_.add(index_, std::move(value));
            }

            explicit binary_op(
                std::string const &json_query,
                std::string const &op,
                nullptr_t) : var_(json_query), op_(op), index_(-1)
            {

            }            

            std::string to_query_string() const
            {
                auto json_query = var_.size() > 0 && var_[0] == '$';
                auto is_value_null = index_ == -1;

                // special case for null value
                if (is_value_null) {
                    if (json_query)
                        return std::format("json_type(body, '{0}') IS NOT NULL AND json_extract(body, '{0}') {1} NULL", var_, op_);
                    else
                        return std::format("[{}] {} NULL", var_, op_);                    
                }

                if (json_query)
                    return std::format("(json_extract(body, '{}') {} ?{})", var_, op_, index_);
                else
                    return std::format("([{}] {} ?{})", var_, op_, index_);
            }

            binder const &get_binder() const
            {
                return binder_;
            }

            binder &get_binder()
            {
                return binder_;
            }

        private:
            std::string var_;
            std::string op_;
            binder binder_;
            int index_;
        };

        /**
         * \brief Represents a LIKE operation for querying.
         */
        struct like : binary_op
        {
            explicit like(
                std::string const &name,
                std::string const &val) : binary_op(name, "LIKE", val) {}
        };

        /**
         * \brief Represents a REGEXP operation for querying.
         */
        struct regexp : binary_op
        {
            explicit regexp(
                std::string const &name,
                std::string const &val) : binary_op(name, "REGEXP", val) {}
        };        

        /**
         * \brief Represents an EQUAL operation for querying.
         */
        struct eq : binary_op
        {
            explicit eq(
                std::string const &name,
                db_value &&val) : binary_op(name, "=", std::move(val)) {}

            explicit eq(
                std::string const &name,
                nullptr_t) : binary_op(name, "IS", nullptr) {}
        };

        /**
         * \brief Represents a NOT EQUAL operation for querying.
         */
        struct neq : binary_op
        {
            explicit neq(
                std::string const &name,
                db_value &&val) : binary_op(name, "!=", std::move(val)) {}

            explicit neq(
                std::string const &name,
                nullptr_t) : binary_op(name, "IS NOT", nullptr) {}                
        };

        /**
         * \brief Represents a GREATER THAN operation for querying.
         */
        struct gt : binary_op
        {
            explicit gt(
                std::string const &name,
                db_value &&val) : binary_op(name, ">", std::move(val)) {}
        };

        /**
         * \brief Represents a LESSER THAN operation for querying.
         */
        struct lt : binary_op
        {
            explicit lt(
                std::string const &name,
                db_value &&val) : binary_op(name, "<", std::move(val)) {}
        };

        /**
         * \brief Represents a GREATER OR EQUAL THAN operation for querying.
         */
        struct gte : binary_op
        {
            explicit gte(
                std::string const &name,
                db_value &&val) : binary_op(name, ">=", std::move(val)) {}
        };

        /**
         * \brief Represents a LESSER OR EQUAL THAN operation for querying.
         */
        struct lte : binary_op
        {
            explicit lte(
                std::string const &name,
                db_value &&val) : binary_op(name, "<=", std::move(val)) {}
        };

        template <Queryable A, Queryable B>
        logic_and<A, B> operator&&(A &&a, B &&b)
        {
            return logic_and<A, B>{std::forward<A>(a), std::forward<B>(b)};
        }

        template <Queryable A, Queryable B>
        logic_or<A, B> operator||(A &&a, B &&b)
        {
            return logic_or<A, B>{std::forward<A>(a), std::forward<B>(b)};
        }

        /**
         * \brief Abstract base class for Queryable objects.
         */
        struct queryable_base
        {
            virtual ~queryable_base() = default;

            // Pure virtual methods
            virtual std::string to_query_string() const = 0;
            virtual binder get_binder() const = 0;
        };

        /**
         * \brief Concrete wrapper for erasing the type of a Queryable object.
         */
        template <Queryable T>
        struct queryable_wrapper : queryable_base
        {
            explicit queryable_wrapper(T &&obj) : obj_(std::forward<T>(obj)) {}

            std::string to_query_string() const override
            {
                return obj_.to_query_string();
            }

            binder get_binder() const override
            {
                return obj_.get_binder();
            }

        private:
            T obj_;
        };

        /**
         * \brief Type-erased Queryable object.
         */
        struct queryable_type_eraser
        {
            template <Queryable T>
            queryable_type_eraser(T &&obj)
                : ptr_(std::make_unique<queryable_wrapper<T>>(std::forward<T>(obj))) {}

            std::string to_query_string() const
            {
                return ptr_->to_query_string();
            }

            binder get_binder() const
            {
                return ptr_->get_binder();
            }

        private:
            std::unique_ptr<queryable_base> ptr_;
        };

        struct order_by
        {
            explicit order_by(std::string const &field, bool ascending = true)
                : field_(field), ascending_(ascending) {}

            std::string const& field() const { return field_; }
            std::string_view direction() const { return ascending_ ? "ASC" : "DESC"; }

            private:
                std::string field_;
                bool ascending_;
        };
    }

    /**
     * \brief Gets the version of the library.
     *
     * \returns std::string The version string.
     */
    std::string get_version() noexcept;

    /**
     * \brief Gets the build timestamp.
     *
     * \returns std::string The timestamp string.
     */
    std::string get_build_timestamp() noexcept;

    /**
     * \brief Exception class for database errors.
     */
    struct db_exception : std::runtime_error
    {
        /**
         * \brief Constructs a new db_exception object.
         *
         * \param db_handle The SQLite database handle.
         * \param msg The error message.
         */
        db_exception(sqlite3 *db_handle, std::string_view msg);
    };

    /**
     * \brief Exception class for database errors.
     */
    struct stmt_exception : db_exception
    {
        /**
         * \brief Constructs a new db_exception object.
         *
         * \param db_handle The SQLite database handle.
         * \param msg The failing statement.
         */
        stmt_exception(sqlite3 *db_handle, std::string_view sql);
    };    

    struct db_document;

    /**
     * \brief Reference to a database document.
     */
    struct db_document_ref
    {
        /**
         * \brief Constructs a new db_document_ref object from a db_document object.
         *
         * \param doc The document object.
         */
        db_document_ref(db_document const &doc);

        /**
         * \brief Constructs a new db_document_ref object.
         *
         * \param table_name The name of the table.
         * \param doc_id The document ID.
         * \param db_handle The SQLite database handle.
         */
        db_document_ref(std::string_view table_name, std::string_view doc_id, sqlite3 *db_handle);

        /**
         * \brief Gets the document ID.
         *
         * \returns std::string The document ID.
         */
        std::string id() const;

        /**
         * \brief Gets the full document object.
         *
         * \returns db_document The document object.
         */
        db_document doc() const;

        /**
         * \brief Removes the document from the collection.
         * \returns void
         */        
        void erase();        

    private:
        sqlite3 *db_handle;
        std::string table_name;
        std::string doc_id;
    };

    /**
     * \brief Represents a database document.
     */
    struct db_document
    {
        /**
         * \brief Gets the document ID.
         *
         * \returns std::string The document ID.
         */
        std::string id() const;

        /**
         * \brief Gets the full body of the document.
         *
         * \returns std::string The document body.
         */
        std::string body() const;

        /**
         * \brief Sets the full body of the document.
         *
         * \param body The new body content.
         * \returns db_document& Reference to the document.
         */
        db_document &body(std::string_view body);

        /**
         * \brief Replace a value in the json body
         *
         * \param query The query that points to the value to replace.
         * \param value The new value
         * \returns db_document& Reference to the document.
         */
        db_document &replace(std::string_view query, std::float_t value);

        /**
         * \brief Replace a value in the json body
         *
         * \param query The query that points to the value to replace.
         * \param value The new value
         * \returns db_document& Reference to the document.
         */
        db_document &replace(std::string_view query, std::double_t value);

        /**
         * \brief Replace a value in the json body
         *
         * \param query The query that points to the value to replace.
         * \param value The new value
         * \returns db_document& Reference to the document.
         */
        db_document &replace(std::string_view query, std::int32_t value);

        /**
         * \brief Replace a value in the json body
         *
         * \param query The query that points to the value to replace.
         * \param value The new value
         * \returns db_document& Reference to the document.
         */
        db_document &replace(std::string_view query, std::int64_t value);

        /**
         * \brief Replace a value in the json body
         *
         * \param query The query that points to the value to replace.
         * \param value The new value
         * \returns db_document& Reference to the document.
         */
        db_document &replace(std::string_view query, std::nullptr_t value);

        /**
         * \brief Replace a value in the json body
         *
         * \param query The query that points to the value to replace.
         * \param value The new value
         * \returns db_document& Reference to the document.
         */
        db_document &replace(std::string_view query, std::string const &value);

        /**
         * \brief Replace a value in the json body
         *
         * \param query The query that points to the value to replace.
         * \param value The new value
         * \returns db_document& Reference to the document.
         */
        db_document &replace(std::string_view query, std::string_view value);

        // set value in json body
        db_document &set(std::string_view query, std::float_t value);
        db_document &set(std::string_view query, std::double_t value);
        db_document &set(std::string_view query, std::int32_t value);
        db_document &set(std::string_view query, std::int64_t value);
        db_document &set(std::string_view query, std::nullptr_t value);
        db_document &set(std::string_view query, std::string const &value);
        db_document &set(std::string_view query, std::string_view value);

        // insert value in json body
        db_document &insert(std::string_view query, std::float_t value);
        db_document &insert(std::string_view query, std::double_t value);
        db_document &insert(std::string_view query, std::int32_t value);
        db_document &insert(std::string_view query, std::int64_t value);
        db_document &insert(std::string_view query, std::nullptr_t value);
        db_document &insert(std::string_view query, std::string const &value);
        db_document &insert(std::string_view query, std::string_view value);

        /**
         * \brief Patches the JSON body of the document.
         *
         * \param json The JSON patch.
         * \returns db_document& Reference to the document.
         */
        db_document &patch(std::string_view json);

        // get string
        std::string get_string(std::string_view query) const;

        // get number
        std::int64_t get_number(std::string_view query) const;

        // get real
        std::double_t get_real(std::string_view query) const;

        // get json type
        json_type get_type(std::string_view query) const;

        // get array length
        std::size_t get_array_length(std::string_view query) const;

        // get object keys
        std::vector<std::string> get_object_keys(std::string_view query) const;

        // get values
        template <typename... Types>
        std::tuple<Types...> get(const std::vector<std::string>& fields) const {
            if (fields.size() != sizeof...(Types)) {
                throw std::invalid_argument("Number of fields does not match the number of types.");
            }

            auto stmt = get_value_stmt_impl(fields);
            auto ret = get_values_impl<Types...>(stmt, std::index_sequence_for<Types...>{});

            return ret;
        }
        
        /**
         * \brief Removes the document from the collection.
         * \returns void
         */        
        void erase();
    private:
        template <typename... Types, std::size_t... Indices>
        std::tuple<Types...> get_values_impl(details::sqlite::statement const &stmt, std::index_sequence<Indices...>) const
        {
            return std::make_tuple(stmt.get<Types>(Indices)...);
        }   
        
        details::sqlite::statement get_value_stmt_impl(const std::vector<std::string>& fields) const;

    private:
        std::string doc_id;
        std::string table_name;
        mutable std::string body_data;
        bool invalid_body;
        sqlite3 *db_handle;
        friend struct db_collection;
        friend struct db_document_ref;

        /**
         * \brief Constructs a new db_document object.
         *
         * \param table_name The name of the table.
         * \param doc_id The document ID.
         * \param body The document body.
         * \param db_handle The SQLite database handle.
         */
        db_document(std::string_view table_name, std::string_view doc_id, std::string_view body, sqlite3 *db_handle);
        db_document(std::string_view table, std::string_view doc_id, sqlite3 *db_handle);
    };

    /**
     * \brief Represents a collection of documents in the database.
     */
    struct db_collection
    {
        /**
         * \brief Constructs an empty db_collection object.
         *
         */
        db_collection() = default;
        db_collection(db_collection const &) = delete;
        db_collection &operator=(db_collection const &) = delete;
        db_collection(db_collection &&) = default;
        db_collection &operator=(db_collection &&) = default;

        /**
         * \brief Constructs a new db_collection object.
         *
         * \param name The name of the collection.
         * \param db_handle The SQLite database handle.
         */
        db_collection(std::string_view name, sqlite3 *db_handle);

        /**
         * \brief Gets the collection name
         *
         * \returns std::string The collection name.
         */
        std::string name() const noexcept;

        /**
         * \brief Gets a document by ID.
         *
         * \param doc_id The document ID.
         * \returns db_document The document object.
         */
        db_document doc(std::string_view doc_id) const;

        /**
         * \brief Creates a new document with a generated UUID.
         *
         * \returns db_document The new document object.
         */
        db_document doc();

        /**
         * \brief Creates a new document with the given ID.
         *
         * \returns db_document The new document object.
         */        
        db_document create(std::string_view doc_id);

        /**
         * \brief Gets the number of documents in the collection.
         *
         * \returns std::size_t The number of documents in the collection.
         */        
        std::size_t count() const;

        /**
         * \brief Gets the number of documents that matches the given query.
         *
         * \param q The query object.
         * \returns std::size_t The number of matching documents.
         */        
        std::size_t count(query::queryable_type_eraser q) const;        

        /**
         * \brief Gets all documents in the collection.
         *
         * \returns std::vector<db_document_ref> The list of document references.
         */
        std::vector<db_document_ref> docs() const;

        /**
         * \brief Removes a document by ID.
         *
         * \param doc_id The document ID.
         */
        void remove(std::string_view doc_id);

        /**
         * \brief Searches documents by query.
         *
         * \param q The query object.
         * \param order_by The order by object (optional)
         * \param limit The maximum number of documents to return (optional).
         * \returns std::vector<db_document_ref> The list of document references.
         */
        std::vector<db_document_ref> find(query::queryable_type_eraser q, std::optional<query::order_by> order_by = std::nullopt, std::optional<int> limit = std::nullopt) const;

        /**
         * \brief Indexes the document based on the specified column and query.
         *
         * This function creates a new virtual column representing the given json query
         * and add an index to that column.
         *
         * \param column_name The name of the column to be created.
         * \param query The json query string in SQLite format representing the field to be indexed
         * \param unique The index should be unique
         * \return A reference to the document.
         */
        db_collection& index(std::string_view column_name, std::string_view query, bool unique = false);

        /**
         * \brief Indexes the document based on the specified columns and query.
         *
         * This function creates a new virtual column representing the given json query
         * and add an index to that column.
         * \param name The index name
         * \param columns A vector of pairs [column_name, query]
         * \param unique The index should be unique
         * \return A reference to the document.
         */        
        db_collection& index(
            std::string name,
            std::vector<std::pair<std::string, std::string>> const &columns,
            bool unique);   
    private:
        sqlite3 *db_handle;
        std::string table_name;
    };

    /**
     * \brief Represents the database.
     */
    struct database
    {
        /**
         * \brief Constructs an empty database object.
         *
         */
        database() = default;
        database(database const &) = delete;
        database &operator=(database const &) = delete;

        database(database &&);
        database &operator=(database &&);

        /**
         * \brief Constructs a new database object.
         *
         * \param connection_string A valid connection string to the database (e.g. the database file path or :memory:).
         */
        explicit database(std::string_view connection_string);

        /**
         * \brief Constructs a new database object.
         *
         * \param connection_string A valid connection string to the database (e.g. the database file path or :memory:).
         * \param mode The mode for opening the database file.
         * \param thread_mode The threading model for the connection.
         */
        explicit database(
            std::string_view connection_string,
            open_mode mode,
            threading_mode thread_mode);

        /**
         * \brief Destroys the database object.
         */
        ~database();

        /**
         * \brief Gets a collection by name.
         *
         * \param name The name of the collection.
         * \returns db_collection The collection object.
         */
        db_collection collection(std::string_view name) const;

        /**
         * \brief Gets all collections
         *
         * \returns std::vector<db_collection> The collection list.
         */
        std::vector<db_collection> collections() const;

        /**
         * \brief Load docudb's sqlite3 extensions (e.g. regexp)
         *
         */
        void load_extensions() const;

        /**
         * \brief Backup the current database into the destination database
         *
         * \param name The destination database.
         * \param progress Progress callback void progress(int remaining, int total) { ... }
         */        
        void backup_to(database& dest, std::function<void(int, int)> progress = [](int, int){}) const;

        /**
         * \brief Returns the database file name
         *
         */
        std::string filename_database() const noexcept;

        /**
         * \brief Returns the database journal file name
         *
         */        
        std::string filename_journal() const noexcept;

        /**
         * \brief Returns the database write-ahead log file name
         *
         */        
        std::string filename_wal() const noexcept;

    private:
        sqlite3 *db_handle;
    };
}
