#include "Irc.hpp"

void Server::routeMessage(Client* c, const std::string& raw)
{
	std::string line = raw;

	// could be more efficient
	while (!line.empty() && line[0] == ' ')
		line.erase(0, 1);
	if (line.empty())
		return;
	if (line[0] == ':')
	{
		size_t sp = line.find(' ');
		if (sp == std::string::npos)
			return;
		line = line.substr(sp + 1);
	}
	while (!line.empty() && line[0] == ' ')
		line.erase(0, 1);

	std::string verb;
	std::vector<std::string> args;

	while (!line.empty())
	{
		if (line[0] == ':')
		{
			args.push_back(line.substr(1));
			break;
		}
		size_t sp = line.find(' ');
		std::string tok;
		if (sp == std::string::npos)
		{
			tok = line;
			line.clear();
		}
		else
		{
			tok = line.substr(0, sp);
			line = line.substr(sp + 1);
			while (!line.empty() && line[0] == ' ')
				line.erase(0, 1);
		}
		if (verb.empty())
			verb = tok;
		else
			args.push_back(tok);
	}

	for (size_t i = 0; i < verb.size(); i++)
		verb[i] = std::toupper(verb[i]);

	if (verb == "CAP" || verb == "PONG")
		return;

	std::string who = c->getNick().empty() ? "*" : c->getNick();

	if (!c->isWelcomed()
		&& verb != "PASS" && verb != "NICK"
		&& verb != "USER" && verb != "QUIT")
	{
		sendNumeric(c, ERR::NOTREGISTERED, who, ":You have not registered");
		return;
	}

	if (verb == "PASS")			execPass(c, args);
	else if (verb == "NICK")	execNick(c, args);
	else if (verb == "USER")	execUser(c, args);
	else if (verb == "JOIN")	execJoin(c, args);
	else if (verb == "PART")	execPart(c, args);
	else if (verb == "PRIVMSG")	execPrivmsg(c, args);
	else if (verb == "NOTICE")	execNotice(c, args);
	else if (verb == "KICK")	execKick(c, args);
	else if (verb == "INVITE")	execInvite(c, args);
	else if (verb == "TOPIC")	execTopic(c, args);
	else if (verb == "MODE")	execMode(c, args);
	else if (verb == "QUIT")	execQuit(c, args);
	else if (verb == "PING")	execPing(c, args);
	else if (c->isWelcomed())
		sendNumeric(c, ERR::UNKNOWNCOMMAND, c->getNick(),
			verb + " :Unknown command");
}

void Server::checkRegistration(Client* c)
{
	if (c->isWelcomed())
		return;
	if (!c->hasAuth() || c->getNick().empty() || c->getUser().empty())
		return;

	c->markWelcomed(true);
	sendNumeric(c, RPL::WELCOME, c->getNick(),
		":Welcome to the IRC Network " + c->fullId());
	sendNumeric(c, RPL::YOURHOST, c->getNick(),
		":Your host is " + hostname() + ", running version 1.0");
	sendNumeric(c, RPL::CREATED, c->getNick(),
		":This server was created today");
	sendNumeric(c, RPL::MYINFO, c->getNick(),
		hostname() + " 1.0 o itkol");
}
