#include <stdio.h>
#include "pin.H"

#define PIN_FAST_ANALYSIS_CALL

using namespace std;

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "malloc_mt.out", "specify output file name");

/*
 * The ID of the buffer
 */
BUFFER_ID bufId;

/*
 * Number of OS pages for the buffer
 */
#define NUM_BUF_PAGES 1024

/*
 * Record of memory references.  Rather than having two separate
 * buffers for reads and writes, we just use one struct that includes a
 * flag for type.
 */
struct MEMREF
{
  ADDRINT     pc;
  ADDRINT     ea;
  UINT32      size;
  UINT32      thread_id;
  BOOL        read;
};


//==============================================================
//  Analysis Routines
//==============================================================
// Note:  threadid+1 is used as an argument to the GetLock()
//        routine.  This is the value that the lock is set to, 
//        so it must be non-zero.

// lock serializes access to the output file.
FILE* trace;
PIN_LOCK lock;

// This routine is executed every time a thread is created.
VOID ThreadStart(THREADID threadid, CONTEXT *ctxt, INT32 flags, VOID *v)
{
    GetLock(&lock, threadid+1);
    fprintf(trace, "thread begin %d\n",threadid);
    fflush(trace);
    ReleaseLock(&lock);
}

// This routine is executed every time a thread is destroyed.
VOID ThreadFini(THREADID threadid, const CONTEXT *ctxt, INT32 code, VOID *v)
{
    GetLock(&lock, threadid+1);
    fprintf(trace, "thread end %d code %d\n",threadid, code);
    fflush(trace);
    ReleaseLock(&lock);
}

// Print a memory read record
VOID PIN_FAST_ANALYSIS_CALL RecordMemRead(VOID * ip, VOID * addr, THREADID thread_id)
{
  GetLock(&lock, thread_id+1);
  fprintf(trace,"%d %p: R %p\n", thread_id, ip, addr);
  fflush(trace);
  ReleaseLock(&lock);
}

// Print a memory write record
VOID PIN_FAST_ANALYSIS_CALL RecordMemWrite(VOID * ip, VOID * addr, THREADID thread_id)
{
  GetLock(&lock, thread_id+1);
  fprintf(trace,"%d %p: W %p\n", thread_id, ip, addr);
  fflush(trace);
  ReleaseLock(&lock);
}

// This routine is executed __parsec_roi_begin() is called.

VOID BeforeROI( THREADID threadid )
{
    GetLock(&lock, threadid+1);
    fprintf(trace, "thread %d entered ROI\n", threadid);
    fflush(trace);
    ReleaseLock(&lock);
}

// This routine is executed __parsec_roi_begin() is called.

VOID AfterROI( THREADID threadid )
{
    GetLock(&lock, threadid+1);
    fprintf(trace, "thread %d exited ROI\n", threadid);
    fflush(trace);
    ReleaseLock(&lock);
}

/**************************************************************************
 *
 *  Callback Routines
 *
 **************************************************************************/

VOID * BufferFull(BUFFER_ID id, THREADID tid, const CONTEXT *ctxt, VOID *buf,
                  unsigned numElements, VOID *v)
{
  struct MEMREF * reference=(struct MEMREF*)buf;

  GetLock(&lock, tid+1);

  for(unsigned int i=0; i<numElements; i++, reference++)
    {
      if (reference->ea != 0)
	fprintf(trace,"%ld %ld %d %d\n", reference->pc, reference->ea, 
		reference->thread_id, reference->read);
    }
  fflush(trace);
  ReleaseLock(&lock);
  //DumpBufferToFile( reference, numElements, tid );
    
  return buf;
}



//====================================================================
// Instrumentation Routines
//====================================================================
//

// This routine is executed for each image.
VOID ImageLoad(IMG img, VOID *)
{
    RTN rtn = RTN_FindByName(img, "__parsec_roi_begin");
    RTN end_rtn = RTN_FindByName(img, "__parsec_roi_end");

    //GetLock(&lock, PIN_ThreadId()+1);

    if ( RTN_Valid( rtn ))
    {
      /*
      fprintf(trace, "thread %d entered ROI\n", PIN_ThreadId());
      fflush(trace);
      */
        printf("thread %d entered ROI\n", PIN_ThreadId());

        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(BeforeROI),
                       IARG_THREAD_ID, IARG_END);
        RTN_Close(rtn);

    }

    if ( RTN_Valid( end_rtn ))
    {
      /*
      fprintf(trace, "thread %d exited ROI\n", PIN_ThreadId());
      fflush(trace);
      */
        printf("thread %d exited ROI\n", PIN_ThreadId());

        RTN_Open(end_rtn);
        RTN_InsertCall(end_rtn, IPOINT_AFTER, AFUNPTR(AfterROI),
                       IARG_THREAD_ID, IARG_END);
        RTN_Close(end_rtn);
    }

    //ReleaseLock(&lock);
}

