/*
 * VDE - libvdeplug_restart
 * Copyright (C) 2017 Renzo Davoli VirtualSquare
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation version 2.1 of the License, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see
 * <https://www.gnu.org/licenses/>.
 */

#define __USE_GNU
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <sys/epoll.h>
#include <sys/wait.h>
#include <libvdeplug.h>
#include <libvdeplug_mod.h>

#define POLLING_SECONDS_DEFAULT 20
#define POLLING_SECONDS_MAX     (7 * 24 * 60 * 60) // one week

static VDECONN *vde_restart_open(char *vde_url, char *descr, int interface_version,
		struct vde_open_args *open_args);
static ssize_t vde_restart_recv(VDECONN *conn, void *buf, size_t len, int flags);
static ssize_t vde_restart_send(VDECONN *conn, const void *buf, size_t len, int flags);
static int vde_restart_datafd(VDECONN *conn);
static int vde_restart_ctlfd(VDECONN *conn);
static int vde_restart_close(VDECONN *conn);

/* Declaration of the connection sructure of the module */
struct vde_restart_conn {
	void *handle;
	struct vdeplug_module *module;
	VDECONN *conn;
	char *nested_url;
	char *descr;
	struct vde_open_args open_args_data;
	struct vde_open_args *open_args;
	int epollfd;
	int pollfd;
	int faulty;
	int polling_seconds;
	time_t lastsend;
};

/* Declaration of the module sructure */
struct vdeplug_module vdeplug_ops={
	/* .flags is not initialized */
	.vde_open_real=vde_restart_open,
	.vde_recv=vde_restart_recv,
	.vde_send=vde_restart_send,
	.vde_datafd=vde_restart_datafd,
	.vde_ctlfd=vde_restart_ctlfd,
	.vde_close=vde_restart_close
};

static int start_polling(int polling_milliseconds) {
	int pfd[2];
	pid_t child, grandchild;
	socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, pfd);
	child = fork();
	if (child < 0) return -1;
	if (child == 0) {
    grandchild = fork();
    setsid();
    if (grandchild < 0) _exit(1);
    if (grandchild == 0) {
      struct pollfd pollfd[] = {{pfd[1], POLLIN, 0}};
      close (pfd[0]);
			close_range(0,pfd[1] - 1, 0);
			close_range(pfd[1] + 1, 4096, 0);
      for(;;) {
        int n = poll(pollfd, 1, polling_milliseconds);
        if (n < 0 || n)
          break;
        //if(n == 0) printf("+++>\n");
        if(n == 0) {
					ssize_t _;
					(void) _;
          _ = write(pfd[1], "", 1);
				} else
          break;
      }
      //printf("close grandchild\n");
      close(pfd[1]);
      _exit(0);
    }
    _exit(0);
  }
  int status;
  waitpid(child, &status, 0);
  close (pfd[1]);
	return pfd[0];
}

static void polling_ack(int fd) {
	char buf[10];
	/* clean the char sent from the polling process */
	ssize_t _;
	(void) _;
	_ = read(fd, buf, 10);
}

static VDECONN *vde_restart_open(char *vde_url, char *descr, int interface_version,
		struct vde_open_args *open_args)
{
	(void) interface_version;
	/* Return value on success; dynamically allocated */
	struct vde_restart_conn *newconn=NULL;
	char *nested_url;
	struct vdeparms parms[] = {
		{NULL, NULL}};
	VDECONN *conn;

