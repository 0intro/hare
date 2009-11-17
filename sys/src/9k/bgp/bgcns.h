/* --------------------------------------------------------------- */
/*  (C) Copyright IBM Corp.  2007, 2007                            */
/*  IBM CPL License                                                */
/* --------------------------------------------------------------- */



/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *   Blue Gene Common Node Services Interface
 *
 *     $Source: /BGP/CVS/bgp/bgcns/common/bgcns.h,v $
 *     $Revision: 1.34 $
 *     $Date: 2008/05/01 16:38:09 $
 *     $Author: tmusta $
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  History:
 *  ~~~~~~~~~
 *
 *  $Log: bgcns.h,v $
 *  Revision 1.34  2008/05/01 16:38:09  tmusta
 *  Issue 4422: Intermittent Rx Link Failures
 *
 *  Revision 1.33  2008/04/21 13:42:32  tmusta
 *  Issue 4136: Doxygen Documenation for DMA Services (per ANL request)
 *
 *  Revision 1.32  2008/03/26 21:33:49  tgooding
 *  Issue 4685: EDRAM cycle reproducibility support
 *
 *  Revision 1.31  2008/03/04 22:25:23  tmusta
 *  Issue 4459: FS Mount Pacing Support for Linux
 *
 *  Revision 1.30  2008/02/22 14:47:47  tmusta
 *  Issue 3986: Updates to versioning support
 *
 *  Revision 1.29  2008/02/11 19:20:15  tmusta
 *  Issue 3986: Adding mapDevice service
 *
 *  Revision 1.28  2008/02/01 21:35:06  tmusta
 *  Issue 3869: Many Torus Errors on Large Blocks
 *
 *  Revision 1.27  2008/01/08 21:22:18  tmusta
 *  Issue 3986: Adding non-blocking versions of Ethernet services
 *
 *  Revision 1.26  2007/12/20 15:53:30  tmusta
 *  Issue 3504: Eliminating remaining mailbox includes from CNK
 *
 *  Revision 1.25  2007/10/10 22:09:03  jjparker
 *  Issue 1813: Handle remote get fifo full condition
 *
 *  Revision 1.24  2007/10/01 18:02:52  tmusta
 *  Issue 3583: clarifying CNS requirements on RAS ASCII strings.
 *
 *  Revision 1.23  2007/07/18 18:52:23  tgooding
 *  Issue 1343,1481,2659: Send end-of-job RAS correctable counters and reset
 *
 *  Revision 1.22  2007/07/16 04:05:12  tgooding
 *  Issue 1578: Add DMA range protection support to CNS/CNK
 *
 *  Revision 1.21  2007/06/12 19:45:25  tgooding
 *  Issue 411: Synchronize timebases on single node reboot
 *
 *  Revision 1.20  2007/06/11 15:30:04  tmusta
 *  Issue 2522: Fixing Copyright for bgcns.h
 *
 *  Revision 1.19  2007/06/06 16:45:41  tmusta
 *  Issue 2382: RAS errors stop dma/torus diagnostic under mm
 *
 *  Revision 1.18  2007/06/05 13:43:00  tmusta
 *  Issue 2313: Make mailbox return signatures consistent
 *
 *  Revision 1.17  2007/05/30 02:12:01  tmusta
 *  Issue 2392: _bgp_GetSerDesLinkStatus is broken
 *
 *  Revision 1.16  2007/05/21 14:54:13  tmusta
 *  Issue 2231: CNS cleanup
 *
 *  Revision 1.15  2007/05/14 19:30:23  tmusta
 *  Issue 2186: Adding non-blocking RAS calls for Linux
 *
 *  Revision 1.14  2007/05/14 17:10:18  tmusta
 *  Issue 1157: Final CNK cleanup (CNS)
 *
 *  Revision 1.13  2007/05/14 02:51:31  tgooding
 *  Issue 1950: add reactive power throttling
 *
 *  Revision 1.12  2007/05/11 14:36:23  tmusta
 *  Issue 1737: CNS support for I/O node reboot
 *
 *  Revision 1.11  2007/05/03 22:06:28  jjparker
 *  Issue 1646: Add DMA interrupt support in messaging
 *
 *  Revision 1.10  2007/05/03 17:33:55  tmusta
 *  Issue 2008: Migrating Broadcom IP into CNS
 *
 *  Revision 1.9  2007/05/03 14:57:44  tmusta
 *  Issue 2038: Adding copyrights to CNS
 *
 *  Revision 1.8  2007/04/30 11:50:47  tmusta
 *  Issue 1975: netbus, torus, collective RAS code to CNS
 *
 *  Revision 1.7  2007/04/24 20:41:25  tmusta
 *  Issue 1867: Move DMA into CNS
 *
 *  Revision 1.6  2007/04/06 12:36:08  tmusta
 *  Issue 1422: RAS Storm Filter
 *
 *  Revision 1.5  2007/03/22 12:40:08  tmusta
 *  Issue 1066: support for non-blocking mailbox writes
 *
 *  Revision 1.4  2007/03/09 21:56:51  tmusta
 *  Issue 1157: Migration of CNK onto CNS - Phase III (mailbox and interrupts)
 *
 *  Revision 1.3  2007/01/12 18:56:26  tmusta
 *  Issue 937: Migrate Linux to CNS
 *
 *  Revision 1.2  2006/12/01 14:18:45  tmusta
 *  Assorted updates to CNS
 *
 *  Revision 1.1  2006/11/14 20:09:38  tmusta
 *  Issue 540: initial drop of Common Node Services
 *
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */


#ifndef _BGCNS_H
#define _BGCNS_H


#ifndef __ASSEMBLY__

