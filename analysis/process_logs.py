import re
import statistics

MEBIBYTE = 1024 * 1024

def extract_timestamp(line, prefix):
    match = re.search(f"{prefix}\s+(\d+)", line)
    if match:
        return int(match.group(1))
    return None

def hex_to_int(hex_string):
    # Remove '0x' prefix if present
    hex_string = hex_string.lower().replace('0x', '')
    return int(hex_string, 16)


def filter_sort_logs(app_file, migration_file, hot_file):
    combined_logs = []

    # Process app.txt
    with open(app_file, 'r') as appfile:
        for line in appfile:
            if line.strip().startswith("hotshift_ts"):
                timestamp = extract_timestamp(line, "hotshift_ts")
                if timestamp:
                    processed_line = line.split('hotshift_ts', 1)[1].strip()
                    combined_logs.append((timestamp, processed_line))

    # Process migration.txt
    with open(migration_file, 'r') as migfile:
        for line in migfile:
            if "migration_ts" in line:
                timestamp = extract_timestamp(line, "migration_ts")
                if timestamp:
                    processed_line = line.split('migration_ts', 1)[1].strip()
                    combined_logs.append((timestamp, processed_line))

    # Process log_hot.txt
    with open(hot_file, 'r') as hotfile:
        for line in hotfile:
            parts = line.strip().split()
            try:
                timestamp = int(parts[0])
                combined_logs.append((timestamp, line.strip()))
            except ValueError:
                assert(False)

    # Sort combined logs by timestamp
    combined_logs.sort(key=lambda x: x[0])

    return combined_logs


def preprocess_logs(app_file, migration_file, hot_file, output_file):
    combined_logs = filter_sort_logs(app_file, migration_file, hot_file)

    # Write sorted logs to output file
    with open(output_file, 'w') as outfile:
        for _, line in combined_logs:
            outfile.write(f"{line}\n")


def extract_ms_times(log_line):
    # Regular expression pattern to match times in parentheses
    pattern = r'\((\d+\.\d+)ms CPU\)'
    
    # Find all matches in the log line
    matches = re.findall(pattern, log_line)
    
    # Convert matched strings to floats
    times = [float(match) for match in matches]
    
    return times

def extract_float(log_line):
    # Regular expression pattern to match a floating-point number
    pattern = r'\b(\d+\.\d+)\b'
    
    match = re.search(pattern, log_line)
    
    if match:
        return float(match.group(1))
    else:
        return None

def process_cpu_util(log_file):
    epoch_ms = 0
    epoch_time_ms = 0
    num_epochs = 0
    policy_time_ms = 0
    num_policies = 0
    
    breakdown_pebs = 0
    breakdown_cxl = 0
    breakdown_policy = 0
    with open(log_file, 'r') as logfile:
        for line in logfile:
            if "EPOCH scan interval" in line:
                epoch_ms = int(line.split(" ")[-1])
                print("epoch ms", epoch_ms)
            elif "EPOCH" in line:
                cpu_times = extract_ms_times(line)
                pebs, cxl, policy = cpu_times
                epoch_time_ms += sum(cpu_times) 
                breakdown_pebs += pebs
                breakdown_cxl += cxl
                breakdown_policy += policy
                num_epochs += 1
            elif "Finished Policy" in line:
                policy_ms = float(line.split(" ")[-1])
                assert(policy_ms)
                policy_time_ms += policy_ms
                num_policies += 1

    print(f"epochs {num_epochs} cpu time {epoch_time_ms}")

    if num_policies > 0:
        assert(breakdown_policy == 0)
        breakdown_policy += policy_time_ms

    print(f"combined thread breakdown pebs {breakdown_pebs:.3f} cxl {breakdown_cxl:.3f} policy {breakdown_policy:.3f}")
    per_epoch_util = ((epoch_time_ms / num_epochs) / epoch_ms) * 100
    pebs_util = (breakdown_pebs / (num_epochs * epoch_ms)) * 100

    policy_util = 0
    if num_policies > 0:
        print("policy time ms ", policy_time_ms)
        print("num policies ", num_policies)
        policy_util = (policy_time_ms / (num_policies * 1000)) * 100

    total_util = policy_util + per_epoch_util

    print(f"pebs util {pebs_util:0.5f}% epoch util {per_epoch_util}% policy util {policy_util}% total {total_util}%")


