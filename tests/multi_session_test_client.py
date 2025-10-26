#!/usr/bin/env python3
"""
Multi-Session Test Client for Dropbox Clone Server
Tests concurrent sessions for the same user
"""

# .. DID NOT USE THIS FILE FOR TESTING
# TRY LATER JUST AS A DOUBLE YEST

import socket
import threading
import time
import base64
import sys

SERVER_HOST = '127.0.0.1'
SERVER_PORT = 8080

class ClientSession:
    def __init__(self, session_id):
        self.session_id = session_id
        self.sock = None
        
    def connect(self):
        """Connect to server"""
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((SERVER_HOST, SERVER_PORT))
        print(f"[Session {self.session_id}] Connected to server")
        
    def send_command(self, command):
        """Send command and receive response"""
        try:
            self.sock.sendall((command + '\n').encode())
            response = self.sock.recv(4096).decode()
            return response
        except Exception as e:
            print(f"[Session {self.session_id}] Error: {e}")
            return None
            
    def login(self, username, password):
        """Login to server"""
        response = self.send_command(f"login {username} {password}")
        print(f"[Session {self.session_id}] Login response: {response.strip()}")
        return response
        
    def signup(self, username, password):
        """Signup to server"""
        response = self.send_command(f"signup {username} {password}")
        print(f"[Session {self.session_id}] Signup response: {response.strip()}")
        return response
        
    def upload(self, filename, content):
        """Upload file"""
        # Encode content to base64
        encoded = base64.b64encode(content.encode()).decode()
        
        response = self.send_command(f"UPLOAD {filename}")
        if "READY_TO_RECEIVE" in response:
            self.sock.sendall(encoded.encode())
            result = self.sock.recv(4096).decode()
            print(f"[Session {self.session_id}] Upload response: {result.strip()}")
            return result
        else:
            print(f"[Session {self.session_id}] Upload failed: {response.strip()}")
            return response
            
    def download(self, filename):
        """Download file"""
        response = self.send_command(f"DOWNLOAD {filename}")
        print(f"[Session {self.session_id}] Download response length: {len(response)}")
        return response
        
    def delete(self, filename):
        """Delete file"""
        response = self.send_command(f"DELETE {filename}")
        print(f"[Session {self.session_id}] Delete response: {response.strip()}")
        return response
        
    def list_files(self):
        """List files"""
        response = self.send_command("LIST")
        print(f"[Session {self.session_id}] List response:\n{response}")
        return response
        
    def logout(self):
        """Logout"""
        response = self.send_command("logout")
        print(f"[Session {self.session_id}] Logout response: {response.strip()}")
        return response
        
    def close(self):
        """Close connection"""
        if self.sock:
            self.sock.close()
            print(f"[Session {self.session_id}] Connection closed")

def test_single_user_multiple_sessions():
    """Test multiple concurrent sessions for the same user"""
    print("\n=== Test 1: Single User, Multiple Sessions ===")
    
    # Create first session and signup
    session1 = ClientSession(1)
    session1.connect()
    session1.signup("alice", "password123")
    time.sleep(0.5)
    
    # Create second session with same user
    session2 = ClientSession(2)
    session2.connect()
    session2.login("alice", "password123")
    time.sleep(0.5)
    
    # Create third session with same user
    session3 = ClientSession(3)
    session3.connect()
    session3.login("alice", "password123")
    time.sleep(0.5)
    
    # Upload files from different sessions
    print("\n--- Concurrent uploads from different sessions ---")
    session1.upload("file1.txt", "Content from session 1")
    time.sleep(0.2)
    session2.upload("file2.txt", "Content from session 2")
    time.sleep(0.2)
    session3.upload("file3.txt", "Content from session 3")
    time.sleep(0.5)
    
    # List files from each session
    print("\n--- Listing files from different sessions ---")
    session1.list_files()
    time.sleep(0.2)
    session2.list_files()
    time.sleep(0.2)
    session3.list_files()
    time.sleep(0.5)
    
    # Download from different sessions
    print("\n--- Concurrent downloads ---")
    session1.download("file2.txt")
    session2.download("file1.txt")
    session3.download("file3.txt")
    time.sleep(0.5)
    
    # Delete from one session, verify from another
    print("\n--- Delete from one session, verify from another ---")
    session1.delete("file2.txt")
    time.sleep(0.5)
    session2.list_files()
    time.sleep(0.5)
    
    # Logout sessions one by one
    print("\n--- Gradual logout ---")
    session1.logout()
    time.sleep(0.5)
    session2.list_files()  # Should still work
    time.sleep(0.5)
    session2.logout()
    time.sleep(0.5)
    session3.list_files()  # Should still work
    time.sleep(0.5)
    
    # Close all connections
    session1.close()
    session2.close()
    session3.close()

