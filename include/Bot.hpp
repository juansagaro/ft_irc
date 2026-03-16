#ifndef BOT_HPP
# define BOT_HPP

# include "Irc.hpp"

# define BOT_NICK "Marvin"
# define BOT_USER "marvin"
# define BOT_HOST "hitchhiker.galaxy"

class Bot
{
	private:

		std::string	_nick;
		std::string	_user;
		std::string	_host;
		time_t		_bootTime;

		Bot(const Bot&);
		Bot& operator=(const Bot&);

		// Command handlers (Bot.cpp)
		std::string	cmdHelp() const;
		std::string	cmdAsk() const;
		std::string	cmdQuote() const;
		std::string	cmdCheer() const;
		std::string	cmdRps(const std::string& choice) const;
		std::string	cmdRoll(const std::string& dice) const;
		std::string	cmdCoin() const;
		std::string	cmdTime() const;
		std::string	cmdRules() const;

		static const char*	pickRandom(const char* const arr[], size_t len);

	public:

		Bot();
		~Bot();

		const std::string&	getNick() const;
		std::string			fullId() const;
		time_t				uptime() const;

		// Simple commands (no server data needed)
		std::string	handleCommand(const std::string& text) const;

		// Data-rich commands (called from Server::botReply with server data)
		std::string	fmtInfo(size_t clients, size_t channels,
						const std::vector<std::string>& nickList,
						const std::vector<std::string>& chanList) const;
		std::string	fmtWho(const std::string& chanName,
						const std::vector<std::string>& users,
						const std::string& modes,
						const std::vector<std::string>& whitelist,
						size_t curUsers, int cap) const;

};

#endif
