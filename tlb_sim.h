/**
 * @file
 * @author Yuan-Cheng Lee <d00944007@csie.ntu.edu.tw>
 * @version 1.0
 *
 * @section LICENSES
 *
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
 *
 * @section DESCRIPTION
 *
 * This file contains the API of TLB Simulator.
 * For a quick start, invoke tlbsim_sim() to run simulation with all traces in a specific folder.
 * For advanced usages, please refer to functions tlbsim_ntlb(), tlbsim_sim_pwc_ept(), tlbsim_sim_pwc_noept(), and tlbsim_sim_ntlb_pwc().
 */
#ifndef _TLB_SIM_H_
#define _TLB_SIM_H_

/**
 * Cache statistics.
 */
struct TLB_COUNTER{
	unsigned long long hit;		/**< Number of cache hits. */ 
	unsigned long long miss;	/**< Number of cache misses. */
};

/**
 * Simulation Result.
 */
struct SIM_RESULT{
	unsigned long long accs;	/**< Total number of memory accesses. */
	struct TLB_COUNTER ntlb;	/**< Statistics of NTLB. */
	struct TLB_COUNTER pwc;		/**< Statistics of PWC. */
};

/**
 * Types of simulation.
 */
enum SIM_CMD {
	SC_NTLB = 0,		/**< Simulate NTLB only. */
	SC_PWC_EPT,			/**< Simulate PWC with extended page table. */
	SC_PWC_NOEPT,		/**< Simulate PWC without extended page table. */
	SC_NTLB_PWC			/**< Simulate both NTLB and PWC. */
};

/**
 * @brief Run a simulation with NTLB.
 *
 * It run a simulation with a NTLB of \a ntlb_size entries and \a ntlb_way way sets.
 * When there is a cache miss in NTLB, it initiates a page translation based on a default extended page table.
 * The result, including the total number of memory accesses and cache statistics,  is put in \a result.
 *
 * @param trace_name Path of the trace file.
 * @param ntlb_size Size of NTLB for simulation.
 * @param ntlb_way Set associativity of NTLB for simulation.
 * @param pwc_size Unused.
 * @param pwc_way Unused.
 * @param result pointer to a user-allocated object for the result. 
 */
void tlbsim_sim_ntlb(const char* trace_name, int ntlb_size, int ntlb_way,
		int pwc_size, int pwc_way, struct SIM_RESULT *result);

/**
 * @brief Run a simulation with PWC and extended page table.
 *
 * It run a simulation with a PWC of \a pwc_size entries and \a pwc_way way sets.
 * When there is a cache miss in PWC, it initiates a page translation based on a default extended page table.
 * The result, including the total number of memory accesses and cache statistics,  is put in \a result.
 *
 * @param trace_name Path of the trace file.
 * @param ntlb_size Unused.
 * @param ntlb_way Unused.
 * @param pwc_size Size of PWC for simulation.
 * @param pwc_way Set associativity of PWC for simulation.
 * @param result pointer to a user-allocated object for the result. 
 */
void tlbsim_sim_pwc_ept(const char* trace_name, int ntlb_size, int ntlb_way,
		int pwc_size, int pwc_way, struct SIM_RESULT *result);

/**
 * @brief Run a simulation with PWC.
 *
 * It run a simulation with a PWC of \a pwc_size entries and \a pwc_way way sets.
 * When there is a cache miss in PWC, it assumes the translation can be retrieved from an additional cache,
 * such as bTLB, without any causing memory accesses.
 * The result, including the total number of memory accesses and cache statistics,  is put in \a result.
 *
 * @param trace_name Path of the trace file.
 * @param ntlb_size Unused.
 * @param ntlb_way Unused.
 * @param pwc_size Size of PWC for simulation.
 * @param pwc_way Set associativity of PWC for simulation.
 * @param result pointer to a user-allocated object for the result. 
 */
void tlbsim_sim_pwc_noept(const char* trace_name, int ntlb_size, int ntlb_way,
		int pwc_size, int pwc_way, struct SIM_RESULT *result);

/**
 * @brief Run a simulation with both NTLB and PWC.
 *
 * It run a simulation with a NTLB and a PWC of \a ntlb_size entries and \a ntlb_way way sets,
 * and \a pwc_size entries and \a pwc_way way sets, respectively.
 * When there is a cache miss in NTLB, it initiates a page translation based on a default extended page table.
 * The result, including the total number of memory accesses and cache statistics,  is put in \a result.
 *
 * @param trace_name Path of the trace file.
 * @param ntlb_size Size of NTLB for simulation.
 * @param ntlb_way Set associativity of NTLB for simulation.
 * @param pwc_size Size of PWC for simulation.
 * @param pwc_way Set associativity of PWC for simulation.
 * @param result pointer to a user-allocated object for the result. 
 */
void tlbsim_sim_ntlb_pwc(const char* trace_name, int ntlb_size, int ntlb_way,
		int pwc_size, int pwc_way, struct SIM_RESULT *result);

/**
 * @brief Run simulation with all traces in a specific folder with the specified type of simulation.
 *
 * It will scan the folder specified by \a path, and run simulation with each trace file one by one.
 * All caches in the simulation are assumed to have the same set associativity.
 * The results are returned as a list ordered by filename and terminated by a NULL element.
 *
 * @param tlb_size Size of the main TLB. It is used to filter trace files.
 * @param ntlb_size Size of NTLB for simulation.
 * @param pwc_size Size of PWC for simulation.
 * @param way Set associativity of caches.
 * @param cmd Type of Simulation.
 * @param path Path to the folder containing trace files for simulation.
 */
struct SIM_RESULT** tlbsim_sim(int tlb_size, int ntlb_size, int pwc_size, int way, enum SIM_CMD cmd, const char *path);
#endif /* _TLB_SIM_H_ */
