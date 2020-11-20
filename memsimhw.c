//
// Virual Memory Simulator Homework
// One-level page table system with FIFO and LRU
// Two-level page table system with LRU
// Inverted page table with a hashing system 
// Submission Year: 2020-2
// Student Name: 박주훈 ( Park Juhun )
// Student Number: B511072
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define PAGESIZEBITS 12			// page size = 4Kbytes
#define VIRTUALADDRBITS 32		// virtual address space size = 4Gbytes

struct frameEntry {
    int pid;
    int fid;
    struct frameEntry *prev;
    struct frameEntry *next;
    int vpnFrom;    // to find PTE source and set invalid
    int svpnFrom;
    struct IHTEntry* nodeFrom;
} frameHead;

struct IHTEntry {
    int fid;
    int pid;
    int vpn;
    struct IHTEntry *next;
    struct IHTEntry *prev; 
} *IHT;

struct pageTableEntry {
    struct frameEntry *frame;
    struct pageTableEntry *secondLevelPageTable;
    int valid;
};

struct procEntry {
	char *traceName;			// the memory trace name
	int pid;					// process (trace) id
	int ntraces;				// the number of memory traces
	int num2ndLevelPageTable;	// The 2nd level page created(allocated);
	int numIHTConflictAccess; 	// The number of Inverted Hash Table Conflict Accesses
	int numIHTNULLAccess;		// The number of Empty Inverted Hash Table Accesses
	int numIHTNonNULLAcess;		// The number of Non Empty Inverted Hash Table Accesses
	int numPageFault;			// The number of page faults
	int numPageHit;				// The number of page hits
	struct pageTableEntry *firstLevelPageTable;
	FILE *tracefp;
};

// for debugging
// void checkFrameList(){
//     int i;
//     struct frameEntry* temp = &frameHead;
//     puts("traverse start");
//     while(temp->next != &frameHead){
//         // printf("fid,vpn : %d, %x\n", (temp->next)->fid, (temp->next)->vpnFrom);
//         printf("fid,vpn : %d, %x, %x\n", (temp->next)->fid, (temp->next)->vpnFrom, (temp->next)->svpnFrom);
//         temp = temp->next;
//     }
//     puts("traverse end");
// }

// void checkFrameListRev(){
//     int i;
//     struct frameEntry* temp = &frameHead;
//     puts("rev traverse start");
//     while(temp->prev != &frameHead){
//         // printf("fid,vpn : %d, %x\n", (temp->prev)->fid, (temp->prev)->vpnFrom);
//         printf("fid,vpn : %d, %x, %x\n", (temp->prev)->fid, (temp->prev)->vpnFrom, (temp->prev)->svpnFrom);
//         temp = temp->prev;
//     }
//     puts("rev traverse end");
// }

