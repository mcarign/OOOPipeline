#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <inttypes.h>
#include <math.h>
#include "../include/pipeline.h"

using namespace std;

Pipeline::Pipeline(FILE *FP, int robSize, int iqSize, int width){
    this->instrCache = FP;
    this->ROB_SIZE = robSize;
    this->IQ_SIZE = iqSize;
    this->WIDTH = width;
    this->rmt.resize(this->NUM_REGISTERS * 2, 0); // RMT has 2 columns [v, ROB tag]
    this->rob.instrArr.resize(this->ROB_SIZE);
    this->rob.arr.resize(this->ROB_SIZE);
    for(int i = 0; i < this->ROB_SIZE; i++){
        this->rob.arr[i].resize(3, 0);
    }
}

Pipeline::~Pipeline(){
}

bool Pipeline::advanceCycle(){
//    state();
//    printf("^^^^^^^^^^^^^^^^^^^^^^^^^^ CYCLE %d ----------------------------\n", this->cycle);
//    if(this->cycle == 58){ return false; }
    if( (this->rob.arr[this->rob.head][2] == 1 || !this->deFU.empty() || !this->rnFU.empty() || !this->rrFU.empty() || !this->diFU.empty() || !this->iq.empty() || !this->execute_list.empty() || !this->wbFU.empty())){
        this->cycle++;
        return true;
    }
    return false;
}

void Pipeline::fetch(){

//    if(feof(this->instrCache)){
//        return;
//    }
    if(this->deFU.empty()){
        int op_type, dest, src1, src2;  // Variables are read from trace file
        uint64_t pc; // Variable holds the pc read from input file

        for(int i = 0; i < this->WIDTH; i++){
            if(fscanf(this->instrCache, "%lx %d %d %d %d", &pc, &op_type, &dest, &src1, &src2) == EOF){
                return;
            }
//            fscanf(this->instrCache, "%lx %d %d %d %d", &pc, &op_type, &dest, &src1, &src2);
            Instruction instr;
            instr.seqNo = this->instrCount;
            instr.opType = op_type;
            instr.dst = dest;
            instr.src[0] = src1;
            instr.srcOG[0] = src1;
            instr.src[1] = src2;
            instr.srcOG[1] = src2;
            instr.fe[0] = this->cycle - 1;
            instr.fe[1] = 1;
            instr.de[0] = this->cycle;
            this->instrCount++;
            this->deFU.push_back(instr);
            
//            if(feof(this->instrCache)){
//                break;
//            }
        }
        this->feStall = 1;
    }else{
        this->feStall++;
    }
}

void Pipeline::decode(){
    if(this->rnFU.empty() && !this->deFU.empty()){
        for(size_t i = 0; i < this->deFU.size(); i++){
            this->deFU.at(i).de[1] = this->cycle - this->deFU.at(i).de[0];
            this->deFU.at(i).rn[0] = this->cycle;
        }
        this->rnFU = this->deFU;
        this->deFU.clear();
    }
}

void Pipeline::rename(){
    Instruction* instr; 
    int dstCol = 0;
    int rdyCol = 1;
    int phaseCol = 2;
    int vacancies = 0;
    int robUnqfy = 1000; // All rob entry indexes will be >= 100
    
//    printf("head= %d    tail= %d\n",this->rob.head,this->rob.tail);
    int occupied = (this->rob.tail - this->rob.head + this->ROB_SIZE) % this->ROB_SIZE;
    if(this->rob.arr[this->rob.tail][phaseCol] == 0){
        vacancies = this->ROB_SIZE - occupied;
//        printf("vacancies= %d\n", vacancies);
    }
//    printf("head=%d    tail=%d\n",this->rob.head, this->rob.tail);
//    printf("occupied=%d    vacancies=%d\n", occupied, vacancies);
    
//    printf("rrFU is empty:%d    rnFU not empty:%d    vacancies >= width:%d\n",this->rrFU.empty(), !this->rnFU.empty(), vacancies>=this->WIDTH);
    if(this->rrFU.empty() && !this->rnFU.empty() && vacancies >= this->WIDTH){
        for(size_t i = 0; i < this->rnFU.size(); i++){
            instr = &this->rnFU.at(i);
            // ALLOCATE a spot in the ROB for the DST. ---------------
            this->rob.arr[this->rob.tail][dstCol] = instr->dst;
            this->rob.arr[this->rob.tail][rdyCol] = 0;
            this->rob.arr[this->rob.tail][phaseCol] = 1;
            // Rename the SOURCES from RMT. ----------------------------
            int src1 = instr->src[0];
            int src2 = instr->src[1];
//            printf("Before: src1=%d\n", instr->src[0]);
            if(src1 != -1 && this->rmt[src1*2] != 0){ 
                instr->src[0] = this->rmt[src1*2+1];
            }
//            printf("Before: src2=%d\n", instr->src[1]);
            if(src2 != -1 && this->rmt[src2*2] != 0){ 
                instr->src[1] = this->rmt[src2*2+1]; 
            }
//            printf("After: src1=%d\n", instr->src[0]);
//            printf("After: src2=%d\n", instr->src[1]);
            // Rename the DESTINATION in ROB. -------------------------------
            if(instr->dst != -1){
                this->rmt[instr->dst*2] = 1;
                this->rmt[instr->dst*2+1] = this->rob.tail + robUnqfy;
            }
            instr->dst = this->rob.tail + robUnqfy;
            this->rob.tail = (this->rob.tail + 1) % this->ROB_SIZE;
        }
        // Move current instructions to next stage --------------------------
        for(size_t i = 0; i < this->rnFU.size(); i++){
            this->rnFU.at(i).rn[1] = this->cycle - this->rnFU.at(i).rn[0];
            this->rnFU.at(i).rr[0] = this->cycle;
        }
        this->rrFU = this->rnFU;
        this->rnFU.clear();
    }
}

