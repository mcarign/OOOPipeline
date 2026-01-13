# Out-of-Order Pipeline Simulator

This is a simulator for a superscaler Out-of-Order(OOO) Pipeline I developed for my Microprocessor Architecture(ECE 563) course at North Carolina State University. This simulator may not cover an OOO pipeline that is commercially used 1-to-1, but it covers the main aspects of an OOO pipeline. This project was meant to focus on modeling data dependencies, so we assumed perfect branch prediction and no expections occur which meant that I did not simulate a BTB, conditional branch predictor, instruction cache, data cache, or load/store queue to keep this program in the scope of the main lesson. 

The simulator reads a trace file of all the instruction that will be passed through it, each line contains pc number, operation type, destination register number, source register 1 number, and source register 2 number. It allows for the user to configure the size of the Reorder Buffer(ROB), Issue Queue(IQ), and function unit. The simulator measures the number of cycles it takes to complete the instructions from the trace as well as the IPC, which gave me the opportunity to perform a deeper analyses of IPC between different sizes across different sized configurations, which I will attach below.

## How to Execute the Program

There is a Makefile in the Project3/src file that compiles the program, make sure that the flag is set on -O3 when executing for a regular run and -g when trying to decode on gdb.

After successfully compiling, to execute the program enter ```./sim <ROB_SIZE> <IQ_SIZE> <WIDTH> <tracefile>``` into the command line.

## Analyses

### Large ROB, analyzing effect on IQ size:
(NOTE: These graphs each test two different tace files, gcc and perl trace files.)
(NOTE: IQ_SIZE and ROB_SIZE are both in Bytes)

<img width="474" height="354" alt="image" src="https://github.com/user-attachments/assets/d4c87433-de35-4083-9f9d-5c7d8415a325" />
<img width="474" height="354" alt="image" src="https://github.com/user-attachments/assets/98eb61bf-381e-4b28-80db-8b292dc93c7f" />

#### "Optimized IQ_SIZE per WIDTH"   
(Min IQ_SIZE that still achieves within 6% of the IPC of the largest IQ_SIZE)

||gcc|perl|
|--|--|--|
|WIDTH=1|8|8|
|WIDTH=2|16|16|
|WIDTH=4|32|64|
|WIDTH=8|64|128|

### Effect of Different ROB Sizes
(NOTE: IQ_SIZE and ROB_SIZE are both in Bytes)

<img width="474" height="354" alt="image" src="https://github.com/user-attachments/assets/66d16e72-a20b-419b-90b4-2f245ee93aca" />
<img width="474" height="354" alt="image" src="https://github.com/user-attachments/assets/b46f9d56-72f3-4da0-b21f-88775e14fdee" />
