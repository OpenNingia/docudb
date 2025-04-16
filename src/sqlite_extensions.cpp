#include "sqlite_extensions.h"
#include <regex>
#include <string>

using namespace std::string_literals;

/**
 * @brief Implements the REGEXP operator for SQLite.
 *        Called by SQLite when expr REGEXP pattern is evaluated.
 * @param context SQLite function context (for setting result).
 * @param argc Number of arguments (should be 2).
 * @param argv Array of SQLite value pointers (argv[0]=pattern, argv[1]=text).
 */
void sqlite_regexp_func(sqlite3_context* context, int argc, sqlite3_value** argv) {
    if (argc != 2) {
        const char* errorMsg = "REGEXP requires exactly two arguments.";
        sqlite3_result_error(context, errorMsg, -1); // Use -1 for length to let SQLite calculate
        return;
    }

    // Get pattern (arg 0) and text (arg 1) as C strings
    // Use sqlite3_value_type to check for NULLs first
    const unsigned char* pattern_uch = sqlite3_value_text(argv[0]);
    const unsigned char* text_uch = sqlite3_value_text(argv[1]);

    // If either pattern or text is NULL, the result of REGEXP is usually NULL or false (0)
    // Let's return false (0) which seems safer in a boolean context.
    if (!pattern_uch || !text_uch) {
        sqlite3_result_int(context, 0); // Return 0 (false) for NULL inputs
        return;
    }

    const char* pattern = reinterpret_cast<const char*>(pattern_uch);
    const char* text = reinterpret_cast<const char*>(text_uch);

    try {
        // Use C++ std::regex for the matching logic
        // NOTE: std::regex syntax is often ECMAScript by default. SQLite users
        // might expect POSIX ERE or other syntax. Sticking to default for now.
        std::regex regex_pattern(pattern); // Compile the pattern

        // Use std::regex_search to see if the pattern matches anywhere in the text
        bool match_found = std::regex_search(text, regex_pattern);

        // Set the SQLite result: 1 for match, 0 for no match
        sqlite3_result_int(context, match_found ? 1 : 0);

    } catch (const std::regex_error& e) {
        // Handle invalid regex patterns provided in the query
        std::string errorMsg = "REGEXP pattern error: "s + e.what();
        sqlite3_result_error(context, errorMsg.c_str(), errorMsg.length());
    } catch (const std::exception& e) {
        // Handle other unexpected errors
        std::string errorMsg = "REGEXP unexpected error: "s + e.what();
        sqlite3_result_error(context, errorMsg.c_str(), errorMsg.length());
    }
}