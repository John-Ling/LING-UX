// Example file — cuckoo filter with fully integrated animated visualisation
#include "cuckoo_filter.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>   // usleep
#include <time.h>

// ═══════════════════════════════════════════════════════════════
//  ANSI escape helpers  (monochrome only — no colour codes)
// ═══════════════════════════════════════════════════════════════
#define A_BOLD      "\033[1m"
#define A_DIM       "\033[2m"
#define A_UL        "\033[4m"
#define A_BLINK     "\033[5m"
#define A_REV       "\033[7m"
#define A_RST       "\033[0m"
#define A_CLEAR     "\033[2J\033[H"
#define A_HIDECUR   "\033[?25l"
#define A_SHOWCUR   "\033[?25h"

// ═══════════════════════════════════════════════════════════════
//  Timing
// ═══════════════════════════════════════════════════════════════
#define DELAY_FRAME   120000   //  120 ms — kick animation cadence
#define DELAY_STEP    380000   //  380 ms — between logical steps
#define DELAY_RESULT  600000   //  600 ms — linger on PASS / FAIL flash

// ═══════════════════════════════════════════════════════════════
//  Layout constants
// ═══════════════════════════════════════════════════════════════
#define VIZ_BUCKET_COUNT  16
#define VIZ_FP_BITS        7
#define VIZ_DEPTH          4

// Log panel: circular buffer of recent events shown below the grid
#define LOG_LINES         14
#define LOG_WIDTH         80

// ═══════════════════════════════════════════════════════════════
//  Test-result accumulator
// ═══════════════════════════════════════════════════════════════
typedef struct { int pass; int fail; } TestScore;
static TestScore g_score = {0, 0};

// ═══════════════════════════════════════════════════════════════
//  Log panel
// ═══════════════════════════════════════════════════════════════
typedef struct {
    char lines[LOG_LINES][LOG_WIDTH + 16];  // +16 for ANSI codes
    int  head;
    int  count;
} LogPanel;

static LogPanel g_log = {{{0}}, 0, 0};

static void log_pushf(const char* fmt, ...)
{
    char buf[LOG_WIDTH + 16];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    strncpy(g_log.lines[g_log.head], buf, LOG_WIDTH + 15);
    g_log.lines[g_log.head][LOG_WIDTH + 15] = '\0';
    g_log.head = (g_log.head + 1) % LOG_LINES;
    if (g_log.count < LOG_LINES) g_log.count++;
}

// ═══════════════════════════════════════════════════════════════
//  VizState — per-operation render hints
// ═══════════════════════════════════════════════════════════════
typedef enum { OP_NONE, OP_INSERT, OP_LOOKUP, OP_REMOVE } OpKind;

typedef struct {
    OpKind   op;
    char     key[48];
    uint16_t fp;
    int      b1, b2;
    int      highlight;
    int      kick_src;
    int      kick_slot;
    uint16_t kick_fp;
    int      kicks;
    bool     found;
    bool     success;
    bool     failed;
    bool     flash_pass;
    bool     flash_fail;
    int      test_num;
    char     test_name[80];
} VizState;

// ═══════════════════════════════════════════════════════════════
//  Hash mirrors  (must exactly match cuckoo_filter.c)
// ═══════════════════════════════════════════════════════════════
static uint64_t viz_hash_fp(uint16_t fp)   { return fp * 0x5bd1e995ULL; }

static uint64_t viz_hash_int(int raw)
{
    uint32_t x = (uint32_t)raw;
    x = ((x >> 16) ^ x) * 0x45d9f3bU;
    x = ((x >> 16) ^ x) * 0x45d9f3bU;
    x = (x >> 16) ^ x;
    return (uint64_t)x;
}

static uint64_t viz_hash_str(const char* s)
{
    unsigned long h = 5381;
    while (*s) { h = h * 33 ^ (unsigned char)*s; s++; }
    return h;
}

static uint16_t viz_fingerprint(uint64_t hash, size_t bits)
{
    uint16_t fp = hash & ((1u << bits) - 1);
    fp += (fp == 0);
    return fp;
}

