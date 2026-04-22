/*
 * ============================================================
 *  DSUC + DBMS Assignment 1 — 1st MCA, HBTU Kanpur
 *  Author  : [Your Name / Group Members]
 *  Roll No : [Your Roll Number(s)]
 *  Date    : April 2026
 *  Compiler: gcc (Linux, UTF-8 locale)
 *  Compile : gcc -o dbms main.c -lm
 *  Run     : ./dbms
 * ============================================================
 *
 *  BILINGUAL SSQL — queries work in ENGLISH *or* HINDI
 *  -------------------------------------------------------
 *  Hindi keyword map (Devanagari → English):
 *
 *    चुनो        → SELECT        डालो       → INSERT
 *    से           → FROM          में         → INTO
 *    जहाँ         → WHERE         मान         → VALUES
 *    क्रम         → ORDER         हटाओ        → DELETE
 *    द्वारा       → BY            अपडेट       → UPDATE
 *    जोड़ो         → JOIN          सेट         → SET
 *    पर           → ON            दिखाओ       → SHOW
 *    प्रकार       → TYPE          तालिकाएं    → TABLES
 *    आंतरिक      → INNER         वर्णन        → DESCRIBE
 *    बाएं         → LEFT          सहेजो       → SAVE
 *    दाएं         → RIGHT         सहायता      → HELP
 *    पूर्ण        → FULL          बाहर/छोड़ो  → EXIT
 *    क्रॉस        → CROSS
 *    संख्यात्मक   → NUM
 *
 *  Example Hindi queries:
 *    चुनो * से students
 *    चुनो * से students जहाँ Name=Alice
 *    चुनो * से students क्रम द्वारा Roll संख्यात्मक
 *    चुनो * से students जोड़ो marks पर Roll=Roll प्रकार आंतरिक
 *    डालो में students मान Name=Ravi, Roll=101
 *    हटाओ से students जहाँ Roll=101
 *    अपडेट students सेट Branch=IT जहाँ Roll=101
 *    दिखाओ तालिकाएं
 *    वर्णन students
 *    सहायता
 *    बाहर
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
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>

/* ============================================================
 *  CONSTANTS
 * ============================================================ */
#define MAX_HEADERS     64
#define MAX_FIELD_LEN   256
#define MAX_RECORDS     100000
#define MAX_FILES       1024
#define MAX_PATH        512
#define QUERY_BUF       2048   /* larger to hold Devanagari UTF-8 */

/* ============================================================
 *  BILINGUAL KEYWORD TRANSLATION
 *  translate_hindi_query() walks the input token-by-token and
 *  replaces any known Hindi UTF-8 word with its English keyword.
 *  Non-Hindi tokens are copied as-is, so mixed queries work too.
 * ============================================================ */

typedef struct { const unsigned char hindi[32]; const char *english; } KWPair;

