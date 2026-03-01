#!/bin/bash
#
# Potyplex EEG - Service Manager (Interactive Menu)
#

# Paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BACKEND_DIR="$SCRIPT_DIR/backend"
FRONTEND_DIR="$SCRIPT_DIR/frontend"

# Config file to store UV environment path
CONFIG_FILE="$SCRIPT_DIR/.env.local"

# Default UV paths
DEFAULT_UV_ENV="$SCRIPT_DIR/.venv"
UV_ENV=""
UV_CACHE=""

# PID files
BACKEND_PID="/tmp/potyplex-backend.pid"
FRONTEND_PID="/tmp/potyplex-frontend.pid"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

# Load config if exists
load_config() {
    if [ -f "$CONFIG_FILE" ]; then
        source "$CONFIG_FILE"
    fi
}

# Save config
save_config() {
    echo "UV_ENV=\"$UV_ENV\"" > "$CONFIG_FILE"
    echo "UV_CACHE=\"$UV_CACHE\"" >> "$CONFIG_FILE"
}

# Check if uv is installed
check_uv_installed() {
    if ! command -v uv &> /dev/null; then
        print_err "uv is not installed"
        print_msg "Install with: curl -LsSf https://astral.sh/uv/install.sh | sh"
        return 1
    fi
    return 0
}

# Setup UV environment
setup_uv_env() {
    clear
    echo -e "${CYAN}"
    echo "╔═══════════════════════════════════════════════════════════╗"
    echo "║              UV Environment Setup                         ║"
    echo "╚═══════════════════════════════════════════════════════════╝"
    echo -e "${NC}"
    echo ""

    if ! check_uv_installed; then
        echo ""
        read -n 1 -s -r -p "Press any key to exit..."
        exit 1
    fi

    echo -e "${BOLD}Where do you want to create the Python virtual environment?${NC}"
    echo ""
    echo -e "  ${CYAN}1${NC}) Project root: $SCRIPT_DIR/.venv"
    echo -e "  ${CYAN}2${NC}) Custom location"
    echo ""
    read -n 1 -s -r -p "Select option (1-2): " env_choice
    echo ""
    echo ""

    case $env_choice in
        1)
            UV_ENV="$SCRIPT_DIR/.venv"
            UV_CACHE="$SCRIPT_DIR/.uv-cache"
            ;;
        2)
            echo -e "Enter the full path for the virtual environment:"
            echo -e "(e.g., /mnt/hd3/uv-common/uv-eeg/.venv)"
            echo ""
            read -r -p "> " custom_path

            if [ -z "$custom_path" ]; then
                print_err "No path provided, using default"
                UV_ENV="$SCRIPT_DIR/.venv"
                UV_CACHE="$SCRIPT_DIR/.uv-cache"
            else
                UV_ENV="$custom_path"
                # Set cache next to venv
                UV_CACHE="$(dirname "$custom_path")/.uv-cache"
            fi
            ;;
        *)
            print_warn "Invalid option, using default"
            UV_ENV="$SCRIPT_DIR/.venv"
            UV_CACHE="$SCRIPT_DIR/.uv-cache"
            ;;
    esac

    echo ""
    print_msg "Creating virtual environment at: $UV_ENV"

    # Create cache directory
    mkdir -p "$UV_CACHE"

    # Create venv with uv
    export UV_CACHE_DIR="$UV_CACHE"
    uv venv "$UV_ENV"

    if [ $? -ne 0 ]; then
        print_err "Failed to create virtual environment"
        read -n 1 -s -r -p "Press any key to exit..."
        exit 1
    fi

    print_ok "Virtual environment created"
    echo ""

    # Install dependencies
    print_msg "Installing backend dependencies..."
    uv pip install --python "$UV_ENV/bin/python" -r "$BACKEND_DIR/requirements.txt"

    if [ $? -eq 0 ]; then
        print_ok "Dependencies installed"
    else
        print_err "Failed to install dependencies"
    fi

    # Save config
    save_config
    print_ok "Configuration saved to $CONFIG_FILE"

    echo ""
    read -n 1 -s -r -p "Press any key to continue..."
}

# Known UV environment locations to check
KNOWN_UV_PATHS=(
    "/mnt/hd3/uv-common/uv-eeg/.venv"
    "$SCRIPT_DIR/.venv"
    "$PROJECT_ROOT/.venv"
)

