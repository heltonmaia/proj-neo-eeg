#!/usr/bin/env python3
"""
Potyplex EEG - Service Manager (Interactive Menu)
Cross-platform script for Linux, Windows, and macOS.

Usage: python run.py
"""

import os
import sys
import subprocess
import signal
import time
import shutil
import json
from pathlib import Path

# Paths
SCRIPT_DIR = Path(__file__).parent.resolve()
BACKEND_DIR = SCRIPT_DIR / "backend"
FRONTEND_DIR = SCRIPT_DIR / "frontend"
CONFIG_FILE = SCRIPT_DIR / ".env.local.json"

# PID files
PID_DIR = Path(os.environ.get("TEMP", "/tmp"))
BACKEND_PID_FILE = PID_DIR / "potyplex-backend.pid"
FRONTEND_PID_FILE = PID_DIR / "potyplex-frontend.pid"
BACKEND_LOG = PID_DIR / "potyplex-backend.log"
FRONTEND_LOG = PID_DIR / "potyplex-frontend.log"

# Colors (ANSI escape codes)
class Colors:
    RED = '\033[0;31m'
    GREEN = '\033[0;32m'
    YELLOW = '\033[1;33m'
    CYAN = '\033[0;36m'
    BOLD = '\033[1m'
    NC = '\033[0m'  # No Color

    @classmethod
    def disable(cls):
        """Disable colors for Windows CMD without ANSI support."""
        cls.RED = cls.GREEN = cls.YELLOW = cls.CYAN = cls.BOLD = cls.NC = ''


# Enable ANSI colors on Windows 10+
if sys.platform == "win32":
    try:
        import ctypes
        kernel32 = ctypes.windll.kernel32
        kernel32.SetConsoleMode(kernel32.GetStdHandle(-11), 7)
    except Exception:
        Colors.disable()


def clear_screen():
    """Clear terminal screen."""
    os.system('cls' if sys.platform == 'win32' else 'clear')


def print_msg(msg):
    print(f"{Colors.CYAN}[INFO]{Colors.NC} {msg}")


def print_ok(msg):
    print(f"{Colors.GREEN}[OK]{Colors.NC} {msg}")


def print_err(msg):
    print(f"{Colors.RED}[ERROR]{Colors.NC} {msg}")


def print_warn(msg):
    print(f"{Colors.YELLOW}[WARN]{Colors.NC} {msg}")


# ============== Configuration ==============

def load_config():
    """Load configuration from file."""
    if CONFIG_FILE.exists():
        try:
            with open(CONFIG_FILE) as f:
                return json.load(f)
        except Exception:
            pass
    return {}


def save_config(config):
    """Save configuration to file."""
    with open(CONFIG_FILE, 'w') as f:
        json.dump(config, f, indent=2)


def get_venv_path(config):
    """Get virtual environment path."""
    return Path(config.get("venv_path", SCRIPT_DIR / ".venv"))


def get_python_executable(config):
    """Get Python executable from venv."""
    venv = get_venv_path(config)
    if sys.platform == "win32":
        return venv / "Scripts" / "python.exe"
    return venv / "bin" / "python"


def get_pip_executable(config):
    """Get pip executable from venv."""
    venv = get_venv_path(config)
    if sys.platform == "win32":
        return venv / "Scripts" / "pip.exe"
    return venv / "bin" / "pip"


# ============== Process Management ==============

def read_pid(pid_file):
    """Read PID from file."""
    try:
        if pid_file.exists():
            return int(pid_file.read_text().strip())
    except Exception:
        pass
    return None


def write_pid(pid_file, pid):
    """Write PID to file."""
    pid_file.write_text(str(pid))


