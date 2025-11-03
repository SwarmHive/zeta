"""
Zetabus Python Library

A Python wrapper for NATS messaging with an API consistent with the C bus library.
"""

from typing import Callable, Optional
import nats
from nats.aio.client import Client as NATSClient
from nats.aio.msg import Msg
import asyncio


class ZetabusPublisher:
    """Publisher for a specific topic."""
    
    def __init__(self, bus: 'Zetabus', topic: str):
        self._bus = bus
        self._topic = topic
    
    async def publish(self, data: bytes) -> None:
        """
        Publish data to the publisher's topic.
        
        Args:
            data: Bytes to publish
        """
        await self._bus._nc.publish(self._topic, data)
    
    @property
    def topic(self) -> str:
        """Get the topic name."""
        return self._topic


class ZetabusSubscriber:
    """Subscriber for a specific topic."""
    
    def __init__(self, bus: 'Zetabus', topic: str, callback: Callable[[str, bytes], None]):
        self._bus = bus
        self._topic = topic
        self._callback = callback
        self._subscription = None
    
    async def _start(self) -> None:
        """Start the subscription (internal)."""
        async def _msg_handler(msg: Msg):
            self._callback(msg.subject, msg.data)
        
        self._subscription = await self._bus._nc.subscribe(self._topic, cb=_msg_handler)
    
    async def destroy(self) -> None:
        """Unsubscribe and cleanup."""
        if self._subscription:
            await self._subscription.unsubscribe()
            self._subscription = None
    
    @property
    def topic(self) -> str:
        """Get the topic name."""
        return self._topic


class Zetabus:
    """Main bus connection."""
    
    def __init__(self):
        self._nc: Optional[NATSClient] = None
        self._url: Optional[str] = None
    
    async def connect(self, url: str = "nats://localhost:4222") -> None:
        """
        Connect to NATS server.
        
        Args:
            url: NATS server URL
        """
        self._url = url
        self._nc = await nats.connect(url)
    
    async def disconnect(self) -> None:
        """Disconnect from NATS server."""
        if self._nc:
            await self._nc.drain()
            await self._nc.close()
            self._nc = None
    
    def create_publisher(self, topic: str) -> ZetabusPublisher:
        """
        Create a publisher for a specific topic.
        
        Args:
            topic: Topic name to publish to
            
        Returns:
            ZetabusPublisher instance
        """
        if not self._nc:
            raise RuntimeError("Bus not connected")
        return ZetabusPublisher(self, topic)
    
    async def create_subscriber(self, topic: str, callback: Callable[[str, bytes], None]) -> ZetabusSubscriber:
        """
        Create a subscriber for a specific topic with a callback.
        
        Args:
            topic: Topic name to subscribe to
            callback: Function to call when messages arrive (topic, data)
            
        Returns:
            ZetabusSubscriber instance
        """
        if not self._nc:
            raise RuntimeError("Bus not connected")
        
        subscriber = ZetabusSubscriber(self, topic, callback)
        await subscriber._start()
        return subscriber
    
    @property
    def url(self) -> Optional[str]:
        """Get the connection URL."""
        return self._url
    
    @property
    def is_connected(self) -> bool:
        """Check if connected to NATS."""
        return self._nc is not None and self._nc.is_connected


# Context manager support
class ZetabusContext:
    """Context manager for Zetabus."""
    
    def __init__(self, url: str = "nats://localhost:4222"):
        self.url = url
        self.bus = Zetabus()
    
    async def __aenter__(self) -> Zetabus:
        await self.bus.connect(self.url)
        return self.bus
    
    async def __aexit__(self, exc_type, exc_val, exc_tb):
        await self.bus.disconnect()
        return False
