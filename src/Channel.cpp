#include "Irc.hpp"

Channel::Channel() : _modeI(false), _modeT(false), _cap(0) {}

Channel::Channel(const std::string& name) : _label(name), _modeI(false), _modeT(false), _cap(0) {}

Channel::Channel(const Channel& src)
	: _label(src._label), _subject(src._subject), _passkey(src._passkey),
	  _users(src._users), _moderators(src._moderators),
	  _whitelist(src._whitelist), _modeI(src._modeI),
	  _modeT(src._modeT), _cap(src._cap) {}

Channel& Channel::operator=(const Channel& src)
{
	if (this != &src)
	{
		_label      = src._label;
		_subject    = src._subject;
		_passkey    = src._passkey;
		_users      = src._users;
		_moderators = src._moderators;
		_whitelist  = src._whitelist;
		_modeI      = src._modeI;
		_modeT      = src._modeT;
		_cap        = src._cap;
	}
	return *this;
}

Channel::~Channel() {}

const std::string&			Channel::getLabel() const { return _label; }
const std::string&			Channel::getSubject() const { return _subject; }
const std::string&			Channel::getPasskey() const { return _passkey; }
const std::set<Client*>&	Channel::getUsers() const { return _users; }
bool						Channel::flagInvite() const { return _modeI; }
bool						Channel::flagTopic() const { return _modeT; }
int							Channel::getCap() const { return _cap; }
const std::set<std::string>&	Channel::getWhitelist() const { return _whitelist; }

void	Channel::changeSubject(const std::string& val) { _subject = val; }
void	Channel::changePasskey(const std::string& val) { _passkey = val; }
void	Channel::toggleInvite(bool val) { _modeI = val; }
void	Channel::toggleTopic(bool val) { _modeT = val; }
void	Channel::changeCap(int val) { _cap = val; }

void Channel::enroll(Client* c) { _users.insert(c); }

void Channel::dismiss(Client* c)
{
	_users.erase(c);
	_moderators.erase(c);
}

bool Channel::enrolled(Client* c) const
{
	return _users.find(c) != _users.end();
}

size_t	Channel::headcount() const { return _users.size(); }
bool	Channel::vacant() const { return _users.empty(); }

void	Channel::promote(Client* c) { _moderators.insert(c); }
void	Channel::demote(Client* c) { _moderators.erase(c); }

bool Channel::isModerator(Client* c) const
{
	return _moderators.find(c) != _moderators.end();
}

bool Channel::hasOps() const { return !_moderators.empty(); }

Client* Channel::firstUser() const
{
	return _users.empty() ? NULL : *_users.begin();
}

void	Channel::allow(const std::string& nick) { _whitelist.insert(nick); }
void	Channel::revoke(const std::string& nick) { _whitelist.erase(nick); }

bool Channel::isAllowed(const std::string& nick) const
{
	return _whitelist.find(nick) != _whitelist.end();
}

void Channel::relay(const std::string& msg, Client* skip)
{
	for (std::set<Client*>::iterator it = _users.begin();
		it != _users.end(); ++it)
	{
		if (*it != skip)
			send((*it)->socketFd(), msg.c_str(), msg.size(), 0);
	}
}