def is_process_running(pid):
    """Check if process is running."""
    if pid is None:
        return False
    try:
        if sys.platform == "win32":
            # Windows: use tasklist
            result = subprocess.run(
                ["tasklist", "/FI", f"PID eq {pid}"],
                capture_output=True, text=True
            )
            return str(pid) in result.stdout
        else:
            # Unix: send signal 0
            os.kill(pid, 0)
            return True
    except (OSError, subprocess.SubprocessError):
        return False


def kill_process(pid, force=False):
    """Kill a process by PID."""
    if pid is None:
        return
    try:
        if sys.platform == "win32":
            subprocess.run(["taskkill", "/F", "/PID", str(pid)],
                         capture_output=True)
        else:
            os.kill(pid, signal.SIGKILL if force else signal.SIGTERM)
    except Exception:
        pass


def kill_process_on_port(port):
    """Kill any process using the specified port."""
    try:
        if sys.platform == "win32":
            # Windows: use netstat and taskkill
            result = subprocess.run(
                ["netstat", "-ano"],
                capture_output=True, text=True
            )
            for line in result.stdout.split('\n'):
                if f":{port}" in line and "LISTENING" in line:
                    parts = line.split()
                    if parts:
                        pid = parts[-1]
                        subprocess.run(["taskkill", "/F", "/PID", pid],
                                      capture_output=True)
        else:
            # Unix: use lsof
            result = subprocess.run(
                ["lsof", "-ti", f":{port}"],
                capture_output=True, text=True
            )
            for pid in result.stdout.strip().split('\n'):
                if pid:
                    os.kill(int(pid), signal.SIGKILL)
    except Exception:
        pass


def is_running(pid_file):
    """Check if service is running based on PID file."""
    pid = read_pid(pid_file)
    return is_process_running(pid)


def get_status(pid_file):
    """Get status string for a service."""
    pid = read_pid(pid_file)
    if is_process_running(pid):
        return f"{Colors.GREEN}● Running{Colors.NC} (PID: {pid})"
    return f"{Colors.RED}● Stopped{Colors.NC}"


# ============== Environment Setup ==============

def check_python():
    """Check if Python is available."""
    return shutil.which("python3") or shutil.which("python")


def check_node():
    """Check if Node.js is available."""
    return shutil.which("node")


def check_npm():
    """Check if npm is available."""
    return shutil.which("npm")


def setup_venv(config):
    """Setup Python virtual environment."""
    clear_screen()
    print(f"{Colors.CYAN}")
    print("╔═══════════════════════════════════════════════════════════╗")
    print("║              Python Environment Setup                     ║")
    print("╚═══════════════════════════════════════════════════════════╝")
    print(f"{Colors.NC}")

    print(f"\n{Colors.BOLD}Where do you want to create the virtual environment?{Colors.NC}\n")
    print(f"  {Colors.CYAN}1{Colors.NC}) Project folder: {SCRIPT_DIR / '.venv'}")
    print(f"  {Colors.CYAN}2{Colors.NC}) Custom location")
    print()

    choice = input("Select option (1-2): ").strip()

    if choice == "2":
        custom_path = input("Enter full path: ").strip()
        if custom_path:
            venv_path = Path(custom_path)
        else:
            print_warn("No path provided, using default")
            venv_path = SCRIPT_DIR / ".venv"
    else:
        venv_path = SCRIPT_DIR / ".venv"

    print()
    print_msg(f"Creating virtual environment at: {venv_path}")

    # Create venv
    try:
        subprocess.run([sys.executable, "-m", "venv", str(venv_path)], check=True)
        print_ok("Virtual environment created")
    except subprocess.CalledProcessError:
        print_err("Failed to create virtual environment")
        input("\nPress Enter to continue...")
        return

    # Save config
    config["venv_path"] = str(venv_path)
    save_config(config)

    # Install dependencies
    print()
    print_msg("Installing backend dependencies...")
    pip = get_pip_executable(config)
    try:
        subprocess.run([str(pip), "install", "-r", str(BACKEND_DIR / "requirements.txt")],
                      check=True)
        print_ok("Dependencies installed")
    except subprocess.CalledProcessError:
        print_err("Failed to install dependencies")

    print_ok(f"Configuration saved to {CONFIG_FILE}")
    input("\nPress Enter to continue...")