// Called for every instruction and instruments reads and writes
VOID Instruction(INS ins, VOID *v)
{
    // instruments loads using a predicated call, i.e.
    // the call happens iff the load will be actually executed
    // (this does not matter for ia32 but arm and ipf have predicated instructions)
  UINT32 refSize;
    if (INS_IsMemoryRead(ins))
    {
      /*
        INS_InsertPredicatedCall(
	    ins, IPOINT_BEFORE, (AFUNPTR)RecordMemRead,
	    IARG_FAST_ANALYSIS_CALL,
            IARG_INST_PTR,
            IARG_MEMORYREAD_EA,
	    IARG_THREAD_ID,
            IARG_END);
      */
      //using fast buffering API
      refSize = INS_MemoryReadSize(ins);

      INS_InsertFillBuffer(ins, IPOINT_BEFORE, bufId,
			   IARG_INST_PTR, offsetof(struct MEMREF, pc),
			   IARG_MEMORYREAD_EA, offsetof(struct MEMREF, ea),
			   IARG_UINT32, refSize, offsetof(struct MEMREF, size),
			   IARG_THREAD_ID, offsetof(struct MEMREF, thread_id),
			   IARG_BOOL, TRUE, offsetof(struct MEMREF, read),
			   IARG_END);

    }

    // instruments stores using a predicated call, i.e.
    // the call happens iff the store will be actually executed
    if (INS_IsMemoryWrite(ins))
    {
      /*
      //GetLock(&lock, PIN_ThreadId()+1);
        INS_InsertPredicatedCall(
            ins, IPOINT_BEFORE, (AFUNPTR)RecordMemWrite,
	    IARG_FAST_ANALYSIS_CALL,
            IARG_INST_PTR,
            IARG_MEMORYWRITE_EA,
	    IARG_THREAD_ID,
            IARG_END);
      //ReleaseLock(&lock);
      */
      //using fast buffering API
      refSize = INS_MemoryWriteSize(ins);

      INS_InsertFillBuffer(ins, IPOINT_BEFORE, bufId,
			   IARG_INST_PTR, offsetof(struct MEMREF, pc),
			   IARG_MEMORYWRITE_EA, offsetof(struct MEMREF, ea),
			   IARG_UINT32, refSize, offsetof(struct MEMREF, size),
			   IARG_THREAD_ID, offsetof(struct MEMREF, thread_id),
			   IARG_BOOL, FALSE, offsetof(struct MEMREF, read),
			   IARG_END);

    }
}


VOID Fini(INT32 code, VOID *v)
{
    //GetLock(&lock, thread_id+1);
    fflush(trace);
    fprintf(trace, "#eof\n");
    fflush(trace);
    fclose(trace);
    //ReleaseLock(&lock);
}


int main(int argc, char *argv[])
{
    // Initialize the pin lock
    InitLock(&lock);
    
    // Initialize pin
    PIN_InitSymbols();
    PIN_Init(argc, argv);

    // Open the trace file    
    trace = fopen(KnobOutputFile.Value().c_str(), "w");

    // Initialize the memory reference buffer;
    // set up the callback to process the buffer.
    //
    bufId = PIN_DefineTraceBuffer(sizeof(struct MEMREF), NUM_BUF_PAGES,
                                  BufferFull, 0);

    if(bufId == BUFFER_ID_INVALID)
      {
        printf("Error: could not allocate initial buffer\n");
        return 1;
      }


    // Register Instruction function to be called with each executed inst.
    INS_AddInstrumentFunction(Instruction, 0);

    // Register ImageLoad to be called when each image is loaded.
    IMG_AddInstrumentFunction(ImageLoad, 0);

    // Register Analysis routines to be called when a thread begins/ends
    PIN_AddThreadStartFunction(ThreadStart, 0);
    PIN_AddThreadFiniFunction(ThreadFini, 0);

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();
    
    return 0;
}
