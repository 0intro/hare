#include <iostream>
#include <boost/random.hpp>
#include <apgaf/mpi/mpi_mailbox_unbuffered.hpp>

using apgaf::mpi::mpi_mailbox_unbuffered;
using std::cout;
using std::endl;

int main(int argc, char** argv) {
  CHK_MPI( MPI_Init(&argc, &argv) );
  //Init the mailbox, defaults to MPI_COMM_WORLD
  //Message payload is a sinlge 'int'
  mpi_mailbox_unbuffered<int> mailbox;

  //Check Commandline
  if(argc != 2) {
    if(mailbox.comm_rank() == 0) {
      std::cerr << "Usage: " << argv[0] << " <number of messages>" << std::endl;
    }
    return 0;
  }

  //Compute number of msgs to send locally
  uint64_t num_msgs_global = atoll(argv[1]);
  uint64_t num_msgs_local = (num_msgs_global / mailbox.comm_size());
  if(int(num_msgs_global % mailbox.comm_size()) < mailbox.comm_rank())
    ++num_msgs_local;

  //Print out alittle status msg
  if(mailbox.comm_rank() == 0) {
    std::cout << "Sending " << atoll(argv[1]) << " unbuffered messages using "
              << mailbox.comm_size() << " processes" << std::endl;
  }

  //Local msgs counters
  uint64_t count_msgs_local_sent(0);
  uint64_t send_checksum_local(0);
  uint64_t recv_checksum_local(0);
  
  //Recv buffer
  std::vector<int> vec_recv_buffer;

  //Seed RNG
  boost::mt19937 rng((31415 + mailbox.comm_rank())*mailbox.comm_rank());
  //distribution that maps to rank size
  boost::uniform_int<> rr(0,mailbox.comm_size()-1);
  //'Glue' distribution to generator
  boost::variate_generator<boost::mt19937&, boost::uniform_int<> > 
                rand_rank(rng, rr);

  //Start Time
  CHK_MPI( MPI_Barrier(MPI_COMM_WORLD) );
  double start_time = MPI_Wtime();

  do {
    do {
      //If we haven't sent all local msgs yet
      if(count_msgs_local_sent < num_msgs_local) {
        if(!mailbox.check_pending_isend()) {
          ++count_msgs_local_sent;
          //generate random destination rank
          int rank = rand_rank();
          //generate random payload 'int'
          int payload = rand_rank();
          mailbox.send(rank, payload);
          send_checksum_local += payload;
        }
      }

      //Check for incomming messages
      vec_recv_buffer.clear();
      mailbox.recv(vec_recv_buffer);
      for(std::vector<int>::const_iterator citr = vec_recv_buffer.begin();
          citr != vec_recv_buffer.end(); ++citr) {
        recv_checksum_local += *citr;
      }

    //While we are still sending & recving
    } while (count_msgs_local_sent < num_msgs_local || 
             vec_recv_buffer.size() > 0);

  //While we haven't globally terminated yet
  } while(!mailbox.test_for_termination());

  //Stop Time
  CHK_MPI( MPI_Barrier(MPI_COMM_WORLD) );
  double stop_time = MPI_Wtime();

  //verify global checksum
  uint64_t recv_checksum_global(0);
  CHK_MPI( MPI_Allreduce( (void*) &recv_checksum_local, 
                          (void*) &recv_checksum_global, 1, MPI_UNSIGNED_LONG, 
                          MPI_SUM, MPI_COMM_WORLD) );
  uint64_t send_checksum_global(0);
  CHK_MPI( MPI_Allreduce( (void*) &send_checksum_local, 
                          (void*) &send_checksum_global, 1, MPI_UNSIGNED_LONG, 
                          MPI_SUM, MPI_COMM_WORLD) );
  assert(send_checksum_global == recv_checksum_global);
  assert(count_msgs_local_sent == num_msgs_local);
 
  //Print stats
  if(mailbox.comm_rank() == 0) {
    double elapsed_time = stop_time - start_time;
    cout << "Elapsed time = " << stop_time - start_time
         << ", Message rate = " << double(num_msgs_global)/elapsed_time
         << " Messages per second" << endl;
  }

  //Thats all!
  CHK_MPI( MPI_Finalize());
  return 0;
}

