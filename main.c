/*
 * ============================================================
 *  DSUC + DBMS Assignment 1 — 1st MCA, HBTU Kanpur
 *  Author  : [Your Name / Group Members]
 *  Roll No : [Your Roll Number(s)]
 *  Date    : April 2026
 *  Compiler: gcc (Alpine Linux)
 *  Compile : gcc -o dbms main.c -lm
 *  Run     : ./dbms
 * ============================================================
 *
 *  DESIGN OVERVIEW
 *  ---------------
 *  The data files use a key:value text format, e.g.
 *      Name: Alice
 *      Roll: 12
 *      <blank line separates records>
 *
 *  Data Structure chosen: Dynamic Array of Records
 *      - A "Record"  = array of Field {header, value} pairs
 *      - A "Table"   = dynamic array of Records + a header list
 *      - Tables are loaded from folders (MCASampleData1, etc.)
 *      - All operations run on RAM; file write is explicit.
 *
 *  This design is GENERIC: any folder with any headers works.
 * ============================================================
 */

/* ---- Standard headers ---- */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>      /* for execution-time measurement */
#include <dirent.h>    /* for reading directory entries   */
#include <sys/stat.h>  /* for stat()                      */
#include <ctype.h>

/* ============================================================
 *  CONSTANTS
 * ============================================================ */
#define MAX_HEADERS     64
#define MAX_FIELD_LEN   256
#define MAX_RECORDS     100000
#define MAX_FILES       1024
#define MAX_PATH        512
#define QUERY_BUF       1024

/* ============================================================
 *  DATA STRUCTURES  (student-written)
 * ============================================================ */

/* One field inside a record */
typedef struct {
    char header[MAX_FIELD_LEN];   /* column name  e.g. "Name"  */
    char value [MAX_FIELD_LEN];   /* cell value   e.g. "Alice" */
} Field;

/* One record = one "row" in the table */
typedef struct {
    Field  *fields;       /* dynamic array of fields           */
    int     num_fields;   /* number of fields in this record   */
} Record;

/* A table = the whole dataset loaded from one folder */
typedef struct {
    char    folder_path[MAX_PATH]; /* source folder             */
    char    headers[MAX_HEADERS][MAX_FIELD_LEN]; /* column names */
    int     num_headers;           /* how many columns          */
    Record *records;               /* dynamic array of records  */
    int     num_records;           /* current count             */
    int     capacity;              /* allocated slots           */
    int     dirty;                 /* 1 = unsaved changes exist */
} Table;

/* ============================================================
 *  UTILITY FUNCTIONS
 * ============================================================ */

/* Trim leading/trailing whitespace in-place */
static void trim(char *s) {
    if (!s || !*s) return;
    /* trim leading */
    char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    /* trim trailing */
    int len = (int)strlen(s);
    while (len > 0 && isspace((unsigned char)s[len-1])) s[--len] = '\0';
}

/* Case-insensitive string comparison */
static int ci_cmp(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return tolower((unsigned char)*a) - tolower((unsigned char)*b);
        a++; b++;
    }
    return *a - *b;
}

/* Get the value of a field by header name (NULL if not found) */
static const char *get_field(const Record *rec, const char *header) {
    for (int i = 0; i < rec->num_fields; i++)
        if (ci_cmp(rec->fields[i].header, header) == 0)
            return rec->fields[i].value;
    return NULL;
}

/* Set (or create) a field value */
static void set_field(Record *rec, const char *header, const char *value) {
    for (int i = 0; i < rec->num_fields; i++) {
        if (ci_cmp(rec->fields[i].header, header) == 0) {
            strncpy(rec->fields[i].value, value, MAX_FIELD_LEN-1);
            return;
        }
    }
    /* add new field */
    rec->fields = realloc(rec->fields, (rec->num_fields + 1) * sizeof(Field));
    strncpy(rec->fields[rec->num_fields].header, header, MAX_FIELD_LEN-1);
    strncpy(rec->fields[rec->num_fields].value,  value,  MAX_FIELD_LEN-1);
    rec->num_fields++;
}

/* ============================================================
 *  TABLE MANAGEMENT
 * ============================================================ */

/* Initialise an empty table */
static Table *table_create(const char *folder_path) {
    Table *t = calloc(1, sizeof(Table));
    strncpy(t->folder_path, folder_path, MAX_PATH-1);
    t->capacity  = 16;
    t->records   = calloc(t->capacity, sizeof(Record));
    t->dirty     = 0;
    return t;
}

/* Grow table if needed */
static void table_ensure_capacity(Table *t) {
    if (t->num_records < t->capacity) return;
    t->capacity *= 2;
    t->records   = realloc(t->records, t->capacity * sizeof(Record));
}

/* Register a header if not already present */
static void table_add_header(Table *t, const char *h) {
    for (int i = 0; i < t->num_headers; i++)
        if (ci_cmp(t->headers[i], h) == 0) return;
    if (t->num_headers < MAX_HEADERS)
        strncpy(t->headers[t->num_headers++], h, MAX_FIELD_LEN-1);
}

/* Free a record's field memory */
static void record_free(Record *r) {
    free(r->fields);
    r->fields     = NULL;
    r->num_fields = 0;
}

/* Free entire table */
static void table_free(Table *t) {
    for (int i = 0; i < t->num_records; i++) record_free(&t->records[i]);
    free(t->records);
    free(t);
}

/* ============================================================
 *  FILE I/O  — load one text file into records
 * ============================================================ */

/*
 * Each file stores ONE record in key:value lines.
 * Blank lines within a file are ignored.
 * Format:
 *     Header1: value1
 *     Header2: value2
 */
static int load_file_into_table(Table *t, const char *filepath) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) { fprintf(stderr, "Cannot open %s\n", filepath); return -1; }

    table_ensure_capacity(t);
    Record *rec   = &t->records[t->num_records];
    rec->fields   = calloc(MAX_HEADERS, sizeof(Field));
    rec->num_fields = 0;

    char line[1024];
    int  got_data = 0;
    while (fgets(line, sizeof(line), fp)) {
        trim(line);
        if (line[0] == '\0') continue;  /* skip blank lines */

        char *colon = strchr(line, ':');
        if (!colon) continue;           /* skip malformed lines */

        *colon = '\0';
        char *key = line;
        char *val = colon + 1;
        trim(key); trim(val);

        set_field(rec, key, val);
        table_add_header(t, key);
        got_data = 1;
    }
    fclose(fp);

    if (got_data) {
        t->num_records++;
        return 0;
    } else {
        free(rec->fields);
        return -1;
    }
}

