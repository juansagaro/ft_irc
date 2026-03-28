*This project has been created as part of the 42 curriculum by ravazque, jsagaro- and fgalvez.*

## Description

**ft_irc** is an IRC (Internet Relay Chat) server written in C++98. It handles multiple concurrent clients through non-blocking I/O using `poll()`, and implements the core subset of the IRC protocol required for real-time text communication over TCP/IP.

The server supports authentication via connection password, nickname and username registration, channel management with operator privileges, private messaging between users, and five channel modes (i, t, k, o, l). It handles partial TCP data correctly by buffering incomplete commands, and shuts down gracefully on SIGINT.

### Bonus features

- **File transfer (DCC)**: The server transparently relays DCC SEND messages between clients, enabling peer-to-peer file transfers through any standard IRC client that supports DCC.
- **Bot (Marvin)**: An integrated pseudo-client bot inspired by Marvin the Paranoid Android. It responds to 11 commands (!help, !ask, !quote, !cheer, !rps, !roll, !coin, !time, !info, !who, !rules) and appears as a member of every channel.

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

Any standard IRC client can be used (irssi, WeeChat, HexChat, LimeChat). Example with irssi:

```bash
irssi -c 127.0.0.1 -p 6667 -w mypassword
```

Example with socat (raw protocol testing):

```bash
socat - TCP:127.0.0.1:6667
PASS mypassword
NICK testuser
USER testuser 0 * :Test User
JOIN #general
PRIVMSG #general :Hello everyone!
```

### Supported commands

| Command | Parameters | Description |
|---------|-----------|-------------|
| `PASS` | `<password>` | Authenticate with the server |
| `NICK` | `<nickname>` | Set or change nickname (max 9 chars) |
| `USER` | `<user> <mode> <unused> :<realname>` | Set username and real name |
| `JOIN` | `<channel>[,<channel>] [<key>[,<key>]]` | Join one or more channels |
| `PART` | `<channel>[,<channel>] [:<reason>]` | Leave one or more channels |
| `PRIVMSG` | `<target> :<message>` | Send a message to user or channel |
| `NOTICE` | `<target> :<message>` | Send a notice (no auto-reply) |
| `KICK` | `<channel> <user> [:<reason>]` | Remove a user from a channel |
| `INVITE` | `<nickname> <channel>` | Invite a user to a channel |
| `TOPIC` | `<channel> [:<topic>]` | View or set the channel topic |
| `MODE` | `<channel> [<flags> [<params>]]` | View or change channel modes |
| `PING` | `<token>` | Connection keepalive |
| `QUIT` | `[:<message>]` | Disconnect from the server |

### Channel modes

| Mode | Parameter | Description |
|------|-----------|-------------|
| `i` | none | Set/remove invite-only channel |
| `t` | none | Restrict TOPIC changes to operators |
| `k` | `<key>` | Set/remove channel password |
| `o` | `<nickname>` | Give/take channel operator status |
| `l` | `<limit>` | Set/remove maximum user count |

## Resources

### References

- [RFC 2812 - IRC Client Protocol](https://datatracker.ietf.org/doc/html/rfc2812)
- [RFC 1459 - Internet Relay Chat Protocol](https://datatracker.ietf.org/doc/html/rfc1459)
- [Modern IRC Client Protocol](https://modern.ircdocs.horse/)
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)

### Use of AI tools

Claude (Anthropic) was used as a support tool during the development of this project for the following tasks:

- **Documentation**: Review of this README.
- **Code review**: Reviewing error handling, edge cases, and protocol compliance against RFC specifications.
- **Debugging support**: Analyzing specific issues with partial data handling and channel mode parsing.
- **Testing**: Helping design test scenarios for edge cases (partial TCP data, buffer overflow protection, multi-mode commands).

All code was written, understood, and validated by the project authors. The final implementation reflects decisions made through study of the IRC protocol specifications and network programming fundamentals.
