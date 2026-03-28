NAME = ircserv

CXXFLAGS = -Wall -Wextra -Werror -std=c++98
CXX = c++
RM = rm -f

SRC_DIR = src
OBJ_DIR = objects
INC_DIR = include

SRCS = main.cpp Server.cpp Network.cpp Client.cpp Channel.cpp \
       Parse.cpp CmdAuth.cpp CmdChannel.cpp CmdMessage.cpp Bot.cpp
OBJS = $(addprefix $(OBJ_DIR)/, $(SRCS:.cpp=.o))

MAGENTA = \033[35m
RESET = \033[0m

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJS)
	@printf "$(MAGENTA)             _           \n$(RESET)"
	@printf "$(MAGENTA)  __ _      |_|          \n$(RESET)"
	@printf "$(MAGENTA) / _| |_     _ _ __ ___  \n$(RESET)"
	@printf "$(MAGENTA)| |_| __|   | |  __/ __| \n$(RESET)"
	@printf "$(MAGENTA)|  _| |_    | | |  | |_  \n$(RESET)"
	@printf "$(MAGENTA)|_|  \__|___|_|_|  \___| \n$(RESET)"
	@printf "\n"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -I$(INC_DIR) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	$(RM) -r $(OBJ_DIR)

fclean: clean
	$(RM) $(NAME)

re: fclean all

.PHONY: all clean fclean re
