cmake_minimum_required(VERSION 3.10)
project(unicode_search C)

# Установка стандарта C11 и включение всех предупреждений
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

# Проверка необходимых заголовочных файлов
include(CheckIncludeFiles)
check_include_files("dirent.h" HAVE_DIRENT_H)
check_include_files("sys/stat.h" HAVE_SYS_STAT_H)
check_include_files("unistd.h" HAVE_UNISTD_H)
check_include_files("wctype.h" HAVE_WCTYPE_H)
check_include_files("wchar.h" HAVE_WCHAR_H)
check_include_files("locale.h" HAVE_LOCALE_H)
check_include_files("sys/mman.h" HAVE_SYS_MMAN_H)
check_include_files("fcntl.h" HAVE_FCNTL_H)

if(NOT HAVE_DIRENT_H OR NOT HAVE_SYS_STAT_H OR NOT HAVE_UNISTD_H OR 
   NOT HAVE_WCTYPE_H OR NOT HAVE_WCHAR_H OR NOT HAVE_LOCALE_H OR
   NOT HAVE_SYS_MMAN_H OR NOT HAVE_FCNTL_H)
    message(FATAL_ERROR "Required headers not found")
endif()

# Проверка необходимых функций
include(CheckFunctionExists)
check_function_exists(mbrtowc HAVE_MBRTOWC)
check_function_exists(towlower HAVE_TOWLOWER)
check_function_exists(mmap HAVE_MMAP)
check_function_exists(munmap HAVE_MUNMAP)

if(NOT HAVE_MBRTOWC OR NOT HAVE_TOWLOWER OR NOT HAVE_MMAP OR NOT HAVE_MUNMAP)
    message(FATAL_ERROR "Required functions not found")
endif()

# Добавление исполняемого файла
add_executable(searcher
    main.c
)

# Установка свойств для поддержки Unicode
if(UNIX)
    target_compile_definitions(searcher PRIVATE _GNU_SOURCE)
    target_link_libraries(searcher m)
endif()

# Установка параметров установки
install(TARGETS searcher DESTINATION bin)