static const KWPair kw_map[] = {
  /* चुनो  -> SELECT */
  { {0xe0,0xa4,0x9a,0xe0,0xa5,0x81,0xe0,0xa4,0xa8,0xe0,0xa5,0x8b,0x00}, "SELECT" },
  /* से    -> FROM   */
  { {0xe0,0xa4,0xb8,0xe0,0xa5,0x87,0x00}, "FROM" },
  /* जहाँ  -> WHERE  */
  { {0xe0,0xa4,0x9c,0xe0,0xa4,0xb9,0xe0,0xa4,0xbe,0xe0,0xa4,0x81,0x00}, "WHERE" },
  /* क्रम  -> ORDER  */
  { {0xe0,0xa4,0x95,0xe0,0xa5,0x8d,0xe0,0xa4,0xb0,0xe0,0xa4,0xae,0x00}, "ORDER" },
  /* द्वारा -> BY    */
  { {0xe0,0xa4,0xa6,0xe0,0xa5,0x8d,0xe0,0xa4,0xb5,0xe0,0xa4,0xbe,0xe0,0xa4,0xb0,0xe0,0xa4,0xbe,0x00}, "BY" },
  /* जोड़ो  -> JOIN  */
  { {0xe0,0xa4,0x9c,0xe0,0xa5,0x8b,0xe0,0xa4,0xa1,0xe0,0xa4,0xbc,0xe0,0xa5,0x8b,0x00}, "JOIN" },
  /* पर    -> ON     */
  { {0xe0,0xa4,0xaa,0xe0,0xa4,0xb0,0x00}, "ON" },
  /* प्रकार -> TYPE  */
  { {0xe0,0xa4,0xaa,0xe0,0xa5,0x8d,0xe0,0xa4,0xb0,0xe0,0xa4,0x95,0xe0,0xa4,0xbe,0xe0,0xa4,0xb0,0x00}, "TYPE" },
  /* डालो  -> INSERT */
  { {0xe0,0xa4,0xa1,0xe0,0xa4,0xbe,0xe0,0xa4,0xb2,0xe0,0xa5,0x8b,0x00}, "INSERT" },
  /* में   -> INTO   */
  { {0xe0,0xa4,0xae,0xe0,0xa5,0x87,0xe0,0xa4,0x82,0x00}, "INTO" },
  /* मान   -> VALUES */
  { {0xe0,0xa4,0xae,0xe0,0xa4,0xbe,0xe0,0xa4,0xa8,0x00}, "VALUES" },
  /* हटाओ  -> DELETE */
  { {0xe0,0xa4,0xb9,0xe0,0xa4,0x9f,0xe0,0xa4,0xbe,0xe0,0xa4,0x93,0x00}, "DELETE" },
  /* अपडेट -> UPDATE */
  { {0xe0,0xa4,0x85,0xe0,0xa4,0xaa,0xe0,0xa4,0xa1,0xe0,0xa5,0x87,0xe0,0xa4,0x9f,0x00}, "UPDATE" },
  /* सेट   -> SET    */
  { {0xe0,0xa4,0xb8,0xe0,0xa5,0x87,0xe0,0xa4,0x9f,0x00}, "SET" },
  /* दिखाओ -> SHOW   */
  { {0xe0,0xa4,0xa6,0xe0,0xa4,0xbf,0xe0,0xa4,0x96,0xe0,0xa4,0xbe,0xe0,0xa4,0x93,0x00}, "SHOW" },
  /* तालिकाएं -> TABLES */
  { {0xe0,0xa4,0xa4,0xe0,0xa4,0xbe,0xe0,0xa4,0xb2,0xe0,0xa4,0xbf,0xe0,0xa4,0x95,0xe0,0xa4,0xbe,0xe0,0xa4,0x8f,0xe0,0xa4,0x82,0x00}, "TABLES" },
  /* वर्णन  -> DESCRIBE */
  { {0xe0,0xa4,0xb5,0xe0,0xa4,0xb0,0xe0,0xa5,0x8d,0xe0,0xa4,0xa3,0xe0,0xa4,0xa8,0x00}, "DESCRIBE" },
  /* सहेजो -> SAVE   */
  { {0xe0,0xa4,0xb8,0xe0,0xa4,0xb9,0xe0,0xa5,0x87,0xe0,0xa4,0x9c,0xe0,0xa5,0x8b,0x00}, "SAVE" },
  /* आंतरिक -> INNER */
  { {0xe0,0xa4,0x86,0xe0,0xa4,0x82,0xe0,0xa4,0xa4,0xe0,0xa4,0xb0,0xe0,0xa4,0xbf,0xe0,0xa4,0x95,0x00}, "INNER" },
  /* बाएं  -> LEFT   */
  { {0xe0,0xa4,0xac,0xe0,0xa4,0xbe,0xe0,0xa4,0x8f,0xe0,0xa4,0x82,0x00}, "LEFT" },
  /* दाएं  -> RIGHT  */
  { {0xe0,0xa4,0xa6,0xe0,0xa4,0xbe,0xe0,0xa4,0x8f,0xe0,0xa4,0x82,0x00}, "RIGHT" },
  /* पूर्ण -> FULL   */
  { {0xe0,0xa4,0xaa,0xe0,0xa5,0x82,0xe0,0xa4,0xb0,0xe0,0xa5,0x8d,0xe0,0xa4,0xa3,0x00}, "FULL" },
  /* क्रॉस -> CROSS  */
  { {0xe0,0xa4,0x95,0xe0,0xa5,0x8d,0xe0,0xa4,0xb0,0xe0,0xa5,0x89,0xe0,0xa4,0xb8,0x00}, "CROSS" },
  /* संख्यात्मक -> NUM */
  { {0xe0,0xa4,0xb8,0xe0,0xa4,0x82,0xe0,0xa4,0x96,0xe0,0xa5,0x8d,0xe0,0xa4,0xaf,0xe0,0xa4,0xbe,0xe0,0xa4,0xa4,0xe0,0xa5,0x8d,0xe0,0xa4,0xae,0xe0,0xa4,0x95,0x00}, "NUM" },
  /* सहायता -> HELP  */
  { {0xe0,0xa4,0xb8,0xe0,0xa4,0xb9,0xe0,0xa4,0xbe,0xe0,0xa4,0xaf,0xe0,0xa4,0xa4,0xe0,0xa4,0xbe,0x00}, "HELP" },
  /* बाहर  -> EXIT   */
  { {0xe0,0xa4,0xac,0xe0,0xa4,0xbe,0xe0,0xa4,0xb9,0xe0,0xa4,0xb0,0x00}, "EXIT" },
  /* छोड़ो -> EXIT   */
  { {0xe0,0xa4,0x9b,0xe0,0xa5,0x8b,0xe0,0xa4,0xa1,0xe0,0xa4,0xbc,0xe0,0xa5,0x8b,0x00}, "EXIT" },
  /* sentinel */
  { {0x00}, NULL }
};

/*
 * UTF-8 aware: advance pointer by one Unicode codepoint.
 * Returns the byte length of that codepoint.
 */
