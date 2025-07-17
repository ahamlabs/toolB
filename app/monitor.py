import ctypes
import mmap
import os
import sys
import time
from toolb_shm_structs import SharedMemoryLayout, REQ_BUFFER_CAPACITY, RES_BUFFER_CAPACITY, SHM_NAME

def draw_bar(label, count, capacity):
    """Creates a text-based progress bar."""
    # Ensure count is never negative for display purposes
    display_count = max(0, count)
    percentage = (display_count / capacity) if capacity > 0 else 0
    bar_length = 30
    filled_length = int(bar_length * percentage)
    bar = '█' * filled_length + '-' * (bar_length - filled_length)
    return f"{label:<18} [{bar}] {display_count}/{capacity} ({percentage:.0%})"

def main():
    """Connects to shared memory and displays buffer status."""
    libc = ctypes.CDLL("libc.so.6" if sys.platform.startswith('linux') else "libc.dylib")
    shm_open = libc.shm_open
    shm_open.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_int]
    shm_open.restype = ctypes.c_int

    # --- FIX 1: Request Read-Write access instead of Read-Only ---
    try:
        O_RDWR = os.O_RDWR
    except AttributeError:
        O_RDWR = 2 # Fallback for systems where it's not defined

    fd = shm_open(SHM_NAME.encode('utf-8'), O_RDWR, 0o666)
    if fd < 0:
        print("🔴 [Monitor] Shared memory not found. Is heartware_server running?")
        sys.exit(1)

    # --- FIX 2: Map the memory as readable AND writable ---
    with mmap.mmap(fd, ctypes.sizeof(SharedMemoryLayout), mmap.MAP_SHARED, mmap.PROT_READ | mmap.PROT_WRITE) as mm:
        shm = SharedMemoryLayout.from_buffer(mm)

        while True:
            try:
                os.system('clear' if os.name != 'nt' else 'cls')

                # Use modulo arithmetic for correct count with wrapping pointers
                req_count = (shm.request_buffer.head - shm.request_buffer.tail + REQ_BUFFER_CAPACITY) % REQ_BUFFER_CAPACITY
                res_count = (shm.response_buffer.head - shm.response_buffer.tail + RES_BUFFER_CAPACITY) % RES_BUFFER_CAPACITY

                print("--- toolB Live Monitor ---")
                print(f"Time: {time.strftime('%H:%M:%S')}\n")
                print("Buffer Status:")
                print(draw_bar("Requests (C->Py)", req_count, REQ_BUFFER_CAPACITY))
                print(draw_bar("Responses (Py->C)", res_count, RES_BUFFER_CAPACITY))
                print("\nRaw Pointers:")
                print(f"Request Buffer:  Head={shm.request_buffer.head}, Tail={shm.request_buffer.tail}")
                print(f"Response Buffer: Head={shm.response_buffer.head}, Tail={shm.response_buffer.tail}")

                time.sleep(0.5)
            except KeyboardInterrupt:
                print("\n✅ [Monitor] Shutting down.")
                break

if __name__ == "__main__":
    main()
