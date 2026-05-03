# Makefile - NewsBot
#
# Dépendances : libcurl, libssl
# Ubuntu/Debian : sudo apt install libcurl4-openssl-dev
# Oracle Linux  : sudo dnf install libcurl-devel openssl-devel

CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=c11 -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lcurl

TARGET  = newsbot
SRCS    = main.c fetcher.c parser.c storage.c summarizer.c \
          discord.c hash.c log.c sources.c
OBJS    = $(SRCS:.c=.o)

.PHONY: all clean install run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo ""
	@echo "✅ Compilation réussie ! Lance avec : ./newsbot"
	@echo ""

%.o: %.c newsbot.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

# Installation des dépendances (Ubuntu/Debian)
install-deps:
	sudo apt update
	sudo apt install -y libcurl4-openssl-dev gcc make

# Lance le bot avec les variables d'environnement
run: $(TARGET)
	@if [ -z "$$GROQ_API_KEY" ]; then \
		echo "❌ Définis GROQ_API_KEY : export GROQ_API_KEY=ta_clé"; \
		exit 1; \
	fi
	@if [ -z "$$DISCORD_TOKEN" ]; then \
		echo "❌ Définis DISCORD_TOKEN : export DISCORD_TOKEN=ton_token"; \
		exit 1; \
	fi
	@if [ -z "$$DISCORD_CHANNEL_ID" ]; then \
		echo "❌ Définis DISCORD_CHANNEL_ID : export DISCORD_CHANNEL_ID=id_channel"; \
		exit 1; \
	fi
	./$(TARGET)
