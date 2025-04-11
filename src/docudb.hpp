#pragma once

#include <string_view>
#include <string>
#include <vector>
#include <stdexcept>

#include <sqlite3.h>
#include <cstdint>
#include <cmath>
#include <variant>
#include <unordered_map>
#include <memory>
#include <format>

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

        /**
         * \brief Represents a binary operation for querying.
         */
        struct binary_op
        {
            explicit binary_op(
                std::string const &json_query,
                std::string const &op,
                db_value &&value) : var_(json_query), op_(op), index_(++bind_counter)
            {
                binder_.add(index_, std::move(value));
            }

            std::string to_query_string() const
            {
                auto json_query = var_.size() > 0 && var_[0] == '$';
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
         * \brief Represents an EQUAL operation for querying.
         */
        struct eq : binary_op
        {
            explicit eq(
                std::string const &name,
                db_value &&val) : binary_op(name, "=", std::move(val)) {}
        };

        /**
         * \brief Represents a NOT EQUAL operation for querying.
         */
        struct neq : binary_op
        {
            explicit neq(
                std::string const &name,
                db_value &&val) : binary_op(name, "!=", std::move(val)) {}
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
    };

    /**
     * \brief Represents a collection of documents in the database.
     */
    struct db_collection
    {
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
         * \brief Creates a new document.
         *
         * \returns db_document The new document object.
         */
        db_document doc();

        /**
         * \brief Gets all documents in the collection.
         *
         * \returns std::vector<db_document_ref> The list of document references.
         */
        std::vector<db_document_ref> docs();

        /**
         * \brief Removes a document by ID.
         *
         * \param doc_id The document ID.
         */
        void remove(std::string_view doc_id);

        /**
         * \brief Searches documents by a LIKE query.
         *
         * \param query The query string.
         * \param value The LIKE operation value.
         * \returns std::vector<db_document_ref> The list of document references.
         */
        std::vector<db_document_ref> find(query::queryable_type_eraser q) const;

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
        db_collection &index(std::string_view column_name, std::string_view query, bool unique = false);

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
         * \brief Constructs a new database object.
         *
         * \param path The path to the database file.
         */
        explicit database(std::string_view path);

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

    private:
        sqlite3 *db_handle;
    };
}
