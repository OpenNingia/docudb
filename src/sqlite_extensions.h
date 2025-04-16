#pragma once

#include <sqlite3.h>

#ifndef SQLITE_EXT_H
#define SQLITE_EXT_H

    void sqlite_regexp_func(sqlite3_context* context, int argc, sqlite3_value** argv);

#endif //SQLITE_EXT_H