#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <signal.h>

#define KB (1L << 10)
#define MB (1L << 20)
#define GB (1L << 30)

#define RESOURCE_PATH "/sys/bus/pci/devices/0000:40:00.1/resource2"
#define BYTES_TO_READ (2 * MB)
#define START_ADDR 0x4080000000
#define DDR_SIZE (16UL * GB)
#define END_ADDR (START_ADDR + DDR_SIZE)

typedef unsigned long counter_req_t;

char *format_number_with_commas(unsigned long number) {
    // Convert the number to a string
    char temp[48];
    sprintf(temp, "%lu", number);

    // Calculate the length of the number string
    int len = strlen(temp);

    // Calculate the length of the result string
    int commas = (len - 1) / 3;
    int result_len = len + commas;

    // Allocate memory for the result string
    char *result = (char *)malloc(result_len + 1);
    if (!result) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    // Insert commas into the result string
    int j = result_len;
    result[j--] = '\0';
    for (int i = len - 1; i >= 0; i--) {
        result[j--] = temp[i];
        if ((len - i) % 3 == 0 && i != 0) {
            result[j--] = ',';
        }
    }

    return result;
}

// Function to print an unsigned long in binary
void print_ul_in_binary(unsigned long n) {
    int bits = sizeof(unsigned long) *
               8; // Calculate the number of bits in an unsigned long
    for (int i = bits - 1; i >= 0; i--) {
        unsigned long mask = 1UL << i; // Create a mask to isolate each bit
        putchar((n & mask)
                    ? '1'
                    : '0'); // Print '1' if the bit is set, otherwise print '0'
    }
    putchar('\n'); // Print a newline at the end
}