def check_environment(config):
    """Check if environment is ready, setup if needed."""
    venv = get_venv_path(config)
    python = get_python_executable(config)

    if venv.exists() and python.exists():
        return True

    # Check known locations
    known_paths = [
        SCRIPT_DIR / ".venv",
        SCRIPT_DIR.parent / ".venv",
    ]

    for path in known_paths:
        py = path / ("Scripts/python.exe" if sys.platform == "win32" else "bin/python")
        if py.exists():
            config["venv_path"] = str(path)
            save_config(config)
            print_msg(f"Found existing environment: {path}")
            time.sleep(1)
            return True

    # No environment found
    print_warn("Python environment not found")
    setup_venv(config)
    return get_venv_path(config).exists()


# ============== Service Control ==============

def start_backend(config):
    """Start the backend server."""
    if is_running(BACKEND_PID_FILE):
        print_warn("Backend already running")
        return False

    kill_process_on_port(8000)

    print_msg("Starting backend...")

    python = get_python_executable(config)
    if not python.exists():
        print_err(f"Python not found: {python}")
        print_msg("Press 's' to setup the environment")
        return False

    # Start uvicorn
    try:
        log_file = open(BACKEND_LOG, 'w')

        if sys.platform == "win32":
            # Windows: use subprocess with CREATE_NEW_PROCESS_GROUP
            proc = subprocess.Popen(
                [str(python), "-m", "uvicorn", "server:app",
                 "--host", "0.0.0.0", "--port", "8000"],
                cwd=str(BACKEND_DIR),
                stdout=log_file,
                stderr=subprocess.STDOUT,
                creationflags=subprocess.CREATE_NEW_PROCESS_GROUP
            )
        else:
            # Unix: use start_new_session
            proc = subprocess.Popen(
                [str(python), "-m", "uvicorn", "server:app",
                 "--host", "0.0.0.0", "--port", "8000"],
                cwd=str(BACKEND_DIR),
                stdout=log_file,
                stderr=subprocess.STDOUT,
                start_new_session=True
            )

        write_pid(BACKEND_PID_FILE, proc.pid)
        time.sleep(1)

        if is_running(BACKEND_PID_FILE):
            print_ok("Backend started")
            return True
        else:
            print_err("Failed to start backend")
            BACKEND_PID_FILE.unlink(missing_ok=True)
            return False

    except Exception as e:
        print_err(f"Failed to start backend: {e}")
        return False


def start_frontend(config):
    """Start the frontend dev server."""
    if is_running(FRONTEND_PID_FILE):
        print_warn("Frontend already running")
        return False

    kill_process_on_port(3000)

    print_msg("Starting frontend...")

    if not check_npm():
        print_err("npm not found. Please install Node.js")
        return False

    # Install dependencies if needed
    if not (FRONTEND_DIR / "node_modules").exists():
        print_msg("Installing frontend dependencies...")
        subprocess.run(["npm", "install"], cwd=str(FRONTEND_DIR))

    # Start vite
    try:
        log_file = open(FRONTEND_LOG, 'w')

        if sys.platform == "win32":
            proc = subprocess.Popen(
                ["npm", "run", "dev"],
                cwd=str(FRONTEND_DIR),
                stdout=log_file,
                stderr=subprocess.STDOUT,
                creationflags=subprocess.CREATE_NEW_PROCESS_GROUP,
                shell=True
            )
        else:
            proc = subprocess.Popen(
                ["npm", "run", "dev"],
                cwd=str(FRONTEND_DIR),
                stdout=log_file,
                stderr=subprocess.STDOUT,
                start_new_session=True
            )

        write_pid(FRONTEND_PID_FILE, proc.pid)
        time.sleep(2)

        if is_running(FRONTEND_PID_FILE):
            print_ok("Frontend started")
            return True
        else:
            print_err("Failed to start frontend")
            FRONTEND_PID_FILE.unlink(missing_ok=True)
            return False

    except Exception as e:
        print_err(f"Failed to start frontend: {e}")
        return False