/*! @page CNS Common Node Services
 *
 *  @section CNS_S10 Overview
 *
 *  As the name implies, the <b>Common Node Services (CNS)</b> layer provides @b services
 *  to the kernel.  These services may be simple queries abstracting various node 
 *  specific data (such as DDR size) or they may be more sophisticated software services, 
 *  such as common machine check handling.  Additionally, some services may be implicit, 
 *  such as the initialization of various hardware devices unique to Blue Gene, such as 
 *  Netbus and SerDes.
 * 
 *  Services are not directly linked into the kernel, but rather are invoked from kernel 
 *  code via a <b>service directory</b> which is itself part of an overall <b>service 
 *  descriptor</b>.  This service descriptor is constructed during initialization and 
 *  is passed to the kernel when the kernel is booted.  The service directory is a 
 *  collection of <b>service references</b>.  
 *
 *  During partition (block) booting, ELF images are loaded onto the compute and I/O nodes.
 *  The bootloader (@i aka microloader) boots first and then transfers control to the Common
 *  Node Services layer so that it, in turn, may boot.
 *
 *  Once the CNS layer has booted, control is transferred to the kernel so that it may also 
 *  boot.  All services provided by the CNS layer are immediately available at this time.
 *
 *  @section CNS_S20 Programming Model
 *
 *  A kernel running on top of the CNS layer is not statically linked to the common services.  
 *  Instead, the services are called via function pointers provided by the services descriptor, 
 *  which is described here:  @ref _BGCNS_ServiceDirectory.
 *
 *  The kernel must operate under the following rules and restrictions:
 *  @li The kernel must not alter the services descriptor.  The descriptor must be treated as a read-only 
 *      data structure even though the kernel may have the ability to alter it.  Because CNS trusts the 
 *      kernel, this also implies that the kernel must not expose the descriptor to any untrusted 
 *      software (such as application code).
 *  @li The kernel must ensure that the CNS virtual memory region is mapped prior to invoking any 
 *      service. 
 *  @li The kernel must ensure that any data passed to services via parameters is mapped.  
 *      Specifically, TLB entries must be mapped as shared (TID = 0) and must be either readable 
 *      (input parameters) or readable and writeable (output parameters).
 *  @li The kernel must treat the virtual address range (@ref _BGCNS_Descriptor::baseVirtualAddress , 
 *      _BGCNS_Descriptor::baseVirtualAddress + @ref _BGCNS_Descriptor::size - 1)  as reserved.
 *      That is, the kernel must not use this region of virtual memory for anything besides accessing
 *      the services descriptor.
 *  @li The kernel must treat the physical address range (@ref _BGCNS_Descriptor::basePhysicalAddress,
 *      _BGCNS_Descriptor::basePhysicalAddress + _BGCNS_Descriptor::size - 1) as reserved.  The
 *      kernel must not map this memory for any other use.
 *  @li The kernel must not access any of the reserved virtual address regions with TLB settings that 
 *      are different from those used by CNS.  The kernel is allowed to unmap any of the reserved 
 *      memory TLBs for its own use.  However, in such a case and per the rule above, the kernel must 
 *      ensure that the region is mapped prior to using any CNS facilities (such as invoking a service).
 *  @li CNS may need to map one or more TLB entries in order to access Blue Gene devices.  In such a case,
 *      CNS may borrow TLB entries; the TLB will be returned to its original state before the service returns
 *      control to the invoking kernel.  Kernels may avoid this behavior for specific devices by using 
 *      the mapDevice service.
 *  @li The kernel's ELF image must avoid the 256K region of memory between 0x07000000 and 0x0703FFFF.  This 
 *      region is used for the pre-relocated CNS image and will be available for general use once CNS boot
 *      is complete.
 *  @li The kernel must not alter any reserved SPRs, DCRs or memory-mapped device registers.
 *
 *  The CNS software may behave unpredictably if any of these rules and restrictions is violated.
 *
 *  Kernels may make the following assumptions about CNS:
 *
 *  @li The data passed in the firmware descriptor (see below) is static.  Specifically, the base addresses, 
 *      size and service directory will not change once CNS boot is complete.
 *
 *  @subsection CNS_21 Programming Examples
 *
 *  @subsubsection CNS_211 Obtaining the Personality
 *
 *  The following example shows how to fetch a copy of the Blue Gene personality structure and also
 *  serves as a simple example of invoking a service:
 *
 *  @code
 *
 *      BGCNS_Descriptor* descr = ...; // obtained from CNS at boot time
 *     _BGP_Personality_t* pers = (_BGP_Personality_t*)(*descr->services->getPersonalityData)();
 *     ...
 *  @endcode
 *
 *  The programming model guarantees that the descriptor is static.  Thus, one can provide a 
 *  convenience method to make service invocation a little more readable
 *
 *  @code
 *
 *
 *  static BGCNS_Descriptor* _cns_descriptor = ...; // obtained from CNS at boot time
 *  
 *  inline BGCNS_ServiceDirectory* cns() { return _cns_descriptor; }
 *
 *  void foo() {
 *     _BGP_Personality_t* pers = (_BGP_Personality_t*)cns()->getPersonalityData();
 *     ...
 *  }
 *
 *  @endcode
 *
 *  This style will be used in all of the subsequent examples.
 *
 *  @subsubsection CNS_212 SMP Initialization
 *
 *  Common Node Services will launch the kernel on a single core (typically core 0) and will 
 *  leave the remaining cores parked.  The kernel can activate additional cores via the @c takeCPU 
 *  service.  Here is a very simple example of such kernel code:
 *
 *  @code
 *
 *    void anEntryPoint(unsigned core, void* arg_not_used) {
 *        // Do whatever your kernel needs to do here.  Typically,
 *        // this function never returns.  You will arrive here
 *        // when takeCPU is invoked (below).
 *    }
 *
 *    void someCodeOnTheMainThread() {
 *
 *        // ...
 *
 *        unsigned N = cns()->getNumberOfCores();
 *
 *        for (core = 1; core < N; core++) {
 *            if ( cns()->takeCPU(core, NULL, &anEntryPoint) != 0 ) {
 *                // error handling goes here
 *            }
 *        }
 *
 *        // ...
 *    }
 *
 *  @endcode
 *
 *  @subsubsection CNS_213 Version Compatibility
 *
 *  Common Node Services structures and APIs should remain compatible within maintenance
 *  releases and e-fixes.  Kernel's may add a runtime check to ensure that the version
 *  of CNS is compatible with the version from compile time.  This is done as follows:
 *
 *  @code
 *
 *      BGCNS_Descriptor* descr = ...; // obtained from CNS at boot time
 *
 *      if ( ! BGCNS_IS_COMPATIBLE(descr) ) {
 *           // incompatible CNS (panic?)
 *      }
 *
 *  @endcode
 *
 *  @subsubsection CNS_23 Interrupts
 *
 *  A kernel wanting to use the CNS interrupt services would first have to enable interrupts 
 *  for the appropriate Blue Gene BIC group and IRQ within that group.  This would likely be 
 *  done at boot time.  So, for example, such a kernel could enable interrupts for the Universal 
 *  Performance Counter (group 5, IRQ 2) to be handled by the non-critical handler of core 0 as 
 *  follows:
 *
 *  @code
 *      cns()->enableInterrupt(5, 2, BGCNS_NonCritical, 0);
 *  @endcode
 *
 *  Such a kernel might also maintain a collection of routines that act as subhandlers of the 
 *  non-critical interrupt handler.  In this example, we'll assume it is simply a two 
 *  dimensional array indexed by group and IRQ:
 *
 *  @code
 *      subhandlers[5][2] = &theUpcSubHandler;
 *  @endcode
 *
 *  That kernel's non-critical interrupt handler would then typically handle all interrupts by 
 *  successively invoking the getInterrupt() service to determine the group and IRQ, and then 
 *  dispatching the appropriate subhandler.  Additionally, the interrupt will be acknowledged 
 *  so to avoid continuous interruption:
 *
 *  @code
 *      unsigned grp, irq;
 *
 *      while ( cns()->getInterrupt(&g, &i, BGCNS_NonCritical) == 0) {
 *          (*subhandlers[g][i])(); // dispatch the handler
 *          cns()->acknowledgeInterrupt(g,i); // ack the interrupt
 *      }
 *  @endcode
 *
 *  @subsubsection CNS_24 Global Barriers and Interrupts
 *
 *  The Blue Gene/P Global Interrupt Controller (aka GLINT) provides 4 independent channels
 *  that may be configured as either a global barrier or a global interrupt.
 *
 *  Barriers are constructed by invoking the barrier service:
 *
 *  @code
 *      unsigned channel = 0;
 * 
 *      // synchronize:
 *      int reset = 1;
 *      int rc;
 *      while ( (rc = cns()->globalBarrier_nonBlocking(channel, reset, 1000)) == BGCNS_RC_CONTINUE ) {
 *        reset = 0;
 *      }
 *
 *      if ( rc == BGCNS_RC_COMPLETE ) {
 *        // good path
 *      }
 *      else {
 *        // error
 *      }
 *  @endcode
 *
 *  Similarly, a barrier with a timeout can also be constructed:
 *
 *  @code
 *      unsigned channel = 0;
 *      int reset = 1;
 *      unsigned long long startTime = ...; // obtain current time
 *      int rc;
 * 
 *      while ( (rc = cns()->globalBarrier_nonBlocking(channel,reset, 1000)) == BGCNS_RC_CONTINUE ) {
 *         reset = 0;
 *         unsigned long long currentTime = ...; // obtain current time
 *         if ( currentTime - startTime > timeout )
 *           break;
 *      }
 *
 *      if ( rc == BGCNS_RC_COMPLETE )  {
 *        // good path
 *      }
 *      else {
 *        // timeout or error
 *      }
 *  @endcode
 *
 *  A node may opt out of a barrier channel via the disableBarrier service:
 *
 *  @code
 *
 *    // some other synchronization mechanism needs to go here
 *
 *    cns()->disableBarrier(channel);
 *
 *  @endcode  
 *
 *  Conversely, it may opt back in:
 *
 *  @code
 *    cns()->enableBarrier(channel, user_mode);
 *  @endcode
 *
 *  By default, CNS reserves the use of channel 2 as a global interrupt for environmental 
 *  monitoring.  It also reserves channel 3 for use as a supervisory mode, compute-node 
 *  only barrier.  Compute node kernels are free to share this channel for the same
 *  purpose (compute node, supervisory barrier).  The enable/disable barrier services
 *  may return errors if operating on a reserved channel.
 *  
 *  NOTE: The standard BG/P software stack, which includes I/O node Linux and Compute Node 
 *  Kernel (CNK) uses channel 0 as an I/O node barrier during boot and transforms it to an 
 *  compute-node only barrier when jobs execute.
 *
 *
 *  @section CNS_3 DMA Services
 *  
 *  The DMA services provided in CNS are low-level services.  Interested readers of this area should
 *  also look at the documentation for the DMA SPIs, which are at a slightly higher level.
 *
 *
 *
 *  @section CNS_4 Reserved and Preferred Addresses
 *
 *
 *  The following virtual memory regions are reserved and must be avoided by 
 *  kernels:
 *
 *  @code
 *
 *  +------------+------------+------+----------------------+-----------------------+
 *  | Lower      | Upper      | Size | Usage                | Attributes            |
 *  +------------+------------+------+----------------------+-----------------------+
 *  | CNSlow[1]  | CNShigh[2] | 256K | CNS                  | I, Rs, Ws, Xs         |
 *  +------------+------------+------+----------------------+-----------------------+
 *
 *    [1] CNSlow  = descr->baseVirtualAddress , usually 0xFFF40000
 *    [2] CNShigh = descr->baseVirtualAddress + descr->size - 1;  usually 0xFFF7FFFF
 *
 *  @endcode
 *
 *  The following virtual memory regions are used by default in CNS.  Kernels that wish to have
 *  a different memory map may do so via the mapDevice service.
 *
 *  @code
 *  +------------+------------+------+----------------------+-----------------------+
 *  | Lower      | Upper      | Size | Usage                | Attributes            |
 *  +------------+------------+------+----------------------+-----------------------+
 *  | 0xFFFB0000 | 0xFFFCFFFF |  64K | Torus                | I, G, Rs, Ws, Ru, Wu  |
 *  +------------+------------+------+----------------------+-----------------------+
 *  | 0xFFFD0000 | 0xFFFD3FFF |  16K | DMA                  | I, G, Rs, Ws, Ru, Wu  |
 *  +------------+------------+------+----------------------+-----------------------+
 *  | 0xFFFD9000 | 0xFFFD9FFF |   4K | DevBus               | I, G, Rs, Ws          |
 *  +------------+------------+------+----------------------+-----------------------+
 *  | 0xFFFDA000 | 0xFFFDAFFF |   4K | UPC                  | I, G, Rs, Ws, Ru, Wu  |
 *  +------------+------------+------+----------------------+-----------------------+
 *  | 0xFFFDC000 | 0xFFFDD3FF |   4K | Collective           | I, G, Rs, Ws, Ru, Wu  |
 *  +------------+------------+------+----------------------+-----------------------+
 *  | 0xFFFDE000 | 0xFFFDEFFF |   4K | BIC                  | I, G, Rs, Ws, Xs      |
 *  +------------+------------+------+----------------------+-----------------------+
 *  | 0xFFFF0000 | 0xFFFF3FFF |  16K | Lockbox (supervisor) | I, G, Rs, Ws          |
 *  +------------+------------+------+----------------------+-----------------------+
 *  | 0xFFFF4000 | 0xFFFF7FFF |  16K | Lockbox (user)       | I, G, Rs, Ws, Ru, Wu  |
 *  +------------+------------+------+----------------------+-----------------------+
 *  | 0xFFFF8000 | 0xFFFFFFFF |  32K | SRAM                 | SWOA, WL1, Rs, Ws, Xs |
 *  +------------+------------+------+----------------------+-----------------------+
 *  @endcode
 *
 */