// ═══════════════════════════════════════════════════════════════
//  Render
// ═══════════════════════════════════════════════════════════════
static void render(uint64_t* buckets, size_t bc, VizState* vs,
                   size_t fp_bits, size_t depth, const char* msg)
{
    size_t show = bc < VIZ_BUCKET_COUNT ? bc : VIZ_BUCKET_COUNT;
    uint64_t mask = (1ULL << fp_bits) - 1;
    static const char* op_names[] = {"", "INSERT", "LOOKUP", "REMOVE"};

    printf(A_CLEAR);

    // header
    printf(
           "  buckets:%-3zu  depth:%-2zu  fp-bits:%-2zu  pass:"
           A_RST A_BOLD " %d" "  fail:" A_RST A_BOLD " %d""\n\n",
           bc, depth, fp_bits, g_score.pass, g_score.fail);

    // test label
    if (vs->test_num > 0)
        printf("  " A_BOLD "[Test %d]" A_RST "  %s\n", vs->test_num, vs->test_name);
    else
        printf("\n");

    // operation line
    if (vs->op != OP_NONE) {
        printf("  %s " A_BOLD "%s" A_RST "  fp=0x%04X  B1=%-3d B2=%-3d",
               op_names[vs->op], vs->key, vs->fp, vs->b1, vs->b2);
        if (vs->kicks > 0) printf("  " A_BOLD "kicks:%d" A_RST, vs->kicks);
        printf("\n");
    } else {
        printf("\n");
    }

    // phase message
    if (msg && msg[0]) printf("  " A_DIM "%s" A_RST "\n", msg);
    else               printf("\n");

    // column headers
    printf("         ");
    for (size_t s = 0; s < depth; s++) printf("  slot%-2zu   ", s);
    printf("\n");
    printf("  ------+");
    for (size_t s = 0; s < depth; s++) printf("---------+");
    printf("\n");

    // bucket rows
    for (size_t i = 0; i < show; i++) {
        int ib1  = (vs->b1 >= 0 && (int)i == vs->b1);
        int ib2  = (vs->b2 >= 0 && (int)i == vs->b2);
        int ihi  = ((int)i == vs->highlight);
        int idis = ((int)i == vs->kick_src);

        if (ib1 || ib2) printf(A_BOLD);
        printf("  [%3zu]%s|", i,
               ib1 && ib2 ? "*" : (ib1 ? "1" : (ib2 ? "2" : " ")));
        printf(A_RST);

        uint64_t bkt = buckets[i];
        for (size_t sl = 0; sl < depth; sl++) {
            uint16_t fv = (bkt >> (sl * fp_bits)) & mask;
            int is_kick = idis && (int)sl == vs->kick_slot;

            if      (is_kick && vs->kick_fp) printf(A_BLINK A_REV);
            else if (ihi && fv)              printf(A_REV);
            else if (fv == 0)                printf(A_DIM);

            if (fv) printf(" 0x%04X ", fv);
            else    printf("  ----  ");
            printf(A_RST "|");
        }

        if      (ib1 && ib2) printf(A_BOLD " <primary+alt" A_RST);
        else if (ib1)        printf(A_BOLD " <primary"     A_RST);
        else if (ib2)        printf(A_BOLD " <alternate"   A_RST);
        printf("\n");
    }
    if (show < bc)
        printf(A_DIM "  ... (%zu more buckets) ...\n" A_RST, bc - show);

    printf("  ------+");
    for (size_t s = 0; s < depth; s++) printf("---------+");
    printf("\n");

    // status flash
    if      (vs->flash_pass) printf(A_REV A_BOLD "  PASS  " A_RST "\n");
    else if (vs->flash_fail) printf(A_REV A_BOLD "  FAIL  " A_RST "\n");
    else if (vs->failed)     printf(A_REV A_BOLD "  INSERT FAILED -- filter full  " A_RST "\n");
    else                     printf("\n");

    // log panel
    printf("\n");
    int start = (g_log.count < LOG_LINES) ? 0 : g_log.head;
    for (int li = 0; li < g_log.count; li++) {
        int idx = (start + li) % LOG_LINES;
        int age = g_log.count - 1 - li;
        if (age > 4) printf(A_DIM);
        printf("  %s" A_RST "\n", g_log.lines[idx]);
    }

    fflush(stdout);
}

