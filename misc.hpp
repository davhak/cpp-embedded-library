/*
 * Copyright 2023 Davit Hakobyan
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MISC_HPP_INCLUDED
#define MISC_HPP_INCLUDED


#ifdef __cplusplus
extern "C" {
#endif

// ===================================================================
// Macros, defines
// ===================================================================

// #define ARM_CROSS_COMPILER 

#if defined( ARM_CROSS_COMPILER )

#if defined (__GNUC__)
#define ENABLE_INTERRUPTS() __asm volatile ("cpsie i" : : : "memory");
#else
#define ENABLE_INTERRUPTS() __asm("cpsie i")
#endif


#if defined (__GNUC__)
#define DISABLE_INTERRUPTS() __asm volatile ("cpsid i" : : : "memory");
#else
#define DISABLE_INTERRUPTS() __asm("cpsid i")
#endif

#define SOFTWARE_BREAKPOINT() __asm("BKPT #0\n\t")

#else

// Define ENABLE_INTERRUPTS, DISABLE_INTERRUPTS and SOFTWARE_BREAKPOINT
// for your platforms

#define ENABLE_INTERRUPTS()

#define DISABLE_INTERRUPTS()

#define SOFTWARE_BREAKPOINT()

#endif // ARM_CROSS_COMPILER

#define ASSERT(expr)          if (!(expr)) failure1()

#define STATIC_STR_LEN(x)	  (sizeof(x) - 1)

#define STATIC_ELEM_COUNT(x)  (sizeof(x) / sizeof(x[0]))

// ===================================================================
// Function prototypes
// ===================================================================

static inline void failure1()
{
	DISABLE_INTERRUPTS();

	SOFTWARE_BREAKPOINT();

	for(;;);
}

#ifdef __cplusplus
}
#endif

#endif // MISC_HPP_INCLUDED
