// Fahri Yigit Yoru 241AMC029
// https://github.com/fahriyigit/calc-lab-home
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: ChatGPT (OpenAI) - guidance, debugging, parser design,
// command-line handling, dynamic input, and float/integer arithmetic.
//
// AI (ChatGPT) was used to help design this final Grade 10 version:
// dynamic line reading, recursive-descent parsing, integer/float value tracking,
// right-associative exponentiation, exact error positions, CLI handling,
// directory processing, and large-file-safe output handling.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

#define FIRST_NAME "fahri_yigit"
#define LAST_NAME "yoru"
#define STUDENT_ID "241AMC029"

typedef struct {
    int is_float;
    long long i;
    double d;
} Value;

typedef struct {
    const char *text;
    size_t index;
    size_t base_position;
    int error;
    size_t error_position;
} Parser;

static Value make_int(long long x)
{
    Value v;
    v.is_float = 0;
    v.i = x;
    v.d = (double)x;
    return v;
}

static Value make_float(double x)
{
    Value v;
    v.is_float = 1;
    v.i = 0;
    v.d = x;
    return v;
}

static double as_double(Value v)
{
    return v.is_float ? v.d : (double)v.i;
}

static void skip_spaces(Parser *p)
{
    while (p->text[p->index] == ' ' ||
           p->text[p->index] == '\t') {
        p->index++;
    }
}

static size_t current_position(Parser *p)
{
    return p->base_position + p->index;
}

static void set_error(Parser *p, size_t position)
{
    if (!p->error) {
        p->error = 1;
        p->error_position = position;
    }
}

static int at_end_of_line(Parser *p)
{
    char c = p->text[p->index];

    return c == '\0' ||
           c == '\n' ||
           c == '\r';
}

/*
 * Grammar:
 *
 * expression := term { ('+' | '-') term }
 * term       := power { ('*' | '/') power }
 * power      := primary [ '**' power ]
 * primary    := NUMBER | '(' expression ')'
 *
 * power() recursively parses its right operand.
 * This makes exponentiation right-associative:
 *
 * 2 ** 3 ** 2
 * becomes
 * 2 ** (3 ** 2)
 */
static int parse_expression(Parser *p, Value *result);

static int parse_number(Parser *p, Value *value)
{
    skip_spaces(p);

    size_t start = p->index;

    if (!isdigit((unsigned char)p->text[p->index])) {
        set_error(p, current_position(p));
        return 0;
    }

    while (isdigit((unsigned char)p->text[p->index])) {
        p->index++;
    }

    int is_float = 0;

    if (p->text[p->index] == '.') {
        size_t dot_index = p->index;

        is_float = 1;
        p->index++;

        if (!isdigit((unsigned char)p->text[p->index])) {
            set_error(
                p,
                p->base_position + dot_index
            );

            return 0;
        }

        while (isdigit((unsigned char)p->text[p->index])) {
            p->index++;
        }
    }

    if (is_float) {
        errno = 0;

        char *endptr = NULL;

        double x = strtod(
            p->text + start,
            &endptr
        );

        (void)endptr;

        if (errno == ERANGE ||
            !isfinite(x)) {

            set_error(
                p,
                p->base_position + start
            );

            return 0;
        }

        *value = make_float(x);
    } else {
        errno = 0;

        char *endptr = NULL;

        long long x = strtoll(
            p->text + start,
            &endptr,
            10
        );

        (void)endptr;

        if (errno == ERANGE) {
            set_error(
                p,
                p->base_position + start
            );

            return 0;
        }

        *value = make_int(x);
    }

    return 1;
}

static int parse_primary(Parser *p, Value *value)
{
    skip_spaces(p);

    if (p->text[p->index] == '(') {
        p->index++;

        Value inside;

        if (!parse_expression(
            p,
            &inside
        )) {
            return 0;
        }

        skip_spaces(p);

        if (p->text[p->index] != ')') {
            set_error(
                p,
                current_position(p)
            );

            return 0;
        }

        p->index++;

        *value = inside;
        return 1;
    }

    return parse_number(p, value);
}

