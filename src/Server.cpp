#include "Irc.hpp"
#include <ctime>

Server::Server(int port, const std::string& pass)
	: _listenPort(port), _connPass(pass), _listenFd(-1), _bot(new Bot())
{
	std::srand(std::time(NULL));
}

Server::~Server()
{
	for (std::map<int, Client*>::iterator it = _sessions.begin();
		it != _sessions.end(); ++it)
	{
		close(it->first);
		delete it->second;
	}
	for (std::map<std::string, Channel*>::iterator it = _rooms.begin();
		it != _rooms.end(); ++it)
		delete it->second;
	if (_listenFd != -1)
		close(_listenFd);
	delete _bot;
}

void Server::boot()
{
	openSocket();
	std::cout << "Listening on port " << _listenPort << std::endl;
}

void Server::run()
{
	while (g_alive)
	{
		if (poll(&_pollSet[0], _pollSet.size(), 1000) < 0)
		{
			if (!g_alive)
				break;
			throw std::runtime_error(std::string("poll() failed: ") + strerror(errno));
		}
		std::vector<struct pollfd> snap = _pollSet;
		for (size_t i = 0; i < snap.size(); i++)
		{
			if (snap[i].revents & (POLLHUP | POLLERR))
			{
				if (snap[i].fd != _listenFd
					&& _sessions.find(snap[i].fd) != _sessions.end())
					dropClient(snap[i].fd);
				continue;
			}
			if (snap[i].revents & POLLIN)
			{
				if (snap[i].fd == _listenFd)
					welcomeClient();
				else if (_sessions.find(snap[i].fd) != _sessions.end())
					receiveData(snap[i].fd);
			}
		}
	}
}

// ── Helpers ─────────────────────────────────────────────────────────────────

void Server::transmit(int fd, const std::string& msg)
{
	send(fd, msg.c_str(), msg.size(), 0);
}

void Server::sendNumeric(Client* c, const std::string& code,
	const std::string& target, const std::string& body)
{
	transmit(c->socketFd(),
		":" + hostname() + " " + code + " " + target + " " + body + "\r\n");
}

Client* Server::locateUser(const std::string& nick)
{
	for (std::map<int, Client*>::iterator it = _sessions.begin();
		it != _sessions.end(); ++it)
	{
		if (it->second->getNick() == nick)
			return it->second;
	}
	return NULL;
}

Channel* Server::locateRoom(const std::string& name)
{
	std::map<std::string, Channel*>::iterator it = _rooms.find(name);
	return (it != _rooms.end()) ? it->second : NULL;
}

bool Server::isLegalNick(const std::string& nick)
{
	if (nick.empty() || nick.size() > 9)
		return false;
	if (!std::isalpha(nick[0]) && nick[0] != '_')
		return false;
	for (size_t i = 1; i < nick.size(); i++)
	{
		char c = nick[i];
		if (!std::isalnum(c) && c != '_' && c != '-'
			&& c != '[' && c != ']' && c != '\\'
			&& c != '`' && c != '^' && c != '{' && c != '}')
			return false;
	}
	return true;
}

void Server::listMembers(Client* c, Channel* ch)
{
	std::string names;
	const std::set<Client*>& grp = ch->getUsers();
	for (std::set<Client*>::const_iterator it = grp.begin();
		it != grp.end(); ++it)
	{
		if (!names.empty())
			names += " ";
		if (ch->isModerator(*it))
			names += "@";
		names += (*it)->getNick();
	}
	if (!names.empty())
		names += " ";
	names += _bot->getNick();
	sendNumeric(c, RPL::NAMREPLY, c->getNick(),
		"= " + ch->getLabel() + " :" + names);
	sendNumeric(c, RPL::ENDOFNAMES, c->getNick(),
		ch->getLabel() + " :End of /NAMES list");
}

void Server::notifyChannels(Client* c, const std::string& msg)
{
	std::set<int> done;
	for (std::map<std::string, Channel*>::iterator it = _rooms.begin();
		it != _rooms.end(); ++it)
	{
		if (!it->second->enrolled(c))
			continue;
		const std::set<Client*>& grp = it->second->getUsers();
		for (std::set<Client*>::const_iterator mi = grp.begin();
			mi != grp.end(); ++mi)
		{
			if (*mi != c && done.find((*mi)->socketFd()) == done.end())
			{
				transmit((*mi)->socketFd(), msg);
				done.insert((*mi)->socketFd());
			}
		}
	}
}