void Pipeline::regRead(){
    Instruction* instr;
    int rdyCol = 1;
    if(this->diFU.empty() && !this->rrFU.empty()){
        for(size_t i = 0; i < this->rrFU.size(); i++){
            instr = &this->rrFU.at(i);
            // checking src1 if ready in ROB
            int robTag = instr->src[0] - 1000;
            if(robTag >= 0 && this->rob.arr[robTag][rdyCol] == 1){
                instr->src1Rdy = 1;
            }
            if(instr->src[0] < this->NUM_REGISTERS){ 
                instr->src1Rdy = 1; 
            }
            // checking src2 if ready in ROB
            robTag = instr->src[1] - 1000;
            if(robTag >= 0 && this->rob.arr[robTag][rdyCol] == 1){
                instr->src2Rdy = 1;
            }
            if(instr->src[1] < this->NUM_REGISTERS){ 
                instr->src2Rdy = 1;
            }
            instr->rr[1] = this->cycle - instr->rr[0];
            instr->di[0] = this->cycle;
        }
        this->diFU = this->rrFU;
        this->rrFU.clear();
    }
}

void Pipeline::dispatch(){
    int vacancies = this->IQ_SIZE - static_cast<int>(this->iq.size());

    if(static_cast<int>(this->diFU.size()) <= vacancies && !this->diFU.empty()){
        for(size_t i = 0; i < this->diFU.size(); i++){
            this->diFU.at(i).di[1] = this->cycle - this->diFU.at(i).di[0];
            this->diFU.at(i).is[0] = this->cycle;
            this->iq.push_back(this->diFU.at(i));
        }
        this->diFU.clear();
    }
}

void Pipeline::issue(){
    int issued = 0;

    if(this->iq.empty()){ return; }

    for(size_t i = 0; i < this->iq.size() && issued < this->WIDTH; i++){
        if(this->iq.at(i).src1Rdy && this->iq.at(i).src2Rdy){    // If valid bit 1, issue
            this->iq.at(i).is[1] = this->cycle - this->iq.at(i).is[0];
            this->iq.at(i).ex[0] = this->cycle;
            this->execute_list.push_back(this->iq.at(i));
            this->iq.erase(this->iq.begin() + i);
            i--;    // to stay in place
            issued++;
        }
    }
}

void Pipeline::execute(){
    int latency;
    int dstCol = 0;

    if(this->execute_list.empty()){ return; }

    for(size_t i = 0; i < this->execute_list.size() && static_cast<int>(i) < this->WIDTH*5; i++){
        Instruction* instr = &this->execute_list.at(i);
        switch (instr->opType){
            case 0:
                latency = 1;
                break;
            case 1:
                latency = 2;
                break;
            case 2:
                latency = 5;
                break;
            default:
                exit(0);
        }
        if(instr->ex[0] + latency == this->cycle){
            int robUnqfy = 1000;
            instr->ex[1] = this->cycle - instr->ex[0];
            instr->wb[0] = this->cycle;
            this->wbFU.push_back(*instr);
            if(instr->src[0] >= robUnqfy){    // Re-renaming src1 for print out
                int robTag = instr->src[0]-robUnqfy;
                this->wbFU.back().src[0] = this->rob.arr[robTag][dstCol];
            }
            if(instr->src[1] >= robUnqfy){    // Re-renaming src2 for print out
                int robTag = instr->src[1]-robUnqfy;
                this->wbFU.back().src[1] = this->rob.arr[robTag][dstCol];
            }
            // wake up in IQ
            for(size_t j = 0; j < this->iq.size(); j++){
                if(instr->dst == this->iq.at(j).src[0]){
                    this->iq.at(j).src1Rdy = 1;
                }
                if(instr->dst == this->iq.at(j).src[1]){
                    this->iq.at(j).src2Rdy = 1;
                }
            }
            // wake up in DI
            for(size_t j = 0; j < this->diFU.size(); j++){
                if(instr->dst == this->diFU.at(j).src[0]){
                    this->diFU.at(j).src1Rdy = 1;
                }
                if(instr->dst == this->diFU.at(j).src[1]){
                    this->diFU.at(j).src2Rdy = 1;
                }
            }
            // wake up in RR
            for(size_t j = 0; j < this->rrFU.size(); j++){
                if(instr->dst == this->rrFU.at(j).src[0]){
                    this->rrFU.at(j).src1Rdy = 1;
                }
                if(instr->dst == this->rrFU.at(j).src[1]){
                    this->rrFU.at(j).src2Rdy = 1;
                }
            }
            this->execute_list.erase(this->execute_list.begin()+i);
            i--;    // to stay in place
        }
    }
}