/* Load all .txt files from a folder */
static int table_load_folder(Table *t) {
    DIR *dir = opendir(t->folder_path);
    if (!dir) {
        fprintf(stderr, "Cannot open folder: %s\n", t->folder_path);
        return -1;
    }

    struct dirent *ent;
    char filepath[MAX_PATH];
    int  count = 0;

    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        /* accept any file (no extension filter — data files may vary) */
        snprintf(filepath, sizeof(filepath), "%s/%s",
                 t->folder_path, ent->d_name);

        struct stat st;
        if (stat(filepath, &st) == 0 && S_ISREG(st.st_mode)) {
            if (load_file_into_table(t, filepath) == 0) count++;
        }
    }
    closedir(dir);
    printf("Loaded %d record(s) from '%s'\n", count, t->folder_path);
    return count;
}

/* Write records back to folder (one file per record) */
static void table_save_folder(const Table *t) {
    for (int i = 0; i < t->num_records; i++) {
        char filepath[MAX_PATH];
        snprintf(filepath, sizeof(filepath), "%s/record_%04d.txt",
                 t->folder_path, i + 1);
        FILE *fp = fopen(filepath, "w");
        if (!fp) { fprintf(stderr, "Cannot write %s\n", filepath); continue; }

        /* Write in canonical header order */
        for (int h = 0; h < t->num_headers; h++) {
            const char *val = get_field(&t->records[i], t->headers[h]);
            if (val)
                fprintf(fp, "%s: %s\n", t->headers[h], val);
        }
        fclose(fp);
    }
    printf("Saved %d record(s) to '%s'\n", t->num_records, t->folder_path);
}

/* ============================================================
 *  DISPLAY
 * ============================================================ */
static void print_separator(const Table *t) {
    printf("+");
    for (int h = 0; h < t->num_headers; h++) {
        int w = (int)strlen(t->headers[h]) + 2;
        if (w < 14) w = 14;
        for (int c = 0; c < w; c++) printf("-");
        printf("+");
    }
    printf("\n");
}

static void table_print(const Table *t) {
    if (t->num_records == 0) { printf("(no records)\n"); return; }

    /* column widths */
    int widths[MAX_HEADERS];
    for (int h = 0; h < t->num_headers; h++) {
        widths[h] = (int)strlen(t->headers[h]);
        if (widths[h] < 12) widths[h] = 12;
        for (int r = 0; r < t->num_records; r++) {
            const char *v = get_field(&t->records[r], t->headers[h]);
            if (v && (int)strlen(v) > widths[h]) widths[h] = (int)strlen(v);
        }
    }

    /* header row */
    printf("+");
    for (int h = 0; h < t->num_headers; h++) {
        for (int c = 0; c < widths[h]+2; c++) printf("-");
        printf("+");
    }
    printf("\n| ");
    for (int h = 0; h < t->num_headers; h++)
        printf("%-*s | ", widths[h], t->headers[h]);
    printf("\n+");
    for (int h = 0; h < t->num_headers; h++) {
        for (int c = 0; c < widths[h]+2; c++) printf("=");
        printf("+");
    }
    printf("\n");

    /* data rows */
    for (int r = 0; r < t->num_records; r++) {
        printf("| ");
        for (int h = 0; h < t->num_headers; h++) {
            const char *v = get_field(&t->records[r], t->headers[h]);
            printf("%-*s | ", widths[h], v ? v : "");
        }
        printf("\n");
    }

    /* footer */
    printf("+");
    for (int h = 0; h < t->num_headers; h++) {
        for (int c = 0; c < widths[h]+2; c++) printf("-");
        printf("+");
    }
    printf("\n");
    printf("Total: %d record(s)\n\n", t->num_records);
}

/* ============================================================
 *  QUESTION 1b — i. SORTING  (student-written)
 *  Generic comparison used by qsort; sort key set globally.
 * ============================================================ */
static char g_sort_header[MAX_FIELD_LEN];
static int  g_sort_numeric = 0;

static int record_cmp(const void *a, const void *b) {
    const Record *ra = (const Record *)a;
    const Record *rb = (const Record *)b;
    const char   *va = get_field(ra, g_sort_header);
    const char   *vb = get_field(rb, g_sort_header);
    if (!va) va = "";
    if (!vb) vb = "";
    if (g_sort_numeric) return atoi(va) - atoi(vb);
    return strcmp(va, vb);
}

static void table_sort(Table *t, const char *header, int numeric) {
    strncpy(g_sort_header, header, MAX_FIELD_LEN-1);
    g_sort_numeric = numeric;
    qsort(t->records, t->num_records, sizeof(Record), record_cmp);
    t->dirty = 1;
    printf("Sorted by '%s' (%s order).\n", header, numeric?"numeric":"alphabetical");
}

/* ============================================================
 *  QUESTION 1b — ii. INSERT
 * ============================================================ */
static void table_insert(Table *t) {
    table_ensure_capacity(t);
    Record *rec   = &t->records[t->num_records];
    rec->fields   = calloc(t->num_headers + 8, sizeof(Field));
    rec->num_fields = 0;

    printf("\n--- Insert New Record ---\n");
    printf("Enter values for each header (press Enter to leave blank):\n");
    for (int h = 0; h < t->num_headers; h++) {
        printf("  %s: ", t->headers[h]);
        fflush(stdout);
        char buf[MAX_FIELD_LEN];
        if (!fgets(buf, sizeof(buf), stdin)) buf[0] = '\0';
        /* strip newline */
        buf[strcspn(buf, "\n")] = '\0';
        trim(buf);
        set_field(rec, t->headers[h], buf);
    }

    /* allow adding extra fields not in current header list */
    printf("  Add extra fields? (y/n): ");
    char choice[4];
    if (fgets(choice, sizeof(choice), stdin) && choice[0] == 'y') {
        char hdr[MAX_FIELD_LEN], val[MAX_FIELD_LEN];
        while (1) {
            printf("  New header name (or empty to stop): ");
            if (!fgets(hdr, sizeof(hdr), stdin)) break;
            hdr[strcspn(hdr,"\n")] = '\0'; trim(hdr);
            if (hdr[0] == '\0') break;
            printf("  Value for '%s': ", hdr);
            if (!fgets(val, sizeof(val), stdin)) val[0]='\0';
            val[strcspn(val,"\n")] = '\0'; trim(val);
            set_field(rec, hdr, val);
            table_add_header(t, hdr);
        }
    }

    t->num_records++;
    t->dirty = 1;
    printf("Record inserted. Total records: %d\n", t->num_records);
}