#define BGCNS_VERSION 0x01020000 /* V1R2M0 efix 0 */ 
#define BGCNS_IS_COMPATIBLE(descr) ( ((descr)->version & 0xFFFF0000) == (BGCNS_VERSION & 0xFFFF0000) ) //!< True iff the given descriptor is compatible with this version of CNS

//! @enum  BGCNS_InterruptType 
//! @brief Defines the different types of interrupts known to 
//!        Common Node Services.
typedef enum  {
    BGCNS_NonCritical,     //!< Non-critical interrupt
    BGCNS_Critical,        //!< Critical interrupt
    BGCNS_MachineCheck,    //!< Machine check
} BGCNS_InterruptType;

//! @enum   BGCNS_FifoOperation
//! @brief  Defines the types of FIFO operations
//! @see    _BGCNS_ServiceDirectory::setDmaFifoControls 
//! @see    _BGCNS_ServiceDirectory::setDmaLocalCopies
//! @see    _BGCNS_ServiceDirectory::setDmaPriority
typedef enum {
    BGCNS_Disable = 0,   
    BGCNS_Enable = 1, 
    BGCNS_Reenable = 2 
} BGCNS_FifoOperation;

//! @enum BGCNS_FifoFacility
//! @brief Defines the various types of FIFO facilities
typedef enum {
    BGCNS_InjectionFifo,                 //!< Normal Injection FIFO
    BGCNS_ReceptionFifo,                 //!< Normal Reception FIFO
    BGCNS_ReceptionHeaderFifo,           //!< Reception Header FIFO (typically used only for debugging)
    BGCNS_InjectionFifoInterrupt,        
    BGCNS_ReceptionFifoInterrupt, 
    BGCNS_ReceptionHeaderFifoInterrupt,
    BGCNS_InjectionCounterInterrupt, 
    BGCNS_ReceptionCounterInterrupt
} BGCNS_FifoFacility;

//! @enum  BGCNS_LinkType
//! @brief Defines the types of MAC links.
//! @see   _BGCNS_ServiceDirectory::macTestLink 
typedef enum {
    BGCNS_Transmitter,  //!< A transmitter link.
    BGCNS_Receiver      //!< A receiver link.
} BGCNS_LinkType;

//! @enum  BGCNS_EnvmonParameter
//! @brief Enumerates the various environmental monitor parameters.
//! @see   _BGCNS_ServiceDirectory::getEnvmonParm
//! @see   _BGCNS_ServiceDirectory::setEnvmonParm
typedef enum {
    BGCNS_envmon_period  = 0,
    BGCNS_envmon_policy,
    BGCNS_envmon_globintwire,

    // temporary                                                                                                                                                                               
    BGCNS_envmon_duration,
    BGCNS_envmon_ddrratio,
    BGCNS_envmon_numparms
} BGCNS_EnvmonParameter;


#define BGCNS_RC_COMPLETE  0         //!< Indicates that the operation completed normally.
#define BGCNS_RC_CONTINUE  1         //!< Indicates that the operation is still in progress.
#define BGCNS_RC_TIMEOUT  -1         //!< Indicates that the operation timed out.
#define BGCNS_RC_ERROR    -2         //!< Indicates that the operation failed.