static long long integer_power(
    long long base,
    long long exponent
)
{
    long long result = 1;

    while (exponent > 0) {
        if (exponent & 1LL) {
            result *= base;
        }

        exponent >>= 1;

        if (exponent > 0) {
            base *= base;
        }
    }

    return result;
}

static int parse_power(Parser *p, Value *result)
{
    Value lhs;

    if (!parse_primary(p, &lhs)) {
        return 0;
    }

    skip_spaces(p);

    if (p->text[p->index] == '*' &&
        p->text[p->index + 1] == '*') {

        size_t operator_position =
            current_position(p);

        p->index += 2;

        skip_spaces(p);

        size_t exponent_position =
            current_position(p);

        if (at_end_of_line(p)) {
            set_error(
                p,
                exponent_position
            );

            return 0;
        }

        if (p->text[p->index] == '+' ||
            p->text[p->index] == '-' ||
            p->text[p->index] == '*' ||
            p->text[p->index] == '/') {

            set_error(
                p,
                current_position(p)
            );

            return 0;
        }

        Value exponent;

        if (!parse_power(
            p,
            &exponent
        )) {
            return 0;
        }

        if (!exponent.is_float &&
            exponent.i < 0) {

            set_error(
                p,
                exponent_position
            );

            return 0;
        }

        if (!lhs.is_float &&
            !exponent.is_float) {

            lhs = make_int(
                integer_power(
                    lhs.i,
                    exponent.i
                )
            );
        } else {
            double x = pow(
                as_double(lhs),
                as_double(exponent)
            );

            if (!isfinite(x)) {
                set_error(
                    p,
                    operator_position
                );

                return 0;
            }

            lhs = make_float(x);
        }
    }

    *result = lhs;
    return 1;
}

static int parse_term(Parser *p, Value *result)
{
    Value lhs;

    if (!parse_power(p, &lhs)) {
        return 0;
    }

    while (1) {
        skip_spaces(p);

        char op = p->text[p->index];

        if (op == '*' &&
            p->text[p->index + 1] == '*') {
            break;
        }

        if (op != '*' &&
            op != '/') {
            break;
        }

        p->index++;

        skip_spaces(p);

        size_t rhs_position =
            current_position(p);

        if (at_end_of_line(p)) {
            set_error(
                p,
                rhs_position
            );

            return 0;
        }

        if (p->text[p->index] == '+' ||
            p->text[p->index] == '-' ||
            p->text[p->index] == '*' ||
            p->text[p->index] == '/') {

            set_error(
                p,
                current_position(p)
            );

            return 0;
        }

        Value rhs;

        if (!parse_power(
            p,
            &rhs
        )) {
            return 0;
        }

        if (op == '*') {

            if (!lhs.is_float &&
                !rhs.is_float) {

                lhs = make_int(
                    lhs.i * rhs.i
                );
            } else {
                double x =
                    as_double(lhs) *
                    as_double(rhs);

                if (!isfinite(x)) {
                    set_error(
                        p,
                        rhs_position
                    );

                    return 0;
                }

                lhs = make_float(x);
            }
        } else {
            int zero =
                rhs.is_float
                ? (rhs.d == 0.0)
                : (rhs.i == 0);

            if (zero) {
                set_error(
                    p,
                    rhs_position
                );

                return 0;
            }

            if (!lhs.is_float &&
                !rhs.is_float) {

                lhs = make_int(
                    lhs.i / rhs.i
                );
            } else {
                double x =
                    as_double(lhs) /
                    as_double(rhs);

                if (!isfinite(x)) {
                    set_error(
                        p,
                        rhs_position
                    );

                    return 0;
                }

                lhs = make_float(x);
            }
        }
    }

    *result = lhs;
    return 1;
}

