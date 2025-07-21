import ctypes
import mmap
import os
import sys
import time
import json
import signal
import asyncio
import multiprocessing
import threading
import importlib
import logging
from toolb_shm_structs import SharedMemoryLayout, REQ_BUFFER_CAPACITY, RES_BUFFER_CAPACITY, SHM_NAME, SEM_REQUEST_READY

# --- Structured Logging Setup ---
class JsonFormatter(logging.Formatter):
    def format(self, record):
        log_record = {
            "timestamp": self.formatTime(record, self.datefmt),
            "level": record.levelname,
            "source": "python_server",
            "process_id": record.process,
            "message": record.getMessage()
        }
        # Add extra fields if they exist
        if hasattr(record, 'extra_data'):
            log_record.update(record.extra_data)
        return json.dumps(log_record)

def setup_logging():
    logger = logging.getLogger("toolB_App")
    if not logger.handlers:
        logger.setLevel(logging.INFO)
        handler = logging.StreamHandler()
        # Use ISO 8601 format with Z for UTC
        handler.setFormatter(JsonFormatter(datefmt='%Y-%m-%dT%H:%M:%SZ'))
        logger.addHandler(handler)
    return logger

logger = setup_logging()

# --- C Function Wrappers ---
if sys.platform != 'win32':
    libc = ctypes.CDLL("libc.so.6" if sys.platform.startswith('linux') else "libc.dylib")
    sem_open = libc.sem_open; sem_open.argtypes = [ctypes.c_char_p, ctypes.c_int]; sem_open.restype = ctypes.c_void_p
    sem_trywait = libc.sem_trywait; sem_trywait.argtypes = [ctypes.c_void_p]; sem_trywait.restype = ctypes.c_int
    sem_close = libc.sem_close; sem_close.argtypes = [ctypes.c_void_p]; sem_close.restype = ctypes.c_int
else:
    sem_open = sem_trywait = sem_close = None

# --- Worker Process Function ---
def worker_process(app_path, task_queue, response_queue):
    signal.signal(signal.SIGINT, signal.SIG_IGN)
    module_str, app_str = app_path.split(":")
    module = importlib.import_module(module_str)
    app = getattr(module, app_str)

    logger.info("Worker process started.")
    while True:
        request_data = task_queue.get()
        if request_data is None: break

        response_data = asyncio.run(_asgi_dispatch(app, request_data))
        response_queue.put(response_data)

    logger.info("Worker process shutting down.")

async def _asgi_dispatch(app, request_data):
    # ... (ASGI dispatch logic is unchanged) ...
    response_future = asyncio.Future(); response_dict = {}
    async def receive(): return {'type': 'http.request', 'body': request_data['body'], 'more_body': False}
    async def send(message):
        if message['type'] == 'http.response.start': response_dict['status'] = message['status']
        elif message['type'] == 'http.response.body':
            response_dict['body'] = message.get('body', b'');
            if not response_future.done(): response_future.set_result(response_dict)
    scope = request_data['scope']
    await app(scope, receive, send)
    final_response = await response_future
    final_response['request_id'] = request_data['request_id']
    return final_response

class ToolBServer:
    def __init__(self, app_path):
        self.app_path = app_path
        self.running = True
        self.processes = []
        self.response_lock = threading.Lock()
        # ... (rest of init is unchanged) ...

    def _connect_to_ipc(self):
        # ... (connection logic is unchanged) ...
        shm_open_c = libc.shm_open; shm_open_c.argtypes = [ctypes.c_char_p, ctypes.c_int]; shm_open_c.restype = ctypes.c_int
        O_RDWR = 2; fd = shm_open_c(SHM_NAME.encode('utf-8'), O_RDWR, 0o666)
        if fd < 0: logger.critical("Shared memory not found. Is C server running?"); sys.exit(1)
        mm = mmap.mmap(fd, ctypes.sizeof(SharedMemoryLayout), mmap.MAP_SHARED, mmap.PROT_READ | mmap.PROT_WRITE)
        self.shm = SharedMemoryLayout.from_buffer(mm); self.req_buffer = self.shm.request_buffer; self.res_buffer = self.shm.response_buffer
        self.request_sem = sem_open(SEM_REQUEST_READY.encode('utf-8'), O_RDWR)
        if self.request_sem == -1: logger.critical("Failed to open semaphore."); sys.exit(1)
        logger.info("Dispatcher connected to IPC.")

    def _result_writer(self, response_queue):
        while self.running or not response_queue.empty():
            try:
                response_data = response_queue.get(timeout=0.1)
                with self.response_lock:
                    res_head = self.res_buffer.head
                    response_index = res_head % RES_BUFFER_CAPACITY
                    response_msg = self.res_buffer.responses[response_index]
                    response_msg.request_id = response_data['request_id']
                    response_msg.status_code = response_data.get('status', 500)
                    response_msg.body = response_data.get('body', b'')
                    self.res_buffer.head = (res_head + 1) % RES_BUFFER_CAPACITY
                log_extra = {'extra_data': {'request_id': response_msg.request_id}}
                logger.info("Sent response to C.", extra=log_extra)
            except multiprocessing.queues.Empty:
                continue

    def run(self, num_workers=None):
        if num_workers is None: num_workers = os.cpu_count()
        logger.info(f"Starting server with {num_workers} worker processes.")
        self._connect_to_ipc()
        task_queue = multiprocessing.Queue(); response_queue = multiprocessing.Queue()
        for _ in range(num_workers):
            p = multiprocessing.Process(target=worker_process, args=(self.app_path, task_queue, response_queue));
            self.processes.append(p); p.start()
        writer_thread = threading.Thread(target=self._result_writer, args=(response_queue,)); writer_thread.start()
        def handle_shutdown(signum, frame):
            logger.info("Signal received. Shutting down..."); self.running = False
        signal.signal(signal.SIGINT, handle_shutdown)

        while self.running:
            if sem_trywait(self.request_sem) == -1:
                time.sleep(0.001); continue
            current_head = self.req_buffer.head; current_tail = self.req_buffer.tail
            while current_tail != current_head:
                request_index = current_tail % REQ_BUFFER_CAPACITY
                request = self.req_buffer.requests[request_index]
                scope = {'type': 'http', 'asgi': {'version': '3.0'}, 'http_version': '1.1', 'server': ('127.0.0.1', 8080), 'client': ('127.0.0.1', 9999), 'scheme': 'http', 'method': request.method.decode(), 'path': request.path.decode(), 'query_string': request.query_params.decode().encode(), 'headers': []}
                task_data = { 'request_id': request.request_id, 'scope': scope, 'body': request.body }
                task_queue.put(task_data)
                current_tail = (current_tail + 1) % REQ_BUFFER_CAPACITY
                log_extra = {'extra_data': {'request_id': request.request_id}}
                logger.info("Dispatched request to workers.", extra=log_extra)
            self.req_buffer.tail = current_tail

        logger.info("Terminating worker processes...")
        for _ in self.processes: task_queue.put(None)
        for p in self.processes: p.join(timeout=2)
        for p in self.processes:
            if p.is_alive(): logger.warning(f"Worker {p.pid} did not exit gracefully. Terminating."); p.terminate(); p.join()
        writer_thread.join(timeout=2)
        if self.request_sem: sem_close(self.request_sem)
        logger.info("Shutdown complete.")
