#include <algorithm>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class UnorderedMapBenchmark {
   private:
    size_t target_memory_gb;
    size_t num_elements;
    std::unordered_map<uint64_t, uint64_t> map;
    std::vector<uint64_t> keys;
    std::mt19937_64 rng;

    // Each entry is roughly 16 bytes (key) + 16 bytes (value) + overhead
    // Estimate ~48 bytes per entry including hash table overhead
    static constexpr size_t BYTES_PER_ENTRY = 48;

   public:
    UnorderedMapBenchmark(size_t memory_gb)
        : target_memory_gb(memory_gb), rng(std::random_device{}()) {
        size_t target_bytes = memory_gb * 1024ULL * 1024ULL * 1024ULL;
        num_elements = target_bytes / BYTES_PER_ENTRY;

        std::cout << "Target memory: " << memory_gb << " GB\n";
        std::cout << "Estimated elements: " << num_elements << "\n";
        std::cout << "Estimated memory per element: " << BYTES_PER_ENTRY
                  << " bytes\n\n";

        generateKeys();
    }

    void generateKeys() {
        std::cout << "Generating " << num_elements << " unique keys...\n";
        auto start = std::chrono::high_resolution_clock::now();

        keys.reserve(num_elements);
        std::uniform_int_distribution<uint64_t> dist;

        // Generate unique keys
        std::unordered_set<uint64_t> unique_keys;
        while (unique_keys.size() < num_elements) {
            uint64_t key = dist(rng);
            if (unique_keys.insert(key).second) {
                keys.push_back(key);
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Key generation took: " << duration.count() << " ms\n\n";
    }

    void benchmarkInsert() {
        std::cout << "=== INSERT BENCHMARK ===\n";
        map.clear();
        map.reserve(num_elements);

        auto start = std::chrono::high_resolution_clock::now();

        for (size_t i = 0; i < num_elements; ++i) {
            map[keys[i]] = keys[i] * 2;  // Simple value transformation
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "Inserted " << num_elements << " elements\n";
        std::cout << "Time: " << duration.count() << " ms\n";
        std::cout << "Rate: " << (num_elements * 1000.0 / duration.count())
                  << " insertions/sec\n";
        std::cout << "Map size: " << map.size() << "\n";
        std::cout << "Load factor: " << std::fixed << std::setprecision(3)
                  << map.load_factor() << "\n\n";
    }

    void benchmarkLookup() {
        std::cout << "=== LOOKUP BENCHMARK ===\n";

        // Shuffle keys for random access pattern
        std::vector<uint64_t> lookup_keys = keys;
        std::shuffle(lookup_keys.begin(), lookup_keys.end(), rng);

        size_t found_count = 0;
        auto start = std::chrono::high_resolution_clock::now();

        for (const auto& key : lookup_keys) {
            auto it = map.find(key);
            if (it != map.end()) {
                found_count++;
                // Access the value to prevent optimization
                volatile uint64_t val = it->second;
                (void)val;
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "Looked up " << lookup_keys.size() << " elements\n";
        std::cout << "Found: " << found_count << " elements\n";
        std::cout << "Time: " << duration.count() << " ms\n";
        std::cout << "Rate: "
                  << (lookup_keys.size() * 1000.0 / duration.count())
                  << " lookups/sec\n\n";
    }

    void benchmarkMissedLookup() {
        std::cout << "=== MISSED LOOKUP BENCHMARK ===\n";

        // Generate keys that don't exist in the map
        std::vector<uint64_t> missed_keys;
        missed_keys.reserve(
            std::min(num_elements,
                     static_cast<size_t>(
                         1000000ULL)));  // Limit to 1M for reasonable test time

        std::uniform_int_distribution<uint64_t> dist;
        size_t target_misses =
            std::min(num_elements, static_cast<size_t>(1000000ULL));

        for (size_t i = 0; i < target_misses; ++i) {
            uint64_t key;
            do {
                key = dist(rng);
            } while (map.find(key) != map.end());
            missed_keys.push_back(key);
        }

        size_t missed_count = 0;
        auto start = std::chrono::high_resolution_clock::now();

        for (const auto& key : missed_keys) {
            auto it = map.find(key);
            if (it == map.end()) {
                missed_count++;
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "Attempted " << missed_keys.size() << " missed lookups\n";
        std::cout << "Actually missed: " << missed_count << " elements\n";
        std::cout << "Time: " << duration.count() << " ms\n";
        std::cout << "Rate: "
                  << (missed_keys.size() * 1000.0 / duration.count())
                  << " missed lookups/sec\n\n";
    }

    void benchmarkIteration() {
        std::cout << "=== ITERATION BENCHMARK ===\n";

        size_t count = 0;
        uint64_t sum = 0;
        auto start = std::chrono::high_resolution_clock::now();

        for (const auto& pair : map) {
            count++;
            sum += pair.second;
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "Iterated over " << count << " elements\n";
        std::cout << "Sum: " << sum << "\n";
        std::cout << "Time: " << duration.count() << " ms\n";
        std::cout << "Rate: " << (count * 1000.0 / duration.count())
                  << " iterations/sec\n\n";
    }

    void benchmarkErase() {
        std::cout << "=== ERASE BENCHMARK ===\n";

        // Erase 10% of elements
        size_t erase_count = num_elements / 10;
        std::vector<uint64_t> erase_keys(keys.begin(),
                                         keys.begin() + erase_count);
        std::shuffle(erase_keys.begin(), erase_keys.end(), rng);

        size_t erased = 0;
        auto start = std::chrono::high_resolution_clock::now();

        for (const auto& key : erase_keys) {
            erased += map.erase(key);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "Attempted to erase " << erase_keys.size()
                  << " elements\n";
        std::cout << "Actually erased: " << erased << " elements\n";
        std::cout << "Time: " << duration.count() << " ms\n";
        std::cout << "Rate: " << (erased * 1000.0 / duration.count())
                  << " erasures/sec\n";
        std::cout << "Final map size: " << map.size() << "\n\n";
    }

    void printMemoryStats() {
        std::cout << "=== MEMORY STATISTICS ===\n";
        std::cout << "Map size: " << map.size() << " elements\n";
        std::cout << "Bucket count: " << map.bucket_count() << "\n";
        std::cout << "Load factor: " << std::fixed << std::setprecision(3)
                  << map.load_factor() << "\n";
        std::cout << "Max load factor: " << map.max_load_factor() << "\n";

        // Estimate actual memory usage
        size_t estimated_memory = map.size() * BYTES_PER_ENTRY;
        std::cout << "Estimated memory usage: "
                  << (estimated_memory / (1024.0 * 1024.0 * 1024.0))
                  << " GB\n\n";
    }

    void runAllBenchmarks() {
        benchmarkInsert();
        benchmarkLookup();
        benchmarkMissedLookup();
        benchmarkIteration();
        benchmarkErase();
        printMemoryStats();
    }
};

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " <memory_size_gb>\n";
    std::cout << "  memory_size_gb: Target memory usage in gigabytes (e.g., 1, "
                 "2, 4)\n";
    std::cout << "Example: " << program_name << " 2\n";
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printUsage(argv[0]);
        return 1;
    }

    // Parse memory size
    char* endptr;
    long memory_gb = std::strtol(argv[1], &endptr, 10);

    if (*endptr != '\0' || memory_gb <= 0 || memory_gb > 100) {
        std::cout << "Error: Invalid memory size. Please specify a positive "
                     "integer <= 100 GB.\n";
        printUsage(argv[0]);
        return 1;
    }

    std::cout << "std::unordered_map Benchmark\n";
    std::cout << "============================\n\n";

    size_t iter = 0;
    UnorderedMapBenchmark benchmark(static_cast<size_t>(memory_gb));
    while (true) {
        iter++;
        std::cout << "iteration" << iter << std::endl;
        try {
            benchmark.runAllBenchmarks();
        } catch (const std::exception& e) {
            std::cout << "Error: " << e.what() << "\n";
            return 1;
        } catch (...) {
            std::cout << "Unknown error occurred\n";
            return 1;
        }
    }

    return 0;
}