// ═══════════════════════════════════════════════════════════════
//  VizState initialiser helper
// ═══════════════════════════════════════════════════════════════
static VizState make_vs(OpKind op, const char* key_label,
                         uint16_t fp, int b1, int b2,
                         int tn, const char* tname)
{
    VizState vs;
    memset(&vs, 0, sizeof(vs));
    vs.op = op; vs.fp = fp;
    vs.b1 = b1; vs.b2 = b2;
    vs.highlight = b1;
    vs.kick_src  = -1;
    vs.kick_slot = -1;
    vs.test_num  = tn;
    snprintf(vs.key,       sizeof(vs.key),       "%s", key_label);
    snprintf(vs.test_name, sizeof(vs.test_name), "%s", tname);
    return vs;
}

// ═══════════════════════════════════════════════════════════════
//  Core animated INSERT  (generic — takes a pre-computed h1)
// ═══════════════════════════════════════════════════════════════
static void do_insert(uint64_t* buckets, size_t bc,
                      uint64_t h1, const char* key_label,
                      size_t fp_bits, size_t depth,
                      int tn, const char* tname)
{
    uint64_t mask = (1ULL << fp_bits) - 1;
    uint16_t fp   = viz_fingerprint(h1, fp_bits);
    int b1 = (int)(h1 % bc);
    int b2 = (int)((b1 ^ viz_hash_fp(fp)) % bc);

    VizState vs = make_vs(OP_INSERT, key_label, fp, b1, b2, tn, tname);

    render(buckets, bc, &vs, fp_bits, depth, "Checking primary bucket (B1)...");
    usleep(DELAY_STEP);

    int off1 = -1, off2 = -1;
    for (int i = 0; i < (int)(depth * fp_bits); i += (int)fp_bits) {
        if (((buckets[b1] >> i) & mask) == 0 && off1 == -1) off1 = i;
        if (((buckets[b2] >> i) & mask) == 0 && off2 == -1) off2 = i;
    }

    if (off1 != -1) {
        buckets[b1] |= (uint64_t)fp << off1;
        vs.success = true;
        render(buckets, bc, &vs, fp_bits, depth, "Open slot in B1 -- placed.");
        usleep(DELAY_FRAME);
        log_pushf("  INSERT %-14s fp=0x%04X -> bucket %d", key_label, fp, b1);
        return;
    }

    vs.highlight = b2;
    render(buckets, bc, &vs, fp_bits, depth, "B1 full -- checking alternate (B2)...");
    usleep(DELAY_STEP);

    if (off2 != -1) {
        buckets[b2] |= (uint64_t)fp << off2;
        vs.success = true;
        render(buckets, bc, &vs, fp_bits, depth, "Open slot in B2 -- placed.");
        usleep(DELAY_FRAME);
        log_pushf("  INSERT %-14s fp=0x%04X -> bucket %d", key_label, fp, b2);
        return;
    }

    // cuckoo kicking
    int cur = (rand() % 2) ? b2 : b1;
    uint16_t cur_fp = fp;

    for (int k = 0; k < MAX_KICK_COUNT; k++) {
        int si      = rand() % (int)depth;
        int bit_off = si * (int)fp_bits;
        uint16_t ev = (buckets[cur] >> bit_off) & mask;

        vs.kicks     = k + 1;
        vs.kick_src  = cur;
        vs.kick_slot = si;
        vs.kick_fp   = ev;
        vs.highlight = cur;

        if (ev == 0) {
            buckets[cur] |= (uint64_t)cur_fp << bit_off;
            vs.success = true;
            char msg[96];
            snprintf(msg, sizeof(msg),
                     "Empty slot at kick %d -- placed fp=0x%04X.", k+1, cur_fp);
            render(buckets, bc, &vs, fp_bits, depth, msg);
            usleep(DELAY_STEP);
            log_pushf("  INSERT %-14s fp=0x%04X  (%d kick%s)",
                      key_label, fp, k+1, k ? "s" : "");
            return;
        }

        buckets[cur] &= ~((uint64_t)mask << bit_off);
        buckets[cur] |=  (uint64_t)cur_fp << bit_off;

        char msg[96];
        snprintf(msg, sizeof(msg),
                 "Kick %d: evict 0x%04X from bucket %d slot %d.", k+1, ev, cur, si);
        render(buckets, bc, &vs, fp_bits, depth, msg);
        usleep(DELAY_FRAME);

        cur    = (int)((cur ^ viz_hash_fp(ev)) % bc);
        cur_fp = ev;
        vs.highlight = cur;
        vs.kick_src  = -1;  // clear blink on the old bucket
        render(buckets, bc, &vs, fp_bits, depth,
               "Relocating displaced fingerprint...");
        usleep(DELAY_FRAME);
    }

    vs.failed = true;
    render(buckets, bc, &vs, fp_bits, depth, "Max kicks exceeded -- insertion failed.");
    usleep(DELAY_STEP);
    log_pushf("  INSERT %-14s FAILED (filter full)", key_label);
}

