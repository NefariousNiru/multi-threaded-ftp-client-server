# Running the Program

1. **Compile the C++ executable:**
   ```bash
   make
   ```

2. **Run the program:**
   ```bash
   ./myftp <HOSTNAME> <PORT>
   ```
   Replace `<HOSTNAME>` with the HOSTNAME eg - localhost and `<PORT>` with the port number the server will run on.

3. **Clean up build artifacts:**

   To remove the compiled files and clean up the directory, run:
   ```bash
   make clean
   ```