def stop_backend():
    """Stop the backend server."""
    if is_running(BACKEND_PID_FILE):
        pid = read_pid(BACKEND_PID_FILE)
        print_msg("Stopping backend...")
        kill_process(pid)
        time.sleep(1)
        if is_process_running(pid):
            kill_process(pid, force=True)
        BACKEND_PID_FILE.unlink(missing_ok=True)
        print_ok("Backend stopped")
    else:
        print_warn("Backend not running")
        BACKEND_PID_FILE.unlink(missing_ok=True)


def stop_frontend():
    """Stop the frontend server."""
    if is_running(FRONTEND_PID_FILE):
        pid = read_pid(FRONTEND_PID_FILE)
        print_msg("Stopping frontend...")
        kill_process(pid)
        time.sleep(1)
        if is_process_running(pid):
            kill_process(pid, force=True)
        FRONTEND_PID_FILE.unlink(missing_ok=True)
        # Also kill any orphan node processes on port 3000
        kill_process_on_port(3000)
        print_ok("Frontend stopped")
    else:
        print_warn("Frontend not running")
        FRONTEND_PID_FILE.unlink(missing_ok=True)


def view_logs(service):
    """View service logs."""
    log_file = BACKEND_LOG if service == "backend" else FRONTEND_LOG

    if not log_file.exists():
        print_err(f"Log file not found: {log_file}")
        return

    print(f"{Colors.CYAN}Showing {service} logs (Ctrl+C to exit){Colors.NC}")
    print("─" * 60)

    try:
        # Tail the log file
        if sys.platform == "win32":
            subprocess.run(["powershell", "-Command",
                          f"Get-Content '{log_file}' -Wait -Tail 50"])
        else:
            subprocess.run(["tail", "-f", str(log_file)])
    except KeyboardInterrupt:
        pass


def install_deps(config):
    """Install all dependencies."""
    venv = get_venv_path(config)
    if not venv.exists():
        print_err("Python environment not found. Setting up...")
        setup_venv(config)
        return

    print_msg("Installing backend dependencies...")
    pip = get_pip_executable(config)
    subprocess.run([str(pip), "install", "-r", str(BACKEND_DIR / "requirements.txt")])

    print_msg("Installing frontend dependencies...")
    subprocess.run(["npm", "install"], cwd=str(FRONTEND_DIR))

    print_ok("All dependencies installed")


def run_tests(config):
    """Run backend tests."""
    venv = get_venv_path(config)
    if not venv.exists():
        print_err("Python environment not found. Setting up...")
        setup_venv(config)
        return

    python = get_python_executable(config)
    print_msg("Running backend tests...")

    subprocess.run([str(python), "-m", "pytest", "test_server.py", "-v"],
                  cwd=str(BACKEND_DIR))


def kill_stale_processes():
    """Kill any stale processes."""
    print_msg("Killing stale processes...")
    kill_process_on_port(8000)
    kill_process_on_port(3000)
    BACKEND_PID_FILE.unlink(missing_ok=True)
    FRONTEND_PID_FILE.unlink(missing_ok=True)
    print_ok("Stale processes killed")


# ============== Menu ==============

