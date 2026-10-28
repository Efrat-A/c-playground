/*
 * Hash Code 2017 practice problem: Pizza
 *
 * Usage:   ./pizza < example.in > example.out
 *          ./pizza example.in example.out
 *
 * Approach:
 *   1. 2D prefix sums of tomatoes -> O(1) ingredient count for any rectangle.
 *   2. Greedy: scan cells row by row; at each uncovered cell try every valid
 *      slice shape (largest area first) anchored at that cell, take the first
 *      one that is free and has >= L of each ingredient.
 *   3. Improvement: repeatedly try to grow each slice by one row/column in any
 *      direction while it stays valid (area <= H) and doesn't overlap.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct { int r1, c1, r2, c2; } Slice;
typedef struct { int h, w; } Shape;

static int R, C, L, H;
static char **pizza;     /* pizza[r][c] = 'T' or 'M' */
static int *ps;          /* prefix sum of tomatoes, (R+1) x (C+1) */
static unsigned char *covered;

#define PS(r, c) ps[(r) * (C + 1) + (c)]
#define COV(r, c) covered[(r) * C + (c)]

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) { fprintf(stderr, "out of memory\n"); exit(1); }
    return p;
}

static bool read_input(FILE *in) {
    if (fscanf(in, "%d %d %d %d", &R, &C, &L, &H) != 4) return false;
    if (R < 1 || R > 1000 || C < 1 || C > 1000 ||
        L < 1 || L > 1000 || H < 1 || H > 1000) return false;

    pizza = xmalloc(sizeof(char *) * R);
    char *buf = xmalloc(C + 2);
    for (int r = 0; r < R; ++r) {
        if (fscanf(in, "%s", buf) != 1 || (int)strlen(buf) != C) {
            fprintf(stderr, "bad row %d\n", r);
            return false;
        }
        pizza[r] = xmalloc(C);
        memcpy(pizza[r], buf, C);
    }
    free(buf);
    return true;
}

static void build_prefix(void) {
    ps = calloc((size_t)(R + 1) * (C + 1), sizeof(int));
    if (!ps) { fprintf(stderr, "out of memory\n"); exit(1); }
    for (int r = 0; r < R; ++r)
        for (int c = 0; c < C; ++c)
            PS(r + 1, c + 1) = PS(r, c + 1) + PS(r + 1, c) - PS(r, c)
                             + (pizza[r][c] == 'T');
}

/* tomatoes in the inclusive rectangle */
static int tomatoes(int r1, int c1, int r2, int c2) {
    return PS(r2 + 1, c2 + 1) - PS(r1, c2 + 1) - PS(r2 + 1, c1) + PS(r1, c1);
}

static bool ingredients_ok(int r1, int c1, int r2, int c2) {
    int area = (r2 - r1 + 1) * (c2 - c1 + 1);
    if (area > H) return false;
    int t = tomatoes(r1, c1, r2, c2);
    return t >= L && area - t >= L;
}

static bool is_free(int r1, int c1, int r2, int c2) {
    for (int r = r1; r <= r2; ++r)
        for (int c = c1; c <= c2; ++c)
            if (COV(r, c)) return false;
    return true;
}

static void mark(int r1, int c1, int r2, int c2) {
    for (int r = r1; r <= r2; ++r)
        for (int c = c1; c <= c2; ++c)
            COV(r, c) = 1;
}

static int cmp_shape(const void *a, const void *b) {
    const Shape *x = a, *y = b;
    return (y->h * y->w) - (x->h * x->w);   /* larger area first */
}

/* All h x w with 2L <= h*w <= H (includes both orientations and 1 x H). */
static Shape *get_shapes(int *n) {
    Shape *s = xmalloc(sizeof(Shape) * (size_t)H * H);
    *n = 0;
    for (int h = 1; h <= H; ++h)
        for (int w = 1; h * w <= H; ++w)
            if (h * w >= 2 * L) s[(*n)++] = (Shape){ h, w };
    qsort(s, *n, sizeof(Shape), cmp_shape);
    return s;
}

/* Try to enlarge a slice by one row/column; returns true if it grew. */
static bool try_grow(Slice *s) {
    static const int dir[4][4] = {  /* dr1, dc1, dr2, dc2 */
        { -1, 0, 0, 0 }, { 0, 0, 1, 0 }, { 0, -1, 0, 0 }, { 0, 0, 0, 1 }
    };
    for (int d = 0; d < 4; ++d) {
        int r1 = s->r1 + dir[d][0], c1 = s->c1 + dir[d][1];
        int r2 = s->r2 + dir[d][2], c2 = s->c2 + dir[d][3];
        if (r1 < 0 || c1 < 0 || r2 >= R || c2 >= C) continue;
        if ((r2 - r1 + 1) * (c2 - c1 + 1) > H) continue;
        /* only the new strip needs to be free */
        int nr1 = r1, nr2 = r2, nc1 = c1, nc2 = c2;
        if (d == 0) nr2 = r1;          /* new top row     */
        else if (d == 1) nr1 = r2;     /* new bottom row  */
        else if (d == 2) nc2 = c1;     /* new left column */
        else nc1 = c2;                 /* new right column */
        if (!is_free(nr1, nc1, nr2, nc2)) continue;
        mark(nr1, nc1, nr2, nc2);
        *s = (Slice){ r1, c1, r2, c2 };
        return true;
    }
    return false;
}

static Slice *solve(int *count) {
    covered = calloc((size_t)R * C, 1);
    if (!covered) { fprintf(stderr, "out of memory\n"); exit(1); }

    int nshapes;
    Shape *shapes = get_shapes(&nshapes);
    Slice *slices = xmalloc(sizeof(Slice) * ((size_t)R * C / (2 * L) + 1));
    *count = 0;

    for (int r = 0; r < R; ++r)
        for (int c = 0; c < C; ++c) {
            if (COV(r, c)) continue;
            for (int i = 0; i < nshapes; ++i) {
                int r2 = r + shapes[i].h - 1, c2 = c + shapes[i].w - 1;
                if (r2 >= R || c2 >= C) continue;
                if (!ingredients_ok(r, c, r2, c2)) continue;
                if (!is_free(r, c, r2, c2)) continue;
                mark(r, c, r2, c2);
                slices[(*count)++] = (Slice){ r, c, r2, c2 };
                break;
            }
        }

    /* growing keeps ingredient counts >= L, so slices stay valid */
    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = 0; i < *count; ++i)
            while (try_grow(&slices[i])) changed = true;
    }

    free(shapes);
    return slices;
}

int main(int argc, char **argv) {
    FILE *in = stdin, *out = stdout;
    if (argc > 1 && !(in = fopen(argv[1], "r"))) { perror(argv[1]); return 1; }
    if (argc > 2 && !(out = fopen(argv[2], "w"))) { perror(argv[2]); return 1; }

    if (!read_input(in)) { fprintf(stderr, "invalid input\n"); return 1; }
    build_prefix();

    int n, score = 0;
    Slice *slices = solve(&n);

    fprintf(out, "%d\n", n);
    for (int i = 0; i < n; ++i) {
        Slice s = slices[i];
        fprintf(out, "%d %d %d %d\n", s.r1, s.c1, s.r2, s.c2);
        score += (s.r2 - s.r1 + 1) * (s.c2 - s.c1 + 1);
    }
    fprintf(stderr, "slices: %d, score: %d / %d\n", n, score, R * C);

    if (in != stdin) fclose(in);
    if (out != stdout) fclose(out);
    for (int r = 0; r < R; ++r) free(pizza[r]);
    free(pizza); free(ps); free(covered); free(slices);
    return 0;
}
