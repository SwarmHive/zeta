"""Tests for ZET format Python bindings."""

import os
import tempfile
import unittest
from pathlib import Path

from src.formats.zet.python.zet_format import ZetWriter, ZetReader, ZetMessage


class TestZetFormat(unittest.TestCase):
    """Test suite for ZET format library."""
    
    def setUp(self):
        """Create a temporary file for each test."""
        self.temp_fd, self.temp_file = tempfile.mkstemp(suffix='.zet')
        os.close(self.temp_fd)
    
    def tearDown(self):
        """Clean up temporary file."""
        if os.path.exists(self.temp_file):
            os.unlink(self.temp_file)
    
    def test_write_read_basic(self):
        """Test basic write and read operations."""
        # Write messages
        with ZetWriter(self.temp_file) as writer:
            writer.write_message("test/topic1", b"Hello, World!", sent_ns=1000, received_ns=2000)
            writer.write_message("test/topic2", b"Second message", sent_ns=3000, received_ns=4000)
        
        # Read messages back
        with ZetReader(self.temp_file) as reader:
            start_time = reader.get_start_time()
            self.assertGreater(start_time, 0)
            
            msg1 = reader.read_message()
            self.assertIsNotNone(msg1)
            self.assertEqual(msg1.topic, "test/topic1")
            self.assertEqual(msg1.data, b"Hello, World!")
            self.assertEqual(msg1.sent_ns, 1000)
            self.assertEqual(msg1.received_ns, 2000)
            
            msg2 = reader.read_message()
            self.assertIsNotNone(msg2)
            self.assertEqual(msg2.topic, "test/topic2")
            self.assertEqual(msg2.data, b"Second message")
            self.assertEqual(msg2.sent_ns, 3000)
            self.assertEqual(msg2.received_ns, 4000)
            
            # Should return None at end of file
            msg3 = reader.read_message()
            self.assertIsNone(msg3)
    
    def test_empty_message(self):
        """Test writing and reading empty messages."""
        with ZetWriter(self.temp_file) as writer:
            writer.write_message("empty/topic", b"")
        
        with ZetReader(self.temp_file) as reader:
            msg = reader.read_message()
            self.assertIsNotNone(msg)
            self.assertEqual(msg.topic, "empty/topic")
            self.assertEqual(msg.data, b"")
            self.assertEqual(len(msg.data), 0)
    
    def test_binary_data(self):
        """Test writing and reading binary data."""
        binary_data = bytes([0x00, 0xFF, 0xAB, 0xCD, 0xEF, 0x00, 0x12, 0x34])
        
        with ZetWriter(self.temp_file) as writer:
            writer.write_message("binary/data", binary_data, sent_ns=5000, received_ns=6000)
        
        with ZetReader(self.temp_file) as reader:
            msg = reader.read_message()
            self.assertIsNotNone(msg)
            self.assertEqual(msg.data, binary_data)
            self.assertEqual(msg.sent_ns, 5000)
            self.assertEqual(msg.received_ns, 6000)
    
    def test_multiple_messages_same_topic(self):
        """Test writing and reading multiple messages with the same topic."""
        num_messages = 100
        topic = "repeated/topic"
        
        with ZetWriter(self.temp_file) as writer:
            for i in range(num_messages):
                data = f"Message {i}".encode('utf-8')
                writer.write_message(topic, data, sent_ns=i * 1000, received_ns=i * 1000 + 500)
        
        with ZetReader(self.temp_file) as reader:
            for i in range(num_messages):
                msg = reader.read_message()
                self.assertIsNotNone(msg)
                self.assertEqual(msg.topic, topic)
                self.assertEqual(msg.data, f"Message {i}".encode('utf-8'))
                self.assertEqual(msg.sent_ns, i * 1000)
                self.assertEqual(msg.received_ns, i * 1000 + 500)
            
            # Verify EOF
            self.assertIsNone(reader.read_message())
    
    def test_iterator_interface(self):
        """Test the iterator interface of ZetReader."""
        messages = [
            ("topic1", b"data1", 1000, 2000),
            ("topic2", b"data2", 3000, 4000),
            ("topic3", b"data3", 5000, 6000),
        ]
        
        with ZetWriter(self.temp_file) as writer:
            for topic, data, sent_ns, received_ns in messages:
                writer.write_message(topic, data, sent_ns, received_ns)
        
        with ZetReader(self.temp_file) as reader:
            read_messages = list(reader)
            self.assertEqual(len(read_messages), len(messages))
            
            for read_msg, (topic, data, sent_ns, received_ns) in zip(read_messages, messages):
                self.assertEqual(read_msg.topic, topic)
                self.assertEqual(read_msg.data, data)
                self.assertEqual(read_msg.sent_ns, sent_ns)
                self.assertEqual(read_msg.received_ns, received_ns)
    
    def test_flush(self):
        """Test the flush functionality."""
        with ZetWriter(self.temp_file) as writer:
            writer.write_message("flush/test", b"Test flush")
            writer.flush()
            writer.write_message("flush/test", b"After flush", sent_ns=1000, received_ns=2000)
        
        with ZetReader(self.temp_file) as reader:
            msg1 = reader.read_message()
            self.assertIsNotNone(msg1)
            self.assertEqual(msg1.data, b"Test flush")
            
            msg2 = reader.read_message()
            self.assertIsNotNone(msg2)
            self.assertEqual(msg2.data, b"After flush")
    
    def test_context_manager(self):
        """Test that context managers properly close resources."""
        # Write with context manager
        with ZetWriter(self.temp_file) as writer:
            writer.write_message("test", b"data")
        
        # Read with context manager
        with ZetReader(self.temp_file) as reader:
            msg = reader.read_message()
            self.assertIsNotNone(msg)
    
    def test_unicode_topic(self):
        """Test topics with unicode characters."""
        topic = "test/unicode/日本語"
        data = b"Unicode topic test"
        
        with ZetWriter(self.temp_file) as writer:
            writer.write_message(topic, data)
        
        with ZetReader(self.temp_file) as reader:
            msg = reader.read_message()
            self.assertIsNotNone(msg)
            self.assertEqual(msg.topic, topic)
            self.assertEqual(msg.data, data)
    
    def test_large_message(self):
        """Test writing and reading large messages."""
        large_data = b"x" * (1024 * 1024)  # 1 MB
        
        with ZetWriter(self.temp_file) as writer:
            writer.write_message("large/message", large_data)
        
        with ZetReader(self.temp_file) as reader:
            msg = reader.read_message()
            self.assertIsNotNone(msg)
            self.assertEqual(len(msg.data), len(large_data))
            self.assertEqual(msg.data, large_data)
    
    def test_invalid_file_read(self):
        """Test opening non-existent file for reading."""
        with self.assertRaises(IOError):
            ZetReader("/tmp/nonexistent_file_12345.zet")
    
    def test_invalid_file_write(self):
        """Test creating writer in non-existent directory."""
        with self.assertRaises(IOError):
            ZetWriter("/nonexistent_dir_12345/test.zet")
    
    def test_message_repr(self):
        """Test ZetMessage string representation."""
        msg = ZetMessage(1000, 2000, "test/topic", b"test data")
        repr_str = repr(msg)
        self.assertIn("test/topic", repr_str)
        self.assertIn("1000", repr_str)
        self.assertIn("2000", repr_str)


if __name__ == '__main__':
    unittest.main()