void Pipeline::writeback(){
    int robTag;
    int robUnqfy = 1000;
    int dstCol = 0;
    int rdyCol = 1;

    for(size_t i = 0; i < this->wbFU.size(); i++){
        robTag = this->wbFU.at(i).dst - robUnqfy;
        this->rob.arr[robTag][rdyCol] = 1;
        this->wbFU.at(i).dst = this->rob.arr[robTag][dstCol];
        this->wbFU.at(i).wb[1] = this->cycle - this->wbFU.at(i).wb[0];
        this->wbFU.at(i).rt[0] = this->cycle;
        this->rob.instrArr[robTag] = this->wbFU.at(i);
    }
    this->wbFU.clear();
}

void Pipeline::retire(){
    vector<int> *robEntry;
    int dstCol = 0;
    int rdyCol = 1;
    int phaseCol = 2;

    for(int i = 0; i < this->WIDTH; i++){
        robEntry = &this->rob.arr[this->rob.head];
        if(robEntry->at(rdyCol) == 0 || robEntry->at(phaseCol) == 0){
            return;
        }
        
//        for(int j = 0; j < this->NUM_REGISTERS; j++){
//            if(this->rmt[j*2+1] == this->rob.head + 1000){
//                this->rmt[robEntry->at(dstCol)*2] = 0;
//                break;
//            }
//        }
        if(robEntry->at(dstCol) != -1 && this->rmt[robEntry->at(dstCol)*2+1] == this->rob.head + 1000){
                this->rmt[robEntry->at(dstCol)*2] = 0;
        }
        this->rob.instrArr.at(this->rob.head).rt[1] = this->cycle - this->rob.instrArr.at(this->rob.head).rt[0];
        output(this->rob.instrArr.at(this->rob.head));
        this->rob.arr[this->rob.head][phaseCol] = 0;
        this->rob.head = (this->rob.head + 1) % this->ROB_SIZE;
    }
}

void Pipeline::details(char* trace_file){ 
    printf("# === Simulator Command =========\n");
    printf("# ./sim %d %d %d %s\n", this->ROB_SIZE, this->IQ_SIZE, this->WIDTH, trace_file);
    printf("# === Processor Configuration ===\n");
    printf("# ROB_SIZE = %d\n", this->ROB_SIZE);
    printf("# IQ_SIZE  = %d\n", this->IQ_SIZE);
    printf("# WIDTH    = %d\n", this->WIDTH);
    printf("# === Simulation Results ========\n");
    printf("# Dynamic Instruction Count    = %d\n", this->instrCount);
    printf("# Cycles                       = %d\n", this->cycle);
    printf("# Instructions Per Cycle (IPC) = %.2f\n", (float)this->instrCount / (float)this->cycle);
}

/**
 * Simulator outputs the timing information for each dynamic instruction in the trace, in program order.
 */
void Pipeline::output(Instruction instr){
    printf("%d fu{%d} src{%d,%d} dst{%d} FE{%d,%d} DE{%d,%d} RN{%d,%d} RR{%d,%d} DI{%d,%d} IS{%d,%d} EX{%d,%d} WB{%d,%d} RT{%d,%d}\n",
    instr.seqNo, instr.opType, instr.srcOG[0], instr.srcOG[1], instr.dst, instr.fe[0], instr.fe[1], instr.de[0], instr.de[1], instr.rn[0], instr.rn[1], instr.rr[0], instr.rr[1],
    instr.di[0], instr.di[1], instr.is[0], instr.is[1], instr.ex[0], instr.ex[1], instr.wb[0], instr.wb[1], instr.rt[0], instr.rt[1]);
}