// ═══════════════════════════════════════════════════════════════
//  Core animated LOOKUP / REMOVE  (generic)
// ═══════════════════════════════════════════════════════════════
static bool do_lookup(uint64_t* buckets, size_t bc,
                      uint64_t h1, const char* key_label,
                      size_t fp_bits, size_t depth,
                      bool expected,
                      int tn, const char* tname, OpKind op)
{
    uint64_t mask = (1ULL << fp_bits) - 1;
    uint16_t fp   = viz_fingerprint(h1, fp_bits);
    int b1 = (int)(h1 % bc);
    int b2 = (int)((b1 ^ viz_hash_fp(fp)) % bc);

    VizState vs = make_vs(op, key_label, fp, b1, b2, tn, tname);

    render(buckets, bc, &vs, fp_bits, depth, "Scanning B1 for fingerprint...");
    usleep(DELAY_STEP);

    bool found = false;
    for (size_t sl = 0; sl < depth && !found; sl++)
        if (((buckets[b1] >> (sl * fp_bits)) & mask) == fp) found = true;

    if (!found) {
        vs.highlight = b2;
        render(buckets, bc, &vs, fp_bits, depth, "Not in B1 -- scanning B2...");
        usleep(DELAY_STEP);
        for (size_t sl = 0; sl < depth && !found; sl++)
            if (((buckets[b2] >> (sl * fp_bits)) & mask) == fp) found = true;
    }

    vs.found      = found;
    bool pass     = (found == expected);
    vs.flash_pass = pass;
    vs.flash_fail = !pass;
    if (pass) g_score.pass++; else g_score.fail++;

    const char* op_str = (op == OP_LOOKUP) ? "LOOKUP" : "REMOVE-CHECK";
    log_pushf("  %-12s %-16s %s  [%s]",
              op_str, key_label,
              found ? "found    " : "not found",
              pass  ? "PASS"      : "FAIL");

    render(buckets, bc, &vs, fp_bits, depth,
           found ? "Fingerprint found." : "Fingerprint not found.");
    usleep(DELAY_RESULT);

    vs.flash_pass = vs.flash_fail = false;
    return found;
}

static void do_remove(uint64_t* buckets, size_t bc,
                      uint64_t h1, const char* key_label,
                      size_t fp_bits, size_t depth,
                      int tn, const char* tname)
{
    uint64_t mask = (1ULL << fp_bits) - 1;
    uint16_t fp   = viz_fingerprint(h1, fp_bits);
    int b1 = (int)(h1 % bc);
    int b2 = (int)((b1 ^ viz_hash_fp(fp)) % bc);

    VizState vs = make_vs(OP_REMOVE, key_label, fp, b1, b2, tn, tname);

    render(buckets, bc, &vs, fp_bits, depth, "Locating fingerprint in B1...");
    usleep(DELAY_STEP);

    for (int i = 0; i < (int)(depth * fp_bits); i += (int)fp_bits) {
        if (((buckets[b1] >> i) & mask) == fp) {
            buckets[b1] &= ~((uint64_t)mask << i);
            vs.success = true;
            render(buckets, bc, &vs, fp_bits, depth, "Removed from B1.");
            usleep(DELAY_STEP);
            log_pushf("  REMOVE       %-16s fp=0x%04X from bucket %d", key_label, fp, b1);
            return;
        }
    }
    vs.highlight = b2;
    render(buckets, bc, &vs, fp_bits, depth, "Not in B1 -- checking B2...");
    usleep(DELAY_STEP);
    for (int i = 0; i < (int)(depth * fp_bits); i += (int)fp_bits) {
        if (((buckets[b2] >> i) & mask) == fp) {
            buckets[b2] &= ~((uint64_t)mask << i);
            vs.success = true;
            render(buckets, bc, &vs, fp_bits, depth, "Removed from B2.");
            usleep(DELAY_STEP);
            log_pushf("  REMOVE       %-16s fp=0x%04X from bucket %d", key_label, fp, b2);
            return;
        }
    }
    log_pushf("  REMOVE       %-16s NOT FOUND", key_label);
}

