#include <u.h>
#include <libc.h>
#include <stdio.h>

typedef u8int uint8_t;
typedef u16int uint16_t;
typedef u32int uint32_t;
typedef u64int uint64_t;
typedef uchar int8_t;
typedef short int16_t;
typedef int int32_t;
typedef vlong int64_t;
typedef long size_t;
typedef long ssize_t;

#define CIO_VERSION_LATEST 22

#define GROUPS_MAX 32

typedef enum
{
   CiodInitialized                     = 0x00,
   CiodException                       = 0x01,
   CiodFunctionFailed                  = 0x02,
   CiodLockCreateFailed                = 0x03,
   CiodDeviceOpenFailed                = 0x04,
   CiodQueueCreateFailed               = 0x05,
   CiodThreadCreateFailed              = 0x06,
   CiodInvalidControlMsg               = 0x07,
   CiodInvalidDataMsg                  = 0x08,
   CiodWrongControlMsg                 = 0x09,
   CiodWrongDataMsg                    = 0x0a,
   CiodCreateListenerFailed            = 0x0b,
   CiodAcceptConnFailed                = 0x0c,
   CiodAllocateFailed                  = 0x0d,
   CiodControlReadFailed               = 0x0e,
   CiodDataReadFailed                  = 0x0f,
   CiodDuplicateFdFailed               = 0x10,
   CiodSetGroupsFailed                 = 0x11,
   CiodSetGidFailed                    = 0x12,
   CiodSetUidFailed                    = 0x13,
   CiodExecFailed                      = 0x14,
   CiodNodeExited                      = 0x15,
   CiodInvalidDebugRequest             = 0x16,
   CiodDebuggerReadFailed              = 0x17,
   CiodInvalidNode                     = 0x18,
   CiodInvalidThread                   = 0x19,
   CiodDebuggerWriteFailed             = 0x1a,
   CiodPersonalityOpenFailed           = 0x1b,
   CiodInvalidPersonality              = 0x1c,
   CiodTooManyNodes                    = 0x1d,
   CiodSharedMemoryCreateFailed        = 0x1e,
   CiodSharedMemoryAttachFailed        = 0x1f,
   CiodIncompleteCoreFile              = 0x20,
   CiodBadAlignment                    = 0x21,
   CiodSharedMemoryExtendFailed        = 0x22,
   CiodHungProxy                       = 0x23,
   CiodInvalidProtocol                 = 0x24,
   CiodControlWriteFailed              = 0x25,
   CiodDataWriteFailed                 = 0x26,
   CiodCreateConnFailed                = 0x27,
   CiodSecondaryControlReadFailed      = 0x28,
   CiodSecondaryControlWriteFailed     = 0x29,
   CiodSecondaryDataReadFailed         = 0x2a,
   CiodSecondaryDataWriteFailed        = 0x2b,
   CiodNotEnoughMemory                 = 0x2c,
} BGP_RAS_CIOD_ErrCodes;

enum CioMessageType
{
    CIO_VERSION_MESSAGE,
    LOGIN_MESSAGE,
    LOAD_MESSAGE,
    START_MESSAGE,
    KILL_MESSAGE,
    NODE_STATUS_MESSAGE,
    START_TOOL_MESSAGE,
    RESULT_MESSAGE,
    END_MESSAGE,
    RECONNECT_MESSAGE,
    END_TOOL_MESSAGE, /* New in V1R3M0 */
    NODE_LOCATION_MESSAGE, /* New in V1R3M0 */
    CIO_LAST_MESSAGE
};

enum CioNodeStatusType
{
    CIO_NODE_STATUS_UNKNOWN,
    CIO_NODE_STATUS_LOADED,
    CIO_NODE_STATUS_KILLED,
    CIO_NODE_STATUS_EXITED,
    CIO_NODE_STATUS_ERROR
};

enum JobMode
{
    M_SMP_MODE,
    M_VIRTUAL_NODE_MODE,
    M_DUAL_MODE
};

struct MessagePrefix
{
    uint32_t type;
    uint32_t version;
    uint32_t target;
    uint32_t length;
    uint32_t jobid; /* New in V1R3M0 */
};

