import re

def extract_mops(log_line):
    # Regular expression pattern to match the mops number
    pattern = r'total=([\d.]+)\s*mops'
    
    # Search for the pattern in the log line
    match = re.search(pattern, log_line)
    
    if match:
        # If found, convert the matched string to a float and return
        return float(match.group(1))
    else:
        # If not found, return None or raise an exception
        return None 

# def extract_histogram_data(line):
#     # Regular expression pattern to match the histogram data
#     pattern = r'Hist\[(\d+)\]=(\d+)'
#     
#     # Search for the pattern in the line
#     match = re.search(pattern, line)
#     
#     if match:
#         # If found, return the extracted values as a string
#         return f"{match.group(1)},{match.group(2)}"
#     else:
#         # If not found, return None or raise an exception
#         return None
def extract_histogram_data(line):
    first = line[line.find('[')+1:line.find(']')]
    second = line[line.find('=')+1:]
    result = (int(first), int(second))
    return result


def process_flexkvs(logfile, mops_out, lat_hist_out):
    start = True
    mops_cnt = 0
    latencies = []

    with open(mops_out, 'w') as mops_file:
        for line in logfile:
            if "Warmup complete" in line:
                start = True
            elif start and "TP: " in line:
                mops = extract_mops(line)
                assert(mops)
                mops_file.write(f"{mops}\n")
                mops_cnt += 1
            elif "Hist" in line:
                lat_us, count = extract_histogram_data(line)
                print(lat_us, count)
                for _ in range(count):
                    latencies.append(lat_us)

    with open(lat_hist_out, 'w') as lat_hist_file:
        n_lat = len(latencies) 
        step = int(n_lat / 100)
        for i in range(1, 100):
            val = latencies[i * step]
            lat_hist_file.write(f"{i},{val}\n")
        tail = 99.5
        idx = int(tail * step)
        val = latencies[idx]
        lat_hist_file.write(f"{tail},{val}\n")

        tail = 99.9
        idx = int(tail * step)
        val = latencies[idx]
        lat_hist_file.write(f"{tail},{val}\n")

        tail = 99.99
        idx = int(tail * step)
        val = latencies[idx]
        lat_hist_file.write(f"{tail},{val}\n")

    print(f"Processed {mops_cnt} mop lines and {n_lat} latencies")

if __name__ == "__main__":
    flexkvs_log = "flexkvs.txt"
    migration_log = "out.txt"

    mops_out = "flexkvs_mops.txt"
    latency_hist_out = "flexkvs_lat_hist.txt"
    
    with open(flexkvs_log, 'r') as logfile:
        process_flexkvs(logfile, mops_out, latency_hist_out)

