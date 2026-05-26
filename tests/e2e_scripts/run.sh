#!/bin/bash

# Check if config file argument is provided
if [ $# -eq 0 ]; then
    echo "Usage: $0 <config_file>"
    echo "Example: $0 test_config.sh"
    echo ""
    echo "The config file should define TEST_COMMAND variable, e.g.:"
    echo "TEST_COMMAND='./exp_scripts/faster/ycsb_run_a.sh'"
    exit 1
fi

CONFIG_FILE="$1"

# Check if the config file exists
if [ ! -f "$CONFIG_FILE" ]; then
    echo "Error: Config file '$CONFIG_FILE' not found."
    exit 1
fi

# Source the config file to get TEST_COMMAND
echo "Sourcing config file: $CONFIG_FILE"
source "$CONFIG_FILE"

# Check if TEST_COMMAND is defined
if [ -z "$TEST_COMMAND" ]; then
    echo "Error: TEST_COMMAND is not defined in '$CONFIG_FILE'."
    echo "Please define TEST_COMMAND in the config file, e.g.:"
    echo "TEST_COMMAND='./exp_scripts/faster/ycsb_run_a.sh'"
    exit 1
fi

echo "Test command from config: $TEST_COMMAND"

# Define the log file paths for each process
UCM_LOG_FILE="/tmp/ucm.log"
TEST_LOG_FILE="/tmp/test_program.log"

# Clear the log files at the start of the script to ensure fresh logs each run
> "$UCM_LOG_FILE"
> "$TEST_LOG_FILE"

# Declare PIDs globally so they can be accessed by the trap function
UCM_PID=""
TEST_PID=""

# Function to clean up background processes on script exit or interrupt.
# It takes a 'reason' argument to distinguish between a Ctrl+C interrupt
# and a natural completion of TEST.
cleanup_processes() {
    local reason=$1 # "trap" or "test_finished"
    echo "" # Newline for cleaner output
    echo "Cleanup triggered by: $reason. Attempting to terminate background processes..."

    # Kill UCM process if it's still running
    if [ -n "$UCM_PID" ] && ps -p $UCM_PID > /dev/null; then
        echo "Killing UCM process (PID: $UCM_PID)..."
        # Use kill -TERM for a graceful termination attempt
        kill -TERM $UCM_PID 2>/dev/null
    fi

    # Kill TEST process ONLY if the reason is "trap" (i.e., script interruption).
    # If TEST finished naturally, we don't try to kill it again.
    if [ "$reason" == "trap" ] && [ -n "$TEST_PID" ] && ps -p $TEST_PID > /dev/null; then
        echo "Killing TEST process (PID: $TEST_PID)..."
        kill -TERM $TEST_PID 2>/dev/null
    fi

    # Wait briefly for background processes to actually terminate after sending kill signals.
    # This helps avoid zombie processes and ensures resources are cleaned up.
    wait $UCM_PID 2>/dev/null # Wait for UCM to exit
    if [ "$reason" == "trap" ]; then
        wait $TEST_PID 2>/dev/null # Only wait for TEST if it was killed by the trap
    fi
    echo "Background process termination attempted."
}

# Function to display the log file content and then exit the script.
# Takes the desired exit status as an argument.
display_all_logs_and_exit() {
    local exit_status=$1
    echo "----------------------------------------"
    echo "--- Start of UCM Log (from $UCM_LOG_FILE) ---"
    if [ -f "$UCM_LOG_FILE" ]; then
        cat "$UCM_LOG_FILE"
    else
        echo "UCM log file '$UCM_LOG_FILE' not found or empty."
    fi
    echo "--- End of UCM Log ---"

    echo "----------------------------------------"
    echo "--- Start of TEST Log (from $TEST_LOG_FILE) ---"
    if [ -f "$TEST_LOG_FILE" ]; then
        cat "$TEST_LOG_FILE"
    else
        echo "TEST log file '$TEST_LOG_FILE' not found or empty."
    fi
    echo "--- End of TEST Log ---"
    echo "----------------------------------------"

    # Exit the script with the specified status
    exit $exit_status
}


echo "Starting 'sudo ./build/ucm' in the background. Its output is redirected to '$UCM_LOG_FILE'..."
# Start 'ucm' in the background. Redirect stdout and stderr to its dedicated log file.
sudo ./build/ucm --fastmem-sz 2g --slowmem-sz 4g 2> "$UCM_LOG_FILE" &
UCM_PID=$! # Store the Process ID of ucm
echo "'sudo ./build/ucm' started with PID: $UCM_PID"

echo "Starting '$TEST_COMMAND' (TEST process) in the background. Its output is redirected to '$TEST_LOG_FILE'..."
# Start the test command in the background. Redirect stdout and stderr to its dedicated log file.
eval "NEMO_CI=1 $TEST_COMMAND" &> "$TEST_LOG_FILE" &
TEST_PID=$! # Store the Process ID of TEST
echo "'$TEST_COMMAND' (TEST process) started with PID: $TEST_PID"

echo "Waiting for '$TEST_COMMAND' (TEST process) to complete. Press Ctrl+C to stop both processes and view all logs."


while true; do
    if ! kill -0 $UCM_PID 2>/dev/null; then
        wait $UCM_PID
        exit_code=$?
        echo "UCM Process exited unexpectedly; exit code $exit_code"

        sudo kill -9 $TEST_PID 2>/dev/null
        display_all_logs_and_exit 1
    fi
    if ! kill -0 $TEST_PID 2>/dev/null; then
        # test completed; get exit code
        wait $TEST_PID
        TEST_STATUS=$?
        echo "Test process $TEST_PID finished first with exit code $TEST_STATUS"
        break
    fi
    sleep 1
done

echo "TEST process (PID: $TEST_PID) completed with status: $TEST_STATUS."

# Now that TEST has completed, kill the UCM process if it's still running.
# Pass "test_finished" as the reason to cleanup_processes.
cleanup_processes "test_finished"

# Check the exit status of TEST and determine the script's final exit behavior.
if [ $TEST_STATUS -ne 0 ]; then
    echo "Error: '$TEST_COMMAND' (TEST process) exited with an error status ($TEST_STATUS)."
    # Display all logs and exit with TEST's error status
    display_all_logs_and_exit $TEST_STATUS
else
    echo "Success: '$TEST_COMMAND' (TEST process) completed successfully, and UCM process was terminated."
    # Display all logs and exit with success status (0)
    display_all_logs_and_exit 0
fi