struct DataPrefixIn
{
	uint32_t type;
	uint32_t eof;
	uint32_t errno;
};

struct DataPrefixOut
{
	uint32_t type;
	uint32_t core;
	uint32_t treeaddr;
	uint32_t logicalrank;
};

struct DataPrefixGeneral
{
	uint32_t type;
	uint32_t subtype;
};

enum DataType
{
	VERSION_MESSAGE,
	STDIN_MESSAGE,
	STDOUT_MESSAGE,
	STDERR_MESSAGE,
	GENERAL_MESSAGE,
	LAST_MESSAGE
};

static int DataStreamVersion = 1;

#define VERSION_MSG_ENC_LEN 64

#define DEBUG_INITFINI 1
static int logfd;
static int debug_flags = DEBUG_INITFINI;

static int cio_protocol_version;
static int current_job_id, current_job_mode;
static int stdiofd = 0;

char *squidboy = "Hello, Squidboy!\n";

unsigned long
ntohl(int x)
{
	unsigned long n;
	unsigned char *p;

	n = x;
	p = (unsigned char*)&n;
	return (p[0]<<24)|(p[1]<<16)|(p[2]<<8)|p[3];
}

unsigned long
htonl(unsigned long h)
{
	unsigned long n;
	unsigned char *p;

	p = (unsigned char*)&n;
	p[0] = h>>24;
	p[1] = h>>16;
	p[2] = h>>8;
	p[3] = h;
	return n;
}

/*
 * A more robust version of read(2).
 */
static ssize_t
socket_read(int fd, void* buf, size_t count)
{
	return readn(fd, buf, count);
}

/*
 * A more robust version of write(2).
 */
static ssize_t
socket_write(int fd, const void* buf, size_t count)
{
	return write(fd, buf, count);
}

static int
stream_read_prefix(int fd, struct MessagePrefix* prefix)
{
    if (socket_read(fd, prefix, sizeof(*prefix)) != sizeof(*prefix))
		return 1;

    return 0;
}

static int
stream_read_int(int fd, int* result)
{
    int res;

    if (socket_read(fd, &res, sizeof(res)) != sizeof(res))
		return 1;

    *result = ntohl(res);

    return 0;
}

static int
stream_read_arr(int fd, void** arr, int* size)
{
    if (stream_read_int(fd, size))
		return 1;

    if (*size < 0)
		return 2;

    if (!(*arr = malloc(*size)))
		return 3;

    if (socket_read(fd, *arr, *size) != *size) {
		free(*arr);
		return 1;
    }

    return 0;
}

static int
stream_read_str(int fd, char** str)
{
    int len;

    return stream_read_arr(fd, (void**)str, &len);
}

static int
stream_write_int(int fd, int data)
{
    data = htonl(data);

    if (socket_write(fd, &data, sizeof(data)) != sizeof(data))
		return 1;

    return 0;
}

static int
stream_write_arr(int fd, const void* arr, int size)
{
    if (stream_write_int(fd, size))
		return 1;

    if (socket_write(fd, arr, size) != size)
		return 1;

    return 0;
}

static int
cio_write_prefix(int type, int length)
{
    struct MessagePrefix prefix;

    prefix.type = htonl(type);
    prefix.version = htonl(cio_protocol_version);
    prefix.target = 0;
    prefix.length = htonl(length);
    prefix.jobid = current_job_id;

    if (socket_write(stdiofd, &prefix, sizeof(prefix)) !=
			sizeof(prefix)) {
		return 1;
    }

    return 0;
}

static int
cio_write_result(enum CioMessageType message, int result, int value)
{
    if (cio_write_prefix(RESULT_MESSAGE, sizeof(uint32_t) * 3) ||
			stream_write_int(stdiofd, message) ||
			stream_write_int(stdiofd, result) ||
			stream_write_int(stdiofd, value)) {
		fprint(logfd, "Error writing message on CIO stream!\n");
		return 1;
    }

    return 0;
}

static int
cio_write_node_status(int cpu, int p2p, enum CioNodeStatusType type, int value)
{
    if (cio_write_prefix(NODE_STATUS_MESSAGE, sizeof(uint32_t) * 4) ||
			stream_write_int(stdiofd, cpu) ||
			stream_write_int(stdiofd, p2p) ||
			stream_write_int(stdiofd, type) ||
			stream_write_int(stdiofd, value)){

		fprint(logfd, "Error writing message on CIO stream!\n");
		return 1;
    }

    return 0;
}