/* ============================================================
 *  QUESTION 1b — iii. DELETE
 * ============================================================ */
static void table_delete(Table *t) {
    printf("\n--- Delete Record ---\n");
    printf("Available headers: ");
    for (int h = 0; h < t->num_headers; h++)
        printf("%s ", t->headers[h]);
    printf("\n");

    char header[MAX_FIELD_LEN], value[MAX_FIELD_LEN];
    printf("Search by header: "); fflush(stdout);
    if (!fgets(header, sizeof(header), stdin)) return;
    header[strcspn(header,"\n")] = '\0'; trim(header);

    printf("Value to match  : "); fflush(stdout);
    if (!fgets(value, sizeof(value), stdin)) return;
    value[strcspn(value,"\n")] = '\0'; trim(value);

    int deleted = 0;
    for (int r = 0; r < t->num_records; ) {
        const char *v = get_field(&t->records[r], header);
        if (v && ci_cmp(v, value) == 0) {
            record_free(&t->records[r]);
            /* shift left */
            for (int j = r; j < t->num_records - 1; j++)
                t->records[j] = t->records[j+1];
            t->num_records--;
            deleted++;
        } else r++;
    }
    if (deleted) {
        t->dirty = 1;
        printf("Deleted %d record(s).\n", deleted);
    } else {
        printf("No matching record found.\n");
    }
}

/* ============================================================
 *  QUESTION 1b — iv. UPDATE
 * ============================================================ */
static void table_update(Table *t) {
    printf("\n--- Update Record ---\n");
    printf("Available headers: ");
    for (int h = 0; h < t->num_headers; h++) printf("%s ", t->headers[h]);
    printf("\n");

    char srch_hdr[MAX_FIELD_LEN], srch_val[MAX_FIELD_LEN];
    char upd_hdr [MAX_FIELD_LEN], upd_val [MAX_FIELD_LEN];

    printf("Search by header   : "); fgets(srch_hdr, sizeof(srch_hdr), stdin);
    srch_hdr[strcspn(srch_hdr,"\n")] = '\0'; trim(srch_hdr);
    printf("Value to match     : "); fgets(srch_val, sizeof(srch_val), stdin);
    srch_val[strcspn(srch_val,"\n")] = '\0'; trim(srch_val);
    printf("Header to update   : "); fgets(upd_hdr,  sizeof(upd_hdr),  stdin);
    upd_hdr [strcspn(upd_hdr, "\n")] = '\0'; trim(upd_hdr);
    printf("New value          : "); fgets(upd_val,  sizeof(upd_val),  stdin);
    upd_val [strcspn(upd_val, "\n")] = '\0'; trim(upd_val);

    int updated = 0;
    for (int r = 0; r < t->num_records; r++) {
        const char *v = get_field(&t->records[r], srch_hdr);
        if (v && ci_cmp(v, srch_val) == 0) {
            set_field(&t->records[r], upd_hdr, upd_val);
            table_add_header(t, upd_hdr);
            updated++;
        }
    }
    if (updated) {
        t->dirty = 1;
        printf("Updated %d record(s).\n", updated);
    } else printf("No matching record found.\n");
}

/* ============================================================
 *  QUESTION 2a — JOIN OPERATIONS
 * ============================================================ */

/*
 * Result of a join is itself a Table (in-memory).
 * We build it in RAM and return it; the user can choose to save it.
 */

/* Helper: check if two records match on a given key */
static int records_match(const Record *r1, const Record *r2,
                         const char *key1, const char *key2) {
    const char *v1 = get_field(r1, key1);
    const char *v2 = get_field(r2, key2);
    if (!v1 || !v2) return 0;
    return (ci_cmp(v1, v2) == 0);
}

/* Merge two records into a new one; prefix col names with table alias */
static Record merge_records(const Record *r1, const char *alias1,
                             const Record *r2, const char *alias2) {
    Record out;
    out.num_fields = 0;
    out.fields     = calloc(r1->num_fields + r2->num_fields + 2, sizeof(Field));

    char hdr[MAX_FIELD_LEN];
    for (int i = 0; i < r1->num_fields; i++) {
        snprintf(hdr, sizeof(hdr), "%s.%s", alias1, r1->fields[i].header);
        set_field(&out, hdr, r1->fields[i].value);
    }
    for (int i = 0; i < r2->num_fields; i++) {
        snprintf(hdr, sizeof(hdr), "%s.%s", alias2, r2->fields[i].header);
        set_field(&out, hdr, r2->fields[i].value);
    }
    return out;
}

/* Adds merged record to result table and registers headers */
static void result_append(Table *res, const Record *merged,
                           const char *alias1, const Table *t1,
                           const char *alias2, const Table *t2) {
    /* ensure all headers are registered */
    char hdr[MAX_FIELD_LEN];
    for (int h = 0; h < t1->num_headers; h++) {
        snprintf(hdr, sizeof(hdr), "%s.%s", alias1, t1->headers[h]);
        table_add_header(res, hdr);
    }
    for (int h = 0; h < t2->num_headers; h++) {
        snprintf(hdr, sizeof(hdr), "%s.%s", alias2, t2->headers[h]);
        table_add_header(res, hdr);
    }
    table_ensure_capacity(res);
    /* deep copy merged record */
    Record copy;
    copy.num_fields = merged->num_fields;
    copy.fields     = calloc(merged->num_fields + 1, sizeof(Field));
    memcpy(copy.fields, merged->fields, merged->num_fields * sizeof(Field));
    res->records[res->num_records++] = copy;
}

/* NULL-padded record for outer joins */
static Record null_padded(const Record *r, const char *alias,
                           const Table *other_t, const char *other_alias) {
    Record out;
    out.num_fields = 0;
    out.fields     = calloc(r->num_fields + other_t->num_headers + 2, sizeof(Field));

    char hdr[MAX_FIELD_LEN];
    for (int i = 0; i < r->num_fields; i++) {
        snprintf(hdr, sizeof(hdr), "%s.%s", alias, r->fields[i].header);
        set_field(&out, hdr, r->fields[i].value);
    }
    for (int h = 0; h < other_t->num_headers; h++) {
        snprintf(hdr, sizeof(hdr), "%s.%s", other_alias, other_t->headers[h]);
        set_field(&out, hdr, "NULL");
    }
    return out;
}