static int utf8_charlen(const unsigned char *s) {
    if (!s || !*s) return 0;
    if ((*s & 0x80) == 0x00) return 1;   /* ASCII      */
    if ((*s & 0xE0) == 0xC0) return 2;
    if ((*s & 0xF0) == 0xE0) return 3;
    if ((*s & 0xF8) == 0xF0) return 4;
    return 1; /* fallback: skip byte */
}

/*
 * Translate a query string: replace Hindi keywords with English.
 * Works token-by-token.  A "token" here is any run of non-space bytes.
 * dest must be at least QUERY_BUF bytes.
 */
static void translate_hindi_query(const char *src, char *dest, int dest_len) {
    const unsigned char *p = (const unsigned char *)src;
    char *out = dest;
    char *end = dest + dest_len - 1;

    while (*p) {
        /* skip whitespace, copy to output */
        if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
            if (out < end) *out++ = (char)*p;
            p++;
            continue;
        }

        /* collect one whitespace-delimited token */
        const unsigned char *tok_start = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r')
            p += utf8_charlen(p);
        int tok_len = (int)(p - tok_start);

        /* try to match against Hindi keyword table */
        const char *replacement = NULL;
        for (int k = 0; kw_map[k].english != NULL; k++) {
            int klen = (int)strlen((const char *)kw_map[k].hindi);
            if (klen == tok_len &&
                memcmp(tok_start, kw_map[k].hindi, klen) == 0) {
                replacement = kw_map[k].english;
                break;
            }
        }

        if (replacement) {
            int rlen = (int)strlen(replacement);
            if (out + rlen < end) {
                memcpy(out, replacement, rlen);
                out += rlen;
            }
        } else {
            /* copy original token bytes */
            if (out + tok_len < end) {
                memcpy(out, tok_start, tok_len);
                out += tok_len;
            }
        }
    }
    *out = '\0';
}

/* ============================================================
    DATA STRUCTURES  (student-written)
   ============================================================ */

typedef struct {
    char header[MAX_FIELD_LEN];
    char value [MAX_FIELD_LEN];
} Field;

typedef struct {
    Field  *fields;
    int     num_fields;
} Record;

typedef struct {
    char    folder_path[MAX_PATH];
    char    headers[MAX_HEADERS][MAX_FIELD_LEN];
    int     num_headers;
    Record *records;
    int     num_records;
    int     capacity;
    int     dirty;
} Table;

/* ============================================================
    UTILITY FUNCTIONS
   ============================================================ */

static void trim(char *s) {
    if (!s || !*s) return;
    char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    int len = (int)strlen(s);
    while (len > 0 && isspace((unsigned char)s[len-1])) s[--len] = '\0';
}

static int ci_cmp(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return tolower((unsigned char)*a) - tolower((unsigned char)*b);
        a++; b++;
    }
    return *a - *b;
}

static const char *get_field(const Record *rec, const char *header) {
    for (int i = 0; i < rec->num_fields; i++)
        if (ci_cmp(rec->fields[i].header, header) == 0)
            return rec->fields[i].value;
    return NULL;
}

static void set_field(Record *rec, const char *header, const char *value) {
    for (int i = 0; i < rec->num_fields; i++) {
        if (ci_cmp(rec->fields[i].header, header) == 0) {
            strncpy(rec->fields[i].value, value, MAX_FIELD_LEN-1);
            return;
        }
    }
    rec->fields = realloc(rec->fields, (rec->num_fields + 1) * sizeof(Field));
    strncpy(rec->fields[rec->num_fields].header, header, MAX_FIELD_LEN-1);
    strncpy(rec->fields[rec->num_fields].value,  value,  MAX_FIELD_LEN-1);
    rec->num_fields++;
}

/* ============================================================
 *  TABLE MANAGEMENT
 * ============================================================ */

static Table *table_create(const char *folder_path) {
    Table *t = calloc(1, sizeof(Table));
    strncpy(t->folder_path, folder_path, MAX_PATH-1);
    t->capacity  = 16;
    t->records   = calloc(t->capacity, sizeof(Record));
    t->dirty     = 0;
    return t;
}

static void table_ensure_capacity(Table *t) {
    if (t->num_records < t->capacity) return;
    t->capacity *= 2;
    t->records   = realloc(t->records, t->capacity * sizeof(Record));
}

static void table_add_header(Table *t, const char *h) {
    for (int i = 0; i < t->num_headers; i++)
        if (ci_cmp(t->headers[i], h) == 0) return;
    if (t->num_headers < MAX_HEADERS)
        strncpy(t->headers[t->num_headers++], h, MAX_FIELD_LEN-1);
}

static void record_free(Record *r) {
    free(r->fields);
    r->fields     = NULL;
    r->num_fields = 0;
}

static void table_free(Table *t) {
    for (int i = 0; i < t->num_records; i++) record_free(&t->records[i]);
    free(t->records);
    free(t);
}

