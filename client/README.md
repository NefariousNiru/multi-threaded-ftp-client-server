# Running the Program

1. **Compile the C++ executable:**
   ```bash
   make
   ```

2. **Run the program:**
   ```bash
   ./myftp <HOSTNAME> <nport> <tport>
   ```
   Replace `<HOSTNAME>` with the HOSTNAME eg - localhost, `<nport>` with the port the server will run on and `<tport>` with the port number the terminate thread will run on.

3. **Clean up build artifacts:**

   To remove the compiled files and clean up the directory, run:
   ```bash
   make clean
   ```