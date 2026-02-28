#!/bin/bash
#
# Potyplex EEG - Service Manager
# Usage: ./run.sh [command]
#

# Paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BACKEND_DIR="$SCRIPT_DIR/backend"
FRONTEND_DIR="$SCRIPT_DIR/frontend"
UV_ENV="/mnt/hd3/uv-common/uv-eeg/.venv"
UV_CACHE="/mnt/hd3/uv-cache"

# PID files
BACKEND_PID="/tmp/potyplex-backend.pid"
FRONTEND_PID="/tmp/potyplex-frontend.pid"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

print_header() {
    echo -e "${CYAN}"
    echo "╔════════════════════════════════════════╗"
    echo "║       Potyplex EEG - Manager           ║"
    echo "╚════════════════════════════════════════╝"
    echo -e "${NC}"
}

print_status() {
    echo -e "${CYAN}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[OK]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
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

# Start backend
start_backend() {
    if is_running "$BACKEND_PID"; then
        print_warning "Backend already running (PID: $(cat $BACKEND_PID))"
        return 1
    fi

    print_status "Starting backend..."

    # Check uv environment
    if [ ! -d "$UV_ENV" ]; then
        print_error "UV environment not found: $UV_ENV"
        return 1
    fi

    cd "$BACKEND_DIR"
    export UV_CACHE_DIR="$UV_CACHE"

    # Start uvicorn in background
    nohup "$UV_ENV/bin/uvicorn" server:app --host 0.0.0.0 --port 8000 > /tmp/potyplex-backend.log 2>&1 &
    echo $! > "$BACKEND_PID"

    sleep 1
    if is_running "$BACKEND_PID"; then
        print_success "Backend started (PID: $(cat $BACKEND_PID))"
        print_status "API: http://localhost:8000"
        print_status "WebSocket: ws://localhost:8000/ws"
        print_status "Log: /tmp/potyplex-backend.log"
    else
        print_error "Failed to start backend"
        rm -f "$BACKEND_PID"
        return 1
    fi
}

# Start frontend
start_frontend() {
    if is_running "$FRONTEND_PID"; then
        print_warning "Frontend already running (PID: $(cat $FRONTEND_PID))"
        return 1
    fi

    print_status "Starting frontend..."

    cd "$FRONTEND_DIR"

    # Check if node_modules exists
    if [ ! -d "node_modules" ]; then
        print_status "Installing dependencies..."
        npm install
    fi

    # Start vite in background
    nohup npm run dev > /tmp/potyplex-frontend.log 2>&1 &
    echo $! > "$FRONTEND_PID"

    sleep 2
    if is_running "$FRONTEND_PID"; then
        print_success "Frontend started (PID: $(cat $FRONTEND_PID))"
        print_status "URL: http://localhost:3000"
        print_status "Log: /tmp/potyplex-frontend.log"
    else
        print_error "Failed to start frontend"
        rm -f "$FRONTEND_PID"
        return 1
    fi
}

# Stop backend
stop_backend() {
    if is_running "$BACKEND_PID"; then
        local pid=$(cat "$BACKEND_PID")
        print_status "Stopping backend (PID: $pid)..."
        kill "$pid" 2>/dev/null
        sleep 1
        if ps -p "$pid" > /dev/null 2>&1; then
            kill -9 "$pid" 2>/dev/null
        fi
        rm -f "$BACKEND_PID"
        print_success "Backend stopped"
    else
        print_warning "Backend not running"
        rm -f "$BACKEND_PID"
    fi
}

# Stop frontend
stop_frontend() {
    if is_running "$FRONTEND_PID"; then
        local pid=$(cat "$FRONTEND_PID")
        print_status "Stopping frontend (PID: $pid)..."
        # Kill the npm process and its children (vite)
        pkill -P "$pid" 2>/dev/null
        kill "$pid" 2>/dev/null
        sleep 1
        rm -f "$FRONTEND_PID"
        print_success "Frontend stopped"
    else
        print_warning "Frontend not running"
        rm -f "$FRONTEND_PID"
    fi
}

# Show status
show_status() {
    echo ""
    echo "Service Status:"
    echo "───────────────"

    if is_running "$BACKEND_PID"; then
        echo -e "  Backend:  ${GREEN}●${NC} Running (PID: $(cat $BACKEND_PID))"
    else
        echo -e "  Backend:  ${RED}●${NC} Stopped"
    fi

    if is_running "$FRONTEND_PID"; then
        echo -e "  Frontend: ${GREEN}●${NC} Running (PID: $(cat $FRONTEND_PID))"
    else
        echo -e "  Frontend: ${RED}●${NC} Stopped"
    fi
    echo ""
}

# Show logs
show_logs() {
    local service=$1
    case $service in
        backend)
            if [ -f /tmp/potyplex-backend.log ]; then
                tail -f /tmp/potyplex-backend.log
            else
                print_error "Backend log not found"
            fi
            ;;
        frontend)
            if [ -f /tmp/potyplex-frontend.log ]; then
                tail -f /tmp/potyplex-frontend.log
            else
                print_error "Frontend log not found"
            fi
            ;;
        *)
            print_error "Usage: $0 logs [backend|frontend]"
            ;;
    esac
}

# Print usage
print_usage() {
    echo "Usage: $0 [command]"
    echo ""
    echo "Commands:"
    echo "  start         Start all services (backend + frontend)"
    echo "  stop          Stop all services"
    echo "  restart       Restart all services"
    echo "  status        Show service status"
    echo ""
    echo "  backend       Start backend only"
    echo "  frontend      Start frontend only"
    echo "  stop-backend  Stop backend only"
    echo "  stop-frontend Stop frontend only"
    echo ""
    echo "  logs backend  Follow backend logs"
    echo "  logs frontend Follow frontend logs"
    echo ""
    echo "  install       Install all dependencies"
    echo "  help          Show this help"
    echo ""
}

# Install dependencies
install_deps() {
    print_status "Installing backend dependencies..."
    cd "$BACKEND_DIR"
    export UV_CACHE_DIR="$UV_CACHE"
    "$UV_ENV/bin/pip" install -r requirements.txt

    print_status "Installing frontend dependencies..."
    cd "$FRONTEND_DIR"
    npm install

    print_success "All dependencies installed"
}

# Main
print_header

case "$1" in
    start)
        start_backend
        start_frontend
        show_status
        ;;
    stop)
        stop_frontend
        stop_backend
        show_status
        ;;
    restart)
        stop_frontend
        stop_backend
        sleep 1
        start_backend
        start_frontend
        show_status
        ;;
    status)
        show_status
        ;;
    backend)
        start_backend
        ;;
    frontend)
        start_frontend
        ;;
    stop-backend)
        stop_backend
        ;;
    stop-frontend)
        stop_frontend
        ;;
    logs)
        show_logs "$2"
        ;;
    install)
        install_deps
        ;;
    help|--help|-h)
        print_usage
        ;;
    *)
        print_usage
        show_status
        ;;
esac