static int parse_expression(Parser *p, Value *result)
{
    Value lhs;

    if (!parse_term(p, &lhs)) {
        return 0;
    }

    while (1) {
        skip_spaces(p);

        char op = p->text[p->index];

        if (op == ')' ||
            at_end_of_line(p)) {
            break;
        }

        if (op == '#') {
            set_error(
                p,
                current_position(p)
            );

            return 0;
        }

        if (op != '+' &&
            op != '-') {

            set_error(
                p,
                current_position(p)
            );

            return 0;
        }

        p->index++;

        skip_spaces(p);

        if (at_end_of_line(p)) {
            set_error(
                p,
                current_position(p)
            );

            return 0;
        }

        if (p->text[p->index] == '+' ||
            p->text[p->index] == '-' ||
            p->text[p->index] == '*' ||
            p->text[p->index] == '/') {

            set_error(
                p,
                current_position(p)
            );

            return 0;
        }

        Value rhs;

        if (!parse_term(
            p,
            &rhs
        )) {
            return 0;
        }

        if (!lhs.is_float &&
            !rhs.is_float) {

            if (op == '+') {
                lhs = make_int(
                    lhs.i + rhs.i
                );
            } else {
                lhs = make_int(
                    lhs.i - rhs.i
                );
            }
        } else {
            double x;

            if (op == '+') {
                x =
                    as_double(lhs) +
                    as_double(rhs);
            } else {
                x =
                    as_double(lhs) -
                    as_double(rhs);
            }

            if (!isfinite(x)) {
                set_error(
                    p,
                    current_position(p)
                );

                return 0;
            }

            lhs = make_float(x);
        }
    }

    *result = lhs;
    return 1;
}

static int is_blank_line(const char *line)
{
    while (*line != '\0') {
        if (!isspace(
            (unsigned char)*line
        )) {
            return 0;
        }

        line++;
    }

    return 1;
}

static int is_comment_line(const char *line)
{
    size_t i = 0;

    while (line[i] == ' ' ||
           line[i] == '\t') {
        i++;
    }

    return line[i] == '#';
}

/*
 * ChatGPT helped with the realloc-doubling approach below.
 * It avoids a fixed maximum line length and does not repeatedly
 * copy the entire input file.
 */
static int read_line_dynamic(
    FILE *file,
    char **buffer,
    size_t *capacity,
    size_t *length
)
{
    if (*buffer == NULL ||
        *capacity == 0) {

        *capacity = 256;

        *buffer = malloc(
            *capacity
        );

        if (*buffer == NULL) {
            return -1;
        }
    }

    *length = 0;

    int ch;

    while ((ch = fgetc(file)) != EOF) {

        if (*length + 1 >= *capacity) {

            size_t new_capacity =
                *capacity * 2;

            char *new_buffer =
                realloc(
                    *buffer,
                    new_capacity
                );

            if (new_buffer == NULL) {
                return -1;
            }

            *buffer = new_buffer;
            *capacity = new_capacity;
        }

        (*buffer)[(*length)++] =
            (char)ch;

        if (ch == '\n') {
            break;
        }
    }

    if (ch == EOF &&
        *length == 0) {
        return 0;
    }

    (*buffer)[*length] = '\0';

    return 1;
}

static const char *get_base_name(
    const char *path
)
{
    const char *slash =
        strrchr(path, '/');

    if (slash != NULL) {
        return slash + 1;
    }

    return path;
}

static void get_name_without_extension(
    const char *path,
    char *output,
    size_t output_size
)
{
    const char *filename =
        get_base_name(path);

    snprintf(
        output,
        output_size,
        "%s",
        filename
    );

    char *dot =
        strrchr(output, '.');

    if (dot != NULL) {
        *dot = '\0';
    }
}

static void make_output_name(
    const char *input_path,
    char *output,
    size_t output_size
)
{
    char base[256];

    get_name_without_extension(
        input_path,
        base,
        sizeof(base)
    );

    snprintf(
        output,
        output_size,
        "%s_%s_%s_%s.txt",
        base,
        FIRST_NAME,
        LAST_NAME,
        STUDENT_ID
    );
}

static int ensure_directory(
    const char *path
)
{
    if (mkdir(path, 0777) == 0) {
        return 0;
    }

    if (errno == EEXIST) {

        struct stat st;

        if (stat(path, &st) == 0 &&
            S_ISDIR(st.st_mode)) {
            return 0;
        }
    }

    fprintf(
        stderr,
        "Cannot create output directory: %s\n",
        path
    );

    return 1;
}

