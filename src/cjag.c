#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
//#include <getopt.h>
#include <time.h>
#include <sched.h>

#include "detection/paging.h"
#include "util/error.h"
#include "util/colorprint.h"

#include "cjag.h"
#include "jag/common.h"
#include "jag/send.h"
#include "jag/receive.h"
#include "detection/cache.h"
#include "detection/cpu.h"
#include "util/getopt_helper.h"

static size_t *usable_sets;

//static getopt_arg_t options[] =
//        {
//                {"receive",    no_argument,       NULL, 'r', "Start in [c]receive mode[/c] (default: [m]send mode[/m])", NULL},
//                {"threshold",  required_argument, NULL, 't', "Set the cache-miss threshold to [y]THRESHOLD[/y] (default: 280)",                             "THRESHOLD"},
//                {"delay",      required_argument, NULL, 'd', "Increase the [m]jamming[/m] and [c]listening[/c] delays by factor [y]DELAY[/y] (default: 1)", "DELAY"},
//                {"patience",   required_argument, NULL, 'p', "Sets the watchdog timeout to [y]SECONDS[/y] seconds (default: 10)",                           "SECONDS"},
//                {"cache-size", required_argument, NULL, 'c', "Set the size of the L3 cache to [y]SIZE[/y] bytes (default: auto detect)",                    "SIZE"},
//                {"ways",       required_argument, NULL, 'w', "Set the number of cache ways to [y]WAYS[/y] ways (default: auto detect)",                     "WAYS"},
//                {"slices",     required_argument, NULL, 's', "Set the number of cache slices to [y]SLICES[/y] slices (default: auto detect)",               "SLICES"},
//                {"no-color",   no_argument,       NULL, 'n', "Disable color output.",                                    NULL},
//                {"verbose",    no_argument,       NULL, 'v', "Verbose mode",                                             NULL},
//                {"help",       no_argument,       NULL, 'h', "Show this help.",                                          NULL},
//                {NULL, 0,                         NULL, 0, NULL,                                                         NULL}
//        };


void show_help()
{
    printf("cjag options:\n");
    printf("    -r (start in receive mode)\n");
    printf("    -t <int> (set cache-miss threshold)\n");
    printf("    -d <int> (set jamming and listening delays by given factor)\n");
    //printf("    -p <int> (set watchdog timeout to given seconds)\n");
    printf("    -c <int> (set size of L3 cache in bytes)\n");
    printf("    -w <int> (set number of cache ways)\n");
    printf("    -s <int> (set number of cache slices)\n");

}

void init_options(int argc, const char **argv, cjag_config_t *config)
{
    for (int i = 1; i < argc;) {

        const char *opt = argv[i];

        if (opt[0] != '-') {
            printf("Only short options are allowed\n");
            exit(1);
        }

        if (opt[1] == 'r') {
            config->send = 0;
            i++;
            continue;
        }

        if (i + 1 >= argc) {
            printf("-%c needs int arg after it\n", opt[1]);
            exit(1);
        }

        switch (opt[1]) {
        case 'c':
            config->cache_size = atoi(argv[i + 1]);
            i += 2;
            break;
        case 't':
            config->cache_miss_threshold = atoi(argv[i + 1]);
            i += 2;
            break;
        case 'w':
            config->cache_ways = atoi(argv[i + 1]);
            config->cache_kill_count = config->cache_ways - 1;
            i += 2;
            break;
        case 's':
            config->cache_slices = atoi(argv[i + 1]);
            i += 2;
            break;
        case 'n':
            config->color_output = 0;
            i++;
            break;
        case 'd':
            config->jag_send_count = config->jag_send_count * atoi(argv[i + 1]);
            config->jag_recv_count = config->jag_recv_count * atoi(argv[i + 1]);
            i += 2;
            break;
        case 'p':
            config->timeout = atoi(argv[i + 1]);
            i += 2;
            break;
        case 'v':
            config->verbose = 1;
            i++;
            break;
        case 'h':
            show_help(argv[0], &config);
            exit(0);
            break;
        default:
            printf("cjag: unknown argument: %s\n", opt);
            show_help(argv[0], &config);
            exit(0);
        }
    }
}

