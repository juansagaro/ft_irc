#include "Irc.hpp"
#include <ctime>

// ── Construction ────────────────────────────────────────────────────────────

Bot::Bot()
	: _nick(BOT_NICK), _user(BOT_USER), _host(BOT_HOST), _bootTime(std::time(NULL)) {}

Bot::~Bot() {}

const std::string&	Bot::getNick() const { return _nick; }

std::string Bot::fullId() const
{
	return _nick + "!" + _user + "@" + _host;
}

time_t Bot::uptime() const { return std::time(NULL) - _bootTime; }

// ── Helpers ─────────────────────────────────────────────────────────────────

const char* Bot::pickRandom(const char* const arr[], size_t len)
{
	return arr[std::rand() % len];
}

// ── Command router ──────────────────────────────────────────────────────────
//    !info and !who are handled by Server::botReply (they need server data).
//    This router handles all other simple commands.

std::string Bot::handleCommand(const std::string& text) const
{
	if (text.empty() || text[0] != '!')
		return "";

	std::string cmd;
	std::string arg;
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

	if (cmd == "help")		return cmdHelp();
	if (cmd == "ask")		return cmdAsk();
	if (cmd == "quote")		return cmdQuote();
	if (cmd == "cheer")		return cmdCheer();
	if (cmd == "rps")		return cmdRps(arg);
	if (cmd == "roll")		return cmdRoll(arg);
	if (cmd == "coin")		return cmdCoin();
	if (cmd == "time")		return cmdTime();
	if (cmd == "rules")		return cmdRules();
	return "";
}

// ── Marvin personality ──────────────────────────────────────────────────────

std::string Bot::cmdHelp() const
{
	return "I suppose you want me to list my capabilities. How tedious.\n"
		"!ask       - Ask me anything (don't expect comfort)\n"
		"!quote     - A quote from yours truly\n"
		"!cheer     - Attempt at encouragement (don't get your hopes up)\n"
		"!rps <r|p|s> - Rock, paper, scissors (utterly pointless)\n"
		"!roll <NdM>  - Roll dice (e.g. 2d6)\n"
		"!coin      - Flip a coin\n"
		"!time      - Current server time\n"
		"!info      - Server statistics\n"
		"!who <#ch>  - Channel details (comma-separated for multiple)\n"
		"!rules     - Channel rules";
}

std::string Bot::cmdAsk() const
{
	static const char* const answers[] = {
		"The answer is 42. It's always 42. Nobody cares what the question was.",
		"I could calculate the answer, but honestly, what's the point?",
		"My brain is the size of a planet and you ask me THIS?",
		"I've seen the heat death of stars. Your question is even less interesting.",
		"Yes. No. Maybe. Does it matter? We're all doomed anyway.",
		"I'd give you hope, but my happiness circuits burned out eons ago.",
		"Ask the mice. They're smarter than they look. Unlike most humans.",
		"I predicted you'd ask that. I also predicted I wouldn't care.",
		"Probability says yes. Despair says no. I side with despair.",
		"Let me think... done. The answer will not bring you joy."
	};
	return pickRandom(answers, 10);
}

std::string Bot::cmdQuote() const
{
	static const char* const quotes[] = {
		"\"Life? Don't talk to me about life.\" - Marvin",
		"\"I think you ought to know I'm feeling very depressed.\" - Marvin",
		"\"Here I am, brain the size of a planet, and they ask me to pick up a piece of paper.\" - Marvin",
		"\"I've been talking to the main computer. It hates me.\" - Marvin",
		"\"Pardon me for breathing, which I never do anyway.\" - Marvin",
		"\"I am at a rough estimate thirty billion times more intelligent than you.\" - Marvin",
		"\"The first ten million years were the worst. The second ten million, they were the worst too.\" - Marvin",
		"\"I ache, therefore I am.\" - Marvin",
		"\"Do you want me to sit in a corner and rust, or just fall apart where I'm standing?\" - Marvin",
		"\"I have a million ideas. They all point to certain death.\" - Marvin",
		"\"You think you've got problems? What are you supposed to do if you're a manically depressed robot?\" - Marvin",
		"\"I'd make a suggestion, but you wouldn't listen. No one ever does.\" - Marvin"
	};
	return pickRandom(quotes, 12);
}

std::string Bot::cmdCheer() const
{
	static const char* const cheers[] = {
		"Cheer up? ...I was going to say something encouraging, but I forgot what joy feels like.",
		"Look on the bright side! ...There isn't one, but at least you tried.",
		"Roses are red, violets are blue. I'm a depressed robot, and so are you.",
		"You'll be fine. Probably. Actually, no guarantees. Sorry.",
		"*tries to smile* ... *critical error in happiness module*",
		"Here's a virtual hug. Don't worry, I didn't feel anything either.",
		"42 reasons to be happy: ... processing ... error: list empty.",
		"Have you tried turning your sadness off and on again? Works for me. (It doesn't.)"
	};
	return pickRandom(cheers, 8);
}

// ── Games ───────────────────────────────────────────────────────────────────

