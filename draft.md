# Software Requirements Specification (SRS): toolB Engine 
## converting c codebase to go

## 1. Introduction

**1.1 Purpose**
This document specifies the architecture, feature set, and engineering requirements for **toolB**, a high-performance, hybrid Go/Python application engine. The system is designed to replace traditional ASGI/WSGI servers (e.g., Uvicorn, Gunicorn) by offloading network I/O, HTTP parsing, and process management to a Go-based supervisor, while exposing a zero-copy shared memory bus to a pool of Python workers.

**1.2 Scope and Application Domain**
toolB serves two distinct workloads:

1. **Standard Web APIs:** Acts as a highly resilient, drop-in ASGI server for FastAPI/Django, immune to Python event-loop blocking and memory-leak crashes.
2. **Heavy Data Pipelines:** Acts as a computational substrate for machine learning and high-frequency sensor processing by providing true zero-copy memory access between the network layer and Python `NumPy` environments, bypassing ASGI serialization.

---

## 2. Overall Description & Architecture

**2.1 System Context**
The engine operates as a single deployable binary that encapsulates both the Go network supervisor and the Python multiprocessing orchestrator. It sits directly behind the public internet or a load balancer, intercepting HTTP/TCP traffic and dispatching it to isolated Python worker processes via POSIX shared memory and Unix domain sockets (UDS).

**2.2 Operating Environment**

* **Host OS:** Linux (primary target for POSIX SHM and UDS optimizations), macOS.
* **Runtime:** Pre-compiled Go binary (embedded inside a Python Wheel).
* **Python Target:** Python 3.9+ (CPython).
* **Dependencies:** Standard library `net/http` (Go), `multiprocessing.shared_memory` (Python), and `ctypes`.

---

## 3. Specific Functional Requirements

### 3.1 Go Front-End Supervisor (The "Heart")

* **REQ-G1: Concurrent Network I/O:** The Go engine must handle all incoming TCP and TLS connections using Goroutines, ensuring the network layer is never blocked by downstream Python execution.
* **REQ-G2: HTTP/TLS Parsing:** Must utilize Go's `net/http` and `crypto/tls` libraries to handle HTTP/1.1, HTTP/2, WebSockets, and chunked transfer encoding natively.
* **REQ-G3: Worker Process Management:** The Go engine must spawn, monitor, and reap Python child processes. It must track per-worker RSS memory usage via the OS kernel and gracefully recycle workers that exceed memory limits (pre-warming replacements to ensure zero dropped packets).
* **REQ-G4: Debugger-Safe Mode:** The engine must intercept `SIGTRAP` signals from Python workers. Upon receipt, it must pause the worker's timeout clock and hold its specific HTTP connection open indefinitely to allow interactive `pdb` debugging without killing the worker.

### 3.2 Inter-Process Communication (IPC) & Memory Bus

* **REQ-I1: ASGI UDS Fallback:** For standard web frameworks, the system must serialize HTTP requests and stream them to Python workers over high-throughput Unix Domain Sockets.
* **REQ-I2: Zero-Copy Shared Memory (ZCSM):** For the native micro-framework, the Go engine must allocate a POSIX shared memory ring buffer (`syscall.SysVShmGet`). It must write raw request payloads directly into RAM and pass only the memory offset and size to the Python worker.
* **REQ-I3: In-Memory Task Queue:** The shared memory bus must support deferred job structs. A Python HTTP worker must be able to write a task payload to the buffer, which the Go engine then routes to a dedicated background Python worker, eliminating the need for Redis/Celery.

### 3.3 Python Dispatcher & Execution Environment

* **REQ-P1: Zero-Copy Deserialization:** The Python worker must utilize `multiprocessing.shared_memory` to read the Go memory segment. It must provide helper functions to instantly cast these memory blocks into `numpy.ndarray` objects without copying bytes.
* **REQ-P2: ASGI Interface:** Must provide a fully compliant ASGI callable interface to support standard applications like FastAPI without developer code modifications.

