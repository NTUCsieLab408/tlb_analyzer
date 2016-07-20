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

/* Memory Layout:
 *   0x0000_0000 -> 0x01FF_FFFF		Hypervisor (32MB)
 *   0x0200_0000 -> 0x21FF_FFFF		First VM (512MB)
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef USE_QEMU
#include <cpu-all.h>
#include <exec-all.h>
#endif	/* USE_QEMU */

static int tlb_size;
static int tlb_set;
static int tlb_set_step;
static int tlb_set_mask;

struct TLB_ENTRY{
	unsigned int va;		// Virtual Address
	unsigned int pa;		// Physical Address
	unsigned int asid;		// AP-Specific ID
	unsigned int ts;		// LRU
};

struct TLB_COUNTER{
	unsigned long long hit;
	unsigned long long miss;
};

static struct TLB_ENTRY *sl_tlb;
static struct TLB_COUNTER sl_cnt;

static unsigned long long systs;
static int started;

static int fout;
static int fbc;
static uint32_t *fbuf;

static int (*my_pte_helper)(void *arg, uint32_t address, uint32_t *l1, uint32_t *l2, uint32_t *gpa);

#ifdef USE_QEMU
static int get_ptes(void *arg, uint32_t address, uint32_t *l1, uint32_t *l2, uint32_t *gpa);
#endif /* USE_QEMU */

void tlbtrace_stop(void);

static int last_ins_idx = 0;
static int tlbtrace_refmem_sl(unsigned int addr, unsigned int asid, int ins)
{
	int i, mi = 0;
	unsigned int mts = sl_tlb[0].ts;

	if(ins){
		if(sl_tlb[last_ins_idx].va == addr && sl_tlb[last_ins_idx].asid == asid){	// fast path
			sl_tlb[last_ins_idx].ts = ++systs;
			sl_cnt.hit++;
			return 1;
		}
	}

	for(i = ((addr >> 12) & tlb_set_mask) ;i<tlb_size;i+=tlb_set_step){
		if(sl_tlb[i].va == addr && sl_tlb[i].asid == asid){	// hit
			sl_tlb[i].ts = ++systs;
			sl_cnt.hit++;
			if(ins)	last_ins_idx = i;
			return 1;
		}

		if(sl_tlb[i].ts < mts){
			mts = sl_tlb[i].ts;
			mi = i;
		}
	}

	// miss and refill with LRU policy
	sl_tlb[mi].va = addr;
	sl_tlb[mi].asid = asid;
	sl_tlb[mi].ts = ++systs;
	if(ins)	last_ins_idx = mi;
	sl_cnt.miss++;

	if(systs >= 150000000000ULL){
		tlbtrace_stop();
		exit(0);
	}

	return 0;
}

/* Main function of PWC method */
static void tlbtrace_refmem_pwc(unsigned int addr, void *arg)
{
	uint32_t l1_ppa = 0, l2_ppa = 0, gpa = 0;
	int ret;

	// get the PPAs of the PTEs
	ret = my_pte_helper(arg, addr, &l1_ppa, &l2_ppa, &gpa);		// ret: 1 if section or fault, 2 if the walk is completed
	fbuf[fbc++] = addr | ret;
	fbuf[fbc++] = l1_ppa;
	fbuf[fbc++] = l2_ppa;
	fbuf[fbc++] = gpa;

	if(fbc == 1024 * 1024 * 8){
		write(fout, fbuf, sizeof(uint32_t) * 1024 * 1024 * 8);
		fbc = 0;
	}
}


void tlbtrace_refmem(unsigned int addr, unsigned int asid, int ins, void *arg)
{
	addr = addr & 0xFFFFF000;

	if(tlbtrace_refmem_sl(addr, asid, ins) != 0)		return;		// hit in first level TLB

	tlbtrace_refmem_pwc(addr, arg);						// simulate PWC
}

#define FLUSH_TLB(tlb, size)		do{ \
	int _idx_; \
	for(_idx_ = 0; _idx_ < size; _idx_++){\
		tlb[_idx_].va = 0xFFFFFFFF; \
		tlb[_idx_].asid = 0; \
		tlb[_idx_].ts = 0; \
	} \
}while(0)

#define FLUSH_TLB_ENTRY(tlb, size, _va, _asid)		do{ \
	int _idx_; \
	for(_idx_ = 0; _idx_ < size; _idx_++){\
		if((tlb[_idx_].va != (_va)) || (tlb[_idx_].asid != (_asid)))	continue; \
		tlb[_idx_].va = 0xFFFFFFFF; \
		tlb[_idx_].asid = 0; \
		tlb[_idx_].ts = 0; \
	} \
}while(0)

#define FLUSH_TLB_ASID(tlb, size, _asid)		do{ \
	int _idx_; \
	for(_idx_ = 0; _idx_ < size; _idx_++){\
		if(tlb[_idx_].asid != _asid)	continue; \
		tlb[_idx_].va = 0xFFFFFFFF; \
		tlb[_idx_].asid = 0; \
		tlb[_idx_].ts = 0; \
	} \
}while(0)

#define INIT_TLB(tlb, size, cnt)		do{ \
	FLUSH_TLB(tlb, size);	\
	cnt.miss = cnt.hit = 0; \
}while(0)

void tlbtrace_flush_all()
{
	if(!started)	return;
	FLUSH_TLB(sl_tlb, tlb_size);
}

void tlbtrace_flush_entry(unsigned long va)
{
	if(!started)	return;
	FLUSH_TLB_ENTRY(sl_tlb, tlb_size, va & 0xFFFFF000, va & 0xFF);
}

