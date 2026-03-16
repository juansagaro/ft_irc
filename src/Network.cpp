#include "Irc.hpp"

void Server::openSocket()
{
	_listenFd = socket(AF_INET, SOCK_STREAM, 0);
	if (_listenFd < 0)
		throw std::runtime_error(std::string("socket() failed: ") + strerror(errno));

	int on = 1;
	if (setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
	{
		close(_listenFd);
		throw std::runtime_error(std::string("setsockopt() failed: ") + strerror(errno));
	}
	fcntl(_listenFd, F_SETFL, O_NONBLOCK);

	struct sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family      = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port        = htons(_listenPort);

	if (bind(_listenFd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
	{
		close(_listenFd);
		throw std::runtime_error(std::string("bind() failed: ") + strerror(errno));
	}
	if (listen(_listenFd, MAX_FDS) < 0)
	{
		close(_listenFd);
		throw std::runtime_error(std::string("listen() failed: ") + strerror(errno));
	}

	struct pollfd entry;
	entry.fd      = _listenFd;
	entry.events  = POLLIN;
	entry.revents = 0;
	_pollSet.push_back(entry);
}

void Server::welcomeClient()
{
	struct sockaddr_in addr;
	socklen_t len = sizeof(addr);
	int fd = accept(_listenFd, (struct sockaddr*)&addr, &len);
	if (fd < 0)
		return;

	fcntl(fd, F_SETFL, O_NONBLOCK);

	struct pollfd entry;
	entry.fd      = fd;
	entry.events  = POLLIN;
	entry.revents = 0;
	_pollSet.push_back(entry);

	Client* fresh = new Client(fd);
	fresh->changeHost(inet_ntoa(addr.sin_addr));
	_sessions[fd] = fresh;
}

void Server::receiveData(int fd)
{
	char buf[READ_SIZE];
	int n = recv(fd, buf, sizeof(buf) - 1, 0);
	if (n <= 0)
	{
		if (_sessions.find(fd) != _sessions.end())
		{
			std::string notice = ":" + _sessions[fd]->fullId()
				+ " QUIT :Connection lost\r\n";
			notifyChannels(_sessions[fd], notice);
		}
		dropClient(fd);
		return;
	}

	buf[n] = '\0';
	Client* c = _sessions[fd];
	if (!c->bufferAppend(buf))
	{
		dropClient(fd);
		return;
	}

	std::string& raw = c->recvBuf();
	size_t pos;
	while ((pos = raw.find('\n')) != std::string::npos)
	{
		std::string line = raw.substr(0, pos);
		raw.erase(0, pos + 1);
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);
		if (!line.empty())
			routeMessage(c, line);
		if (_sessions.find(fd) == _sessions.end())
			return;
	}
}

void Server::dropClient(int fd)
{
	std::map<int, Client*>::iterator ci = _sessions.find(fd);
	if (ci == _sessions.end())
		return;

	Client* c = ci->second;

	for (std::map<std::string, Channel*>::iterator it = _rooms.begin();
		it != _rooms.end(); )
	{
		it->second->dismiss(c);
		if (it->second->vacant())
		{
			delete it->second;
			_rooms.erase(it++);
		}
		else
			++it;
	}

	for (std::vector<struct pollfd>::iterator it = _pollSet.begin();
		it != _pollSet.end(); ++it)
	{
		if (it->fd == fd)
		{
			_pollSet.erase(it);
			break;
		}
	}

	close(fd);
	delete c;
	_sessions.erase(fd);
}
