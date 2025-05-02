CC = gcc
CFLAGS = -Wall -Wextra
LDFLAGS = -lX11
NAME = status_bar

SERVICE_FILE_NAME = $(NAME).service
USER_SYSTEMD_DIR = $(HOME)/.config/systemd/user
SERVICE_FILE = $(USER_SYSTEMD_DIR)/$(SERVICE_FILE_NAME)

build: $(NAME)

$(NAME): *.c
	$(CC) $(CFLAGS) -o $(NAME) *.c $(LDFLAGS)

clean:
	rm -f /usr/local/bin/$(NAME)
	rm -f $(NAME)

install: $(NAME) install_service
	sudo cp $(NAME) /usr/local/bin

install_service: $(SERVICE_FILE_NAME)
	mkdir -p $(USER_SYSTEMD_DIR)
	cp ./$(SERVICE_FILE_NAME) $(SERVICE_FILE)
	systemctl --user daemon-reload
	systemctl --user enable --now $(SERVICE_FILE_NAME)

uninstall:
	rm -f /usr/local/bin/$(NAME)
	rm -f $(SERVICE_FILE)
	systemctl --user stop $(SERVICE_FILE_NAME) || true
	systemctl --user disable $(SERVICE_FILE_NAME) || true
	systemctl --user daemon-reload
