#include <fcntl.h>
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

int fpga_regs() {
    unsigned long *register_space = map_registers(RESOURCE_PATH);
    if (!register_space) {
        perror("Mapping device registers failed");
        return 1;
    }
    printf("registers mapped at: 0x%p\n", register_space);

    // Tell the FPGA the starting physical address of its memory.
    register_space[0] = START_ADDR;
    // an imperfect test as this value is most certainly cached, but we test
    // anyway...
    if (register_space[0] != START_ADDR) {
        printf("failed to set the device's starting address, bailing out...\n");
        exit(1);
    }

    track_read_write(register_space);

    unsigned long build_num = register_space[9];
    printf("Using build number %lu...\n", build_num);

    size_t secs = 0;
    while (1) {
        unsigned long waddr0 = register_space[3];
        unsigned long raddr0 = register_space[4];
        unsigned long waddr1 = register_space[5];
        unsigned long raddr1 = register_space[6];
        unsigned long slave_sel_0 = register_space[7];
        unsigned long slave_sel_1 = register_space[8];
        unsigned long counter_inc_addr_0 = register_space[10];
        unsigned long counter_inc_addr_1 = register_space[11];
        unsigned long counter_rd_addr_0 = register_space[12];
        unsigned long counter_rd_addr_1 = register_space[13];

        printf("#%lu waddr0 %p\t raddr0 %p\t waddr1 %p\t raddr1 %p sl0 %lu sl1 "
               "%lu inc0 %lu inc1 %lu rd0 %lu rd1 %lu\n",
               secs++, (void *)waddr0, (void *)raddr0, (void *)waddr1,
               (void *)raddr1, slave_sel_0, slave_sel_1, counter_inc_addr_0, counter_inc_addr_1,
            counter_rd_addr_0, counter_rd_addr_1);

        printf("wdata[0] ");
        for (int i = 0; i < 8; i++) {
            printf("%016lX ", register_space[32 + i]);
        }
        printf("\n");
        printf("wdata[1] ");
        for (int i = 0; i < 8; i++) {
            printf("%016lX ", register_space[40 + i]);
        }
        printf("\n");

        sleep(1);
    }

    unmap_registers((void *)register_space);

    return 0;
}

int main() { fpga_regs(); }