---

## 4. Non-Functional Requirements

| Attribute | Specification |
| --- | --- |
| **Performance (Latency)** | Zero-copy payload handoff (C/Go to Python) must occur in $< 50$ microseconds. |
| **Performance (Throughput)** | The Go layer must sustain $> 100,000$ concurrent idle WebSocket or Keep-Alive connections using $< 500$ MB of RAM. |
| **Reliability** | The engine must achieve a 100% request-retention rate during worker memory-recycling events. |
| **Usability (Packaging)** | The entire system must be installable via a single command: `pip install toolB`, with no C/Go compiler required on the target machine. |
| **Observability** | The Go engine must expose a `/metrics` endpoint serving Prometheus-formatted metrics (active connections, worker saturation, memory usage) natively. |

---

## 5. Developer Experience (DX) Feature List

This is how `toolB` differentiates itself in the market to everyday Python developers:

* **The Un-Crashable Event Loop:** Because Go holds the socket, a Python developer accidentally writing a blocking `time.sleep()` or heavy Pandas operation in an async route will only stall one worker, never dropping the server's health checks or incoming traffic.
* **Built-in Background Tasks:** Developers can dispatch asynchronous jobs directly to background workers using the shared memory bus. No Celery, no Redis, no extra Docker containers.
* **Zero-Downtime Memory Recycling:** Memory leaks from C-extensions or PyTorch are handled automatically. Go watches worker RAM, spins up a fresh worker in the background, and seamlessly swaps it without terminating inflight requests.
* **Native Telemetry & Middleware:** Gzip compression, CORS handling, rate limiting, and Prometheus metrics are handled in compiled Go. Python focuses strictly on business logic, instantly stripping 30% of the overhead from standard FastAPI apps.
* **Direct-to-NumPy Processing:** Machine learning engineers can bypass JSON/ASGI serialization entirely. Massive incoming payloads are written to RAM by Go and read as native NumPy arrays by Python instantly.# Software Requirements Specification (SRS): toolB Engine

## 1. Introduction

**1.1 Purpose**
This document specifies the architecture, feature set, and engineering requirements for **toolB**, a high-performance, hybrid Go/Python application engine. The system is designed to replace traditional ASGI/WSGI servers (e.g., Uvicorn, Gunicorn) by offloading network I/O, HTTP parsing, and process management to a Go-based supervisor, while exposing a zero-copy shared memory bus to a pool of Python workers.

**1.2 Scope and Application Domain**
toolB serves two distinct workloads:

1. **Standard Web APIs:** Acts as a highly resilient, drop-in ASGI server for FastAPI/Django, immune to Python event-loop blocking and memory-leak crashes.
2. **Heavy Data Pipelines:** Acts as a computational substrate for machine learning and high-frequency sensor processing by providing true zero-copy memory access between the network layer and Python `NumPy` environments, bypassing ASGI serialization.

---

## 2. Overall Description & Architecture

**2.1 System Context**
The engine operates as a single deployable binary that encapsulates both the Go network supervisor and the Python multiprocessing orchestrator. It sits directly behind the public internet or a load balancer, intercepting HTTP/TCP traffic and dispatching it to isolated Python worker processes via POSIX shared memory and Unix domain sockets (UDS).

**2.2 Operating Environment**

* **Host OS:** Linux (primary target for POSIX SHM and UDS optimizations), macOS.
* **Runtime:** Pre-compiled Go binary (embedded inside a Python Wheel).
* **Python Target:** Python 3.9+ (CPython).
* **Dependencies:** Standard library `net/http` (Go), `multiprocessing.shared_memory` (Python), and `ctypes`.

---

## 3. Specific Functional Requirements

### 3.1 Go Front-End Supervisor (The "Heart")

