### **Step-by-Step Flow:**


``` text
1. Client connects to server (port 8080)
   └─> Accept thread calls accept()
   └─> Pushes socket to client_pool->socket_queue

2. Client thread wakes up
   └─> Dequeues socket
   └─> Reads command: "UPLOAD test.txt"
   
3. Commands.c::handle_upload() runs
   ├─> Checks authentication 
   ├─> Sends "READY_TO_RECEIVE\n"
   ├─> Reads base64 encoded data
   ├─> Creates task_t with:
   │   ├─> cmd = UPLOAD
   │   ├─> username = "alice"
   │   ├─> filename = "test.txt"
   │   ├─> data = "YmFzZTY0IGRhdGE..." (encoded)
   │   └─> sock_fd = socket descriptor
   └─> queue_enqueue(task_queue, task) 
   
4. Worker thread wakes up (was blocked on queue_dequeue)
   ├─> Dequeues task from queue
   ├─> Decodes base64 → raw bytes
   ├─> Checks quota with metadata_check_quota()
   ├─> Saves file: ./storage/alice/test.txt
   ├─> Updates metadata: metadata_add_file()
   └─> Writes response to sock_fd: "UPLOAD_SUCCESS\n" 

5. Client receives response
   └─> Sees "UPLOAD_SUCCESS"

``` 