void Pipeline::state(){
    printf("DE fu size: %lu\n", this->deFU.size());
    if(!this->deFU.empty()){
        for(size_t i = 0 ; i < this->deFU.size(); i++){
            Instruction instr = this->deFU.at(i);
            printf("deFU[%lu]: seqNo=%d, op=%d, dst=%d, src1=%d, src1Rdy=%d, src2=%d, src2Rdy=%d\n", i, instr.seqNo, instr.opType, instr.dst, instr.src[0],instr.src1Rdy, instr.src[1],instr.src2Rdy );
        }
    }
    printf("RN fu size: %lu\n", this->rnFU.size());
    if(!this->rnFU.empty()){
        for(size_t i = 0 ; i < this->rnFU.size(); i++){
            Instruction instr = this->rnFU.at(i);
            printf("rnFU[%lu]: seqNo=%d, op=%d, dst=%d, src1=%d, src1Rdy=%d, src2=%d, src2Rdy=%d\n", i, instr.seqNo, instr.opType, instr.dst, instr.src[0],instr.src1Rdy, instr.src[1],instr.src2Rdy );
        }
    }
    printf("RR fu size: %lu\n", this->rrFU.size());
    if(!this->rrFU.empty()){
        for(size_t i = 0 ; i < this->rrFU.size(); i++){
            Instruction instr = this->rrFU.at(i);
            printf("rrFU[%lu]: seqNo=%d, op=%d, dst=%d, src1=%d, src1Rdy=%d, src2=%d, src2Rdy=%d\n", i, instr.seqNo, instr.opType, instr.dst, instr.src[0],instr.src1Rdy, instr.src[1],instr.src2Rdy );
        }
    }
    printf("DI fu size: %lu\n", this->diFU.size());
    if(!this->diFU.empty()){
        for(size_t i = 0 ; i < this->diFU.size(); i++){
            Instruction instr = this->diFU.at(i);
            printf("diFU[%lu]: seqNo=%d, op=%d, dst=%d, src1=%d, src1Rdy=%d, src2=%d, src2Rdy=%d\n", i, instr.seqNo, instr.opType, instr.dst, instr.src[0],instr.src1Rdy, instr.src[1],instr.src2Rdy );
        }
    }
    printf("IQ fu size: %lu\n", this->iq.size());
    if(!this->iq.empty()){
        for(size_t i = 0 ; i < this->iq.size(); i++){
            Instruction instr = this->iq.at(i);
            printf("iq[%lu]: seqNo=%d, op=%d, dst=%d, src1=%d, src1Rdy=%d, src2=%d, src2Rdy=%d\n", i, instr.seqNo, instr.opType, instr.dst, instr.src[0],instr.src1Rdy, instr.src[1],instr.src2Rdy );
        }
    }
    printf("EX fu size: %lu\n", this->execute_list.size());
    if(!this->execute_list.empty()){
        for(size_t i = 0 ; i < this->execute_list.size(); i++){
            Instruction instr = this->execute_list.at(i);
            printf("exFU[%lu]: seqNo=%d, op=%d, dst=%d, src1=%d, src1Rdy=%d, src2=%d, src2Rdy=%d\n", i, instr.seqNo, instr.opType, instr.dst, instr.src[0],instr.src1Rdy, instr.src[1],instr.src2Rdy );
        }
    }
    printf("WB fu size: %lu\n", this->wbFU.size());
    if(!this->wbFU.empty()){
        for(size_t i = 0 ; i < this->wbFU.size(); i++){
            Instruction instr = this->wbFU.at(i);
            printf("wbFU[%lu]: seqNo=%d, op=%d, dst=%d, src1=%d, src1Rdy=%d, src2=%d, src2Rdy=%d\n", i, instr.seqNo, instr.opType, instr.dst, instr.src[0],instr.src1Rdy, instr.src[1],instr.src2Rdy );
        }
    }
    int iters = 0;
    printf("head=%d    tail=%d\n",this->rob.head, this->rob.tail);
    for(int i = this->rob.head; iters < static_cast<int>(this->rob.arr.size()) && this->rob.arr[i][1] != -1; i=(i+1)%this->ROB_SIZE){
        printf("rob[%d]:    dst=%d    rdy=%d    phase=%d\n",i, this->rob.arr[i][0], this->rob.arr[i][1], this->rob.arr[i][2]);
        iters++;
    }
    for(int i = 0; i < this->NUM_REGISTERS; i++){
        printf("rmt[%d]:    v=%d    rob tag=%d\n", i, this->rmt[i*2], this->rmt[i*2+1]);
    }
}

