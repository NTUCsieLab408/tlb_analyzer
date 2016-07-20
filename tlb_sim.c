/**
 * This file is part of TLB Analyzer
 *
 * TLB Analyzer is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * TLB Analyzer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with TLB Analyzer.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <dirent.h>

#include "tlb_sim.h"

#define CACHE_MAX_ENTRIES	512
#define MAX_TRACE_FILES		128

struct TLB_ENTRY{
	unsigned int va;		// Virtual Address
	unsigned int pa;		// Physical Address
	unsigned int asid;		// AP-Specific ID
	unsigned int ts;		// LRU
};

unsigned long long systs;
int TLB_WAYMASK_NTLB, TLB_MAX_ENTRIES_NTLB, TLB_WAYSTEP_NTLB;
int TLB_WAYMASK_PWC, TLB_MAX_ENTRIES_PWC, TLB_WAYSTEP_PWC;

struct TLB_ENTRY ntlb_tlb[CACHE_MAX_ENTRIES];	// nested tlb
struct TLB_ENTRY ntlb2_tlb[CACHE_MAX_ENTRIES];	// nested tlb
struct TLB_ENTRY pwc_tlb[CACHE_MAX_ENTRIES];		// page walk cache
struct TLB_ENTRY pwc2_tlb[CACHE_MAX_ENTRIES];		// page walk cache
struct TLB_ENTRY pwc3_tlb[CACHE_MAX_ENTRIES];		// page walk cache

#define FLUSH_TLB(tlb, size)		do{ \
	int _idx_; \
	for(_idx_ = 0; _idx_ < size; _idx_++){\
		tlb[_idx_].va = 0xFFFFFFFF; \
		tlb[_idx_].asid = 0; \
		tlb[_idx_].ts = 0; \
	} \
}while(0)

#define INIT_TLB(tlb, size, cnt)		do{ \
	FLUSH_TLB(tlb, size);	\
	cnt.miss = cnt.hit = 0; \
}while(0)

struct TLB_COUNTER ntlb_cnt;
struct TLB_COUNTER ntlb2_cnt;
struct TLB_COUNTER pwc_cnt;
struct TLB_COUNTER pwc2_cnt;
struct TLB_COUNTER pwc3_cnt;

unsigned long long ntlb2_mem_accs;
unsigned long long pwc2_mem_accs;
unsigned long long pwc3_mem_accs;
unsigned long long full_mem_accs;

static FILE *fin;
static char trace_files[MAX_TRACE_FILES][512];
static int trace_count;

static void flush_all()
{
	systs = 0;
	full_mem_accs = pwc3_mem_accs = pwc2_mem_accs = ntlb2_mem_accs = 0;

	INIT_TLB(ntlb_tlb, CACHE_MAX_ENTRIES, ntlb_cnt);
	INIT_TLB(ntlb2_tlb, CACHE_MAX_ENTRIES, ntlb2_cnt);
	INIT_TLB(pwc_tlb, CACHE_MAX_ENTRIES, pwc_cnt);
	INIT_TLB(pwc2_tlb, CACHE_MAX_ENTRIES, pwc2_cnt);
	INIT_TLB(pwc3_tlb, CACHE_MAX_ENTRIES, pwc3_cnt);
}

static int tlbtrace_ntlb_find2(unsigned int addr, int final)
{
	int i, mi = 0;
	unsigned int mts = ntlb2_tlb[0].ts;

	if(!final)	ntlb2_mem_accs++;	// access EPT desc content

	for(i = ((addr >> 12) & TLB_WAYMASK_NTLB) ;i<TLB_MAX_ENTRIES_NTLB;i+=TLB_WAYSTEP_NTLB){
		if(ntlb2_tlb[i].va == addr){			// hit
			ntlb2_tlb[i].ts = ++systs;
			ntlb2_cnt.hit++;
			return 1;
		}

		if(ntlb2_tlb[i].ts < mts){
			mts = ntlb2_tlb[i].ts;
			mi = i;
		}
	}

	ntlb2_mem_accs+=2;	// access EPTL2 + EPTL1

	ntlb2_tlb[mi].va = addr;
	ntlb2_tlb[mi].ts = ++systs;
	ntlb2_cnt.miss++;
	
	return 0;
}

static void emulate_ntlb2(int level, uint32_t l1_gpa, uint32_t l2_gpa, uint32_t gpa)
{
	tlbtrace_ntlb_find2(l1_gpa & 0xFFFFF000, 0);	// mem_accs = hit * 1 + miss * 3

	if(level > 1){
		tlbtrace_ntlb_find2(l2_gpa & 0xFFFFF000, 0);
	}

	if(level > 2){
		tlbtrace_ntlb_find2(gpa, 1);	// mem_accs = hit * 0 + miss * 2
	}
}

void tlbsim_sim_ntlb(const char* trace_name, int ntlb_size, int ntlb_way,
		int pwc_size __attribute__((unused)), int pwc_way __attribute((unused)), struct SIM_RESULT *result)
{
	uint32_t t[4];

	TLB_MAX_ENTRIES_NTLB = ntlb_size;
	TLB_WAYSTEP_NTLB = ntlb_size / ntlb_way;
	TLB_WAYMASK_NTLB = TLB_WAYSTEP_NTLB - 1;

	fin = fopen(trace_name, "r");

	flush_all();
	while(fread(t, sizeof(uint32_t), 4, fin) == 4){
		emulate_ntlb2(t[0] & 0xF, t[1], t[2], t[3]);
	}

	result->accs = ntlb2_mem_accs;
	result->ntlb = ntlb2_cnt;

	fclose(fin);
}

static int tlbtrace_refppa_pwc2(unsigned int addr, unsigned int asid)
{
	int i, mi = 0;
	unsigned int mts = pwc2_tlb[0].ts;

	for(i = ((addr >> 2) & TLB_WAYMASK_PWC) ;i<TLB_MAX_ENTRIES_PWC;i+=TLB_WAYSTEP_PWC){
		if(pwc2_tlb[i].va == addr && pwc2_tlb[i].asid == asid){			// hit
			pwc2_tlb[i].ts = ++systs;
			pwc2_cnt.hit++;
			return 1;
		}

		if(pwc2_tlb[i].ts < mts){
			mts = pwc2_tlb[i].ts;
			mi = i;
		}
	}

	pwc2_tlb[mi].va = addr;
	pwc2_tlb[mi].ts = ++systs;
	pwc2_tlb[mi].asid = asid;
	pwc2_cnt.miss++;

	return 0;
}

static void emulate_pwc2(int level, uint32_t l1_gpa, uint32_t l2_gpa, uint32_t gpa)
{
	uint32_t l1_mpa, l2_mpa;

	if(tlbtrace_refppa_pwc2(l1_gpa, 1) == 0){		// hit * 0 + miss * 3
		l1_mpa = (l1_gpa >> 20) * 4;
		l2_mpa = 16 * 1024 + (l1_gpa >> 20) * 1024 + ((l1_gpa >> 12) & 0xFF) * 4;
		if(tlbtrace_refppa_pwc2(l1_mpa, 0) == 0)	pwc2_mem_accs++;
		if(tlbtrace_refppa_pwc2(l2_mpa, 0) == 0)	pwc2_mem_accs++;
		pwc2_mem_accs++;
	}

	if(level > 1){
		if(tlbtrace_refppa_pwc2(l2_gpa, 1) == 0){		// hit * 0 + miss * 3
			l1_mpa = (l2_gpa >> 20) * 4;
			l2_mpa = 16 * 1024 + (l2_gpa >> 20) * 1024 + ((l2_gpa >> 12) & 0xFF) * 4;
			if(tlbtrace_refppa_pwc2(l1_mpa, 0) == 0)	pwc2_mem_accs++;
			if(tlbtrace_refppa_pwc2(l2_mpa, 0) == 0)	pwc2_mem_accs++;
			pwc2_mem_accs++;
		}
	}

	if(level > 2){
		l1_mpa = (gpa >> 20) * 4;
		l2_mpa = 16 * 1024 + (gpa >> 20) * 1024 + ((gpa >> 12) & 0xFF) * 4;
		if(tlbtrace_refppa_pwc2(l1_mpa, 0) == 0)	pwc2_mem_accs++;
		if(tlbtrace_refppa_pwc2(l2_mpa, 0) == 0)	pwc2_mem_accs++;
	}
}

void tlbsim_sim_pwc_ept(const char* trace_name, int ntlb_size __attribute__((unused)), int ntlb_way __attribute__((unused)),
		int pwc_size, int pwc_way, struct SIM_RESULT *result)
{
	uint32_t t[4];

	TLB_MAX_ENTRIES_PWC = pwc_size;
	TLB_WAYSTEP_PWC = pwc_size / pwc_way;
	TLB_WAYMASK_PWC = TLB_WAYSTEP_PWC - 1;

	fin = fopen(trace_name, "r");

	flush_all();
	while(fread(t, sizeof(uint32_t), 4, fin) == 4){
		emulate_pwc2(t[0] & 0xF, t[1], t[2], t[3]);
	}

	result->accs = pwc2_mem_accs;
	result->pwc = pwc2_cnt;

	fclose(fin);
}

static int tlbtrace_refppa_pwc3(unsigned int addr, unsigned int asid)
{
	int i, mi = 0;
	unsigned int mts = pwc3_tlb[0].ts;

	for(i = ((addr >> 2) & TLB_WAYMASK_PWC) ;i<TLB_MAX_ENTRIES_PWC;i+=TLB_WAYSTEP_PWC){
		if(pwc3_tlb[i].va == addr && pwc3_tlb[i].asid == asid){			// hit
			pwc3_tlb[i].ts = ++systs;
			pwc3_cnt.hit++;
			return 1;
		}

		if(pwc3_tlb[i].ts < mts){
			mts = pwc3_tlb[i].ts;
			mi = i;
		}
	}

	pwc3_tlb[mi].va = addr;
	pwc3_tlb[mi].ts = ++systs;
	pwc3_tlb[mi].asid = asid;
	pwc3_cnt.miss++;

	return 0;
}

static void emulate_pwc3(int level, uint32_t l1_gpa, uint32_t l2_gpa, uint32_t gpa __attribute__((__unused__)))
{
	if(tlbtrace_refppa_pwc3(l1_gpa, 1) == 0){		// hit * 0 + miss * 3
		pwc3_mem_accs++;
	}

	if(level > 1){
		if(tlbtrace_refppa_pwc3(l2_gpa, 1) == 0){		// hit * 0 + miss * 3
			pwc3_mem_accs++;
		}
	}
}

void tlbsim_sim_pwc_noept(const char* trace_name, int ntlb_size __attribute__((unused)), int ntlb_way __attribute__((unused)),
		int pwc_size, int pwc_way, struct SIM_RESULT *result)
{
	uint32_t t[4];

	TLB_MAX_ENTRIES_PWC = pwc_size;
	TLB_WAYSTEP_PWC = pwc_size / pwc_way;
	TLB_WAYMASK_PWC = TLB_WAYSTEP_PWC - 1;

	fin = fopen(trace_name, "r");

	flush_all();
	while(fread(t, sizeof(uint32_t), 4, fin) == 4){
		emulate_pwc3(t[0] & 0xF, t[1], t[2], t[3]);
	}

	result->accs = pwc3_mem_accs;
	result->pwc = pwc3_cnt;

	fclose(fin);
}

static int tlbtrace_ntlb_find(unsigned int addr)
{
	int i, mi = 0;
	unsigned int mts = ntlb_tlb[0].ts;

	for(i = ((addr >> 12) & TLB_WAYMASK_NTLB) ;i<TLB_MAX_ENTRIES_NTLB;i+=TLB_WAYSTEP_NTLB){
		if(ntlb_tlb[i].va == addr){			// hit
			ntlb_tlb[i].ts = ++systs;
			ntlb_cnt.hit++;
			return 1;
		}

		if(ntlb_tlb[i].ts < mts){
			mts = ntlb_tlb[i].ts;
			mi = i;
		}
	}

	ntlb_tlb[mi].va = addr;
	ntlb_tlb[mi].ts = ++systs;
	ntlb_cnt.miss++;
	
	return 0;
}


static int tlbtrace_refppa_pwc(unsigned int addr, unsigned int asid)
{
	int i, mi = 0;
	unsigned int mts = pwc_tlb[0].ts;

	for(i = ((addr >> 2) & TLB_WAYMASK_PWC) ;i<TLB_MAX_ENTRIES_PWC;i+=TLB_WAYSTEP_PWC){
		if(pwc_tlb[i].va == addr && pwc_tlb[i].asid == asid){			// hit
			pwc_tlb[i].ts = ++systs;
			pwc_cnt.hit++;
			return 1;
		}

		if(pwc_tlb[i].ts < mts){
			mts = pwc_tlb[i].ts;
			mi = i;
		}
	}

	pwc_tlb[mi].va = addr;
	pwc_tlb[mi].ts = ++systs;
	pwc_tlb[mi].asid = asid;
	pwc_cnt.miss++;

	//if(asid == 0)	real_miss++;
	return 0;
}

static void emulate_full(int level, uint32_t l1_gpa, uint32_t l2_gpa, uint32_t gpa)
{
	uint32_t l1_mpa, l2_mpa;

	if(tlbtrace_refppa_pwc(l1_gpa, 1) == 0){	// PTL1 desc
		if(tlbtrace_ntlb_find(l1_gpa & 0xFFFFF000) == 0){	// miss => refppa
			l1_mpa = (l1_gpa >> 20) * 4;		// base = 0
			l2_mpa = 16 * 1024 + (l1_gpa >> 20) * 1024 + ((l1_gpa >> 12) & 0xFF) * 4;
			if(tlbtrace_refppa_pwc(l1_mpa, 0) == 0)	full_mem_accs++;		// EPTL1 desc
			if(tlbtrace_refppa_pwc(l2_mpa, 0) == 0)	full_mem_accs++;		// EPTL2 desc
		}
		full_mem_accs++;
	}

	if(level > 1){
		if(tlbtrace_refppa_pwc(l2_gpa, 1) == 0){	// PTL2 desc
			if(tlbtrace_ntlb_find(l2_gpa & 0xFFFFF000) == 0){
				l1_mpa = (l2_gpa >> 20) * 4;		// base = 0
				l2_mpa = 16 * 1024 + (l2_gpa >> 20) * 1024 + ((l2_gpa >> 12) & 0xFF) * 4;
				if(tlbtrace_refppa_pwc(l1_mpa, 0) == 0)	full_mem_accs++;	// EPTL1 desc
				if(tlbtrace_refppa_pwc(l2_mpa, 0) == 0) full_mem_accs++;	// EPTL2 desc
			}
			full_mem_accs++;
		}
	}

	if(level > 2){
		if(tlbtrace_ntlb_find(gpa) == 0){
			l1_mpa = (gpa >> 20) * 4;		// base = 0
			l2_mpa = 16 * 1024 + (gpa >> 20) * 1024 + ((gpa >> 12) & 0xFF) * 4;
			if(tlbtrace_refppa_pwc(l1_mpa, 0) == 0)	full_mem_accs++;	// EPTL1 desc
			if(tlbtrace_refppa_pwc(l2_mpa, 0) == 0)	full_mem_accs++;	// EPTL2 desc
		}
	}
}


void tlbsim_sim_ntlb_pwc(const char* trace_name, int ntlb_size, int ntlb_way,
		int pwc_size, int pwc_way, struct SIM_RESULT *result)
{
	uint32_t t[4];

	TLB_MAX_ENTRIES_NTLB = ntlb_size;
	TLB_WAYSTEP_NTLB = ntlb_size / ntlb_way;
	TLB_WAYMASK_NTLB = TLB_WAYSTEP_NTLB - 1;

	TLB_MAX_ENTRIES_PWC = pwc_size;
	TLB_WAYSTEP_PWC = pwc_size / pwc_way;
	TLB_WAYMASK_PWC = TLB_WAYSTEP_PWC - 1;

	fin = fopen(trace_name, "r");

	flush_all();
	while(fread(t, sizeof(uint32_t), 4, fin) == 4){
		emulate_full(t[0] & 0xF, t[1], t[2], t[3]);
	}

	result->accs = full_mem_accs;
	result->ntlb = ntlb_cnt;
	result->pwc = pwc_cnt;

	fclose(fin);
}

static int comparator(const void* p1, const void* p2)
{
	return strcmp((const char *) p1, (const char *) p2);
}

static void list_trace_files(const char *path, int tlb_size, int tlb_way)
{
	DIR* d;
	struct dirent *dent;
	char suffix[32];
	char buf[512];

	trace_count = 0;

	if((d = opendir(path)) == NULL){
		fprintf(stderr, "can't open dir %s\n", path);
		return;
	}

	snprintf(suffix, 32, "_%d.%d", tlb_size, tlb_way);

	while((dent = readdir(d)) != NULL){
		if(strstr(dent->d_name, suffix) == NULL)	continue;
		if(memcmp(dent->d_name, "trace_", 6) != 0)	continue;
		snprintf(buf, 512, "%s/%s", path, dent->d_name);
		strcpy(trace_files[trace_count++], buf);

		if(trace_count == 128)	break;
	}

	closedir(d);

	qsort(trace_files, trace_count, sizeof(trace_files[0]), comparator);
}

struct SIM_RESULT** tlbsim_sim(int tlb_size, int ntlb_size, int pwc_size, int way, enum SIM_CMD cmd, const char *path)
{
	static void (*funcs[4])(const char*, int, int, int, int, struct SIM_RESULT *result) = {
		tlbsim_sim_ntlb,
		tlbsim_sim_pwc_ept,
		tlbsim_sim_pwc_noept,
		tlbsim_sim_ntlb_pwc};

	struct SIM_RESULT **results;
	int i;

	list_trace_files(path, tlb_size, way);

	if((results = (struct SIM_RESULT**)malloc(sizeof(struct SIM_RESULT*) * (trace_count + 1))) == NULL){
		fprintf(stderr, "[tlbsim] out of memory.\n");
		return NULL;
	}

	memset(results, 0, sizeof(struct SIM_RESULT*) * (trace_count + 1));

	for(i=0;i<trace_count;i++){
		results[i] = (struct SIM_RESULT*)malloc(sizeof(struct SIM_RESULT));
		memset(results[i], 0, sizeof(struct SIM_RESULT));
		funcs[cmd](trace_files[i], ntlb_size, way, pwc_size, way, results[i]);
	}

	return results;
}
