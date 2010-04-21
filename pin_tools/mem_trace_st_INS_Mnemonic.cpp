#include <stdio.h>
#include "pin.H"
#include "instlib.H"
#include <Python.h>

#define PIN_FAST_ANALYSIS_CALL

using namespace INSTLIB;

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "malloc_mt.out", "specify output file name");

/*
 * The ID of the buffer
 */
BUFFER_ID bufId;


/*
 * The output trace file format
 * <IP> <EA> <SIZE> <R/W> <# inst executed b/w this and prev. memory inst.>
 */
FILE * trace;

char* g_argv;

UINT64 ctr = 0;
char _inst[4096*4096][32];


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
  BOOL        read;
  char*       inst;
};

BOOL ENABLE_LOGGING = FALSE;

/*
 *==============================================================
 *  Analysis Routines
 *==============================================================
 */


void exec_pycode()
{
  Py_Initialize();

  // ugly hack to get python to open the trace file
  char* py_argv[2];
  py_argv[0] = "gen_PA.py";
  py_argv[1] = g_argv;

  PySys_SetArgv(2, py_argv);

  FILE* py_gen_PA = fopen("gen_PA.py", "r");
  if(py_gen_PA == NULL)
    {
      printf("Could not open file gen_PA.py\n");
      exit(0);
    }

  PyRun_SimpleFileEx(py_gen_PA,"gen_PA.py",1);

  Py_Finalize();
}

// This routine is executed when __parsec_roi_begin() is called.
VOID BeforeROI( THREADID threadid )
{

    // Open the trace file    
    trace = fopen(KnobOutputFile.Value().c_str(), "w");// error check here!!
    fprintf(trace, "thread %d entered ROI\n", threadid);
    fflush(trace);
    fclose(trace); // error check here!!

    /*
      now dump the VA->PA mappings from pagemap file for each 
      memory section in the trace file here
    */
    exec_pycode();

    ENABLE_LOGGING = TRUE;
    // Open the trace file again
    trace = fopen(KnobOutputFile.Value().c_str(), "a");// error check here!!
}

// This routine is executed when __parsec_roi_begin() is called.
VOID AfterROI( THREADID threadid )
{
    trace = fopen(KnobOutputFile.Value().c_str(), "a");// error check here!!
    fprintf(trace, "thread %d exited ROI\n#eof\n", threadid);
    fflush(trace);
    fclose(trace); // error check here!!

    /*
      now dump the VA->PA mappings from pagemap file for each 
      memory section in the trace file here
    */
    exec_pycode();
    ENABLE_LOGGING = FALSE;

    // Open the trace file again
    trace = fopen(KnobOutputFile.Value().c_str(), "a");// error check here!!
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
	fprintf(trace,"%d %d %p %p ",
		reference->size, reference->read, 
		(VOID*)reference->pc, (VOID*)reference->ea);
	//(reference->inst));

	for(int j=0; j<32; j++)
	  fprintf(trace,"%c",reference->inst[j]);

	fprintf(trace,"\n");

      }
    }
  fflush(trace);
  ctr = 0;
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

  if(!ENABLE_LOGGING)
    {
      return;
    }

  if(INS_Valid(ins) )
  {

    memset(&(_inst[ctr]), '\0',32);
    string temp = INS_Disassemble(ins);
    temp.copy(_inst[ctr], temp.size());

    UINT32 refSize;
    //char* inst_name = strdup(INS_Disassemble(ins).c_str());

    if (INS_IsMemoryRead(ins))
    {
      refSize = INS_MemoryReadSize(ins);
      INS_InsertFillBufferPredicated(ins, IPOINT_BEFORE, bufId,
			   IARG_INST_PTR, offsetof(struct MEMREF, pc),
			   IARG_MEMORYREAD_EA, offsetof(struct MEMREF, ea),
			   IARG_UINT32, refSize, offsetof(struct MEMREF, size),
			   IARG_BOOL, TRUE, offsetof(struct MEMREF, read),
			   IARG_ADDRINT, (ADDRINT) _inst[ctr], offsetof(struct MEMREF, inst),
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
       ctr++; //free(inst_name);
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

    for(int i = 0; i< argc; i++)
      printf("%s, ",argv[i]);
    // this is a horrible hack to read the output trace file's name 
    // which is then exported to python
    g_argv = argv[15];


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
    IMG_AddInstrumentFunction(ImageLoad, 0);

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();
    
    return 0;
}
