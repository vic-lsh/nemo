// mbm_poll.c — per-core memory bandwidth polling with libpqos
// Build: gcc -O2 -Wall -o mbm_poll mbm_poll.c -lpqos
// Run (root): sudo ./mbm_poll [-i interval_ms] [-c core_list] [-d duration_s]

#define _GNU_SOURCE
#include <pqos.h>
#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t g_stop = 0;
static void on_sigint(int sig) { (void)sig; g_stop = 1; }

static void die(const char *msg) {
    fprintf(stderr, "Error: %s\n", msg);
    exit(EXIT_FAILURE);
}

static void die_pqos(const char *ctx, int ret) {
    // fprintf(stderr, "PQOS error in %s: %s (%d)\n", ctx, pqos_strerror(ret), ret);
    fprintf(stderr, "PQOS error in %s: (%d)\n", ctx, ret);
    exit(EXIT_FAILURE);
}

// Simple core list parser: "0-3,8,10-12" -> fill cores[], return count
static int parse_core_list(const char *s, unsigned *cores, int maxcores) {
    int n = 0;
    const char *p = s;
    while (*p) {
        char *end = NULL;
        long a = strtol(p, &end, 10);
        if (end == p || a < 0) return -1;
        if (*end == '-') {
            char *end2 = NULL;
            long b = strtol(end + 1, &end2, 10);
            if (end2 == end + 1 || b < a) return -1;
            for (long x = a; x <= b; x++) {
                if (n >= maxcores) return -1;
                cores[n++] = (unsigned)x;
            }
            p = (*end2 == ',') ? end2 + 1 : end2;
        } else {
            if (n >= maxcores) return -1;
            cores[n++] = (unsigned)a;
            p = (*end == ',') ? end + 1 : end;
        }
        if (*p == ',') p++;
    }
    return n;
}

static void msleep(unsigned ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000ULL;
    nanosleep(&ts, NULL);
}

