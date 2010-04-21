#include <stdio.h>
#include "pin.H"
#include "instlib.H"
//#include <Python.h>

#define PIN_FAST_ANALYSIS_CALL

using namespace INSTLIB;

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "malloc_mt.out", "specify output file name");

/*
 * The ID of the buffer
 */
BUFFER_ID bufId;

BOOL ENABLE_LOGGING = TRUE;

/*
 * The output trace file format
 * <IP> <EA> <SIZE> <R/W> 
 */
FILE * trace;

/*
 * Number of OS pages for the buffer
 */
#define NUM_BUF_PAGES 4096

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
// This routine is executed when __parsec_roi_begin() is called.
VOID BeforeROI( THREADID threadid )
{
    fprintf(trace, "thread %d entered ROI\n", threadid);
    fflush(trace);
    ENABLE_LOGGING = TRUE;
}

// This routine is executed when __parsec_roi_begin() is called.
VOID AfterROI( THREADID threadid )
{
    fprintf(trace, "thread %d exited ROI\n#eof\n", threadid);
    fflush(trace);
    ENABLE_LOGGING = FALSE;
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
  printf("dumping buffer to file\n");
  for(unsigned int i=0; i<numElements; i++, reference++)
    {
      if (reference->pc != 0){
	fprintf(trace,"%lld %lld %d %d \n",
		(long long int) reference->pc, 
		(long long int) reference->ea,
		reference->size, reference->read);
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
// This routine is executed for each image.
VOID ImageLoad(IMG img, VOID *)
{
    RTN rtn = RTN_FindByName(img, "__parsec_roi_begin");
    RTN end_rtn = RTN_FindByName(img, "__parsec_roi_end");

    if ( RTN_Valid( rtn ))
    {
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(BeforeROI),
                       IARG_THREAD_ID, IARG_END);
        RTN_Close(rtn);

    }

    if ( RTN_Valid( end_rtn ))
    {
        RTN_Open(end_rtn);
        RTN_InsertCall(end_rtn, IPOINT_AFTER, AFUNPTR(AfterROI),
                       IARG_THREAD_ID, IARG_END);
        RTN_Close(end_rtn);
    }
}

/*
 * Called for every instruction and instruments reads and writes
 */
VOID Instruction(INS ins, VOID *v)
{

  if(INS_Valid(ins) )
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
    if (INS_HasMemoryRead2(ins))
    {
      refSize = INS_MemoryReadSize(ins);
      INS_InsertFillBufferPredicated(ins, IPOINT_BEFORE, bufId,
			   IARG_INST_PTR, offsetof(struct MEMREF, pc),
			   IARG_MEMORYREAD2_EA, offsetof(struct MEMREF, ea),
			   IARG_UINT32, refSize, offsetof(struct MEMREF, size),
			   IARG_BOOL, TRUE, offsetof(struct MEMREF, read), 
			   IARG_END);
    }
    if (INS_IsMemoryWrite(ins))
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
    fprintf(trace, "#eof\n");
    fflush(trace);
    fclose(trace);
}


int main(int argc, char *argv[])
{
    // Initialize pin
    PIN_InitSymbols();
    PIN_Init(argc, argv);

    printf("opening the trace file\n");
    trace = fopen(KnobOutputFile.Value().c_str(), "w");// error check here!!

    printf("opened the trace file\n");

    // Initialize the memory reference buffer;
    // set up the callback to process the buffer.
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
    //IMG_AddInstrumentFunction(ImageLoad, 0);

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();
    
    return 0;
}
