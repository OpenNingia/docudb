#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <docudb.hpp>

using namespace std::string_view_literals;

TEST_CASE("Create a collection and insert a new item")
{
    docudb::database db{":memory:"};
    auto test_collection = db.collection("test_collection");

    auto new_doc_body = "{\"text\":\"Hello, world\"}"sv;
    auto new_doc = test_collection.doc().body(new_doc_body);

    // uuid is 36 characters long
    REQUIRE(new_doc.id().size() == 36);

    auto docs = test_collection.where("$.text", docudb::ops::like{"%world%"});

    REQUIRE(docs.size() == 1);
    REQUIRE(docs.front().doc().body().data() == doctest::Contains("world"));
}

TEST_CASE("Malformed JSON should throw an exception")
{
    docudb::database db{":memory:"};
    auto test_collection = db.collection("test_collection");
    auto new_doc_body = "A malformed JSON string"sv;
    auto new_doc = test_collection.doc();

    REQUIRE_THROWS_AS(new_doc.body(new_doc_body), docudb::db_exception);
}

TEST_CASE("Update a document doesn't change docid")
{
    // initialize database
    docudb::database db{":memory:"};
    // initialize collection
    auto test_collection = db.collection("test_collection");
    // create a new document and store its id
    auto new_doc_body = "{\"text\":\"Hello, world\"}"sv;
    auto new_doc = test_collection.doc().body(new_doc_body);
    auto initial_doc_id = new_doc.id();
    // get the document from the collection and update the body
    auto my_doc = test_collection.doc(initial_doc_id).body("{\"text\":\"Hello, universe\"}"sv);
    // now find the document in the collection, again
    auto my_doc_again = test_collection.where("$.text", docudb::ops::like{R"(%universe%)"}).front().doc();
    // check if the docid is the same
    REQUIRE(my_doc_again.id() == initial_doc_id);
}

TEST_CASE("Insert a new key")
{
    // initialize database
    docudb::database db{":memory:"};
    // initialize collection
    auto test_collection = db.collection("test_collection");
    // create a new document and store its id
    auto new_doc_body = "{\"text\":\"Hello, world\"}"sv;
    auto new_doc = test_collection.doc().body(new_doc_body);
    auto initial_doc_id = new_doc.id();
    // insert a new item in the document
    new_doc.insert("$.new_key"sv, "new value"sv);
    // find the document using the added value
    auto my_doc_again = test_collection.where("$.new_key", docudb::ops::eq("new value")).front().doc();
    // check if the docid is the same
    REQUIRE(my_doc_again.id() == initial_doc_id);
}

TEST_CASE("Insert is a NOOP if the key already exist")
{
    // initialize database
    docudb::database db{":memory:"};
    // initialize collection
    auto test_collection = db.collection("test_collection");
    // create a new document and store its id
    auto new_doc_body = "{\"text\":\"Hello, world\"}"sv;
    auto new_doc = test_collection.doc().body(new_doc_body);
    auto initial_doc_id = new_doc.id();
    new_doc.insert("$.text"sv, "new value"sv);
    auto result = test_collection.where("$.text", docudb::ops::eq("new value"));
    REQUIRE(result.empty());
}

TEST_CASE("Replace an existing key")
{
    // initialize database
    docudb::database db{":memory:"};
    // initialize collection
    auto test_collection = db.collection("test_collection");
    // create a new document and store its id
    auto new_doc_body = "{\"text\":\"Hello, world\"}"sv;
    auto new_doc = test_collection.doc().body(new_doc_body);
    auto initial_doc_id = new_doc.id();
    // insert a new item in the document
    new_doc.replace("$.text"sv, "new value"sv);
    // find the document using the added value
    auto my_doc_again = test_collection.where("$.text", docudb::ops::eq("new value")).front().doc();
    // check if the docid is the same
    REQUIRE(my_doc_again.id() == initial_doc_id);
}

TEST_CASE("Replace is a NOOP if the key doesn't exist")
{
    // initialize database
    docudb::database db{":memory:"};
    // initialize collection
    auto test_collection = db.collection("test_collection");
    // create a new document and store its id
    auto new_doc_body = "{\"text\":\"Hello, world\"}"sv;
    auto new_doc = test_collection.doc().body(new_doc_body);
    auto initial_doc_id = new_doc.id();
    new_doc.replace("$.new_key"sv, "new value"sv);
    auto result = test_collection.where("$.new_key", docudb::ops::eq("new value"));
    REQUIRE(result.empty());
}

TEST_CASE("Build document using fluent syntax")
{
    // initialize database
    docudb::database db{":memory:"};
    // initialize collection
    auto test_collection = db.collection("test_collection");
    // create a new document and store its id
    auto new_doc = test_collection.doc()
        .set("$.text"sv, "Hello World"sv)
        .set("$.number"sv, 42)
        .set("$.real"sv, 42.42);

    INFO("Document ID: ", new_doc.id());
    INFO("Document: ", new_doc.body());

    // check values
    REQUIRE(new_doc.get_string("$.text") == "Hello World"sv);
    REQUIRE(new_doc.get_number("$.number") == 42ll);
    REQUIRE(new_doc.get_real("$.real") == 42.42);
}

TEST_CASE("Create index")
{
    // initialize database
    docudb::database db{":memory:"};
    // initialize collection
    auto test_collection = db.collection("test_collection");
    // create index
    REQUIRE_NOTHROW(test_collection.index("user", "$.user"));
}

TEST_CASE("Unique index")
{
    // initialize database
    docudb::database db{":memory:"};
    // initialize collection
    auto test_collection = db.collection("test_collection");
    // create index
    REQUIRE_NOTHROW(test_collection.index("user", "$.user", true));
    // insert document with user 'wario'
    REQUIRE_NOTHROW(test_collection.doc().set("$.user", "wario"sv));
    // inserting wario again will throw exception
    REQUIRE_THROWS_AS(test_collection.doc().set("$.user", "wario"sv), docudb::db_exception);
}