/* ============================================================
 *  FILE I/O
 * ============================================================ */

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
        if (line[0] == '\0') continue;
        char *colon = strchr(line, ':');
        if (!colon) continue;
        *colon = '\0';
        char *key = line;
        char *val = colon + 1;
        trim(key); trim(val);
        set_field(rec, key, val);
        table_add_header(t, key);
        got_data = 1;
    }
    fclose(fp);

    if (got_data) { t->num_records++; return 0; }
    else          { free(rec->fields); return -1; }
}

static int table_load_folder(Table *t) {
    DIR *dir = opendir(t->folder_path);
    if (!dir) { fprintf(stderr, "Cannot open folder: %s\n", t->folder_path); return -1; }

    struct dirent *ent;
    char filepath[MAX_PATH];
    int  count = 0;

    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        snprintf(filepath, sizeof(filepath), "%s/%s", t->folder_path, ent->d_name);
        struct stat st;
        if (stat(filepath, &st) == 0 && S_ISREG(st.st_mode))
            if (load_file_into_table(t, filepath) == 0) count++;
    }
    closedir(dir);
    printf("Loaded %d record(s) from '%s'\n", count, t->folder_path);
    return count;
}

static void table_save_folder(const Table *t) {
    for (int i = 0; i < t->num_records; i++) {
        char filepath[MAX_PATH];
        snprintf(filepath, sizeof(filepath), "%s/record_%04d.txt", t->folder_path, i+1);
        FILE *fp = fopen(filepath, "w");
        if (!fp) { fprintf(stderr, "Cannot write %s\n", filepath); continue; }
        for (int h = 0; h < t->num_headers; h++) {
            const char *val = get_field(&t->records[i], t->headers[h]);
            if (val) fprintf(fp, "%s: %s\n", t->headers[h], val);
        }
        fclose(fp);
    }
    printf("Saved %d record(s) to '%s'\n", t->num_records, t->folder_path);
}

/* ============================================================
 *  DISPLAY
 * ============================================================ */

static void table_print(const Table *t) {
    if (t->num_records == 0) { printf("(no records)\n"); return; }

    int widths[MAX_HEADERS];
    for (int h = 0; h < t->num_headers; h++) {
        widths[h] = (int)strlen(t->headers[h]);
        if (widths[h] < 12) widths[h] = 12;
        for (int r = 0; r < t->num_records; r++) {
            const char *v = get_field(&t->records[r], t->headers[h]);
            if (v && (int)strlen(v) > widths[h]) widths[h] = (int)strlen(v);
        }
    }

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

    for (int r = 0; r < t->num_records; r++) {
        printf("| ");
        for (int h = 0; h < t->num_headers; h++) {
            const char *v = get_field(&t->records[r], t->headers[h]);
            printf("%-*s | ", widths[h], v ? v : "");
        }
        printf("\n");
    }

    printf("+");
    for (int h = 0; h < t->num_headers; h++) {
        for (int c = 0; c < widths[h]+2; c++) printf("-");
        printf("+");
    }
    printf("\n");
    printf("Total: %d record(s)\n\n", t->num_records);
}

/* ============================================================
 *  SORTING
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
 *  INSERT / DELETE / UPDATE  (interactive)
 * ============================================================ */

static void table_insert(Table *t) {
    table_ensure_capacity(t);
    Record *rec   = &t->records[t->num_records];
    rec->fields   = calloc(t->num_headers + 8, sizeof(Field));
    rec->num_fields = 0;

    printf("\n--- Insert New Record / नया रिकॉर्ड डालें ---\n");
    for (int h = 0; h < t->num_headers; h++) {
        printf("  %s: ", t->headers[h]);
        fflush(stdout);
        char buf[MAX_FIELD_LEN];
        if (!fgets(buf, sizeof(buf), stdin)) buf[0] = '\0';
        buf[strcspn(buf, "\n")] = '\0';
        trim(buf);
        set_field(rec, t->headers[h], buf);
    }
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

static void table_delete(Table *t) {
    printf("\n--- Delete Record / रिकॉर्ड हटाएं ---\n");
    printf("Available headers: ");
    for (int h = 0; h < t->num_headers; h++) printf("%s ", t->headers[h]);
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
            for (int j = r; j < t->num_records - 1; j++) t->records[j] = t->records[j+1];
            t->num_records--; deleted++;
        } else r++;
    }
    if (deleted) { t->dirty = 1; printf("Deleted %d record(s).\n", deleted); }
    else           printf("No matching record found.\n");
}