std::string Server::hostname() const { return "ircserv"; }

void Server::botReply(Client* c, const std::string& dest,
	const std::string& text)
{
	std::string reply;

	// ── Parse command name and argument ─────────────────────────────────
	std::string cmd;
	std::string arg;
	if (!text.empty() && text[0] == '!')
	{
		size_t sp = text.find(' ');
		if (sp == std::string::npos)
			cmd = text.substr(1);
		else
		{
			cmd = text.substr(1, sp - 1);
			arg = text.substr(sp + 1);
		}
		for (size_t i = 0; i < cmd.size(); i++)
			cmd[i] = std::tolower(cmd[i]);
	}

	// ── Data-rich commands (need server data) ───────────────────────────
	if (cmd == "info")
	{
		std::vector<std::string> nickList;
		for (std::map<int, Client*>::iterator it = _sessions.begin();
			it != _sessions.end(); ++it)
			nickList.push_back(it->second->getNick());

		std::vector<std::string> chanList;
		for (std::map<std::string, Channel*>::iterator it = _rooms.begin();
			it != _rooms.end(); ++it)
			chanList.push_back(it->first);

		reply = _bot->fmtInfo(_sessions.size(), _rooms.size(),
			nickList, chanList);
	}
	else if (cmd == "who")
	{
		if (arg.empty())
		{
			reply = "Usage: !who <#channel>[,#channel2,...]. Even that is apparently too complex.";
		}
		else
		{
			std::vector<std::string> targets = splitList(arg, ',');
			for (size_t t = 0; t < targets.size(); t++)
			{
				std::string chanName = targets[t];
				Channel* ch = locateRoom(chanName);
				if (!ch)
				{
					reply += chanName + ": No such channel. Another disappointment.\n";
					continue;
				}

				// Build user list with role prefixes
				std::vector<std::string> users;
				const std::set<Client*>& grp = ch->getUsers();
				for (std::set<Client*>::const_iterator it = grp.begin();
					it != grp.end(); ++it)
				{
					std::string entry;
					if (ch->isModerator(*it))
						entry += "@";
					entry += (*it)->getNick();
					users.push_back(entry);
				}
				users.push_back("*" + _bot->getNick());

				// Build mode string
				std::string modes;
				if (ch->flagInvite())
					modes += "i";
				if (ch->flagTopic())
					modes += "t";
				if (!ch->getPasskey().empty())
					modes += "k";
				if (ch->getCap() > 0)
					modes += "l";
				if (!modes.empty())
					modes = "+" + modes;

				// Whitelist
				std::vector<std::string> wl;
				const std::set<std::string>& wset = ch->getWhitelist();
				for (std::set<std::string>::const_iterator it = wset.begin();
					it != wset.end(); ++it)
					wl.push_back(*it);

				reply += _bot->fmtWho(chanName, users, modes,
					wl, ch->headcount(), ch->getCap());
			}
		}
	}
	else
	{
		// ── Simple commands (no server data needed) ─────────────────────
		reply = _bot->handleCommand(text);
	}

	if (reply.empty())
		return;

	std::string prefix = ":" + _bot->fullId() + " PRIVMSG " + dest + " :";
	std::istringstream ss(reply);
	std::string line;
	while (std::getline(ss, line))
	{
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);
		if (line.empty())
			continue;
		if (dest[0] == '#' || dest[0] == '&')
		{
			Channel* ch = locateRoom(dest);
			if (ch)
				ch->relay(prefix + line + "\r\n");
		}
		else
			transmit(c->socketFd(), prefix + line + "\r\n");
	}
}

std::vector<std::string> Server::splitList(const std::string& s, char sep)
{
	std::vector<std::string> out;
	std::istringstream ss(s);
	std::string tok;
	while (std::getline(ss, tok, sep))
	{
		if (!tok.empty())
			out.push_back(tok);
	}
	return out;
}
