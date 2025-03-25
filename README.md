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

## License

This project is licensed under the MIT License.