#define BGCNS_NUM_DMA_RECEPTION_GROUPS           4
#define BGCNS_NUM_DMA_RECEPTION_FIFOS_PER_GROUP  8

//! @brief Describes the mapping of physical torus reception FIFOs to DMA reception FIFOs (rmFIFOs).  
//!     The first dimension indexes DMA reception groups, which are a combination of PID0 and PID1 bits
//!     from the DMA packet.
//!
//!     The second dimension indexes through the different dimensions: X+, X-, Y+, Y-, Z+, Z-, high priority
//!     and local copy.
typedef unsigned char BGCNS_ReceptionMap[BGCNS_NUM_DMA_RECEPTION_GROUPS][BGCNS_NUM_DMA_RECEPTION_FIFOS_PER_GROUP];

//! @brief Indicates that an interrupt is to be broadcast on all cores.
//! @see   _BGCNS_ServiceDirectory::enableInterrupt
#define BGCNS_ALL_CORE_BROADCAST 0xFFFFFFFFu


//! @enum   BGCNS_DeviceMasks
//! @brief  Provides a list of masks for various Blue Gene devices

typedef enum {
    BGCNS_SRAM       = 0x80000000u,
    BGCNS_BIC        = 0x40000000u,
    BGCNS_Torus      = 0x20000000u,
    BGCNS_DevBus     = 0x10000000u,
    BGCNS_XEMAC      = 0x08000000u,
    BGCNS_LockBox    = 0x04000000u,
    BGCNS_Collective = 0x02000000u,
    BGCNS_SRAM_Err   = 0x01000000u,
    BGCNS_DMA        = 0x00800000u
} BGCNS_DeviceMasks;