void replaceFirst(struct procEntry* procTable, int pid, int vpn, int svpn, int type, struct IHTEntry* node){

    struct frameEntry* temp;
    struct pageTableEntry* SLPT;
    struct IHTEntry* IHTtemp;
    
    if (type == 1){ // using 2nd-level page table
        SLPT = (procTable[pid].firstLevelPageTable)[vpn].secondLevelPageTable;
        SLPT[svpn].valid = 1;   // set PTE valid

        SLPT[svpn].frame = frameHead.next;   // replace head frame
        ((procTable[frameHead.next->pid].firstLevelPageTable)[frameHead.next->vpnFrom].secondLevelPageTable)[frameHead.next->svpnFrom].valid = 0;    // set premapped PTE invalid
        frameHead.next->vpnFrom = vpn;
        frameHead.next->svpnFrom = svpn;
        frameHead.next->pid = pid;

    } else if (type == 2) {
        int HTindex, nFrame;
        nFrame = svpn;  // use empty argument space

        HTindex = (node->vpn + node->pid) % nFrame;
        
        if (IHT[HTindex].next == NULL){ // push to empty
            IHT[HTindex].next = node;
            node->prev = &IHT[HTindex];
            node->next = NULL;
        } else {    // push (switch head)
            IHTtemp = IHT[HTindex].next;   // old head
            IHT[HTindex].next = node;   // new head
            node->prev = &IHT[HTindex];
            node->next = IHTtemp;
            IHTtemp->prev = node;
        }

        node->fid = frameHead.next->fid;
        
        // remove old mapping
        IHTtemp = frameHead.next->nodeFrom;
        if (IHTtemp->next == NULL){ // if tail
            (IHTtemp->prev)->next = NULL;
            free(IHTtemp);
        } else {
            (IHTtemp->prev)->next = IHTtemp->next;
            (IHTtemp->next)->prev = IHTtemp->prev;
            free(IHTtemp);
        }
        frameHead.next->nodeFrom = node;
        
    } else {

        (procTable[pid].firstLevelPageTable)[vpn].valid = 1;    // set PTE valid

        (procTable[pid].firstLevelPageTable)[vpn].frame = frameHead.next;   // replace head frame
        (procTable[frameHead.next->pid].firstLevelPageTable)[frameHead.next->vpnFrom].valid = 0;    // set premapped PTE invalid
        frameHead.next->vpnFrom = vpn;
        frameHead.next->pid = pid;
    }

    temp = frameHead.next;
    frameHead.next = temp->next;
    (temp->next)->prev = &frameHead;    // temp get detached from head

    (frameHead.prev)->next = temp;
    temp->prev = frameHead.prev;
    frameHead.prev = temp;
    temp->next = &frameHead; // temp get attached to tail
}