void *map_registers(char *bar_file_path) {
    int fd;
    void *register_space;
    fd = open(bar_file_path, O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("Open failed");
        return NULL;
    }

    register_space =
        mmap(NULL, BYTES_TO_READ, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (register_space == MAP_FAILED) {
        perror("mmap failed");
        return NULL;
    }

    return register_space;
}

void unmap_registers(void *register_space) {
    munmap((void *)register_space, BYTES_TO_READ);
}

counter_req_t construct_request(size_t relative_addr) {
    // Format of the request:
    //
    // 8 byte value where the last bit is a valid bit (must be 1).
    // The remaining bits are the actual address to read, offset by the
    // starting address.
    //
    // Based on this address, FPGA calculates which page this address belongs
    // to, and returns the count for that page.

    return (relative_addr << 1) + 1;
}

void write_request(unsigned long *register_space, counter_req_t req) {
    register_space[1] = req;
    printf("request written\n");
}

unsigned long read_response(unsigned long *register_space) {
    unsigned long resp = 0;
    do {
        resp = register_space[2];
    } while ((resp & 1) == 0);
    return resp;
}

unsigned int extract_bits(unsigned long num, int start_bit, int end_bit) {
    // Calculate the number of bits to extract
    int num_bits = end_bit - start_bit;

    // Create a mask to extract the required bits
    unsigned long mask = ((1UL << num_bits) - 1) << start_bit;

    // Extract the bits and shift them to the rightmost position
    unsigned int result = (num & mask) >> start_bit;

    return result;
}

void parse_and_process_resp(unsigned long resp) {
    unsigned int page_no = extract_bits(resp, 4, 17);
    unsigned int access_count = extract_bits(resp, 20, 52);

    printf("read from page %u\n", page_no);
    printf("count %u\n", access_count);
}

// Configure FPGA counters to only track memory reads.
void track_read_only(unsigned long *register_space) {
    register_space[16] = 0b001;
}

// Configure FPGA counters to only track memory writes.
void track_write_only(unsigned long *register_space) {
    register_space[16] = 0b010;
}

// Configure FPGA counters to track both memory reads and writes.
void track_read_write(unsigned long *register_space) {
    register_space[16] = 0b100;
}

uint64_t divup(uint64_t a, uint64_t b) {
    return (a + b - 1) / b;
}

#define AR_S (0)
#define AR_ID (12)
#define AR_ADDR (64)
#define AR_LEN (10)
#define AR_SIZE (3)
#define AR_BURST (2)
#define AR_PROT (3)
#define AR_QOS (4)
#define AR_VALID (1)
#define AR_CACHE (4)
#define AR_LOCK (2)
#define AR_REGION (4)
#define AR_USER (6)

#define AR_S_ACC (0)
#define AR_ID_ACC (AR_S_ACC + AR_ID)
#define AR_ADDR_ACC (AR_ID_ACC + AR_ADDR)
#define AR_LEN_ACC (AR_ADDR_ACC + AR_LEN)
#define AR_SIZE_ACC (AR_LEN_ACC + AR_SIZE)
#define AR_BURST_ACC (AR_SIZE_ACC + AR_BURST)
#define AR_PROT_ACC (AR_BURST_ACC + AR_PROT)
#define AR_QOS_ACC (AR_PROT_ACC + AR_QOS)
#define AR_VALID_ACC (AR_QOS_ACC + AR_VALID)
#define AR_CACHE_ACC (AR_VALID_ACC + AR_CACHE)
#define AR_LOCK_ACC (AR_CACHE_ACC + AR_LOCK)
#define AR_REGION_ACC (AR_LOCK_ACC + AR_REGION)
#define AR_USER_ACC (AR_REGION_ACC + AR_USER)

#define ar_id(n) get_bits(n, AR_ID_ACC-1, AR_S_ACC)
#define ar_addr(n) get_bits(n, AR_ADDR_ACC-1, AR_ID_ACC)
#define ar_len(n) get_bits(n, AR_LEN_ACC-1, AR_ADDR_ACC)
#define ar_size(n) get_bits(n, AR_SIZE_ACC-1, AR_LEN_ACC)
#define ar_burst(n) get_bits(n, AR_BURST_ACC-1, AR_SIZE_ACC)
#define ar_prot(n) get_bits(n, AR_PROT_ACC-1, AR_BURST_ACC)
#define ar_qos(n) get_bits(n, AR_QOS_ACC-1, AR_PROT_ACC)
#define ar_valid(n) get_bits(n, AR_VALID_ACC-1, AR_QOS_ACC)
#define ar_cache(n) get_bits(n, AR_CACHE_ACC-1, AR_VALID_ACC)
#define ar_lock(n) get_bits(n, AR_LOCK_ACC-1, AR_CACHE_ACC)
#define ar_region(n) get_bits(n, AR_REGION_ACC-1, AR_LOCK_ACC)
#define ar_user(n) get_bits(n, AR_USER_ACC-1, AR_REGION_ACC)

//#define ar_fifo_full(n) get_bits(n, 126, 126)
//#define ar_fifo_empty_n(n) get_bits(n, 127, 127)

#define ar_fifo_full(n) (uint64_t)(((n)[15] & 0x40) >> 6)
#define ar_fifo_empty_n(n) (uint64_t)(((n)[15] & 0x80) >> 7)

char get_bit(char* in, int bit) {
    int byte = bit / 8;
    int bitoff = bit % 8;
    return (in[byte] & (1 << bitoff)) != 0;
}
void set_bit(char* in, int bit) {
    int byte = bit / 8;
    int bitoff = bit % 8;
    in[byte] |= (1 << bitoff);
}

// start, end inclusive
uint64_t get_bits(char* b, uint64_t end, uint64_t start) {
    uint64_t oldend = end;
    uint64_t oldstart = start;
    // messed up HW endianness, oops
    start = AR_USER_ACC-oldend-1;
    end = AR_USER_ACC-oldstart-1;
    if (start > end) {
        perror("oh no1");
        exit(1);
    }
    uint64_t len = end - start + 1;
    if (len > 64) {
        perror("oh no2");
        exit(1);
    }
    uint64_t ret = 0;
    char *ret_bytes = (char*)&ret;
    for (int i = 0; i < len; i++) {
        if (get_bit(b, i+start)) {
            set_bit(ret_bytes, i);
        }
    }
    return ret;
}

typedef struct {
    uint64_t p1;
    uint64_t p2;
} ar_entry;

ar_entry get_ar(int chan, volatile unsigned long *register_space) {
    uint64_t part1_addr = chan == 0 ? 0xaaa : 0xaba;
    uint64_t part2_addr = chan == 0 ? 0xaab : 0xabb;
    ar_entry ar;
    ar.p1 = register_space[part1_addr];
    ar.p2 = register_space[part2_addr];
    return ar;
}

FILE *fil;
bool syncfile;

uint64_t errbus_old = 0;
void print_errbus(volatile unsigned long *register_space) {
    uint64_t errbus = register_space[2];
    if (errbus != errbus_old) {
        fprintf(fil, "ERROR: BUS ERROR 0x%lx\n", errbus);
        fprintf(stderr, "ERROR: BUS ERROR 0x%lx\n", errbus);
        if (syncfile) {
            fflush(fil);
            fsync(fileno(fil));
        }
        errbus_old = errbus;
    }
}

// AR chan=0 id=254 addr=408006c780 len=0 size=6 burst=0 prot=0 qos=0 valid=1 cache=0 lock=0 region=0 user=2 fifo_full=0 fifo_empty_n=1 raw=80007f0000002040 363c00018010002
bool weird(int chan, ar_entry *e) {
    char* a = (char*)e;
    if (ar_len(a) != 0) return true;
    if (ar_size(a) != 6) return true;
    if (ar_burst(a) != 0) return true;
    if (ar_prot(a) != 0) return true;
    if (ar_qos(a) != 0) return true;
    if (ar_valid(a) != 1) return true;
    if (ar_cache(a) != 0) return true;
    if (ar_lock(a) != 0) return true;
    if (ar_region(a) != 0) return true;
    if (ar_user(a) != 2) return true;
    if (ar_fifo_full(a) != 0) return true;
    if (ar_fifo_empty_n(a) != 1) return true;
    return false;
}

void print_ar_entry(int chan, ar_entry *e) {
    char* a = (char*)e;
    fprintf(fil, "AR chan=%d id=%ld addr=%lx len=%ld size=%ld burst=%ld prot=%lx qos=%lx valid=%ld cache=%lx lock=%lx region=%lx user=%lx fifo_full=%ld fifo_empty_n=%ld raw=%lx %lx",
    chan, ar_id(a), ar_addr(a), ar_len(a), ar_size(a), ar_burst(a), ar_prot(a),
    ar_qos(a), ar_valid(a), ar_cache(a), ar_lock(a), ar_region(a), ar_user(a), ar_fifo_full(a), ar_fifo_empty_n(a),
    e->p2, e->p1);
    if (weird(chan, e)) {
        fprintf(fil, " ERROR\n");
        fprintf(stderr, "ERROR\n");
    }
    else {
        fprintf(fil, "\n");
    }
    if (syncfile) {
        fflush(fil);
        fsync(fileno(fil));
    }
}

static volatile sig_atomic_t keep_running = 1;

static void sig_handler(int _)
{
    (void)_;
    keep_running = 0;
}


int n_chan = 2;
void empty_ar_logs(volatile unsigned long *register_space) {
    int n_enabled = n_chan;
    bool enabled[n_chan];
    for (int i = 0; i < n_chan; i++) enabled[i] = true;
    ar_entry e;
    while (keep_running && n_enabled) {
        print_errbus(register_space);
        for (int chan = 0; chan < n_chan; chan++) {
            if (enabled[chan]) {
                e = get_ar(chan, register_space);
                if (!ar_fifo_empty_n((char*)&e)) {
                    enabled[chan] = false;
                    n_enabled--;
                } else {
                    print_ar_entry(chan, &e);
                }
            }
        }
    }
    if (n_enabled) {
        printf("Queues still have things in them.\n");
    }
}


void poll_ar_logs(volatile unsigned long *register_space) {
    int n_empty_n = -1;
    bool empty_n[n_chan];
    for (int i = 0; i < n_chan; i++) empty_n[i] = false;
    ar_entry e;
    char* r = (char*)&e;
    while (true) {
        print_errbus(register_space);
        for (int chan = 0; chan < n_chan; chan++) {
            e = get_ar(chan, register_space);
            if (!ar_fifo_empty_n(r)) {
                if (empty_n[chan]) {
                    n_empty_n--;
                }
                empty_n[chan] = false;
            } else {
                print_ar_entry(chan, &e);
                if (n_empty_n == -1) {
                    fprintf(stderr, "Queues not empty! reading....\n");
                    n_empty_n = n_chan;
                    for (int i = 0; i < n_chan; i++) empty_n[i] = true;
                }
            }
        }
        if (n_empty_n == 0) {
            n_empty_n = -1;
            fflush(fil);
            fsync(fileno(fil));
            fprintf(stderr, "Queues emptied.\n");
        }
        if (!keep_running) {
            fflush(fil);
            fsync(fileno(fil));
            break;
        }
    }
}

int fpga_regs() {
    volatile uint64_t *register_space = map_registers(RESOURCE_PATH);
    if (!register_space) {
        perror("Mapping device registers failed");
        return 1;
    }
    /*

    | --------------- 16 -------------- |
    | start | usable              | end |

    start = 0x010200000 bytes
    claim = 0x3EFE00000 usable bytes,
    actual= 0x3E0000000 usable bytes.

    end = start_phys + actual;
    start_phys = phys_map_addr + start;

    */
    printf("registers mapped at: 0x%p\n", register_space);
    const uint64_t phys_map_addr = 0x4080000000L;
    //const uint64_t phys_size = 16L * GB;
    //const uint64_t phys_size_usable_reported = 16909336576L; // according to ndctl list
    const uint64_t phys_size_usable_actual = 0x3E0000000L; // trial and error
    const uint64_t dax_size_start = 0x10200000L; // 0.25 GiB + 2MiB
    // start + phys_size_usable_actual + end = phys_size
    //const uint64_t dax_size_end = phys_size - phys_size_usable_actual - dax_size_start; // 254 MiB
    const uint64_t dax_end = dax_size_start + phys_size_usable_actual;
    //printf("typical 0x20008 (start) = 0x%lx, typical 0x20010 (end) = 0x%lx, typical 0x20018 (shift) = 9 (for a page)\n", phys_map_addr + dax_size_start, phys_map_addr + phys_size - dax_size_end);
    printf("typical 0x20008 (start) = 0x%lx, typical 0x20010 (end) = 0x%lx, typical 0x20018 (shift) = 9 (for a page)\n", phys_map_addr + dax_size_start, phys_map_addr + dax_end);

    signal(SIGINT, sig_handler);

    printf("Register Interface Ready. Use 'r <addr>' or 'w <addr> <data>' (type 'exit' to quit)\n");
#define INPUT_LINE_SIZE 128
    char line[INPUT_LINE_SIZE];
    uint64_t REG_SPACE_SIZE = 2 * MB;

    while (keep_running) {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)) {
            break; // EOF or error
        }

        // Remove trailing newline
        line[strcspn(line, "\n")] = 0;

        if (strcmp(line, "exit") == 0) {
            break;
        }

        char *cmd = strtok(line, " \t");
        if (!cmd) {
            continue;
        }

        if (strcmp(cmd, "r") == 0) {
            char *addr_str = strtok(NULL, " \t");
            if (!addr_str) {
                printf("Usage: r <addr>\n");
                continue;
            }

            char *endptr;
            uint64_t addr = strtoull(addr_str, &endptr, 0);

            if (*endptr != '\0' || addr >= REG_SPACE_SIZE || (addr % sizeof(uint64_t) != 0)) {
                printf("Error: Invalid, unaligned, or out-of-bounds address.\n");
                continue;
            }

            uint64_t data;
            //if (addr % sizeof(uint64_t) != 0) {
                data = register_space[addr / sizeof(addr)];
                /*
            } else {
                volatile char *reg_space_char = (volatile char*)register_space;
                volatile char *reg_with_offset = reg_space_char + addr;
                volatile uint64_t *reg_u64 = (volatile uint64_t*)reg_with_offset;
                data = *reg_u64;
            }
            */

            printf("register_space[%lu] = 0x%016lx\n", addr, data);

        } else if (strcmp(cmd, "w") == 0) {
            char *addr_str = strtok(NULL, " \t");
            char *data_str = strtok(NULL, " \t");

            if (!addr_str || !data_str) {
                printf("Usage: w <addr> <data>\n");
                continue;
            }

            char *endptr1, *endptr2;
            uint64_t addr = strtoull(addr_str, &endptr1, 0);
            uint64_t data = strtoull(data_str, &endptr2, 0);

            if (*endptr1 != '\0' || *endptr2 != '\0' || addr >= REG_SPACE_SIZE || (addr % sizeof(uint64_t) != 0)) {
                printf("Error: Invalid, unaligned, or out-of-bounds address or data.\n");
                continue;
            }

            register_space[addr / sizeof(addr)] = data;
            printf("Wrote 0x%016lx to register_space[%lu]\n", data, addr);

        } else {
            printf("Unknown command. Use 'r <addr>' or 'w <addr> <data>'\n");
        }
    }

    unmap_registers((void *)register_space);

    return 0;
}

int main(int argc, char** argv) {
    fpga_regs();
}
