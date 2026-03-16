#include "Irc.hpp"

bool g_alive = true;

static void handleSignal(int sig)
{
	(void)sig;
	g_alive = false;
	std::cout << std::endl;
}

int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		std::cerr << "Usage: " << argv[0] << " <port> <password>" << std::endl;
		return 1;
	}

	int port = std::atoi(argv[1]);
	if (port <= 0 || port > 65535)
	{
		std::cerr << "Error: port must be between 1 and 65535" << std::endl;
		return 1;
	}

	std::string pass = argv[2];
	if (pass.empty())
	{
		std::cerr << "Error: password cannot be empty" << std::endl;
		return 1;
	}

	signal(SIGINT, handleSignal);
	signal(SIGPIPE, SIG_IGN);

	try
	{
		Server srv(port, pass);
		srv.boot();
		srv.run();
	}
	catch (const std::exception& e)
	{
		std::cerr << "Fatal: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