/* INNER JOIN */
static Table *join_inner(const Table *t1, const char *alias1, const char *key1,
                          const Table *t2, const char *alias2, const char *key2) {
    Table *res = table_create("(inner_join_result)");
    for (int i = 0; i < t1->num_records; i++) {
        for (int j = 0; j < t2->num_records; j++) {
            if (records_match(&t1->records[i], &t2->records[j], key1, key2)) {
                Record m = merge_records(&t1->records[i], alias1,
                                         &t2->records[j], alias2);
                result_append(res, &m, alias1, t1, alias2, t2);
                free(m.fields);
            }
        }
    }
    return res;
}

/* LEFT OUTER JOIN */
static Table *join_left(const Table *t1, const char *alias1, const char *key1,
                         const Table *t2, const char *alias2, const char *key2) {
    Table *res = table_create("(left_join_result)");
    for (int i = 0; i < t1->num_records; i++) {
        int matched = 0;
        for (int j = 0; j < t2->num_records; j++) {
            if (records_match(&t1->records[i], &t2->records[j], key1, key2)) {
                Record m = merge_records(&t1->records[i], alias1,
                                         &t2->records[j], alias2);
                result_append(res, &m, alias1, t1, alias2, t2);
                free(m.fields);
                matched = 1;
            }
        }
        if (!matched) {
            Record m = null_padded(&t1->records[i], alias1, t2, alias2);
            result_append(res, &m, alias1, t1, alias2, t2);
            free(m.fields);
        }
    }
    return res;
}

/* RIGHT OUTER JOIN (swap + left) */
static Table *join_right(const Table *t1, const char *alias1, const char *key1,
                          const Table *t2, const char *alias2, const char *key2) {
    return join_left(t2, alias2, key2, t1, alias1, key1);
}

/* FULL OUTER JOIN */
static Table *join_full(const Table *t1, const char *alias1, const char *key1,
                         const Table *t2, const char *alias2, const char *key2) {
    Table *res = table_create("(full_join_result)");

    /* track which t2 rows matched */
    int *matched2 = calloc(t2->num_records, sizeof(int));

    for (int i = 0; i < t1->num_records; i++) {
        int matched = 0;
        for (int j = 0; j < t2->num_records; j++) {
            if (records_match(&t1->records[i], &t2->records[j], key1, key2)) {
                Record m = merge_records(&t1->records[i], alias1,
                                         &t2->records[j], alias2);
                result_append(res, &m, alias1, t1, alias2, t2);
                free(m.fields);
                matched = matched2[j] = 1;
            }
        }
        if (!matched) {
            Record m = null_padded(&t1->records[i], alias1, t2, alias2);
            result_append(res, &m, alias1, t1, alias2, t2);
            free(m.fields);
        }
    }
    /* unmatched from t2 */
    for (int j = 0; j < t2->num_records; j++) {
        if (!matched2[j]) {
            Record m = null_padded(&t2->records[j], alias2, t1, alias1);
            result_append(res, &m, alias1, t1, alias2, t2);
            free(m.fields);
        }
    }
    free(matched2);
    return res;
}

/* CROSS JOIN */
static Table *join_cross(const Table *t1, const char *alias1,
                          const Table *t2, const char *alias2) {
    Table *res = table_create("(cross_join_result)");
    for (int i = 0; i < t1->num_records; i++) {
        for (int j = 0; j < t2->num_records; j++) {
            Record m = merge_records(&t1->records[i], alias1,
                                     &t2->records[j], alias2);
            result_append(res, &m, alias1, t1, alias2, t2);
            free(m.fields);
        }
    }
    return res;
}

/* ============================================================
 *  QUESTION 2b — SEMI-STRUCTURED QUERY LANGUAGE  (SSQL)
 *
 *  Supported syntax:
 *
 *  SELECT * FROM <table_alias>
 *  SELECT <col1>,<col2>,... FROM <table_alias>
 *  SELECT * FROM <table_alias> WHERE <col> = <val>
 *  SELECT * FROM <table_alias> WHERE <col> != <val>
 *  SELECT * FROM <table_alias> ORDER BY <col> [NUM]
 *  SELECT * FROM <t1> JOIN <t2> ON <col1>=<col2> TYPE <INNER|LEFT|RIGHT|FULL|CROSS>
 *  INSERT INTO <table_alias> VALUES <h1>=<v1>, <h2>=<v2>, ...
 *  DELETE FROM <table_alias> WHERE <col> = <val>
 *  UPDATE <table_alias> SET <col>=<val> WHERE <srch_col>=<srch_val>
 *  SAVE <table_alias>
 *  SHOW TABLES
 *  DESCRIBE <table_alias>
 *  HELP
 *  EXIT
 * ============================================================ */

/* Maximum tables open at once */
#define MAX_TABLES 16
static Table *g_tables[MAX_TABLES];
static char   g_aliases[MAX_TABLES][64];
static int    g_num_tables = 0;

static int find_table(const char *alias) {
    for (int i = 0; i < g_num_tables; i++)
        if (ci_cmp(g_aliases[i], alias) == 0) return i;
    return -1;
}

static void register_table(Table *t, const char *alias) {
    int idx = find_table(alias);
    if (idx >= 0) { /* replace */
        table_free(g_tables[idx]);
        g_tables[idx] = t;
        return;
    }
    if (g_num_tables >= MAX_TABLES) {
        fprintf(stderr, "Too many tables open.\n"); return;
    }
    g_tables[g_num_tables]  = t;
    strncpy(g_aliases[g_num_tables], alias, 63);
    g_num_tables++;
}

/* Simple tokeniser: split on spaces, respect quotes */
static int tokenize(char *line, char **tokens, int max_tokens) {
    int n = 0;
    char *p = line;
    while (*p && n < max_tokens) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        if (*p == '"' || *p == '\'') {
            char q = *p++;
            tokens[n++] = p;
            while (*p && *p != q) p++;
            if (*p) *p++ = '\0';
        } else {
            tokens[n++] = p;
            while (*p && *p != ' ' && *p != '\t') p++;
            if (*p) *p++ = '\0';
        }
    }
    return n;
}