static int write_value(
    FILE *output,
    Value value
)
{
    if (value.is_float) {

        return fprintf(
            output,
            "%.15g\n",
            value.d
        ) < 0;
    }

    return fprintf(
        output,
        "%lld\n",
        value.i
    ) < 0;
}

static int process_file(
    const char *input_path,
    const char *output_dir
)
{
    FILE *input =
        fopen(input_path, "r");

    if (input == NULL) {
        fprintf(
            stderr,
            "Cannot open input file: %s\n",
            input_path
        );

        return 1;
    }

    if (ensure_directory(
        output_dir
    ) != 0) {

        fclose(input);
        return 1;
    }

    char output_filename[512];
    char output_path[1024];

    make_output_name(
        input_path,
        output_filename,
        sizeof(output_filename)
    );

    snprintf(
        output_path,
        sizeof(output_path),
        "%s/%s",
        output_dir,
        output_filename
    );

    FILE *output =
        fopen(output_path, "w");

    if (output == NULL) {
        fprintf(
            stderr,
            "Cannot create output file: %s\n",
            output_path
        );

        fclose(input);
        return 1;
    }

    char *line = NULL;

    size_t capacity = 0;
    size_t line_length = 0;
    size_t file_position = 1;

    int file_error = 0;

    size_t first_error_position = 0;

    int read_status;

    while (
        (read_status =
            read_line_dynamic(
                input,
                &line,
                &capacity,
                &line_length
            )
        ) == 1
    ) {

        if (!is_blank_line(line) &&
            !is_comment_line(line)) {

            Parser parser;

            parser.text = line;
            parser.index = 0;
            parser.base_position =
                file_position;
            parser.error = 0;
            parser.error_position = 0;

            Value value;

            if (!parse_expression(
                &parser,
                &value
            )) {

                file_error = 1;

                first_error_position =
                    parser.error_position;

                break;
            }

            skip_spaces(&parser);

            if (!at_end_of_line(
                &parser
            )) {

                file_error = 1;

                first_error_position =
                    current_position(
                        &parser
                    );

                break;
            }

            if (write_value(
                output,
                value
            )) {

                fprintf(
                    stderr,
                    "Cannot write output file: %s\n",
                    output_path
                );

                free(line);
                fclose(input);
                fclose(output);

                return 1;
            }
        }

        /*
         * The absolute position counts every character
         * in the file, including newline characters and
         * characters in comments.
         */
        file_position += line_length;
    }

    if (read_status == -1) {
        fprintf(
            stderr,
            "Out of memory while reading: %s\n",
            input_path
        );

        free(line);
        fclose(input);
        fclose(output);

        return 1;
    }

    free(line);
    fclose(input);

    /*
     * Results are written as they are parsed, so large files
     * do not require storing every result in memory.
     *
     * If a later expression has an error, reopen the output
     * in "w" mode so all earlier partial results are discarded.
     */
    if (file_error) {

        fclose(output);

        output =
            fopen(output_path, "w");

        if (output == NULL) {
            fprintf(
                stderr,
                "Cannot rewrite output file: %s\n",
                output_path
            );

            return 1;
        }

        fprintf(
            output,
            "ERROR:%zu\n",
            first_error_position
        );
    }

    if (fclose(output) != 0) {

        fprintf(
            stderr,
            "Cannot finish output file: %s\n",
            output_path
        );

        return 1;
    }

    return 0;
}

static int has_txt_extension(
    const char *name
)
{
    size_t length =
        strlen(name);

    if (length < 4) {
        return 0;
    }

    return strcmp(
        name + length - 4,
        ".txt"
    ) == 0;
}

