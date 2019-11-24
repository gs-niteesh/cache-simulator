#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <getopt.h>
#include <string.h>
#include "cachelab.h"

typedef struct _line
{
    bool valid;
    int tag;
    int last_used;
} line;

typedef struct _set
{
    line *lines;
} set;

typedef struct _csim
{
    set *sets;
    int E;
    int s;
    int b;
} csim;

typedef enum
{
    HIT,
    MISS,
    MISSHIT,
    MISSEVICTION,
    MISSEVICTIONHIT,
    HITHIT
} cache_status;

typedef struct _
{
    cache_status cs;
    int verbose;
    int hit;
    int miss;
    int evictions;
} status;

void initialize_sim(csim *);
void simulate(csim *, FILE *, status *);

cache_status load(csim *sim, int, int);
cache_status modify(csim *sim, int, int);
cache_status store(csim *sim, int, int);

// Finds the least recently used line.
int find_lru(set *, int);
// Finds the least empty line.
int find_empty(set *, int);
// Finds the highest used value.
int find_max(set *, int);

// Utility functions
void parse_input(long, int, int, int *, int *, int *);
void printUsage();
FILE *open_file(char *);
char *get_status(status *);

int main(int argc, char *argv[])
{
    int opt;
    char *trace;
    csim sim;
    FILE *fptr;
    status stat = {.verbose = 0, .hit = 0, .miss = 0, .evictions = 0};

    while ((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1)
    {
        switch (opt)
        {
        case 'h':
            printUsage();
            break;
        case 'v':
            stat.verbose = 1;
            break;
        case 's':
            sim.s = atoi(optarg);
            break;
        case 'E':
            sim.E = atoi(optarg);
            break;
        case 'b':
            sim.b = atoi(optarg);
            break;
        case 't':
            trace = optarg;
            break;
        default:
            printUsage();
            break;
        }
    }

    fptr = open_file(trace);
    initialize_sim(&sim);
    simulate(&sim, fptr, &stat);

    printSummary(stat.hit, stat.miss, stat.evictions);

    return 0;
}

void initialize_sim(csim *sim)
{
    // The number of sets: S = 2^s
    int nosets = 2 << (sim->s - 1);
    sim->sets = (set *)malloc(sizeof(set) * nosets);

    if (!sim->sets)
    {
        printf("Cannot allocate memory\n");
        exit(-1);
    }
    for (int i = 0; i < nosets; i++)
    {
        sim->sets[i].lines = (line *)malloc(sizeof(line) * sim->E);
        memset(sim->sets[i].lines, 0, sizeof(line) * sim->E);
    }
}

void simulate(csim *sim, FILE *fptr, status *stat)
{
    char buff[100], op, *temp;
    int set, tag, offset, size;
    long addr;

    while (fgets(buff, 50, fptr) != NULL)
    {
        if (buff[0] == 'I')
            continue;

        sscanf(buff, " %c %lx,%d", &op, &addr, &size);

        parse_input(addr, sim->s, sim->b, &tag, &set, &offset);

        if (op == 'L')
            stat->cs = load(sim, set, tag);
        else if (op == 'M')
            stat->cs = modify(sim, set, tag);
        else if (op == 'S')
            stat->cs = store(sim, set, tag);
        temp = get_status(stat);
        if (stat->verbose)
            printf("%c %lx,%d %s\n", op, addr, size, temp);
    }
}

cache_status load(csim *sim, int nset, int tag)
{
    set *c_set = &sim->sets[nset];
    line *c_line;

    int max = find_max(c_set, sim->E);

    for (int i = 0; i < sim->E; i++)
    {
        c_line = &c_set->lines[i];
        if (c_line->tag == tag && c_line->valid)
        {
            c_line->last_used = max + 1;
            return HIT;
        }
    }

    int empty_line = find_empty(c_set, sim->E);
    if (empty_line != -1)
    {
        c_set->lines[empty_line].tag = tag;
        c_set->lines[empty_line].valid = 1;
        c_set->lines[empty_line].last_used = (max == -1) ? 1 : max + 1;
        return MISS;
    }

    int index = find_lru(c_set, sim->E);
    c_line = &c_set->lines[index];
    c_line->tag = tag;
    c_line->valid = 1;
    c_line->last_used = max + 1;
    return MISSEVICTION;
}

cache_status modify(csim *sim, int st, int tag)
{
    cache_status ret = load(sim, st, tag);
    if (ret == MISS)
    {
        return MISSHIT;
    }
    else if (ret == HIT)
    {
        return HITHIT;
    }
    else if (ret == MISSEVICTION)
    {
        return MISSEVICTIONHIT;
    }
    // Shouldn't reach here
    return -1;
}

cache_status store(csim *sim, int st, int tag)
{
    // A store is similar to load.
    return load(sim, st, tag);
}

int find_lru(set *s, int E)
{
    line *c_line = &s->lines[0];
    int min_line = 0;
    int min = c_line->last_used;

    for (int i = 1; i < E; i++)
    {
        c_line = &s->lines[i];
        if (c_line->last_used < min && c_line->valid)
        {
            min_line = i;
            min = c_line->last_used;
        }
    }
    return min_line;
}

int find_empty(set *s, int E)
{
    line *c_line = NULL;
    for (int i = 0; i < E; i++)
    {
        c_line = &s->lines[i];
        if (!c_line->valid)
        {
            return i;
        }
    }
    return -1;
}

int find_max(set *s, int E)
{
    int max = -1;
    line *c_line;
    for (int i = 0; i < E; i++)
    {
        c_line = &s->lines[i];
        if (c_line->valid && max < c_line->last_used)
        {
            max = c_line->last_used;
        }
    }
    return max;
}

void parse_input(long addr, int s, int b, int *tag, int *set, int *offset)
{
    unsigned long temp = 0xFFFFFFFFFFFFFFFF;
    *offset = addr & (temp >> (64 - b));
    *set = (addr & ((temp >> (64 - s)) << b)) >> b;
    *tag = (addr & (temp << (b + s)));
}

FILE *open_file(char *filename)
{
    FILE *fd;
    fd = fopen(filename, "r");
    if (!fd)
    {
        perror("Error opening the file\n");
        exit(-1);
    }
    return fd;
}

char *get_status(status *stat)
{
    switch (stat->cs)
    {
    case MISS:
        stat->miss++;
        return "miss";
    case HIT:
        stat->hit++;
        return "hit";
    case HITHIT:
        stat->hit++;
        stat->hit++;
        return "hit hit";
    case MISSHIT:
        stat->miss++;
        stat->hit++;
        return "miss hit";
    case MISSEVICTION:
        stat->miss++;
        stat->evictions++;
        return "miss eviction";
    case MISSEVICTIONHIT:
        stat->miss++;
        stat->hit++;
        stat->evictions++;
        return "miss eviction hit";
    }
    return "";
}

void printUsage()
{
    printf("Usage: ./csim-ref [-hv] -s <s> -E <E> -b <b> -t <tracefile>\n");
    printf("  -h: Optional help flag that prints usage info\n");
    printf("  -v: Optional verbose flag that displays trace infoi\n");
    printf("  -s <s>: Number of set index bits (S = 2^s is the number of sets\n");
    printf("  -E <E>: Associativity (number of lines per set)\n");
    printf("  -b <b>: Number of block bits (B = 2b is the block size)\n");
    printf("  -t <tracefile>: Name of the valgrind trace to replay\n");
}