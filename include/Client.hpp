#ifndef CLIENT_HPP
# define CLIENT_HPP

# include "Irc.hpp"

class Client
{
	private:

		int			_sockFd;
		std::string	_nick;
		std::string	_user;
		std::string	_fullname;
		std::string	_host;
		std::string	_recvBuf;
		bool		_authorized;
		bool		_welcomed;

	public:

		Client();
		Client(int fd);
		Client(const Client& src);
		Client& operator=(const Client& src);
		~Client();

		int					socketFd() const;
		const std::string&	getNick() const;
		const std::string&	getUser() const;
		const std::string&	getFullname() const;
		const std::string&	getHost() const;
		bool				hasAuth() const;
		bool				isWelcomed() const;
		std::string&		recvBuf();

		void	changeNick(const std::string& val);
		void	changeUser(const std::string& val);
		void	changeFullname(const std::string& val);
		void	changeHost(const std::string& val);
		void	markAuth(bool val);
		void	markWelcomed(bool val);

		bool		bufferAppend(const std::string& chunk);
		void		bufferClear();
		std::string	fullId() const;

};

#endif
