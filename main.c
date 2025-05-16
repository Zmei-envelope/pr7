#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>       // Для работы с директориями
#include <sys/stat.h>     // Для получения информации о файлах
#include <unistd.h>       // Для системных вызовов 
#include <wctype.h>       // Для работы с широкими символами 
#include <wchar.h>        // Для работы с широкими символами
#include <locale.h>       // Для установки локали (поддержка Unicode)
#include <sys/mman.h>     // Для работы с mmap
#include <fcntl.h>        // Для флагов открытия файлов


// Максимальная длина пути и строки
#define MAX_PATH_LEN 1024
#define MAX_LINE_LEN 4096

// Флаг игнорирования регистра символов
int ignore_case = 0;

/*
 * Поиск подстроки с поддержкой Unicode
 * Сравнение символов с учетом/без учета регистра
 * Обработка многобайтовых кодировок
 */

 int is_word_boundary(wchar_t ch) {
    return iswspace(ch) || iswpunct(ch) || ch == L'\0';
}

int strstr_unicode(const char *haystack, const char *needle, int ignore_case) {
    mbstate_t state = {0};
    const char *h = haystack;
    
    while (*h) {
        const char *h_pos = h;
        const char *n = needle;
        mbstate_t h_state = state;
        mbstate_t n_state = state;
        
        // Проверяем, что перед словом граница (начало строки или не-буква)
        if (h != haystack) {
            wchar_t prev_ch;
            const char *prev_pos = h;
            mbstate_t prev_state = {0};
            mbrtowc(&prev_ch, prev_pos, MB_CUR_MAX, &prev_state);
            if (!is_word_boundary(prev_ch)) {
                // Переходим к следующему символу
                wchar_t dummy;
                size_t advance = mbrtowc(&dummy, h, MB_CUR_MAX, &state);
                if (advance == (size_t)-1 || advance == (size_t)-2) {
                    h++;
                    state = (mbstate_t){0};
                } else {
                    h += advance;
                }
                continue;
            }
        }
        
        // Проверяем совпадение слова
        int match = 1;
        while (*h_pos && *n) {
            wchar_t h_ch, n_ch;
            size_t h_len = mbrtowc(&h_ch, h_pos, MB_CUR_MAX, &h_state);
            size_t n_len = mbrtowc(&n_ch, n, MB_CUR_MAX, &n_state);
            
            if (h_len == (size_t)-1 || h_len == (size_t)-2 ||
                n_len == (size_t)-1 || n_len == (size_t)-2) {
                return 0;
            }
            
            if (ignore_case) {
                h_ch = towlower(h_ch);
                n_ch = towlower(n_ch);
            }
            
            if (h_ch != n_ch) {
                match = 0;
                break;
            }
            
            h_pos += h_len;
            n += n_len;
        }
        
        // Проверяем, что после слова граница (конец строки или не-буква)
        if (match && *n == '\0') {
            wchar_t next_ch;
            mbstate_t next_state = {0};
            mbrtowc(&next_ch, h_pos, MB_CUR_MAX, &next_state);
            if (is_word_boundary(next_ch)) {
                return 1;
            }
        }
        
        // Переходим к следующему символу
        wchar_t dummy;
        size_t advance = mbrtowc(&dummy, h, MB_CUR_MAX, &state);
        if (advance == (size_t)-1 || advance == (size_t)-2) {
            h++;
            state = (mbstate_t){0};
        } else {
            h += advance;
        }
    }
    
    return 0;
}
/*
 * Поиск в файле с использованием отображения в память
 * Обработка строк файла
 * Вывод результатов поиска
 */
void search_in_file_mmap(const char *filename, const char *search_word) {
    // Открытие файла
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("open");
        return;
    }

    // Получение информации о файле
    struct stat st;
    if (fstat(fd, &st) == -1) {
        perror("fstat");
        close(fd);
        return;
    }

    // Проверка на пустой файл
    if (st.st_size == 0) {
        close(fd);
        return;
    }

    // Отображение файла в память
    char *map = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return;
    }

    // Инициализация переменных для обработки строк
    int line_number = 1;
    char *line_start = map;
    char *current = map;
    char *end = map + st.st_size;

    // Обработка содержимого файла
    while (current < end) {
        if (*current == '\n') {
            // Выделение строки
            size_t line_length = current - line_start;
            char line[line_length + 1];
            memcpy(line, line_start, line_length);
            line[line_length] = '\0';

            // Проверка наличия искомого слова
            if (strstr_unicode(line, search_word, ignore_case)) {
                // Вывод результата
                printf("%s:%d: %s\n", filename, line_number, line);
            }

            // Подготовка к обработке следующей строки
            line_start = current + 1;
            line_number++;
        }
        current++;
    }

    // Обработка последней строки
    if (line_start < end) {
        size_t line_length = end - line_start;
        char line[line_length + 1];
        memcpy(line, line_start, line_length);
        line[line_length] = '\0';

        if (strstr_unicode(line, search_word, ignore_case)) {
            printf("%s:%d: %s\n", filename, line_number, line);
        }
    }

    // Освобождение ресурсов
    munmap(map, st.st_size);
    close(fd);
}

/*
 * Рекурсивный обход директорий
 * Обработка всех файлов и поддиректорий
 * Включение скрытых файлов и директорий
 */
void search_in_directory(const char *dir_path, const char *search_word) {
    // Открытие директории
    DIR *dir = opendir(dir_path);
    if (!dir) {
        perror("opendir");
        return;
    }

    // Чтение содержимого директории
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Пропуск текущей и родительской директории
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Формирование полного пути
        char full_path[MAX_PATH_LEN];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        // Получение информации о файле
        struct stat st;
        if (lstat(full_path, &st) == -1) {
            perror("lstat");
            continue;
        }

        // Обработка директории
        if (S_ISDIR(st.st_mode)) {
            search_in_directory(full_path, search_word);
        } 
        // Обработка обычного файла
        else if (S_ISREG(st.st_mode)) {
            search_in_file_mmap(full_path, search_word);
        }
    }

    // Закрытие директории
    closedir(dir);
}

/*
 * Точка входа в программу
 * Обработка аргументов командной строки
 * Запуск процесса поиска
 */
int main(int argc, char *argv[]) {
    // Установка локали для поддержки Unicode
    setlocale(LC_ALL, "ru_RU.UTF-8");

    // Проверка количества аргументов
    if (argc < 2 || argc > 4) {
        fprintf(stderr, "Usage: %s [-i] <word> [directory]\n", argv[0]);
        return 1;
    }

    // Обработка флага игнорирования регистра
    int word_index = 1;
    if (argc > 2 && strcmp(argv[1], "-i") == 0) {
        ignore_case = 1;
        word_index = 2;
    }

    // Получение искомого слова и директории
    const char *search_word = argv[word_index];
    const char *directory = (word_index + 1 < argc) ? argv[word_index + 1] : ".";

    // Вывод информации о параметрах поиска
    printf("Searching for '%s' in '%s' (case %s)\n",
           search_word, directory, ignore_case ? "insensitive" : "sensitive");

    // Запуск поиска
    search_in_directory(directory, search_word);

    return 0;
}