def test_concurrent_operations():
    """Test truly concurrent operations using threads"""
    print("\n=== Test 2: Concurrent Operations (Threaded) ===")
    
    def worker(session_id, username, password):
        session = ClientSession(session_id)
        session.connect()
        
        if session_id == 1:
            session.signup(username, password)
        else:
            time.sleep(0.5)  # Wait for signup to complete
            session.login(username, password)
        
        # Each session uploads multiple files
        for i in range(3):
            filename = f"session{session_id}_file{i}.txt"
            content = f"Data from session {session_id}, file {i}"
            session.upload(filename, content)
            time.sleep(0.1)
        
        # List files
        session.list_files()
        
        # Download a file uploaded by another session
        other_session = (session_id % 3) + 1
        session.download(f"session{other_session}_file0.txt")
        
        # Logout and close
        session.logout()
        session.close()
    
    # Start 3 concurrent sessions
    threads = []
    for i in range(1, 4):
        t = threading.Thread(target=worker, args=(i, "bob", "secret123"))
        threads.append(t)
        t.start()
    
    # Wait for all threads to complete
    for t in threads:
        t.join()
    
    print("\n=== All concurrent sessions completed ===")

def test_quota_enforcement_multi_session():
    """Test quota enforcement across multiple sessions"""
    print("\n=== Test 3: Quota Enforcement Across Sessions ===")
    
    session1 = ClientSession(1)
    session1.connect()
    session1.signup("charlie", "pass456")
    
    session2 = ClientSession(2)
    session2.connect()
    session2.login("charlie", "pass456")
    
    # Upload large files from both sessions
    large_content = "X" * 500000  # 500KB
    
    print("\n--- Session 1 uploading 500KB ---")
    session1.upload("large1.txt", large_content)
    time.sleep(0.5)
    
    print("\n--- Session 2 uploading 500KB ---")
    session2.upload("large2.txt", large_content)
    time.sleep(0.5)
    
    print("\n--- Session 1 trying to upload another 500KB (should exceed quota) ---")
    session1.upload("large3.txt", large_content)
    time.sleep(0.5)
    
    # List to show quota usage
    session1.list_files()
    
    session1.logout()
    session2.logout()
    session1.close()
    session2.close()

def test_session_cleanup_on_disconnect():
    """Test that sessions are properly cleaned up on disconnect"""
    print("\n=== Test 4: Session Cleanup on Disconnect ===")
    
    session1 = ClientSession(1)
    session1.connect()
    session1.signup("dave", "password")
    
    session2 = ClientSession(2)
    session2.connect()
    session2.login("dave", "password")
    
    session3 = ClientSession(3)
    session3.connect()
    session3.login("dave", "password")
    
    print("\n--- 3 sessions active ---")
    time.sleep(0.5)
    
    # Disconnect session 2 without logout
    print("\n--- Closing session 2 without logout ---")
    session2.close()
    time.sleep(0.5)
    
    # Other sessions should still work
    session1.list_files()
    session3.list_files()
    
    session1.logout()
    session3.logout()
    session1.close()
    session3.close()

if __name__ == "__main__":
    print("Multi-Session Test Suite for Dropbox Clone Server")
    print("=" * 60)
    print("Make sure the server is running on {}:{}".format(SERVER_HOST, SERVER_PORT))
    print("=" * 60)
    
    try:
        # Run all tests
        test_single_user_multiple_sessions()
        time.sleep(2)
        
        test_concurrent_operations()
        time.sleep(2)
        
        test_quota_enforcement_multi_session()
        time.sleep(2)
        
        test_session_cleanup_on_disconnect()
        
        print("\n" + "=" * 60)
        print("All tests completed!")
        print("=" * 60)
        
    except KeyboardInterrupt:
        print("\n\nTest interrupted by user")
        sys.exit(0)
    except Exception as e:
        print(f"\n\nTest failed with error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)