# DropBox Clone - Project Report

## 2. Design

### 2.1 Project Overview

The DropBox Clone is a command-line client-server file storage system that demonstrates operating system concepts of synchronization and threading. The system allows multiple clients to connect concurrently, authenticate, and perform file operations (upload, download, list, delete) through a worker thread pool architecture.

**Key Objectives:**
* Implement thread-safe file operations with proper synchronization
* Support multiple concurrent client sessions (including multiple sessions per user)
* Use worker thread pool pattern for scalable task processing
* Ensure no busy-waiting through condition variables
* Provide robust shutdown handling with proper resource cleanup

### 2.2 System Architecture

#### 2.2.1 High-Level Architecture

```
┌─────────────────────────────────────────────────────────┐
│                   Client Application                    │
│                  (Command-Line Interface)               │
│                                                         │
│  • Connects to server via TCP socket                    │
│  • Sends commands (UPLOAD, DOWNLOAD, LIST, DELETE)      │
│  • Receives responses from workers                      │
└────────────────────┬────────────────────────────────────┘
                     │
                     │ TCP Connection
                     │
┌────────────────────┴───────────────────────────────────┐
│                   Server Application                   │
│                                                        │
│  ┌────────────────────────────────────────────────┐    │
│  │          Accept Thread (main.c)                │    │
│  │     Listens on port 8080, accepts clients      │    │
│  └──────────────────┬─────────────────────────────┘    │
│                     │                                  │
│                     ▼                                  │
│  ┌─────────────────────────────────────────────────┐   │
│  │     Client Threadpool (5 threads)               │   │
│  │  • 1 thread per active client connection        │   │
│  │  • Creates ClientSession per connection         │   │
│  │  • Handles authentication (login/signup)        │   │
│  │  • Parses commands and enqueues to worker pool  │   │
│  │  • Cleans up session on disconnect              │   │
│  └──────────────────┬──────────────────────────────┘   │
│                     │                                  │
│                     ▼                                  │
│  ┌─────────────────────────────────────────────────┐   │
│  │        Command Handler (commands.c)             │   │
│  │  • Validates user permissions                   │   │
│  │  • Prepares task with metadata                  │   │
│  │  • Enqueues to global Task Queue                │   │
│  └──────────────────┬──────────────────────────────┘   │
│                     │                                  │
│                     ▼                                  │
│  ┌─────────────────────────────────────────────────┐   │
│  │          Global Task Queue                      │   │
│  │  • FIFO queue of client requests                │   │
│  │  • Protected by mutex + condition variable      │   │
│  │  • Contains socket descriptor + task metadata   │   │
│  └──────────────────┬──────────────────────────────┘   │
│                     │                                  │
│                     ▼                                  │
│  ┌─────────────────────────────────────────────────┐   │
│  │      Worker Threadpool (3 threads)              │   │
│  │  • Dequeues tasks from Task Queue               │   │
│  │  • Executes file I/O operations                 │   │
│  │  • Uses per-user locking for thread safety      │   │
│  │  • Writes response directly to client socket    │   │
│  └──────────────────┬──────────────────────────────┘   │
│                     │                                  │
│                     ▼                                  │
│  ┌─────────────────────────────────────────────────┐   │
│  │     Metadata & Storage (metadata.c)             │   │
│  │  • User accounts, file lists, quotas            │   │
│  │  • Protected by mutexes                         │   │
│  │  • Per-user locks for file operations           │   │
│  │  • Session tracking (active session counts)     │   │
│  └─────────────────────────────────────────────────┘   │
│                                                        │
│                 File System Storage                    │
└────────────────────────────────────────────────────────┘
```

#### 2.2.2 Component Description

**Client Application:**
* Command-line interface (no GUI)
* Sends text-based commands to server
* Receives and displays responses

**Server Components:**

