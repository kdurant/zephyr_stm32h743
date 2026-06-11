#include <zephyr/kernel.h>
#include <zephyr/posix/sys/socket.h>
#include <zephyr/posix/unistd.h>
#include <zephyr/console/console.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define PORT 6666
#define BACKLOG 5
#define BUF_SIZE 1024

static int start_server(void)
{
	int serv;
	struct sockaddr_in addr;

	serv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (serv < 0) {
		printk("socket() failed: %d\n", errno);
		return -1;
	}

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(PORT);

	if (bind(serv, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		printk("bind() failed: %d\n", errno);
		close(serv);
		return -1;
	}

	if (listen(serv, BACKLOG) < 0) {
		printk("listen() failed: %d\n", errno);
		close(serv);
		return -1;
	}

	printk("TCP server on 0.0.0.0:%d\n", PORT);
	return serv;
}

static void handle_client(int client)
{
	char buf[BUF_SIZE];
	int idx = 0;
	int ret;

	printk("Client connected\n");

	while (1) {
		int c = console_getchar();

		if (c >= 0) {
			if (c == '\r' || c == '\n') {
				console_putchar('\r');
				console_putchar('\n');
				if (idx > 0) {
					buf[idx] = '\n';
					send(client, buf, idx + 1, 0);
					idx = 0;
				}
			} else if (c == '\b' || c == 0x7f) {
				if (idx > 0) {
					console_putchar('\b');
					console_putchar(' ');
					console_putchar('\b');
					idx--;
				}
			} else if (idx < (int)sizeof(buf) - 1) {
				console_putchar(c);
				buf[idx++] = (char)c;
			}
		}

		ret = recv(client, buf, sizeof(buf) - 1, MSG_DONTWAIT);
		if (ret > 0) {
			buf[ret] = '\0';
			printk("\nC: %s", buf);
		} else if (ret == 0 || (ret < 0 && errno != EAGAIN)) {
			if (ret < 0) {
				printk("\nrecv error: %d\n", errno);
			}
			break;
		}
	}

	printk("\nClient disconnected\n");
}

int main(void)
{
	int serv, client;
	struct sockaddr_in addr;
	socklen_t addr_len;

	console_init();
	console_set_rx_timeout(K_MSEC(200));

	serv = start_server();
	if (serv < 0) {
		return 0;
	}

	while (1) {
		addr_len = sizeof(addr);
		client = accept(serv, (struct sockaddr *)&addr, &addr_len);
		if (client < 0) {
			printk("accept() failed: %d\n", errno);
			continue;
		}

		handle_client(client);
		close(client);
		printk("Waiting for connection...\n");
	}

	close(serv);
	return 0;
}
