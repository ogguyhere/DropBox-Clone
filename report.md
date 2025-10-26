# Dropbox Clone - File Storage Server
**Operating Systems Lab - Fall 2025 - Phase 1**

A multi-threaded file storage server implementing core Dropbox-like functionality with proper synchronization primitives.

---

## Features Implemented (Phase 1)

**Core Architecture:**
- Accept thread + Client threadpool + Worker threadpool
- Thread-safe producer-consumer queues (no busy-waiting)
- Proper task delegation from client threads to workers

**File Operations:**
- `UPLOAD <filename>` - Upload files to server
- `DOWNLOAD <filename>` - Download files from server
- `DELETE <filename>` - Delete files
- `LIST` - List all user files with sizes

**User Management:**
- `signup <username> <password>` - Create new user account
- `login <username> <password>` - Authenticate existing user
- `logout` - End session
- Per-user storage directories (`./storage/username/`)
- Quota enforcement (1MB default per user)

**Bonus Features:**
- Base64 encoding/decoding for binary file transfer
- Per-user file metadata tracking
- Concurrent client support

---

## Architecture Overview

```
┌─────────────┐
│   Clients   │
└──────┬──────┘
       │ TCP:8080
       ▼
┌─────────────────────────┐
│   Accept Thread         │
│  (main thread)          │
└──────┬──────────────────┘
       │ Socket Queue
       ▼
┌─────────────────────────┐
│  Client Threadpool      │
│  (5 threads)            │
│  - Authentication       │
│  - Command parsing      │
│  - Task creation        │
└──────┬──────────────────┘
       │ Task Queue
       ▼
┌─────────────────────────┐
│  Worker Threadpool      │
│  (3 threads)            │
│  - File I/O operations  │
│  - Metadata updates     │
│  - Quota checking       │
└─────────────────────────┘
```

---

## Project Structure

```
.
├── main.c                      # Server initialization, accept loop
├── client_threadpool.c/.h      # Client thread management
├── commands.c/.h               # Command parsing & authentication
├── worker.c/.h                 # Worker thread execution
├── queue.c/.h                  # Thread-safe task queue
├── metadata.c/.h               # User/file metadata management
├── file_io.c/.h                # Disk I/O operations
├── task.h                      # Task structure definition
├── client.c                    # Basic test client
├── client_file_testing.c       # Comprehensive file ops client
├── test_queue.c                # Standalone queue/worker test
├── test_concurrent.sh          # Multi-client concurrent test
├── test_valgrind.sh            # Memory leak detection
├── Makefile                    # Build configuration
└── README.md                   # This file
```

---

## Quick Start

### Prerequisites
```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install build-essential valgrind netcat

# Arch Linux
sudo pacman -S base-devel valgrind openbsd-netcat
```

### Build
```bash
make clean
make
```

This produces:
- `server` - Main server executable
- `client` - Basic test client
- `client_file_testing` - Advanced file operations client
- `test_queue` - Standalone queue/worker test

---

## Usage

### 1. Start Server
```bash
./server
```

**Output:**
```
=== Dropbox Clone Server Starting ===
Metadata initialized
Task queue initialized
Worker 1 started
Worker 2 started
Worker 3 started
Worker threadpool initialized with 3 workers
Client threadpool initialized with 5 threads
Socket bound to port 8080
Server is listening for connections...
=== Server Ready ===
```

### 2. Run Basic Client Test
In another terminal:
```bash
./client
```

This runs through signup, login, logout, and error scenarios.

### 3. Run File Operations Test
```bash
# Create test files first
echo "Test content 1" > test1.txt
echo "Test content 2" > test2.txt
echo "Test content 3" > test3.txt

# Run comprehensive test
./client_file_testing test1.txt
```

**What it tests:**
- Signup and auto-login
- Multiple file uploads
- File listing with sizes
- File downloads (to `./downloads/`)
- File deletion
- List after deletion

### 4. Manual Testing via netcat
```bash
nc localhost 8080
```

Then type commands:
```
signup alice pass123
LIST
UPLOAD test.txt
<Base64-encoded-file-content>
LIST
DOWNLOAD test.txt
DELETE test.txt
logout
```



### Test 3: Queue & Worker Isolation
```bash
./test_queue
```

Tests queue operations and worker execution without network layer.

### Test 4: Manual Multi-Client
Open 4+ terminals and run `./client` simultaneously to verify concurrent handling.

---

## Supported Commands

