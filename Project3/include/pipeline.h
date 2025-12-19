#ifndef PIPELINE_H
#define PIPELINE_H

#include "inttypes.h"
#include <vector>
#include <queue>
#include <list>

class Pipeline{
    private:
        typedef struct{
            int seqNo;
            int opType;
            int dst;
            int src1Rdy = 0;
            int src2Rdy = 0;
            int srcOG[2];
            int src[2];
            int fe[2];
            int de[2];
            int rn[2];
            int rr[2];
            int di[2];
            int is[2];
            int ex[2];
            int wb[2];
            int rt[2];
        } Instruction;

        typedef struct{
            int head = 0;
            int tail = 0;
            std::vector<std::vector<int>> arr;
            std::vector<Instruction> instrArr;
        } ROB;
       

        FILE *instrCache;
        int cycle = 1;
        int instrCount = 0;
        int WIDTH;
        int IQ_SIZE;
        int ROB_SIZE;
        const int NUM_REGISTERS = 67;
        int feStall = 1;
        std::vector<Instruction> deFU;
        std::vector<Instruction> rnFU;
        std::vector<Instruction> rrFU;
        std::vector<Instruction> diFU;
        std::vector<Instruction> iq;
        std::vector<Instruction> execute_list;
        std::vector<Instruction> wbFU;
        std::vector<int> rmt;
        ROB rob;
    public:
        
        Pipeline(FILE *FP, int robSize, int iqSize, int width);
        
        ~Pipeline();

        bool advanceCycle();

        /**
         * Fetches instruction from PC. The instructions will be passed in and 
         * recorded as an Instruction struct.
         **/
        void fetch();

        /**
         * Passes the instruction bundle through to the next stage only if it is empty.
         **/
        void decode();

        /**
         * Function that allocates the next ROB entry at the current tail, renames the source registers
         * and then the destination registers in the RMT.
         **/
        void rename();

        /**
         * Pretend to "load" the values into the registers. We are not simulating real data so all this 
         * function will do is maske sure that the dispatch function unit it empty.
         **/
        void regRead();

        /**
         * Dispatch will find a spot in the issue queue(IQ).
         **/
        void dispatch();

        /**
         * Issue will send the ready instruction to the execute stage. But make sure to send the oldest ready
         * instruction first and only up to WIDTH instructions at a time. Remove an instr. from IQ, tehn 
         * add instr. to execute_list. Set a timer for the instr. in the execute_list that will allow you
         * to model its execution latency.
         **/
        void issue();

        /**
         * From the execute list check for instr.s that are finishing execution in current cycle.
         * Remove instr. from execute_list, add instr. to WB, then wakeup dependent instr. in IQ, DI, and RR.
         **/
        void execute();

        /**
         *
         *
         **/
        void writeback();

        /**
         *
         *
         **/
        void retire();

        void state();

        void details(char* trace_file);

        void output(Instruction instr);
};
#endif
