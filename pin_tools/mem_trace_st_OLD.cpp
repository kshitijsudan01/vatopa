#include <stdio.h>
#include "pin.H"
#include "instlib.H"

#define PIN_FAST_ANALYSIS_CALL

using namespace INSTLIB;

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "malloc_mt.out", "specify output file name");

/*
 * The ID of the buffer
 */
BUFFER_ID bufId;
//BUFFER_ID bufId2; 

/*
 * The output trace file format
 * <IP> <EA> <SIZE> <R/W> <# inst executed b/w this and prev. memory inst.>
 */
FILE * trace;
//FILE * trace2;
//FILE * trace3;

/*
 * Count of inst. executed
 */
//UINT64 icount = 0;
//ICOUNT inst_count;

/*
 * Number of OS pages for the buffer
 */
#define NUM_BUF_PAGES 2048

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
  BOOL        read;
};

/*
 *==============================================================
 *  Analysis Routines
 *==============================================================
 */

/*
 * Increment count 
 */
//VOID do_count() { icount++;}

/*
 * Reset count to 0 
 */
//VOID PIN_FAST_ANALYSIS_CALL reset_count() { icount = 0;}


// This routine is executed __parsec_roi_begin() is called.
VOID BeforeROI( THREADID threadid )
{
    fprintf(trace, "thread %d entered ROI\n", threadid);
    fflush(trace);
}

// This routine is executed __parsec_roi_begin() is called.
VOID AfterROI( THREADID threadid )
{
    fprintf(trace, "thread %d exited ROI\n", threadid);
    fflush(trace);
}

// This function is called before every instruction is executed
// and prints the IP
VOID printip(void *ip, INS ins)
{
  //printf("here\n");
  //fprintf(trace3, "%p %s\n", ip, INS_Mnemonic(ins).c_str()); 

  /*
  if(INS_IsMemoryRead(ins))
    {
      refSize = INS_MemoryReadSize(ins);
      INS_InsertFillBuffer(ins, IPOINT_BEFORE, bufId,
			   IARG_INST_PTR, offsetof(struct MEMREF, pc),
			   IARG_MEMORYREAD_EA, offsetof(struct MEMREF, ea),
			   IARG_UINT32, refSize, offsetof(struct MEMREF, size),
			   IARG_UINT32, icount, offsetof(struct MEMREF, i_count),
			   IARG_BOOL, TRUE, offsetof(struct MEMREF, read),
			   IARG_END);
      
      
    }
    else if (INS_IsMemoryWrite(ins))
    {
      //using fast buffering API
      refSize = INS_MemoryWriteSize(ins);
      INS_InsertFillBuffer(ins, IPOINT_BEFORE, bufId,
			   IARG_INST_PTR, offsetof(struct MEMREF, pc),
			   IARG_MEMORYWRITE_EA, offsetof(struct MEMREF, ea),
			   IARG_UINT32, refSize, offsetof(struct MEMREF, size),
			   IARG_UINT32, icount, offsetof(struct MEMREF, i_count),
			   IARG_BOOL, FALSE, offsetof(struct MEMREF, read),
			   IARG_END);
  else
    {
      INS_InsertFillBuffer(ins, IPOINT_BEFORE, bufId,
			   IARG_INST_PTR, offsetof(struct MEMREF, pc),
			   0, offsetof(struct MEMREF, ea),
			   IARG_UINT32, 0, offsetof(struct MEMREF, size),
			   IARG_UINT32, icount, offsetof(struct MEMREF, i_count),
			   IARG_BOOL, FALSE, offsetof(struct MEMREF, read),
			   IARG_END);
      

    }
  */
}


/*
 **************************************************************************
 *
 *  Callback Routines
 *
 **************************************************************************
 */