void oneLevelVMSim(int replace_type, int nFrame, int numProcess, struct procEntry* procTable) {
    int i;
    int nMapped;
    int targetFid;

    unsigned addr;
    unsigned vpn;
    char rw;

    struct frameEntry* temp;

    struct frameEntry* frames = (struct frameEntry*)malloc(sizeof(struct frameEntry) * nFrame);

    nMapped = 0;
    i=0;
    while(1){
        // puts("------------------------------------");
        if (fscanf(procTable[i].tracefp, "%x %c", &addr, &rw) == EOF)
            break;

        vpn = addr/4096;    // first 20 bit of 32bit full virtual address
        if (nMapped < nFrame && (procTable[i].firstLevelPageTable)[vpn].frame == NULL){    // before frame fully mapped
            frames[nMapped].fid = nMapped;
            frames[nMapped].pid = i;

            if (nMapped == 0){
                frameHead.next = &frames[0];
                frameHead.prev = &frames[0];
                frames[0].prev = &frameHead;
                frames[0].next = &frameHead;
                frames[0].vpnFrom = vpn;
            } else {
                temp = frameHead.prev; // tail
                frameHead.prev = &frames[nMapped]; // new tail
                temp->next = &frames[nMapped];
                frames[nMapped].prev = temp;
                frames[nMapped].next = &frameHead;
                frames[nMapped].vpnFrom = vpn;
            }

            (procTable[i].firstLevelPageTable)[vpn].frame = &frames[nMapped];
            (procTable[i].firstLevelPageTable)[vpn].valid = 1;

            procTable[i].numPageFault++;
            nMapped++;
        } else {    // after frame fully mapped

            if ( ((procTable[i].firstLevelPageTable)[vpn].frame != NULL) && (procTable[i].firstLevelPageTable)[vpn].valid ){ // hit
                if (replace_type == 1){ // update frame_list order when hit access using LRU
                    targetFid = (procTable[i].firstLevelPageTable)[vpn].frame->fid;
                    temp = &frames[targetFid];

                    (temp->prev)->next = temp->next;    // detach frame
                    (temp->next)->prev = temp->prev;

                    temp->prev = frameHead.prev;    // attach to tail
                    temp->next = &frameHead;
                    (frameHead.prev)->next = temp;
                    frameHead.prev = temp;       
                }
                procTable[i].numPageHit++;
            }
            else {    // fault
                procTable[i].numPageFault++;
                replaceFirst(procTable, i, vpn, 0, 0, 0);
            }

        }
        procTable[i].ntraces++;

        i++;
        if (i == numProcess)
            i=0;
    }

	for(i=0; i < numProcess; i++) {
		printf("**** %s *****\n",procTable[i].traceName);
		printf("Proc %d Num of traces %d\n",i,procTable[i].ntraces);
		printf("Proc %d Num of Page Faults %d\n",i,procTable[i].numPageFault);
		printf("Proc %d Num of Page Hit %d\n",i,procTable[i].numPageHit);
		assert(procTable[i].numPageHit + procTable[i].numPageFault == procTable[i].ntraces);
	}
	
}
void twoLevelVMSim(int nFrame, int numProcess, struct procEntry* procTable, int first_level_bits) {
    int i;
    int nMapped;
    int targetFid;
    int second_level_bits = 32 - first_level_bits - 12;

    unsigned addr;
    unsigned fvpn, svpn;
    char rw;

    struct pageTableEntry* SLPT;
    struct frameEntry* temp;

    struct frameEntry* frames = (struct frameEntry*)malloc(sizeof(struct frameEntry) * nFrame);

    nMapped = 0;
    i=0;
    while(1){
        if (fscanf(procTable[i].tracefp, "%x %c", &addr, &rw) == EOF)
            break;

        fvpn = addr/(1L<<(32-first_level_bits));  // first_level_bits of 32bit full virtual address
        svpn = (addr%(1L<<(32-first_level_bits)))/(1L<<12);  // second_level_bits of 32bit full virtual address

        if ( (procTable[i].firstLevelPageTable)[fvpn].secondLevelPageTable == NULL ){    // make SLPT
            (procTable[i].firstLevelPageTable)[fvpn].secondLevelPageTable = (struct pageTableEntry*)malloc(sizeof(struct pageTableEntry) * (1L<<second_level_bits));
            procTable[i].num2ndLevelPageTable++;
        }
        SLPT = (procTable[i].firstLevelPageTable)[fvpn].secondLevelPageTable;

        if (nMapped < nFrame && SLPT[svpn].frame == NULL){  // before frame fully mapped

            frames[nMapped].fid = nMapped;
            frames[nMapped].pid = i;

            if (nMapped == 0){
                frameHead.next = &frames[0];
                frameHead.prev = &frames[0];
                frames[0].prev = &frameHead;
                frames[0].next = &frameHead;
                frames[0].vpnFrom = fvpn;
                frames[0].svpnFrom = svpn;
            } else {
                temp = frameHead.prev; // tail
                frameHead.prev = &frames[nMapped]; // new tail
                temp->next = &frames[nMapped];
                frames[nMapped].prev = temp;
                frames[nMapped].next = &frameHead;
                frames[nMapped].vpnFrom = fvpn;
                frames[nMapped].svpnFrom = svpn;
            }

            SLPT[svpn].frame = &frames[nMapped];

            SLPT[svpn].valid = 1;
            procTable[i].numPageFault++;
            nMapped++;

        } else {
            if (SLPT[svpn].frame != NULL && SLPT[svpn].valid){ // hit
                targetFid = SLPT[svpn].frame->fid;
                temp = &frames[targetFid];

                (temp->prev)->next = temp->next;    // detach frame
                (temp->next)->prev = temp->prev;

                temp->prev = frameHead.prev;    // attach to tail
                temp->next = &frameHead;
                (frameHead.prev)->next = temp;
                frameHead.prev = temp;

                procTable[i].numPageHit++;

            } else {    // fault
                procTable[i].numPageFault++;
                replaceFirst(procTable, i, fvpn, svpn, 1, 0);
            }
        }

        procTable[i].ntraces++;
        i++;
        if (i == numProcess)
            i=0;
    }

	for(i=0; i < numProcess; i++) {
		printf("**** %s *****\n",procTable[i].traceName);
		printf("Proc %d Num of traces %d\n",i,procTable[i].ntraces);
		printf("Proc %d Num of second level page tables allocated %d\n",i,procTable[i].num2ndLevelPageTable);
		printf("Proc %d Num of Page Faults %d\n",i,procTable[i].numPageFault);
		printf("Proc %d Num of Page Hit %d\n",i,procTable[i].numPageHit);
		assert(procTable[i].numPageHit + procTable[i].numPageFault == procTable[i].ntraces);
	}

}