1. **Accept Thread (main.c)**
   * Listens for incoming TCP connections on port 8080
   * Accepts new client connections
   * Passes socket descriptor to client threadpool

2. **Client Threadpool**
   * One thread handles one client connection at a time
   * Creates `ClientSession` structure per connection
   * Handles authentication (login/signup)
   * Parses incoming commands
   * Enqueues validated tasks to worker pool
   * Manages command loop until client disconnects
   * Cleans up session resources on disconnect

3. **Command Handler (commands.c)**
   * Validates user permissions for requested operations
   * Prepares task structure with all necessary metadata
   * Enqueues tasks to global Task Queue for workers

4. **Global Task Queue**
   * FIFO queue ensuring fairness
   * Each task contains: client socket, operation type, file metadata
   * Protected by mutex and condition variable
   * Enables efficient worker coordination without busy-waiting

5. **Worker Threadpool (3 threads)**
   * Dequeues tasks from global Task Queue
   * Executes file operations (UPLOAD, DOWNLOAD, LIST, DELETE)
   * Uses per-user locking to prevent race conditions
   * Writes results directly back to client via socket descriptor
   * Returns to waiting state when queue is empty

6. **Metadata & Storage (metadata.c)**
   * Manages user accounts and authentication
   * Tracks file lists per user
   * Enforces storage quotas
   * Maintains session counters (global and per-user)
   * All accesses protected by mutexes

### 2.3 Threading Model

#### 2.3.1 Thread Architecture

```
Main Thread
    │
    ├─── Accept Thread
    │       │
    │       └─── Accepts connections → passes to Client Threadpool
    │
    ├─── Client Threadpool 
    │       ├─── Client Thread 1 (handles 1 connection)
    │       ├─── Client Thread 2 (handles 1 connection)
    │       ├─── Client Thread 3 (handles 1 connection)
    │       ├─── Client Thread 4 (handles 1 connection)
    │       └─── Client Thread 5 (handles 1 connection)
    │               │
    │               └─── Enqueues to Task Queue
    │
    └─── Worker Threadpool 
            ├─── Worker Thread 1 (dequeues & executes tasks)
            ├─── Worker Thread 2 (dequeues & executes tasks)
            └─── Worker Thread 3 (dequeues & executes tasks)
```

#### 2.3.2 Thread Responsibilities

| Thread Type    | Responsibility                                              |
|----------------|-------------------------------------------------------------|
| Accept Thread  |Accept incoming TCP connections                              |
| Client Threads | Handle client authentication, parse commands, enqueue tasks |    
| Worker Threads | Execute file operations, respond to clients                 |
| Main Thread    | Initialize server, manage shutdown                          |
 

### 2.4 Multi-Session Support

#### 2.4.1 Session Management

**Goal:** Enable multiple simultaneous clients, including multiple sessions for the same user.

**Implementation:**
* Each TCP connection is assigned a unique `ClientSession` structure
* Sessions are identified by pthread IDs
* Multiple sessions can log in as the same user concurrently
* Sessions are independent but share global metadata safely

**Session Flow:**
```
Client Connects
      │
      ▼
Client Thread Assigned
      │
      ▼
ClientSession Created
      │
      ▼
Authentication (login/signup)
      │
      ▼
Session Added to Active List
      │
      ▼
Command Loop (parse → enqueue → wait)
      │
      ▼
Client Disconnects
      │
      ▼
Session Cleaned Up
      │
      ▼
Session Count Decremented
```

#### 2.4.2 Key Features

1. **Per-Connection Session Management**
   * Each TCP connection gets its own `ClientSession` structure
   * Sessions are independent and can belong to the same user
   * Session tracking using pthread IDs as unique identifiers

2. **Concurrent Operations for Same User**
   * Multiple sessions can log in as the same user simultaneously
   * Per-user locking ensures thread-safe file operations
   * Quota enforcement works correctly across all sessions

