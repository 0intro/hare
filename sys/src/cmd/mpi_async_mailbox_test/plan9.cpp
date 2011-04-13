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
	struct AmRing * amringsetup(int logN, int core);
	void amrstartup(struct AmRing *amr);
	int amrsend(struct AmRing *amr, void *data, int size, int rank);
	unsigned char *amrrecvbuf(struct AmRing *amr, unsigned char *p, int *rank);
	double MPI_Wtime(void);
	int MPI_Init(int *argc,char ***argv);
	extern int myproc, nproc;
};

using std::cout;
using std::endl;

int main(int argc, char** argv) {
	int test_for_termination = 0;
	MPI_Init(&argc, &argv);
	
	if(argc != 2) {
		if(myproc == 0) {
			std::cerr << "Usage: " << argv[0] << " <number of messages>" << std::endl;
		}
		return 0;
	}
	
	if (myproc) { std::cerr << "myproc > 0 so quit\n"; return 0;}
	struct AmRing *amring = amringsetup(20, 1);
	std::cerr << "EXIT EARLY\n";
	amrstartup(amring);	
	return 0;
	std::cerr << "EXIT EARLY\n";
	
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
	
	//Recv buffer
	unsigned char *buf;
	
	//Seed RNG
	boost::mt19937 rng((31415 + myproc*nproc));
	//distribution that maps to rank size
	boost::uniform_int<> rr(0,nproc-1);
	//'Glue' distribution to generator
	boost::variate_generator<boost::mt19937&, boost::uniform_int<> > 
		rand_rank(rng, rr);
	
	//CHK_MPI( MPI_Barrier(MPI_COMM_WORLD) );
	double start_time = MPI_Wtime();
	
	do {
		do {
			//If we haven't sent all local msgs yet
			if(count_msgs_local_sent < num_msgs_local) {
				++count_msgs_local_sent;
				//generate random destination rank
				int rank = rand_rank();
				//generate random payload 'int'
				int payload = rand_rank();
				amrsend(amring, &payload, sizeof(payload), rank);
				send_checksum_local += payload;
			}
			/* we only get one at a time for now */
			/* this will probably overrun */
			int rank;
			static unsigned char buf[256];
			if (amrrecvbuf(amring, buf, &rank)) {
				recv_checksum_local += *(uint64_t *)&buf[8];
			}
			
			//While we are still sending & recving
		} while (count_msgs_local_sent < num_msgs_local);

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