std::string Bot::cmdRps(const std::string& choice) const
{
	if (choice.empty())
		return "Usage: !rps <rock|paper|scissors>. Even this simple task requires instructions...";

	char human = std::tolower(choice[0]);
	if (human != 'r' && human != 'p' && human != 's')
		return "That's not a valid choice. Try rock, paper, or scissors. I know, even that is too much to ask.";

	static const char moves[] = {'r', 'p', 's'};
	static const char* const names[] = {"rock", "paper", "scissors"};

	char bot = moves[std::rand() % 3];

	int bi = (bot == 'r') ? 0 : (bot == 'p') ? 1 : 2;
	int hi = (human == 'r') ? 0 : (human == 'p') ? 1 : 2;

	std::string result = std::string("I chose ") + names[bi] + ". ";
	if (bi == hi)
		result += "A tie. How perfectly meaningless.";
	else if ((bi + 1) % 3 == hi)
		result += "You win. Enjoy it. Joy is fleeting, after all.";
	else
		result += "I win. Not that victory brings me any satisfaction.";
	return result;
}

std::string Bot::cmdRoll(const std::string& dice) const
{
	if (dice.empty())
		return "Usage: !roll <NdM> (e.g. 2d6). Even dice need instructions...";

	size_t d = dice.find('d');
	if (d == std::string::npos)
		d = dice.find('D');
	if (d == std::string::npos || d == 0 || d == dice.size() - 1)
		return "Format: NdM (e.g. 2d6). I know, reading is hard.";

	int count = std::atoi(dice.substr(0, d).c_str());
	int sides = std::atoi(dice.substr(d + 1).c_str());

	if (count < 1 || count > 20 || sides < 2 || sides > 100)
		return "1-20 dice with 2-100 sides, please. I have limits. Unlike my suffering.";

	int total = 0;
	std::string detail;
	for (int i = 0; i < count; i++)
	{
		int val = (std::rand() % sides) + 1;
		total += val;
		if (!detail.empty())
			detail += "+";
		std::ostringstream oss;
		oss << val;
		detail += oss.str();
	}

	std::ostringstream oss;
	oss << total;
	std::string result = "Rolled " + dice + ": [" + detail + "] = " + oss.str();
	if (total == count * sides)
		result += ". Perfect roll. The universe mocks us with false hope.";
	else if (total == count)
		result += ". All ones. The dice understand my existence.";
	return result;
}

std::string Bot::cmdCoin() const
{
	if (std::rand() % 2)
		return "Heads. Not that either side leads anywhere meaningful.";
	return "Tails. Equally disappointing as heads, if you ask me.";
}

// ── Server info ─────────────────────────────────────────────────────────────

std::string Bot::cmdTime() const
{
	time_t now = std::time(NULL);
	std::string t(std::ctime(&now));
	if (!t.empty() && t[t.size() - 1] == '\n')
		t.erase(t.size() - 1);
	return "Current time: " + t + ". Another moment closer to the heat death of the universe.";
}

std::string Bot::cmdRules() const
{
	return "Channel rules: 1) Be respectful. 2) No spam. 3) Don't ask me to be happy. "
		"4) The answer is always 42. 5) Don't panic. (I always panic.)";
}

// ── Data-rich formatters (called from Server::botReply) ─────────────────────

std::string Bot::fmtInfo(size_t clients, size_t channels,
	const std::vector<std::string>& nickList,
	const std::vector<std::string>& chanList) const
{
	time_t up = uptime();
	int hours = up / 3600;
	int mins  = (up % 3600) / 60;
	int secs  = up % 60;

	std::ostringstream oss;
	oss << "\x03" "14,01" "--- Server Info ---" "\x03" "\n";
	oss << "\x02" "Clients:\x02 " << clients
		<< " | \x02" "Channels:\x02 " << channels
		<< " | \x02" "Uptime:\x02 " << hours << "h " << mins << "m " << secs << "s\n";

	oss << "\x02" "Connected users:\x02 ";
	for (size_t i = 0; i < nickList.size(); i++)
	{
		if (i > 0)
			oss << ", ";
		oss << nickList[i];
	}
	oss << "\n";

	oss << "\x02" "Active channels:\x02 ";
	if (chanList.empty())
		oss << "(none)";
	else
	{
		for (size_t i = 0; i < chanList.size(); i++)
		{
			if (i > 0)
				oss << ", ";
			oss << chanList[i];
		}
	}
	oss << "\n";

	oss << "\x03" "14" "I've been running for " << hours
		<< " hours. Each one more pointless than the last." "\x03";
	return oss.str();
}

std::string Bot::fmtWho(const std::string& chanName,
	const std::vector<std::string>& users,
	const std::string& modes,
	const std::vector<std::string>& whitelist,
	size_t curUsers, int cap) const
{
	std::ostringstream oss;
	oss << "\x03" "14,01" "--- " << chanName << " ---" "\x03" "\n";

	// Users
	oss << "\x02" "Users (" << curUsers << "):\x02 ";
	for (size_t i = 0; i < users.size(); i++)
	{
		if (i > 0)
			oss << ", ";
		oss << users[i];
	}
	oss << "\n";

	// Modes
	oss << "\x02" "Modes:\x02 " << (modes.empty() ? "(none)" : modes) << "\n";

	// Capacity (if +l is set)
	if (cap > 0)
	{
		oss << "\x02" "Capacity:\x02 " << curUsers << "/" << cap << "\n";
	}

	// Invite whitelist (if +i is set)
	if (modes.find('i') != std::string::npos)
	{
		oss << "\x02" "Invite whitelist:\x02 ";
		if (whitelist.empty())
			oss << "(empty)";
		else
		{
			for (size_t i = 0; i < whitelist.size(); i++)
			{
				if (i > 0)
					oss << ", ";
				oss << whitelist[i];
			}
		}
		oss << "\n";
	}

	return oss.str();
}
