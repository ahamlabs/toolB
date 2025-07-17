import ctypes

# --- Configuration (must match C header) ---
REQ_BUFFER_CAPACITY = 16
RES_BUFFER_CAPACITY = 16
SHM_NAME = "/toolb_ipc"

METHOD_LEN = 8
PATH_LEN = 256
QUERY_PARAMS_LEN = 256
CONTENT_TYPE_LEN = 128
BOUNDARY_LEN = 70 # New field
AUTH_HEADER_LEN = 256
BODY_LEN = 4096
RESPONSE_LEN = 4096

# --- Structures (must match C header) ---
class RequestMessage(ctypes.Structure):
    _fields_ = [
        ("request_id", ctypes.c_uint64),
        ("method", ctypes.c_char * METHOD_LEN),
        ("path", ctypes.c_char * PATH_LEN),
        ("query_params", ctypes.c_char * QUERY_PARAMS_LEN),
        ("content_type", ctypes.c_char * CONTENT_TYPE_LEN),
        ("boundary", ctypes.c_char * BOUNDARY_LEN), # New field
        ("authorization", ctypes.c_char * AUTH_HEADER_LEN),
        ("content_length", ctypes.c_int),
        ("body", ctypes.c_char * BODY_LEN),
    ]

class ResponseMessage(ctypes.Structure):
    _fields_ = [
        ("request_id", ctypes.c_uint64),
        ("status_code", ctypes.c_int),
        ("body", ctypes.c_char * RESPONSE_LEN),
    ]

class RequestRingBuffer(ctypes.Structure):
    _fields_ = [
        ("head", ctypes.c_uint32),
        ("tail", ctypes.c_uint32),
        ("requests", RequestMessage * REQ_BUFFER_CAPACITY),
    ]

class ResponseRingBuffer(ctypes.Structure):
    _fields_ = [
        ("head", ctypes.c_uint32),
        ("tail", ctypes.c_uint32),
        ("responses", ResponseMessage * RES_BUFFER_CAPACITY),
    ]

class SharedMemoryLayout(ctypes.Structure):
    _fields_ = [
        ("request_buffer", RequestRingBuffer),
        ("response_buffer", ResponseRingBuffer),
    ]
