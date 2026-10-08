# IE3010 NetMessenger - IT23629844

## Personalisation
- Registration Number: IT23629844
- Numeric part: 23629844
- Last four digits: 9844
- Server/client port: 15844 (6000 + 9844)
- Node ID: NID:6298 (digits 3-6 of 23629844)
- Server source: server_9844.c
- Client source: client_9844.c
- Makefile: Makefile_9844
- Log: netmsg_IT23629844.log
- Storage: ./storage/IT23629844/<username>/<filename>
- Submission ZIP: IE3010_IT23629844.zip

## Build
```bash
make -f Makefile_9844
```

## Run server
```bash
./server_9844
```

## Check listening port
```bash
ss -tlnp | grep 15844
```

## Run five clients
Use five terminal windows:
```bash
./client_9844 127.0.0.1 dulara
./client_9844 127.0.0.1 kamal
./client_9844 127.0.0.1 user3
./client_9844 127.0.0.1 user4
./client_9844 127.0.0.1 user5
```

## Commands
REGISTER is sent automatically by the client at startup.
```text
LIST
BCAST <message>
PMSG <username> <message>
JOIN <room>
LEAVE <room>
ROOMS
RMSG <room> <message>
SENDFILE <target> <filename> <local-path>
QUIT
```

The wire protocol follows the assignment: text commands/responses are newline terminated, SENDFILE is followed immediately by exactly the declared number of raw bytes, and OK/ERR responses carry the personalised NID:6298 tag.

## File transfer
Example:
```text
SENDFILE kamal test.txt /home/dulara/test.txt
```

The server stores the file under:
```text
./storage/IT23629844/dulara/test.txt
```

The receiving client saves received files under:
```text
./received_files/test.txt
```

## Evidence checklist
Capture real screenshots for:
1. Server startup
2. `ss -tlnp | grep 15844`
3. Five simultaneous clients
4. LIST
5. BCAST
6. PMSG
7. JOIN / ROOMS / RMSG / LEAVE
8. File transfer
9. Storage directory
10. Error handling
11. QUIT / disconnect
12. Log file
13. Key source-code sections
14. Git history

## Project Structure

- server_9844.c - Multithreaded TCP server implementation
- client_9844.c - TCP client implementation
- Makefile_9844 - Build and run configuration
- storage/IT23629844 - User file storage
- received_files - Client-side received files
- netmsg_IT23629844.log - Server activity log
- README.md - Project setup and usage information

## Build and Run Workflow

1. Build both server and client using:
   make -f Makefile_9844

2. Start the server using:
   ./server_9844

3. Connect a client using:
   ./client_9844 127.0.0.1 <username>

4. Verify the server listener using:
   ss -tlnp | grep 15844

5. Use the supported messaging, room, file-transfer and disconnect commands.

6. Review server activity in:
   netmsg_IT23629844.log
