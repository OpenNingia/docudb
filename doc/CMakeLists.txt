find_program(STANDARDESE NAMES standardese)

if (STANDARDESE-NOTFOUND)
    message("Standardese not found")
endif(STANDARDESE-NOTFOUND)

if (STANDARDESE)
    add_custom_target(
        docudb_docs 
        ALL
        COMMAND ${STANDARDESE} --config ${CMAKE_SOURCE_DIR}/standardese.config ${CMAKE_SOURCE_DIR}/src/*
        SOURCES 
        ${CMAKE_SOURCE_DIR}/standardese.config
        ${CMAKE_SOURCE_DIR}/src/docudb.hpp
        COMMENT "Building documentation with standardese"
        )
endif()