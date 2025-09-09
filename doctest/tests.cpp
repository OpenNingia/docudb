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

    auto docs = test_collection.find(docudb::query::like("$.text", "%world%"));

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
    auto my_doc_again = test_collection.find(docudb::query::like("$.text", R"(%universe%)")).front().doc();
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
    auto my_doc_again = test_collection.find(docudb::query::eq("$.new_key", "new value")).front().doc();
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
    auto result = test_collection.find(docudb::query::eq("$.text", "new value"));
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
    auto my_doc_again = test_collection.find(docudb::query::eq("$.text", "new value")).front().doc();
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
    auto result = test_collection.find(docudb::query::eq("$.new_key", "new value"));
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

// Verify that a document can be deleted and is no longer retrievable.
TEST_CASE("Delete a document")
{
    docudb::database db{":memory:"};
    auto coll = db.collection("delete_test");
    auto doc = coll.doc().set("$.value", 123);
    auto docid = doc.id();

    REQUIRE(coll.find(docudb::query::eq("$.value", 123)).size() == 1);

    doc.erase();
    REQUIRE(coll.find(docudb::query::eq("$.value", 123)).empty());
}

// Check that multiple documents can be inserted and queried correctly.
TEST_CASE("Insert multiple documents and query")
{
    docudb::database db{":memory:"};
    auto coll = db.collection("multi_insert");
    coll.doc().set("$.user", "alice"sv).set("$.score", 10);
    coll.doc().set("$.user", "bob"sv).set("$.score", 20);

    auto result = coll.find(docudb::query::eq("$.user", "bob"));
    REQUIRE(result.size() == 1);
    REQUIRE(result.front().doc().get_number("$.score") == 20);
}

// Test greater-than, less-than, and ranges.
TEST_CASE("Query with comparison operators")
{
    docudb::database db{":memory:"};
    auto coll = db.collection("comparison");
    coll.doc().set("$.score", 5);
    coll.doc().set("$.score", 15);
    coll.doc().set("$.score", 25);

    auto gt10 = coll.find(docudb::query::gt("$.score", 10));
    REQUIRE(gt10.size() == 2);

    auto lt20 = coll.find(docudb::query::lt("$.score", 20));
    REQUIRE(lt20.size() == 2);
}

// Insert and query nested JSON documents.
TEST_CASE("Nested JSON querying")
{
    docudb::database db{":memory:"};
    auto coll = db.collection("nested");
    coll.doc().set("$.user.name", "alice"sv).set("$.user.age", 30);

    auto result = coll.find(docudb::query::eq("$.user.name", "alice"));
    REQUIRE(result.size() == 1);
    REQUIRE(result.front().doc().get_number("$.user.age") == 30);
}

// Ensure correct handling of JSON null and missing fields.
TEST_CASE("Null and missing field handling")
{
    docudb::database db{":memory:"};
    auto coll = db.collection("null_test");
    coll.doc().set("$.field", nullptr);

    auto result = coll.find(docudb::query::eq("$.field", nullptr));
    REQUIRE(result.size() == 1);

    // Query for missing field
    coll.doc().set("$.other", 123);
    auto missing = coll.find(docudb::query::eq("$.field", nullptr));
    REQUIRE(missing.size() == 1); // Only the document with explicit null should match
}

// Test creating an index on a nested key and querying it.
TEST_CASE("Index on nested key")
{
    docudb::database db{":memory:"};
    auto coll = db.collection("index_nested");
    REQUIRE_NOTHROW(coll.index("nested_idx", "$.user.name"));

    coll.doc().set("$.user.name", "bob"sv);
    auto result = coll.find(docudb::query::eq("$.user.name", "bob"));
    REQUIRE(result.size() == 1);
}

// Test updating a portion of a document without overwriting the whole document.
TEST_CASE("Partial update of a document")
{
    docudb::database db{":memory:"};
    auto coll = db.collection("partial_update");
    auto doc = coll.doc().set("$.a", 1).set("$.b", 2);
    auto id = doc.id();

    doc.set("$.b", 3);
    auto updated = coll.doc(id);
    REQUIRE(updated.get_number("$.a") == 1);
    REQUIRE(updated.get_number("$.b") == 3);
}

TEST_CASE("Large document handling")
{
    docudb::database db{":memory:"};
    auto coll = db.collection("large_doc");
    std::string big_string(10000, 'x');
    coll.doc().set("$.big", big_string);

    auto result = coll.find(docudb::query::like("$.big", "%xxx%"));
    REQUIRE(result.size() == 1);
}

