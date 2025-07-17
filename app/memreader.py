import ctypes
import mmap
import os
import sys
import time

# --- Constants and Structures (must match C header) ---
SHM_NAME = "/toolb_ringbuffer"
BUFFER_CAPACITY = 10
METHOD_MAX_LEN = 8
PATH_MAX_LEN = 128
BODY_MAX_LEN = 1024

class RequestMessage(ctypes.Structure):
    _fields_ = [
        ("method", ctypes.c_char * METHOD_MAX_LEN),
        ("path", ctypes.c_char * PATH_MAX_LEN),
        ("body_len", ctypes.c_int),
        ("body", ctypes.c_char * BODY_MAX_LEN),
    ]

class SharedRingBuffer(ctypes.Structure):
    _fields_ = [
        ("head", ctypes.c_uint32),
        ("tail", ctypes.c_uint32),
        ("capacity", ctypes.c_uint32),
        ("requests", RequestMessage * BUFFER_CAPACITY),
    ]

def consume_from_memory():
    """Connects to and consumes from the shared memory ring buffer."""

    # --- Use ctypes to call the C shm_open function for cross-platform compatibility ---
    # Load the standard C library
    libc = ctypes.CDLL("libc.so.6" if sys.platform.startswith('linux') else "libc.dylib")

    # Get a reference to the shm_open function
    shm_open = libc.shm_open
    shm_open.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_int]
    shm_open.restype = ctypes.c_int

    # Define O_RDWR constant if not in os module
    try:
        O_RDWR = os.O_RDWR
    except AttributeError:
        O_RDWR = 2 # Fallback for systems where it's not defined

    # Call shm_open to get the file descriptor
    fd = shm_open(SHM_NAME.encode('utf-8'), O_RDWR, 0o666)

    if fd < 0:
        print("🔴 [Python] Shared memory not found. Is 'heartware' running?")
        sys.exit(1)

    # Map the entire ring buffer structure into memory
    with mmap.mmap(fd, ctypes.sizeof(SharedRingBuffer), mmap.MAP_SHARED, mmap.PROT_READ | mmap.PROT_WRITE) as mm:
        shm = SharedRingBuffer.from_buffer(mm)
        print("✅ [Python] Consumer connected. Waiting for requests...")

        while True:
            # Check if the buffer is empty
            if shm.head == shm.tail:
                time.sleep(0.5) # Wait if no new requests
                continue

            # Get the message from the tail position
            request = shm.requests[shm.tail]

            print(f"⬅️  [Python] Consumed from slot {shm.tail}: Path='{request.path.decode()}'")

            # Advance the tail pointer, freeing the slot for the producer
            shm.tail = (shm.tail + 1) % shm.capacity

            # Simulate work; make it faster than producer to see buffer drain
            time.sleep(0.5)

if __name__ == "__main__":
    try:
        consume_from_memory()
    except KeyboardInterrupt:
        print("\n🐍 [Python] Consumer shutting down.")