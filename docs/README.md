*This project has been created as part of the 42 curriculum by ravazque.*

## Description

**ft_irc** is an IRC (Internet Relay Chat) server written in C++98. It handles multiple concurrent clients through non-blocking I/O using `poll()`, and implements the core subset of the IRC protocol required for real-time text communication over TCP/IP.

## Instructions

### Compilation

```bash
make        # Build the project
make clean  # Remove object files
make fclean # Remove object files and binary
make re     # Rebuild from scratch
```

### Execution

```bash
./ircserv <port> <password>
```

- **port**: Port number to listen on (1-65535)
- **password**: Connection password required by clients

### Connecting

Any standard IRC client can be used (irssi, WeeChat, HexChat, LimeChat). Example:

```bash
irssi -c 127.0.0.1 -p 6667 -w mypassword
```

## Resources

- [RFC 2812 - IRC Client Protocol](https://datatracker.ietf.org/doc/html/rfc2812)
- [RFC 1459 - Internet Relay Chat Protocol](https://datatracker.ietf.org/doc/html/rfc1459)
- [Modern IRC Client Protocol](https://modern.ircdocs.horse/)
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)