TEST_CASE("db_collection::count() returns correct document count")
{
    docudb::database db{":memory:"};
    auto coll = db.collection("count_test");

    // Initially, collection should be empty
    REQUIRE(coll.count() == 0);

    // Insert one document
    coll.doc().set("$.a", 1);
    REQUIRE(coll.count() == 1);

    // Insert another document
    coll.doc().set("$.b", 2);
    REQUIRE(coll.count() == 2);

    // Remove one document and check count
    auto docs = coll.docs();
    REQUIRE(docs.size() == 2);
    docs.front().erase();
    REQUIRE(coll.count() == 1);

    // Remove the last document
    docs.back().erase();
    REQUIRE(coll.count() == 0);
}

TEST_CASE("db_collection::count(query) returns correct count for filtered queries")
{
    docudb::database db{":memory:"};
    auto coll = db.collection("count_query_test");

    // Insert documents with different fields
    coll.doc().set("$.type", "fruit"sv).set("$.name", "apple"sv);
    coll.doc().set("$.type", "fruit"sv).set("$.name", "banana"sv);
    coll.doc().set("$.type", "vegetable"sv).set("$.name", "carrot"sv);
    coll.doc().set("$.type", "fruit"sv).set("$.name", "pear"sv);

    // Count all fruits
    auto fruit_count = coll.count(docudb::query::eq("$.type", "fruit"));
    REQUIRE(fruit_count == 3);

    // Count all vegetables
    auto veg_count = coll.count(docudb::query::eq("$.type", "vegetable"));
    REQUIRE(veg_count == 1);

    // Count all apples
    auto apple_count = coll.count(docudb::query::eq("$.name", "apple"));
    REQUIRE(apple_count == 1);

    // Count with a query that matches nothing
    auto none_count = coll.count(docudb::query::eq("$.name", "potato"));
    REQUIRE(none_count == 0);
}

TEST_CASE("db_collection::find(query, order_by, limit) supports ordering and limiting")
{
    docudb::database db{":memory:"};
    auto coll = db.collection("find_order_limit");

    // Insert documents with a numeric field
    coll.doc().set("$.value", 10);
    coll.doc().set("$.value", 30);
    coll.doc().set("$.value", 20);

    // Query all, order by value ascending
    auto ordered_asc = coll.find(docudb::query::gt("$.value", 0), docudb::query::order_by("$.value"), std::nullopt);
    REQUIRE(ordered_asc.size() == 3);
    REQUIRE(ordered_asc[0].doc().get_number("$.value") == 10);
    REQUIRE(ordered_asc[1].doc().get_number("$.value") == 20);
    REQUIRE(ordered_asc[2].doc().get_number("$.value") == 30);

    // Query all, order by value descending, limit 2
    auto ordered_desc_limit = coll.find(docudb::query::gt("$.value", 0), docudb::query::order_by("$.value", false), 2);
    REQUIRE(ordered_desc_limit.size() == 2);
    REQUIRE(ordered_desc_limit[0].doc().get_number("$.value") == 30);
    REQUIRE(ordered_desc_limit[1].doc().get_number("$.value") == 20);
}

TEST_CASE("db_collection::index(name, columns, unique) creates multi-column index and enforces uniqueness")
{
    docudb::database db{":memory:"};
    auto coll = db.collection("index_multi_test");

    // Insert documents with two fields
    coll.doc().set("$.a", 1).set("$.b", 10);
    coll.doc().set("$.a", 2).set("$.b", 20);

    // Create a unique index on both fields
    std::vector<std::pair<std::string, std::string>> columns = {
        {"a_idx", "$.a"},
        {"b_idx", "$.b"}
    };
    REQUIRE_NOTHROW(coll.index("multi_idx", columns, true));

    // Inserting a document with the same a and b should fail due to unique constraint
    auto doc = coll.doc();
    REQUIRE_THROWS_AS(doc.set("$.a", 1).set("$.b", 10);, docudb::db_exception); // Should throw on duplicate

    // Inserting a document with a unique combination should succeed
    REQUIRE_NOTHROW(coll.doc().set("$.a", 3).set("$.b", 30));
}