static void table_update(Table *t) {
    printf("\n--- Update Record / रिकॉर्ड अपडेट करें ---\n");
    printf("Available headers: ");
    for (int h = 0; h < t->num_headers; h++) printf("%s ", t->headers[h]);
    printf("\n");

    char srch_hdr[MAX_FIELD_LEN], srch_val[MAX_FIELD_LEN];
    char upd_hdr [MAX_FIELD_LEN], upd_val [MAX_FIELD_LEN];

    printf("Search by header   : "); fgets(srch_hdr, sizeof(srch_hdr), stdin);
    srch_hdr[strcspn(srch_hdr,"\n")] = '\0'; trim(srch_hdr);
    printf("Value to match     : "); fgets(srch_val, sizeof(srch_val), stdin);
    srch_val[strcspn(srch_val,"\n")] = '\0'; trim(srch_val);
    printf("Header to update   : "); fgets(upd_hdr, sizeof(upd_hdr), stdin);
    upd_hdr[strcspn(upd_hdr,"\n")] = '\0'; trim(upd_hdr);
    printf("New value          : "); fgets(upd_val, sizeof(upd_val), stdin);
    upd_val[strcspn(upd_val,"\n")] = '\0'; trim(upd_val);

    int updated = 0;
    for (int r = 0; r < t->num_records; r++) {
        const char *v = get_field(&t->records[r], srch_hdr);
        if (v && ci_cmp(v, srch_val) == 0) {
            set_field(&t->records[r], upd_hdr, upd_val);
            table_add_header(t, upd_hdr);
            updated++;
        }
    }
    if (updated) { t->dirty = 1; printf("Updated %d record(s).\n", updated); }
    else           printf("No matching record found.\n");
}

/* ============================================================
 *  JOIN OPERATIONS
 * ============================================================ */

static int records_match(const Record *r1, const Record *r2,
                         const char *key1, const char *key2) {
    const char *v1 = get_field(r1, key1);
    const char *v2 = get_field(r2, key2);
    if (!v1 || !v2) return 0;
    return (ci_cmp(v1, v2) == 0);
}

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

static void result_append(Table *res, const Record *merged,
                           const char *alias1, const Table *t1,
                           const char *alias2, const Table *t2) {
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
    Record copy;
    copy.num_fields = merged->num_fields;
    copy.fields     = calloc(merged->num_fields + 1, sizeof(Field));
    memcpy(copy.fields, merged->fields, merged->num_fields * sizeof(Field));
    res->records[res->num_records++] = copy;
}

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

static Table *join_inner(const Table *t1, const char *alias1, const char *key1,
                          const Table *t2, const char *alias2, const char *key2) {
    Table *res = table_create("(inner_join_result)");
    for (int i = 0; i < t1->num_records; i++)
        for (int j = 0; j < t2->num_records; j++)
            if (records_match(&t1->records[i], &t2->records[j], key1, key2)) {
                Record m = merge_records(&t1->records[i], alias1, &t2->records[j], alias2);
                result_append(res, &m, alias1, t1, alias2, t2);
                free(m.fields);
            }
    return res;
}

static Table *join_left(const Table *t1, const char *alias1, const char *key1,
                         const Table *t2, const char *alias2, const char *key2) {
    Table *res = table_create("(left_join_result)");
    for (int i = 0; i < t1->num_records; i++) {
        int matched = 0;
        for (int j = 0; j < t2->num_records; j++)
            if (records_match(&t1->records[i], &t2->records[j], key1, key2)) {
                Record m = merge_records(&t1->records[i], alias1, &t2->records[j], alias2);
                result_append(res, &m, alias1, t1, alias2, t2);
                free(m.fields); matched = 1;
            }
        if (!matched) {
            Record m = null_padded(&t1->records[i], alias1, t2, alias2);
            result_append(res, &m, alias1, t1, alias2, t2);
            free(m.fields);
        }
    }
    return res;
}

static Table *join_right(const Table *t1, const char *alias1, const char *key1,
                          const Table *t2, const char *alias2, const char *key2) {
    return join_left(t2, alias2, key2, t1, alias1, key1);
}

static Table *join_full(const Table *t1, const char *alias1, const char *key1,
                         const Table *t2, const char *alias2, const char *key2) {
    Table *res = table_create("(full_join_result)");
    int *matched2 = calloc(t2->num_records, sizeof(int));
    for (int i = 0; i < t1->num_records; i++) {
        int matched = 0;
        for (int j = 0; j < t2->num_records; j++)
            if (records_match(&t1->records[i], &t2->records[j], key1, key2)) {
                Record m = merge_records(&t1->records[i], alias1, &t2->records[j], alias2);
                result_append(res, &m, alias1, t1, alias2, t2);
                free(m.fields); matched = matched2[j] = 1;
            }
        if (!matched) {
            Record m = null_padded(&t1->records[i], alias1, t2, alias2);
            result_append(res, &m, alias1, t1, alias2, t2);
            free(m.fields);
        }
    }
    for (int j = 0; j < t2->num_records; j++)
        if (!matched2[j]) {
            Record m = null_padded(&t2->records[j], alias2, t1, alias1);
            result_append(res, &m, alias1, t1, alias2, t2);
            free(m.fields);
        }
    free(matched2);
    return res;
}

static Table *join_cross(const Table *t1, const char *alias1,
                          const Table *t2, const char *alias2) {
    Table *res = table_create("(cross_join_result)");
    for (int i = 0; i < t1->num_records; i++)
        for (int j = 0; j < t2->num_records; j++) {
            Record m = merge_records(&t1->records[i], alias1, &t2->records[j], alias2);
            result_append(res, &m, alias1, t1, alias2, t2);
            free(m.fields);
        }
    return res;
}

