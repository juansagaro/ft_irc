#include "Irc.hpp"

// ── JOIN ────────────────────────────────────────────────────────────────────

void Server::execJoin(Client* c, const std::vector<std::string>& args)
{
	if (args.empty())
	{
		sendNumeric(c, ERR::NEEDMOREPARAMS, c->getNick(),
			"JOIN :Not enough parameters");
		return;
	}
	std::vector<std::string> names = splitList(args[0], ',');
	std::vector<std::string> keys;
	if (args.size() > 1)
		keys = splitList(args[1], ',');
	for (size_t i = 0; i < names.size(); i++)
		enterRoom(c, names[i], (i < keys.size()) ? keys[i] : "");
}

void Server::enterRoom(Client* c, const std::string& name,
	const std::string& key)
{
	if (name.empty() || (name[0] != '#' && name[0] != '&'))
	{
		sendNumeric(c, ERR::BADCHANMASK, c->getNick(),
			name + " :Bad Channel Mask");
		return;
	}
	Channel* ch = locateRoom(name);
	bool fresh = false;
	if (!ch)
	{
		ch = new Channel(name);
		_rooms[name] = ch;
		fresh = true;
	}
	if (ch->enrolled(c))
		return;
	if (ch->flagInvite() && !ch->isAllowed(c->getNick()))
	{
		sendNumeric(c, ERR::INVITEONLYCHAN, c->getNick(),
			name + " :Cannot join channel (+i)");
		return;
	}
	if (!ch->getPasskey().empty() && ch->getPasskey() != key)
	{
		sendNumeric(c, ERR::BADCHANNELKEY, c->getNick(),
			name + " :Cannot join channel (+k)");
		return;
	}
	if (ch->getCap() > 0 && (int)ch->headcount() >= ch->getCap())
	{
		sendNumeric(c, ERR::CHANNELISFULL, c->getNick(),
			name + " :Cannot join channel (+l)");
		return;
	}

	ch->enroll(c);
	if (fresh)
		ch->promote(c);
	ch->revoke(c->getNick());
	ch->relay(":" + c->fullId() + " JOIN " + name + "\r\n");

	if (!ch->getSubject().empty())
		sendNumeric(c, RPL::TOPIC, c->getNick(),
			name + " :" + ch->getSubject());
	else
		sendNumeric(c, RPL::NOTOPIC, c->getNick(),
			name + " :No topic is set");
	listMembers(c, ch);
}

// ── PART ────────────────────────────────────────────────────────────────────

void Server::execPart(Client* c, const std::vector<std::string>& args)
{
	if (args.empty())
	{
		sendNumeric(c, ERR::NEEDMOREPARAMS, c->getNick(),
			"PART :Not enough parameters");
		return;
	}
	std::vector<std::string> names = splitList(args[0], ',');
	std::string reason = args.size() > 1 ? args[1] : "";

	for (size_t i = 0; i < names.size(); i++)
	{
		Channel* ch = locateRoom(names[i]);
		if (!ch)
		{
			sendNumeric(c, ERR::NOSUCHCHANNEL, c->getNick(),
				names[i] + " :No such channel");
			continue;
		}
		if (!ch->enrolled(c))
		{
			sendNumeric(c, ERR::NOTONCHANNEL, c->getNick(),
				names[i] + " :You're not on that channel");
			continue;
		}
		std::string msg = ":" + c->fullId() + " PART " + names[i];
		if (!reason.empty())
			msg += " :" + reason;
		msg += "\r\n";
		ch->relay(msg);
		ch->dismiss(c);
		if (ch->vacant())
		{
			delete ch;
			_rooms.erase(names[i]);
		}
		else
			ensureOp(ch);
	}
}

// ── TOPIC ───────────────────────────────────────────────────────────────────

void Server::execTopic(Client* c, const std::vector<std::string>& args)
{
	if (args.empty())
	{
		sendNumeric(c, ERR::NEEDMOREPARAMS, c->getNick(),
			"TOPIC :Not enough parameters");
		return;
	}
	std::string rn = args[0];
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
	if (args.size() == 1)
	{
		if (ch->getSubject().empty())
			sendNumeric(c, RPL::NOTOPIC, c->getNick(),
				rn + " :No topic is set");
		else
			sendNumeric(c, RPL::TOPIC, c->getNick(),
				rn + " :" + ch->getSubject());
		return;
	}
	if (ch->flagTopic() && !ch->isModerator(c))
	{
		sendNumeric(c, ERR::CHANOPRIVSNEEDED, c->getNick(),
			rn + " :You're not channel operator");
		return;
	}
	ch->changeSubject(args[1]);
	ch->relay(":" + c->fullId() + " TOPIC " + rn + " :" + args[1] + "\r\n");
}

// ── MODE (decomposed) ──────────────────────────────────────────────────────

void Server::applyModeI(Channel* ch, bool plus, std::string& out)
{
	ch->toggleInvite(plus);
	out += plus ? "+i" : "-i";
}