// ═══════════════════════════════════════════════════════════════
//  Typed convenience wrappers
// ═══════════════════════════════════════════════════════════════
static void viz_ins_chr(uint64_t* b, size_t bc, char c, size_t fp, size_t d, int tn, const char* tn2)
{
    char lbl[8]; snprintf(lbl, sizeof(lbl), "'%c'", c);
    do_insert(b, bc, viz_hash_int((int)(unsigned char)c), lbl, fp, d, tn, tn2);
}
static void viz_ins_str(uint64_t* b, size_t bc, const char* s, size_t fp, size_t d, int tn, const char* tn2)
{
    char lbl[50]; snprintf(lbl, sizeof(lbl), "\"%s\"", s);
    do_insert(b, bc, viz_hash_str(s), lbl, fp, d, tn, tn2);
}
static void viz_ins_int(uint64_t* b, size_t bc, int v, size_t fp, size_t d, int tn, const char* tn2)
{
    char lbl[24]; snprintf(lbl, sizeof(lbl), "%d", v);
    do_insert(b, bc, viz_hash_int(v), lbl, fp, d, tn, tn2);
}

static bool viz_get_chr(uint64_t* b, size_t bc, char c, size_t fp, size_t d, bool ex, int tn, const char* tn2)
{
    char lbl[8]; snprintf(lbl, sizeof(lbl), "'%c'", c);
    return do_lookup(b, bc, viz_hash_int((int)(unsigned char)c), lbl, fp, d, ex, tn, tn2, OP_LOOKUP);
}
static bool viz_get_str(uint64_t* b, size_t bc, const char* s, size_t fp, size_t d, bool ex, int tn, const char* tn2)
{
    char lbl[50]; snprintf(lbl, sizeof(lbl), "\"%s\"", s);
    return do_lookup(b, bc, viz_hash_str(s), lbl, fp, d, ex, tn, tn2, OP_LOOKUP);
}
static bool viz_get_int(uint64_t* b, size_t bc, int v, size_t fp, size_t d, bool ex, int tn, const char* tn2)
{
    char lbl[24]; snprintf(lbl, sizeof(lbl), "%d", v);
    return do_lookup(b, bc, viz_hash_int(v), lbl, fp, d, ex, tn, tn2, OP_LOOKUP);
}

static void viz_del_chr(uint64_t* b, size_t bc, char c, size_t fp, size_t d, int tn, const char* tn2)
{
    char lbl[8]; snprintf(lbl, sizeof(lbl), "'%c'", c);
    do_remove(b, bc, viz_hash_int((int)(unsigned char)c), lbl, fp, d, tn, tn2);
}
// (check that deleted key is gone -- treated as a lookup asserting false)
static void viz_del_check(uint64_t* b, size_t bc, char c, size_t fp, size_t d, int tn, const char* tn2)
{
    char lbl[8]; snprintf(lbl, sizeof(lbl), "'%c'", c);
    do_lookup(b, bc, viz_hash_int((int)(unsigned char)c), lbl, fp, d, false, tn, tn2, OP_REMOVE);
}

// ═══════════════════════════════════════════════════════════════
//  Section banner
// ═══════════════════════════════════════════════════════════════
static void show_banner(uint64_t* buckets, size_t bc,
                         size_t fp_bits, size_t depth,
                         int tn, const char* name)
{
    VizState vs;
    memset(&vs, 0, sizeof(vs));
    vs.b1 = -1; vs.b2 = -1; vs.highlight = -1; vs.kick_src = -1; vs.kick_slot = -1;
    vs.op = OP_NONE; vs.test_num = tn;
    snprintf(vs.test_name, sizeof(vs.test_name), "%s", name);

    char sep[72]; memset(sep, '-', 70); sep[70] = '\0';
    log_pushf("  %s", sep);
    log_pushf("  [Test %d] %s", tn, name);

    render(buckets, bc, &vs, fp_bits, depth, "");
    usleep(DELAY_STEP);
}