int main(int argc, char **argv) {
//    printf("cjag: argc = %d\n", argc);
//
//    for (int i = 0; i < argc; i++) {
//        printf("cjag: argv[%d] = %s\n", i, argv[i]);
//    }

    watchdog_t watchdog_settings;
    cjag_config_t config = {
            .send = 1,
            .color_output = 1,
            .channels = 6,
            .cache_miss_threshold = 280,
            .cache_probe_count = 3,
            .cache_slices = 4,
            .jag_send_count = 5000 * 3,
            .jag_recv_count = 15000 * 3,
            .set_offset = 10,
            .verbose = 0,
            .timeout = 10,
            .watchdog = &watchdog_settings
    };

    cache_config_l3_t l3 = get_l3_info();
    config.cache_size = l3.size;
    config.cache_ways = l3.ways;
    config.cache_kill_count = config.cache_ways - 1;
    //config.cache_slices = get_slices();

    //struct option *long_options = getopt_get_long_options((getopt_arg_t *) options);
    //ERROR_ON(!long_options, ERROR_OOM, config.color_output);

    init_options(argc, argv, &config);
    //free(long_options);

    show_welcome(&config);
    show_parameters(&config);

    ERROR_ON(config.cache_slices > 16, ERROR_TOO_MANY_CPUS, config.color_output);
    ERROR_ON(config.cache_slices < 1 || config.cache_slices > 8 || (config.cache_slices & (config.cache_slices - 1)),
             ERROR_SLICES_NOT_SUPPORTED, config.color_output);
    ERROR_ON(config.cache_ways < 2, ERROR_INVALID_WAYS, config.color_output);
    ERROR_ON(config.timeout <= 1, ERROR_TIMEOUT_TOO_LOW, config.color_output);

    ERROR_ON(config.jag_send_count <= 0 || config.jag_recv_count <= 0, ERROR_INVALID_SPEED, config.color_output);

    //ERROR_ON(!has_huge_pages(), ERROR_NO_HUGEPAGES, config.color_output);

    printf("Pre-init config:\n");
    printf("    cache_slices = %d\n", config.cache_slices);
    printf("    cache_ways = %d\n", config.cache_ways);
    printf("    timeout = %d\n", config.timeout);
    printf("    send_count = %d\n", config.jag_send_count);
    printf("    recv_count = %d\n", config.jag_recv_count);
    int init = jag_init(&config);
    ERROR_ON(!init, ERROR_NO_CACHE_SETS, config.color_output);

    usable_sets = calloc(config.channels, sizeof(size_t));
    ERROR_ON(!usable_sets, ERROR_OOM, config.color_output);

//    watchdog_start(config.watchdog, config.timeout, timeout, (void *) &config);

    void **addrs = malloc(config.cache_ways * config.channels * sizeof(void *));
    if (config.send) {
        printf_color(config.color_output, "[y]Send mode[/y]: start [m]jamming[/m]...\n\n");
        printf_color(config.color_output, "[g][ # ][/g] Jamming set...\n");
//        watchdog_reset(config.watchdog);
        jag_send(&config, send_callback);
        if(config.verbose) {
            printf_color(config.color_output, "[y]Eviction sets[/y]:\n");
            print_eviction_sets(config.addr, &config);
        }

        printf_color(config.color_output, "\n[y]Verification mode[/y]: start [c]receiving[/c]...\n\n");
//        watchdog_reset(config.watchdog);
//        sleep(1);
//        watchdog_reset(config.watchdog);
//        sleep(1);
//        for (int i = 0; i < 15; i++) {
//            watchdog_reset(config.watchdog);
//            sched_yield();
//        }

        printf_color(config.color_output, "[g][ # ][/g] Checking sets...\n");
//        watchdog_reset(config.watchdog);
        jag_receive(addrs, usable_sets, &config, watch_callback);
//        watchdog_done(config.watchdog);
        if(config.verbose) {
            printf_color(config.color_output, "[y]Eviction sets[/y]:\n");
            print_eviction_sets(addrs, &config);
        }

        int correct = 0;
        for (int i = 0; i < config.channels; i++) {
            if (i == usable_sets[i] - config.set_offset) {
                correct++;
            }
        }

        printf_color(config.color_output,
                     "\n[g][ # ][/g] Done. [g]%.2f%%[/g] of the channels are established, your system is [r]%s[/r][y]%s[/y].\n\n",
                     (correct * 100.0 / config.channels), correct ? "[ V U L N E R A B L E ]" : "",
                     correct ? "" : "[ maybe not vulnerable ]");
    } else {
        printf_color(config.color_output, "[y]Receive mode[/y]: start [c]listening[/c]...\n\n");
        printf_color(config.color_output, "[g][ # ][/g] Receiving sets...\n");
        exit(0);
//        watchdog_reset(config.watchdog);
        jag_receive(addrs, usable_sets, &config, receive_callback);
        if(config.verbose) {
            printf_color(config.color_output, "[y]Eviction sets[/y]:\n");
            print_eviction_sets(addrs, &config);
        }

        printf_color(config.color_output, "\n[g][ # ][/g] Reconstructing mapping...\n");
        for (int i = 0; i < config.channels; i++) {
            printf_color(config.color_output, "[g][ + ][/g]   Sender[[m]%d[/m]] -> Receiver[[c]%lu[/c]]\n", i,
                         usable_sets[i]);
        }

        printf_color(config.color_output, "\n[y]Test mode[/y]: start [m]probing[/m]...\n\n");
//        watchdog_reset(config.watchdog);
//        sleep(1);
//        for (int i = 0; i < 15; i++) {
//            watchdog_reset(config.watchdog);
//            sched_yield();
//        }

//        watchdog_reset(config.watchdog);
        void *old_addr = config.addr;
        config.addr = addrs; // send back the received addresses
        jag_send(&config, probe_callback);
        config.addr = old_addr; // bit of a hack to prevent double free and memory leak...
//        watchdog_done(config.watchdog);
        printf_color(config.color_output, "\n[g][ # ][/g] Done!\n\n");
    }

    ERROR_ON(!jag_free(&config), ERROR_UNMAP_FAILED, config.color_output);
    free(usable_sets);
    free(addrs);

    return 0;
}


