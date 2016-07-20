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
 * This file contains the API of TLB Tracer.
 * To use the tracing funcationality, first invoke tlbtrace_init() to initialize the tracer.
 * Then start the tracer by invoking tlbtrace_start().
 * For each memory access, invoke tlbtrace_refmem() to add the referenced address to the trace file.
 *
 * @subsection trace_file_format Trace File Format
 * The trace file contains sequences of memory accesses specified in 4-tuples.
 * Each 4-tuple consists of four integers of uint32_t (mva, l1_pa, l2_pa, pa).
 * - \e mva is the modified input address, which is the prefix of the page number bitwise-OR with the traversal result.
 *   The least significant bit will be 1 when the traversal results in a page fault.
 * - \e l1_pa is the address of the first level descriptor of the page table for the input address.
 * - \e l2_pa is the address of the second level descriptor of the page table for the input address.
 * - \e pa is the output address.
 */
#ifndef _TLB_TRACE_H_
#define _TLB_TRACE_H_

/**
 * @brief Initialization function for the tracer.
 *
 * @param size Size of the main TLB.
 * @param set Set associativity of the main TLB.
 * @param pte_helper Callback function for page table traversal.
 * - \a arg is a user-defined object.
 * - \a addr is the input address.
 * - \a l1 is a pointer to the output address of the first level descriptor of the page table.
 * - \a l2 is a pointer to the output address of the second level descriptor of the page table.
 * - \a pa is a pointer to the output address.
 * @return
 * - 0 on success.
 * - Negative integer on failure.
 */
int tlbtrace_init(int size, int set, int (*pte_helper)(void* arg, uint32_t addr, uint32_t *l1, uint32_t *l2, uint32_t *pa));

/**
 * @brief Cleanup function for the tracer.
 */
void tlbtrace_destroy(void);

/**
 * @brief Start the tracer.
 *
 * On each invocation, it first reset all states of the tracer.
 * Then it creates a new trace file with a filename in the format $MM$dd_$hh$mm_$size.$set.
 * - \e $MM is the month of the current date.
 * - \e $dd is the day of the current date.
 * - \e $hh is the hour of the current time.
 * - \e $mm is the minute of the current time.
 * - \e $size is the size of the main TLB passed to tlbtrace_init().
 * - \e $set is the set associativity of the main TLB passed to tlbtrace_init().
 */
void tlbtrace_start(void);

/**
 * @brief Stop the tracer.
 *
 * Flush buffered data and close the trace file.
 */
void tlbtrace_stop(void);

/**
 * @brief Toggle the start/stop state of the tracer.
 */
void tlbtrace_toggle(void);

/**
 * @brief Get the state of the tracer.
 *
 * @return
 * - 0 indicates the tracer is not running
 * - 1 otherwise.
 */
int tlbtrace_enabled(void);

/**
 * @brief Add a memory access to the trace file.
 *
 * It first looks up the main TLB. If the TLB is hit, it returns directly. Otherwise, it initiates a page translation.
 *
 * @param addr Address to be traced.
 * @param asid Address space ID of the referenced address.
 * @param ins Whether it is an instruction fetch or a data load/store operation.
 * - 0 indicates a data load/store operation.
 * - 1 indicates an instruction fetch.
 * @param arg A user-defined object which will be passed to the callback function for page table traversal.
 */
void tlbtrace_refmem(unsigned int addr, unsigned int asid, int ins, void *arg);

#ifdef USE_QEMU
/**
 * @brief Helper function for tracing instruction fetches in QEMU.
 *
 * \param pc Current program counter.
 */
void REGPARM qemu_trace_pc_helper(unsigned int pc);
#endif /* USE_QEMU */

#endif /* _TLB_TRACE_H_ */
