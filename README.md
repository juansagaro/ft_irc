# ft_irc

## 📖 About

"ft_irc" is a project at 42 Madrid that involves creating an IRC (Internet Relay Chat) server written in C++98. This project introduces network programming fundamentals, non-blocking I/O multiplexing, and teaches how to implement a real-time communication protocol that can be tested with any standard IRC client.

The goal is to implement a fully functional server that handles multiple simultaneous clients, supports authentication, channel management, private messaging, and operator privileges, all through a single `poll()` event loop without forking.

## 🎯 Objectives

- Understanding TCP/IP socket programming and client-server architecture
- Implementing non-blocking I/O multiplexing with `poll()`
- Learning the IRC protocol (RFC 1459 / RFC 2812)
- Managing concurrent connections and partial data buffering
- Implementing authentication, user registration, and channel management
- Handling operator privileges and channel modes
- Writing robust, crash-resistant network code in C++98

## 📋 Function Overview

<details>
<summary><strong>ft_irc</strong></summary>

<br>

| Feature | Description |
|---------|-------------|
| **Non-blocking Server** | Single `poll()` event loop handling all I/O operations |
| **Authentication** | Password-based connection authentication |
| **User Registration** | PASS/NICK/USER registration flow with welcome messages |
| **Channel System** | Create, join, part channels with `#` or `&` prefix |
| **Private Messaging** | Direct messages between users via PRIVMSG |
| **Channel Messaging** | Broadcast messages to all channel members |
| **Operator System** | Channel operators with elevated privileges |
| **Channel Modes** | Five configurable channel modes (i, t, k, o, l) |
| **Invite System** | Invite users to channels, with invite-only support |
| **Topic Management** | View and set channel topics with optional restriction |
| **Kick Command** | Operators can remove users from channels |
| **Partial Data Handling** | Aggregates TCP packets before processing commands |
| **Graceful Shutdown** | SIGINT handler for clean server termination |
| **File Transfer (DCC)** | Transparent relay of DCC SEND for peer-to-peer file transfers |
| **Bot (Marvin)** | Integrated pseudo-client bot with 11 interactive commands |

<br>

</details>

<details>
<summary><strong>Usage Example & Testing</strong></summary>

### Quick Start

**1. Compile and launch the server:**
```bash
make && ./ircserv 6667 mypassword
```

**2. Connect with a client (open another terminal):**
```bash
# Using socat
socat - TCP:127.0.0.1:6667
```

**3. Register and chat:**
```
PASS mypassword
NICK alice
USER alice 0 * :Alice Smith
JOIN #test
PRIVMSG #test :Hello from alice!
```

### Multi-client Test

Open three terminals to simulate a full chat scenario:

**Terminal 1 - Server:**
```bash
./ircserv 6667 secret
```

**Terminal 2 - Client A:**
```bash
socat - TCP:127.0.0.1:6667
PASS secret
NICK alice
USER alice 0 * :Alice
JOIN #general
PRIVMSG #general :Hi everyone!
```

**Terminal 3 - Client B:**
```bash
socat - TCP:127.0.0.1:6667
PASS secret
NICK bob
USER bob 0 * :Bob
JOIN #general
PRIVMSG #general :Hey alice!
PRIVMSG alice :This is a private message
```

### Testing Channel Modes

```bash
# alice creates a channel (becomes operator automatically)
JOIN #secret

# Set invite-only + password + user limit
MODE #secret +ikl key123 10

# Set topic (operator only when +t is set)
TOPIC #secret :Welcome to the secret channel

# Invite bob
INVITE bob #secret

# bob joins with key
JOIN #secret key123

# Give bob operator status
MODE #secret +o bob

# Kick bob
KICK #secret bob :reason here
```

### Testing with irssi

```bash
irssi -c 127.0.0.1 -p 6667 -w mypassword -n mynick
```

Once connected:
```
/join #test
/msg #test Hello!
/msg othernick Private hello
/topic #test New topic
/mode #test +i
/invite othernick #test
/kick #test othernick reason
/quit bye
```

### Testing the Bot (Marvin)

Marvin responds to `!commands` in channels and via direct message:

```bash
# In a channel:
PRIVMSG #test :!help
PRIVMSG #test :!roll 2d6
PRIVMSG #test :!rps rock
PRIVMSG #test :!info
PRIVMSG #test :!who #test

# Direct message:
PRIVMSG Marvin :!quote
```

Available commands: `!help`, `!ask`, `!quote`, `!cheer`, `!rps`, `!roll`, `!coin`, `!time`, `!info`, `!who`, `!rules`

### File Transfer (DCC)

DCC file transfers work transparently between IRC clients:

```bash
# In irssi - sender:
/dcc send bob /path/to/file.txt

# In irssi - receiver:
/dcc get alice
```

### Graceful Shutdown

Press `Ctrl+C` on the server terminal to trigger a clean shutdown via SIGINT.

