#!/bin/bash

# UID Brute Smarter Test Runner Script
# Cross-platform testing without Flipper Zero hardware

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test configuration
MIN_COVERAGE=90
TEST_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$TEST_DIR")"

# Helper functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check dependencies
check_dependencies() {
    log_info "Checking dependencies..."
    
    local missing_deps=()
    
    # Check for required tools
    if ! command -v gcc &> /dev/null; then
        missing_deps+=("gcc")
    fi
    
    if ! command -v make &> /dev/null; then
        missing_deps+=("make")
    fi
    
    if ! command -v lcov &> /dev/null; then
        missing_deps+=("lcov")
    fi
    
    if ! command -v bc &> /dev/null; then
        missing_deps+=("bc")
    fi
    
    if [ ${#missing_deps[@]} -ne 0 ]; then
        log_error "Missing dependencies: ${missing_deps[*]}"
        log_info "Install with: sudo apt-get install gcc make lcov bc"
        exit 1
    fi
    
    log_success "All dependencies available"
}

# Run tests
run_tests() {
    log_info "Running unit tests..."
    
    cd "$TEST_DIR"
    
    # Run all tests
    make test
    
    log_success "All unit tests passed!"
}

# Run specific test categories
run_menu_tests() {
    log_info "Running menu navigation tests..."
    
    cd "$TEST_DIR"
    make test-menu
    
    log_success "Menu navigation tests passed!"
}

run_brute_tests() {
    log_info "Running brute force completion tests..."
    
    cd "$TEST_DIR"
    make test-brute
    
    log_success "Brute force completion tests passed!"
}

run_ui_tests() {
    log_info "Running UI state management tests..."
    
    cd "$TEST_DIR"
    make test-ui
    
    log_success "UI state management tests passed!"
}

# Generate coverage report
generate_coverage() {
    log_info "Generating coverage report..."
    
    cd "$TEST_DIR"
    
    # Generate coverage
    make coverage
    
    # Check coverage files
    if [ -f "coverage/coverage.info" ]; then
        log_success "Coverage data generated"
        
        # Extract coverage percentages
        local pattern_coverage
        pattern_coverage=$(lcov --summary coverage/coverage.info 2>/dev/null | grep -A5 "pattern_engine.c" | grep "lines.*:" | sed 's/.*:\s*\([0-9.]*\).*/\1/' || echo "0")
        
        log_info "Pattern engine coverage: ${pattern_coverage}%"
        
        # Check against minimum threshold
        if (( $(echo "$pattern_coverage < $MIN_COVERAGE" | bc -l) )); then
            log_error "Pattern engine coverage below ${MIN_COVERAGE}%: ${pattern_coverage}%"
            return 1
        else
            log_success "Pattern engine coverage meets threshold: ${pattern_coverage}%"
        fi
        
        # Generate HTML report if lcov is available
        if command -v genhtml &> /dev/null; then
            genhtml coverage/coverage.info --output-directory coverage/html
            log_success "HTML coverage report generated at: coverage/html/index.html"
        fi
    else
        log_error "Coverage data not generated"
        return 1
    fi
}

# Run quick checks
run_quick_checks() {
    log_info "Running quick checks..."
    
    # Check for common issues
    local issues=0
    
    # Check for gets() usage
    if grep -r "gets(" --include="*.c" --include="*.h" "$PROJECT_DIR" &> /dev/null; then
        log_error "Found unsafe gets() usage"
        issues=$((issues + 1))
    fi
    
    # Check for TODO/FIXME comments
    local todo_count
    todo_count=$(find "$PROJECT_DIR" -name "*.c" -o -name "*.h" | xargs grep -n "TODO\|FIXME" 2>/dev/null | wc -l)
    if [ "$todo_count" -gt 0 ]; then
        log_warning "Found $todo_count TODO/FIXME comments"
    fi
    
    # Check for memory operations
    local malloc_count
    malloc_count=$(grep -r "malloc\|calloc" --include="*.c" "$PROJECT_DIR" 2>/dev/null | wc -l)
    log_info "Found $malloc_count memory allocation operations"
    
    if [ "$issues" -eq 0 ]; then
        log_success "Quick checks passed"
    else
        log_error "Quick checks found $issues issues"
    fi
    
    return $issues
}

# Clean build artifacts
clean() {
    log_info "Cleaning build artifacts..."
    
    cd "$TEST_DIR"
    make clean
    
    log_success "Clean completed"
}

# Show usage
show_usage() {
    echo "UID Brute Smarter Test Runner"
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -h, --help      Show this help message"
    echo "  -t, --test      Run unit tests only"
    echo "  -m, --menu      Run menu navigation tests only"
    echo "  -b, --brute     Run brute force completion tests only"
    echo "  -u, --ui        Run UI state management tests only"
    echo "  -c, --coverage  Run tests with coverage"
    echo "  -q, --quick     Run quick checks"
    echo "  -a, --all       Run tests, coverage, and checks"
    echo "  --clean         Clean build artifacts"
    echo ""
    echo "Examples:"
    echo "  $0 --all        Run complete test suite"
    echo "  $0 --test       Run tests only"
    echo "  $0 --menu       Run menu navigation tests"
    echo "  $0 --brute      Run brute force completion tests"
    echo "  $0 --ui         Run UI state management tests"
    echo "  $0 --coverage   Run tests with coverage"
}

# Main function
main() {
    local action="all"
    
    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                show_usage
                exit 0
                ;;
            -t|--test)
                action="test"
                shift
                ;;
            -m|--menu)
                action="menu"
                shift
                ;;
            -b|--brute)
                action="brute"
                shift
                ;;
            -u|--ui)
                action="ui"
                shift
                ;;
            -c|--coverage)
                action="coverage"
                shift
                ;;
            -q|--quick)
                action="quick"
                shift
                ;;
            -a|--all)
                action="all"
                shift
                ;;
            --clean)
                action="clean"
                shift
                ;;
            *)
                log_error "Unknown option: $1"
                show_usage
                exit 1
                ;;
        esac
    done
    
    log_info "Starting UID Brute Smarter test suite..."
    log_info "Test directory: $TEST_DIR"
    log_info "Project directory: $PROJECT_DIR"
    
    case $action in
        "test")
            check_dependencies
            run_tests
            ;;
        "menu")
            check_dependencies
            run_menu_tests
            ;;
        "brute")
            check_dependencies
            run_brute_tests
            ;;
        "ui")
            check_dependencies
            run_ui_tests
            ;;
        "coverage")
            check_dependencies
            run_tests
            generate_coverage
            ;;
        "quick")
            run_quick_checks
            ;;
        "clean")
            clean
            ;;
        "all")
            check_dependencies
            run_quick_checks
            run_tests
            generate_coverage
            log_success "Complete test suite finished successfully!"
            ;;
    esac
}

# Run main function with all arguments
main "$@"