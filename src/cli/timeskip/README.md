# timeskip - Zetabus Recording and Playback

`timeskip` is a tool for recording and replaying Zetabus messages, similar to ROS's `rosbag`.

## Features

- ✅ Record Zetabus messages to `.zet` files
- ✅ **Pausable recording** - pause/resume without stopping
- ✅ **File size tracking** - see how much data you're recording
- ✅ Two-threaded architecture (lock-free circular buffer)
- ✅ High-frequency message support
- ✅ Configurable NATS server (env var or CLI arg)
- ✅ Real-time statistics during recording
- ✅ **Interactive playback with keyboard controls**
- ✅ **Speed control (0.5x to 10x, or MAX speed)**
- ✅ **Seeking (forward/backward by 10 messages)**
- ✅ **Pause/Resume**
- ✅ **Skip to next message**

## Usage

### Recording

Record all messages on a topic:

```bash
timeskip record <topic> -o output.zet
```

Examples:

```bash
# Record sensor data with default NATS server
timeskip record "sensor.temperature"

# Record with custom output file
timeskip record "sensor.*" -o my_recording.zet

# Specify NATS server
timeskip record "sensor.*" -s nats://192.168.1.100:4222

# Use environment variable for NATS server
export NATS_URL=nats://192.168.1.100:4222
timeskip record "sensor.*"
```

### Playback

Play back a recorded `.zet` file:

```bash
timeskip play recording.zet
```

#### Playback Options

```bash
# Specify NATS server
timeskip play recording.zet -s nats://192.168.1.100:4222

# Control playback speed
timeskip play recording.zet --speed 2.0    # 2x speed
timeskip play recording.zet --speed 0.5    # Half speed
timeskip play recording.zet --speed 0      # Maximum speed (no delays)

# Disable interactive controls
timeskip play recording.zet --no-interactive
```

#### Interactive Controls (Default)

When playing back in interactive mode, you can control playback with your keyboard:

| Key | Action |
|-----|--------|
| `←` | Seek backward (10 messages, works while paused) |
| `→` | Seek forward (10 messages, works while paused) |
| `↑` | Speed up (+0.5x, max 10x, works while paused) |
| `↓` | Slow down (-0.5x, min 0.5x, works while paused) |
| `n` | Skip to next message |
| `p` or `Space` | Pause/Resume |
| `q` | Quit playback |

**Visual Progress Bar** (auto-adjusts to terminal width):
```
▶️ PLAYBACK [=============|                    ] 12.3/27.2s 1.5x 1234/2730
⏸️ PLAYBACK [=============|                    ] 12.3/27.2s 1.5x 1234/2730  (paused)
```

The progress bar shows:
- Play/Pause icon (▶️ or ⏸️)
- Visual progress with position marker (`|`)
- Current position / total duration
- Playback speed
- Message number / total messages

**You can seek while paused!** Use arrow keys to navigate through the recording without auto-playing.

### Recording Statistics

While recording, you'll see real-time statistics:

```
� RECORDING | Rcv: 1523 | Written: 1520 | Dropped: 0 | Size: 2.34 MB
```

**Interactive Controls During Recording**:
- Press `p` to pause/resume recording
- Press `Ctrl+C` to stop and save

When paused:
```
⏸️  PAUSED   | Rcv: 1523 | Written: 1520 | Dropped: 3 | Size: 2.34 MB
```

**Note**: Messages received while paused are dropped (not buffered). Use this to skip unwanted data without stopping the recording.

Press `Ctrl+C` to stop and see final statistics:

```
📈 Recording Statistics:
  Messages received: 1523
  Messages written:  1523
  Messages dropped:  0
  File size:         2.34 MB
```

## .zet File Format

The `.zet` format is a binary format optimized for robotics:

### Header (32 bytes)
- Magic: "ZET\0" (4 bytes)
- Version: uint32 (4 bytes)
- Start timestamp: uint64 nanoseconds (8 bytes)
- Reserved: (16 bytes)

### Message Records (variable)
- Timestamp sent: uint64 ns (8 bytes)
- Timestamp received: uint64 ns (8 bytes)
- Topic length: uint16 (2 bytes)
- Payload size: uint32 (4 bytes)
- Topic: variable (null-terminated string)
- Payload: variable (raw bytes)

## Architecture

### Two-Threaded Design

**Thread 1: Receiver (NATS thread)**
- Receives messages via Zetabus subscription
- Pushes to lock-free circular buffer
- Never blocks on disk I/O

**Thread 2: Writer (dedicated thread)**
- Drains circular buffer in batches
- Writes to disk efficiently
- Flushes regularly to prevent data loss

### Benefits
- ✅ No dropped messages due to disk I/O
- ✅ Handles high-frequency data (100Hz+)
- ✅ Works on slow storage (SD cards, etc.)
- ✅ Batch writes for efficiency

## Configuration

### NATS Server Priority

1. `-s` / `--server` CLI argument
2. `NATS_URL` environment variable
3. Default: `nats://localhost:4222`

### Buffer Size

Default: 10,000 messages in circular buffer

- Adjust in code if needed for extremely high-frequency topics
- Monitor "Dropped" statistic during recording

## Development

### Building

```bash
bazel build //src/cli/timeskip
```

### Running

```bash
# Record
bazel run //src/cli/timeskip -- record "test.*"

# Playback
bazel run //src/cli/timeskip -- play recording.zet
```

## Quick Start Example

```bash
# Terminal 1: Start NATS server (if not running)
docker run -p 4222:4222 nats:latest

# Terminal 2: Record some data
bazel run //src/cli/timeskip -- record "sensor.*" -o test.zet

# Terminal 3: Publish some test data (while recording)
# ... use your Zetabus publisher ...

# Press Ctrl+C in Terminal 2 to stop recording

# Terminal 2: Play back the recording
bazel run //src/cli/timeskip -- play test.zet

# Use arrow keys to seek, space to pause, q to quit
```

## Roadmap

- [x] Recording functionality
- [x] Playback functionality  
- [x] Speed control (0.5x to 10x, MAX)
- [x] Interactive seeking (keyboard arrows)
- [x] Pause/Resume
- [x] Skip to next message
- [ ] Topic filtering during playback
- [ ] File info command (`timeskip info file.zet`)
- [ ] Time-based seeking (jump to timestamp)
- [ ] Index generation for large files
- [ ] Publisher timestamp support (requires Zetabus protocol update)
- [ ] Multi-file playback (concatenate recordings)

## Comparison to rosbag

| Feature | timeskip | rosbag2 |
|---------|----------|---------|
| Setup | Zero config | Requires ROS2 environment |
| File format | Binary (.zet) | SQLite / MCAP |
| Performance | Lock-free circular buffer | SQLite transactions |
| Playback speed | ✅ 0.5x to 10x + MAX | ✅ Supported |
| Interactive seeking | ✅ Arrow keys | ❌ Not supported |
| Pause/Resume | ✅ Space bar | ❌ Not supported |
| Message filtering | Coming soon | ✅ Supported |
| Ease of use | ✅ Simple CLI | Complex plugin system |

## Architecture Notes

### Why Two Threads?

Single-threaded approach would block on disk I/O, causing message drops on high-frequency topics. The circular buffer decouples message reception from disk writes.

### Why Lock-Free Buffer?

Mutexes would add latency in the NATS callback. Lock-free atomics ensure minimal overhead in the hot path.

### Why Not Async I/O?

Simpler implementation for MVP. Batch writes + dedicated thread is sufficient for most robotics workloads.
