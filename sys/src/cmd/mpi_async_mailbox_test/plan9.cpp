#include <iostream>
#include <boost/random.hpp>

struct AmRing {
	unsigned char *base;
	int size;
	int prod;
	int logN;
	unsigned char _116[128-16];
	int con;
	unsigned char _124[128-4];
};

extern "C" {
	int xyztorank(int x, int y, int z);
	struct AmRing * amringsetup(int logN, int core);
	void amrstartup(struct AmRing *amr);
	int amrsend(struct AmRing *amr, void *data, int size, int rank);
	unsigned char *amrrecvbuf(struct AmRing *amr, unsigned char *p, int *rank);
	void * amrrecv(struct AmRing *amr, int *num, void *p, int *x, int *y, int *z);
	double MPI_Wtime(void);
	int MPI_Init(int *argc,char ***argv);
	extern int myproc, nproc, amrdebug;
	extern void *mallocz(int size, int zero);
};

using std::cout;
using std::endl;

int main(int argc, char** argv) {
	int debug = 3;
	int test_for_termination = 0;
	MPI_Init(&argc, &argv);
	if(argc != 2) {
		if(myproc == 0) {
			std::cerr << "Usage: " << argv[0] << " <number of messages>" << std::endl;
		}
		return 0;
	}
	
	std::cerr << "myproc " << myproc << " nproc " << nproc << "\n";
	if (myproc >= nproc)
		while (1);
	std::cerr << "call amringsetup\n";
	struct AmRing *amring = amringsetup(20, 1);
	std::cerr << "Call amrstartup\n";
	amrstartup(amring);	

	//Compute number of msgs to send locally
	uint64_t num_msgs_global = atoll(argv[1]);
	uint64_t num_msgs_local = (num_msgs_global / nproc);
	if(int(num_msgs_global % nproc) < myproc)
		++num_msgs_local;
	
	
	if(myproc == 0) {
		std::cout << "Sending " << atoll(argv[1]) 
			  << " unbuffered messages using "
			  << nproc << " processes" << std::endl;
	}
	
	//Local msgs counters
	uint64_t count_msgs_local_sent(0);
	uint64_t send_checksum_local(0);
	uint64_t recv_checksum_local(0);
	
	//Recv/send buffer
	unsigned char *buf;
	buf = (unsigned char *)mallocz(1024, 1);
	//Seed RNG
	boost::mt19937 rng((31415 + myproc*nproc));
	//distribution that maps to rank size
	boost::uniform_int<> rr(0,nproc-1);
	//'Glue' distribution to generator
	boost::variate_generator<boost::mt19937&, boost::uniform_int<> > 
		rand_rank(rng, rr);
	std::cerr << "----------> start\n";
	amrdebug = 0 + 4;
	//CHK_MPI( MPI_Barrier(MPI_COMM_WORLD) );
	double start_time = MPI_Wtime();
	
	do {
		//std::cerr << "count_msgs_local_sent " << count_msgs_local_sent << " num_msgs_local " << num_msgs_local << "\n";
		//If we haven't sent all local msgs yet
		if(count_msgs_local_sent < num_msgs_local) {
			++count_msgs_local_sent;
			//generate random destination rank
			int rank = rand_rank();
			//generate random payload 'int'
			int payload = rand_rank();
			if (debug > 2)
				std::cerr << "--> send to " << rank << " count_msgs_local_sent " << count_msgs_local_sent << " num_msgs local " << num_msgs_local << "\n";
			buf[0] = 'm';
			memcpy(&buf[4], &payload, sizeof(payload));
			amrsend(amring, buf, 240, rank);
			send_checksum_local += payload;
			if (debug > 2)
				std::cerr << "Sent\n";
		}
		/* we only get one at a time for now */
		/* this will probably overrun */
		int rank, num, rx, ry, rz;
		amrrecv(amring, &num, buf, &rx, &ry, &rz);
		rank = xyztorank(rx, ry, rz);
		if (debug > 4)
			std::cerr << "amrrecv gets " << num << " packets from " << rank << "\n";
		if (num && buf[0] == 'm') {
			if (debug > 2)
				std::cerr << "Got something! from " << rank << "\n";
			recv_checksum_local += *(uint64_t *)&buf[16];
		}
			
	} while(!test_for_termination);
	
/* need to figure out barriers */
//Stop Time
//	CHK_MPI( MPI_Barrier(MPI_COMM_WORLD) );
	double stop_time = MPI_Wtime();
	
//verify global checksum
	uint64_t recv_checksum_global(0);
//	CHK_MPI( MPI_Allreduce( (void*) &recv_checksum_local, 
//				(void*) &recv_checksum_global, 1, MPI_UNSIGNED_LONG, 
//				MPI_SUM, MPI_COMM_WORLD) );
	uint64_t send_checksum_global(0);
//	CHK_MPI( MPI_Allreduce( (void*) &send_checksum_local, 
//				(void*) &send_checksum_global, 1, MPI_UNSIGNED_LONG, 
//				MPI_SUM, MPI_COMM_WORLD) );
	assert(send_checksum_global == recv_checksum_global);
	assert(count_msgs_local_sent == num_msgs_local);
	
//Print stats
	if(myproc == 0) {
		double elapsed_time = stop_time - start_time;
		cout << "Elapsed time = " << stop_time - start_time
		     << ", Message rate = " << double(num_msgs_global)/elapsed_time
		     << " Messages per second" << endl;
	}
	
	
	return 0;
}

