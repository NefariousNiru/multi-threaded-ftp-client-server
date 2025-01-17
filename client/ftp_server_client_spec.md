
# Server-Client Interaction Specification

This document outlines the expected communication protocol between the server and client in the FTP project.

---

## General Rules

1. **Message Protocol**:
   - All messages are plain text, except for file content during `get` and `put` commands.
   - Messages are terminated with a newline character (`\n`).

2. **Listening Behavior**:
   - After sending any command, the client **must wait** and listen for a server response before proceeding with further actions.
   - For commands involving file transfers (`get`/`put`), additional data handling is required as specified.

---

## Interaction Details

### **Server to Client** (Messages the client should listen for)

1. **Initial Connection**:
   - **Server Sends**: Welcome message.
     ```
     Example: "Connected to MyFTPServer!"
     ```

2. **Command Responses**:
   - **General**:
     ```
     SUCCESS: <Description of successful operation>
     ERROR: <Description of the error>
     ```
     - Example:
       ```
       SUCCESS: Directory created successfully.
       ERROR: File not found.
       ```

   - **File Transfers**:
     - `get`:
       - `SUCCESS: FILE_TRANSFER_START`
       - Binary file data (raw content, not a string).
       - `FILE_TRANSFER_END`
     - `put`:
       - `SUCCESS: READY_TO_RECEIVE`
       - Final acknowledgment after the file is received:
         ```
         SUCCESS: File transfer completed.
         ```

3. **Multiline Responses**:
   - Commands like `ls` may send multiline responses:
     ```
     SUCCESS: 
     file1.txt
     file2.cpp
     dir1
     ```

---

### **Client to Server** (Messages the server listens for)

1. **Command Structure**:
   - **General**:
     ```
     <command> [arguments]\n
     ```
     - Examples:
       ```
       pwd\n
       ls\n
       cd dir1\n
       mkdir newdir\n
       delete file.txt\n
       ```

   - **File Transfers**:
     - `get`:
       ```
       get <filename>\n
       ```
       - Example:
         ```
         get file1.txt\n
         ```

     - `put`:
       ```
       put <filename>\n
       ```
       - The client must send:
         - The file's binary content.
         - `FILE_TRANSFER_END\n` to indicate the end of the file transfer.

2. **Session Termination**:
   - **Command**:
     ```
     quit\n
     ```

---

## Examples of Interaction

### **Basic Commands**

1. **Client Sends**:
   ```
   pwd\n
   ```
   **Server Responds**:
   ```
   SUCCESS: /home/user/projects\n
   ```

2. **Client Sends**:
   ```
   mkdir newdir\n
   ```
   **Server Responds**:
   ```
   SUCCESS: Directory created successfully.\n
   ```

3. **Client Sends**:
   ```
   ls\n
   ```
   **Server Responds**:
   ```
   SUCCESS: 
   file1.txt
   file2.cpp
   dir1
   ```

---

### **File Transfer**

1. **Download (`get`)**:

   **Client Sends**:
   ```
   get file1.txt\n
   ```

   **Server Responds**:
   ```
   SUCCESS: FILE_TRANSFER_START\n
   ```
   **Server Sends**:
   - Binary file data.
   - `FILE_TRANSFER_END\n`

2. **Upload (`put`)**:

   **Client Sends**:
   ```
   put file2.txt\n
   ```

   **Server Responds**:
   ```
   SUCCESS: READY_TO_RECEIVE\n
   ```
   **Client Sends**:
   - Binary file data.
   - `FILE_TRANSFER_END\n`

   **Server Responds**:
   ```
   SUCCESS: File transfer completed.\n
   ```

---

## Client Responsibilities

1. **Listen After Every Command**:
   - The client must **always** listen for a response after sending a command, even if the response is multiline or includes binary data.

2. **Special Cases**:
   - For file transfers, the client must correctly handle:
     - Receiving file data for `get`.
     - Sending file data for `put`.

3. **Graceful Handling**:
   - The client should handle `ERROR` responses gracefully and display them to the user.
