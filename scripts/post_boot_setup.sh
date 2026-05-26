#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'  # No Color

echo "1. Setting up DAX devices..."

sudo ndctl create-namespace -f -e namespace0.0 --mode=devdax --align 2M &> /dev/null
if [ $? -eq 0 ]; then
    echo "Setup /dev/dax0.0 success."
else
    echo -e "${RED}!!!!!!!!!!!!!!!!!!!!!!!!${NC}"
    echo -e "${RED}/dev/dax0.0 setup failed${NC}"
    echo -e "${RED}!!!!!!!!!!!!!!!!!!!!!!!!${NC}"
fi

sudo ndctl create-namespace -f -e namespace1.0 --mode=devdax --align 2M &> /dev/null
if [ $? -eq 0 ]; then
    echo "Setup /dev/dax1.0 success."
else
    echo -e "${RED}!!!!!!!!!!!!!!!!!!!!!!!!${NC}"
    echo -e "${RED}/dev/dax1.0 setup failed${NC}"
    echo -e "${RED}!!!!!!!!!!!!!!!!!!!!!!!!${NC}"
fi

echo "List of dax devices: "
ndctl list --human

echo

echo "2. Setting up Intel DSA..."

sudo accel-config load-config -c config/dsa_memcpy_profile.conf -e
if [ $? -eq 0 ]; then
    echo "Setup DSA engine success."
else
    echo -e "${RED}!!!!!!!!!!!!!!!!!!!!!!!${NC}"
    echo -e "${RED}DSA engine setup failed${NC}"
    echo -e "${RED}!!!!!!!!!!!!!!!!!!!!!!!${NC}"
fi
