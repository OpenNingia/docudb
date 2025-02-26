# Header file `docudb.hpp`

``` cpp
namespace docudb
{
    namespace ops
    {
        struct like;

        struct eq;

        struct neq;
    }

    struct db_exception;

    struct db_document_ref;

    struct db_document;

    struct db_collection;

    struct database;
}
```

### Struct `docudb::ops::like`

``` cpp
struct like
{
};
```

Represents a LIKE operation for querying.

-----

### Struct `docudb::ops::eq`

``` cpp
struct eq
{
};
```

Represents an EQUAL operation for querying.

-----

### Struct `docudb::ops::neq`

``` cpp
struct neq
{
};
```

Represents a NOT EQUAL operation for querying.

-----

### Struct `docudb::db_exception`

``` cpp
struct db_exception
: std::runtime_error
{
    db_exception(sqlite3* db_handle, std::string_view msg);
};
```

Exception class for database errors.

### Constructor `docudb::db_exception::db_exception`

``` cpp
db_exception(sqlite3* db_handle, std::string_view msg);
```

Constructs a new db\_exception object.

#### Parameters

  - `db_handle` - The SQLite database handle.
  - `msg` - The error message.

-----

-----

### Struct `docudb::db_document_ref`

``` cpp
struct db_document_ref
{
    db_document_ref(std::string_view table_name, std::string_view doc_id, sqlite3* db_handle);

    std::string id() const;

    docudb::db_document doc() const;
};
```

Reference to a database document.

### Constructor `docudb::db_document_ref::db_document_ref`

``` cpp
db_document_ref(std::string_view table_name, std::string_view doc_id, sqlite3* db_handle);
```

Constructs a new db\_document\_ref object.

#### Parameters

  - `table_name` - The name of the table.
  - `doc_id` - The document ID.
  - `db_handle` - The SQLite database handle.

-----

### Function `docudb::db_document_ref::id`

``` cpp
std::string id() const;
```

Gets the document ID.

*Return values:* std::string The document ID.

-----

### Function `docudb::db_document_ref::doc`

``` cpp
docudb::db_document doc() const;
```

Gets the full document object.

*Return values:* db\_document The document object.

-----

-----

### Struct `docudb::db_document`

``` cpp
struct db_document
{
    std::string id() const;

    std::string body() const;

    docudb::db_document& body(std::string_view body);

    docudb::db_document& replace(std::string_view query, float value);

    docudb::db_document& replace(std::string_view query, double value);

    docudb::db_document& replace(std::string_view query, std::int32_t value);

    docudb::db_document& replace(std::string_view query, std::int64_t value);

    docudb::db_document& replace(std::string_view query, std::nullptr_t value);

    docudb::db_document& replace(std::string_view query, std::string const& value);

    docudb::db_document& replace(std::string_view query, std::string_view value);

    docudb::db_document& patch(std::string const& json);
};
```

Represents a database document.

### Function `docudb::db_document::id`

``` cpp
std::string id() const;
```

Gets the document ID.

*Return values:* std::string The document ID.

-----

### Function `docudb::db_document::body`

``` cpp
std::string body() const;
```

Gets the full body of the document.

*Return values:* std::string The document body.

-----

### Function `docudb::db_document::body`

``` cpp
docudb::db_document& body(std::string_view body);
```

Sets the full body of the document.

*Return values:* db\_document& Reference to the document.

#### Parameters

  - `body` - The new body content.

-----

### Function `docudb::db_document::replace`

``` cpp
docudb::db_document& replace(std::string_view query, float value);
```

Replace a value in the json body

*Return values:* db\_document& Reference to the document.

#### Parameters

  - `query` - The query that points to the value to replace.
  - `value` - The new value

-----

### Function `docudb::db_document::replace`

``` cpp
docudb::db_document& replace(std::string_view query, double value);
```

Replace a value in the json body

*Return values:* db\_document& Reference to the document.

#### Parameters

  - `query` - The query that points to the value to replace.
  - `value` - The new value

-----

### Function `docudb::db_document::replace`

``` cpp
docudb::db_document& replace(std::string_view query, std::int32_t value);
```

Replace a value in the json body

*Return values:* db\_document& Reference to the document.

#### Parameters

  - `query` - The query that points to the value to replace.
  - `value` - The new value

-----

### Function `docudb::db_document::replace`

``` cpp
docudb::db_document& replace(std::string_view query, std::int64_t value);
```

Replace a value in the json body

