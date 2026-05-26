#!/bin/bash

# Unified script to check or apply clang-format to C/H files.
# Default: Checks formatting.
# With -f or --format flag: Formats files in-place.
# With -d or --debug flag: Outputs additional debug information.

# --- Configuration ---
DIRECTORIES_TO_SCAN="src/ tests/ libs/ bench/ fpga_ctl/"
# Using an array for FILE_PATTERNS_FIND to ensure correct expansion in find
FILE_PATTERNS_FIND=(-name "*.c" -o -name "*.h")

# --- Global Variables ---
# This array will be populated by find_files function
FOUND_FILES_ARRAY=()

# --- Helper Functions ---
print_usage() {
  echo "Usage: $0 [-f | --format] [-d | --debug] [-h | --help]"
  echo ""
  echo "Checks or formats C (.c) and Header (.h) files in specified directories"
  echo "using clang-format. The default behavior (no flags) is to check formatting."
  echo ""
  echo "Options:"
  echo "  -f, --format    Format the files in-place."
  echo "  -d, --debug     Enable debug mode (lists files and clang-format version)."
  echo "  -h, --help      Show this help message and exit."
  echo ""
  echo "Monitored directories: $DIRECTORIES_TO_SCAN"
}

check_clang_format_installed() {
  if ! command -v clang-format &> /dev/null; then
    echo "Error: clang-format could not be found. Please install it first."
    echo "On a Debian-based system, you might run: sudo apt-get install clang-format"
    exit 127
  fi
}

# Populates FOUND_FILES_ARRAY
find_target_files() {
  local search_dirs="$1"
  # Clear the array before populating
  FOUND_FILES_ARRAY=()
  # Use process substitution and a while-read loop to safely handle filenames
  # with spaces or special characters.
  while IFS= read -r -d $'\0' file; do
      # Ensure file actually exists.
      if [ -e "$file" ]; then
          FOUND_FILES_ARRAY+=("$file")
      fi
  done < <(find $search_dirs -type f \( "${FILE_PATTERNS_FIND[@]}" \) -print0 2>/dev/null)
}

# --- Argument Parsing ---
FORMAT_MODE=false
DEBUG_MODE=false

# Handle no arguments specifically for non-interactive cases (like CI)
if [ "$#" -eq 0 ] && [ ! -t 0 ]; then
    : # No explicit action needed, default modes (FORMAT_MODE=false, DEBUG_MODE=false) are fine.
elif [ "$#" -gt 0 ]; then
    while [[ "$#" -gt 0 ]]; do
        case "$1" in
            -f|--format)
                FORMAT_MODE=true
                shift # past argument
                ;;
            -d|--debug)
                DEBUG_MODE=true
                shift # past argument
                ;;
            -h|--help)
                print_usage
                exit 0
                ;;
            *)
                echo "Unknown option: $1" >&2
                print_usage >&2 # Print usage to stderr
                exit 1
                ;;
        esac
    done
fi

# --- Main Script Logic ---
check_clang_format_installed

if $DEBUG_MODE; then
  echo "--- Debug Mode Enabled ---"
  echo "🔧 clang-format version:"
  clang-format --version
  echo "--------------------------"
fi

echo "🔍 Using directories: $DIRECTORIES_TO_SCAN"
find_target_files "$DIRECTORIES_TO_SCAN" # Populates global FOUND_FILES_ARRAY

if [ ${#FOUND_FILES_ARRAY[@]} -eq 0 ]; then
  echo "ℹ️ No .c or .h files found in the specified directories."
  exit 0
fi

echo "🔎 Found ${#FOUND_FILES_ARRAY[@]} C/H files to process."

if $DEBUG_MODE; then
  echo "📄 Files targeted for processing:"
  if [ ${#FOUND_FILES_ARRAY[@]} -gt 0 ]; then
    for f_debug in "${FOUND_FILES_ARRAY[@]}"; do
      echo "  - $f_debug"
    done
  else
    echo "  (No files found to list)" # Should not happen due to earlier check
  fi
  echo "--------------------------"
fi

if $FORMAT_MODE; then
  # --- Format Mode ---
  echo "✨ Formatting files in-place with 'clang-format -i'..."
  
  if [ ${#FOUND_FILES_ARRAY[@]} -gt 0 ]; then
    printf "%s\0" "${FOUND_FILES_ARRAY[@]}" | xargs -0 --no-run-if-empty clang-format -i
    echo "✅ Formatting complete."
  else
    echo "ℹ️ No files were passed to clang-format for formatting."
  fi
  exit 0
else
  # --- Check Mode (default) ---
  echo "🔎 Checking file formatting..."
  MISFORMATTED_FILES=()
  ALL_FORMATTED=true

  for f in "${FOUND_FILES_ARRAY[@]}"; do
    if ! clang-format --dry-run -Werror "$f" >/dev/null 2>&1; then
      MISFORMATTED_FILES+=("$f")
      ALL_FORMATTED=false
    fi
  done

  if $ALL_FORMATTED; then
    echo "✅ All checked files are correctly formatted."
    exit 0
  else
    echo "❌ The following files are not correctly formatted:"
    for f_misformatted in "${MISFORMATTED_FILES[@]}"; do
      echo "  - $f_misformatted"
    done
    echo ""
    echo "👉 To fix these files, run this script with the -f (or --format) flag: $0 --format"
    exit 1
  fi
fi
