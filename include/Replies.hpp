#ifndef REPLIES_HPP
# define REPLIES_HPP

# include <string>

namespace RPL
{
	static const std::string WELCOME       = "001";
	static const std::string YOURHOST      = "002";
	static const std::string CREATED       = "003";
	static const std::string MYINFO        = "004";
	static const std::string CHANNELMODEIS = "324";
	static const std::string NOTOPIC       = "331";
	static const std::string TOPIC         = "332";
	static const std::string INVITING      = "341";
	static const std::string NAMREPLY      = "353";
	static const std::string ENDOFNAMES    = "366";
}

namespace ERR
{
	static const std::string NOSUCHNICK        = "401";
	static const std::string NOSUCHCHANNEL     = "403";
	static const std::string CANNOTSENDTOCHAN  = "404";
	static const std::string NORECIPIENT       = "411";
	static const std::string NOTEXTTOSEND      = "412";
	static const std::string UNKNOWNCOMMAND    = "421";
	static const std::string NONICKNAMEGIVEN   = "431";
	static const std::string ERRONEUSNICKNAME  = "432";
	static const std::string NICKNAMEINUSE     = "433";
	static const std::string USERNOTINCHANNEL  = "441";
	static const std::string NOTONCHANNEL      = "442";
	static const std::string USERONCHANNEL     = "443";
	static const std::string NOTREGISTERED     = "451";
	static const std::string NEEDMOREPARAMS    = "461";
	static const std::string ALREADYREGISTERED = "462";
	static const std::string PASSWDMISMATCH    = "464";
	static const std::string CHANNELISFULL     = "471";
	static const std::string INVITEONLYCHAN    = "473";
	static const std::string BADCHANNELKEY     = "475";
	static const std::string BADCHANMASK       = "476";
	static const std::string CHANOPRIVSNEEDED  = "482";
	static const std::string UNKNOWNMODE       = "472";
}

#endif