//! @typedef BGCNS_ServiceDirectory 
//! @struct  _BGCNS_ServiceDirectory 
//! @brief   The service directory is a collection of function pointers to services
//!          provided by the Common Node Services.
typedef struct _BGCNS_ServiceDirectory {

    /*------------------------------------------*/
    /*--- Informational services for the node --*/
    /*------------------------------------------*/


    int (*isIONode)(void);                             //!< Returns 1 if this is an I/O node; 0 if not.


    /*-----------------------------------------------------------------*/
    /*--- Informational services for obtaining Raw personality data ---*/
    /*-----------------------------------------------------------------*/

    unsigned int (*getPersonalitySize)(void);           //!< Returns the size (in bytes) of the Blue Gene personality.  
    void* (*getPersonalityData)(void);		      //!< Returns a pointer to the raw personality data.   


    /*-----------------------------------------------*/
    /*--- Services for Symmetric Multi-Processing ---*/
    /*-----------------------------------------------*/


    unsigned (*getNumberOfCores)(void);                  //!< Returns the number of CPUs on this node.

    //! @brief Called by the kernel to activate a CPU.
    //! @param[in] cpu The index of the cpu (core) to be activated.
    //! @param[in] entry The (kernel) entry point function.  This function will be invoked when
    //!            the CPU is actually activated.
    //! @param[in] arg A pointer to the lone argument to be passed to the entry point.
    //! @return Zero (0) if the CPU was succsessfully activated.  Non-zero if the CPU was not
    //!            activated (e.g. invalid cpu argument, or the cpu has already been
    //!            activated).
    //! @remarks   See Section x of the Common Node Services overview for details.
    int (*takeCPU)(unsigned cpu, void *arg, void (*entry)(unsigned cpu, void *arg));


    /*--------------------------------------*/
    /*--- Services for Blue Gene devices ---*/
    /*--------------------------------------*/

    //! @brief  Checks active devices for a clean termination state and returns 0
    //!         if everything is nominal.  Returns non-zero if any anomaly is
    //!         detected and logs violations.
    //! @param[in] job_rc specifies the return code of the job that is terminating.
    int (*terminationCheck)(int job_rc); 

    /*-------------------------------*/
    /*--- Services for interrupts ---*/
    /*-------------------------------*/


    //! @brief Enables the specified interrupt.  For all interrupts except inter-processor
    //!        interrupts, the interrupt will bendled by the specified core.  
    //! @param[in] group Specifies the Blue Gene interrupt group
    //! @param[in] irq  Specifies the interrupt index within the group
    //! @param[in] itype Specifies the type of interrupt that hardware will present
    //!            for this group/irq.
    //! @param[in] core Specifies which core will handle the interrupt.  If specified as
    //!            BGCNS_ALL_CORE_BROADCAST, then all cores will handle the interrupt.
    //! @return    Returns zero (0) if the interrupt is enabled and returns non-zero if it was not 
    //!            (including the case of bad arguments).
    int (*enableInterrupt)(unsigned group, unsigned irq, BGCNS_InterruptType itype, unsigned core);

    //! @brief Disables the specified interrupt.
    //! @param[in] group Specifies the Blue Gene interrupt group
    //! @param[in] irq  Specifies the interrupt index within the group
    //! @return    Returns zero (0) if the interrupt is disabled and returns non-zero if it was not 
    //!            (including the case of bad arguments).
    int (*disableInterrupt)(unsigned group, unsigned irq);

    //! @brief Queries the Blue Gene interrupt hardware for interrupts of the given
    //!        type and returns the group/IRQ.  This service is typically used in the
    //!        context of an interrupt handler.  Since multiple interrupt conditions
    //!        may be present, the service is typically invoked from the handler 
    //!        (along with corresponding acknowledgement) until the return code
    //!        indicates that no more interrupts are present.
    //! @param[out] group Specifies the Blue Gene interrupt group.  The value is valid
    //!        only when the return code is 0.
    //! @param[out] irq  Specifies the interrupt index within the group.  The value is
    //!        valid only when the reutrn code is zero.
    //! @param[in] itype Specifies the type of interrupt being queried.
    //! @return Returns zero (0) if an interrupt condition of the specified type exists.  Returns -1
    //!        if no such condition exists.
    int (*getInterrupt)(BGCNS_InterruptType itype, unsigned* group, unsigned* irq);

    //! @brief Acknowledges the specified interrupt, thus clearing the interrupt 
    //!       condition in the interrupt controller hardware.
    //! @param[in] group Specifies the Blue Gene interrupt group
    //! @param[in] irq  Specifies the interrupt index within the group
    //! @return    Returns zero (0) if the interrupt is acknowledged and returns non-zero if it was not 
    //!            (including the case of bad arguments).
    //! @remarks Note that for some interrupts, it is not sufficient to only acknowledge
    //!       the interrupt; the hardware condition that triggered the interrupt may
    //!       also need to be cleared.
    int (*acknowledgeInterrupt)(unsigned group, unsigned irq);

    //! @brief Raises the specified interrupt.
    //! @param[in] group Specifies the Blue Gene interrupt group
    //! @param[in] irq  Specifies the interrupt index within the group
    int (*raiseInterrupt)(unsigned group, unsigned irq);


    /*------------------------*/
    /*--- Mailbox services ---*/
    /*------------------------*/

    unsigned (*getMailboxMaximumConsoleInputSize)(void);   //!< Returns the actual maximum console message input data size.
    unsigned (*getMailboxMaximumConsoleOutputSize)(void);  //!< Returns the actual maximum console message output data size.

    //! @brief Writes a text message to the output mailbox.
    //! @param[in] msg a pointer to the message to be written.
    //! @param[in] msglen the length (in bytes) of the message to be written.
    //! @remarks As with all common services, the message data area must be mapped via
    //!          the TLB when the service is called.  The behavior is not defined if this
    //!          is not the case.
    //! @return Zero (0) if the message was written successfully, non-zero if anything went
    //           wrong (including a message that is too large).
    int (*writeToMailboxConsole)(char *msg, unsigned msglen);

    //! @brief Writes a text message to the output mailbox but does not wait for a
    //!        response back from the control system.  When this service is used,
    //!        the caller must poll for completion using the testForOutboxCompletion
    //!        service.
    //! @param[in] msg a pointer to the message to be written.
    //! @param[in] msglen the length (in bytes) of the message to be written.
    //! @remarks As with all common services, the message data area must be mapped via
    //!          the TLB when the service is called.  The behavior is not defined if this
    //!          is not the case.
    //! @return Zero (0) if the message was written successfully, non-zero if anything went
    //           wrong (including a message that is too large).
    int (*writeToMailboxConsole_nonBlocking)(char* msg, unsigned msglen);

    //! @brief Tests the outbox to see if the last message was picked up by the control 
    //!        system.  
    //! @return Zero (0) if the last message was piecked and returns non-zero if it has not.
    //! @remarks Typically the caller will invoke this service after having called 
    //!        writeToMailboxConsole_nonBlocking and will then invoke this service in a
    //!        loop until zero is returned.
    int (*testForOutboxCompletion)(void);

    //! @brief Reads a message from the input mail box.
    //! @param msg a pointer to a data area into which the message will be placed.
    //! @param maxMsgSize gives the size of the data area, i.e. the largest message
    //!        that may be safely received into the buffer.
    //! @return The actual length of the message (0 if no message was receieved).
    //! @remarks As with all common services, the message data area must be mapped
    //!          via the TLB when this service is called.  The results are not defined if
    //!          this is not the case.
    unsigned (*readFromMailboxConsole)(char *buf, unsigned bufsize);

    int (*testInboxAttention)(void);                      //!< Returns 1 if something is available in the input mailbox.

    int (*_no_longer_in_use_1_)(void); //!< Obsolete ... do not use.

    int (*writeToMailbox)(void* message, unsigned length, unsigned cmd);

    /*------------------------------------*/
    /*---  RAS and diagnostic services ---*/
    /*------------------------------------*/

    //! @brief TBD
    void (*machineCheck)(void *regs);

    //! @brief Writes a RAS event to the log.
    //! @param[in] facility The facility (aka component).
    //! @param[in] unit The unit (aka subcomponent).
    //! @param[in] err_code The error code.
    //! @param[in] numDetails The number of additional details.
    //! @param[in] details The list of additional details.
    //! @return Zero if the message was written, non-zero if some error condition occurred.
    //! @see bgp/arch/include/common/bgp_ras.h for details on facility, unit and err_code.
    int (*writeRASEvent)( unsigned facility, unsigned unit, unsigned short err_code, unsigned numDetails, unsigned details[] );

    //! @brief Writes a RAS string to the log.
    //! @param[in] facility The facility (aka component).
    //! @param[in] unit The unit (aka subcomponent).
    //! @param[in] err_code The error code.
    //! @param[in] str The message string being written (ASCII encoded, null-terminated).  Note that the length of this string is
    //!     limited to _BGP_RAS_ASCII_MAX_LEN characters.  The implementation may choose to truncate the string if it exceeds this
    //!     length.
    //! @return Zero if the entire message was written; non-zero if some error condition occurred (including the case where the
    //!      string was truncated).
    //! @see bgp/arch/include/common/bgp_ras.h for details on facility, unit and err_code.
    int (*writeRASString)( unsigned facility, unsigned unit, unsigned short err_code, char* str );


    /*---------------------------------*/
    /*--- Global Interrupt services ---*/
    /*---------------------------------*/

    //! @brief A global (compute node) barrier.  This call will block until all other compute nodes
    //!        in the partition also arrive at the barrier.
    int (*globalBarrier)(void);

    //! @brief  A global (compute node) barrier.  This call will block until all other compute nodes
    //!         in the partition also arrive at the barrier or until the timeout is reached.
    //! @param  timeoutInMillis specifies the timeout duration.  Units are milliseconds.
    //! @return BGCNS_RC_COMPLETE if the barrier completed.  BGCNS_RC_TIMEOUT if the barrier timed
    //!         out.  BGCNS_RC_ERROR if some other error occurred.
    int (*globalBarrierWithTimeout)(unsigned timeoutInMillis);



    /*-------------------------*/
    /*---  Network services ---*/
    /*-------------------------*/

  
    void (*initializeNetworks)(void);  //!< @todo Is this is going away??? Talk to Andy

    void (*_no_longer_in_use_381)(void); //!< @warning Do not use

    void (*_no_longer_in_use_384)(void);//!< @warning Do not use


    /*--------------------------*/
    /*---  DMA unit services ---*/
    /*--------------------------*/

#define BGCNS_DMA_CAPTURE_X_PLUS         0   //!< watch the X+ receiver
#define BGCNS_DMA_CAPTURE_X_MINUS        1   //!< watch the X- receiver
#define BGCNS_DMA_CAPTURE_Y_PLUS         2   //!< watch the Y+ receiver
#define BGCNS_DMA_CAPTURE_Y_MINUS        3   //!< watch the Y- receiver
#define BGCNS_DMA_CAPTURE_Z_PLUS         4   //!< watch the Z+ receiver
#define BGCNS_DMA_CAPTURE_Z_MINUS        5   //!< watch the Z- receiver
#define BGCNS_DMA_CAPTURE_DISABLE        7   //!< disable link capturing

    //! @brief Sets the link capture facility of the DMA unit to watch the specified
    //!        receiver (or disable).
    //! @param[in] link Specifies the link being monitored.  Use the BGCNS_DMA_CAPTURE_*
    //!        mnemonics defined above.
    //! @return Zero if the operation succeeded, non-zero if it did not (e.g. an invalid
    //!        link was specified).
    int (*setDmaLinkCapture)(int link);

    //! @brief Clears the link capture unit so that another packet can be captured.
    void (*clearDmaLinkCapture)(void);

#define BGCNS_RC_DMA_NO_PACKET_CAPTURED      0
#define BGCNS_RC_DMA_CAPTURE_UNIT_ERROR     -1
#define BGCNS_RC_DMA_DATA_CONFLICT          -2 //!< if initial read indicates a bad packet is captured but subsequent read shows bad packet not captured
#define BGCNS_RC_DMA_DATA_CONFLICT2         -3 //!< if bad packet is captured, but all the bytes are the same
    //! @brief Reads the DMA link capture packets.
    int (*readDmaLinkCapturePackets)(unsigned char* good_packet, int* good_packet_size, unsigned char* bad_packet, int* bad_packet_size);


#define BGCNS_DMA_ALL_GROUPS 0xFFFFFFFF

    //! @brief Sets FIFO controls for the DMA unit.
    //!
    //! An operation on facility BGCNS_InjectionFifo enables or disables a subset of the 128 DMA injection FIFOs.
    //! The FIFOs are organized into four groups of 32.  The mask argument is a bit mask (bit i controls the i-th imFIFO
    //! within that group, that is the (group*32)+i imFIFO.
    //!
    //! An operation on facility BGCNS_ReceptionFifo enables or disables a subset of the 32 DMA reception FIFOs.  
    //! The group argument is ignored and the mask argument is a bit mask (bit i controls the i-th reception FIFO).
    //!
    //! An operation on facility BGCNS_ReceptionHeaderFifo enables or disables the header FIFO for the specified
    //! group.  The mask argument is ignored.  Note that the header FIFO is typically used for debugging.
    //!
    //! An operation on facility BGCNS_InjectionFifoInterrupt enables or disables threshold interrupts for the
    //! specified injection FIFO.  Threshold interrupts occur if available space is less than the configured
    //! threshold when the FIFO is used for a remote get operation.  The group and mask arguments are as
    //! described in the BGCNS_InjectionFifo operation (above).
    //!
    //! An operation on facility BGCNS_ReceptionFifoInterrupt enables or disables interrupts for the specified
    //! reception FIFO(s).  If enabled, an interrupt will occur when the reception FIFO's available space drops
    //! below the configured threshold.  The group argument selects the interrupt type (type 0, 1, 2 or 3).
    //! The mask argument is a bit mask selecting one or more of the 32 normal reception FIFOs.
    //!
    //! An operation on facility BGCNS_ReceptionHeaderFifoInterrupt enables or disables interrupts for the specified
    //! reception header FIFO.  Reception header FIFOs are used for debug purposes only.
    //!
    //! An operation on facility BGCNS_InjectionCounterInterrupt enables or disables "Counter Hit Zero" interrupts.
    //! The group argument does not specify counter group, but rather specifies interrupt 0, 1, 2 or 3.  The mask
    //! argument is a bit mask that selects one or more counter subgroups to operate on (the 256 injection counters
    //! are partitioned into 32 subgroups of 8 counters).
    //!
    //! An operation on facility BGCNS_ReceptionCounterInterrupt enables or disables "Counter Hit Zero" interrupts
    //! for reception counters.  The group and mask arguments are the as as described in the the 
    //! BGCNS_InjectionCounterInterrupt operation (above).
    //!
    //! The buffer argument is used as a means to save/restore in an opaque manner.  This is achieved by passing
    //! a non-NULL buffer to a disable operation and subsequently passing that buffer during a reenable
    //! operation (the buffer is used to snapshot state).
    //!
    //!
    //! @code
    //!   +---------------------------------+-----------+---------+-------+
    //!   | Facility                        | group     | mask    | Notes |
    //!   +---------------------------------+-----------+---------+-------+
    //!   | BGCNS_InjectionFifo             | 0..3      | 32 bits | [1]   |
    //!   +---------------------------------+-----------+---------+-------+
    //!   | BGCNS_ReceptionFifo             | n/a       | 32 bits | [2]   |
    //!   +---------------------------------+-----------+---------+-------+
    //!   | BGCNS_ReceptionHeaderFifo       | 0..3, ALL | N/A     |       |
    //!   +---------------------------------+-----------+---------+-------+
    //!   | BGCNS_InjectionFifoInterrupt    | 0..3      | 32 bits | [1]   |
    //!   +---------------------------------+-----------+---------+-------+
    //!   | BGCNS_ReceptionFifoInterrupt    | 0..3      | 32 bits | [3]   |
    //!   +---------------------------------+-----------+---------+-------+
    //!   | BGCNS_InjectionCounterInterrupt | 0..3      | 32 bits | [3][4]|
    //!   +---------------------------------+-----------+---------+-------+
    //!   | BGCNS_ReceptionCounterInterrupt | 0..3      | 32 bits | [3][4]|
    //!   +---------------------------------+-----------+---------+-------+
    //!
    //!     [1] There are 128 injection FIFOs partitioned into 4 groups of 32.
    //!     [2] There are 32 normal reception FIFOs in BG/P.
    //!     [3] There are 4 interrupt lines.  The group argument selects one these 4.
    //!     [4] There are 256 counters of each type (injection and reception).  The
    //!         32-bit mask partitions them into groups of 8.
    //!         
    //! @endcode
    //!
    //! @param[in] operation defines the type of operation being performed (enable, disable, or re-enable).
    //! @param[in] facility defines the type of FIFO being configured.
    //! @param[in] group is interpreted differently based on the facility.
    //! @param[in] mask is interpreted differently based on the facility.
    //! @param[out] buffer is interpreted differently based on the operation and facility.  It is generally used to capture
    //!   a copy of the facility's current state in an enable operation (and may be null, in which case it is ignored).  It is 
    //!   generally used as the value to be loaded in a re-enable operation.  In this manner, a state value captured by an enable
    //!   operation may be easily restored by a subsequent re-enable operation.  The buffer argument is generally ignored by
    //!   disable operations.
    int (*setDmaFifoControls)(BGCNS_FifoOperation operation, BGCNS_FifoFacility facility, unsigned group, unsigned mask, unsigned* buffer);

    //! @brief Maps injection FIFOs onto physical (torus hardware) FIFOs.
    //! @param[in] group specifies the injection FIFO group.
    //! @param[in] fifoIds is an array of length numberOfFifos whose elements are the identifiers of the imFIFO (within that
    //!   given group).
    //! @param[in] injection_map is an array of length numberOfFifos whose elements are 8-bit masks identifying which of the
    //!   physical torus injection FIFOs are mapped.  Bits 0-3 correspond to torus group 0, and bits 4-7 correspond to torus
    //!   group 1.  Bits 3 and 7 are the high priority FIFOs.
    //! @param[in] numberOfFifos describes the number of elements contained in the fifoIds and injection_map arguments.
    //! @return Zero if the map was properly set.  Non-zero if it was not, including the case of illegal arguments.
    //! @note In BG/P, there are 128 injection FIFOs partitioned into 4 groups of 32.  So the legal range of the group
    //!   argument is 0..3 and the legal range for the fifoIds[] elements is 0..31.

    int (*setDmaInjectionMap)(unsigned group, unsigned fifoIds[], unsigned char injection_map[], unsigned numberOfFifos);

    //! @brief Enables or disables "local copy" behavior for the specified injection FIFOs.  A local copy injection FIFO
    //!   can be used to perform memory copies within a node via the DMA engine.
    //! @param[in] operation specifies whether local copies is being enabled or disabled on the specified FIFOs.  The BGCNS_Reenable
    //!   operation is not supported.
    //! @param[in] group specifies the injection FIFO group.
    //! @param[in] bits selects one or more injection FIFOs from within the group on which to operate.
    //! @return Zero if the operation succeeded; non-zero if it did not.
    //! @note In BG/P, there are 128 injection FIFOs partitioned into 4 groups of 32.  So the legal range of the group
    //!   argument is 0..3.
    int (*setDmaLocalCopies)(BGCNS_FifoOperation operation, unsigned group, unsigned bits);

    //! @brief Enables or disables the priority bit for the specified injection FIFOs.  The priority bit
    //!   is used by the hardware arbitration (details are not further documented here).
    //! @param[in] operation specifies whether priority bits are being set or cleared.
    //! @param[in] group specifies the injection FIFO group.
    //! @param[in] bits selects one or more injection FIFOs from within the group on which to operate.
    //! @note In BG/P, there are 128 injection FIFOs partitioned into 4 groups of 32.  So the legal range of the group
    //!   argument is 0..3.
    int (*setDmaPriority)(BGCNS_FifoOperation operation, unsigned group, unsigned bits);

    //! @brief Sets the mapping from physical (torus hardware) reception FIFOs to reception FIFOs.  The hardware supports
    //!   8 torus FIFOs (six torus dimensions plus high priority plus local copy).  Furthermore, the hardware supports
    //!   4 groups as derived from the PID0 and PID1 bits of the DMA packet.  Thus the mapping is a 4 x 8 matrix of
    //!   reception FIFO ids.
    //! @param[in] torus_reception_map maps {group} X {torus-hardware-FIFOs} --> reception FIFOs.
    //! @param[in] fifo_types is an array of N values specifying the type of each normal reception FIFO (see also threshold).  For BGP,
    //!   N=2 (there are 32 normal reception FIFOs).
    //! @param[in] header_types is an array of N values specifying the type of each reception header FIFO (see also threshold).  For
    //!   BGP, N=4 (there are 4 reception header FIFOs).  Note that reception header FIFOs are typically only used for debugging purposes.
    //! @param[in] threshold is an array of N threshold values.  The value threshold[i] specifies the threshold value for reception
    //!   FIFO type i.  If reception FIFO interrupts are enabled (see setDmaFifoControls) and a reception FIFO's available space drops
    //!   below its threshold, an interrupt is driven.  For BGP, N=2 (there are type 0 and type 1 injection FIFOs).  
    int (*setDmaReceptionMap)( BGCNS_ReceptionMap torus_reception_map, unsigned fifo_types[], unsigned header_types[], unsigned threshold[]);

    //! @brief Gets the reception map.
    //! @see setDmaReceptionMap for descriptions of the map and arguments.
    int (*getDmaReceptionMap)( BGCNS_ReceptionMap torus_reception_map, unsigned fifo_types[], unsigned short* store_headers, unsigned header_types[], unsigned threshold[]);

    //! @brief In some versions of the BG/P hardware, the DMA hardware will stop when a reception FIFO runs out of space.
    //! @deprecated This service is useful only on early versions of the hardware and will be removed in a future release.
    int (*clearDmaFullReceptionFifo)(void);


    //! @brief Resets the MAC unit's PHY.
    //! @return Zero if the unit was properly reset.  Returns non-zero if some error occurred.
    //! @deprecated See macResetPHY_nonBlocking.
    int (*macResetPHY)(void);

    //! @brief Tests the MAC unit's link.
    //! @param[in] link_type specifies the type of link to be tested.
    //! @return One (1) if the link is active; zero (0) if it is not.
    //! @deprecated See macTestLink_nonBlocking
    int (*macTestLink)(BGCNS_LinkType link_type);

    //! @brief Reads one of the MAC's XGMII registers.
    //! @param[in] device_address
    //! @param[in] port_address
    //! @param[in] register_address
    //! @return The register's value or a negative number if some error occurred.
    //! @deprecated Low level MAC register access is being eliminated.
    int (*macXgmiiRead)(unsigned device_address, unsigned port_address, unsigned register_address);

    //! @brief Writes one of the MAC's XGMII registers.
    //! @param[in] device_address
    //! @param[in] port_address
    //! @param[in] register_address
    //! @param[in] value
    //! @return Zero (0) if the register was successfully written; non-zero if some error occurred.
    //! @deprecated Low level MAC register access is being eliminated.
    int (*macXgmiiWrite)(unsigned device_address, unsigned port_address, unsigned register_address, unsigned value);
      

    //! @brief Trains SerDes in a non-blocking manner.  The standard usage is to inititate
    //!      training with trainSerDes(1), check the return code, and then continue to invoke
    //!      trainSerDes(0) as long as the return code is BGCNS_RC_CONTINUE.
    //! @param[in] reset Should be 1 when initiating a retraining sequence and 0 for any
    //!      continuations.
    //! @return BGCNS_RC_CONTINUE if training is still ongoing (the caller should re-invoke
    //!      the service again (with reset=0).  BGCNS_RC_COMPLETE if training is complete.
    //!      BGCNS_ERROR if some error has occurred.
    int (*trainSerDes)(int reset);

    //! @brief Fetches the value of the specified control parameter of the environmental monitor.
    //! @param[in] parameter Parameter to retrieve.  Should be a valid parameter in the BGCNS_EnvmonParameter enumeration
    //! @param[in] value Pointer to the storage location that will contain the parameter's value when the function successfully returns.
    //! @return Zero if the register was successfully fetched; non-zero if some error occurred.
    int (*getEnvmonParm)(BGCNS_EnvmonParameter parameter, unsigned int* value);

    //! @brief Stores a value to the specified control parameter of the environmental monitor
    //! @param[in] parameter Parameter to store.  Should be a valid parameter in the BGCNS_EnvmonParameter enumeration
    //! @param[in] value New value for the parameter
    //! @return Zero if the register was successfully fetched; non-zero if some error occurred.
    int (*setEnvmonParm)(BGCNS_EnvmonParameter parameter, unsigned int value);

    //! @brief Performs checks and ensures that the node will continue to operate within tolerances.
    //! @note MUST be called regularly as indicated by nextCallbackTime parameter
    //! @param[in] nextCallbackTime Upon returning, this will contain the PPC Timebase register value indicating when the next 
    //!            time the operating system needs to call performEnvMgmt.  Failure to do so may result in poorly performing 
    //!            nodes or shutdown of the block / rack.
    int (*performEnvMgmt)(unsigned long long* nextCallbackTime);
      

    //! @brief Writes a RAS message to the output mailbox but does not wait for a
    //!        response back from the control system.  When this service is used,
    //!        the caller must poll for completion using the testForOutboxCompletion
    //!        service.
    //! @param[in] facility The facility (aka component).  See bgp_ras.h for a list of facilities.
    //! @param[in] unit The unit (aka subcomponent).  See bgp_ras.h for a list of units.
    //! @param[in] err_code The error code.  See bgp_ras.h for a list of error code.s
    //! @param[in] numDetails The number of additional details.
    //! @param[in] details The list of additional details.
    //! @return Zero if the message was written, non-zero if some error condition occurred.
    int (*writeRASEvent_nonBlocking)( unsigned facility, unsigned unit, unsigned short err_code, unsigned numDetails, unsigned details[] );

    //! @brief Writes a RAS message to the output mailbox but does not wait for a
    //!        response back from the control system.  When this service is used,
    //!        the caller must poll for completion using the testForOutboxCompletion
    //!        service.
    //! @param[in] facility The facility (aka component).  See bgp_ras.h for a list of facilities.
    //! @param[in] unit The unit (aka subcomponent).  See bgp_ras.h for a list of units.
    //! @param[in] err_code The error code.  See bgp_ras.h for a list of error code.s
    //! @param[in] str The message string being written (ASCII encoded, null-terminated).  Note that the length of this string is
    //!     limited to _BGP_RAS_ASCII_MAX_LEN characters.  The implementation may choose to truncate the string if it exceeds this
    //!     length.
    //! @return Zero if the entire message was written; non-zero if some error condition occurred (including the case where the
    //!      string was truncated).
    //! @return Zero if the message was written, non-zero if some error condition occurred.
    int (*writeRASString_nonBlocking)( unsigned facility, unsigned unit, unsigned short err_code, char* str );
    
    //! @brief Sets the core's timebase registers to the specified value.  
    //! @param[in] newtime The new 64-bit timebase
    //! @return Zero if the timebase was successfully set, non-zero if some error condition occurred.
    //! @deprecated
    int (*synchronizeTimebase)(unsigned long long newtime);
    
    //! @brief Sets the node's DMA physical protection settings.  
    //! @note on BGP, there are a maximum of 8 read ranges and 8 write ranges
    //! @return Zero if the DMA ranges were set, non-zero if some error condition occurred.
    int (*dmaSetRange)(unsigned numreadranges,  unsigned long long* read_lower_paddr, unsigned long long* read_upper_paddr, 
			 unsigned numwriteranges, unsigned long long* write_lower_paddr, unsigned long long* write_upper_paddr);

    //! @brief Checks the status of the devices and reports correctible RAS (if any)
    //! @param[in] clear_error_counts If non-zero, function will also reset the hardware error counters after posting any RAS.
    //! @return Zero if successful, non-zero if some error condition occurred.
    int (*statusCheck)(unsigned clear_error_counts);

    //! @brief Stops the DMA and clears any reception unit failure
    int (*stopDma)(void);

    //! @brief Starts the DMA
    int (*startDma)(void);

    //! @brief Performs a hard exit.  The status code is provided to the control system.  
    //! @return This service never returns.
    void (*exit)(int rc);

    //! @brief Resets the MAC unit's PHY but does not block.
    //! @param[in] reset indicates whether this is the beginning (1) or a continuation (0) of a 
    //!     reset sequence.  That is, callers should initiate a reset sequence with reset=1 and then
    //!     if receiving a return code of BGCNS_RC_CONTINUE, should invoke this servicate again with
    //!     reset=0.
    //! @param[in] timeoutInMillis the (approximate) number of milliseconds that this service can have
    //!     before returning.  If the allotted time is not sufficient, the service will return BGCNS_RC_CONTINUE
    //!     to indicate that it needs additional time.
    //! @return BGCNS_RC_COMPLETE if the unit was properly reset.  BGCNS_RC_CONTINUE if the reset operation is
    //!     not yet complete.  BGCNS_RC_ERROR if the reset operation failed.
    int (*macResetPHY_nonBlocking)(int reset, unsigned timeoutInMillis);

    //! @brief Tests the MAC unit's link but does not block.
    //! @param[in] link_type specifies the type of link to be tested.
    //! @param[out] result points to the link status, which is valid only when the return code is
    //!     BGCNS_RC_COMPLETE. A value of one (1) indicates that the link is active; zero (0) 
    //!     indicates that it is inactive.
    //! @param[in] reset indicates whether this is the beginning (1) or a continuation (0) of a 
    //!     test link sequence.  That is, callers should initiate a sequence with reset=1 and then
    //!     if receiving a return code of BGCNS_RC_CONTINUE, should invoke this service again with
    //!     reset=0.
    //! @param[in] timeoutInMillis the (approximate) number of milliseconds that this service can have
    //!     before returning.  If the allotted time is not sufficient, the service will return BGCNS_RC_CONTINUE
    //!     to indicate that it needs additional time.
    //! @return BGCNS_RC_COMPLETE if the test is complete (result is valid only in this case). BGCNS_RC_CONTINUE 
    //!     if the reset operation is not yet complete.  BGCNS_RC_ERROR if the reset operation failed.
    int (*macTestLink_nonBlocking)(BGCNS_LinkType link_type, unsigned* result, int reset, unsigned timeoutInMillis);

    void * _not_in_use_1068;
    void * _not_in_use_1069;


    //! @brief Indicates that a new job is about to start.
    //! @return Zero (0) if CNS is ready for a new job to start.  Returns non-zero otherwise.
    int (*startNextJob)(void);

    //! @brief Indicates that the CNS should use the specified virtual address when accessing the
    //!     given device.  When a device is remapped, CNS will no longer make any attempt to map
    //!     a TLB to access that device -- it is the responsibility of the kernel to handle the 
    //!     TLB either  proactively or reactively (via a fault).
    //! @param[in] device specifies the device being mapped.
    //! @param[in] base_address is the root virtual address of the device.  The address should be
    //!     naturally aligned (relative to the size of the device).  See the seciton Reserved and
    //!     Preferred Addresses for more information.
    //! @return Zero (0) if the device was successfully remapped.  Returns non-zero if it was not.
    //! @remarks The lock box is in active use by CNS during early boot and thus it is not
    //!    possible to remap the BGCNS_LockBox device until all cores are activated by the kernel
    //!    (that is, takeCPU has been called for all cores).
    int (*mapDevice)(BGCNS_DeviceMasks device, void* base_address);

    //! @brief Enables barriers on the specified channel.
    //! @param channel specifies the channel being enabled.
    //! @param user_mode indicates whether the barrier is to be used in user-mode code.
    //! @return Zero if global barriers were enabled.  Returns non-zero if the request could not be
    //!        completed, including the case of attempting to enable a reserved channel.
    int (*enableBarrier)(unsigned int channel, int user_mode);

    //! @brief Disables barriers on the specified channel.
    //! @return Zero if global barriers were disabled.  Returnsnon-zero if the request could not be 
    //!        completed, including the case of attempting to disable a reserved channel.
    int (*disableBarrier)(unsigned int channel);

    //! @brief A global barrier that does not block indefinitely.
    //! @param channel indicates the GLINT hardware channel to use.
    //! @param reset indicates whether this is the beginning (1) or a continuation (0) of a barrier
    //!   sequence.  That is, caller should inititate a barrier operation by passing reset=1 and then,
    //!   if receiving a return code of BGCNS_RC_CONTINUE, should invoke the service again with
    //!   reset=0.
    //! @param timeoutInMillis is the (approximate) number of milliseconds that this service is allowed
    //!   to wait for barrier participants before returning to the caller.
    //! @return BGCNS_RC_COMPLETE indicates that all participants have arrived at the barrier.  BGCNS_RC_CONTINUE
    //!   indicates that not all partipants arrived within the alloted timeout period.  BGCNS_RC_ERROR 
    //!   indicates that other problem has been detected.
    //! @remarks This service is not thread safe.  It is considered a programming error to invoke it
    //!   from multiple threads concurrently and the behavior is not defined.
    int (*globalBarrier_nonBlocking)(unsigned channel, int reset, unsigned timeoutInMillis);
      
    //! @brief Restart kernel in cycle reproducibility mode.
    //! @return Zero if no restart was required for reproducibility.
    //! @remarks This service must be called from each core and only after all I/O operations have been completed.  
    //!   Processors will be reset and kernels will start again.  
    int (*setupReproducibility)(void);
      
} BGCNS_ServiceDirectory;

