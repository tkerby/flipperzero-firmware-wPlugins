#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo "🔍 Running Comprehensive Code Checks..."
ERRORS=0

# 1. Clang-Format Check
echo -e "\n${YELLOW}[1/5] Checking code formatting...${NC}"
if clang-format --dry-run --Werror govee_control/*.c govee_control/**/*.c 2>/dev/null; then
    echo -e "${GREEN}✓ Code formatting OK${NC}"
else
    echo -e "${RED}✗ Formatting issues found. Run: clang-format -i govee_control/**/*.c${NC}"
    ERRORS=$((ERRORS+1))
fi

# 2. Clang-Tidy Static Analysis
echo -e "\n${YELLOW}[2/5] Running static analysis...${NC}"
if command -v clang-tidy &> /dev/null; then
    clang-tidy govee_control/*.c -- -I. -Igovee_control 2>/dev/null
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ Static analysis passed${NC}"
    else
        echo -e "${RED}✗ Static analysis found issues${NC}"
        ERRORS=$((ERRORS+1))
    fi
else
    echo -e "${YELLOW}⚠ clang-tidy not installed${NC}"
fi

# 3. Cppcheck Analysis
echo -e "\n${YELLOW}[3/5] Running cppcheck...${NC}"
if command -v cppcheck &> /dev/null; then
    cppcheck --enable=all --error-exitcode=1 --suppress=missingInclude \
             --suppress=unusedFunction --quiet govee_control/ 2>&1
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ Cppcheck passed${NC}"
    else
        echo -e "${RED}✗ Cppcheck found issues${NC}"
        ERRORS=$((ERRORS+1))
    fi
else
    echo -e "${YELLOW}⚠ cppcheck not installed (brew install cppcheck)${NC}"
fi

# 4. Compiler Warnings Check
echo -e "\n${YELLOW}[4/5] Checking compiler warnings...${NC}"
if command -v gcc &> /dev/null; then
    gcc -Wall -Wextra -Werror -Wpedantic -Wformat=2 -Wshadow \
        -Wnull-dereference -Wuninitialized -Warray-bounds=2 \
        -c govee_control/govee_control.c -o /tmp/test.o 2>/dev/null
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ No compiler warnings${NC}"
        rm -f /tmp/test.o
    else
        echo -e "${RED}✗ Compiler warnings/errors found${NC}"
        ERRORS=$((ERRORS+1))
    fi
fi

# 5. Memory Safety Check (if PVS-Studio available)
echo -e "\n${YELLOW}[5/5] Checking for common vulnerabilities...${NC}"
grep -r "strcpy\|strcat\|sprintf\|gets" govee_control/ --include="*.c" > /dev/null 2>&1
if [ $? -ne 0 ]; then
    echo -e "${GREEN}✓ No unsafe string functions found${NC}"
else
    echo -e "${RED}✗ Found unsafe string functions (use strncpy, strncat, snprintf)${NC}"
    grep -r "strcpy\|strcat\|sprintf\|gets" govee_control/ --include="*.c"
    ERRORS=$((ERRORS+1))
fi

# Check for potential buffer overflows
grep -r "malloc\|calloc\|realloc" govee_control/ --include="*.c" | \
    grep -v "free" > /dev/null 2>&1
if [ $? -eq 0 ]; then
    echo -e "${YELLOW}⚠ Check memory allocations have corresponding free() calls${NC}"
fi

# Summary
echo -e "\n================================"
if [ $ERRORS -eq 0 ]; then
    echo -e "${GREEN}✅ All checks passed!${NC}"
    exit 0
else
    echo -e "${RED}❌ Found $ERRORS issue(s). Please fix before committing.${NC}"
    exit 1
fi