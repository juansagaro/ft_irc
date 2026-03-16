#ifndef SERVER_HPP
# define SERVER_HPP

# include "Irc.hpp"

class Server
{
	private:

		int								_listenPort;
		std::string						_connPass;
		int								_listenFd;
		std::vector<struct pollfd>		_pollSet;
		std::map<int, Client*>			_sessions;
		std::map<std::string, Channel*>	_rooms;
		Bot*							_bot;

		Server();
		Server(const Server&);
		Server& operator=(const Server&);

		// Network.cpp
		void	openSocket();
		void	welcomeClient();
		void	receiveData(int fd);
		void	dropClient(int fd);

		// Parse.cpp
		void	routeMessage(Client* c, const std::string& raw);
		void	checkRegistration(Client* c);

		// CmdAuth.cpp
		void	execPass(Client* c, const std::vector<std::string>& args);
		void	execNick(Client* c, const std::vector<std::string>& args);
		void	execUser(Client* c, const std::vector<std::string>& args);
		void	execQuit(Client* c, const std::vector<std::string>& args);
		void	execPing(Client* c, const std::vector<std::string>& args);

		// CmdChannel.cpp
		void	execJoin(Client* c, const std::vector<std::string>& args);
		void	execPart(Client* c, const std::vector<std::string>& args);
		void	execTopic(Client* c, const std::vector<std::string>& args);
		void	execMode(Client* c, const std::vector<std::string>& args);
		void	enterRoom(Client* c, const std::string& name, const std::string& key);
		void	applyModeI(Channel* ch, bool plus, std::string& out);
		void	applyModeT(Channel* ch, bool plus, std::string& out);
		void	applyModeK(Client* c, Channel* ch, bool plus, const std::vector<std::string>& args, size_t& pi, std::string& out, std::string& outArgs);
		void	applyModeO(Client* c, Channel* ch, bool plus, const std::vector<std::string>& args, size_t& pi, std::string& out, std::string& outArgs);
		void	applyModeL(Client* c, Channel* ch, bool plus, const std::vector<std::string>& args, size_t& pi, std::string& out, std::string& outArgs);

		// CmdMessage.cpp
		void	execPrivmsg(Client* c, const std::vector<std::string>& args);
		void	execNotice(Client* c, const std::vector<std::string>& args);
		void	execKick(Client* c, const std::vector<std::string>& args);
		void	execInvite(Client* c, const std::vector<std::string>& args);

		// Helpers (Server.cpp)
		void		transmit(int fd, const std::string& msg);
		void		sendNumeric(Client* c, const std::string& code, const std::string& target, const std::string& body);
		Client*		locateUser(const std::string& nick);
		Channel*	locateRoom(const std::string& name);
		bool		isLegalNick(const std::string& nick);
		void		listMembers(Client* c, Channel* ch);
		void		notifyChannels(Client* c, const std::string& msg);
		std::string	hostname() const;

		void		botReply(Client* c, const std::string& dest, const std::string& text);

		static std::vector<std::string> splitList(const std::string& s, char sep);

	public:

		Server(int port, const std::string& pass);
		~Server();

		void	boot();
		void	run();

};

#endif
