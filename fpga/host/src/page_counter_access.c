#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define KB (1 << 10)
#define MB (1 << 20)
#define GB (1 << 30)

#define RESOURCE_PATH "/sys/bus/pci/devices/0000:40:00.1/resource2"
#define BYTES_TO_READ (2 * MB)

#define HUGE_PAGE_SIZE (2 * MB)
#define N_HUGEPAGES_IN_GB (512)

#define START_ADDR 0x4080000000
#define DDR_SIZE (16UL * GB)
#define END_ADDR (START_ADDR + DDR_SIZE)

#define N_HUGE_PAGES (DDR_SIZE / (HUGE_PAGE_SIZE))

static_assert(N_HUGE_PAGES == (1 << 13), "huge page count for 16GB incorrect");
static uint32_t page_access_cnts[N_HUGE_PAGES];

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

void print_access_counts(void) {
    for (size_t i = 0; i < N_HUGE_PAGES; i++) {
        printf("%u", page_access_cnts[i]);
        if ((i + 1) % 16 == 0) {
            printf("\n");
        } else {
            printf("\t");
        }
    }
}

void print_access_counts_gb(void) {
    size_t n_gbs = N_HUGE_PAGES / N_HUGEPAGES_IN_GB;

    for (size_t i = 0; i < n_gbs; i++) {
        unsigned long long sum = 0;
        for (size_t j = 0; j < N_HUGEPAGES_IN_GB; j++) {
            sum += page_access_cnts[i * N_HUGEPAGES_IN_GB + j];
        }
        printf("%12llu", sum);
        if (i + 1 == n_gbs) {
            printf("\n");
        } else {
            printf(" ");
        }
    }
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

uint32_t parse_and_validate_response(unsigned long resp,
                                     unsigned long relative_addr) {
    unsigned int page_no = extract_bits(resp, 4, 17);
    unsigned int access_count = extract_bits(resp, 20, 52);

    unsigned int expected_page_no = relative_addr / (2UL * MB);

    // TODO: suspected RTL bug: page number is modded by 4096
    assert((expected_page_no % 4096) == page_no);
    // if (expected_page_no != page_no) {
    //     printf("Expected page number %u, got %u\n", expected_page_no,
    //     page_no); printf("Response in binary:\t"); print_ul_in_binary(resp);
    //     // assert(expected_page_no == page_no);
    // }

    return access_count;
}

bool validate_register_space(unsigned long *register_space,
                             unsigned long *start_addr) {
    *start_addr = register_space[0];
    if (*start_addr != START_ADDR) {
        printf("Abort: FPGA should have physical memory range configured to "
               "%p, got %p.\n",
               (void *)START_ADDR, (void *)start_addr);
        return false;
    }
    return true;
}

unsigned long *map_and_validate_registers(char *bar_path) {
    unsigned long *register_space = map_registers(bar_path);
    if (!register_space) {
        perror("Mapping device registers failed");
        exit(1);
    }

    unsigned long start_addr = 0;
    if (!validate_register_space(register_space, &start_addr)) {
        exit(1);
    }

    return register_space;
}

int access_page_counts() {
    unsigned long *register_space = map_and_validate_registers(RESOURCE_PATH);

    while (1) {
        for (size_t i = 0; i < N_HUGE_PAGES; i++) {
            unsigned long page_start_rel_addr = i * HUGE_PAGE_SIZE;

            counter_req_t req = construct_request(page_start_rel_addr);
            write_request(register_space, req);

            unsigned long res = read_response(register_space);
            unsigned int cnt =
                parse_and_validate_response(res, page_start_rel_addr);

            page_access_cnts[i] = cnt;
        }

        print_access_counts_gb();
        sleep(2);
    }

    unmap_registers((void *)register_space);
    return 0;
}

int access_page_count(unsigned long address) {
    unsigned long *register_space = map_and_validate_registers(RESOURCE_PATH);

    assert(address >= START_ADDR); // should be validated before this fn
    unsigned long relative_addr = address - START_ADDR;

    while (1) {
        counter_req_t req = construct_request(relative_addr);
        write_request(register_space, req);

        unsigned long res = read_response(register_space);

        printf("resp in binary: ");
        print_ul_in_binary(res);

        uint32_t access_count = parse_and_validate_response(res, relative_addr);
        // printf("read from page %u\n", page_no);
        printf("count %u\n", access_count);

        sleep(1);
    }

    unmap_registers((void *)register_space);

    return 0;
}

int parse_addr_str(char *address, unsigned long *address_value) {
    char *endptr;
    unsigned long addr_val = strtoul(address, &endptr, 16);

    if (errno == ERANGE && (addr_val == ULONG_MAX || addr_val == 0)) {
        return -1;
    }
    *address_value = addr_val;

    return 0;
}

bool is_valid_addr(unsigned long address_value) {
    unsigned long start = START_ADDR;
    unsigned long end = END_ADDR;
    bool valid = address_value >= start && (address_value < end);
    if (!valid) {
        printf("Address %p not within expected range [%p, %p)\n",
               (void *)address_value, (void *)start, (void *)end);
    }
    return valid;
}

int access_single_page_count(int argc, char *argv[]) {
    char *address = argv[1];
    unsigned long address_val = 0;

    if (parse_addr_str(address, &address_val) < 0) {
        fprintf(stderr, "Invalid address value.\n");
        return 1;
    }

    if (!is_valid_addr(address_val)) {
        return 1;
    }

    printf("Reading access count for the page of this addr: %p\n",
           (void *)address_val);

    access_page_count(address_val);

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc >= 3) {
        fprintf(stderr, "Usage: %s [address]\n", argv[0]);
        return 1;
    }

    if (argc == 2) {
        access_single_page_count(argc, argv);
    } else {
        access_page_counts();
    }
}
