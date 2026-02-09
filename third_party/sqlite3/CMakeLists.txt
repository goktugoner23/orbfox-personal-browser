add_library(sqlite3 STATIC sqlite3.c)
target_include_directories(sqlite3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# SQLite3 compile options for Windows
if(MSVC)
    target_compile_options(sqlite3 PRIVATE /W0)
endif()

# Create alias to match FindSQLite3 target name
add_library(SQLite::SQLite3 ALIAS sqlite3)