/* ============================================================
 *  SSQL ENGINE
 * ============================================================ */

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
    if (idx >= 0) { table_free(g_tables[idx]); g_tables[idx] = t; return; }
    if (g_num_tables >= MAX_TABLES) { fprintf(stderr, "Too many tables open.\n"); return; }
    g_tables[g_num_tables] = t;
    strncpy(g_aliases[g_num_tables], alias, 63);
    g_num_tables++;
}

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
 * ssql_exec: the main query dispatcher.
 *
 * NEW: before any processing, the raw query is passed through
 * translate_hindi_query() so that Hindi keywords are silently
 * replaced with their English equivalents.  The rest of the
 * function is identical to the original — no duplicate logic.
 */
static int ssql_exec(char *raw_query) {
    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    /* ── Bilingual translation step ── */
    char translated[QUERY_BUF];
    translate_hindi_query(raw_query, translated, QUERY_BUF);

    char buf[QUERY_BUF];
    strncpy(buf, translated, QUERY_BUF-1);

    char *tokens[64];
    int   ntok = tokenize(buf, tokens, 64);
    if (ntok == 0) return 0;

    for (int i = 0; i < ntok && i < 8; i++)
        for (char *c = tokens[i]; *c; c++) *c = (char)toupper((unsigned char)*c);

    /* --- HELP / सहायता --- */
    if (strcmp(tokens[0], "HELP") == 0) {
        printf("\n=== SSQL Help / सहायता ===\n"
               "\n  [ English / अंग्रेज़ी ]\n"
               "  SELECT * FROM <alias>\n"
               "  SELECT <c1>,<c2> FROM <alias>\n"
               "  SELECT * FROM <alias> WHERE <col> =|!=|>|< <val>\n"
               "  SELECT * FROM <alias> ORDER BY <col> [NUM]\n"
               "  SELECT * FROM <t1> JOIN <t2> ON <c1>=<c2> TYPE INNER|LEFT|RIGHT|FULL|CROSS\n"
               "  INSERT INTO <alias> VALUES <h1>=<v1>, <h2>=<v2>\n"
               "  DELETE FROM <alias> WHERE <col>=<val>\n"
               "  UPDATE <alias> SET <col>=<newval> WHERE <col>=<val>\n"
               "  SAVE <alias>  |  SHOW TABLES  |  DESCRIBE <alias>  |  EXIT\n"
               "\n  [ Hindi / हिंदी ]\n"
               "  चुनो * से <alias>\n"
               "  चुनो * से <alias> जहाँ <col>=<val>\n"
               "  चुनो * से <alias> क्रम द्वारा <col> [संख्यात्मक]\n"
               "  चुनो * से <t1> जोड़ो <t2> पर <c1>=<c2> प्रकार आंतरिक|बाएं|दाएं|पूर्ण|क्रॉस\n"
               "  डालो में <alias> मान <h1>=<v1>, <h2>=<v2>\n"
               "  हटाओ से <alias> जहाँ <col>=<val>\n"
               "  अपडेट <alias> सेट <col>=<val> जहाँ <col>=<val>\n"
               "  सहेजो <alias>  |  दिखाओ तालिकाएं  |  वर्णन <alias>  |  बाहर\n\n");
        goto timing;
    }

    /* --- EXIT --- */
    if (strcmp(tokens[0], "EXIT") == 0 || strcmp(tokens[0], "QUIT") == 0)
        return -1;

    /* --- SHOW TABLES --- */
    if (strcmp(tokens[0], "SHOW") == 0) {
        printf("\nOpen tables / खुली तालिकाएं:\n");
        for (int i = 0; i < g_num_tables; i++)
            printf("  [%d] %-20s  %d records  %s\n", i,
                   g_aliases[i], g_tables[i]->num_records,
                   g_tables[i]->dirty ? "(unsaved)" : "");
        printf("\n");
        goto timing;
    }

    /* --- DESCRIBE <alias> --- */
    if (strcmp(tokens[0], "DESCRIBE") == 0 && ntok >= 2) {
        int idx = find_table(tokens[1]);
        if (idx < 0) { printf("Table '%s' not found.\n", tokens[1]); goto timing; }
        Table *t = g_tables[idx];
        printf("\nTable: %s  |  %d record(s)  |  %d column(s)\n",
               tokens[1], t->num_records, t->num_headers);
        printf("Columns: ");
        for (int h = 0; h < t->num_headers; h++)
            printf("%s%s", t->headers[h], h < t->num_headers-1 ? ", " : "\n");
        printf("\n");
        goto timing;
    }

    /* --- SAVE <alias> --- */
    if (strcmp(tokens[0], "SAVE") == 0 && ntok >= 2) {
        int idx = find_table(tokens[1]);
        if (idx < 0) { printf("Table '%s' not found.\n", tokens[1]); goto timing; }
        table_save_folder(g_tables[idx]);
        g_tables[idx]->dirty = 0;
        goto timing;
    }

    /* --- SELECT --- */
    if (strcmp(tokens[0], "SELECT") == 0 && ntok >= 4) {
        int from_pos = -1;
        for (int i = 1; i < ntok; i++)
            if (strcmp(tokens[i], "FROM") == 0) { from_pos = i; break; }
        if (from_pos < 0) { printf("Syntax: SELECT ... FROM ...\n"); goto timing; }

        const char *alias = tokens[from_pos + 1];
        int idx = find_table(alias);
        if (idx < 0) { printf("Table '%s' not found.\n", alias); goto timing; }
        Table *src = g_tables[idx];

        int join_pos = -1;
        for (int i = from_pos+2; i < ntok; i++)
            if (strcmp(tokens[i], "JOIN") == 0) { join_pos = i; break; }

        if (join_pos >= 0) {
            const char *alias2 = tokens[join_pos + 1];
            char *eq = NULL;
            const char *on_str = (join_pos+3 < ntok) ? tokens[join_pos+3] : NULL;
            char key1[64]="", key2[64]="";
            if (on_str) {
                strncpy(key1, on_str, 63);
                eq = strchr(key1, '=');
                if (eq) { *eq = '\0'; strncpy(key2, eq+1, 63); }
            }
            const char *jtype = (join_pos+5 < ntok) ? tokens[join_pos+5] : "INNER";

            int idx2 = find_table(alias2);
            if (idx2 < 0) { printf("Table '%s' not found.\n", alias2); goto timing; }
            Table *t2 = g_tables[idx2];

            Table *res = NULL;
            if      (strcmp(jtype,"INNER")==0) res=join_inner(src,alias,key1,t2,alias2,key2);
            else if (strcmp(jtype,"LEFT")==0)  res=join_left (src,alias,key1,t2,alias2,key2);
            else if (strcmp(jtype,"RIGHT")==0) res=join_right(src,alias,key1,t2,alias2,key2);
            else if (strcmp(jtype,"FULL")==0)  res=join_full (src,alias,key1,t2,alias2,key2);
            else if (strcmp(jtype,"CROSS")==0) res=join_cross(src,alias,t2,alias2);
            else { printf("Unknown join type: %s\n", jtype); goto timing; }

            printf("\n=== %s JOIN result (%d rows) ===\n", jtype, res->num_records);
            table_print(res);

            char res_alias[64];
            snprintf(res_alias, sizeof(res_alias), "%s_%s_join", alias, alias2);
            register_table(res, res_alias);
            printf("Result stored as '%s'\n", res_alias);
            goto timing;
        }

        int where_pos = -1, order_pos = -1;
        for (int i = from_pos+2; i < ntok; i++) {
            if (strcmp(tokens[i],"WHERE") == 0) where_pos = i;
            if (strcmp(tokens[i],"ORDER") == 0) order_pos = i;
        }

        char w_col[64]="", w_op[4]="=", w_val[MAX_FIELD_LEN]="";
        if (where_pos >= 0 && where_pos+1 < ntok) {
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
            memcpy(copy.fields, src->records[r].fields, copy.num_fields * sizeof(Field));
            filtered->records[filtered->num_records++] = copy;
        }

        if (order_pos >= 0 && order_pos+2 < ntok) {
            int numeric = (order_pos+3 < ntok &&
                           strcmp(tokens[order_pos+3],"NUM")==0);
            table_sort(filtered, tokens[order_pos+2], numeric);
        }

        Table *result = filtered;
        if (strcmp(tokens[1], "*") != 0) {
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
        snprintf(res_alias, sizeof(res_alias), "result_%ld", (long)time(NULL) % 10000);
        register_table(result, res_alias);
        printf("Result stored as '%s'\n", res_alias);
        goto timing;
    }

    /* --- INSERT INTO --- */
    if (strcmp(tokens[0], "INSERT") == 0 && ntok >= 5
        && strcmp(tokens[1],"INTO") == 0) {
        const char *alias = tokens[2];
        int idx = find_table(alias);
        if (idx < 0) { printf("Table '%s' not found.\n", alias); goto timing; }
        Table *t = g_tables[idx];

        table_ensure_capacity(t);
        Record *rec   = &t->records[t->num_records];
        rec->fields   = calloc(MAX_HEADERS, sizeof(Field));
        rec->num_fields = 0;

        for (int i = 4; i < ntok; i++) {
            char pair[MAX_FIELD_LEN];
            strncpy(pair, tokens[i], MAX_FIELD_LEN-1);
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

    /* --- DELETE FROM --- */
    if (strcmp(tokens[0], "DELETE") == 0 && ntok >= 5
        && strcmp(tokens[1],"FROM") == 0) {
        const char *alias = tokens[2];
        int idx = find_table(alias);
        if (idx < 0) { printf("Table '%s' not found.\n", alias); goto timing; }
        Table *t = g_tables[idx];

        char *pair = tokens[4];
        char col[64]="", val[MAX_FIELD_LEN]="";
        char *eq = strchr(pair, '=');
        if (eq) { strncpy(col, pair, eq-pair); strncpy(val, eq+1, MAX_FIELD_LEN-1); }
        else    { strncpy(col, pair, 63); if (ntok >= 6) strncpy(val, tokens[5], MAX_FIELD_LEN-1); }
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

    /* --- UPDATE --- */
    if (strcmp(tokens[0], "UPDATE") == 0 && ntok >= 6) {
        const char *alias = tokens[1];
        int idx = find_table(alias);
        if (idx < 0) { printf("Table '%s' not found.\n", alias); goto timing; }
        Table *t = g_tables[idx];

        int set_pos=-1, whr_pos=-1;
        for (int i=0;i<ntok;i++) {
            if (strcmp(tokens[i],"SET")==0)   set_pos=i;
            if (strcmp(tokens[i],"WHERE")==0) whr_pos=i;
        }
        if (set_pos<0||whr_pos<0) { printf("Syntax error in UPDATE.\n"); goto timing; }

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

    printf("Unknown command. Type HELP or सहायता for usage.\n");

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
    if (!fgets(alias, sizeof(alias), stdin)) return;
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
    if      (strcmp(jtype,"INNER")==0) res=join_inner(g_tables[i1],a1,k1,g_tables[i2],a2,k2);
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
    printf("║  DSUC+DBMS Assignment — In-Memory DBMS       ║\n");
    printf("║  Bilingual SSQL: English + हिंदी दोनों       ║\n");
    printf("╠══════════════════════════════════════════════╣\n");
    printf("║ 1. Load folder (table)                       ║\n");
    printf("║ 2. Display table                             ║\n");
    printf("║ 3. Sort                                      ║\n");
    printf("║ 4. Insert record (interactive)               ║\n");
    printf("║ 5. Delete record (interactive)               ║\n");
    printf("║ 6. Update record (interactive)               ║\n");
    printf("║ 7. Save table to folder                      ║\n");
    printf("║ 8. Join two tables                           ║\n");
    printf("║ 9. SSQL Query Shell (English + हिंदी)        ║\n");
    printf("║ 0. Exit                                      ║\n");
    printf("╚══════════════════════════════════════════════╝\n");
    printf("Choice: ");
}

/* ============================================================
 *  MAIN
 * ============================================================ */
int main(void) {
    printf("\n=== DSUC+DBMS Assignment 1 — HBTU Kanpur ===\n");
    printf("    Generic In-Memory Data Management System\n");
    printf("    Bilingual SSQL: English + हिंदी\n\n");
    printf("    Tip: In the query shell, type 'सहायता' or 'HELP'\n\n");

    char choice[8];
    while (1) {
        print_menu();
        if (!fgets(choice, sizeof(choice), stdin)) break;
        choice[strcspn(choice,"\n")] = '\0'; trim(choice);

        if (strcmp(choice,"0") == 0) break;

        else if (strcmp(choice,"1") == 0) menu_load_folder();

        else if (strcmp(choice,"2") == 0) {
            char alias[64];
            printf("Table alias: "); fgets(alias,64,stdin);
            alias[strcspn(alias,"\n")]=0; trim(alias);
            int idx=find_table(alias);
            if (idx<0) printf("Not found.\n");
            else table_print(g_tables[idx]);
        }

        else if (strcmp(choice,"3") == 0) menu_sort();

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
                printf("[Insert time: %.4f ms]\n",
                       (e.tv_sec-s.tv_sec)*1000.0+(e.tv_nsec-s.tv_nsec)/1e6);
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
                printf("[Delete time: %.4f ms]\n",
                       (e.tv_sec-s.tv_sec)*1000.0+(e.tv_nsec-s.tv_nsec)/1e6);
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
                printf("[Update time: %.4f ms]\n",
                       (e.tv_sec-s.tv_sec)*1000.0+(e.tv_nsec-s.tv_nsec)/1e6);
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

        else if (strcmp(choice,"8") == 0) menu_join();

        else if (strcmp(choice,"9") == 0) {
            printf("\n=== SSQL Query Shell ===\n");
            printf("    English and हिंदी both work.\n");
            printf("    Type HELP or सहायता  |  EXIT or बाहर to quit\n\n");
            char qbuf[QUERY_BUF];
            while (1) {
                printf("ssql> "); fflush(stdout);
                if (!fgets(qbuf, QUERY_BUF, stdin)) break;
                qbuf[strcspn(qbuf,"\n")]=0; trim(qbuf);
                if (ssql_exec(qbuf) == -1) break;
            }
        }

        else printf("Invalid choice.\n");
    }

    for (int i=0; i<g_num_tables; i++) {
        if (g_tables[i]->dirty) {
            printf("Table '%s' has unsaved changes. Save? (y/n): ", g_aliases[i]);
            char ch[4]; fgets(ch,4,stdin);
            if (ch[0]=='y') table_save_folder(g_tables[i]);
        }
        table_free(g_tables[i]);
    }
    printf("Goodbye! / अलविदा!\n");
    return 0;
}
