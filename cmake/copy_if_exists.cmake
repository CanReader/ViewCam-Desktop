# copy_if_exists.cmake — Copies a file only if the source exists.
# Usage: cmake -Dsrc=<source> -Ddst=<destination> -P copy_if_exists.cmake

if(EXISTS "${src}")
    file(COPY "${src}" DESTINATION "${dst}")
    message(STATUS "Copied: ${src} -> ${dst}")
else()
    message(STATUS "Skipped (not found): ${src}")
endif()
