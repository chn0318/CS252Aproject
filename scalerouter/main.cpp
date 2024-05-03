#include "scalerouter.h"

static void usage(const char *argv0)
{
	fprintf(stdout, "Usage:\n");
	fprintf(stdout, " %s start a server and wait for connection\n", argv0);
	fprintf(stdout, " %s <host> connect to server at <host>\n", argv0);
	fprintf(stdout, "\n");
	fprintf(stdout, "Options:\n");
	fprintf(stdout, " -p, --port <port> listen on/connect to port <port> (default 18515)\n");
	fprintf(stdout, " -d, --ib-dev <dev> use IB device <dev> (default first device found)\n");
	fprintf(stdout, " -i, --ib-port <port> use port <port> of IB device (default 1)\n");
	fprintf(stdout, " -g, --gid_idx <git index> gid index to be used in GRH (default not used)\n");
	fprintf(stdout, " -r, --disable_rdma <disable rdma> whether enable shared memory communication, or RDMA (default not used)\n");
}

struct config_t config = {
	NULL, /* dev_name */
	NULL, /* server_name */
	0,    /* tcp_port */
	1,	  /* ib_port */ // default IB device port
	3, 	  /* gid_idx */ // default gid_idx to work with RoCEv2
    1,    /* disable_rdma */ // default disable shared memory communication 
	};

int main (int argc, char **argv) {
    /* parse the command line parameters */
	while (1) {
		int c;
		struct option long_options[] = {
			{name : "port", has_arg : 1, flag : NULL, val : 'p'},
			{name : "ib-dev", has_arg : 1, flag : NULL, val : 'd'},
			{name : "ib-port", has_arg : 1, flag : NULL, val : 'i'},
			{name : "gid-idx", has_arg : 1, flag : NULL, val : 'g'},
			{name : "disable-rdma", has_arg : 1, flag : NULL, val : 'r'},
			{name : NULL, has_arg : 0, flag : NULL, val : '\0'}
		};
		c = getopt_long(argc, argv, "p:d:i:g:r:", long_options, NULL);
		if (c == -1)
			break;
		switch (c) {
			case 'p':
				config.tcp_port = strtoul(optarg, NULL, 0);
				break;
			case 'd':
				config.dev_name = strdup(optarg);
				break;
			case 'i':
				config.ib_port = strtoul(optarg, NULL, 0);
				if (config.ib_port < 0) {
					usage(argv[0]);
					return 1;
				}
				break;
			case 'g':
				config.gid_idx = strtoul(optarg, NULL, 0);
				if (config.gid_idx < 0) {
					usage(argv[0]);
					return 1;
				}
				break;
			case 'r':
				config.disable_rdma = strtoul(optarg, NULL, 0);
				if (config.disable_rdma < 0) {
					usage(argv[0]);
					return 1;
				}
				break;
			default:
				usage(argv[0]);
				return 1;
		}
	}

    ScaleRouter scalerouter(config);
    scalerouter.start();
}