/* Apply WHERE filter, return 1 if record passes */
static int where_match(const Record *r, const char *col,
                       const char *op, const char *val) {
    const char *rv = get_field(r, col);
    if (!rv) rv = "";
    if (strcmp(op, "=")  == 0) return ci_cmp(rv, val) == 0;
    if (strcmp(op, "!=") == 0) return ci_cmp(rv, val) != 0;
    if (strcmp(op, ">")  == 0) return atof(rv) > atof(val);
    if (strcmp(op, "<")  == 0) return atof(rv) < atof(val);
    if (strcmp(op, ">=") == 0) return atof(rv) >= atof(val);
    if (strcmp(op, "<=") == 0) return atof(rv) <= atof(val);
    return 1;
}

/* Project specific columns from table into a new table */
static Table *project(const Table *src, char **cols, int ncols) {
    Table *out = table_create(src->folder_path);
    for (int h = 0; h < ncols; h++) table_add_header(out, cols[h]);

    for (int r = 0; r < src->num_records; r++) {
        table_ensure_capacity(out);
        Record rec;
        rec.fields     = calloc(ncols + 1, sizeof(Field));
        rec.num_fields = 0;
        for (int h = 0; h < ncols; h++) {
            const char *v = get_field(&src->records[r], cols[h]);
            set_field(&rec, cols[h], v ? v : "");
        }
        out->records[out->num_records++] = rec;
    }
    return out;
}

/*
 * Execute one SSQL statement.
 * Returns 0 on normal, -1 on exit.
 */