def print_menu(config):
    """Print the interactive menu."""
    clear_screen()

    venv = get_venv_path(config)
    venv_status = f"{Colors.GREEN}● OK{Colors.NC}" if venv.exists() else f"{Colors.RED}● Not found{Colors.NC}"
    venv_name = venv.parent.name if venv.exists() else "none"

    print(f"{Colors.CYAN}")
    print("╔═══════════════════════════════════════════════════════════╗")
    print("║              Potyplex EEG - Service Manager               ║")
    print("╚═══════════════════════════════════════════════════════════╝")
    print(f"{Colors.NC}")

    # Status
    print(f"{Colors.BOLD}Status:{Colors.NC}")
    print("─" * 60)
    print(f"  Backend:  {get_status(BACKEND_PID_FILE)}")
    print(f"  Frontend: {get_status(FRONTEND_PID_FILE)}")
    print(f"  Python:   {venv_status} {Colors.CYAN}{venv_name}{Colors.NC}")
    print("─" * 60)
    print()

    # URLs if running
    if is_running(BACKEND_PID_FILE):
        print(f"  {Colors.CYAN}API:{Colors.NC}       http://localhost:8000")
        print(f"  {Colors.CYAN}WebSocket:{Colors.NC} ws://localhost:8000/ws")
    if is_running(FRONTEND_PID_FILE):
        print(f"  {Colors.CYAN}Frontend:{Colors.NC}  http://localhost:3000")
    if is_running(BACKEND_PID_FILE) or is_running(FRONTEND_PID_FILE):
        print()

    # Menu
    print(f"{Colors.BOLD}Menu:{Colors.NC}")
    print("─" * 60)
    print(f"  {Colors.CYAN}1{Colors.NC}) Start All         {Colors.CYAN}5{Colors.NC}) Stop Backend")
    print(f"  {Colors.CYAN}2{Colors.NC}) Stop All          {Colors.CYAN}6{Colors.NC}) Stop Frontend")
    print(f"  {Colors.CYAN}3{Colors.NC}) Restart All       {Colors.CYAN}7{Colors.NC}) View Backend Logs")
    print(f"  {Colors.CYAN}4{Colors.NC}) Start Backend     {Colors.CYAN}8{Colors.NC}) View Frontend Logs")
    print(f"  {Colors.CYAN}9{Colors.NC}) Install Deps      {Colors.CYAN}t{Colors.NC}) Run Tests")
    print(f"  {Colors.CYAN}s{Colors.NC}) Setup Python Env  {Colors.CYAN}k{Colors.NC}) Kill Stale Processes")
    print(f"  {Colors.CYAN}0{Colors.NC}) Exit")
    print("─" * 60)
    print()


def wait_key():
    """Wait for user to press Enter."""
    input("\nPress Enter to continue...")


def main():
    """Main entry point."""
    config = load_config()

    # Kill stale processes on startup
    kill_process_on_port(8000)
    kill_process_on_port(3000)

    # Check environment
    check_environment(config)
    config = load_config()  # Reload after potential setup

    while True:
        print_menu(config)

        choice = input("Select option: ").strip().lower()
        print()

        if choice == "1":
            start_backend(config)
            start_frontend(config)
            wait_key()
        elif choice == "2":
            stop_frontend()
            stop_backend()
            wait_key()
        elif choice == "3":
            stop_frontend()
            stop_backend()
            time.sleep(1)
            start_backend(config)
            start_frontend(config)
            wait_key()
        elif choice == "4":
            start_backend(config)
            wait_key()
        elif choice == "5":
            stop_backend()
            wait_key()
        elif choice == "6":
            stop_frontend()
            wait_key()
        elif choice == "7":
            view_logs("backend")
        elif choice == "8":
            view_logs("frontend")
        elif choice == "9":
            install_deps(config)
            wait_key()
        elif choice == "t":
            run_tests(config)
            wait_key()
        elif choice == "s":
            setup_venv(config)
            config = load_config()
        elif choice == "k":
            kill_stale_processes()
            wait_key()
        elif choice in ("0", "q"):
            print(f"{Colors.CYAN}Bye!{Colors.NC}")
            sys.exit(0)
        else:
            print_warn("Invalid option")
            time.sleep(1)


if __name__ == "__main__":
    main()
