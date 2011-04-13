#ifndef APGAF_MPI_MAILBOX_UNBUFFERED_HPP_INCLUDED
#define APGAF_MPI_MAILBOX_UNBUFFERED_HPP_INCLUDED

#include <vector>
#include <apgaf/mpi/mpi.h>
#include <apgaf/mpi/mpi_async_termination_detection_tree.hpp>

namespace apgaf { namespace mpi {

//This is a 'dirt simple' MPI mailbox that is
//*unbuffered*.  This is for evaluation
//purposes only, and is not expected to perform
//well on 'standard linux IB clusters'.
//
// Interfaces:  send (dest_rank, msg)
//              recv (vector<msg>& msgs)
//              bool test_for_termination()
template <typename MsgType>
class mpi_mailbox_unbuffered {
private:
public:
  typedef MsgType message_type;
  
  //Constructs w/ defaults!
  mpi_mailbox_unbuffered() :
          m_termination_detection(MPI_COMM_WORLD, 2, 2, 3, 4) {
    m_mpi_comm = MPI_COMM_WORLD;
    m_msg_tag = 1;
    m_b_pending_isend = false;
    m_num_local_sent = 0;
    m_num_local_recv = 0;
    CHK_MPI(MPI_Comm_rank(m_mpi_comm, &m_mpi_rank));
    CHK_MPI(MPI_Comm_size(m_mpi_comm, &m_mpi_size));
    internal_post_irecv();
  }

  
  mpi_mailbox_unbuffered(MPI_Comm in_mpi_comm,
                         int in_msg_tag,
                         int in_tree_children,
                         int in_query_status_tag,
                         int in_query_response_tag,
                         int in_terminate_tag):
                         m_termination_detection(in_mpi_comm,
                                                 in_tree_children, 
                                                 in_query_status_tag,
                                                 in_query_response_tag,
                                                 in_terminate_tag) {
    m_mpi_comm = in_mpi_comm;
    m_b_pending_isend = false;
    m_num_local_sent = 0;
    m_num_local_recv = 0;
    CHK_MPI(MPI_Comm_rank(m_mpi_comm, &m_mpi_rank));
    CHK_MPI(MPI_Comm_size(m_mpi_comm, &m_mpi_size));
    m_msg_tag = in_msg_tag;
    internal_post_irecv();
  }

  ~mpi_mailbox_unbuffered() {
    CHK_MPI( MPI_Cancel(&m_irecv_request) );
  }

  int comm_rank() const {return m_mpi_rank;}
  int comm_size() const {return m_mpi_size;}

  

  void send(int in_dest_rank, const message_type& in_msg) {
    //Increment local send counter
    ++m_num_local_sent;
    //Wait for existing pending isends to complete
    //poll for recvs using internal recv buffer
    while(check_pending_isend()) {
      //std::cout << m_mpi_rank << ": send isn't ready!" << std::endl;
      //Make this a parameter, needed to prevent buffer overlow
      //WARNING: could cause global deadlock if set too small!!!
      if(m_vec_internal_recv_buffer.size() < 100000)
        internal_recv(m_vec_internal_recv_buffer);
    }
    //Post the new isend
    internal_post_isend(in_dest_rank, in_msg);
  }

  size_t recv(std::vector<message_type>& out_msg_vec) {
    size_t count_to_return(0);
    //Clear external recv buffer
    out_msg_vec.clear();
    //Copy internal buffer to external recv buffer
    std::copy(m_vec_internal_recv_buffer.begin(),
              m_vec_internal_recv_buffer.end(), 
              std::back_inserter(out_msg_vec));
    //Clear interanl recv buffer
    m_vec_internal_recv_buffer.clear();
    //Check for new msgs
    while(internal_recv(out_msg_vec) > 0) { }
    //Increment local recv counter 
    count_to_return = out_msg_vec.size();
    m_num_local_recv += count_to_return;
    return count_to_return;
  }

  bool test_for_termination() {
    if(check_pending_isend()) return false;
    if(m_termination_detection.test_for_termination(m_num_local_sent,
                                                    m_num_local_recv)) {
      return true;
    }
    return false;
  }
  
  //Returns true if an MPI_Isend is still pending
  bool check_pending_isend() {
    if(!m_b_pending_isend) return false;
    int flag(0);
    CHK_MPI( MPI_Test( &m_isend_request, &flag, MPI_STATUS_IGNORE) );
    if(flag == 1) {
      //isend just completed!
      m_b_pending_isend = false;
    }
    return (flag == 0); //retuns true if send is incomplete
  }
private:

  size_t internal_recv(std::vector<message_type>& out_msg_vec) {
    //do recv here!
    int flag(0);
    CHK_MPI( MPI_Test( &m_irecv_request, &flag, MPI_STATUS_IGNORE) );
    if(flag == 1) {
      out_msg_vec.push_back(m_irecv_buffer);
      internal_post_irecv();
      return 1;
    }
    return 0;
  }

  void internal_post_isend(int in_dest_rank, const message_type& in_msg) {
    //Copy msg to internal send buffer
    m_isend_buffer = in_msg;
    //Post MPI_Isend
    CHK_MPI( MPI_Isend( (void*) &m_isend_buffer, sizeof(message_type),
                        MPI_BYTE, in_dest_rank, m_msg_tag, m_mpi_comm,
                        &m_isend_request) );
    //Set pending isend flag to true
    m_b_pending_isend = true;
  }

  void internal_post_irecv() {
    //Post new Irecv
    CHK_MPI( MPI_Irecv( (void*) &m_irecv_buffer, sizeof(message_type),
                        MPI_BYTE, MPI_ANY_SOURCE, m_msg_tag, 
                        m_mpi_comm, &m_irecv_request) ); 
  }

  MPI_Comm m_mpi_comm;
  int m_msg_tag;
  int m_mpi_rank;
  int m_mpi_size;

  ///Internal message recv buffer
  std::vector<message_type> m_vec_internal_recv_buffer;

  ///MPI_Isend buffers and requests
  MPI_Request m_isend_request;
  bool        m_b_pending_isend;
  message_type m_isend_buffer;

  ///MPI_Irecv buffers and requests
  MPI_Request m_irecv_request;
  message_type m_irecv_buffer;

  uint64_t m_num_local_sent;
  uint64_t m_num_local_recv;

  async_termination_detection_tree<uint64_t> m_termination_detection;
};

}} //end namespace apgaf { namespace mpi {
#endif //APGAF_MPI_MAILBOX_UNBUFFERED_HPP_INCLUDED
