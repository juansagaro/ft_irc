#ifndef CHANNEL_HPP
# define CHANNEL_HPP

# include "Irc.hpp"

class Channel
{
	private:

		std::string				_label;
		std::string				_subject;
		std::string				_passkey;
		std::set<Client*>		_users;
		std::set<Client*>		_moderators;
		std::set<std::string>	_whitelist;
		bool					_modeI;
		bool					_modeT;
		int						_cap;

	public:

		Channel();
		Channel(const std::string& name);
		Channel(const Channel& src);
		Channel& operator=(const Channel& src);
		~Channel();

		const std::string&			getLabel() const;
		const std::string&			getSubject() const;
		const std::string&			getPasskey() const;
		const std::set<Client*>&	getUsers() const;
		bool						flagInvite() const;
		bool						flagTopic() const;
		int							getCap() const;
		const std::set<std::string>&	getWhitelist() const;

		void	changeSubject(const std::string& val);
		void	changePasskey(const std::string& val);
		void	toggleInvite(bool val);
		void	toggleTopic(bool val);
		void	changeCap(int val);

		void	enroll(Client* c);
		void	dismiss(Client* c);
		bool	enrolled(Client* c) const;
		size_t	headcount() const;
		bool	vacant() const;

		void	promote(Client* c);
		void	demote(Client* c);
		bool	isModerator(Client* c) const;
		bool	hasOps() const;
		Client*	firstUser() const;

		void	allow(const std::string& nick);
		void	revoke(const std::string& nick);
		bool	isAllowed(const std::string& nick) const;

		void	relay(const std::string& msg, Client* skip = NULL);

};

#endif