3. **Session Tracking**
   * Active session counter for each user in metadata
   * Global active session counter in client threadpool
   * Session count displayed during login/logout

4. **Automatic Cleanup**
   * Sessions automatically cleaned up on disconnect
   * Session count decremented when connection closes
   * No resource leaks even with abrupt disconnects

### 2.5 Task Queue Design

#### 2.5.1 Queue Structure

**Purpose:** Central coordination between client threads and worker threads

**Task Contents:**
* Client socket descriptor (for direct response)
* Operation type (UPLOAD, DOWNLOAD, LIST, DELETE)
* User identifier
* File metadata (filename, size, path)
* Request parameters

**Queue Characteristics:**
* **FIFO ordering:** Ensures fairness, prevents task starvation
* **Thread-safe:** Protected by mutex and condition variable
* **Bounded capacity:** Prevents memory overflow under high load
* **Efficient waiting:** Workers use condition variables (no busy-wait)

#### 2.5.2 Enqueue/Dequeue Operations

**Enqueue (by Client Threads):**
```c
pthread_mutex_lock(&queue_mutex);
add_task_to_queue(task);
pthread_cond_signal(&queue_cond);  // Wake up a worker
pthread_mutex_unlock(&queue_mutex);
```

**Dequeue (by Worker Threads):**
```c
pthread_mutex_lock(&queue_mutex);
while (queue_is_empty(&task_queue) && !shutdown_flag) {
    pthread_cond_wait(&queue_cond, &queue_mutex);  // No busy-wait
}
if (!shutdown_flag) {
    task = dequeue_task(&task_queue);
}
pthread_mutex_unlock(&queue_mutex);
```

### 2.6 Synchronization Mechanisms

#### 2.6.1 Critical Sections

The following resources require synchronization:

1. **Global Task Queue**
   * **Lock:** Mutex + condition variable
   * **Protects:** Enqueue/dequeue operations
   * **Purpose:** Enables workers to wait efficiently when queue is empty

2. **User Metadata**
   * **Lock:** Per-user mutex
   * **Protects:** File lists, quotas, session counts
   * **Purpose:** Prevents race conditions during concurrent operations for same user

3. **File Operations**
   * **Lock:** Per-user lock (within metadata)
   * **Protects:** File reads/writes for a specific user
   * **Purpose:** Serializes UPLOAD/DOWNLOAD/DELETE operations per user

4. **Session Management**
   * **Lock:** Client threadpool mutex
   * **Protects:** Active session list, global session counter
   * **Purpose:** Thread-safe session tracking

#### 2.6.2 Synchronization Primitives

* **Mutexes (`pthread_mutex_t`):** Exclusive access to shared resources
* **Condition Variables (`pthread_cond_t`):** Efficient thread coordination, eliminates busy-waiting
* **Per-User Locks:** Fine-grained locking to allow concurrent operations for different users

#### 2.6.3 No Busy-Wait Implementation

**Bonus 1 Achievement:** All threads wait on condition variables rather than polling loops.

**Examples:**
* Workers wait on `queue_cond` when task queue is empty
* Accept thread blocks on `accept()` system call
* Client threads block on `recv()` waiting for commands

**Benefits:**
* Reduced CPU usage
* Improved system responsiveness
* Better power efficiency

#### 2.6.4 Deadlock Prevention

Strategies implemented:
* **Lock ordering:** Consistent acquisition order (global queue lock → per-user lock)
* **Timeout mechanisms:** Avoid indefinite waiting
* **Minimal critical sections:** Release locks quickly
* **No nested locks:** Avoid holding multiple locks simultaneously where possible

### 2.7 Data Flow

#### 2.7.1 Complete Request Flow

