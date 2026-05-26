import re

def extract_trial_times(log_file_path, output_file_path):
    # Regex pattern to match "Trial Time: X.XXXXX" lines
    trial_time_pattern = re.compile(r'Trial Time:\s+(\d+\.\d+)')
    
    trial_times = []
    
    try:
        # Read the log file
        with open(log_file_path, 'r') as log_file:
            log_content = log_file.read()
            
            # Find all trial time matches
            matches = trial_time_pattern.findall(log_content)
            
            # Convert matched strings to float values
            for match in matches:
                trial_times.append(float(match))
        
        # Write the extracted trial times to the output file
        with open(output_file_path, 'w') as output_file:
            for time in trial_times:
                output_file.write(f"{time}\n")
                
        print(f"Successfully extracted {len(trial_times)} trial times to {output_file_path}")
        
    except FileNotFoundError:
        print(f"Error: Could not find the log file at {log_file_path}")
    except Exception as e:
        print(f"Error: {str(e)}")

if __name__ == "__main__":
    # Define the paths
    log_file_path = "gap_bc.txt"  # Replace with your actual log file path
    output_file_path = "gap_bc_trial_times.txt"
    
    extract_trial_times(log_file_path, output_file_path)