int putenvint(char *key, int value)
{
	char envbuf[255];
	
	sprint(envbuf, "%d\0", value);
	return putenv(key, envbuf);
}

void process_env(char *envpair)
{
	char *value;
	int count;
	
	for(count=0; count < strlen(envpair); count++) {
		if(envpair[count] == '=')
			break;
	}
		
	if(count == strlen(envpair)) {
		fprint(logfd, "WARNING: Problem parsing environment! (%s)\n", envpair);
		return;
	}
 
 	envpair[count] = '\0';
 	value = &envpair[count+1];
 	
	putenv(envpair, value);
	envpair[count] = '=';	
}

static int
cio_write_data_out(int type, int core, int treeaddr, int rank, char *data, int len)
{	
	if(cio_write_prefix(type, sizeof(uint32_t)*4 + len) ||
		stream_write_int(stdiofd, core) ||
		stream_write_int(stdiofd, treeaddr) ||
		stream_write_int(stdiofd, rank) ||
		stream_write_arr(stdiofd, data, len)) {
		    fprint(logfd, "Error writing message on DATA stream!\n");
		    return 1;
	}	

	return 0;
}

static int
cio_write_data_general(int type, char *data, int len)
{

	stream_write_int(stdiofd, GENERAL_MESSAGE);
	stream_write_int(stdiofd, type);
	stream_write_arr(stdiofd, data, len);

    return 0;
}

static int
cio_write_data_vers(void)
{
	char *eyecatcher = "<DataStream>";
	char _encMessage[64]; /* don't know what goes here */
	
	stream_write_int(stdiofd, VERSION_MESSAGE);
	stream_write_arr(stdiofd, eyecatcher, 12);
	stream_write_int(stdiofd, 1);
	stream_write_arr(stdiofd,_encMessage, 64);
	
	return 0;
}

void
debug_the_rest(int fd)
{
	uchar buffer[1024];
	int n;
	int count;
	
	/* DEBUG FOR NOW, SPIN READING AND REPORTING */
	for(;;) {
		n = read(fd, buffer, 1024);
		fprint(logfd, "--- Read (%d) bytes ---\n\t", n);
		for(count=0; count < n; count++) {
			if(count % 16 == 0)
				fprint(logfd,"\n\t");
			fprint(logfd, "%#2.2ux ", buffer[count]);
		}
		fprint(logfd, "\n");
	}
}

int receive_data_loop(void)
{
    for (;;) {
	struct MessagePrefix prefix;

	if (stream_read_prefix(stdiofd, &prefix)) {
	    /* We don't know what the message type was supposed to be... */

	    fprint(logfd, "Error reading command prefix on DATA stream!\n");
	    return 1;
	}
	
	switch (prefix.type) {
	    case CIO_VERSION_MESSAGE:
	    {
		char *enc_msg, **enc_msg_p = &enc_msg;
		int enc_len;

		current_job_id = prefix.jobid;

		if (stream_read_int(stdiofd, &cio_protocol_version)){

		    fprint(logfd, "Error reading message on DATA stream!\n");
		    return 1;
		}

		if (cio_protocol_version != 3) {
		    fprint(logfd, "Version mismatch on DATA stream!\n");
		    return 1;
		}
		
		//debug_the_rest(stdiofd);
				
		if (stream_read_arr(stdiofd, (void**)enc_msg_p,
				    &enc_len)) {
		    fprint(logfd, "Error reading message on DATA stream!\n");
		    return 1;
		}
				
		if (enc_len != VERSION_MSG_ENC_LEN) {
		    fprint(logfd, "Invalid data on DATA stream!\n");
		    return 1;
		}

		if (debug_flags & DEBUG_INITFINI) {
		    fprint(logfd, "DATA VERSION\n");
		    fprint(logfd, "received version %d\n",
			    cio_protocol_version);
		}
		
		/* We don't care about the encrypted message (nor timeout
		   for that matter).  Just send back an ACK.  */

		memset(enc_msg, '\0', VERSION_MSG_ENC_LEN);
 
		if (cio_write_prefix(CIO_VERSION_MESSAGE, sizeof(uint32_t) * 3
				      + VERSION_MSG_ENC_LEN) ||
		    stream_write_int(stdiofd, cio_protocol_version) || 
		    stream_write_arr(stdiofd, enc_msg,
				     VERSION_MSG_ENC_LEN)) {
		    fprint(logfd, "Error writing message on DATA stream!\n");
		    return 1;
		}

		free(enc_msg);
		
		debug_the_rest(stdiofd);

/* BUG: This breaks everything 		
fprint(logfd, "sending squidboy\n");
		cio_write_data_out(STDERR_MESSAGE, 0, 0, 0, squidboy, strlen(squidboy));
fprint(logfd, "done sending squidboy\n");
*/

		break;
	    }

	    default:
		fprint(logfd, "Unknown message type %d on DATA stream!\n",
			prefix.type);
	} /* switch (prefix.type) */
    } /* for (;;) */
}

