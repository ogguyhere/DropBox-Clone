# Per-File Concurrency Control

## Design Choice: Per-File Mutex Locks

### Why Per-File?
- Allows concurrent operations on different files
- Serializes conflicting operations on same file
- Prevents race conditions like simultaneous DELETE + UPLOAD

### Implementation:
1. Each `file_t` struct has `pthread_mutex_t file_lock`
2. Workers acquire file lock before any operation
3. Lock released after disk I/O + metadata update complete

### Trade-offs:
**Pros:**
- Fine-grained locking (better concurrency)
- Simple to reason about
- No deadlocks (single lock acquisition)

**Cons:**
- More memory (one mutex per file)
- Need to handle lock cleanup on file deletion
```

---

## 📝 Your Deliverables Checklist
```
[ ] 1. Add per-file locking to metadata.h and metadata.c
[ ] 2. Integrate file locking in worker.c for all operations
[ ] 3. Ensure atomic metadata updates (or rollback on failure)
[ ] 4. Fix shutdown_flag type mismatch
[ ] 5. Fix memory cleanup (who frees task_t?)
[ ] 6. Create test_valgrind.sh script
[ ] 7. Create test_tsan.sh script
[ ] 8. Run both scripts and fix all reported issues
[ ] 9. Write concurrency_control.md report
[ ] 10. Update README with Phase 2 completion