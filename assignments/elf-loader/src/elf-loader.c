// SPDX-License-Identifier: BSD-3-Clause

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <sys/resource.h>
#include <elf.h>
#include <sys/auxv.h>
#include <time.h>

#define PT_LOAD 1

void *map_elf(const char *filename)
{
	// This part helps you store the content of the ELF file inside the buffer.
	struct stat st;
	void *file;
	int fd;

	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		perror("open");
		exit(1);
	}

	fstat(fd, &st);

	file = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (file == MAP_FAILED) {
		perror("mmap");
		close(fd);
		exit(1);
	}

	return file;
}

void load_and_run(const char *filename, int argc, char **argv, char **envp)
{
	// Contents of the ELF file are in the buffer: elf_contents[x] is the x-th byte of the ELF file.
	void *elf_contents = map_elf(filename);

	/**
	 * TODO: ELF Header Validation
	 * Validate ELF magic bytes - "Not a valid ELF file" + exit code 3 if invalid.
	 * Validate ELF class is 64-bit (ELFCLASS64) - "Not a 64-bit ELF" + exit code 4 if invalid.
	 */

	const char elf_magic[5] = {0x7f, 0x45, 0x4c, 0x46, 0x02};

	for (int i = 0; i < 4; i++) {
		if (((char *)elf_contents)[i] != elf_magic[i]) {
			perror("Not a valid ELF file");
			exit(3);
		}
	}

	if (((char *)elf_contents)[4] != elf_magic[4]) {
		perror("Not a 64-bit ELF");
		exit(4);
	}

	/**
	 * TODO: Load PT_LOAD segments
	 * For minimal syscall-only binaries.
	 * For each PT_LOAD segment:
	 * - Map the segments in memory. Permissions can be RWX for now.
	 */

	Elf64_Ehdr elf_hdr;
	memcpy(&elf_hdr, elf_contents, sizeof(elf_hdr));

	size_t page = 4096;

	/**
	 * TODO: Load Memory Regions with Correct Permissions
	 * For each PT_LOAD segment:
	 *	- Set memory permissions according to program header p_flags (PF_R, PF_W, PF_X).
	 *	- Use mprotect() or map with the correct permissions directly using mmap().
	 */

	u_int64_t load_base = 0;

	// For PIE executables, we need to load them at a random base address.
	if (elf_hdr.e_type == ET_DYN) {
		srand(time(NULL));
		load_base = (0x4000000 + (rand() % 0x10000000)) & ~(page-1);
	}

	for (unsigned int i = 0; i < elf_hdr.e_phnum; i++) {
		Elf64_Phdr ph;
		memcpy(&ph, elf_contents + elf_hdr.e_phoff + i * elf_hdr.e_phentsize, sizeof(ph));

		if (ph.p_type != PT_LOAD) {
			continue;
		}

		// Align the mapping to page boundaries
		u_int64_t aligned_vaddr = (ph.p_vaddr + load_base) & ~(page - 1);
		u_int64_t delta = ph.p_vaddr + load_base - aligned_vaddr;

		void *addr = mmap((void *)aligned_vaddr, ph.p_memsz + delta,
							PROT_READ | PROT_WRITE | PROT_EXEC,
							MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);

		if (addr == MAP_FAILED) {
			perror("mmap");
			exit(1);
		}

		// Copy the segment contents from the ELF file to the mapped memory
		memcpy(addr + delta, elf_contents + ph.p_offset, ph.p_filesz);

		// If p_memsz is larger than p_filesz, zero out the remaining memory (BSS section)
		if (ph.p_memsz > ph.p_filesz) {
			memset(addr + delta + ph.p_filesz, 0, ph.p_memsz - ph.p_filesz);
		}

		unsigned int prot = 0;
		prot = ph.p_flags & PF_R ? prot | PROT_READ : prot;
		prot = ph.p_flags & PF_W ? prot | PROT_WRITE : prot;
		prot = ph.p_flags & PF_X ? prot | PROT_EXEC : prot;

		// Set the correct permissions for the mapped segment
		if (mprotect(addr, ph.p_memsz + delta, prot) < 0) {
			perror("mprotect");
			exit(1);
		}
	}

	/**
	 * TODO: Support Static Non-PIE Binaries with libc
	 * Must set up a valid process stack, including:
	 *	- argc, argv, envp
	 *	- auxv vector (with entries like AT_PHDR, AT_PHENT, AT_PHNUM, etc.)
	 * Note: Beware of the AT_RANDOM, AT_PHDR entries, the application will crash if you do not set them up properly.
	 */
	struct rlimit rl;
	getrlimit(RLIMIT_STACK, &rl);

	char *stack_base = mmap(NULL, rl.rlim_cur, PROT_READ | PROT_WRITE,
							MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	if (stack_base == MAP_FAILED) {
		perror("stack mmap");
		exit(1);
	}

	u_int64_t *stack = (u_int64_t *)(stack_base + rl.rlim_cur);  // top
	stack = (u_int64_t *)((u_int64_t)stack & ~0xF);        // align 16 bytes

	unsigned long auxv[38] = { AT_SYSINFO_EHDR, getauxval(AT_SYSINFO_EHDR),
								AT_HWCAP, getauxval(AT_HWCAP),
								AT_PAGESZ, 4096,
								AT_CLKTCK, getauxval(AT_CLKTCK),
								AT_PHDR, (u_int64_t)elf_contents + elf_hdr.e_phoff,
								AT_PHENT, elf_hdr.e_phentsize,
								AT_PHNUM, elf_hdr.e_phnum,
								AT_BASE, load_base,
								AT_FLAGS, 0,
								AT_ENTRY, elf_hdr.e_entry + load_base,
								AT_UID, getuid(),
								AT_EUID, geteuid(),
								AT_GID, getgid(),
								AT_EGID, getegid(),
								AT_SECURE, 0,
								AT_RANDOM, (u_int64_t)stack - 16,
								AT_EXECFN, (u_int64_t)argv[0],
								AT_PLATFORM, 0,
								AT_NULL, 0
							};

	// Push auxv
	for (int i = 37; i >= 0; i--) {
		stack--;
		*stack = auxv[i];
	}

	int envc = 0;
	while (envp[envc] != NULL) {
		envc++;
	}

	// Push envp, argv, argc
	stack--;
	*stack = 0;  // envp null terminator

	for (int i = envc - 1; i >= 0; i--) {
		stack--;
		*stack = (u_int64_t)envp[i];
	}

	stack--;
	*stack = 0;  // argv null terminator

	for (int i = argc - 1; i >= 0; i--) {
		stack--;
		*stack = (u_int64_t)argv[i];
	}

	stack--;
	*stack = argc;  // argc at top of stack

	void *sp = (void *)stack;

	/**
	 * TODO: Support Static PIE Executables
	 * Map PT_LOAD segments at a random load base.
	 * Adjust virtual addresses of segments and entry point by load_base.
	 * Stack setup (argc, argv, envp, auxv) same as above.
	 */

	// TODO: Set the entry point and the stack pointer
	void (*entry)() = (void (*)())(elf_hdr.e_entry + load_base);

	// Transfer control
	__asm__ __volatile__(
			"mov %0, %%rsp\n"
			"xor %%rbp, %%rbp\n"
			"jmp *%1\n"
			:
			: "r"(sp), "r"(entry)
			: "memory"
			);
}

int main(int argc, char **argv, char **envp)
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <static-elf-binary>\n", argv[0]);
		exit(1);
	}

	load_and_run(argv[1], argc - 1, &argv[1], envp);
	return 0;
}
