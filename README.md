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
```

# IE3010 NetMessenger - Full Execution and Testing Commands

## 1. Open the Project Directory

```bash
cd /media/sf_IT23629844/NetMessenger
pwd
ls -lh

2. Verify Personalised Configuration
printf "Registration Number : IT23629844\n"
printf "Numeric Part        : 23629844\n"
printf "Last Four Digits    : 9844\n"
printf "Server Port         : 15844\n"
printf "Node ID             : NID:6298\n"
printf "Server Source       : server_9844.c\n"
printf "Client Source       : client_9844.c\n"
printf "Makefile             : Makefile_9844\n"
printf "Log File             : netmsg_IT23629844.log\n"
printf "Storage Root         : ./storage/IT23629844\n"

3. Compile the Server and Client
make -f Makefile_9844

Verify the generated programs:
ls -lh server_9844 client_9844

4. Start the Server
Open Terminal 1:
cd /media/sf_IT23629844/NetMessenger
./server_9844

Expected startup information:
NetMessenger Server started
Registration: IT23629844
Listening on TCP port 15844
Node ID: NID:6298
Maximum clients: 50

Keep the server running.
5. Verify the TCP Listening Port
Open another terminal:
cd /media/sf_IT23629844/NetMessenger
ss -tlnp | grep 15844

The server should be listening on TCP port 15844.
6. Start Client 1
Open Terminal 3:
cd /media/sf_IT23629844/NetMessenger
./client_9844 127.0.0.1 dulara

Expected:
OK REGISTERED dulara NID:6298

7. Start Additional Clients
Open separate terminals for each client.
Client 2
cd /media/sf_IT23629844/NetMessenger
./client_9844 127.0.0.1 kamal

Client 3
cd /media/sf_IT23629844/NetMessenger
./client_9844 127.0.0.1 user3

Client 4
cd /media/sf_IT23629844/NetMessenger
./client_9844 127.0.0.1 user4

Client 5
cd /media/sf_IT23629844/NetMessenger
./client_9844 127.0.0.1 user5

Client 6
cd /media/sf_IT23629844/NetMessenger
./client_9844 127.0.0.1 user6

The assignment requires the server to support multiple simultaneous clients. Five or more clients can therefore be used as execution evidence.

8. Registration and Presence Testing
Observe the other client terminals after each new client connects.
Expected presence notifications include:
MSG PRESENCE JOIN kamal
MSG PRESENCE JOIN user3
MSG PRESENCE JOIN user4
MSG PRESENCE JOIN user5
MSG PRESENCE JOIN user6

9. Test LIST
From the dulara client:
LIST

Expected form:
OK USERS dulara,kamal,user3,user4,user5,user6 NID:6298

10. Test Broadcast Messaging
From the dulara client:
BCAST Hello from dulara - IT23629844

Expected response to sender:
OK SENT NID:6298

Other connected clients should receive a broadcast message similar to:
MSG BCAST dulara Hello from dulara - IT23629844

11. Test Private Messaging
From the dulara client:
PMSG kamal Hello Kamal

Expected sender response:
OK SENT NID:6298

The kamal client should receive:
MSG PRIV dulara Hello Kamal

12. Test Chat Room Creation and Joining
From dulara:
JOIN study

Expected:
OK JOINED study NID:6298

From kamal:
JOIN study

Expected:
OK JOINED study NID:6298

13. List Available Rooms
ROOMS

Expected form:
OK ROOMS study NID:6298

14. Send a Room Message
From dulara:
RMSG study Hello study room

Expected sender response:
OK SENT NID:6298

Members of the room receive a message similar to:
MSG ROOM study dulara Hello study room

15. Leave a Room
From kamal:
LEAVE study

Expected:
OK LEFT study NID:6298

16. Test Invalid User Error
From a connected client:
PMSG nobody Hello

Expected form:
ERR 002 USER_NOT_FOUND NID:6298

17. Test Invalid Room Error
RMSG NoSuchRoom Hello

Expected form:
ERR 003 ROOM_NOT_FOUND NID:6298

18. Prepare a File for File Transfer
Create a test file:
echo "IE3010 NetMessenger file transfer test - IT23629844" > test.txt

Check the file:
cat test.txt

19. Test File Transfer
From the dulara client, send the file to kamal:
SENDFILE kamal test.txt /home/dulara/test.txt

The receiving client should receive the transferred file.
Expected server/client response:
OK FILE_RECEIVED test.txt NID:6298

20. Verify Server-Side Storage
Open another terminal:
cd /media/sf_IT23629844/NetMessenger
find storage/IT23629844 -maxdepth 3 -type f -print

Expected structure:
storage/IT23629844/.gitkeep
storage/IT23629844/dulara/test.txt
storage/IT23629844/kamal/...
storage/IT23629844/user3/...
storage/IT23629844/user4/...
storage/IT23629844/user5/...
storage/IT23629844/user6/...

21. Inspect the Stored File
cat storage/IT23629844/dulara/test.txt

Expected:
IE3010 NetMessenger file transfer test - IT23629844

22. Test Graceful Disconnect
From a client:
QUIT

Expected:
OK BYE NID:6298

The server should clean up the disconnected user's state.
23. Test Reconnection
After quitting, reconnect:
./client_9844 127.0.0.1 dulara

Expected:
OK REGISTERED dulara NID:6298

24. Check Server Log
Open another terminal:
cd /media/sf_IT23629844/NetMessenger
cat netmsg_IT23629844.log

For a shorter view:
tail -n 30 netmsg_IT23629844.log

The log should contain timestamped connection, disconnection, messaging and file-transfer activity.
25. Check Storage Files
find storage/IT23629844 -type f -print

26. Check Received Files
find received_files -type f -print

27. Inspect Project Files
ls -lh

28. Check Source Code for Socket Operations
grep -n -E "socket\(|bind\(|listen\(|accept\(" server_9844.c

29. Check Concurrent Client Handling
grep -n "pthread_create" server_9844.c

30. Check Registration Implementation
grep -n -A25 -B5 "REGISTER" server_9844.c

31. Check Broadcast and Private Messaging
grep -n "BCAST" server_9844.c
grep -n "PMSG" server_9844.c

32. Check Chat Room Implementation
grep -n -E "JOIN|LEAVE|ROOMS|RMSG" server_9844.c

33. Check File Transfer Implementation
grep -n -E "SENDFILE|storage|fwrite|fread|FILE_RECEIVED" server_9844.c

34. Check Error Handling
sed -n '105,120p' server_9844.c
sed -n '507,520p' server_9844.c

35. Check Disconnect and Logging Code
sed -n '53,68p' server_9844.c
sed -n '595,615p' server_9844.c

36. Git Status
From the project directory:
git status

37. Git Commit History
git log --oneline --decorate -10

38. Push Changes to GitHub
git add .
git commit -m "Update project evidence and documentation"
git push origin main

39. Final Verification
git status
git log --oneline --decorate -10

The final working tree should be clean after all intended changes have been committed.
40. Stop the Server
When testing is complete, return to the server terminal and press:
Ctrl+C


Assignment eke protocol commands, personalised port/NID, storage path, logging, and required functional areas me structure ekata directly align wenawa. 

**One correction:** `grep -n "pthread_create"` command eka use karanna epa if your actual server uses `select()` rather than pthreads. Assignment eka threads **or** `select/poll/epoll` allow karanawa. Oyage actual implementation evidence eka anuwa concurrency method eka report eke accurately state karanna oni. :chatgpt-content-reference{index="3"}

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
