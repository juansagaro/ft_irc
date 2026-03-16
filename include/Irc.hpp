#ifndef IRC_HPP
# define IRC_HPP

# include <iostream>
# include <string>
# include <sstream>
# include <vector>
# include <map>
# include <set>
# include <algorithm>
# include <cstdlib>
# include <cstring>
# include <cerrno>
# include <csignal>
# include <cctype>

# include <sys/socket.h>
# include <sys/types.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <netdb.h>
# include <unistd.h>
# include <fcntl.h>
# include <poll.h>

#  define MAX_FDS 128
#  define READ_SIZE 512
#  define BUF_LIMIT 4096

extern bool g_alive;

class Client;
class Channel;
class Server;
class Bot;

# include "Replies.hpp"
# include "Client.hpp"
# include "Channel.hpp"
# include "Bot.hpp"
# include "Server.hpp"

#endif