void tlbtrace_flush_asid(unsigned long asid)
{
	if(!started)	return;
	FLUSH_TLB_ASID(sl_tlb, tlb_size, asid);
}


#ifdef USE_QEMU
static int pcnt = 0;
void tlbtrace_refmem_qemu(unsigned int addr, int ins)
{
	CPUState *env = first_cpu;	// global variable provided by QEMU
	unsigned int asid;

	if(!started)	return;
	if((env->uncached_cpsr & CPSR_M) != ARM_CPU_MODE_USR)	return;			// we only trace access in user mode

	asid = env->cp15.c13_context & 0xFF;

	//if(pcnt++ < 100)	fprintf(stderr, "[TLBTRACE] addr=0x%08X, asid=0x%08X\n", addr, asid);
	tlbtrace_refmem(addr, asid, ins, first_cpu);

}

static uint32_t get_level1_table_address(CPUState *env, uint32_t address)
{
    uint32_t table;

    if (address & env->cp15.c2_mask)
        table = env->cp15.c2_base1 & 0xffffc000;
    else
        table = env->cp15.c2_base0 & env->cp15.c2_base_mask;

    table |= (address >> 18) & 0x3ffc;
    return table;
}

static int get_ptes(void *arg, uint32_t address, uint32_t *l1, uint32_t *l2, uint32_t *gpa)
{
	CPUState *env = arg;
	int type;
    uint32_t table;
    uint32_t desc;
    int domain;

    /* Pagetable walk.  */
    /* Lookup l1 descriptor.  */
    *l1 = table = get_level1_table_address(env, address);
    desc = ldl_phys(table);
    type = (desc & 3);

	if(type == 0 || type == 3){					/* Section translation fault.  */
		return 1;
    }else if(type == 2 && (desc & (1 << 18))) {	/* Supersection.  */
        domain = 0;
    }else{										/* Section or page.  */
        domain = (desc >> 4) & 0x1e;
    }

    domain = (env->cp15.c3 >> domain) & 3;
    if(domain == 0 || domain == 2){
		return 1;
    }

	if(type == 2)	return 1;
	
	/* Lookup l2 entry.  */
	*l2 = table = (desc & 0xfffffc00) | ((address >> 10) & 0x3fc);

	desc = ldl_phys(table);
	if((desc & 0x3) == 0)	return 2;

	*gpa = (desc & 0xFFFFF000);

	return 3;
}


static int pccnt = 0;
void REGPARM qemu_trace_pc_helper(unsigned int pc)
{
	if(!started)	return;
	tlbtrace_refmem_qemu(pc, 1);
}

#endif /* USE_QEMU */

void tlbtrace_start(void)
{
	fprintf(stderr, "[TLBTRACE] starting... (%s:%d)\n", __FUNCTION__, __LINE__);
	fprintf(stderr, "[TLBTRACE] CONFIG => %d, %d-WAY\n", tlb_size, tlb_set);

#ifdef USE_QEMU
	tlb_flush(first_cpu, 1);
#endif /* USE_QEMU */

	time_t now = time(NULL);
	struct tm tm;
	char buf[256];
	localtime_r(&now, &tm);
	snprintf(buf, 256, "trace_%02d%02d_%02d%02d_%d.%d", tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tlb_size, tlb_set);
	fout = open(buf, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	fbc = 0;
	fbuf = malloc(1024 * 1024 * 8 * sizeof(uint32_t));

	fprintf(stderr, "[TLBTRACE] FILE=%s\n", buf);

	systs = 0;

	INIT_TLB(sl_tlb, tlb_size, sl_cnt);
}

void tlbtrace_stop(void)
{
#ifdef USE_QEMU
	tlb_flush(first_cpu, 1);
#endif /* USE_QEMU */

	if(fbc > 0){
		write(fout, fbuf, fbc * sizeof(uint32_t));
	}
	free(fbuf);

	close(fout);

	fprintf(stderr, "[TLBTRACE] stopping... (%s:%d)\n", __FUNCTION__, __LINE__);

	fprintf(stderr, "ITEM\t%20s\t%20s\thit ratio\n", "hit", "miss");

	fprintf(stderr, "TLB\t%20llu\t%20llu\t%.5lf\n", sl_cnt.hit, sl_cnt.miss, 
			100.0 * (double)sl_cnt.hit / (double)(sl_cnt.hit + sl_cnt.miss));
}

void tlbtrace_toggle(void)
{
	started = !started;
	if(started)	tlbtrace_start();
	else		tlbtrace_stop();
}

int tlbtrace_enabled(void)
{
	return started;
}

int tlbtrace_init(int size, int set, int (*pte_helper)(void* arg, uint32_t addr, uint32_t *l1, uint32_t *l2, uint32_t *pa))
{
	fprintf(stderr, "[TLBTRACE] initializing... (%s:%d)\n", __FUNCTION__, __LINE__);
	started = 0;

	tlb_size = size;
	tlb_set = set;
	tlb_set_step = size / set;
	tlb_set_mask = tlb_set_step - 1;

	sl_tlb = (struct TLB_ENTRY*)malloc(sizeof(struct TLB_ENTRY) * tlb_size);

#ifdef USE_QEMU
	my_pte_helper = get_ptes;
#else
	my_pte_helper = pte_helper;
#endif /* USE_QEMU */

	return 0;
}

void tlbtrace_destroy(void)
{
	fprintf(stderr, "[TLBTRACE] destroying... (%s:%d)\n", __FUNCTION__, __LINE__);

	started = 0;

	if(sl_tlb != NULL){
		free(sl_tlb);
		sl_tlb = NULL;
	}
}

#ifdef _MY_DEBUG_
int main(int argc, char* argv[])
{

	return 0;
}
#endif	/* _MY_DEBUG_ */