static int ssql_exec(char *query) {
    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start); /* start timing */

    char buf[QUERY_BUF];
    strncpy(buf, query, QUERY_BUF-1);

    /* Upper-case first token for keyword matching */
    char *tokens[64];
    int   ntok = tokenize(buf, tokens, 64);
    if (ntok == 0) return 0;

    /* convert keyword tokens to uppercase */
    for (int i = 0; i < ntok && i < 8; i++) {
        for (char *c = tokens[i]; *c; c++) *c = (char)toupper((unsigned char)*c);
    }

    /* --- HELP --- */
    if (strcmp(tokens[0], "HELP") == 0) {
        printf("\n=== SSQL Help ===\n"
               "  SELECT * FROM <alias>\n"
               "  SELECT <c1>,<c2> FROM <alias>\n"
               "  SELECT * FROM <alias> WHERE <col> =|!=|>|< <val>\n"
               "  SELECT * FROM <alias> ORDER BY <col> [NUM]\n"
               "  SELECT * FROM <t1> JOIN <t2> ON <c1>=<c2> TYPE INNER|LEFT|RIGHT|FULL|CROSS\n"
               "  INSERT INTO <alias> VALUES <h1>=<v1>, <h2>=<v2>, ...\n"
               "  DELETE FROM <alias> WHERE <col>=<val>\n"
               "  UPDATE <alias> SET <col>=<newval> WHERE <col>=<val>\n"
               "  SAVE <alias>\n"
               "  SHOW TABLES\n"
               "  DESCRIBE <alias>\n"
               "  EXIT\n\n");
        return 0;
    }

    /* --- EXIT --- */
    if (strcmp(tokens[0], "EXIT") == 0 || strcmp(tokens[0], "QUIT") == 0)
        return -1;

    /* --- SHOW TABLES --- */
    if (strcmp(tokens[0], "SHOW") == 0) {
        printf("\nOpen tables:\n");
        for (int i = 0; i < g_num_tables; i++)
            printf("  [%d] %-20s  %d records  %s\n", i,
                   g_aliases[i], g_tables[i]->num_records,
                   g_tables[i]->dirty ? "(unsaved)" : "");
        printf("\n");
        return 0;
    }

    /* --- DESCRIBE <alias> --- */
    if (strcmp(tokens[0], "DESCRIBE") == 0 && ntok >= 2) {
        int idx = find_table(tokens[1]);
        if (idx < 0) { printf("Table '%s' not found.\n", tokens[1]); return 0; }
        Table *t = g_tables[idx];
        printf("\nTable: %s  |  %d record(s)  |  %d column(s)\n",
               tokens[1], t->num_records, t->num_headers);
        printf("Columns: ");
        for (int h = 0; h < t->num_headers; h++)
            printf("%s%s", t->headers[h], h < t->num_headers-1 ? ", " : "\n");
        printf("\n");
        return 0;
    }

    /* --- SAVE <alias> --- */
    if (strcmp(tokens[0], "SAVE") == 0 && ntok >= 2) {
        int idx = find_table(tokens[1]);
        if (idx < 0) { printf("Table '%s' not found.\n", tokens[1]); return 0; }
        table_save_folder(g_tables[idx]);
        g_tables[idx]->dirty = 0;
        return 0;
    }

    /* --- SELECT --- */
    if (strcmp(tokens[0], "SELECT") == 0 && ntok >= 4) {
        /* find FROM position */
        int from_pos = -1;
        for (int i = 1; i < ntok; i++)
            if (strcmp(tokens[i], "FROM") == 0) { from_pos = i; break; }
        if (from_pos < 0) { printf("Syntax: SELECT ... FROM ...\n"); return 0; }

        const char *alias = tokens[from_pos + 1];
        int idx = find_table(alias);
        if (idx < 0) { printf("Table '%s' not found.\n", alias); return 0; }
        Table *src = g_tables[idx];

        /* Check for JOIN */
        int join_pos = -1;
        for (int i = from_pos+2; i < ntok; i++)
            if (strcmp(tokens[i], "JOIN") == 0) { join_pos = i; break; }

        if (join_pos >= 0) {
            /* SELECT * FROM t1 JOIN t2 ON c1=c2 TYPE <type> */
            const char *alias2  = tokens[join_pos + 1];
            /* ON */
            char *eq = NULL;
            const char *on_str  = (join_pos+3 < ntok) ? tokens[join_pos+3] : NULL;
            char key1[64]="", key2[64]="";
            if (on_str) {
                strncpy(key1, on_str, 63);
                eq = strchr(key1, '=');
                if (eq) { *eq = '\0'; strncpy(key2, eq+1, 63); }
            }
            const char *jtype = (join_pos+5 < ntok) ? tokens[join_pos+5] : "INNER";

            int idx2 = find_table(alias2);
            if (idx2 < 0) { printf("Table '%s' not found.\n", alias2); return 0; }
            Table *t2 = g_tables[idx2];

            Table *res = NULL;
            if (strcmp(jtype,"INNER")==0) res=join_inner(src,alias,key1,t2,alias2,key2);
            else if (strcmp(jtype,"LEFT")==0)  res=join_left (src,alias,key1,t2,alias2,key2);
            else if (strcmp(jtype,"RIGHT")==0) res=join_right(src,alias,key1,t2,alias2,key2);
            else if (strcmp(jtype,"FULL")==0)  res=join_full (src,alias,key1,t2,alias2,key2);
            else if (strcmp(jtype,"CROSS")==0) res=join_cross(src,alias,t2,alias2);
            else { printf("Unknown join type: %s\n", jtype); return 0; }

            printf("\n=== %s JOIN result (%d rows) ===\n", jtype, res->num_records);
            table_print(res);

            /* store result */
            char res_alias[64];
            snprintf(res_alias, sizeof(res_alias), "%s_%s_join", alias, alias2);
            register_table(res, res_alias);
            printf("Result stored as '%s'\n", res_alias);
            goto timing;
        }

        /* WHERE clause? */
        int where_pos = -1, order_pos = -1;
        for (int i = from_pos+2; i < ntok; i++) {
            if (strcmp(tokens[i],"WHERE")   == 0) where_pos = i;
            if (strcmp(tokens[i],"ORDER")   == 0) order_pos = i;
        }

        char w_col[64]="", w_op[4]="=", w_val[MAX_FIELD_LEN]="";
        if (where_pos >= 0 && where_pos+1 < ntok) {
            /* parse col op val — may be fused "col=val" or separate */
            char *wp = tokens[where_pos+1];
            char *op_ptr;
            if ((op_ptr = strstr(wp, "!=")) != NULL) {
                *op_ptr = '\0'; strncpy(w_col, wp, 63);
                strncpy(w_op, "!=", 3);
                strncpy(w_val, op_ptr+2, MAX_FIELD_LEN-1);
            } else if ((op_ptr = strchr(wp, '=')) != NULL) {
                *op_ptr = '\0'; strncpy(w_col, wp, 63);
                strncpy(w_op, "=", 3);
                strncpy(w_val, op_ptr+1, MAX_FIELD_LEN-1);
            } else {
                strncpy(w_col, wp, 63);
                if (where_pos+2 < ntok) strncpy(w_op, tokens[where_pos+2], 3);
                if (where_pos+3 < ntok) strncpy(w_val, tokens[where_pos+3], MAX_FIELD_LEN-1);
            }
            trim(w_col); trim(w_op); trim(w_val);
        }

        /* Build filtered table */
        Table *filtered = table_create(src->folder_path);
        for (int h = 0; h < src->num_headers; h++)
            table_add_header(filtered, src->headers[h]);

        for (int r = 0; r < src->num_records; r++) {
            if (where_pos >= 0 &&
                !where_match(&src->records[r], w_col, w_op, w_val)) continue;
            table_ensure_capacity(filtered);
            Record copy;
            copy.num_fields = src->records[r].num_fields;
            copy.fields     = calloc(copy.num_fields+1, sizeof(Field));
            memcpy(copy.fields, src->records[r].fields,
                   copy.num_fields * sizeof(Field));
            filtered->records[filtered->num_records++] = copy;
        }

        /* ORDER BY */
        if (order_pos >= 0 && order_pos+2 < ntok) {
            int numeric = (order_pos+3 < ntok &&
                           strcmp(tokens[order_pos+3],"NUM")==0);
            table_sort(filtered, tokens[order_pos+2], numeric);
        }

        /* PROJECTION */
        Table *result = filtered;
        if (strcmp(tokens[1], "*") != 0) {
            /* parse comma-separated columns (fused in tokens[1]) */
            char colbuf[MAX_FIELD_LEN];
            strncpy(colbuf, tokens[1], MAX_FIELD_LEN-1);
            char *proj_cols[MAX_HEADERS];
            int  nproj = 0;
            char *tok = strtok(colbuf, ",");
            while (tok) { proj_cols[nproj++] = tok; tok = strtok(NULL, ","); }
            result = project(filtered, proj_cols, nproj);
            table_free(filtered);
        }

        printf("\n=== Query result (%d rows) ===\n", result->num_records);
        table_print(result);

        char res_alias[64];
        snprintf(res_alias, sizeof(res_alias), "result_%ld",
                 (long)time(NULL) % 10000);
        register_table(result, res_alias);
        printf("Result stored as '%s'\n", res_alias);
        goto timing;
    }

    /* --- INSERT INTO <alias> VALUES h1=v1, h2=v2 ... --- */
    if (strcmp(tokens[0], "INSERT") == 0 && ntok >= 5
        && strcmp(tokens[1],"INTO") == 0) {
        const char *alias = tokens[2];
        int idx = find_table(alias);
        if (idx < 0) { printf("Table '%s' not found.\n", alias); return 0; }
        Table *t = g_tables[idx];

        /* tokens[3] should be VALUES, rest are h=v pairs */
        table_ensure_capacity(t);
        Record *rec   = &t->records[t->num_records];
        rec->fields   = calloc(MAX_HEADERS, sizeof(Field));
        rec->num_fields = 0;

        for (int i = 4; i < ntok; i++) {
            char pair[MAX_FIELD_LEN];
            strncpy(pair, tokens[i], MAX_FIELD_LEN-1);
            /* strip trailing comma */
            pair[strcspn(pair,",")] = '\0';
            char *eq = strchr(pair, '=');
            if (!eq) continue;
            *eq = '\0';
            trim(pair); trim(eq+1);
            set_field(rec, pair, eq+1);
            table_add_header(t, pair);
        }
        t->num_records++;
        t->dirty = 1;
        printf("Inserted 1 record. Total: %d\n", t->num_records);
        goto timing;
    }

    /* --- DELETE FROM <alias> WHERE col=val --- */
    if (strcmp(tokens[0], "DELETE") == 0 && ntok >= 5
        && strcmp(tokens[1],"FROM") == 0) {
        const char *alias = tokens[2];
        int idx = find_table(alias);
        if (idx < 0) { printf("Table '%s' not found.\n", alias); return 0; }
        Table *t = g_tables[idx];

        /* parse WHERE col=val */
        char *pair = tokens[4]; /* e.g. "Name=Alice" or could be split */
        char col[64]="", val[MAX_FIELD_LEN]="";
        char *eq = strchr(pair, '=');
        if (eq) { strncpy(col, pair, eq-pair); strncpy(val, eq+1, MAX_FIELD_LEN-1); }
        else   { strncpy(col, pair, 63);
                 if (ntok >= 6) strncpy(val, tokens[5], MAX_FIELD_LEN-1); }
        trim(col); trim(val);

        int deleted = 0;
        for (int r = 0; r < t->num_records; ) {
            const char *v = get_field(&t->records[r], col);
            if (v && ci_cmp(v, val) == 0) {
                record_free(&t->records[r]);
                for (int j = r; j < t->num_records-1; j++) t->records[j]=t->records[j+1];
                t->num_records--; deleted++;
            } else r++;
        }
        t->dirty = (deleted > 0);
        printf("Deleted %d record(s).\n", deleted);
        goto timing;
    }

    /* --- UPDATE <alias> SET col=val WHERE col=val --- */
    if (strcmp(tokens[0], "UPDATE") == 0 && ntok >= 6) {
        const char *alias = tokens[1];
        int idx = find_table(alias);
        if (idx < 0) { printf("Table '%s' not found.\n", alias); return 0; }
        Table *t = g_tables[idx];

        /* find SET and WHERE positions */
        int set_pos=-1, whr_pos=-1;
        for (int i=0;i<ntok;i++) {
            if (strcmp(tokens[i],"SET")==0)   set_pos=i;
            if (strcmp(tokens[i],"WHERE")==0) whr_pos=i;
        }
        if (set_pos<0||whr_pos<0) { printf("Syntax error in UPDATE.\n"); return 0; }

        char set_col[64]="", set_val[MAX_FIELD_LEN]="";
        char whr_col[64]="", whr_val[MAX_FIELD_LEN]="";

        char *sp = tokens[set_pos+1], *ep = strchr(sp,'=');
        if (ep) { strncpy(set_col,sp,ep-sp); strncpy(set_val,ep+1,MAX_FIELD_LEN-1); }

        char *wp = tokens[whr_pos+1], *ep2 = strchr(wp,'=');
        if (ep2) { strncpy(whr_col,wp,ep2-wp); strncpy(whr_val,ep2+1,MAX_FIELD_LEN-1); }
        trim(set_col);trim(set_val);trim(whr_col);trim(whr_val);

        int updated=0;
        for (int r=0; r<t->num_records; r++) {
            const char *v = get_field(&t->records[r], whr_col);
            if (v && ci_cmp(v, whr_val)==0) {
                set_field(&t->records[r], set_col, set_val);
                table_add_header(t, set_col);
                updated++;
            }
        }
        t->dirty = (updated>0);
        printf("Updated %d record(s).\n", updated);
        goto timing;
    }

    printf("Unknown command. Type HELP for usage.\n");

