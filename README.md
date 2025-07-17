# toolB — High-Speed Middleware Engine

**toolB** is a high-performance, experimental middleware engine designed to accelerate Python-based web servers. It achieves this by offloading performance-critical tasks—such as network I/O and HTTP parsing—to a multi-threaded C server. Communication with the Python layer occurs via a lock-free, shared memory ring buffer.

This project demonstrates a hybrid **C + Python** architecture that circumvents traditional bottlenecks, such as Python’s Global Interpreter Lock (GIL), for network operations.

---

## 🎯 Core Idea & Architecture

Modern Python frameworks like **FastAPI** and **Flask** are developer-friendly but can struggle with latency and concurrency due to the GIL. toolB introduces a low-level engine that handles I/O and parsing in C, enabling true parallelism and low-latency performance.

### ⚙️ Architecture Overview

The system follows a **Producer-Consumer** pattern:

- **Producer (C Server - `heartware_server`)**: A multi-threaded TCP server written in C that accepts and parses HTTP requests using a high-performance C parser.
- **Shared Memory Bus**: A POSIX shared memory segment with two **lock-free ring buffers** for high-speed, bidirectional communication.
- **Consumer (Python App - `toolb_app.py`)**: A Python process that reads requests from shared memory, processes them, and writes responses back.

```text
   Raw HTTP Requests
           |
 (handled concurrently)
           |
           v
   ┌──────────────────┐      ┌──────────────────┐      ┌────────────────┐
   │ heartware_server ◄────► │  Shared Memory   ◄────► │ toolb_app.py   │
   │ (Multi-threaded) │      │ (Ring Buffers)   │      │ (Python Logic) │
   └──────────────────┘      └──────────────────┘      └────────────────┘
```

---

## ✨ Features

- **🧵 Multi-threaded C Server**: Handles concurrent TCP connections using `pthreads`.
- **⚡ Lock-free Shared Memory IPC**: Enables high-speed communication between the C and Python layers.
- **🧠 High-Performance HTTP Parsing**: Parses headers, methods, paths, query strings, and `multipart/form-data`.
- **🛡️ Robust Error Handling**: All critical system calls in C are wrapped with error checks.
- **🧹 Graceful Shutdown**: Signal handling (`SIGINT`) ensures resource cleanup on exit.
- **🔒 Race Condition Prevention**: Uses `pthread_mutex` to protect shared state when needed.
- **📊 Real-Time Monitoring**: A curses-based live dashboard shows system metrics and buffer status.

---

## 📂 Directory Structure

```bash
toolb/
├── app/
│   ├── toolb_app.py           # Python consumer app
│   ├── toolb_shm_structs.py   # ctypes mirror of C structs
│   └── monitor.py             # Real-time system dashboard
├── bin/
│   └── heartware_server       # Compiled C server binary
├── src/
│   ├── c_parser.c             # C-based HTTP parser
│   ├── heartware_server.c     # Multi-threaded server code
│   └── include/
│       └── toolb_shm.h        # Shared memory struct definitions
├── Makefile                   # Build automation
└── README.md                  # You are here 🚀
```

---

## 🚀 Build & Run Instructions

### 🧰 Prerequisites

- A C compiler: `gcc` or `clang`
- `make`
- Python 3.x

### 🔨 Step 1: Build the C Server

```bash
make clean && make
```

This compiles the C code and places the binary in the `bin/` directory.

### 🧪 Step 2: Run the System (Three Terminals)

#### Terminal 1: Start the C Server

```bash
make run-server
```

#### Terminal 2: Start the Python App

```bash
make run-app
```

#### Terminal 3: Start the Monitoring Dashboard

```bash
make run-monitor
```

---

## 🧪 Step 3: Test the Server

### ✅ Basic GET

```bash
curl http://localhost:8080/api/status
```

### 🧾 GET with Query Parameters

```bash
curl "http://localhost:8080/api/users?id=123&role=admin"
```

### 📤 POST with JSON

```bash
curl -X POST http://localhost:8080/api/data \
-H "Content-Type: application/json" \
-d '{"message": "hello"}'
```

### 🧪 Test Concurrency

```bash
curl "http://localhost:8080/api/status" & \
curl "http://localhost:8080/api/users?id=101" & \
curl -X POST http://localhost:8080/api/data -d '{"id": 1}' &
```

Watch the logs and monitor dashboard for real-time updates.

---
<img width="2940" height="1912" alt="image" src="https://github.com/user-attachments/assets/184263ea-d24a-4921-a0d2-ff16629a9b99" />


## 🛣️ Future Roadmap

- **🚀 Performance Optimizations**: Use SIMD (SSE/AVX) intrinsics in critical C functions.
- **🔐 TLS Support**: Add HTTPS via OpenSSL.
- **⚙️ Config Management**: Replace hardcoded constants with `config.ini` or environment-based config.

---

## 👨‍💻 Authors

**Sayan Sarkar**  
GitHub: [psyoherion](https://github.com/psypherion)  
Email: [williamskyle562@gmail.com](mailto:williamskyle562@gmail.com)

---

## 📄 License

This project is currently released as a proof-of-concept. License details coming soon.
