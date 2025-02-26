#include <iostream>
#include <docudb.hpp>

int main() {

    // Open a database
    try {
        docudb::database db{"test.docudb"};
        std::cout << "Database opened successfully." << std::endl;
        auto test_collection = db.collection("test_collection");

        auto new_doc_body = "{\"text\":\"Hello, world\"}";
        auto new_doc = test_collection.doc();

        std::cout << "Create document: " << new_doc.id() << std::endl;

        new_doc.body(new_doc_body);

        auto docs = test_collection.where("$.text", docudb::ops::like{"%world%"});
        for(auto&& doc: docs) {
            std::cout << "Found document: " << doc.id() << std::endl;
        }

        std::cout << docs.front().doc().body() << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "db error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}