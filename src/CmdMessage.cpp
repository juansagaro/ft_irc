#include "Irc.hpp"

// ── PRIVMSG ─────────────────────────────────────────────────────────────────

void Server::execPrivmsg(Client* c, const std::vector<std::string>& args)
{
	if (args.empty())
	{
		sendNumeric(c, ERR::NORECIPIENT, c->getNick(),
			":No recipient given (PRIVMSG)");
		return;
	}
	if (args.size() < 2)
	{
		sendNumeric(c, ERR::NOTEXTTOSEND, c->getNick(), ":No text to send");
		return;
	}

	std::string dest = args[0];
	std::string text = args[1];

	if (dest[0] == '#' || dest[0] == '&')
	{
		Channel* ch = locateRoom(dest);
		if (!ch)
		{
			sendNumeric(c, ERR::NOSUCHCHANNEL, c->getNick(),
				dest + " :No such channel");
			return;
		}
		if (!ch->enrolled(c))
		{
			sendNumeric(c, ERR::CANNOTSENDTOCHAN, c->getNick(),
				dest + " :Cannot send to channel");
			return;
		}
		ch->relay(":" + c->fullId() + " PRIVMSG "
			+ dest + " :" + text + "\r\n", c);
		if (!text.empty() && text[0] == '!')
			botReply(c, dest, text);
	}
	else if (dest == _bot->getNick())
	{
		botReply(c, c->getNick(), text);
	}
	else
	{
		Client* target = locateUser(dest);
		if (!target)
		{
			sendNumeric(c, ERR::NOSUCHNICK, c->getNick(),
				dest + " :No such nick/channel");
			return;
		}
		transmit(target->socketFd(), ":" + c->fullId() + " PRIVMSG "
			+ dest + " :" + text + "\r\n");
	}
}

// ── NOTICE ──────────────────────────────────────────────────────────────────

void Server::execNotice(Client* c, const std::vector<std::string>& args)
{
	if (args.size() < 2)
		return;

	std::string dest = args[0];
	std::string text = args[1];

	if (dest[0] == '#' || dest[0] == '&')
	{
		Channel* ch = locateRoom(dest);
		if (!ch || !ch->enrolled(c))
			return;
		ch->relay(":" + c->fullId() + " NOTICE "
			+ dest + " :" + text + "\r\n", c);
	}
	else
	{
		Client* target = locateUser(dest);
		if (!target)
			return;
		transmit(target->socketFd(), ":" + c->fullId() + " NOTICE "
			+ dest + " :" + text + "\r\n");
	}
}

// ── KICK ────────────────────────────────────────────────────────────────────

void Server::execKick(Client* c, const std::vector<std::string>& args)
{
	if (args.size() < 2)
	{
		sendNumeric(c, ERR::NEEDMOREPARAMS, c->getNick(),
			"KICK :Not enough parameters");
		return;
	}

	std::string rn = args[0];
	std::string target = args[1];
	std::string reason = args.size() > 2 ? args[2] : c->getNick();

	Channel* ch = locateRoom(rn);
	if (!ch)
	{
		sendNumeric(c, ERR::NOSUCHCHANNEL, c->getNick(),
			rn + " :No such channel");
		return;
	}
	if (!ch->enrolled(c))
	{
		sendNumeric(c, ERR::NOTONCHANNEL, c->getNick(),
			rn + " :You're not on that channel");
		return;
	}
	if (!ch->isModerator(c))
	{
		sendNumeric(c, ERR::CHANOPRIVSNEEDED, c->getNick(),
			rn + " :You're not channel operator");
		return;
	}

	Client* victim = locateUser(target);
	if (!victim || !ch->enrolled(victim))
	{
		sendNumeric(c, ERR::USERNOTINCHANNEL, c->getNick(),
			target + " " + rn + " :They aren't on that channel");
		return;
	}

	ch->relay(":" + c->fullId() + " KICK " + rn
		+ " " + target + " :" + reason + "\r\n");
	ch->dismiss(victim);
	if (ch->vacant())
	{
		delete ch;
		_rooms.erase(rn);
	}
	else
		ensureOp(ch);
}

// ── INVITE ──────────────────────────────────────────────────────────────────

void Server::execInvite(Client* c, const std::vector<std::string>& args)
{
	if (args.size() < 2)
	{
		sendNumeric(c, ERR::NEEDMOREPARAMS, c->getNick(),
			"INVITE :Not enough parameters");
		return;
	}

	std::string target = args[0];
	std::string rn = args[1];

	Client* who = locateUser(target);
	if (!who)
	{
		sendNumeric(c, ERR::NOSUCHNICK, c->getNick(),
			target + " :No such nick/channel");
		return;
	}
	Channel* ch = locateRoom(rn);
	if (!ch)
	{
		sendNumeric(c, ERR::NOSUCHCHANNEL, c->getNick(),
			rn + " :No such channel");
		return;
	}
	if (!ch->enrolled(c))
	{
		sendNumeric(c, ERR::NOTONCHANNEL, c->getNick(),
			rn + " :You're not on that channel");
		return;
	}
	if (ch->enrolled(who))
	{
		sendNumeric(c, ERR::USERONCHANNEL, c->getNick(),
			target + " " + rn + " :is already on channel");
		return;
	}
	if (ch->flagInvite() && !ch->isModerator(c))
	{
		sendNumeric(c, ERR::CHANOPRIVSNEEDED, c->getNick(),
			rn + " :You're not channel operator");
		return;
	}

	ch->allow(who->getNick());
	sendNumeric(c, RPL::INVITING, c->getNick(), target + " " + rn);
	transmit(who->socketFd(), ":" + c->fullId()
		+ " INVITE " + target + " " + rn + "\r\n");
}