def process_miss_ratio(log_file, miss_ratio_log):
    epoch_ms = 0
    epoch_s = 0
    iter = 0.0

    # miss_ratio_thresh = 0.10
    
    with open(log_file, 'r') as logfile, open(miss_ratio_log, 'w') as miss_ratio_file:
        for line in logfile:
            if "EPOCH scan interval" in line:
                epoch_ms = float(line.split(" ")[-1])
                epoch_s = epoch_ms / 1000
                # print("epoch ms", epoch_ms)
            elif "miss ratio" in line:
                miss_ratio = float(line.split(" ")[-1])
                second = iter * epoch_s
                # print(iter, epoch_s, second)
                miss_ratio_file.write(f"{second},{miss_ratio}\n")
                iter += 1


def process_miss_ratio_compare(log_file, miss_ratio_access_cnt, miss_ratio_pebs_cnt):
    epoch_ms = 0
    epoch_s = 0

    # miss_ratio_thresh = 0.10
    
    with open(log_file, 'r') as logfile, open(miss_ratio_access_cnt, 'w') as miss_ratio_acc, open(miss_ratio_pebs_cnt, 'w') as miss_ratio_pebs:
        for line in logfile:
            if "EPOCH scan interval" in line:
                epoch_ms = float(line.split(" ")[-1])
                epoch_s = epoch_ms / 1000
                # print("epoch ms", epoch_ms)
            elif "miss ratio access count" in line:
                splitted = line.strip().split(" ")
                fastmem_access = splitted[-2]
                slowmem_access = splitted[-1]
                miss_ratio_acc.write(f"{fastmem_access},{slowmem_access}\n")
            elif "miss ratio pebs count" in line:
                splitted = line.strip().split(" ")
                cxl_cnt = splitted[-2]
                total_cnt = splitted[-1]
                miss_ratio_pebs.write(f"{cxl_cnt},{total_cnt}\n")


def process_mem_usage(log_file, mem_usage_log):
    epoch_ms = 0
    epoch_s = 0
    iter = 0.0

    # miss_ratio_thresh = 0.10
    
    with open(log_file, 'r') as logfile, open(mem_usage_log, 'w') as mem_usage_file:
        for line in logfile:
            if "EPOCH scan interval" in line:
                epoch_ms = float(line.split(" ")[-1])
                epoch_s = epoch_ms / 1000
                # print("epoch ms", epoch_ms)
            elif "mem usage" in line:
                split_line = line.split(" ")
                nvm_gb = float(split_line[-1])
                dram_gb = float(split_line[-2])
                second = iter * epoch_s
                mem_usage_file.write(f"{second},{dram_gb},{nvm_gb}\n")
                iter += 1


def process_migration_count(log_file, migration_file):
    epoch_ms = 0
    epoch_s = 0
    iter = 0.0

    with open(log_file, 'r') as logfile, open(migration_file, 'w') as mig_f:
        for line in logfile:
            if "migration_stats" in line:
                numbers = [int(x) for x in line.split() if x.isdigit()]
                #print(numbers)
                assert(len(numbers) == 2)
                up = numbers[0]
                down = numbers[1]
                mig_f.write(f"{up},{down}\n")

def parse_hotset_line(log_line):
    # Regular expression pattern to match the required information
    pattern = r'(\d+)\s+accessing hot set \[([0-9a-fxA-F]+),\s*([0-9a-fxA-F]+)\)'
    
    match = re.search(pattern, log_line)
    
    if match:
        first_number = int(match.group(1))
        address1 = match.group(2)
        address2 = match.group(3)
        return first_number, address1, address2
    else:
        return None

def parse_hottest_dram(line):
    return line.split(",")[1:]