void Server::applyModeT(Channel* ch, bool plus, std::string& out)
{
	ch->toggleTopic(plus);
	out += plus ? "+t" : "-t";
}

void Server::applyModeK(Client* c, Channel* ch, bool plus,
	const std::vector<std::string>& args,
	size_t& pi, std::string& out, std::string& outArgs)
{
	if (plus)
	{
		if (pi >= args.size())
		{
			sendNumeric(c, ERR::NEEDMOREPARAMS, c->getNick(),
				"MODE :Not enough parameters");
			return;
		}
		ch->changePasskey(args[pi]);
		out += "+k";
		outArgs += " " + args[pi];
		pi++;
	}
	else
	{
		ch->changePasskey("");
		out += "-k";
	}
}

void Server::applyModeO(Client* c, Channel* ch, bool plus,
	const std::vector<std::string>& args,
	size_t& pi, std::string& out, std::string& outArgs)
{
	if (pi >= args.size())
	{
		sendNumeric(c, ERR::NEEDMOREPARAMS, c->getNick(),
			"MODE :Not enough parameters");
		return;
	}
	Client* who = locateUser(args[pi]);
	if (!who)
	{
		sendNumeric(c, ERR::NOSUCHNICK, c->getNick(),
			args[pi] + " :No such nick/channel");
		pi++;
		return;
	}
	if (!ch->enrolled(who))
	{
		sendNumeric(c, ERR::USERNOTINCHANNEL, c->getNick(),
			args[pi] + " " + ch->getLabel()
			+ " :They aren't on that channel");
		pi++;
		return;
	}
	if (plus)
		ch->promote(who);
	else
		ch->demote(who);
	out += plus ? "+o" : "-o";
	outArgs += " " + args[pi];
	pi++;
}

void Server::applyModeL(Client* c, Channel* ch, bool plus,
	const std::vector<std::string>& args,
	size_t& pi, std::string& out, std::string& outArgs)
{
	if (plus)
	{
		if (pi >= args.size())
		{
			sendNumeric(c, ERR::NEEDMOREPARAMS, c->getNick(),
				"MODE :Not enough parameters");
			return;
		}
		int val = std::atoi(args[pi].c_str());
		if (val <= 0)
		{
			pi++;
			return;
		}
		ch->changeCap(val);
		out += "+l";
		outArgs += " " + args[pi];
		pi++;
	}
	else
	{
		ch->changeCap(0);
		out += "-l";
	}
}

void Server::execMode(Client* c, const std::vector<std::string>& args)
{
	if (args.empty())
	{
		sendNumeric(c, ERR::NEEDMOREPARAMS, c->getNick(),
			"MODE :Not enough parameters");
		return;
	}
	std::string rn = args[0];
	if (rn[0] != '#' && rn[0] != '&')
		return;

	Channel* ch = locateRoom(rn);
	if (!ch)
	{
		sendNumeric(c, ERR::NOSUCHCHANNEL, c->getNick(),
			rn + " :No such channel");
		return;
	}

	if (args.size() == 1)
	{
		std::string flags = "+";
		std::string extra;
		if (ch->flagInvite()) flags += "i";
		if (ch->flagTopic())  flags += "t";
		if (!ch->getPasskey().empty())
		{
			flags += "k";
			extra += " " + ch->getPasskey();
		}
		if (ch->getCap() > 0)
		{
			flags += "l";
			std::ostringstream oss;
			oss << ch->getCap();
			extra += " " + oss.str();
		}
		sendNumeric(c, RPL::CHANNELMODEIS, c->getNick(),
			rn + " " + flags + extra);
		return;
	}

	if (!ch->isModerator(c))
	{
		sendNumeric(c, ERR::CHANOPRIVSNEEDED, c->getNick(),
			rn + " :You're not channel operator");
		return;
	}

	std::string flagStr = args[1];
	bool plus = true;
	size_t pi = 2;
	std::string applied, appliedArgs;

	for (size_t i = 0; i < flagStr.size(); i++)
	{
		char f = flagStr[i];
		if (f == '+') { plus = true;  continue; }
		if (f == '-') { plus = false; continue; }

		if      (f == 'i') applyModeI(ch, plus, applied);
		else if (f == 't') applyModeT(ch, plus, applied);
		else if (f == 'k') applyModeK(c, ch, plus, args, pi, applied, appliedArgs);
		else if (f == 'o') applyModeO(c, ch, plus, args, pi, applied, appliedArgs);
		else if (f == 'l') applyModeL(c, ch, plus, args, pi, applied, appliedArgs);
		else
		{
			std::string s(1, f);
			sendNumeric(c, ERR::UNKNOWNMODE, c->getNick(),
				s + " :is unknown mode char to me");
		}
	}

	if (!applied.empty())
		ch->relay(":" + c->fullId() + " MODE " + rn
			+ " " + applied + appliedArgs + "\r\n");
}