// ═══════════════════════════════════════════════════════════════
//  main
// ═══════════════════════════════════════════════════════════════
int main(void)
{
    srand((unsigned int)time(NULL));
    printf(A_HIDECUR);

    size_t bc  = VIZ_BUCKET_COUNT;
    size_t fpb = VIZ_FP_BITS;
    size_t dep = VIZ_DEPTH;

    uint64_t* buckets = calloc(bc, sizeof(uint64_t));
    if (!buckets) { perror("calloc"); return EXIT_FAILURE; }

    // ── Test 1 ── Insert a-z then verify all are found ─────────
    show_banner(buckets, bc, fpb, dep, 1, "Insert a-z then verify all are found");
    for (char c = 'a'; c <= 'z'; c++)
        viz_ins_chr(buckets, bc, c, fpb, dep, 1, "Insert a-z then verify all are found");
    for (char c = 'a'; c <= 'z'; c++)
        viz_get_chr(buckets, bc, c, fpb, dep, true, 1, "Insert a-z then verify all are found");

    // ── Test 2 ── Lookup digits 0-9  (never inserted) ──────────
    show_banner(buckets, bc, fpb, dep, 2, "Lookup digits 0-9 (never inserted)");
    for (char c = '0'; c <= '9'; c++)
        viz_get_chr(buckets, bc, c, fpb, dep, false, 2, "Lookup digits 0-9 (never inserted)");

    // ── Test 3 ── Lookup absent strings ────────────────────────
    show_banner(buckets, bc, fpb, dep, 3, "Lookup absent strings");
    const char* absent[] = { "hello", "world", "foo", "bar", "baz" };
    for (int i = 0; i < 5; i++)
        viz_get_str(buckets, bc, absent[i], fpb, dep, false, 3, "Lookup absent strings");

    // ── Test 4 ── Delete 'a','m','z' then re-lookup ────────────
    show_banner(buckets, bc, fpb, dep, 4, "Delete 'a','m','z' then re-lookup");
    const char del_keys[] = { 'a', 'm', 'z' };
    for (int i = 0; i < 3; i++)
        viz_del_chr(buckets, bc, del_keys[i], fpb, dep, 4, "Delete 'a','m','z' then re-lookup");
    // Deleted keys must now be absent
    for (int i = 0; i < 3; i++)
        viz_del_check(buckets, bc, del_keys[i], fpb, dep, 4, "Delete 'a','m','z' then re-lookup");
    // Remaining keys must still be present
    for (char c = 'b'; c <= 'y'; c++) {
        if (c == 'm') continue;
        viz_get_chr(buckets, bc, c, fpb, dep, true, 4, "Delete 'a','m','z' then re-lookup");
    }

    // ── Test 5 ── Integer keys 1-10 insert then lookup 1-15 ────
    memset(buckets, 0, bc * sizeof(uint64_t));  // fresh slate for ints
    show_banner(buckets, bc, fpb, dep, 5, "Insert integers 1-10 then lookup 1-15");
    for (int v = 1; v <= 10; v++)
        viz_ins_int(buckets, bc, v, fpb, dep, 5, "Insert integers 1-10 then lookup 1-15");
    for (int v = 1; v <= 10; v++)
        viz_get_int(buckets, bc, v, fpb, dep, true,  5, "Insert integers 1-10 then lookup 1-15");
    for (int v = 11; v <= 15; v++)
        viz_get_int(buckets, bc, v, fpb, dep, false, 5, "Insert integers 1-10 then lookup 1-15");

    // ── Final summary frame ─────────────────────────────────────
    {
        VizState vs;
        memset(&vs, 0, sizeof(vs));
        vs.b1 = -1; vs.b2 = -1; vs.highlight = -1; vs.kick_src = -1; vs.kick_slot = -1;
        log_pushf("  pass: %d   fail: %d   total: %d",
                  g_score.pass, g_score.fail, g_score.pass + g_score.fail);
        render(buckets, bc, &vs, fpb, dep, "Test suite complete.");
        usleep(DELAY_RESULT);
    }

    free(buckets);
    printf(A_SHOWCUR);
    printf("\n  pass: %d   fail: %d\n\n", g_score.pass, g_score.fail);
    return EXIT_SUCCESS;
}