void show_welcome(cjag_config_t *config) {
    printf_color(config->color_output,
                 "\n___[y]/\\/\\/\\/\\/\\[/y]__________[r]/\\/\\[/r]______[r]/\\/\\[/r]________[r]/\\/\\/\\/\\/\\[/r]_\n");
    printf_color(config->color_output,
                 "_[y]/\\/\\[/y]__________________[r]/\\/\\[/r]____[r]/\\/\\/\\/\\[/r]____[r]/\\/\\[/r]_________\n");
    printf_color(config->color_output,
                 "_[y]/\\/\\[/y]__________________[r]/\\/\\[/r]__[r]/\\/\\[/r]____[r]/\\/\\[/r]__[r]/\\/\\[/r]__[r]/\\/\\/\\[/r]_\n");
    printf_color(config->color_output,
                 "_[y]/\\/\\[/y]__________[r]/\\/\\[/r]____[r]/\\/\\[/r]__[r]/\\/\\/\\/\\/\\/\\[/r]__[r]/\\/\\[/r]____[r]/\\/\\[/r]_\n");
    printf_color(config->color_output,
                 "___[y]/\\/\\/\\/\\/\\[/y]____[r]/\\/\\/\\/\\[/r]____[r]/\\/\\[/r]____[r]/\\/\\[/r]____[r]/\\/\\/\\/\\/\\[/r]_\n");
    printf_color(config->color_output, "________________________________________________________\n\n");
}

void show_parameters(cjag_config_t *config) {
    int size = config->cache_size / 1024;
    const char *unit = "KB";
    if (size >= 1024 && size % 1024 == 0) {
        size /= 1024;
        unit = "MB";
    }
    printf_color(config->color_output, "      %10s %10s %10s %10s\n", "Size", "Ways", "Slices", "Threshold");
    printf_color(config->color_output, "L3    [g]%7d %s[/g] [g]%9d[/g]  [g]%7d[/g]  [g]%9d[/g]\n\n", size, unit,
                 config->cache_ways, config->cache_slices, config->cache_miss_threshold);
}

