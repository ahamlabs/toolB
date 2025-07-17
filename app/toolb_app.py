import ctypes
import mmap
import os
import sys
import time
import json
import signal

# --- Load shared memory structures ---
from toolb_shm_structs import SharedMemoryLayout, REQ_BUFFER_CAPACITY, RES_BUFFER_CAPACITY, SHM_NAME

# --- Global flag for graceful shutdown ---
running = True

def signal_handler(signum, frame):
    """Handles Ctrl+C to allow for a graceful exit."""
    global running
    print("\n🐍 [App] Signal received. Shutting down gracefully...")
    running = False

def main():
    """Connects to shared memory and processes requests in a loop."""
    signal.signal(signal.SIGINT, signal_handler)

    libc = ctypes.CDLL("libc.so.6" if sys.platform.startswith('linux') else "libc.dylib")
    shm_open = libc.shm_open
    shm_open.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_int]
    shm_open.restype = ctypes.c_int

    try:
        O_RDWR = os.O_RDWR
    except AttributeError:
        O_RDWR = 2

    fd = shm_open(SHM_NAME.encode('utf-8'), O_RDWR, 0o666)
    if fd < 0:
        print("🔴 [App] Shared memory not found. Is heartware_server running?")
        sys.exit(1)

    with mmap.mmap(fd, ctypes.sizeof(SharedMemoryLayout), mmap.MAP_SHARED, mmap.PROT_READ | mmap.PROT_WRITE) as mm:
        shm = SharedMemoryLayout.from_buffer(mm)
        req_buffer = shm.request_buffer
        res_buffer = shm.response_buffer

        print("✅ [App] Connected to shared memory. Waiting for requests...")

        local_req_tail = req_buffer.tail

        while running:
            if local_req_tail != req_buffer.head:
                request_index = local_req_tail % REQ_BUFFER_CAPACITY
                request = req_buffer.requests[request_index]

                print(f"⬅️  [App] Received request #{request.request_id}: {request.method.decode()} {request.path.decode()}")

                # --- Handle Multipart vs. JSON ---
                boundary = request.boundary.decode()
                if boundary:
                    # This is a multipart request, likely a file upload
                    # A real app would use a library like 'cgi' or 'werkzeug' to parse the body
                    status_message = f"Multipart request received with boundary: {boundary}"
                else:
                    # This is a standard request
                    status_message = "Standard request processed"

                # --- Write to Response Buffer ---
                res_head = res_buffer.head
                response_index = res_head % RES_BUFFER_CAPACITY
                response = res_buffer.responses[response_index]
                response.request_id = request.request_id
                response.status_code = 200

                response_body = json.dumps({
                    "status": status_message,
                    "request_id": request.request_id,
                    "path_received": request.path.decode()
                })
                response.body = response_body.encode('utf-8')

                res_buffer.head = res_head + 1
                local_req_tail += 1
                req_buffer.tail = local_req_tail

                print(f"➡️  [App] Sent response #{request.request_id}")
            else:
                time.sleep(0.01)

    print("✅ [App] Shutdown complete.")

if __name__ == "__main__":
    main()