*Return values:* db\_document& Reference to the document.

#### Parameters

  - `query` - The query that points to the value to replace.
  - `value` - The new value

-----

### Function `docudb::db_document::replace`

``` cpp
docudb::db_document& replace(std::string_view query, std::nullptr_t value);
```

Replace a value in the json body

*Return values:* db\_document& Reference to the document.

#### Parameters

  - `query` - The query that points to the value to replace.
  - `value` - The new value

-----

### Function `docudb::db_document::replace`

``` cpp
docudb::db_document& replace(std::string_view query, std::string const& value);
```

Replace a value in the json body

*Return values:* db\_document& Reference to the document.

#### Parameters

  - `query` - The query that points to the value to replace.
  - `value` - The new value

-----

### Function `docudb::db_document::replace`

``` cpp
docudb::db_document& replace(std::string_view query, std::string_view value);
```

Replace a value in the json body

*Return values:* db\_document& Reference to the document.

#### Parameters

  - `query` - The query that points to the value to replace.
  - `value` - The new value

-----

### Function `docudb::db_document::patch`

``` cpp
docudb::db_document& patch(std::string const& json);
```

Patches the JSON body of the document.

*Return values:* db\_document& Reference to the document.

#### Parameters

  - `json` - The JSON patch.

-----

-----

### Struct `docudb::db_collection`

``` cpp
struct db_collection
{
    db_collection(std::string_view name, sqlite3* db_handle);

    docudb::db_document doc(std::string_view doc_id);

    docudb::db_document doc();

    void remove(std::string_view doc_id);

    std::vector<db_document_ref> where(std::string_view query, ops::like value);

    std::vector<db_document_ref> where(std::string_view query, ops::eq value);

    std::vector<db_document_ref> where(std::string_view query, ops::neq value);
};
```

Represents a collection of documents in the database.

### Constructor `docudb::db_collection::db_collection`

``` cpp
db_collection(std::string_view name, sqlite3* db_handle);
```

Constructs a new db\_collection object.

#### Parameters

  - `name` - The name of the collection.
  - `db_handle` - The SQLite database handle.

-----

### Function `docudb::db_collection::doc`

``` cpp
docudb::db_document doc(std::string_view doc_id);
```

Gets a document by ID.

*Return values:* db\_document The document object.

#### Parameters

  - `doc_id` - The document ID.

-----

### Function `docudb::db_collection::doc`

``` cpp
docudb::db_document doc();
```

Creates a new document.

*Return values:* db\_document The new document object.

-----

### Function `docudb::db_collection::remove`

``` cpp
void remove(std::string_view doc_id);
```

Removes a document by ID.

#### Parameters

  - `doc_id` - The document ID.

-----

### Function `docudb::db_collection::where`

``` cpp
std::vector<db_document_ref> where(std::string_view query, ops::like value);
```

Searches documents by a LIKE query.

*Return values:* std::vector\<db\_document\_ref\> The list of document references.

#### Parameters

  - `query` - The query string.
  - `value` - The LIKE operation value.

-----

### Function `docudb::db_collection::where`

``` cpp
std::vector<db_document_ref> where(std::string_view query, ops::eq value);
```

Searches documents by an EQUAL query.

*Return values:* std::vector\<db\_document\_ref\> The list of document references.

#### Parameters

  - `query` - The query string.
  - `value` - The EQUAL operation value.

-----

### Function `docudb::db_collection::where`

``` cpp
std::vector<db_document_ref> where(std::string_view query, ops::neq value);
```

Searches documents by a NOT EQUAL query.

*Return values:* std::vector\<db\_document\_ref\> The list of document references.

#### Parameters

  - `query` - The query string.
  - `value` - The NOT EQUAL operation value.

-----

-----

### Struct `docudb::database`

``` cpp
struct database
{
    explicit database(std::string_view path);

    ~database();

    docudb::db_collection collection(std::string_view name);
};
```

Represents the database.

### Constructor `docudb::database::database`

``` cpp
explicit database(std::string_view path);
```

Constructs a new database object.

#### Parameters

  - `path` - The path to the database file.

-----

### Destructor `docudb::database::~database`

``` cpp
~database();
```

Destroys the database object.

-----

### Function `docudb::database::collection`

``` cpp
docudb::db_collection collection(std::string_view name);
```

Gets a collection by name.

*Return values:* db\_collection The collection object.

#### Parameters

  - `name` - The name of the collection.

-----

-----