	/* Get nested parameters */
	nested_url = vde_parsenestparms(vde_url);
	printf("%s %s\n", vde_url, nested_url);
	if (vde_parseparms(vde_url, parms) != 0)
		return NULL;
	/* Open connection using the nested url */
	conn = vde_open(nested_url, descr, open_args);
	if (conn == NULL)
		return  NULL;
	/* calloc initializes the memory */
	if ((newconn=calloc(1, sizeof(struct vde_restart_conn)))==NULL) {
		errno = ENOMEM;
		goto error;
	}
	newconn->conn=conn;
	newconn->nested_url = strdup(nested_url);
	if (descr != NULL)
		newconn->descr = strdup(descr);
	else
		newconn->descr = NULL;
	if (open_args) {
		newconn->open_args_data = *open_args;
		newconn->open_args = &newconn->open_args_data;
	} else
		newconn->open_args = NULL;
	if ((newconn->epollfd = epoll_create1(0)) < 0)
		goto error;
	struct epoll_event ev = {.events = EPOLLIN, .data.fd = vde_datafd(conn)};
	if ((epoll_ctl(newconn->epollfd, EPOLL_CTL_ADD, vde_datafd(conn), &ev)) < 0)
		goto poll_error;
	newconn->faulty = 0;
	newconn->lastsend = 0;
	newconn->polling_seconds = POLLING_SECONDS_DEFAULT;
	if (*vde_url) {
		char *tail;
		int polling_seconds = strtol(vde_url, &tail, 0);
		if(*tail) {
			fprintf(stderr, "invalid polling period \"%s\"\n", vde_url);
			goto poll_error;
		}
		if (polling_seconds < 1) polling_seconds = 1;
		if (polling_seconds > POLLING_SECONDS_MAX)
			polling_seconds = POLLING_SECONDS_MAX;
		newconn->polling_seconds = polling_seconds;
	}
	fcntl(newconn->epollfd, F_SETFD, FD_CLOEXEC);
	if ((newconn->pollfd = start_polling(newconn->polling_seconds * 1000)) < 0)
		goto poll_error;
	struct epoll_event evp = {.events = EPOLLIN, .data.fd = newconn->pollfd};
  if ((epoll_ctl(newconn->epollfd, EPOLL_CTL_ADD, newconn->pollfd, &evp)) < 0) {
		close(newconn->pollfd);
		goto poll_error;
	}
	return (VDECONN *) newconn;

poll_error:
	close(newconn->epollfd);
error:
	vde_close(conn);
	return NULL;
}

#if 0
void dump(void *buf, size_t len) {
	unsigned char *b=buf;
	size_t i;
	for (i=0; i<len; i++)
		printf("%02x ",b[i]);
	printf("\n\n");
}
#endif

static void reopen(struct vde_restart_conn *vde_conn) {
	vde_conn->faulty = 1;
	vde_close(vde_conn->conn);
	vde_conn->conn = vde_open(vde_conn->nested_url, vde_conn->descr, vde_conn->open_args);
	if (vde_conn->conn) {
		struct epoll_event ev = {.events = EPOLLIN, .data.fd = vde_datafd(vde_conn->conn)};
		if ((epoll_ctl(vde_conn->epollfd, EPOLL_CTL_ADD, vde_datafd(vde_conn->conn), &ev)) < 0)
			return;
		vde_conn->faulty = 0;
	} 
}

/* Right to Left <---- */
static ssize_t vde_restart_recv(VDECONN *conn, void *buf, size_t len, int flags) {
	struct vde_restart_conn *vde_conn = (struct vde_restart_conn *)conn;
	struct epoll_event ev;
	int nfds = epoll_wait(vde_conn->epollfd, &ev, 1, -1);
	if (nfds <= 0)
		return 1;
	//printf("epollout! %d\n", ev.data.fd);
	if (ev.data.fd == vde_conn->pollfd) {
		polling_ack(vde_conn->pollfd);
		//printf("POLL! %d\n", vde_conn->polling_seconds);
		if ((time(NULL) - vde_conn->lastsend) >= vde_conn->polling_seconds) {
			ssize_t n = vde_send(vde_conn->conn, "", 1, 0);
			if (n == -1 && errno != EAGAIN)
				reopen(vde_conn);
		}
		return 1;
	}
	/* Length of the received packet */
	ssize_t retval = vde_recv(vde_conn->conn, buf, len, flags);
	//printf("recv %zd\n", retval);
	return retval;
}

/* Left to Right ----> */
static ssize_t vde_restart_send(VDECONN *conn, const void *buf, size_t len, int flags) {
  ssize_t retval;
  struct vde_restart_conn *vde_conn = (struct vde_restart_conn *)conn;
  retval = vde_send(vde_conn->conn, buf, len, flags);
	if (retval > 0)
		vde_conn->lastsend = time(NULL);
  //printf("send %zd\n", retval);
  return retval;
}

static int vde_restart_datafd(VDECONN *conn) {
	struct vde_restart_conn *vde_conn = (struct vde_restart_conn *)conn;
	return vde_conn->epollfd;
}

static int vde_restart_ctlfd(VDECONN *conn) {
	(void) conn;
	return -1;
}

static int vde_restart_close(VDECONN *conn) {
	int rv;
	struct vde_restart_conn *vde_conn = (struct vde_restart_conn *)conn;
	rv = vde_close(vde_conn->conn);
	close(vde_conn->pollfd);
	close(vde_conn->epollfd);
	free(vde_conn->nested_url);
	if (vde_conn->descr)
		free(vde_conn->descr);
	free(vde_conn);
	return rv;
}
