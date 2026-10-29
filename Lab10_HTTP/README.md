# Lab 10 — HTTP Client and Server (Group 11)

This directory contains a small HTTP/1.0 client and server written in C using POSIX sockets.

## Files

- `client.c` — connects to `127.0.0.1:8080`, sends an HTTP `GET` or `HEAD` request, and prints the response.
- `server.c` — listens on TCP port `8080` and serves `www/index.html`.
- `www/index.html` — sample page served by the server.

## Requirements

A C compiler (such as GCC) and a POSIX-compatible environment (Linux, macOS, or WSL).

## Build

Run these commands from the `Lab10_HTTP` directory:

```sh
gcc -Wall -Wextra -pedantic server.c -o server
gcc -Wall -Wextra -pedantic client.c -o client
```

## Run

Open terminal 1 and start the server:

```sh
./server
```

Open terminal 2 and send a GET request:

```sh
./client
# or explicitly:
./client GET
```

To send a HEAD request (headers only):

```sh
./client HEAD
```

The client connects to `127.0.0.1` on port `8080`. The server handles one connection at a time and supports `/` and `/index.html`. It returns HTTP status codes `200`, `400`, `404`, and `405` as appropriate.

## Notes

- Start the server before running the client.
- If port 8080 is already in use, stop the other process or change `PORT` in both source files.
- This is an educational example, not a production-ready HTTP server.
