"""
Python bindings for ZET format library.

Provides classes for reading and writing .zet files.
"""

import ctypes
import os
from pathlib import Path
from typing import Optional, Tuple
import platform


# Find the shared library
def _find_library():
    """Locate the zet_format shared library."""
    # When using Bazel, the library will be in the runfiles
    # For development, try common locations
    possible_paths = [
        # Bazel runfiles location
        Path(__file__).parent / "libzet_format.so",
        # Development build location
        Path(__file__).parent.parent / "c" / "libzet_format.so",
    ]
    
    for path in possible_paths:
        if path.exists():
            return str(path)
    
    raise RuntimeError("Could not find libzet_format.so")


# Load the C library
_lib_path = _find_library()
_lib = ctypes.CDLL(_lib_path)


# C structure definitions
class _ZetHeader(ctypes.Structure):
    _fields_ = [
        ("magic", ctypes.c_char * 4),
        ("version", ctypes.c_uint32),
        ("start_time_ns", ctypes.c_uint64),
        ("reserved", ctypes.c_uint8 * 16),
    ]


class _ZetMessageHeader(ctypes.Structure):
    _fields_ = [
        ("sent_ns", ctypes.c_uint64),
        ("received_ns", ctypes.c_uint64),
        ("topic_len", ctypes.c_uint16),
        ("payload_size", ctypes.c_uint32),
    ]


class _ZetMessage(ctypes.Structure):
    _fields_ = [
        ("sent_ns", ctypes.c_uint64),
        ("received_ns", ctypes.c_uint64),
        ("topic", ctypes.POINTER(ctypes.c_char)),
        ("data", ctypes.c_void_p),
        ("size", ctypes.c_size_t),
    ]


# Define C function signatures
# Writer API
_lib.zet_writer_create.argtypes = [ctypes.c_char_p]
_lib.zet_writer_create.restype = ctypes.c_void_p

_lib.zet_writer_destroy.argtypes = [ctypes.c_void_p]
_lib.zet_writer_destroy.restype = None

_lib.zet_writer_write_message.argtypes = [
    ctypes.c_void_p,  # writer
    ctypes.c_uint64,  # sent_ns
    ctypes.c_uint64,  # received_ns
    ctypes.c_char_p,  # topic
    ctypes.c_void_p,  # data
    ctypes.c_size_t,  # size
]
_lib.zet_writer_write_message.restype = ctypes.c_int

_lib.zet_writer_flush.argtypes = [ctypes.c_void_p]
_lib.zet_writer_flush.restype = None

# Reader API
_lib.zet_reader_create.argtypes = [ctypes.c_char_p]
_lib.zet_reader_create.restype = ctypes.c_void_p

_lib.zet_reader_destroy.argtypes = [ctypes.c_void_p]
_lib.zet_reader_destroy.restype = None

_lib.zet_reader_read_message.argtypes = [ctypes.c_void_p, ctypes.POINTER(_ZetMessage)]
_lib.zet_reader_read_message.restype = ctypes.c_int

_lib.zet_message_free.argtypes = [ctypes.POINTER(_ZetMessage)]
_lib.zet_message_free.restype = None

_lib.zet_reader_get_start_time.argtypes = [ctypes.c_void_p]
_lib.zet_reader_get_start_time.restype = ctypes.c_uint64


class ZetMessage:
    """A message read from a .zet file."""
    
    def __init__(self, sent_ns: int, received_ns: int, topic: str, data: bytes):
        self.sent_ns = sent_ns
        self.received_ns = received_ns
        self.topic = topic
        self.data = data
    
    def __repr__(self):
        return f"ZetMessage(topic='{self.topic}', size={len(self.data)}, sent_ns={self.sent_ns}, received_ns={self.received_ns})"


class ZetWriter:
    """Writer for .zet format files."""
    
    def __init__(self, filename: str):
        """
        Create a new .zet file for writing.
        
        Args:
            filename: Path to the .zet file to create
        """
        self._filename = filename
        self._writer = _lib.zet_writer_create(filename.encode('utf-8'))
        if not self._writer:
            raise IOError(f"Failed to create ZET writer for {filename}")
    
    def write_message(self, topic: str, data: bytes, sent_ns: int = 0, received_ns: int = 0) -> None:
        """
        Write a message to the .zet file.
        
        Args:
            topic: Topic name
            data: Message payload
            sent_ns: Timestamp when message was sent (nanoseconds, 0 if unknown)
            received_ns: Timestamp when message was received (nanoseconds, 0 for current time)
        """
        if not self._writer:
            raise RuntimeError("Writer is closed")
        
        result = _lib.zet_writer_write_message(
            self._writer,
            ctypes.c_uint64(sent_ns),
            ctypes.c_uint64(received_ns),
            topic.encode('utf-8'),
            data,
            len(data)
        )
        
        if result != 0:
            raise IOError(f"Failed to write message to {self._filename}")
    
    def flush(self) -> None:
        """Flush the file buffer to disk."""
        if self._writer:
            _lib.zet_writer_flush(self._writer)
    
    def close(self) -> None:
        """Close the writer and flush all data."""
        if self._writer:
            _lib.zet_writer_destroy(self._writer)
            self._writer = None
    
    def __enter__(self):
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
        return False
    
    def __del__(self):
        self.close()


class ZetReader:
    """Reader for .zet format files."""
    
    def __init__(self, filename: str):
        """
        Open a .zet file for reading.
        
        Args:
            filename: Path to the .zet file to read
        """
        self._filename = filename
        self._reader = _lib.zet_reader_create(filename.encode('utf-8'))
        if not self._reader:
            raise IOError(f"Failed to open ZET file {filename}")
    
    def read_message(self) -> Optional[ZetMessage]:
        """
        Read the next message from the file.
        
        Returns:
            ZetMessage if successful, None if end of file
        """
        if not self._reader:
            raise RuntimeError("Reader is closed")
        
        c_msg = _ZetMessage()
        result = _lib.zet_reader_read_message(self._reader, ctypes.byref(c_msg))
        
        if result != 0:
            return None  # End of file or error
        
        # Convert C message to Python
        topic = ctypes.string_at(c_msg.topic).decode('utf-8')
        data = ctypes.string_at(c_msg.data, c_msg.size)
        
        msg = ZetMessage(c_msg.sent_ns, c_msg.received_ns, topic, data)
        
        # Free the C message memory
        _lib.zet_message_free(ctypes.byref(c_msg))
        
        return msg
    
    def get_start_time(self) -> int:
        """
        Get the recording start time from the file header.
        
        Returns:
            Start time in nanoseconds
        """
        if not self._reader:
            raise RuntimeError("Reader is closed")
        
        return _lib.zet_reader_get_start_time(self._reader)
    
    def read_all_messages(self):
        """
        Generator that yields all messages in the file.
        
        Yields:
            ZetMessage objects
        """
        while True:
            msg = self.read_message()
            if msg is None:
                break
            yield msg
    
    def close(self) -> None:
        """Close the reader."""
        if self._reader:
            _lib.zet_reader_destroy(self._reader)
            self._reader = None
    
    def __enter__(self):
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
        return False
    
    def __del__(self):
        self.close()
    
    def __iter__(self):
        """Iterate over all messages in the file."""
        return self.read_all_messages()
