#include <benchmark/benchmark.h>
#include <docudb.hpp>
#include <random>
#include <string>

using namespace std::string_view_literals;

void AddEmptyDocument(benchmark::State &state)
{
    // initialize database
    docudb::database db{":memory:"};
    // initialize collection
    auto test_collection = db.collection("test_collection");

    while (state.KeepRunning())
    {
        benchmark::DoNotOptimize(
            test_collection.doc());
    }
}

BENCHMARK(AddEmptyDocument);

void AddDocument(benchmark::State &state)
{
    // initialize database
    docudb::database db{":memory:"};
    // initialize collection
    auto test_collection = db.collection("test_collection");

    while (state.KeepRunning())
    {
        benchmark::DoNotOptimize(
            test_collection.doc()
                .set("$.text", "Hello World"sv)
                .set("$.int", 42)
                .set("$.real", 42.42));
    }
}

BENCHMARK(AddDocument);

std::string GenerateRandomUsername()
{
    const char charset[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789";
    const size_t max_index = (sizeof(charset) - 1);
    std::default_random_engine rng(std::random_device{}());
    std::uniform_int_distribution<> dist(0, max_index);

    std::string username;
    for (size_t i = 0; i < 8; ++i)
    {
        username += charset[dist(rng)];
    }
    return username;
}

std::string GenerateRandomPassword(std::size_t length)
{
    const char charset[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789"
        "!@#$%^&*()_+-=[]{}|;:,.<>?";
    const size_t max_index = (sizeof(charset) - 1);
    std::default_random_engine rng(std::random_device{}());
    std::uniform_int_distribution<> dist(0, max_index);

    std::string password;
    for (size_t i = 0; i < length; ++i)
    {
        password += charset[dist(rng)];
    }
    return password;
}

void FillCollection(docudb::db_collection &collection, std::size_t size)
{
    for (std::size_t i = 0; i < size; i++)
    {
        collection.doc()
            .set("$.user", GenerateRandomUsername())
            .set("$.password", GenerateRandomPassword(16));
    }
}

void SearchNoIndex(benchmark::State &state)
{
    // initialize database
    docudb::database db{":memory:"};

    auto collection = db.collection("test");
    FillCollection(collection, state.range_x());

    while (state.KeepRunning())
    {
        benchmark::DoNotOptimize(
            collection.where("$.user", docudb::ops::eq("wario")));
    }
}

BENCHMARK(SearchNoIndex)->Arg(10)->Arg(100)->Arg(1000)->Arg(10000);

void SearchWithIndex(benchmark::State &state)
{
    // initialize database
    docudb::database db{":memory:"};

    auto collection = db.collection("test");
    FillCollection(collection, state.range_x());
    collection.index("user"sv, "$.user"sv);

    while (state.KeepRunning())
    {
        benchmark::DoNotOptimize(
            collection.where("user", docudb::ops::eq("wario")));
    }
}

BENCHMARK(SearchWithIndex)->Arg(10)->Arg(100)->Arg(1000)->Arg(10000);

BENCHMARK_MAIN();