VOID * BufferFull(BUFFER_ID id, THREADID tid, const CONTEXT *ctxt, VOID *buf,
                  unsigned numElements, VOID *v)
{
  struct MEMREF * reference=(struct MEMREF*)buf;
  for(unsigned int i=0; i<numElements; i++, reference++)
    {
      if (reference->pc != 0){
	fprintf(trace,"%d %d %p\n", 
		reference->size, reference->read,(VOID*)reference->ea);
      }
    }
  fflush(trace);
  return buf;
}

/*
 *====================================================================
 * Instrumentation Routines
 *====================================================================
 */

/*
 * Called for every instruction and instruments reads and writes
 */
VOID Instruction(INS ins, VOID *v)
{

  if(INS_Valid(ins))
  {

    UINT32 refSize;

    if (INS_IsMemoryRead(ins))
    {
      refSize = INS_MemoryReadSize(ins);
      INS_InsertFillBufferPredicated(ins, IPOINT_BEFORE, bufId,
			   IARG_INST_PTR, offsetof(struct MEMREF, pc),
			   IARG_MEMORYREAD_EA, offsetof(struct MEMREF, ea),
			   IARG_UINT32, refSize, offsetof(struct MEMREF, size),
			   IARG_BOOL, TRUE, offsetof(struct MEMREF, read),
			   IARG_END);
    }
    else if (INS_IsMemoryWrite(ins))
    {
      refSize = INS_MemoryWriteSize(ins);
      INS_InsertFillBufferPredicated(ins, IPOINT_BEFORE, bufId,
			   IARG_INST_PTR, offsetof(struct MEMREF, pc),
			   IARG_MEMORYWRITE_EA, offsetof(struct MEMREF, ea),
			   IARG_UINT32, refSize, offsetof(struct MEMREF, size),
			   IARG_BOOL, FALSE, offsetof(struct MEMREF, read),
			   IARG_END);
    }
    else
    {
      INS_InsertFillBufferPredicated(ins, IPOINT_BEFORE, bufId,
			   IARG_INST_PTR, offsetof(struct MEMREF, pc),
			   IARG_PTR, 0, offsetof(struct MEMREF, ea),
			   IARG_UINT32, 0, offsetof(struct MEMREF, size),
			   IARG_BOOL, FALSE, offsetof(struct MEMREF, read),
			   IARG_END);
    }
  }
}


VOID Fini(INT32 code, VOID *v)
{
    fflush(trace);
    //fflush(trace2);
    //fprintf(trace2, "#eof\n");
    fprintf(trace, "#eof\n");
    fflush(trace);
    fclose(trace);
    //fclose(trace2);
}


int main(int argc, char *argv[])
{
    // Initialize pin
    PIN_InitSymbols();
    PIN_Init(argc, argv);

    // Activate instruction counter
    //inst_count.Activate();
    
    // Open the trace file    
    trace = fopen(KnobOutputFile.Value().c_str(), "w");
    //trace2 = fopen("ip_trace.out", "w");
    //trace3 = fopen("printip.out","w");

    // Initialize the memory reference buffer;
    // set up the callback to process the buffer.
    //
    bufId = PIN_DefineTraceBuffer(sizeof(struct MEMREF), NUM_BUF_PAGES,
                                  BufferFull, 0);

    /*
    bufId2 = PIN_DefineTraceBuffer(sizeof(struct TRACE_f), NUM_BUF_PAGES,
                                  TraceBufferFull, 0);
    */

    if(bufId == BUFFER_ID_INVALID /*|| bufId2 == BUFFER_ID_INVALID*/)
      {
        printf("Error: could not allocate initial buffer\n");
        return 1;
      }


    // Register Instruction function to be called with each executed inst.
    INS_AddInstrumentFunction(Instruction, 0);

    // Register ImageLoad to be called when each image is loaded.
    //IMG_AddInstrumentFunction(ImageLoad, 0);

    // Register Analysis routines to be called when a thread begins/ends
    //PIN_AddThreadStartFunction(ThreadStart, 0);
    //PIN_AddThreadFiniFunction(ThreadFini, 0);

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();
    
    return 0;
}