static int process_directory(
    const char *directory_path,
    const char *output_dir
)
{
    DIR *directory =
        opendir(directory_path);

    if (directory == NULL) {
        fprintf(
            stderr,
            "Cannot open input directory: %s\n",
            directory_path
        );

        return 1;
    }

    if (ensure_directory(
        output_dir
    ) != 0) {

        closedir(directory);
        return 1;
    }

    struct dirent *entry;

    int status = 0;

    while (
        (entry = readdir(directory))
        != NULL
    ) {

        if (!has_txt_extension(
            entry->d_name
        )) {
            continue;
        }

        char input_path[1024];

        snprintf(
            input_path,
            sizeof(input_path),
            "%s/%s",
            directory_path,
            entry->d_name
        );

        struct stat st;

        if (stat(
            input_path,
            &st
        ) != 0) {

            fprintf(
                stderr,
                "Cannot access input file: %s\n",
                input_path
            );

            status = 1;
            continue;
        }

        if (!S_ISREG(
            st.st_mode
        )) {
            continue;
        }

        if (process_file(
            input_path,
            output_dir
        ) != 0) {

            status = 1;
        }
    }

    closedir(directory);

    return status;
}

static void make_default_output_for_file(
    const char *input_path,
    char *output,
    size_t output_size
)
{
    char base[256];

    get_name_without_extension(
        input_path,
        base,
        sizeof(base)
    );

    snprintf(
        output,
        output_size,
        "%s_%s_%s",
        base,
        FIRST_NAME,
        STUDENT_ID
    );
}

static void make_default_output_for_directory(
    const char *directory_path,
    char *output,
    size_t output_size
)
{
    char clean_path[1024];

    snprintf(
        clean_path,
        sizeof(clean_path),
        "%s",
        directory_path
    );

    size_t length =
        strlen(clean_path);

    while (
        length > 1 &&
        clean_path[length - 1] == '/'
    ) {

        clean_path[
            length - 1
        ] = '\0';

        length--;
    }

    const char *base =
        get_base_name(
            clean_path
        );

    snprintf(
        output,
        output_size,
        "%s_%s_%s",
        base,
        FIRST_NAME,
        STUDENT_ID
    );
}

static void print_usage(void)
{
    fprintf(
        stderr,
        "Usage: calc [-d DIR | --dir DIR] "
        "[-o OUTDIR | --output-dir OUTDIR] "
        "[input.txt]\n"
    );
}

int main(
    int argc,
    char *argv[]
)
{
    const char *input_file = NULL;
    const char *input_dir = NULL;
    const char *output_dir = NULL;

    for (
        int i = 1;
        i < argc;
        i++
    ) {

        if (
            strcmp(
                argv[i],
                "-o"
            ) == 0 ||
            strcmp(
                argv[i],
                "--output-dir"
            ) == 0
        ) {

            if (i + 1 >= argc) {

                fprintf(
                    stderr,
                    "Missing output directory\n"
                );

                print_usage();

                return 1;
            }

            output_dir =
                argv[++i];
        }

        else if (
            strcmp(
                argv[i],
                "-d"
            ) == 0 ||
            strcmp(
                argv[i],
                "--dir"
            ) == 0
        ) {

            if (i + 1 >= argc) {

                fprintf(
                    stderr,
                    "Missing input directory\n"
                );

                print_usage();

                return 1;
            }

            input_dir =
                argv[++i];
        }

        else if (
            argv[i][0] == '-'
        ) {

            fprintf(
                stderr,
                "Unknown option: %s\n",
                argv[i]
            );

            print_usage();

            return 1;
        }

        else {

            if (
                input_file != NULL
            ) {

                fprintf(
                    stderr,
                    "Too many input files\n"
                );

                print_usage();

                return 1;
            }

            input_file =
                argv[i];
        }
    }

    if (
        input_dir != NULL &&
        input_file != NULL
    ) {

        fprintf(
            stderr,
            "Use either an input file or -d, not both\n"
        );

        print_usage();

        return 1;
    }

    if (
        input_dir == NULL &&
        input_file == NULL
    ) {

        fprintf(
            stderr,
            "No input file or directory specified\n"
        );

        print_usage();

        return 1;
    }

    char default_output[2048];

    if (output_dir == NULL) {

        if (input_dir != NULL) {

            make_default_output_for_directory(
                input_dir,
                default_output,
                sizeof(default_output)
            );

        } else {

            make_default_output_for_file(
                input_file,
                default_output,
                sizeof(default_output)
            );
        }

        output_dir =
            default_output;
    }

    if (input_dir != NULL) {

        return process_directory(
            input_dir,
            output_dir
        );
    }

    return process_file(
        input_file,
        output_dir
    );
}