import socket
import threading
import base64
import time

SERVER_HOST = '127.0.0.1'
SERVER_PORT = 8080

USERNAME = "user"
PASSWORD = "pass"

# tiny content to upload
FILE_CONTENT = b"A"
FILENAME = "file.txt"

def client_task(id):
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect((SERVER_HOST, SERVER_PORT))
        print(f"[Client {id}] Connected")

        # signup (ignore errors if already exists)
        s.sendall(f"signup {USERNAME} {PASSWORD}\n".encode())
        time.sleep(0.1)
        resp = s.recv(1024).decode()
        print(f"[Client {id}] signup: {resp.strip()}")

        # login
        s.sendall(f"login {USERNAME} {PASSWORD}\n".encode())
        time.sleep(0.1)
        resp = s.recv(1024).decode()
        print(f"[Client {id}] login: {resp.strip()}")

        # LIST
        s.sendall("LIST\n".encode())
        time.sleep(0.1)
        resp = s.recv(1024).decode()
        print(f"[Client {id}] LIST: {resp.strip()}")

        # UPLOAD
        s.sendall(f"UPLOAD {FILENAME}\n".encode())
        time.sleep(0.1)
        resp = s.recv(1024).decode()
        if "READY" in resp:
            encoded = base64.b64encode(FILE_CONTENT).decode()
            s.sendall(f"{encoded}\n".encode())
            time.sleep(0.1)
            resp = s.recv(1024).decode()
            print(f"[Client {id}] UPLOAD: {resp.strip()}")

        # final LIST
        s.sendall("LIST\n".encode())
        time.sleep(0.1)
        resp = s.recv(1024).decode()
        print(f"[Client {id}] final LIST: {resp.strip()}")

        s.close()
    except Exception as e:
        print(f"[Client {id}] ERROR: {e}")

# spawn multiple clients
threads = []
NUM_CLIENTS = 3  # test 3 clients at once
for i in range(NUM_CLIENTS):
    t = threading.Thread(target=client_task, args=(i+1,))
    t.start()
    threads.append(t)
    time.sleep(0.05)  # slight stagger

for t in threads:
    t.join()

print("All clients done.")
