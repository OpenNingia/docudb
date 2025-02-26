#pragma once

#include <string_view>
#include <string>
#include <vector>
#include <stdexcept>

#include <sqlite3.h>
#include <cstdint>

namespace docudb
{
    namespace ops
    {
        /**
         * \brief Represents a LIKE operation for querying.
         */
        struct like
        {
            std::string value;
        };

        /**
         * \brief Represents an EQUAL operation for querying.
         */
        struct eq
        {
            std::string value;
        };

        /**
         * \brief Represents a NOT EQUAL operation for querying.
         */
        struct neq
        {
            std::string value;
        };
    }

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
        db_document &replace(std::string_view query, float value);

        /**
         * \brief Replace a value in the json body
         * 
         * \param query The query that points to the value to replace.
         * \param value The new value
         * \returns db_document& Reference to the document.
         */        
        db_document &replace(std::string_view query, double value);

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
        db_document &set(std::string_view query, float value);
        db_document &set(std::string_view query, double value);
        db_document &set(std::string_view query, std::int32_t value);
        db_document &set(std::string_view query, std::int64_t value);
        db_document &set(std::string_view query, std::nullptr_t value);
        db_document &set(std::string_view query, std::string const &value);
        db_document &set(std::string_view query, std::string_view value);

        // insert value in json body
        db_document &insert(std::string_view query, float value);
        db_document &insert(std::string_view query, double value);
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
        db_document &patch(std::string const &json);
        db_document &patch(std::string_view json);

    private:
        std::string doc_id;
        std::string table_name;
        std::string body_data;
        sqlite3 *db_handle;
        friend struct db_collection;

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
         * \brief Gets a document by ID.
         * 
         * \param doc_id The document ID.
         * \returns db_document The document object.
         */
        db_document doc(std::string_view doc_id);

        /**
         * \brief Creates a new document.
         * 
         * \returns db_document The new document object.
         */
        db_document doc();

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
        std::vector<db_document_ref> where(std::string_view query, ops::like value);

        /**
         * \brief Searches documents by an EQUAL query.
         * 
         * \param query The query string.
         * \param value The EQUAL operation value.
         * \returns std::vector<db_document_ref> The list of document references.
         */
        std::vector<db_document_ref> where(std::string_view query, ops::eq value);

        /**
         * \brief Searches documents by a NOT EQUAL query.
         * 
         * \param query The query string.
         * \param value The NOT EQUAL operation value.
         * \returns std::vector<db_document_ref> The list of document references.
         */
        std::vector<db_document_ref> where(std::string_view query, ops::neq value);        

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
        db_collection collection(std::string_view name);

    private:
        sqlite3 *db_handle;
    };
}