# Check and setup environment
check_environment() {
    load_config

    # If UV_ENV is set and exists, we're good
    if [ -n "$UV_ENV" ] && [ -d "$UV_ENV" ]; then
        return 0
    fi

    # Check known locations
    for known_path in "${KNOWN_UV_PATHS[@]}"; do
        if [ -d "$known_path" ]; then
            UV_ENV="$known_path"
            UV_CACHE="$(dirname "$known_path")/.uv-cache"
            save_config
            print_msg "Found existing environment: $UV_ENV"
            sleep 1
            return 0
        fi
    done

    # No environment found, run setup
    setup_uv_env
    load_config
}

# Check if process is running
is_running() {
    local pid_file=$1
    if [ -f "$pid_file" ]; then
        local pid=$(cat "$pid_file")
        if ps -p "$pid" > /dev/null 2>&1; then
            return 0
        fi
    fi
    return 1
}

# Get status string
get_status() {
    local pid_file=$1
    if is_running "$pid_file"; then
        echo -e "${GREEN}● Running${NC} (PID: $(cat $pid_file))"
    else
        echo -e "${RED}● Stopped${NC}"
    fi
}

# Get env status
get_env_status() {
    if [ -d "$UV_ENV" ]; then
        echo -e "${GREEN}● OK${NC}"
    else
        echo -e "${RED}● Not found${NC}"
    fi
}

# Clear screen and print header with status
print_menu() {
    clear
    echo -e "${CYAN}"
    echo "╔═══════════════════════════════════════════════════════════╗"
    echo "║              Potyplex EEG - Service Manager               ║"
    echo "╚═══════════════════════════════════════════════════════════╝"
    echo -e "${NC}"

    # Status section
    echo -e "${BOLD}Status:${NC}"
    echo "─────────────────────────────────────────────────────────────"
    echo -e "  Backend:  $(get_status $BACKEND_PID)"
    echo -e "  Frontend: $(get_status $FRONTEND_PID)"
    echo -e "  Python:   $(get_env_status) ${CYAN}$(basename $(dirname $UV_ENV))${NC}"
    echo "─────────────────────────────────────────────────────────────"
    echo ""

    # URLs if running
    if is_running "$BACKEND_PID"; then
        echo -e "  ${CYAN}API:${NC}       http://localhost:8000"
        echo -e "  ${CYAN}WebSocket:${NC} ws://localhost:8000/ws"
    fi
    if is_running "$FRONTEND_PID"; then
        echo -e "  ${CYAN}Frontend:${NC}  http://localhost:3000"
    fi
    if is_running "$BACKEND_PID" || is_running "$FRONTEND_PID"; then
        echo ""
    fi

    # Menu options
    echo -e "${BOLD}Menu:${NC}"
    echo "─────────────────────────────────────────────────────────────"
    echo -e "  ${CYAN}1${NC}) Start All         ${CYAN}5${NC}) Stop Backend"
    echo -e "  ${CYAN}2${NC}) Stop All          ${CYAN}6${NC}) Stop Frontend"
    echo -e "  ${CYAN}3${NC}) Restart All       ${CYAN}7${NC}) View Backend Logs"
    echo -e "  ${CYAN}4${NC}) Start Backend     ${CYAN}8${NC}) View Frontend Logs"
    echo -e "  ${CYAN}9${NC}) Install Deps      ${CYAN}s${NC}) Setup Python Env"
    echo -e "                         ${CYAN}0${NC}) Exit"
    echo "─────────────────────────────────────────────────────────────"
    echo ""
}

print_msg() {
    echo -e "${CYAN}[INFO]${NC} $1"
}

print_ok() {
    echo -e "${GREEN}[OK]${NC} $1"
}

print_err() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