//void show_usage(char *binary, cjag_config_t *config) {
//    printf_color(config->color_output, "[y]USAGE[/y]\n  %s [g][options][/g] \n\n", binary);
//    getopt_arg_t null_option = {0};
//    size_t count = 0;
//    printf_color(config->color_output, "\n[y]OPTIONS[/y]\n");
//    do {
//        if (!memcmp((void *) &options[count], (void *) &null_option, sizeof(getopt_arg_t))) {
//            break;
//        } else if (options[count].description) {
//            printf_color(config->color_output, "  [g]-%c[/g]%s[y]%s[/y]%s [g]%s%s%s[/g][y]%s[/y]\n     ",
//                         options[count].val,
//                         options[count].has_arg != no_argument ? " " : "",
//                         options[count].has_arg != no_argument ? options[count].arg_name : "",
//                         options[count].name ? "," : "", options[count].name ? "--" : "", options[count].name,
//                         options[count].has_arg != no_argument ? "=" : "",
//                         options[count].has_arg != no_argument ? options[count].arg_name : "");
//            printf_color(config->color_output, options[count].description);
//            printf_color(config->color_output, "\n\n");
//        }
//        count++;
//    } while (1);
//    printf("\n");
//    printf_color(config->color_output,
//                 "[y]EXAMPLE[/y]\n  Start [m]%s[/m] and [c]%s --receive[/c] in two different terminals (or on two co-located VMs).\n\n\n",
//                 binary, binary);
//    printf_color(config->color_output,
//                 "[y]AUTHORS[/y]\n  Written by Michael Schwarz, Lukas Giner, Daniel Gruss, Clémentine Maurice and Manuel Weber.\n\n");
//
//}

void send_callback(cjag_config_t *config, int set) {
    watchdog_reset(config->watchdog);
    printf_color(config->color_output, "[g][ + ][/g]   ...set #%d\n", set);
}

void probe_callback(cjag_config_t *config, int set) {
    watchdog_reset(config->watchdog);
    printf_color(config->color_output, "[g][ + ][/g] Probing set #%d ([[c]%d[/c]] -> [[m]%d[/m]])\n", set,
                 usable_sets[set - 1], set - 1);
}

void receive_callback(cjag_config_t *config, int set) {
    watchdog_reset(config->watchdog);
    printf_color(config->color_output, "[g][ + ][/g]   ...set #%d\n", set);
}

void watch_callback(cjag_config_t *config, int set) {
    watchdog_reset(config->watchdog);
    printf("usable_sets = %p\n", (void *)usable_sets);
    printf_color(config->color_output, "[g][ + ][/g]   Sender[[m]%d[/m]] <-> Sender[[c]%lu[/c]] [g]%s[/g][r]%s[/r]\n",
                 set - 1, usable_sets[set - 1] - config->set_offset,
                 (set - 1 == (usable_sets[set - 1] - config->set_offset)) ? "[ OK ]" : "",
                 (set - 1 != (usable_sets[set - 1] - config->set_offset)) ? "[FAIL]" : "");
}

void timeout(void *arg) {
    cjag_config_t *config = (cjag_config_t *) arg;

    printf_color(config->color_output,
                 ERROR_TAG "Timeout after [g]%d seconds[/g].\n        Please [y]try to restart[/y] the application.\n\n",
                 config->timeout);
    exit(1);
}

void print_eviction_sets(void** addr, cjag_config_t* config) {
    for (int i = 0; i < config->channels; i++) {
        printf_color(config->color_output, "[g][ . ] Set #%d[/g]\n", i + 1);
        for (int j = 0; j < config->cache_ways; j++) {
            void* v_addr = GET_EVICTION_SET(addr, i, config)[j];
            printf_color(config->color_output, "  %p\n", v_addr);
        }
    }
}