* **REQ-G1: Concurrent Network I/O:** The Go engine must handle all incoming TCP and TLS connections using Goroutines, ensuring the network layer is never blocked by downstream Python execution.
* **REQ-G2: HTTP/TLS Parsing:** Must utilize Go's `net/http` and `crypto/tls` libraries to handle HTTP/1.1, HTTP/2, WebSockets, and chunked transfer encoding natively.
* **REQ-G3: Worker Process Management:** The Go engine must spawn, monitor, and reap Python child processes. It must track per-worker RSS memory usage via the OS kernel and gracefully recycle workers that exceed memory limits (pre-warming replacements to ensure zero dropped packets).
* **REQ-G4: Debugger-Safe Mode:** The engine must intercept `SIGTRAP` signals from Python workers. Upon receipt, it must pause the worker's timeout clock and hold its specific HTTP connection open indefinitely to allow interactive `pdb` debugging without killing the worker.

### 3.2 Inter-Process Communication (IPC) & Memory Bus

* **REQ-I1: ASGI UDS Fallback:** For standard web frameworks, the system must serialize HTTP requests and stream them to Python workers over high-throughput Unix Domain Sockets.
* **REQ-I2: Zero-Copy Shared Memory (ZCSM):** For the native micro-framework, the Go engine must allocate a POSIX shared memory ring buffer (`syscall.SysVShmGet`). It must write raw request payloads directly into RAM and pass only the memory offset and size to the Python worker.
* **REQ-I3: In-Memory Task Queue:** The shared memory bus must support deferred job structs. A Python HTTP worker must be able to write a task payload to the buffer, which the Go engine then routes to a dedicated background Python worker, eliminating the need for Redis/Celery.

### 3.3 Python Dispatcher & Execution Environment

* **REQ-P1: Zero-Copy Deserialization:** The Python worker must utilize `multiprocessing.shared_memory` to read the Go memory segment. It must provide helper functions to instantly cast these memory blocks into `numpy.ndarray` objects without copying bytes.
* **REQ-P2: ASGI Interface:** Must provide a fully compliant ASGI callable interface to support standard applications like FastAPI without developer code modifications.

---

## 4. Non-Functional Requirements

| Attribute | Specification |
| --- | --- |
| **Performance (Latency)** | Zero-copy payload handoff (C/Go to Python) must occur in $< 50$ microseconds. |
| **Performance (Throughput)** | The Go layer must sustain $> 100,000$ concurrent idle WebSocket or Keep-Alive connections using $< 500$ MB of RAM. |
| **Reliability** | The engine must achieve a 100% request-retention rate during worker memory-recycling events. |
| **Usability (Packaging)** | The entire system must be installable via a single command: `pip install toolB`, with no C/Go compiler required on the target machine. |
| **Observability** | The Go engine must expose a `/metrics` endpoint serving Prometheus-formatted metrics (active connections, worker saturation, memory usage) natively. |

---

## 5. Developer Experience (DX) Feature List

This is how `toolB` differentiates itself in the market to everyday Python developers:

* **The Un-Crashable Event Loop:** Because Go holds the socket, a Python developer accidentally writing a blocking `time.sleep()` or heavy Pandas operation in an async route will only stall one worker, never dropping the server's health checks or incoming traffic.
* **Built-in Background Tasks:** Developers can dispatch asynchronous jobs directly to background workers using the shared memory bus. No Celery, no Redis, no extra Docker containers.
* **Zero-Downtime Memory Recycling:** Memory leaks from C-extensions or PyTorch are handled automatically. Go watches worker RAM, spins up a fresh worker in the background, and seamlessly swaps it without terminating inflight requests.
* **Native Telemetry & Middleware:** Gzip compression, CORS handling, rate limiting, and Prometheus metrics are handled in compiled Go. Python focuses strictly on business logic, instantly stripping 30% of the overhead from standard FastAPI apps.
* **Direct-to-NumPy Processing:** Machine learning engineers can bypass JSON/ASGI serialization entirely. Massive incoming payloads are written to RAM by Go and read as native NumPy arrays by Python instantly.
