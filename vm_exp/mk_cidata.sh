#!/bin/bash

set -euo pipefail

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <output-directory>"
    exit 1
fi

OUTPUT_DIR="$1"
mkdir -p "$OUTPUT_DIR"

# Define file paths
TEMPLATE_FILE="cidata/user-data.template"
OUTPUT_FILE="${OUTPUT_DIR}/user-data"
SSH_DIR="$HOME/.ssh"

LOGIN_ISO_FILE="${OUTPUT_DIR}/cidata.iso"

# Check if the template file exists
if [ ! -f "$TEMPLATE_FILE" ]; then
    echo "Error: Template file '$TEMPLATE_FILE' not found."
    exit 1
fi

# Find the user's public key
# We'll look for common public key names, prioritizing id_rsa.pub
PUB_KEY_FILE=""
if [ -f "$SSH_DIR/id_rsa.pub" ]; then
    PUB_KEY_FILE="$SSH_DIR/id_rsa.pub"
elif [ -f "$SSH_DIR/id_ecdsa.pub" ]; then
    PUB_KEY_FILE="$SSH_DIR/id_ecdsa.pub"
elif [ -f "$SSH_DIR/id_ed25519.pub" ]; then
    PUB_KEY_FILE="$SSH_DIR/id_ed25519.pub"
else
    # Fallback to finding any .pub file
    FOUND_PUB_KEYS=$(find "$SSH_DIR" -maxdepth 1 -name "*.pub" -print -quit)
    if [ -n "$FOUND_PUB_KEYS" ]; then
        PUB_KEY_FILE="$FOUND_PUB_KEYS"
    fi
fi

# Check if a public key was found
if [ -z "$PUB_KEY_FILE" ]; then
    echo "Error: No public SSH key found in '$SSH_DIR'."
    echo "Please ensure you have generated an SSH key pair (e.g., id_rsa.pub, id_ecdsa.pub, or id_ed25519.pub)."
    echo "You can generate one using: ssh-keygen -t rsa -b 4096"
    exit 1
fi

# Read the public key content
SSH_PUB_KEY=$(cat "$PUB_KEY_FILE")

# Replace the placeholder in the template with the public key and save to the output file
ESCAPED_SSH_PUB_KEY=$(printf '%s\n' "$SSH_PUB_KEY" | sed 's/[&/\|]/\\&/g')
sed "s|<SSH_PUB_KEY>|$ESCAPED_SSH_PUB_KEY|" "$TEMPLATE_FILE" > "$OUTPUT_FILE"

echo "Successfully generated '$OUTPUT_FILE' with your SSH public key from '$PUB_KEY_FILE'."

# Finally, make the .iso image with login info that would be provided to the VM
cloud-localds "$LOGIN_ISO_FILE" "$OUTPUT_FILE" cidata/meta-data

echo "Successfully generated login info file '$LOGIN_ISO_FILE'"
