# DocuDB

DocuDB is a lightweight document database built on top of SQLite. It provides a simple interface for managing JSON documents.

## Features

- Create, read, update, and delete documents
- Query documents with various operations (like, eq, neq, ...)
- Patch JSON documents

## Compiler support

The library has been tested with the following C++ compilers

- cl 19.40.33812 (VS 2022)
- g++ 13.1.0

However it should build with any reasonable C++20 compiler.

## Inspiration

https://dgl.cx/2020/06/sqlite-json-support

## Installation

To use DocDB, include the header file in your project:

```cpp
#include "docudb.hpp"
```

## Usage

### Creating a Database

```cpp
#include "docudb.hpp"

int main() {
    docudb::database db(":memory:");
    return 0;
}
```

### Creating a Collection

```cpp
docudb::db_collection collection = db.collection("my_collection");
```

### Creating a Document

```cpp
docudb::db_document new_doc = collection.doc();
new_doc.body(R"({"name": "John Doe", "age": 30})");
```

### Querying Documents

```cpp
std::vector<docudb::db_document_ref> results = collection.where("name", docudb::ops::like{"John%"});
for (const auto& doc_ref : results) {
    docudb::db_document doc = doc_ref.doc();
    std::cout << doc.body() << std::endl;
}
```

### Updating a Document

```cpp
auto document_id = ...
docudb::db_document doc = collection.doc(document_id);
doc.set("$.age"sv, 42);
```

### Deleting a Document

```cpp
auto document_id = ...
collection.remove(document_id);
```

## Concurrency and Thread Safety

It is crucial to understand the concurrency model of DocuDB to use it safely in a multi-threaded application. The library's thread safety is directly inherited from the underlying SQLite C library it is built upon.

### SQLite's Threading Modes

SQLite databases can be opened in one of three threading modes. You should be aware of how your system's SQLite library is configured. For detailed information, please refer to the official SQLite documentation on the topic: **[SQLite and Multiple Threads](https://sqlite.org/threadsafe.html)**.

Here is a summary of how to use DocuDB safely based on SQLite's mode:

*   **Serialized Mode:** This is the default mode on most platforms. In this mode, a single `docudb::database` object (and its corresponding `sqlite3` handle) **can be safely shared and used across multiple threads**. SQLite guarantees that all access is serialized, preventing race conditions.

*   **Multi-thread Mode:** In this mode, a single `docudb::database` object **must not be shared across threads**. The safe and recommended pattern is to create a separate `docudb::database` connection object for each thread that needs to interact with the database.

*   **Single-thread Mode:** In this mode, all database access must be confined to the single thread that created the connection. The library is not thread-safe in this configuration.

### Query Builder Limitations

The Query Builder DSL uses a `thread_local` counter to generate unique names for bound parameters (e.g., `:p1`, `:p2`). This has an important implication:

**A single query builder chain must be constructed and executed on the same thread.**

You cannot, for example, start building a query on one thread and pass the partially-built query object to another thread to finish and execute it.

This design also means that while you can use a connection pool, you must be careful if a single thread can acquire different database connections from the pool. Using multiple `docudb::database` objects on the same thread can cause the `thread_local` counter to be shared, potentially leading to incorrect query parameter binding if queries are built in an interleaved fashion. The most robust approach remains creating one connection per thread.

### Object Lifetime and Other Safety Issues

#### Dangling Document References (`db_document_ref`)

A `docudb::db_document_ref` object holds a raw pointer to the internal `sqlite3` handle, which is managed by the `docudb::database` object. If the `database` object is destroyed before a `db_document_ref` that refers to it, the reference will become a dangling pointer, and using it will lead to undefined behavior.

**Safe Usage:** Always ensure that the `docudb::database` object outlives any document or collection objects derived from it.

```cpp
// SAFE: db outlives doc_ref
void safe_example() {
    docudb::database db("my.db");
    auto collection = db.collection("my_collection");
    auto doc_ref = collection.doc("some_id"); 
    // ... use doc_ref ...
} // doc_ref is destroyed first, then db is destroyed.

// UNSAFE: The returned db_document_ref is dangling because its db is destroyed.
std::optional<docudb::db_document_ref> get_ref() {
    docudb::database db("my.db");
    auto collection = db.collection("my_collection");
    return collection.doc("some_id"); 
}
```

#### Non-Cryptographically Secure UUIDs

The library generates UUIDs for new documents using `std::mt19937`, which is **not cryptographically secure**. Do not rely on these UUIDs in security-sensitive contexts where guessability is a concern. If you need secure random identifiers, generate them using a dedicated cryptography library and provide them when creating documents.

Of course. Here is a new "Usage" section for your `README.md` file that explains how to use the new database connection options. You can add this to your main usage or features section.

## Advanced Usage

### Connecting to a Database

You can connect to a database file or an in-memory database by creating a `docudb::database` object.

#### Basic Connection

The simplest way to open a database is to provide a path. This is suitable for most use cases.

```cpp
#include "docudb/docudb.hpp"

// Open a database file. It will be created if it doesn't exist.
docudb::database db("my_app.db");

// Open an in-memory database.
docudb::database in_memory_db(":memory:");
```

By default the database will be opened with the default built-in in the SQLite library.

#### Advanced Connection Options

For more control over the connection, you can provide optional `open_mode` and `threading_mode` arguments to the constructor.

```cpp
explicit database(
    std::string_view connection_string,
    open_mode mode = open_mode::read_write_create,
    threading_mode thread_mode = threading_mode::serialized);
```

**1. Open Mode (`open_mode`)**

This enum controls the file access permissions.

*   `docudb::open_mode::read_only`: Opens an existing database for reading only.
*   `docudb::open_mode::read_write`: Opens an existing database for reading and writing.
*   `docudb::open_mode::read_write_create`: (Default) Opens for reading and writing, creating the file if it does not already exist.

**Example: Opening a database for read-only access**
```cpp
// This will fail if "existing_db.db" does not exist.
docudb::database db(
    "existing_db.db",
    docudb::open_mode::read_only
);
```

**2. Threading Mode (`threading_mode`)**

This enum specifies the concurrency model for the connection. For more details on what these mean, please refer to the **Concurrency and Thread Safety** section.

*   `docudb::threading_mode::default_mode`: Uses SQLite's default mode.
*   `docudb::threading_mode::multi_thread`: The connection can only be used in one thread at a time.
*   `docudb::threading_mode::serialized`: (Default) The connection is protected by a mutex and can be safely used across multiple threads.

**Example: Opening a database in Multi-Thread mode**
```cpp
// Open a database intended for single-threaded access.
docudb::database db(
    "my_app.db",
    docudb::open_mode::read_write_create,
    docudb::threading_mode::multi_thread
);
```

## License

This project is licensed under the MIT License.
