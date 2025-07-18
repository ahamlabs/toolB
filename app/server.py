import ctypes
import mmap
import os
import sys
import time
import json
import signal
import asyncio
from toolb_shm_structs import SharedMemoryLayout, REQ_BUFFER_CAPACITY, RES_BUFFER_CAPACITY, SHM_NAME

class ToolBServer:
    """
    An application server that runs an ASGI application (like FastAPI)
    using the toolB high-performance C server and shared memory IPC.
    """
    def __init__(self, app):
        self.app = app
        self.shm = None
        self.req_buffer = None
        self.res_buffer = None
        self.running = True
        signal.signal(signal.SIGINT, self._handle_shutdown)

    def _handle_shutdown(self, signum, frame):
        print("\n🐍 [Server] Signal received. Shutting down gracefully...")
        self.running = False

    def _connect_to_shm(self):
        """Connects to the shared memory segment created by the C server."""
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
            print("🔴 [Server] Shared memory not found. Is heartware_server running?")
            sys.exit(1)

        mm = mmap.mmap(fd, ctypes.sizeof(SharedMemoryLayout), mmap.MAP_SHARED, mmap.PROT_READ | mmap.PROT_WRITE)
        self.shm = SharedMemoryLayout.from_buffer(mm)
        self.req_buffer = self.shm.request_buffer
        self.res_buffer = self.shm.response_buffer
        print("✅ [Server] Connected to shared memory.")

    async def _asgi_dispatch(self, request):
        """
        Translates a toolB RequestMessage into an ASGI scope and calls the
        ASGI application.
        """
        response_future = asyncio.Future()
        response_data = {}

        async def receive():
            return {
                'type': 'http.request',
                'body': request.body,
                'more_body': False
            }

        async def send(message):
            if message['type'] == 'http.response.start':
                response_data['status'] = message['status']
                response_data['headers'] = message.get('headers', [])
            elif message['type'] == 'http.response.body':
                response_data['body'] = message.get('body', b'')
                if not response_future.done():
                    response_future.set_result(response_data)

        scope = {
            'type': 'http',
            'asgi': {'version': '3.0'},
            'http_version': '1.1',
            'server': ('127.0.0.1', 8080),
            'client': ('127.0.0.1', 9999),
            'scheme': 'http',
            'method': request.method.decode(),
            'path': request.path.decode(),
            'query_string': request.query_params.decode().encode(),
            'headers': [
                (b'content-type', request.content_type.decode().encode()),
                (b'authorization', request.authorization.decode().encode())
            ]
        }

        await self.app(scope, receive, send)
        return await response_future

    def run(self):
        """The main server loop."""
        self._connect_to_shm()

        while self.running:
            # Always read the latest head from the C server
            current_head = self.req_buffer.head
            current_tail = self.req_buffer.tail

            # Check if there's a new request to process
            if current_tail != current_head:
                # Use modulo for array access
                request_index = current_tail % REQ_BUFFER_CAPACITY
                request = self.req_buffer.requests[request_index]

                print(f"⬅️  [Server] Received request #{request.request_id} from C.")

                response_data = asyncio.run(self._asgi_dispatch(request))

                # Use modulo for array access
                res_head = self.res_buffer.head
                response_index = res_head % RES_BUFFER_CAPACITY
                response_msg = self.res_buffer.responses[response_index]

                response_msg.request_id = request.request_id
                response_msg.status_code = response_data.get('status', 500)
                response_msg.body = response_data.get('body', b'')

                # DEFINITIVE FIX: Update pointers using modulo arithmetic to match the C server.
                # This ensures both processes stay perfectly synchronized.
                self.res_buffer.head = (res_head + 1) % RES_BUFFER_CAPACITY
                self.req_buffer.tail = (current_tail + 1) % REQ_BUFFER_CAPACITY

                print(f"➡️  [Server] Sent response #{request.request_id} to C.")
            else:
                time.sleep(0.01)

        print("✅ [Server] Shutdown complete.")