int main(int argc, char **argv) {
    unsigned interval_ms = 1000;
    unsigned duration_s = 0; // 0 = infinite
    unsigned user_cores[4096];
    int n_user_cores = -1;

    // CLI
    int opt;
    while ((opt = getopt(argc, argv, "i:c:d:")) != -1) {
        switch (opt) {
            case 'i': interval_ms = (unsigned)strtoul(optarg, NULL, 10); break;
            case 'c': {
                n_user_cores = parse_core_list(optarg, user_cores, 4096);
                if (n_user_cores <= 0) die("invalid -c core list");
                break;
            }
            case 'd': duration_s = (unsigned)strtoul(optarg, NULL, 10); break;
            default:
                fprintf(stderr, "Usage: %s [-i interval_ms] [-c core_list] [-d duration_s]\n", argv[0]);
                return EXIT_FAILURE;
        }
    }

    signal(SIGINT, on_sigint);
    signal(SIGTERM, on_sigint);

    // Initialize PQoS
    struct pqos_config cfg = {
        .fd_log = STDOUT_FILENO,
        .callback_log = NULL,
        .context_log = NULL,
        .interface = PQOS_INTER_OS // let lib pick (resctrl/MSR). You can also set AUTO.
    };
    int ret = pqos_init(&cfg);
    if (ret != PQOS_RETVAL_OK) die_pqos("pqos_init", ret);

    // Discover capabilities and CPU topology
    const struct pqos_cap *cap = NULL;
    const struct pqos_cpuinfo *cpu = NULL;
    ret = pqos_cap_get(&cap, &cpu);
    if (ret != PQOS_RETVAL_OK) die_pqos("pqos_cap_get", ret);

    const struct pqos_capability *cap_mon = NULL;
    ret = pqos_cap_get_type(cap, PQOS_CAP_TYPE_MON, &cap_mon);
    if (ret != PQOS_RETVAL_OK || cap_mon == NULL) die("Monitoring capability not present");

    const struct pqos_cap_mon *mon = cap_mon->u.mon;
    int have_tmem = 0, have_lmem = 0, have_rmem = 0;
    printf("mon num events: %u\n", mon->num_events);
    for (unsigned i = 0; i < mon->num_events; i++) {
        printf("event type %x\n", mon->events[i].type);
        if (mon->events[i].type == PQOS_MON_EVENT_TMEM_BW) have_tmem = 1;
        if (mon->events[i].type == PQOS_MON_EVENT_LMEM_BW) have_lmem = 1;
        if (mon->events[i].type == PQOS_MON_EVENT_RMEM_BW) have_rmem = 1;
    }

    if (!have_tmem && !have_lmem && !have_rmem) {
        die("MBM bandwidth monitoring not supported on this platform");
    }
    if (have_tmem) {
        printf("total memory bandwidth support found\n");
    }
    if (have_lmem) {
        printf("local memory bandwidth support found\n");
    }
    if (have_rmem) {
        printf("remote memory bandwidth support found\n");
    }

    // if (!have_tmem && !(have_lmem && have_rmem))
    //     die("MBM (TMEM_BW or LMEM_BW+RMEM_BW) not supported on this platform");

    unsigned ncores = 0;
    const unsigned *all_cores = NULL;
    all_cores = pqos_cpu_get_cores(cpu, 0 /* any socket */, &ncores);
    if (all_cores == NULL || ncores == 0) die("Failed to get cores");

    unsigned cores[4096];
    unsigned cores_cnt = 0;

    if (n_user_cores > 0) {
        for (int i = 0; i < n_user_cores; i++) cores[cores_cnt++] = user_cores[i];
    } else {
        // Use all discovered cores
        for (unsigned i = 0; i < ncores; i++) cores[cores_cnt++] = all_cores[i];
    }

    // Prepare events mask
    enum pqos_mon_event events = 0;
    if (have_tmem) events |= PQOS_MON_EVENT_TMEM_BW;
    if (have_lmem) events |= PQOS_MON_EVENT_LMEM_BW;
    if (have_rmem) events |= PQOS_MON_EVENT_RMEM_BW;

    printf("monitoring %u cores\n", cores_cnt);

    // for (unsigned i = 0; i < cores_cnt; i++) {
    //     ret = pqos_mon_stop(1 /* one core */, &cores[i], events, NULL, &groups[i]);
    //     if (ret != PQOS_RETVAL_OK) die_pqos("pqos_mon_start", ret);
    // }

    // ret = pqos_mon_start_cores(cores_cnt, &cores, events, NULL, groups);
    // if (ret != PQOS_RETVAL_OK) die_pqos("pqos_mon_start_cores", ret);

    // One monitoring group per core
    struct pqos_mon_data **group_ptrs = malloc(cores_cnt * sizeof(*group_ptrs));
    if (!group_ptrs) die("oom");
    struct pqos_mon_data *groups = calloc(cores_cnt, sizeof(*groups));
    if (!groups) die("oom");
    for (unsigned i = 0; i < cores_cnt; i++) group_ptrs[i] = &groups[i];

    for (unsigned i = 0; i < cores_cnt; i++) {
        ret = pqos_mon_start(1 /* one core */, &cores[i], events, NULL, &groups[i]);
        if (ret == PQOS_RETVAL_RESOURCE) continue;
        if (ret != PQOS_RETVAL_OK) die_pqos("pqos_mon_start", ret);
    }

    // ret = pqos_mon_start_cores(cores_cnt, &cores, events, NULL, &groups);
    // if (ret != PQOS_RETVAL_OK) die_pqos("pqos_mon_start_cores", ret);

    // Baseline poll to seed counters
    // ret = pqos_mon_poll(&groups, cores_cnt);
    // if (ret != PQOS_RETVAL_OK) die_pqos("pqos_mon_poll (baseline)", ret);

    // Store previous byte counters for delta
    uint64_t *prev_total = calloc(cores_cnt, sizeof(uint64_t));
    uint64_t *prev_local = calloc(cores_cnt, sizeof(uint64_t));
    uint64_t *prev_remote = calloc(cores_cnt, sizeof(uint64_t));
    if (!prev_total || !prev_local || !prev_remote) die("oom");

    printf("# interval_ms=%u cores=%u\n", interval_ms, cores_cnt);
    printf("time,core,MBW_total(MB/s),MBW_local(MB/s),MBW_remote(MB/s)\n");
    fflush(stdout);

    time_t start = time(NULL);

    while (!g_stop) {
        if (duration_s && (time(NULL) - start >= (time_t)duration_s)) break;

        msleep(interval_ms);

        ret = pqos_mon_poll(group_ptrs, cores_cnt);
        if (ret != PQOS_RETVAL_OK) die_pqos("pqos_mon_poll", ret);

        // Timestamp
        struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
        char tbuf[64];
        struct tm tm; localtime_r(&ts.tv_sec, &tm);
        strftime(tbuf, sizeof(tbuf), "%F %T", &tm);

        for (unsigned i = 0; i < cores_cnt; i++) {
            const struct pqos_event_values *v = &groups[i].values;

            // Deltas
            uint64_t dt = v->mbm_total_delta;
            uint64_t dl = v->mbm_local_delta;
            uint64_t dr = v->mbm_remote_delta;

            prev_total[i] += dt;
            prev_local[i] += dl;
            prev_remote[i] += dr;

            double secs = interval_ms / 1000.0;
            double mbw_t = dt / (1024.0 * 1024.0) / secs;
            double mbw_l = dl / (1024.0 * 1024.0) / secs;
            double mbw_r = dr / (1024.0 * 1024.0) / secs;

            printf("%s.%03ld,%u,%.2f,%.2f,%.2f\n",
                   tbuf, ts.tv_nsec / 1000000L, cores[i],
                   mbw_t, mbw_l, mbw_r);
        }
        fflush(stdout);
    }

    // Cleanup
    for (unsigned i = 0; i < cores_cnt; i++) {
        ret = pqos_mon_stop(&groups[i]);
        if (ret != PQOS_RETVAL_OK) die_pqos("pqos_mon_stop", ret);
    }
    free(prev_total); free(prev_local); free(prev_remote);
    free(groups);

    ret = pqos_fini();
    if (ret != PQOS_RETVAL_OK) die_pqos("pqos_fini", ret);

    return 0;
}