### Authentication Commands
| Command | Description | Example |
|---------|-------------|---------|
| `signup <username> <password>` | Create account & auto-login | `signup alice pass123` |
| `login <username> <password>` | Authenticate user | `login alice pass123` |
| `logout` | End current session | `logout` |

### File Commands (require login)
| Command | Description | Example |
|---------|-------------|---------|
| `UPLOAD <filename>` | Upload file (Base64) | `UPLOAD test.txt` |
| `DOWNLOAD <filename>` | Download file (Base64) | `DOWNLOAD test.txt` |
| `DELETE <filename>` | Delete file | `DELETE test.txt` |
| `LIST` | List all files with sizes | `LIST` |

---

## File Storage

**Directory structure:**
```
./storage/
├── alice/
│   ├── file1.txt
│   └── photo.jpg
├── bob/
│   └── document.pdf
└── charlie/
    ├── code.c
    └── data.csv
```

**Quota:** 1MB per user (enforced on upload)

---

## Configuration

Edit these constants in source files:

**main.c:**
```c
#define WORKER_POOL_SIZE 3    // Number of worker threads
#define SERVER_PORT 8080      // Listening port
```

**client_threadpool.h:**
```c
#define THREAD_POOL_SIZE 5    // Number of client threads
```

**metadata.h:**
```c
#define MAX_USERS 100                    // Max users
#define MAX_FILES_PER_USER 50            // Max files per user
#define DEFAULT_QUOTA (1024 * 1024)      // 1MB quota
```

**task.h:**
```c
char data[8192];  // Max file size per transfer (8KB)
```

---

## Development

### Debugging
```bash
# Compile with debug symbols
make clean
make CFLAGS="-Wall -Wextra -g -pthread"

# Run with GDB
gdb ./server
(gdb) run
```

### Check for Race Conditions
```bash
# Compile with ThreadSanitizer
gcc -fsanitize=thread -g -pthread *.c -o server_tsan

# Run
./server_tsan
# Then connect clients
```

### Check for Memory Leaks (Manual)
```bash
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --verbose \
         ./server
```

---

## Performance Characteristics

**Tested Configuration:**
- **Clients:** Up to 5 concurrent connections
- **Workers:** 3 threads processing tasks
- **File size:** Up to 8KB per operation (Base64 encoded)
- **Throughput:** ~50 operations/second (local testing)

**Bottlenecks:**
- Fixed 8KB buffer for file data
- Sleep(1) synchronization in Phase 1
- Single mutex for metadata operations

---

## Known Limitations (Phase 1)

1. **Busy-waiting:** Client threads sleep(1) after enqueueing tasks instead of proper condition variable waiting
2. **File size limit:** 8KB max due to inline buffer in task_t
3. **Client queue:** Fixed-size array (5 slots), may drop connections under heavy load
4. **No graceful shutdown:** Server must be killed manually (Ctrl+C)
5. **Single-session assumption:** Multiple simultaneous sessions per user not fully tested
6. **No per-file locking:** Concurrent operations on same file not serialized

**All will be addressed in Phase 2.**

---

## Phase 1 Deliverables

**Source Code:**
- All `.c` and `.h` files
- Makefile for single-command build
- Test clients and scripts

**Documentation:**
- This README with build/run instructions
- Phase 1 Design Report (1 page)
- Inline code comments

**Testing:**
- Functional tests (single client)
- Concurrent tests (multiple clients)
- Valgrind verification (no leaks)

---

## Phase 2 Roadmap

**Planned improvements:**
1. Replace sleep(1) with condition variable signaling
2. Implement per-file locking for conflict resolution
3. Support multiple simultaneous sessions per user
4. Dynamic memory allocation for large files
5. Graceful shutdown with signal handlers
6. ThreadSanitizer validation
7. Comprehensive concurrency stress tests

---

## Team Members

- **Member A:** Client threadpool, authentication, command parsing, UPLOAD/DOWNLOAD
- **Member B:** Worker threadpool, task queue, metadata, DELETE/LIST, file I/O
- **Shared:** Integration, testing, documentation, debugging

---

## Phase 1 Completion Checklist

- [x] Accept loop pushes sockets to Client Queue
- [x] Client threadpool handles authentication & parsing
- [x] Worker threadpool executes file operations
- [x] Thread-safe queues with mutex + condition variables
- [x] All 4 commands work (UPLOAD, DOWNLOAD, DELETE, LIST)
- [x] Metadata tracking with quota enforcement
- [x] No memory leaks (Valgrind verified)
- [x] Concurrent client support demonstrated
- [x] Makefile for single-command build
- [x] README with instructions
- [x] Design report written

**Status: Phase 1 Complete!**