```
Client                  Client Thread           Task Queue          Worker Thread
  │                           │                       │                    │
  │─── UPLOAD file.txt ──────▶│                       │                    │
  │                           │                       │                    │
  │                           │ Validate user         │                    │
  │                           │ Prepare task          │                    │
  │                           │                       │                    │
  │                           │─── Enqueue task ─────▶│                    │
  │                           │    (socket + metadata)│                    │
  │                           │                       │                    │
  │                           │ [Waits for response]  │                    │
  │                           │                       │                    │
  │                           │                       │◀─── Dequeue ───────│
  │                           │                       │                    │
  │                           │                       │                    │ Execute upload
  │                           │                       │                    │ Lock user
  │                           │                       │                    │ Write file
  │                           │                       │                    │ Update metadata
  │                           │                       │                    │ Unlock user
  │                           │                       │                    │
  │◀──────────────────── SUCCESS (direct write via socket) ───────────────│
  │                           │                       │                    │
```

#### 2.7.2 File Upload Sequence

```
Client                          Server
  │                              │
  │─────── UPLOAD request ───────▶│ [Client Thread validates]
  │        (filename, size)      │
  │                              │
  │                              │ [Task enqueued to Queue]
  │                              │
  │          [Worker dequeues]   │
  │                              │
  │◀────── Ready to receive ─────│ [Worker acquired per-user lock]
  │                              │
  │─────── File data chunks ─────▶│ [Worker writes to disk]
  │                              │
  │◀────── SUCCESS ──────────────│ [Worker updates metadata, releases lock]
  │                              │
```

#### 2.7.3 File Download Sequence

```
Client                          Server
  │                              │
  │─────── DOWNLOAD request ─────▶│ [Client Thread validates]
  │        (filename)            │
  │                              │
  │                              │ [Task enqueued to Queue]
  │                              │
  │          [Worker dequeues]   │
  │                              │
  │◀────── File metadata ────────│ [Worker acquired per-user lock]
  │        (size, checksum)      │
  │                              │
  │─────── Ready ────────────────▶│
  │                              │
  │◀────── File data chunks ─────│ [Worker streams file, releases lock]
  │                              │
```

### 2.8 Supported Operations

| Command | Description | Worker Thread Action |
|---------|-------------|---------------------|
| **UPLOAD** | Upload file to server | Receive file data, write to storage, update metadata |
| **DOWNLOAD** | Download file from server | Read file from storage, send to client |
| **LIST** | List user's files | Read metadata, format list, send to client |
| **DELETE** | Delete file from server | Remove file from storage, update metadata |

All operations are processed through the worker thread pool and enforce per-user locking for thread safety.

### 2.9 Concurrency Control

#### 2.9.1 Race Condition Prevention

**Scenario 1: Multiple workers accessing task queue**
```c
// Protected by queue mutex + condition variable
pthread_mutex_lock(&queue_mutex);
while (queue_is_empty(&task_queue) && !shutdown_flag) {
    pthread_cond_wait(&queue_cond, &queue_mutex);
}
task = dequeue_task(&task_queue);
pthread_mutex_unlock(&queue_mutex);
```

**Scenario 2: Concurrent file operations for same user**
```c
// Protected by per-user lock
lock_user(task->username);
execute_file_operation(task);  // UPLOAD/DOWNLOAD/DELETE
update_user_metadata(task->username);
unlock_user(task->username);
```

**Scenario 3: Session tracking**
```c
// Protected by threadpool mutex
pthread_mutex_lock(&threadpool_mutex);
active_sessions++;
user->session_count++;
pthread_mutex_unlock(&threadpool_mutex);
```

#### 2.9.2 Producer-Consumer Pattern

The system implements classic producer-consumer:
* **Producers:** Client threads (enqueue tasks)
* **Consumers:** Worker threads (dequeue and process tasks)
* **Shared Buffer:** Global Task Queue
* **Synchronization:** Mutex + condition variable

**Benefits:**
* Decouples client handling from task execution
* Workers can process tasks while clients wait for new commands
* FIFO fairness prevents starvation

### 2.10 Robust Shutdown Handling

