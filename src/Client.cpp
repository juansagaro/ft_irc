#include "Irc.hpp"

Client::Client() : _sockFd(-1), _authorized(false), _welcomed(false) {}

Client::Client(int fd) : _sockFd(fd), _authorized(false), _welcomed(false) {}

Client::Client(const Client& src)
	: _sockFd(src._sockFd), _nick(src._nick), _user(src._user),
	  _fullname(src._fullname), _host(src._host), _recvBuf(src._recvBuf),
	  _authorized(src._authorized), _welcomed(src._welcomed) {}

Client& Client::operator=(const Client& src)
{
	if (this != &src)
	{
		_sockFd     = src._sockFd;
		_nick       = src._nick;
		_user       = src._user;
		_fullname   = src._fullname;
		_host       = src._host;
		_recvBuf    = src._recvBuf;
		_authorized = src._authorized;
		_welcomed   = src._welcomed;
	}
	return *this;
}

Client::~Client() {}

int					Client::socketFd() const { return _sockFd; }
const std::string&	Client::getNick() const { return _nick; }
const std::string&	Client::getUser() const { return _user; }
const std::string&	Client::getFullname() const { return _fullname; }
const std::string&	Client::getHost() const { return _host; }
bool				Client::hasAuth() const { return _authorized; }
bool				Client::isWelcomed() const { return _welcomed; }
std::string&		Client::recvBuf() { return _recvBuf; }

void	Client::changeNick(const std::string& val) { _nick = val; }
void	Client::changeUser(const std::string& val) { _user = val; }
void	Client::changeFullname(const std::string& val) { _fullname = val; }
void	Client::changeHost(const std::string& val) { _host = val; }
void	Client::markAuth(bool val) { _authorized = val; }
void	Client::markWelcomed(bool val) { _welcomed = val; }

bool Client::bufferAppend(const std::string& chunk)
{
	_recvBuf += chunk;
	return _recvBuf.size() <= BUF_LIMIT;
}

void Client::bufferClear() { _recvBuf.clear(); }

std::string Client::fullId() const
{
	std::string n = _nick.empty() ? "*" : _nick;
	std::string u = _user.empty() ? "*" : _user;
	return n + "!" + u + "@" + _host;
}
