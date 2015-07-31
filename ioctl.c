/*
*
* ioctl user tool
*
* Copyright (C) 2015 Loic Poulain
*
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*
*/

#include <stdio.h>
#include <sys/ioctl.h>
#include <asm/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <linux/sockios.h>
#include <linux/cdrom.h>
#include <linux/hdreg.h>
#include <getopt.h>

struct {
	char *name;
	int cmd;
} ioctl_string[] = {
	{ "CDROMEJECT", CDROMEJECT },
	{ "HDIO_GET_IDENTITY", HDIO_GET_IDENTITY},
	{ "HDIO_GETGEO", HDIO_GETGEO },
	{ },
};

struct ioctl_request {
	unsigned int cmd;
	unsigned int type;
	unsigned int dir;
	unsigned int nr;
	unsigned int size;
	unsigned long arg;
	int legacy;
	int address;
};

static void ioctl_fill(struct ioctl_request *req, unsigned int cmd)
{
	if (cmd & 0xffff0000) {
		req->cmd = cmd;
		req->type = _IOC_TYPE(cmd);
		req->dir = _IOC_DIR(cmd);
		req->nr = _IOC_NR(cmd);
		req->size = _IOC_SIZE(cmd);
	} else {
		req->type = _IOC_TYPE(cmd);
		req->cmd = cmd;
		req->size = 0;
		req->legacy = 1;
	}

	req->arg = 0;
}

static int ioctl_alloc_data(struct ioctl_request *req)
{
	void *data = calloc(req->size, 1);
	if (!data)
		return -ENOMEM;

	req->arg = (unsigned long)data;

	return 0;
}

static int ioctl_print_data(struct ioctl_request *req)
{
	unsigned char *data = (char *)req->arg;
	int i, j;

	for (i = 1; i <= req->size; i++) {
		printf("%02x ", *data++);
		if (!(i % 20)) {
			//for (j = 20; j > 0; j--)
			//	printf("%c ", *(data - j));
			printf("\n");
		}
	}
}

static void print_ioctl_list(void)
{
	int i = 0;

	while (ioctl_string[i].name) {
		struct ioctl_request req;

		ioctl_fill(&req, ioctl_string[i].cmd);
	
		if (!req.legacy) {
			printf("\t%s(0x%x) type(%c) dir(%d) nr(%d) size(%d)\n",
			       ioctl_string[i].name, req.cmd, req.type, req.dir,
			       req.nr, req.size);
		} else {
			printf("\t%s(0x%x) type(%c) legacy\n",
			        ioctl_string[i].name, req.cmd, req.type);
		}

		i++;
	}
}

static void usage(void)
{
	printf("Usage: ioctl [options] <ioctl>\n" \
	       "options:\n" \
	       "\t-L, --list  print known ioctl\n" \
	       "\t-D, --dev   <device>     Specify device to use\n" \
	       "\t-A, --alloc <size> \n" \
	       "\t-V, --value <val> send a specific value \n" \
	       "\t-O, --object <data> send specific data \n" \
	       );
		
}

static const struct option main_options[] = {
	{ "data", required_argument, NULL, 'd' },
	{ "help", no_argument, NULL, 'h' },
	{ "list", no_argument, NULL, 'L'},
	{ "alloc", required_argument, NULL, 'A' },
	{ "value", required_argument, NULL, 'V' },
	{ "dev", required_argument, NULL, 'D' },
	{ },
};

int main(int argc, char *argv[])
{
	int fd, err, i = 0;
	unsigned long cmd = 0;
	struct ioctl_request req;
	void *data;

	unsigned long request_id = 0;
	char *dev_path = 0;
	unsigned int alloc = 0;
	char *ioctl_param = 0;
	unsigned long value = 0;

	for (;;) {
		int opt = getopt_long(argc, argv, "A:D:V:Lh", main_options,
				      NULL);
		if (opt < 0)
			break;

		switch (opt) {
		case 'L':
			print_ioctl_list();
			return EXIT_SUCCESS;
		case 'A':
			alloc = atoi(optarg);
			break;
		case 'D':
			dev_path = optarg;
			break;
		case 'V':
			value = atoi(optarg);
			break;
		case 'h':
			usage();
			return EXIT_SUCCESS;
		default:
			break;
		}
	}

	if (argc - optind != 1) {
		printf("You must specify a request id\n");
		usage();
		return EXIT_FAILURE;
	}

	ioctl_param = argv[optind];
	if (ioctl_param[0] == '0' &&
	    (ioctl_param[1] == 'x' ||
	    ioctl_param[1] == 'X')) {
		request_id = strtol(ioctl_param, NULL, 16);
	} else {
		while (ioctl_string[i].name) {
			if (!strcmp(ioctl_string[i].name, ioctl_param)) { 
				request_id = ioctl_string[i].cmd;
				break;
			}
			i++;
		}
	}

	if (!request_id) {
		printf("bad ioctl id: %s\n", ioctl_param);
		return EXIT_FAILURE;
	}

	if (!dev_path) {
		printf("You must specify a device path or socket\n");
		usage();
		return EXIT_FAILURE;
	}

	if (dev_path) {
		fd = open(dev_path, O_RDWR|O_NONBLOCK);
		if (fd < 0) {
			perror("Failed to open");
			return EXIT_FAILURE;
		}
	}
	
	ioctl_fill(&req, request_id);

	if (value) {
		req.arg = value;
	} else if (alloc)
		req.size = alloc;

	if (req.size)
		ioctl_alloc_data(&req);

	printf("ioctl(%s, 0x%x, 0x%lx)\n", dev_path, req.cmd, req.arg);
	err = ioctl(fd, req.cmd, req.arg);
	if (err < 0) {
		perror("ioctl error");
		goto error;
	}

	ioctl_print_data(&req);

	close (fd);

	return EXIT_SUCCESS;

error:
	close(fd);
	
	return EXIT_FAILURE;
}