# Start backend
start_backend() {
    if is_running "$BACKEND_PID"; then
        print_warn "Backend already running"
        return 1
    fi

    print_msg "Starting backend..."

    if [ ! -d "$UV_ENV" ]; then
        print_err "UV environment not found: $UV_ENV"
        print_msg "Press 's' to setup the environment"
        return 1
    fi

    cd "$BACKEND_DIR"
    export UV_CACHE_DIR="$UV_CACHE"

    nohup "$UV_ENV/bin/uvicorn" server:app --host 0.0.0.0 --port 8000 > /tmp/potyplex-backend.log 2>&1 &
    echo $! > "$BACKEND_PID"

    sleep 1
    if is_running "$BACKEND_PID"; then
        print_ok "Backend started"
    else
        print_err "Failed to start backend"
        rm -f "$BACKEND_PID"
        return 1
    fi
}

# Start frontend
start_frontend() {
    if is_running "$FRONTEND_PID"; then
        print_warn "Frontend already running"
        return 1
    fi

    print_msg "Starting frontend..."

    cd "$FRONTEND_DIR"

    if [ ! -d "node_modules" ]; then
        print_msg "Installing dependencies..."
        npm install
    fi

    nohup npm run dev > /tmp/potyplex-frontend.log 2>&1 &
    echo $! > "$FRONTEND_PID"

    sleep 2
    if is_running "$FRONTEND_PID"; then
        print_ok "Frontend started"
    else
        print_err "Failed to start frontend"
        rm -f "$FRONTEND_PID"
        return 1
    fi
}

# Stop backend
stop_backend() {
    if is_running "$BACKEND_PID"; then
        local pid=$(cat "$BACKEND_PID")
        print_msg "Stopping backend..."
        kill "$pid" 2>/dev/null
        sleep 1
        if ps -p "$pid" > /dev/null 2>&1; then
            kill -9 "$pid" 2>/dev/null
        fi
        rm -f "$BACKEND_PID"
        print_ok "Backend stopped"
    else
        print_warn "Backend not running"
        rm -f "$BACKEND_PID"
    fi
}

# Stop frontend
stop_frontend() {
    if is_running "$FRONTEND_PID"; then
        local pid=$(cat "$FRONTEND_PID")
        print_msg "Stopping frontend..."
        pkill -P "$pid" 2>/dev/null
        kill "$pid" 2>/dev/null
        sleep 1
        rm -f "$FRONTEND_PID"
        print_ok "Frontend stopped"
    else
        print_warn "Frontend not running"
        rm -f "$FRONTEND_PID"
    fi
}

# View logs
view_logs() {
    local service=$1
    local logfile="/tmp/potyplex-${service}.log"

    if [ -f "$logfile" ]; then
        echo -e "${CYAN}Showing $service logs (Ctrl+C to exit)${NC}"
        echo "─────────────────────────────────────────────────────────────"
        tail -f "$logfile"
    else
        print_err "Log file not found: $logfile"
    fi
}

# Install dependencies
install_deps() {
    if [ ! -d "$UV_ENV" ]; then
        print_err "UV environment not found. Setting up..."
        setup_uv_env
        return
    fi

    print_msg "Installing backend dependencies..."
    cd "$BACKEND_DIR"
    export UV_CACHE_DIR="$UV_CACHE"
    "$UV_ENV/bin/pip" install -r requirements.txt

    print_msg "Installing frontend dependencies..."
    cd "$FRONTEND_DIR"
    npm install

    print_ok "All dependencies installed"
}

# Wait for keypress
wait_key() {
    echo ""
    read -n 1 -s -r -p "Press any key to continue..."
}

# Main loop
main() {
    # Check environment on startup
    check_environment

    while true; do
        print_menu

        read -n 1 -s -r -p "Select option: " choice
        echo ""
        echo ""

        case $choice in
            1)
                start_backend
                start_frontend
                wait_key
                ;;
            2)
                stop_frontend
                stop_backend
                wait_key
                ;;
            3)
                stop_frontend
                stop_backend
                sleep 1
                start_backend
                start_frontend
                wait_key
                ;;
            4)
                start_backend
                wait_key
                ;;
            5)
                stop_backend
                wait_key
                ;;
            6)
                stop_frontend
                wait_key
                ;;
            7)
                view_logs "backend"
                ;;
            8)
                view_logs "frontend"
                ;;
            9)
                install_deps
                wait_key
                ;;
            s|S)
                setup_uv_env
                ;;
            0|q|Q)
                echo -e "${CYAN}Bye!${NC}"
                exit 0
                ;;
            *)
                print_warn "Invalid option"
                sleep 1
                ;;
        esac
    done
}

# Run
main
