#include <stdio.h>
#include <stdlib.h>

#include "tlb_sim.h"

int main(int argc, char* argv[])
{
	int tlb_size, ntlb_size, pwc_size;
	int tlb_way;
	int i;
	int cmd;
	struct SIM_RESULT **results;

	if(argc != 6){
		fprintf(stderr, "Usage: %s tlb_size tlb_way ntlb_size pwc_size cmd_idx={NTLB, PWC_EPT, PWC_NOEPT, FULL}\n", argv[0]);
		return 1;
	}

	tlb_size = atoi(argv[1]);
	tlb_way = atoi(argv[2]);
	ntlb_size = atoi(argv[3]);
	pwc_size = atoi(argv[4]);
	cmd = atoi(argv[5]);

	results = tlbsim_sim(tlb_size, ntlb_size, pwc_size, tlb_way, cmd, "./TRACES");

	fprintf(stdout, "%-10s\t%20s\t%20s\t%20s\t%20s\n", "Cache", "Hit", "Miss", "Hit Ratio", "Mem Access");

	for(i=0;results[i] != NULL;i++){
		fprintf(stdout, "%-10s\t%20llu\t%20llu\t%20.4lf\n", "NTLB", results[i]->ntlb.hit, results[i]->ntlb.miss,
			100.0 * ((double)results[i]->ntlb.hit) / ((double)(results[i]->ntlb.hit + results[i]->ntlb.miss)));

		fprintf(stdout, "%-10s\t%20llu\t%20llu\t%20.4lf\t%20llu\n", "PWC", results[i]->pwc.hit, results[i]->pwc.miss,
			100.0 * ((double)results[i]->pwc.hit) / ((double)(results[i]->pwc.hit + results[i]->pwc.miss)),
			results[i]->accs);

		free(results[i]);
	}

	free(results);

	return 0;
}