int findNode(struct IHTEntry* IHT, int pid, int vpn, int nFrame, struct procEntry* procTable){
    int HTindex;
    struct IHTEntry* temp;

    HTindex = (pid+vpn)%nFrame;
    temp = &IHT[HTindex];

    if(temp->next)  // if list is not empty
        procTable[pid].numIHTNonNULLAcess++;
    else
        procTable[pid].numIHTNULLAccess++;

    while(temp->next){  // if list is not empty, traverse list and find
        temp = temp->next;
        procTable[pid].numIHTConflictAccess++;

        if( (temp->pid == pid) && (temp->vpn == vpn) ){
            return temp->fid;
        }
    }

    return -1;   // -1 on not matched
}

void invertedPageVMSim(int nFrame, int numProcess, struct procEntry* procTable) {
    int i;
    int nMapped;
    int targetFid;

    unsigned addr;
    unsigned vpn;
    char rw;

    struct frameEntry* temp;
    struct IHTEntry* IHTtemp;

    struct frameEntry* frames = (struct frameEntry*)malloc(sizeof(struct frameEntry) * nFrame);

    IHT = (struct IHTEntry*)malloc(sizeof(struct IHTEntry) * nFrame);


    nMapped = 0;
    i=0;
    while(1){
        int fid;
        int HTindex;
        struct IHTEntry* node;

        if (fscanf(procTable[i].tracefp, "%x %c", &addr, &rw) == EOF)
            break;

        vpn = addr/4096;    // first 20 bit of 32bit full virtual address
        if ( (fid = findNode(IHT, i, vpn, nFrame, procTable)) == -1 && nMapped < nFrame ){    // before frame fully mapped
            frames[nMapped].fid = nMapped;

            if (nMapped == 0){
                frameHead.next = &frames[0];
                frameHead.prev = &frames[0];
                frames[0].prev = &frameHead;
                frames[0].next = &frameHead;
            } else {
                temp = frameHead.prev; // tail
                frameHead.prev = &frames[nMapped]; // new tail
                temp->next = &frames[nMapped];
                frames[nMapped].prev = temp;
                frames[nMapped].next = &frameHead;
            }

            // link node with IHT
            node = (struct IHTEntry*)malloc(sizeof(struct IHTEntry));
            node->pid = i;
            node->vpn = vpn;

            HTindex = (vpn + i) % nFrame;
            if (IHT[HTindex].next == NULL){ // push to empty
                IHT[HTindex].next = node;
                node->prev = &IHT[HTindex];
                node->next = NULL;
            } else {    // push (switch head)
                IHTtemp = IHT[HTindex].next;   // old head
                IHT[HTindex].next = node;   // new head
                node->prev = &IHT[HTindex];
                node->next = IHTtemp;
                IHTtemp->prev = node;
            }

            frames[nMapped].nodeFrom = node;
            node->fid = nMapped;            

            procTable[i].numPageFault++;
            nMapped++;
        } else {    // after frame fully mapped

            if (fid != -1){ // hit
                // update frame_list order when hit access using LRU
                temp = &frames[fid];

                (temp->prev)->next = temp->next;    // detach frame
                (temp->next)->prev = temp->prev;

                temp->prev = frameHead.prev;    // attach to tail
                temp->next = &frameHead;
                (frameHead.prev)->next = temp;
                frameHead.prev = temp;

                procTable[i].numPageHit++;
            }
            else {    // fault
                node = (struct IHTEntry*)malloc(sizeof(struct IHTEntry));
                node->pid = i;
                node->vpn = vpn;

                replaceFirst(procTable, i, vpn, nFrame, 2, node);
                
                procTable[i].numPageFault++;
            }

        }
        procTable[i].ntraces++;

        i++;
        if (i == numProcess)
            i=0;
    }


	for(i=0; i < numProcess; i++) {
		printf("**** %s *****\n",procTable[i].traceName);
		printf("Proc %d Num of traces %d\n",i,procTable[i].ntraces);
		printf("Proc %d Num of Inverted Hash Table Access Conflicts %d\n",i,procTable[i].numIHTConflictAccess);
		printf("Proc %d Num of Empty Inverted Hash Table Access %d\n",i,procTable[i].numIHTNULLAccess);
		printf("Proc %d Num of Non-Empty Inverted Hash Table Access %d\n",i,procTable[i].numIHTNonNULLAcess);
		printf("Proc %d Num of Page Faults %d\n",i,procTable[i].numPageFault);
		printf("Proc %d Num of Page Hit %d\n",i,procTable[i].numPageHit);
		assert(procTable[i].numPageHit + procTable[i].numPageFault == procTable[i].ntraces);
		assert(procTable[i].numIHTNULLAccess + procTable[i].numIHTNonNULLAcess == procTable[i].ntraces);
	}
}

