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
	int sendonebeacon(struct AmRing *amr, int numnodes, unsigned char bdata);
};

using std::cout;
using std::endl;

/* test for termination
 * 
 * Honestly I don't remember the state of the code I sent you.  Here is what I
   do now for termination detection:
   
   All threads keep a count of #messages sent, received & processed.
   When count_received == count_processed (nothing locally left), we check test_for_termination
   
   1) If rank == 0, begin logarithmic tree sending signals to begin an
      asynchronous reduction.  Ranks only participate when they are locally
      empty, and reduce their counts up the tree.
   
   2) When the reduction completes, rank 0 has the global count of count_sent
      and count_processed.
   
   3) If they are equal, a second reduction occurs to check if messages
      overlapped with the asynchronous reduction.
   
   4) When 2 reductions have equal global counts for count_received and
      count_processed, rank 0 sends down another control signal to quit.
*/
int main(int argc, char** argv) {
	int debug = 0 + 6;
	int test_for_termination = 0;
	int sum_uptodate = 0;
	unsigned int *all_recv, *all_sent;
	unsigned int rtot, stot;
	MPI_Init(&argc, &argv);
	if(argc != 2) {
		if(myproc == 0) {
			std::cout << "Usage: " << argv[0] << " <number of messages>" << std::endl;
		}
		return 0;
	}
	
	std::cout << "myproc " << myproc << " nproc " << nproc << "\n";
	if (myproc >= nproc)
		while (1);
	amrdebug = 0;
	std::cout << "call amringsetup\n";
	struct AmRing *amring = amringsetup(20, 1);
	std::cout << "Call amrstartup\n";
	amrdebug = 0; //(! myproc) ? 5 : 0;
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
	uint64_t count_msgs_local_recv(0);
	uint64_t send_checksum_local(0);
	uint64_t recv_checksum_local(0);

	// data
	all_recv = (unsigned int *) mallocz(nproc * sizeof(*all_recv), 0);
	all_sent = (unsigned int *) mallocz(nproc * sizeof(*all_sent), 0);

	//Recv/send buffer
	unsigned char *rbuf, *xbuf;
	rbuf = (unsigned char *)mallocz(1024, 1);
	xbuf = (unsigned char *)mallocz(1024, 1);
	//Seed RNG
	boost::mt19937 rng((31415 + myproc*nproc));
	//distribution that maps to rank size
	boost::uniform_int<> rr(0,nproc-1);
	//'Glue' distribution to generator
	boost::variate_generator<boost::mt19937&, boost::uniform_int<> > 
		rand_rank(rng, rr);
	std::cout << "----------> start\n";
	amrdebug = 0;
	//CHK_MPI( MPI_Barrier(MPI_COMM_WORLD) );
	double start_time = MPI_Wtime();
	double last_time_sent = 0.;
	
	do {
		//std::cout << "count_msgs_local_sent " << count_msgs_local_sent << "  of num_msgs_local " << num_msgs_local << " sent\n";
		//If we haven't sent all local msgs yet
		if(count_msgs_local_sent < num_msgs_local) {
			++count_msgs_local_sent;
			all_sent[myproc]++;
			//generate random destination rank
			int rank = rand_rank();
			// don't send to myself ... 
			if (rank == myproc)
			    rank = (rank + 1) % nproc;
			//generate random payload 'int'
			int payload = rand_rank();
			if (debug > 2)
				std::cout << "--> send to " << rank << " count_msgs_local_sent " << count_msgs_local_sent << " num_msgs local " << num_msgs_local << "\n";
			xbuf[0] = 'm';
			memcpy(&xbuf[4], &payload, sizeof(payload));
			if (debug > 7) {
				int i;
				std::cout << "send ..";
				for(i = 0; i < 256; i++) printf("%02x ", xbuf[i]);
				std::cout << "\n";
			}

			amrsend(amring, xbuf, 240, rank);
			send_checksum_local += payload;
			if (debug > 2)
				std::cout << "Sent\n";
			if (debug > 2)
			cout << "count_msgs_local_sent " << count_msgs_local_sent  << " count_msgs_local_recv " << 
				count_msgs_local_recv << "\n";
		} else if (! myproc) {
			if ((MPI_Wtime() - last_time_sent) < 2.0)
				continue;
			if (debug > 2) 
				std::cout << "Send r beacon\n";
			sendonebeacon(amring, nproc, 'r');
			last_time_sent = MPI_Wtime();
		}
		/* we only get one at a time for now */
		/* this will probably overrun */
		int rank, num = 0, rx, ry, rz;
		amrdebug = 0; //(! myproc) ? 5 : 0;
		amrrecv(amring, &num, rbuf, &rx, &ry, &rz);
		amrdebug = 0;
		if (debug > 7) {
			int i;
			std::cout << "recv ..";
			for(i = 0; i < 256; i++) printf("%02x ", rbuf[i]);
			std::cout << "\n";
		}


		if (num) {
			int oldamrdebug;
			if (! count_msgs_local_recv)
				double start_time = MPI_Wtime();
			rank = xyztorank(rx, ry, rz);
			if (debug > 4)
				std::cout << "amrrecv gets " << num << " packets from " << rank << " msg is " << rbuf[16] << "\n";
			switch (rbuf[16]) {
				/* data */
			case 'm':
				if (debug > 2)
					std::cout << "Got 'm' from " << rank << "\n";
				recv_checksum_local += *(uint64_t *)&rbuf[20];
				count_msgs_local_recv++;
				all_recv[myproc]++;
				sum_uptodate = 0;
				break;
				/* reduction request from 0 */
			case 'r':
				if (debug > 2)
					std::cout << "Got an 'r' message " << count_msgs_local_sent << " / " << count_msgs_local_recv << " sum_uptodate " << sum_uptodate << "\n";
				if (sum_uptodate)
					continue;
				xbuf[0] = 'R';
				memcpy(&xbuf[4], &recv_checksum_local, sizeof(recv_checksum_local));
				*((unsigned int *)(&xbuf[4])) = count_msgs_local_sent;
				*((unsigned int *)(&xbuf[8])) = count_msgs_local_recv;
				//memcpy(&xbuf[4], &count_msgs_local_sent, sizeof(count_msgs_local_sent));
				//memcpy(&xbuf[8],&count_msgs_local_recv, sizeof(count_msgs_local_recv));
				oldamrdebug = amrdebug;
				if (debug > 7) {
					int i;
					std::cout << "Respond 'R': ";
					for(i = 0; i < 256; i++) printf("%02x ", xbuf[i]);
					std::cout << "\n";
				}
				amrdebug = 8;
				amrsend(amring, xbuf, 240, 0);
				amrdebug = oldamrdebug;
				sum_uptodate = 1;
				break;
				/* reduction response from non-0 */
			case 'R':
				if (debug > 2) {
					int i;
					std::cout << "got an 'R' from " << rank << "\n";
					for(i = 0; i < 256; i++) printf("%02x ", rbuf[i]);
					std::cout << "\n";
				}
				
				recv_checksum_local += *(uint64_t *)(&rbuf[20]);
				all_sent[rank] = *(uint *)(&rbuf[20]);
				all_recv[rank] = *(uint *)(&rbuf[24]);
				if (debug > 2) 
					std::cout << "all_sent " << all_sent[rank] << " all_recv[rank] " << all_recv[rank] << "\n";
				for(int i = rtot = stot = 0; i < nproc; i++){
					rtot += all_recv[i];
					stot += all_sent[i];
					std::cout << i << " rtot " << rtot << " stot " << stot << "\n";
				}
				if (debug > 2) 
					std::cout << " rtot " << rtot << " stot " << stot << "\n";
				if (rtot + stot > 2*num_msgs_global) {
					sendonebeacon(amring, nproc, 'q');
					test_for_termination = 1;
				}
				break;
			case 'q': 
				test_for_termination = 1;
				break;
			default: 
				std::cout << "Unknown message type" << rbuf[16] << "\n";
				break;
			}
			if (debug > 2)
				cout << "count_msgs_local_sent " << count_msgs_local_sent  << " count_msgs_local_recv " << 
					count_msgs_local_recv << " t for t " << test_for_termination << "\n";
		}

	} while(!test_for_termination);
	
		if (debug > 2)
			cout << "count_msgs_local_sent " << count_msgs_local_sent  << " count_msgs_local_recv " << 
				count_msgs_local_recv << "\n";
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