timing:;
    clock_gettime(CLOCK_MONOTONIC, &t_end);
    double elapsed_ms = (t_end.tv_sec - t_start.tv_sec)*1000.0
                      + (t_end.tv_nsec - t_start.tv_nsec)/1e6;
    printf("[Execution time: %.4f ms]\n\n", elapsed_ms);
    return 0;
}

/* ============================================================
 *  INTERACTIVE MENU
 * ============================================================ */
static void menu_load_folder(void) {
    char folder[MAX_PATH], alias[64];
    printf("Folder path : "); fflush(stdout);
    if (!fgets(folder, sizeof(folder), stdin)) return;
    folder[strcspn(folder,"\n")] = '\0'; trim(folder);

    printf("Alias (name): "); fflush(stdout);
    if (!fgets(alias,  sizeof(alias),  stdin)) return;
    alias[strcspn(alias,"\n")] = '\0'; trim(alias);

    Table *t = table_create(folder);
    if (table_load_folder(t) < 0) { table_free(t); return; }
    register_table(t, alias);
    printf("Table '%s' ready.\n\n", alias);
}

static void menu_sort(void) {
    char alias[64], header[MAX_FIELD_LEN];
    printf("Table alias : "); fgets(alias, 64, stdin);
    alias[strcspn(alias,"\n")] = '\0'; trim(alias);
    int idx = find_table(alias);
    if (idx < 0) { printf("Not found.\n"); return; }

    Table *t = g_tables[idx];
    printf("Sort by header: ");
    for (int h=0;h<t->num_headers;h++) printf("%s ",t->headers[h]);
    printf("\n> ");
    fgets(header, MAX_FIELD_LEN, stdin);
    header[strcspn(header,"\n")] = '\0'; trim(header);
    printf("Numeric sort? (y/n): ");
    char ch[4]; fgets(ch,4,stdin);

    struct timespec s,e;
    clock_gettime(CLOCK_MONOTONIC,&s);
    table_sort(t, header, ch[0]=='y');
    clock_gettime(CLOCK_MONOTONIC,&e);
    double ms = (e.tv_sec-s.tv_sec)*1000.0+(e.tv_nsec-s.tv_nsec)/1e6;
    table_print(t);
    printf("[Sort time: %.4f ms]\n\n", ms);
}

static void menu_join(void) {
    char a1[64], a2[64], k1[64], k2[64], jtype[16];
    printf("Table 1 alias: "); fgets(a1,64,stdin); a1[strcspn(a1,"\n")]=0; trim(a1);
    printf("Table 2 alias: "); fgets(a2,64,stdin); a2[strcspn(a2,"\n")]=0; trim(a2);
    printf("Join key (table1 header): "); fgets(k1,64,stdin); k1[strcspn(k1,"\n")]=0; trim(k1);
    printf("Join key (table2 header): "); fgets(k2,64,stdin); k2[strcspn(k2,"\n")]=0; trim(k2);
    printf("Join type (INNER/LEFT/RIGHT/FULL/CROSS): "); fgets(jtype,16,stdin);
    jtype[strcspn(jtype,"\n")]=0; trim(jtype);
    for (char *c=jtype;*c;c++) *c=(char)toupper((unsigned char)*c);

    int i1=find_table(a1), i2=find_table(a2);
    if (i1<0||i2<0) { printf("Table(s) not found.\n"); return; }

    struct timespec s,e;
    clock_gettime(CLOCK_MONOTONIC,&s);
    Table *res = NULL;
    if (strcmp(jtype,"INNER")==0) res=join_inner(g_tables[i1],a1,k1,g_tables[i2],a2,k2);
    else if (strcmp(jtype,"LEFT")==0)  res=join_left (g_tables[i1],a1,k1,g_tables[i2],a2,k2);
    else if (strcmp(jtype,"RIGHT")==0) res=join_right(g_tables[i1],a1,k1,g_tables[i2],a2,k2);
    else if (strcmp(jtype,"FULL")==0)  res=join_full (g_tables[i1],a1,k1,g_tables[i2],a2,k2);
    else if (strcmp(jtype,"CROSS")==0) res=join_cross(g_tables[i1],a1,g_tables[i2],a2);
    else { printf("Unknown join type.\n"); return; }
    clock_gettime(CLOCK_MONOTONIC,&e);
    double ms = (e.tv_sec-s.tv_sec)*1000.0+(e.tv_nsec-s.tv_nsec)/1e6;

    printf("\n=== %s JOIN result (%d rows) ===\n", jtype, res->num_records);
    table_print(res);
    printf("[Join time: %.4f ms]\n\n", ms);

    char res_alias[64];
    snprintf(res_alias,64,"%s_%s_join",a1,a2);
    register_table(res, res_alias);
    printf("Stored as '%s'\n\n", res_alias);
}

