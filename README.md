# toolB — High-Speed Concurrent Middleware Engine

**toolB** is a high-performance, experimental middleware engine designed to accelerate web servers written in high-level languages like Python. It achieves this by offloading performance-critical tasks—such as network I/O and HTTP parsing—to a multi-threaded C server, which communicates with a parallel Python application layer via a lock-free, shared memory ring buffer.

This project successfully demonstrates a hybrid C + Python architecture that bypasses Python's Global Interpreter Lock (GIL) for both network operations and application logic, enabling true parallelism on multi-core systems.

## 🎯 Core Idea & Architecture

Modern Python web servers (FastAPI, Flask, etc.) are powerful but are fundamentally limited by the GIL. **toolB** introduces a low-level "heart" that sits between the OS and the Python app, acting as a complete, high-performance replacement for traditional ASGI servers like Uvicorn.

The architecture is a robust implementation of the **Producer-Consumer** pattern:

1. **The Producer (`heartware_server`):** A multi-threaded C server that listens for network connections. Each connection is handled by a dedicated `pthread`, achieving true concurrency at the I/O level. It uses a high-performance C-based parser and signals the Python side using POSIX semaphores.

2. **The Shared Memory Bus:** A POSIX shared memory segment containing two ring buffers for bidirectional communication. This is the core of the high-speed IPC mechanism.

3. **The Consumer (`ToolBServer`):** A sophisticated Python dispatcher that manages a pool of worker **processes**. Because each worker is a separate process with its own GIL, they can execute application logic in true parallel on different CPU cores.

This design ensures the fast C server is never blocked by the Python application, allowing it to handle a high volume of concurrent requests with minimal latency.
```text
   Raw HTTP Requests
           |
 (handled concurrently by C threads)
           |
           v
   ┌──────────────────┐                      ┌──────────────────┐
   │ heartware_server │                      │  toolb_app.py    │
   │ (Multi-threaded) │                      │ (Multi-process)  │
   └──────────────────┘                      └──────────────────┘
           |         ▲                         ▲         |
           | Writes  | Reads                   | Reads   | Writes
           |         |                         |         |
           ▼         |                         |         ▼
   ┌──────────────────────────────────────────────────────────────────────┐
   │                       Shared Memory Bus                              │
   │                                                                      │
   │  [Request Buffer]  <-----(sem_post signal)-----> [Response Buffer]   │
   │                         (POSIX SHM & Semaphores)                     │
   └──────────────────────────────────────────────────────────────────────┘
```

## ✨ Features

* **Concurrent C Server:** A multi-threaded TCP server written in C using `pthreads` to handle multiple connections simultaneously.
* **Parallel Python Execution:** A Python dispatcher (`ToolBServer`) that manages a pool of worker processes using `multiprocessing`, allowing application logic to bypass the GIL and run in true parallel.
* **Efficient Signaling:** Uses POSIX semaphores for high-performance, low-latency notification between the C server and the Python dispatcher, eliminating CPU-intensive polling.
* **Bidirectional IPC:** Utilizes two shared memory ring buffers for high-speed data exchange.
* **High-Performance C Parser:** A robust C-based HTTP parser that handles methods, paths, query parameters, headers, and multipart/form-data requests.
* **Production Hardening:**
    * **Request Timeouts:** C worker threads will time out and send a `504` error if the Python application takes too long to respond, preventing resource leaks.
    * **Robust Error Handling:** The C server includes comprehensive error checking for all critical system calls.
    * **Graceful Shutdown:** Both the C server and Python app use signal handlers (`SIGINT`) to clean up shared memory, semaphores, and child processes on exit (`Ctrl+C`).
    * **Race Condition Safety:** The C server uses `pthread_mutex` to protect shared resources, and the Python dispatcher is architected to prevent deadlocks under high concurrent load.
* **Live Monitoring Dashboard:** A standalone Python script that provides a real-time, text-based UI to monitor the status and buffer usage of the running system.

## 📂 Directory Structure

```text
toolb/
├── app/
│   ├── main.py                # Example FastAPI application
│   ├── server.py              # The core ToolBServer dispatcher
│   ├── toolb_shm_structs.py   # Python ctypes mirror of the C structs
│   └── monitor.py             # The live monitoring dashboard
├── bin/
│   └── heartware_server       # The compiled C server binary
├── src/
│   ├── c_parser.c             # The C-based HTTP parser
│   ├── heartware_server.c     # The main multi-threaded C server code
│   └── include/
│       └── toolb_shm.h        # The C header defining shared memory structs
├── Makefile                   # Automates the build and run process
└── README.md                  # This file
```

## 🚀 Build & Run Instructions

### Prerequisites
* A C compiler (`gcc` or `clang`)
* `make`
* Python 3
* A Python virtual environment is recommended.

### 1. Compile the C Server
From the project root directory, run the `make` command. This will compile all C source files and place the final executable in the `bin/` directory.
```bash
make clean && make
```

### 2. Run the System
You need to run the three main components concurrently in separate terminal windows.

* **Terminal 1: Start the C Server**
  ```bash
  make run-server
  ```
* **Terminal 2: Start the Python Application Server**
  ```bash
  make run-app
  ```
* **Terminal 3: Start the Live Monitor**
  ```bash
  make run-monitor
  ```

### 3. Test the Server
With all components running, open a fourth terminal to send requests using `curl`.

* **Simple GET Request:**
  ```bash
  curl http://localhost:8080/
  ```
* **GET with Query Parameters:**
  ```bash
  curl "http://localhost:8080/api/users?id=123&role=admin"
  ```
* **POST with JSON Body:**
  ```bash
  curl -X POST http://localhost:8080/api/data \
  -H "Content-Type: application/json" \
  -d '{"message": "hello"}'
  ```
* **Test Concurrency:** Send multiple requests simultaneously and watch the logs and monitor.
  ```bash
  curl "http://localhost:8080/api/status" & \
  curl "http://localhost:8080/api/users?id=101&role=admin" & \
  curl -X POST http://localhost:8080/api/data -d '{"id": 1}' &
  ```

<img width="1470" height="956" alt="image" src="https://github.com/user-attachments/assets/c93503aa-5c4b-4322-afcf-27dcfcef62a1" />

## 🛣️ Future Roadmap

* **Performance Optimization:** Profile the C parser and experiment with replacing key functions with **Compiler Intrinsics** (SSE/AVX) for a potential performance boost.
* **HTTPS/TLS Support:** Integrate a library like OpenSSL into the C server to handle encrypted connections.
* **Configuration Management:** Move hardcoded values like port numbers and buffer sizes into a configuration file (`config.ini`, etc.).
* **Packaging:** Package the Python framework as a PyPI module and create a `Dockerfile` for easy deployment.
            
