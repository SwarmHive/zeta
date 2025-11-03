#!/usr/bin/env python3
"""Subscriber example for Zetabus Python library."""

import asyncio
import signal
from src.bus.python.zetabus import ZetabusContext


running = True


def signal_handler(sig, frame):
    global running
    running = False


def message_callback(topic: str, data: bytes):
    """Callback function for received messages."""
    print(f"Received message on topic '{topic}': {data.decode()}")


async def main():
    print("=== Python Subscriber Example ===\n")
    
    # Set up signal handler
    signal.signal(signal.SIGINT, signal_handler)
    
    async with ZetabusContext("nats://localhost:4222") as bus:
        subscriber = await bus.create_subscriber("example.topic", message_callback)
        
        print("Subscribed to topic 'example.topic'")
        print("Waiting for messages... (Press Ctrl+C to exit)\n")
        
        while running:
            await asyncio.sleep(1)
        
        print("\nShutting down...")
        await subscriber.destroy()


if __name__ == "__main__":
    asyncio.run(main())
