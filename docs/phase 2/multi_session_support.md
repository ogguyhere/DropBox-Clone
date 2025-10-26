# Multi-Session Support Implementation Guide

## Overview
This implementation adds robust support for multiple concurrent clients, including multiple simultaneous sessions for the same user. The focus is on client-side session handling with proper synchronization and cleanup.


         ┌──────────────────┐
         │  Client Socket   │
         └────────┬─────────┘
                  │
                  ▼
       ┌──────────────────────┐
       │  Client Threadpool   │  ◄── Dequeues sockets
       │  1 thread per client │
       └──────────┬───────────┘
                  │
                  ▼
       ┌──────────────────────┐
       │  Command Parser &    │
       │  Authentication      │
       └──────────┬───────────┘
                  │
         ┌────────┴────────┐ 
         ▼                 ▼  
    ┌─────────────┐   ┌─────────────┐  
    │   Worker    │   │  Metadata   │ 
    │  Thread     │   │  (shared,   │ 
    │   Pool      │   │   mutex-    │ 
    │  Executes   │   │  protected) │ 
    │ file tasks  │   └─────────────┘ 
    └─────────────┘ 
    
   
**Step-by-step Flow:**
1. Client connects → assigned to a client thread.
2. Client thread handles login/signup and parses commands.
3. Parsed tasks (UPLOAD, DOWNLOAD, LIST, DELETE) are pushed to the **worker threadpool**.
4. Worker threads execute file operations concurrently.
5. Metadata is updated safely using mutex locks to avoid race conditions.
6. Client receives a response when the task is complete.


## Key Features

### 1. **Per-Connection Session Management**
- Each TCP connection gets its own `ClientSession` structure
- Sessions are independent and can belong to the same user
- Session tracking using pthread IDs as unique identifiers

### 2. **Concurrent Operations for Same User**
- Multiple sessions can log in as the same user simultaneously
- Per-user locking ensures thread-safe file operations
- Quota enforcement works correctly across all sessions

### 3. **Session Tracking**
- Active session counter for each user in metadata
- Global active session counter in client threadpool
- Session count displayed during login/logout

### 4. **Automatic Cleanup**
- Sessions automatically cleaned up on disconnect
- Session count decremented when connection closes
- No resource leaks even with abrupt disconnects



# dummy architecture Changes Client Threadpool Layer
```
┌─────────────────────────────────────────┐
│     Accept Thread (main.c)              │
│  Listens on port 8080, accepts clients  │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│  Client Threadpool (5 threads)          │
│  - Each thread handles one connection   │
│  - Creates ClientSession per connection │
│  - Manages command loop                 │
│  - Cleans up on disconnect              │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│  Command Handler (commands.c)           │
│  - Parses commands                      │
│  - Validates permissions                │
│  - Enqueues tasks to worker pool        │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│  Worker Threadpool (3 threads)          │
│  - Executes file I/O operations         │
│  - Per-user locking for safety          │
└─────────────────────────────────────────┘
```