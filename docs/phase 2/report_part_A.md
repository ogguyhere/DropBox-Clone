# Phase 2: Concurrency, Correctness, Robustness – Member A Contributions

## Overview

This section documents the enhancements made in Phase 2, focusing on **no busy wait**, **client-side concurrency**, **worker-to-client communication**, and **robust shutdown handling**. The improvements allow multiple clients (including multiple sessions per user) to interact with the server safely and efficiently, while ensuring proper synchronization, resource cleanup, and consistent metadata updates.
 
## 1. Multi-Session Support

**Goal:** Enable multiple simultaneous clients, including multiple sessions per user.

**Implementation Highlights:**

* Each TCP connection is assigned a unique `ClientSession` structure.
* Client threads handle login/signup and command parsing independently.
* Sessions are independent but share global metadata and task queues safely.
* Per-user locks ensure concurrent operations (UPLOAD, DOWNLOAD, DELETE, LIST) are serialized correctly.
* Metadata updates (file lists, quotas, session counts) are thread-safe.
* Sessions are automatically cleaned up on disconnect, preventing resource leaks.

**Flow Diagram:**

```
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

```

**Key Features:**

* **Per-Connection Session Management:** Each client connection is independent.
* **Concurrent Operations for Same User:** Multiple sessions per user are allowed; per-user locking ensures safe concurrent access.
* **Session Tracking:** Active session counters maintained globally and per-user.
* **Automatic Cleanup:** Sessions are cleaned up on disconnect, even during abrupt client termination.
 

## 2. Worker-to-Client Result Delivery

**Design Choice:** Direct communication from workers to clients.

* Each task in the global **Task Queue** contains the client socket descriptor.
* Worker threads execute tasks and immediately write responses back to the client.
* Client threads remain idle while the worker handles task execution, reducing unnecessary waiting or polling.

**Advantages:**

* Immediate, low-latency response delivery.
* Minimal complexity: no extra threads or result queues required.
* Efficient resource usage and straightforward scalability.

**Rejected Alternatives:**

1. **Client-specific result queues:** Added unnecessary synchronization overhead.
2. **Global dispatcher thread:** Introduced serialization and increased latency.

**Evaluation Table:**

| Criterion       | Chosen Design Evaluation                                        |
| --------------- | --------------------------------------------------------------- |
| Complexity      | Minimal; no extra structures, workers already have socket info. |
| Fairness        | FIFO guaranteed via Task Queue; no task starvation.             |
| Latency         | Immediate responses; no intermediate dispatch.                  |
| Resource Usage  | Low; avoids extra threads and buffers.                          |
| Scalability     | Suitable for typical project client counts.                     |
| Maintainability | Clean separation of concerns; easy to debug and extend.         |

**Conclusion:**
Direct worker-to-client communication is simple, efficient, and ensures correct and timely responses under concurrent client load.
 

## 3. Robust Shutdown Handling

**Enhancements:**

* Server now closes the listening socket to unblock the accept thread.
* Client threads detect a `stop` flag and finish any queued tasks before exiting.
* Worker threads detect a `shutdown` flag and exit when the task queue is empty.
* All sockets and resources are cleaned up, allowing clean server restart without zombie threads or resource leaks.

**Benefits:**

* No hanging threads or blocked sockets on server shutdown.
* Safe for multiple concurrent client sessions.
* Prevents resource leaks and ensures consistent state.
 

## 4. Summary of Contributions

* **Bonus 1: No Busy Wait:** All threads wait on condition variables rather than looping.
* **Multi-Session Client Support:** Independent sessions per user; safe concurrent operations.
* **Worker-to-Client Result Delivery:** Direct, immediate responses to clients; simple, efficient design.
* **Robust Shutdown:** All threads terminate gracefully; sockets closed; memory freed.
* **Testing & Reliability:** Code verified for multiple concurrent clients and testing for other features also done.
 

This report captures **Member A’s contributions** in Phase 2, demonstrating a safe, scalable, and maintainable approach to client concurrency, response delivery, and server shutdown.
