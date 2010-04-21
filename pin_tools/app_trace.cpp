#include <stdio.h>
#include "pin.H"

FILE * trace;
string inst;

// This function is called before every instruction is executed
// and prints the IP
VOID printip(VOID *ip, VOID* ea, UINT32 size, BOOL rw, VOID* inst) 
{ 
  string local_inst = (char*) inst;

  if(ea == NULL)
    fprintf(trace, "%p\t\t%p\t\t\t%d\t\t%d\t\t%s\n", 
	    ip, ea, size, rw, local_inst.c_str()); 
  else
    fprintf(trace, "%p\t\t%p\t\t%d\t\t%d\t\t%s\n", 
	    ip, ea, size, rw, local_inst.c_str()); 
}

// Pin calls this function every time a new instruction is encountered
VOID Instruction(INS ins, VOID *v)
{
    UINT32 refSize = 0;
    inst = INS_Disassemble(ins);

    // Insert a call to printip before every instruction, and pass it the IP
    /*
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)printip, 
		   IARG_INST_PTR, IARG_PTR, inst.c_str(), IARG_END);
    */
   if (INS_IsMemoryRead(ins))
    {
      refSize = INS_MemoryReadSize(ins);
      INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)printip,
			   IARG_INST_PTR, 
			   IARG_MEMORYREAD_EA, 
			   IARG_UINT32, refSize, 
			   IARG_BOOL, TRUE, 
			   IARG_PTR, inst.c_str(),
			   IARG_END);
    }
   if (INS_HasMemoryRead2(ins))
    {
      refSize = INS_MemoryReadSize(ins);
      INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)printip,
			   IARG_INST_PTR, 
			   IARG_MEMORYREAD2_EA, 
			   IARG_UINT32, refSize, 
			   IARG_BOOL, TRUE, 
			   IARG_PTR, inst.c_str(),
			   IARG_END);
    }
   if (INS_IsMemoryWrite(ins))
    {
      refSize = INS_MemoryWriteSize(ins);
      INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)printip,
			   IARG_INST_PTR, 
			   IARG_MEMORYWRITE_EA, 
			   IARG_UINT32, refSize, 
			   IARG_BOOL, FALSE, 
			   IARG_PTR, inst.c_str(),
			   IARG_END);
    }
    else
    {
      INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)printip,
			   IARG_INST_PTR, 
			   IARG_PTR, 0, 
			   IARG_UINT32, 0, 
			   IARG_BOOL, FALSE,
			   IARG_PTR, inst.c_str(),
			   IARG_END);
    }


}

// This function is called when the application exits
VOID Fini(INT32 code, VOID *v)
{
    fprintf(trace, "#eof\n");
    fclose(trace);
}

// argc, argv are the entire command line, including pin -t <toolname> -- ...
int main(int argc, char * argv[])
{
    trace = fopen("itrace.out", "w");
    
    // Initialize pin
    PIN_Init(argc, argv);

    // Register Instruction to be called to instrument instructions
    INS_AddInstrumentFunction(Instruction, 0);

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);
    
    // Start the program, never returns
    PIN_StartProgram();
    
    return 0;
}