def parse_migration(log_line):
    # Regular expression pattern to match the required information
    pattern = r'^(\d+)\s+migrate\s+up\s+page\s+(0x[0-9a-fA-F]+)'
    
    match = re.search(pattern, log_line)
    
    if match:
        timestamp = int(match.group(1))
        address = match.group(2)
        return timestamp, address
    else:
        return None


def is_addr_in_range(addr_str, hot_start, hot_end):
    try:
        addr = hex_to_int(addr_str)
        return addr >= hot_start and addr < hot_end
    except Exception:
        return False


def process_logs(logfile):
    expected_hot_pages = 250

    num_migrations = 0
    num_hotset_shifts = 0
    num_didnt_converge = 0

    hotset_shift_ts = 0
    hot_start = 0
    hot_end = 0
    last_hot_migration_ts = 0
    hot_page_count = 0

    hot_scan_done = True

    migration_times = []
    
    for line in logfile:
        if "hot set" in line:
            num_hotset_shifts += 1

            result = parse_hotset_line(line)
            assert(result)
            ts, addr_start_str, addr_end_str = result
            hot_start = hex_to_int(addr_start_str)
            hot_end = hex_to_int(addr_end_str)
            assert(hot_end - hot_start == 512 * MEBIBYTE)
            hotset_shift_ts = ts
            if not hot_scan_done:
                num_didnt_converge += 1
                print("didn't converge hot pages", hot_page_count)
            hot_scan_done = False
            last_hot_migration_ts = 0
            hot_page_count = 0
            print(f"hotset shift {ts}")

        elif "hottest_dram" in line:
            hot_pages = parse_hottest_dram(line.strip()) 
            hot_page_count = 0
            for hot_page in hot_pages:
                if is_addr_in_range(hot_page, hot_start, hot_end):
                    hot_page_count += 1
            if not hot_scan_done and hot_page_count >= expected_hot_pages:
                # done logic
                hot_scan_done = True
                if last_hot_migration_ts == 0:
                    print("all pages already in hotset")
                else:
                    assert(last_hot_migration_ts > hotset_shift_ts)
                    migration_ts = last_hot_migration_ts - hotset_shift_ts
                    migration_secs = migration_ts / 1_000_000.0
                    migration_times.append(migration_ts)
                    print(f"migration took {migration_secs}s")

        elif "migrate up" in line:
            num_migrations += 1
            result = parse_migration(line)
            assert(result)
            ts, addr_str = result
            if is_addr_in_range(addr_str, hot_start, hot_end):
                last_hot_migration_ts = ts

    if not hot_scan_done:
        num_didnt_converge += 1

    migration_times = migration_times[1:]
    print(f"hotset shifts {num_hotset_shifts}, migrations {num_migrations} didn't converge {num_didnt_converge}")
    mean_migration_ts = statistics.mean(migration_times) / 1_000.0
    median_migration_ts = statistics.median(migration_times) / 1_000.0
    max_migration_ts = max(migration_times) / 1_000.0
    min_migration_ts = min(migration_times) / 1_000.0
    print(f"migration min {min_migration_ts} mean {mean_migration_ts} median {median_migration_ts} max {max_migration_ts}")
    

if __name__ == "__main__":
    app_log = "app.txt"
    hemem_log = "build/out.txt"
    hot_log = "log_hot.txt"
    output_log = "sorted_filtered_logs.txt"
    miss_ratio_log = "miss_ratio_log.txt"
    mem_usage_log = "mem_usage_log.txt"
    migration_log = "migration_log.txt"

    miss_ratio_access = "miss_ratio_access.txt"
    miss_ratio_pebs = "miss_ratio_pebs.txt"
    
    # preprocess_logs(app_log, hemem_log, hot_log, output_log)
    # print(f"Processed and sorted logs have been written to {output_log}")

    # with open(output_log, 'r') as logfile:
    #     process_logs(logfile)

    process_cpu_util(hemem_log)
    # process_miss_ratio_compare(hemem_log, miss_ratio_access, miss_ratio_pebs)
    # process_mem_usage(hemem_log, mem_usage_log)
    # process_migration_count(hemem_log, migration_log)