//! @deprecated
//! @typedef BGCNS_DeprecatedServicesDirectory
//! @struct  _BGCNS_DeprecatedServices
//! @brief   These services exist for historical reasons and are not further documented here.
//!          They may not be available in future releases of CNS.
typedef struct _BGCNS_DeprecatedServices {
    int (*torusTermCheck)(int* nonFatalRc);
    int (*torusLinkErrCheck)(int* nonFatalRc);
    int (*torusCRCExchange)(void);
    int (*collectiveConfigureClassInternal)(unsigned virtualTree, unsigned short specifier);
    int (*collectiveConfigureClass)(unsigned virtualTree, unsigned short specifier);
    unsigned (*collectiveGetClass)(unsigned virtualTree);
    int (*collectiveInit)(void);
    int (*collectiveRelease)(void);
    int (*collectiveHardReset)(void);
    int (*netbusTermCheck)(void);
    unsigned (*getSerDesLinkStatus)(void);
    int  (*dmaTermCheck)(void);
} BGCNS_DeprecatedServicesDirectory;

//! @typedef BGCNS_Descriptor 
//! @struct  _BGCNS_Descriptor
//! @brief  The Common Node Services descriptor.  This descriptor provides information to the kernel regarding
//!         the CNS memory region as well as a service directory.  The descriptor is passed to the kernel
//!         upon boot and must not be altered by the kernel.
typedef struct _BGCNS_Descriptor {
    BGCNS_ServiceDirectory* services;         //!< A pointer to the services directory.
    unsigned baseVirtualAddress;	      //!< The virtual address of the beginning of the CNS memory region.
    unsigned size;			      //!< The size (in bytes) of the CNS memory region.
    unsigned basePhysicalAddress;             //!< The physical address of the CNS memory region.
    unsigned basePhysicalAddressERPN;         //!< The extended real page number of the CNS memory region.
    unsigned bgcns_private_in_use;            //!< Undefined.  This field is for internal use only and may disappear at any time.
    BGCNS_DeprecatedServicesDirectory* deprecatedServices; //!< @deprecated undocumented
    unsigned version;                         //!< The CNS version
} BGCNS_Descriptor;

extern uintptr CommonNodeServices(void*, ...);
extern BGCNS_ServiceDirectory* cns;

#define CNS(f, t, ...)	(t)CommonNodeServices((void*)(cns->f), __VA_ARGS__)


#endif /* !__ASSEMBLY */
#endif /* _BGCNS_H */