int receive_cio_loop(void)
{
    for (;;) {
	struct MessagePrefix prefix;

	if (stream_read_prefix(stdiofd, &prefix)) {
	    /* We don't know what the message type was supposed to be... */

	    fprint(logfd, "Error reading command prefix on CIO stream!\n");
	    return 1;
	}
	
	switch (prefix.type) {
	    case CIO_VERSION_MESSAGE:
	    {
		int timeout;
		char *enc_msg, **enc_msg_p = &enc_msg;
		int enc_len;

		current_job_id = prefix.jobid;

		if (stream_read_int(stdiofd, &cio_protocol_version)){

		    fprint(logfd, "Error reading message on CIO stream!\n");
		    return 1;
		}


		if (cio_protocol_version != CIO_VERSION_LATEST) {
		    fprint(logfd, "Version mismatch on CIO stream!\n");
		    return 1;
		}
				
		if (stream_read_int(stdiofd, &timeout) ||
		    stream_read_arr(stdiofd, (void**)enc_msg_p,
				    &enc_len)) {
		    fprint(logfd, "Error reading message on CIO stream!\n");
		    return 1;
		}
				
		if (enc_len != VERSION_MSG_ENC_LEN) {
		    fprint(logfd, "Invalid data on CIO stream!\n");
		    return 1;
		}

		if (debug_flags & DEBUG_INITFINI) {
		    fprint(logfd, "CIO VERSION\n");
		    fprint(logfd, "received version %d, timeout %d\n",
			    cio_protocol_version, timeout);
		}
		
		/* We don't care about the encrypted message (nor timeout
		   for that matter).  Just send back an ACK.  */

		memset(enc_msg, '\0', VERSION_MSG_ENC_LEN);

		if (cio_write_prefix(CIO_VERSION_MESSAGE, sizeof(uint32_t) * 3
				      + VERSION_MSG_ENC_LEN) ||
		    stream_write_int(stdiofd, cio_protocol_version) || 
		    stream_write_int(stdiofd, timeout) || 
		    stream_write_arr(stdiofd, enc_msg,
				     VERSION_MSG_ENC_LEN)) {
		    fprint(logfd, "Error writing message on CIO stream!\n");
		    return 1;
		}

		free(enc_msg);

		break;
	    }

	    case LOGIN_MESSAGE:
	    {
		int i;
		int current_job_uid, current_job_gid, current_job_umask;
		char* current_job_homedir;
		int current_job_ngroups;
		int current_job_groups[GROUPS_MAX];
		int current_job_id;
	
		if (stream_read_int(stdiofd, &current_job_uid) ||
		    stream_read_int(stdiofd, &current_job_gid) ||
		    stream_read_int(stdiofd, &current_job_umask) ||
		    stream_read_str(stdiofd, &current_job_homedir) ||
		    stream_read_int(stdiofd, &current_job_ngroups)) {
		    fprint(logfd, "Error reading message on CIO stream!\n");
		    return 1;
		}
		
		if (current_job_ngroups > sizeof(current_job_groups) /
		    sizeof(current_job_groups[0])) {
		    fprint(logfd, "Invalid data on CIO stream!\n");
		    return 1;
		}
		
		for (i = 0; i < current_job_ngroups; i++)
		    if (stream_read_int(stdiofd,
					(int*)&current_job_groups[i])) {
				fprint(logfd, "Error reading message on CIO "
				"stream!\n");
				return 1;
		    }
		if (stream_read_int(stdiofd, &current_job_id) ||
		    stream_read_int(stdiofd, &current_job_mode)) {
		    fprint(logfd, "Error reading message on CIO stream!\n");
		    return 1;
		}
		
		putenvint("job_id", current_job_id);
		switch(current_job_mode) {
			case M_SMP_MODE:
				putenv("job_mode", "smp");
				break;
			case M_VIRTUAL_NODE_MODE:
				putenv("job_mode", "virtual node");
				break;
			case M_DUAL_MODE:
				putenv("job_mode", "dual mode");
				break;
			default:
				putenvint("job_mode", current_job_mode);
		};  
		putenvint("job_uid", current_job_uid);
		putenvint("job_gid", current_job_gid);
		putenvint("job_umask", current_job_umask);
		putenv("job_homedir", current_job_homedir);
		
		/* TODO: do we need groups? probably not */
		
		if (debug_flags & DEBUG_INITFINI) {
		    fprint(logfd, "LOGIN\n");
		    fprint(logfd,  "received uid %d, gid %d, umask 0%o, "
			    "homedir %s\n", current_job_uid, current_job_gid,
			    current_job_umask, current_job_homedir);
		    fprint(logfd, "groups: ");
		    for (i = 0; i < current_job_ngroups; i++)
			fprint(logfd, "%d%c", current_job_groups[i],
				i == current_job_ngroups - 1 ? '\n' : ' ');
		    fprint(logfd, "jobid: %d, jobmode: %d\n",
			    current_job_id, current_job_mode);
		}

		if (cio_write_result(LOGIN_MESSAGE, 0, 0))
		    return 1;

		break;
	    }

	    case LOAD_MESSAGE:
	    {
		int current_job_argc, current_job_envc;
		int current_job_proc_count;
		int strace;
		int flags;
		char* rankmap;
		int i;
		char* argptr;
		char* current_job_argenv = nil;
		char** current_job_argenv_p = &current_job_argenv;
		int current_job_nargenv;
		char argbuf[255]; /* TODO: hope that's enough */
		char *argbufptr;
		
		if (stream_read_int(stdiofd, &current_job_argc) ||
		    stream_read_int(stdiofd, &current_job_envc) ||
		    stream_read_int(stdiofd, &strace) ||
		    stream_read_int(stdiofd,
				    &current_job_proc_count) ||
		    stream_read_int(stdiofd, &flags) ||
		    stream_read_str(stdiofd, &rankmap) ||
		    stream_read_arr(stdiofd,
				    (void**)current_job_argenv_p,
				    &current_job_nargenv)) {
				    
		    fprint(logfd, "Error reading message on CIO stream!\n");
		    return 1;
		}
		
		putenvint("job_strace", strace);
		putenvint("job_proc_count", current_job_proc_count);
		putenvint("job_flags", flags);
		putenv("job_rankmap", rankmap);
		putenv("job_argenv", current_job_argenv);
		
		/* TODO: Parse this shit out */
		argptr = current_job_argenv;
		argbufptr = argbuf;
		USED(argbufptr);
		for (i = 0; i < current_job_argc; i++){
			if(((argbufptr-argbuf) + strlen(argptr)) > 255) {
				fprint(logfd, "WARNING: Argument list truncated due to buffer overflow!\n");
			} else {
				sprint(argbufptr, "%s%c", argptr,
			       	i == current_job_argc - 1 ? '\n' : ' ');
				argbufptr += strlen(argptr) + 1;			  
			}
			argptr += strlen(argptr) + 1;
		}
		putenv("job_args", argbuf);
		/* environment variables */
		for (i = 0; i < current_job_envc; i++) {
			process_env(argptr);
			

			argptr += strlen(argptr) + 1;
		}	
			
		if (debug_flags & DEBUG_INITFINI) {
		    fprint(logfd, "LOAD\n");
		    fprint(logfd, "received strace %d, nproc %d\n",
			    strace, current_job_proc_count);
		    fprint(logfd, "flags 0x%x, rankmap %s\n",
			    flags, rankmap);
		    fprint(logfd, "argv: ");
		    argptr = current_job_argenv;
		    for (i = 0; i < current_job_argc; i++)
		    {
			fprint(logfd, "%s%c", argptr,
			       i == current_job_argc - 1 ? '\n' : ' ');
			argptr += strlen(argptr) + 1;
		    }
		    fprint(logfd, "env: ");
		    for (i = 0; i < current_job_envc; i++)
		    {
			fprint(logfd, "%s%c", argptr,
			       i == current_job_envc - 1 ? '\n' : ' ');
			argptr += strlen(argptr) + 1;
		    }
		}

		if (cio_write_result(LOAD_MESSAGE, 0, 0))
		    return 1;

		free(rankmap);

		break;
	    }

	    case START_MESSAGE:
	    {
		if (debug_flags & DEBUG_INITFINI)
		    fprint(logfd, "START\n");

		if (cio_write_result(START_MESSAGE, 0, 0))
		    return 1;

		break;
	    }

	    case KILL_MESSAGE:
	    {
		int signo;
		int node;
		int pset_num = 0;
		int pset_size = 64;
		int sendwhat = CIO_NODE_STATUS_KILLED;

		USED(pset_size);
		USED(pset_num);
		if (stream_read_int(stdiofd, &signo)) {
		    fprint(logfd, "Error reading message on CIO stream!\n");
		    return 1;
		}

		if (debug_flags & DEBUG_INITFINI){
		    fprint(logfd, "KILL\n");
		    fprint(logfd, "signo: %d\n", signo);
		}
		
		/* TODO: for signo 9, send killing, for signo 15 send exited */
		if(signo == 9) 
			sendwhat = CIO_NODE_STATUS_KILLED;
		if(signo == 15)
			sendwhat = CIO_NODE_STATUS_EXITED;

		/* TODO: we'll need to set this in a different way */
		pset_size=64; /* HACK */
		
		for (node = 0; node < pset_size; node++){
		    int cpu;
		    for (cpu = 0;
			 cpu < (current_job_mode == M_SMP_MODE ? 1 :
				(current_job_mode == M_VIRTUAL_NODE_MODE ?
				 4 : 2));
			 cpu++) {
				if (cio_write_node_status(cpu,
						  node + pset_num * pset_size,
						  sendwhat, 0))

			    	return 1;
		    }
		}

		/* There is no reply to a KILL_MESSAGE.  */
		break;
	    }

	    case END_MESSAGE:
	    {
		if (debug_flags & DEBUG_INITFINI)
		    fprint(logfd, "END\n");

		if (cio_write_result(END_MESSAGE, 0, 0))
		    return 1;
		/* TODO: should we close the connection?) */
		close(0);
		return 0;
	    }

	    default:
		fprint(logfd, "Unknown message type %d on CIO stream!\n",
			prefix.type);
	} /* switch (prefix.type) */
    } /* for (;;) */
}


/*
 * Function to handle standard I/O operations
 * (for now focused on output, but we may want input threads as well)
 */
 
void iomain(void)
{
	logfd = create("/tmp/ciodata.log", OWRITE, 0666|DMAPPEND);
	
	/* TODO at some point we'll need an input thread */
	receive_data_loop();
	
	/* Message Output is BROKEN - debug input stream for now */
	debug_the_rest(stdiofd);
}
#define ESTR 255

static void
usage(void)
{
	char *e, estr[ESTR], *p;

	e = estr + ESTR;
	p = seprint(estr, e, "usage: %s"
		" -[I]: for Data Stream"
		"\n",
		argv0);
	write(2, estr, p-estr);
	exits("usage");
}


int main(int argc, char *argv[])
{
	/* TODO: check args and jump to iomain if appropriate */
	ARGBEGIN{
	default:
		usage();
		break;
	case 'I':
		iomain(); /* should never return */
		exits("strange");
		break;
	} ARGEND;
	
	logfd = create("/tmp/ciod.log", OWRITE, 0666|DMAPPEND);

	/* Bind system environment first */
	bind("#ec", "/env", MCREATE);

	return receive_cio_loop();
}