TEST_CASE("db_document::replace() overloads replace values of different types")
{
    docudb::database db{":memory:"};
    auto coll = db.collection("replace_overloads");
    auto doc = coll.doc().set("$.str", "hello"sv).set("$.num", 42).set("$.real", 3.14).set("$.nullval", nullptr);

    // Replace string value
    doc.replace("$.str", "world"sv);
    REQUIRE(doc.get_string("$.str") == "world");

    // Replace integer value
    doc.replace("$.num", 100);
    REQUIRE(doc.get_number("$.num") == 100);

    // Replace floating point value (float)
    doc.replace("$.real", static_cast<float>(2.71));
    REQUIRE(doc.get_real("$.real") == doctest::Approx(2.71));

    // Replace floating point value (double)
    doc.replace("$.real", 1.618);
    REQUIRE(doc.get_real("$.real") == doctest::Approx(1.618));

    // Replace with null
    doc.replace("$.real", nullptr);
    REQUIRE(doc.get_type("$.real") == docudb::json_type::null);

    // Replace a non-existing key (should be a NOOP)
    doc.replace("$.does_not_exist", 12345);
    REQUIRE_THROWS_AS(doc.get_number("$.does_not_exist"), docudb::db_exception);
}

TEST_CASE("db_document::patch applies partial updates from JSON")
{
    docudb::database db{":memory:"};
    auto coll = db.collection("patch_test");
    auto doc = coll.doc().set("$.a", 1).set("$.b", 2);

    // Patch with new and existing fields
    doc.patch(R"({"a": 10, "c": 3})");

    // Existing field 'a' should be updated, 'b' unchanged, 'c' added
    REQUIRE(doc.get_number("$.a") == 10);
    REQUIRE(doc.get_number("$.b") == 2);
    REQUIRE(doc.get_number("$.c") == 3);

    // Patch with nested object
    doc.patch(R"({"nested": {"x": 42}})");
    REQUIRE(doc.get_number("$.nested.x") == 42);

    // Patch with null (deletes the field)
    doc.patch(R"({"a": null})");
    REQUIRE(doc.get_type("$.a") == docudb::json_type::not_found);
}

TEST_CASE("db_document::get(fields) returns tuple of requested types and fields")
{
    docudb::database db{":memory:"};
    auto coll = db.collection("tuple_get_test");
    auto doc = coll.doc()
        .set("$.a", 42)
        .set("$.b", 3.14)
        .set("$.c", "hello"sv);

    // Retrieve as tuple: int, double, string
    auto result = doc.get<std::int64_t, double, std::string>({"$.a", "$.b", "$.c"});

    
    REQUIRE(std::get<0>(result) == 42);
    REQUIRE(std::get<1>(result) == doctest::Approx(3.14));
    REQUIRE(std::get<2>(result) == "hello");
    
    // Retrieve as tuple: string, int
    auto result2 = doc.get<std::string, std::int64_t>({"$.c", "$.a"});

    
    REQUIRE(std::get<0>(result2) == "hello");
    REQUIRE(std::get<1>(result2) == 42);

    // Throws if field count/type mismatch
    REQUIRE_THROWS(doc.get<std::int64_t, std::string>({"$.a"}));
}

TEST_CASE("db_document::get_array_length returns correct length for arrays")
{
    docudb::database db{":memory:"};
    auto coll = db.collection("array_length_test");

    // Create a document with an array field
    auto doc = coll.doc().patch(R"({"arr": [1,2,3,4]})");

    REQUIRE(doc.get_array_length("$.arr") == 4);

    // Nested array
    doc.patch(R"({"nested": {"a":[10,20]}})");
    REQUIRE(doc.get_array_length("$.nested.a") == 2);

    // Empty array
    doc.patch(R"({"empty": []})");
    REQUIRE(doc.get_array_length("$.empty") == 0);

    // Not an array: should throw or return 0 (depending on your implementation)
    REQUIRE_THROWS_AS(doc.get_array_length("$.not_an_array"), docudb::db_exception);
}

TEST_CASE("db_document::get_object_keys returns all keys of an object at the given path")
{
    docudb::database db{":memory:"};
    auto coll = db.collection("object_keys_test");
    auto doc = coll.doc().patch(R"({"obj": {"a": 1, "b": 2, "c": 3}, "empty": {}})");

    // Get keys for "obj"
    auto keys = doc.get_object_keys("$.obj");
    std::sort(keys.begin(), keys.end());
    REQUIRE(keys == std::vector<std::string>({"a", "b", "c"}));

    // Get keys for "empty"
    auto empty_keys = doc.get_object_keys("$.empty");
    REQUIRE(empty_keys.empty());

    // Not an object: should throw
    REQUIRE_THROWS_AS(doc.get_object_keys("$.obj.a"), docudb::db_exception);
}