static void print_menu(void) {
    printf("\n╔══════════════════════════════════════════════╗\n");
    printf("║    DSUC+DBMS Assignment — In-Memory DBMS     ║\n");
    printf("╠══════════════════════════════════════════════╣\n");
    printf("║ 1. Load folder (table)                       ║\n");
    printf("║ 2. Display table                             ║\n");
    printf("║ 3. Sort                                      ║\n");
    printf("║ 4. Insert record (interactive)               ║\n");
    printf("║ 5. Delete record (interactive)               ║\n");
    printf("║ 6. Update record (interactive)               ║\n");
    printf("║ 7. Save table to folder                      ║\n");
    printf("║ 8. Join two tables                           ║\n");
    printf("║ 9. SSQL Query Shell                          ║\n");
    printf("║ 0. Exit                                      ║\n");
    printf("╚══════════════════════════════════════════════╝\n");
    printf("Choice: ");
}

/* ============================================================
 *  MAIN
 * ============================================================ */
int main(void) {
    printf("\n=== DSUC+DBMS Assignment 1 — HBTU Kanpur ===\n");
    printf("    Generic In-Memory Data Management System\n\n");

    char choice[8];
    while (1) {
        print_menu();
        if (!fgets(choice, sizeof(choice), stdin)) break;
        choice[strcspn(choice,"\n")] = '\0'; trim(choice);

        if (strcmp(choice,"0") == 0) break;

        else if (strcmp(choice,"1") == 0) {
            menu_load_folder();
        }

        else if (strcmp(choice,"2") == 0) {
            char alias[64];
            printf("Table alias: "); fgets(alias,64,stdin);
            alias[strcspn(alias,"\n")]=0; trim(alias);
            int idx=find_table(alias);
            if (idx<0) printf("Not found.\n");
            else table_print(g_tables[idx]);
        }

        else if (strcmp(choice,"3") == 0) {
            menu_sort();
        }

        else if (strcmp(choice,"4") == 0) {
            char alias[64];
            printf("Table alias: "); fgets(alias,64,stdin);
            alias[strcspn(alias,"\n")]=0; trim(alias);
            int idx=find_table(alias);
            if (idx<0) printf("Not found.\n");
            else {
                struct timespec s,e;
                clock_gettime(CLOCK_MONOTONIC,&s);
                table_insert(g_tables[idx]);
                clock_gettime(CLOCK_MONOTONIC,&e);
                double ms=(e.tv_sec-s.tv_sec)*1000.0+(e.tv_nsec-s.tv_nsec)/1e6;
                printf("[Insert time: %.4f ms]\n", ms);
            }
        }

        else if (strcmp(choice,"5") == 0) {
            char alias[64];
            printf("Table alias: "); fgets(alias,64,stdin);
            alias[strcspn(alias,"\n")]=0; trim(alias);
            int idx=find_table(alias);
            if (idx<0) printf("Not found.\n");
            else {
                struct timespec s,e;
                clock_gettime(CLOCK_MONOTONIC,&s);
                table_delete(g_tables[idx]);
                clock_gettime(CLOCK_MONOTONIC,&e);
                double ms=(e.tv_sec-s.tv_sec)*1000.0+(e.tv_nsec-s.tv_nsec)/1e6;
                printf("[Delete time: %.4f ms]\n", ms);
            }
        }

        else if (strcmp(choice,"6") == 0) {
            char alias[64];
            printf("Table alias: "); fgets(alias,64,stdin);
            alias[strcspn(alias,"\n")]=0; trim(alias);
            int idx=find_table(alias);
            if (idx<0) printf("Not found.\n");
            else {
                struct timespec s,e;
                clock_gettime(CLOCK_MONOTONIC,&s);
                table_update(g_tables[idx]);
                clock_gettime(CLOCK_MONOTONIC,&e);
                double ms=(e.tv_sec-s.tv_sec)*1000.0+(e.tv_nsec-s.tv_nsec)/1e6;
                printf("[Update time: %.4f ms]\n", ms);
            }
        }

        else if (strcmp(choice,"7") == 0) {
            char alias[64];
            printf("Table alias: "); fgets(alias,64,stdin);
            alias[strcspn(alias,"\n")]=0; trim(alias);
            int idx=find_table(alias);
            if (idx<0) printf("Not found.\n");
            else { table_save_folder(g_tables[idx]); g_tables[idx]->dirty=0; }
        }

        else if (strcmp(choice,"8") == 0) {
            menu_join();
        }

        else if (strcmp(choice,"9") == 0) {
            printf("\n=== SSQL Query Shell (type HELP, EXIT to quit) ===\n");
            char qbuf[QUERY_BUF];
            while (1) {
                printf("ssql> "); fflush(stdout);
                if (!fgets(qbuf, QUERY_BUF, stdin)) break;
                qbuf[strcspn(qbuf,"\n")]=0; trim(qbuf);
                if (ssql_exec(qbuf) == -1) break;
            }
        }

        else {
            printf("Invalid choice.\n");
        }
    }

    /* cleanup */
    for (int i=0; i<g_num_tables; i++) {
        if (g_tables[i]->dirty) {
            printf("Table '%s' has unsaved changes. Save? (y/n): ", g_aliases[i]);
            char ch[4]; fgets(ch,4,stdin);
            if (ch[0]=='y') { table_save_folder(g_tables[i]); }
        }
        table_free(g_tables[i]);
    }
    printf("Goodbye!\n");
    return 0;
}
