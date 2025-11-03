#!/usr/bin/env python3
"""Publisher example for Zetabus Python library."""

import asyncio
from src.bus.python.zetabus import ZetabusContext


async def main():
    print("=== Python Publisher Example ===\n")
    
    async with ZetabusContext("nats://localhost:4222") as bus:
        publisher = bus.create_publisher("example.topic")
        
        print("Publishing messages to topic 'example.topic'...")
        for i in range(5):
            message = f"Hello, Zetabus! Message {i + 1}"
            await publisher.publish(message.encode())
            print(f"Published: {message}")
        
        print("\nAll messages published successfully!")


if __name__ == "__main__":
    asyncio.run(main())