#### 2.10.1 Shutdown Sequence

```
SIGINT received
      │
      ▼
Set shutdown_flag = true
      │
      ▼
Close listening socket → unblocks accept thread
      │
      ▼
Broadcast queue_cond → wakes all workers
      │
      ▼
Workers finish current task then exit
      │
      ▼
Client threads detect stop flag, finish gracefully
      │
      ▼
All threads joined
      │
      ▼
Cleanup resources (free memory, close sockets)
      │
      ▼
Server exits cleanly
```

#### 2.10.2 Shutdown Features

* **No hanging threads:** All threads exit gracefully
* **Socket cleanup:** Listening socket closed to unblock accept
* **Task completion:** Workers finish in-progress tasks before exiting
* **Resource cleanup:** All allocated memory freed
* **No zombie threads:** All threads properly joined
* **Restartable:** Server can be restarted without issues

**Benefits:**
* Safe for multiple concurrent client sessions
* Prevents resource leaks
* No orphaned connections
* Consistent state on restart

### 2.11 Performance Considerations

#### 2.11.1 Scalability Factors

| Factor | Current Design | Impact |
|--------|---------------|--------|
| **Max concurrent clients** | 5 (client threadpool) | Limited by threadpool size |
| **Max concurrent tasks** | 3 (worker threadpool) | Limited by worker count |
| **Per-user operations** | Serialized | Prevents race conditions |
| **Different-user operations** | Parallel | Workers can process simultaneously |
| **Queue capacity** | Bounded | Prevents memory overflow |

#### 2.11.2 Optimization Techniques

* **Thread pooling:** Reuse threads (no creation/destruction overhead)
* **Condition variables:** No busy-waiting, efficient CPU usage
* **Per-user locking:** Fine-grained locks allow parallel operations for different users
* **Direct worker-to-client communication:** No intermediate queues or dispatch overhead
* **Chunked file transfer:** Supports large files without excessive memory usage

### 2.12 Error Handling

#### 2.12.1 Error Categories

1. **Network Errors**
   * Connection timeout
   * Connection loss during transfer
   * Socket errors

2. **File System Errors**
   * File not found
   * Insufficient disk space
   * Permission denied
   * Quota exceeded

3. **Concurrency Errors**
   * Max clients reached (client threadpool full)
   * Queue overflow
   * Lock acquisition failures

4. **Authentication Errors**
   * Invalid credentials
   * User already exists (signup)
   * Session limit reached

#### 2.12.2 Recovery Mechanisms

* Appropriate error codes sent to clients
* Clean resource cleanup on errors
* Session cleanup on abrupt disconnect
* Graceful handling of partial operations
* Logging for debugging

### 2.13 Code Structure

```
project/
├── Makefile
├── README.md
├── src/
│   ├── server/
│   │   ├── main.c              # Accept thread, initialization
│   │   ├── commands.c          # Command parsing, validation
│   │   ├── metadata.c          # User accounts, file lists
│   │   ├── client_thread.c     # Client threadpool
│   │   ├── worker_thread.c     # Worker threadpool
│   │   └── task_queue.c        # Global task queue
│   └── client/
│       └── client.c            # Client application
└── docs/
    ├── design_report.md        # This document
    └── phase2_report.md        # Phase 2 contributions
```

### 2.14 Testing & Verification

#### 2.14.1 Concurrency Tests
* Multiple concurrent client connections (up to 5)
* Multiple sessions for same user
* Simultaneous upload/download operations
* Worker thread race condition verification
* Task queue stress testing

#### 2.14.2 Memory Safety Tests
* Valgrind leak detection (Section 4)
* Buffer overflow prevention
* Proper resource cleanup on shutdown
* Session cleanup on disconnect

#### 2.14.3 Thread Safety Tests
* TSAN (ThreadSanitizer) analysis (Section 5)
* Deadlock detection
* Data race identification
* Lock ordering verification