int main(int argc, char *argv[]) {
	int SIM_TYPE, FIRST_LEVEL_BITS, PHYSICAL_MEMORY_SIZE_BITS;

    struct procEntry *procTable;

    int numProcess, nFrame;
    int i;

    SIM_TYPE = atoi(argv[1]);
    FIRST_LEVEL_BITS = atoi(argv[2]);
    PHYSICAL_MEMORY_SIZE_BITS = atoi(argv[3]);
	
	// initialize procTable for Memory Simulations
    numProcess = argc - 4;
    procTable = (struct procEntry*)malloc(sizeof(struct procEntry) * numProcess);
	for(i = 0; i < numProcess; i++) {

        // initialize
        procTable[i].traceName = argv[i+4];
        procTable[i].pid = i;
        procTable[i].ntraces = 0;
        procTable[i].num2ndLevelPageTable = 0;
        procTable[i].numIHTConflictAccess = 0;
        procTable[i].numIHTNULLAccess = 0;
        procTable[i].numIHTNonNULLAcess = 0;
        procTable[i].numPageFault = 0;
        procTable[i].numPageHit = 0;

        if (SIM_TYPE == 2){
            procTable[i].firstLevelPageTable = (struct pageTableEntry*)malloc(sizeof(struct pageTableEntry) * (1L<<FIRST_LEVEL_BITS));
        } else {
            procTable[i].firstLevelPageTable = (struct pageTableEntry*)malloc(sizeof(struct pageTableEntry) * (1L<<20));
        }

		// opening a tracefile for the process
		printf("process %d opening %s\n",i,argv[i+4]);
		procTable[i].tracefp = fopen(argv[i+4],"r");
		if (procTable[i].tracefp == NULL) {
			printf("ERROR: can't open %s file; exiting...",argv[i+4]);
			exit(1);
		}
	}

    nFrame = (1L << (PHYSICAL_MEMORY_SIZE_BITS - 12));
	printf("Num of Frames %d Physical Memory Size %ld bytes\n",nFrame, (1L<<PHYSICAL_MEMORY_SIZE_BITS));

	if (SIM_TYPE == 0) {
		printf("=============================================================\n");
		printf("The One-Level Page Table with FIFO Memory Simulation Starts .....\n");
		printf("=============================================================\n");
		oneLevelVMSim(0, nFrame, numProcess, procTable);
	}
	
	if (SIM_TYPE == 1) {
		printf("=============================================================\n");
		printf("The One-Level Page Table with LRU Memory Simulation Starts .....\n");
		printf("=============================================================\n");
		oneLevelVMSim(1, nFrame, numProcess, procTable);
	}
	
	if (SIM_TYPE == 2) {
		printf("=============================================================\n");
		printf("The Two-Level Page Table Memory Simulation Starts .....\n");
		printf("=============================================================\n");
		twoLevelVMSim(nFrame, numProcess, procTable, FIRST_LEVEL_BITS);
	}
	
	if (SIM_TYPE == 3) {
		printf("=============================================================\n");
		printf("The Inverted Page Table Memory Simulation Starts .....\n");
		printf("=============================================================\n");
		invertedPageVMSim(nFrame, numProcess, procTable);
	}

	return(0);
}