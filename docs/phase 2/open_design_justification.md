# Justification of Worker-to-Client Direct Communication

## Chosen Design

**Worker-to-Client Direct Communication**

Each task in the global Task Queue contains all necessary metadata, including the client’s socket descriptor. When a worker dequeues a task, it executes the requested operation (UPLOAD, DOWNLOAD, LIST, etc.) and immediately writes the result back to the client using the same socket. The client thread remains idle during this process, ready for further commands.

**Advantages:**
- Immediate delivery of responses to clients.
- Minimal complexity: no additional threads, queues, or synchronization needed.
- Efficient resource usage, low latency, and straightforward scalability.
- Matches the concurrency model and scale of the project.

---

## Alternative Approaches and Reasons for Rejection

### 1. Result Queue per Client
- Workers push results into a client-specific queue; client thread waits to send responses.
- **Issues:** Increased complexity, heavy synchronization, potential race conditions.
- **Rejected** because the added overhead outweighs benefits for the project’s moderate client load.

### 2. Global Result Dispatcher Thread
- Workers enqueue results into a global queue handled by a dispatcher thread.
- **Issues:** Introduces serialization, extra latency, and additional thread management.
- **Rejected** due to unnecessary complexity and reduced responsiveness.

---

## Justification Summary

| Criterion        | Evaluation of Chosen Design                                  |
|-----------------|--------------------------------------------------------------|
| **Complexity**    | Minimal; workers already have socket info, no extra data structures required. |
| **Fairness**      | FIFO ensured by the global Task Queue; no task starvation. |
| **Latency**       | Immediate response; no intermediary dispatch.              |
| **Resource Usage**| Low; avoids extra threads, buffers, or synchronization primitives. |
| **Scalability**   | Suitable for moderate client counts typical in this project. |
| **Maintainability** | Clean separation of concerns; easy to debug and extend. |

**Conclusion:**  
Direct communication from workers to clients is the simplest, most efficient, and maintainable solution for the project’s concurrency and workload, outperforming the alternatives in terms of latency, complexity, and resource usage.