<br>

</details>

## 🚀 Installation & Structure

<details>
<summary><strong>📥 Compilation & Usage</strong></summary>

## Build

```bash
make        # Compile the project
make clean  # Remove object files
make fclean # Full cleanup (objects + binary)
make re     # Recompile from scratch
```

<br>

## Usage

```bash
./ircserv <port> <password>
```

| Argument | Range | Description |
|----------|-------|-------------|
| `port` | 1 - 65535 | TCP port to listen on |
| `password` | non-empty | Connection password for clients |

<br>

## Supported IRC Commands

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

<br>

## Channel Modes

| Mode | Parameter | Description |
|------|-----------|-------------|
| `i` | none | Set/remove invite-only channel |
| `t` | none | Restrict TOPIC changes to operators |
| `k` | `<key>` | Set/remove channel password |
| `o` | `<nickname>` | Give/take channel operator status |
| `l` | `<limit>` | Set/remove maximum user count |

<br>

## Connecting with an IRC Client

**irssi:**
```bash
irssi -c 127.0.0.1 -p 6667 -w mypassword
```

**WeeChat:**
```
/server add local 127.0.0.1/6667 -password=mypassword
/connect local
```

**HexChat:**
Set server to `127.0.0.1/6667`, server password to `mypassword`.

**Testing with socat:**
```bash
socat - TCP:127.0.0.1:6667
PASS mypassword
NICK testuser
USER testuser 0 * :Test User
JOIN    #general
PRIVMSG #general :Hello everyone!
QUIT :bye
```

<br>

</details>

<details>
<summary><strong>📁 Project Structure</strong></summary>

<br>

```
ft_irc/
│
├── docs/
│   ├── README.md                # Project documentation
│   └── ft_irc.md                # Study guide covering the full codebase
├── include/
│   ├── Irc.hpp                  # Main header: standard includes, defines, forward declarations
│   ├── Replies.hpp              # IRC numeric reply constants (RPL:: and ERR:: namespaces)
│   ├── Server.hpp               # Server class: poll loop, command dispatch, helpers
│   ├── Client.hpp               # Client class: connection state, buffer, identity
│   ├── Channel.hpp              # Channel class: membership, modes, relay
│   └── Bot.hpp                  # Bot class: Marvin pseudo-client definition
├── src/
│   ├── main.cpp                 # Entry point, argument validation, signal handlers
│   ├── Server.cpp               # Constructor, destructor, boot(), run(), helper methods
│   ├── Network.cpp              # Socket setup, accept, receive with buffering, disconnect
│   ├── Client.cpp               # Client state management, OCF, fullId() prefix
│   ├── Channel.cpp              # Channel membership, operator management, broadcast
│   ├── Parse.cpp                # IRC message parser, command dispatcher, registration check
│   ├── CmdAuth.cpp              # PASS, NICK, USER, QUIT, PING command handlers
│   ├── CmdChannel.cpp           # JOIN, PART, TOPIC, MODE (with decomposed sub-handlers)
│   ├── CmdMessage.cpp           # PRIVMSG, NOTICE, KICK, INVITE command handlers
│   └── Bot.cpp                  # Bot Marvin: 11 interactive commands with server integration
├── .gitignore
├── Makefile
└── README.md                    # GitHub project overview
```

<br>

</details>

## 💡 Key Learning Outcomes

The ft_irc project teaches fundamental network programming and protocol implementation:

- **Socket Programming**: Creating, binding, and listening on TCP sockets
- **I/O Multiplexing**: Using `poll()` to handle multiple file descriptors in a single thread
- **Non-blocking I/O**: Setting sockets to non-blocking mode with `fcntl()`
- **Protocol Implementation**: Parsing and responding to IRC protocol messages
- **Buffer Management**: Handling partial TCP reads and command reconstruction
- **State Management**: Tracking client registration, channel membership, and operator status
- **Signal Handling**: Graceful shutdown with SIGINT, ignoring SIGPIPE for broken connections
- **Memory Management**: Proper cleanup of dynamically allocated clients and channels

## ⚙️ Technical Specifications

- **Language**: C++ (C++98 standard)
- **Compiler**: c++ with `-Wall -Wextra -Werror -std=c++98`
- **I/O Model**: Single-threaded, non-blocking with `poll()`
- **Protocol**: IRC (RFC 1459 / RFC 2812 subset)
- **Transport**: TCP/IP (IPv4)
- **Max Connections**: 128
- **Buffer Size**: 512 bytes per read, 4096 bytes max per client
- **Nickname Length**: 9 characters maximum

## 🔧 Requirements

- Linux or macOS
- C++ compiler with C++98 support
- Make
- An IRC client for testing (irssi, WeeChat, HexChat, or socat/netcat)

---

> [!NOTE]
> This project provides hands-on experience with network programming, protocol design, and concurrent connection handling, demonstrating proficiency in systems-level C++ programming.
