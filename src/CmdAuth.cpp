#include "Irc.hpp"

void Server::execPass(Client* c, const std::vector<std::string>& args)
{
	if (c->isWelcomed())
	{
		sendNumeric(c, ERR::ALREADYREGISTERED, c->getNick(),
			":You may not reregister");
		return;
	}
	if (args.empty())
	{
		sendNumeric(c, ERR::NEEDMOREPARAMS, "*",
			"PASS :Not enough parameters");
		return;
	}
	if (args[0] == _connPass)
		c->markAuth(true);
	else
		sendNumeric(c, ERR::PASSWDMISMATCH, "*", ":Password incorrect");
}

void Server::execNick(Client* c, const std::vector<std::string>& args)
{
	std::string who = c->getNick().empty() ? "*" : c->getNick();

	if (args.empty())
	{
		sendNumeric(c, ERR::NONICKNAMEGIVEN, who, ":No nickname given");
		return;
	}
	std::string wanted = args[0];
	if (!isLegalNick(wanted))
	{
		sendNumeric(c, ERR::ERRONEUSNICKNAME, who,
			wanted + " :Erroneous nickname");
		return;
	}
	Client* clash = locateUser(wanted);
	if ((clash && clash != c) || wanted == _bot->getNick())
	{
		sendNumeric(c, ERR::NICKNAMEINUSE, who,
			wanted + " :Nickname is already in use");
		return;
	}

	if (c->isWelcomed())
	{
		std::string prev = c->fullId();
		c->changeNick(wanted);
		std::string note = ":" + prev + " NICK :" + wanted + "\r\n";
		transmit(c->socketFd(), note);
		notifyChannels(c, note);
	}
	else
	{
		c->changeNick(wanted);
		checkRegistration(c);
	}
}

void Server::execUser(Client* c, const std::vector<std::string>& args)
{
	std::string who = c->getNick().empty() ? "*" : c->getNick();

	if (c->isWelcomed())
	{
		sendNumeric(c, ERR::ALREADYREGISTERED, c->getNick(),
			":You may not reregister");
		return;
	}
	if (args.size() < 4)
	{
		sendNumeric(c, ERR::NEEDMOREPARAMS, who,
			"USER :Not enough parameters");
		return;
	}
	c->changeUser(args[0]);
	c->changeFullname(args[3]);
	checkRegistration(c);
}

void Server::execQuit(Client* c, const std::vector<std::string>& args)
{
	std::string reason = args.empty() ? "Leaving" : args[0];
	std::string msg = ":" + c->fullId() + " QUIT :Quit: " + reason + "\r\n";
	transmit(c->socketFd(), "ERROR :Closing link (" + c->fullId()
		+ ") [Quit: " + reason + "]\r\n");
	notifyChannels(c, msg);
	dropClient(c->socketFd());
}

void Server::execPing(Client* c, const std::vector<std::string>& args)
{
	if (args.empty())
	{
		sendNumeric(c, ERR::NEEDMOREPARAMS, c->getNick(),
			"PING :Not enough parameters");
		return;
	}
	transmit(c->socketFd(), ":" + hostname() + " PONG "
		+ hostname() + " :" + args[0] + "\r\n");
}
