/* ** por compatibilidad se omiten tildes **
================================================================================
 TRABAJO PRACTICO 3 - System Programming - ORGANIZACION DE COMPUTADOR II - FCEN
================================================================================
  definicion de funciones del manejador de memoria
*/

#include "defines.h"
#include "mmu.h"
#include "i386.h"
#include "error.h"

/**
 * Creates a page table at the specified page directory with some given attributes.
 *
 * @param directoryBase page directory address, 4K aligned
 * @param directoryEntry table index within the page directory
 * @param physicalAddress in-memory location of the page table, 4K aligned
 * @return E_ADDRESS_NOT_ALIGNED, E_OUT_OF_BOUNDS, E_PAGE_TABLE_PRESENT, E_OK
 */
int create_page_table(
	uint directoryBase,
	uint directoryEntry,
	uint physicalAddress,
	uchar readWrite,
	uchar supervisorUser) {

	if (directoryBase != ALIGN(directoryBase)) {
		return E_ADDRESS_NOT_ALIGNED;
	}

	if (directoryEntry >= 1024) {
		return E_OUT_OF_BOUNDS;
	}

	page_entry *pageDirectory = (page_entry *)directoryBase;

	if (pageDirectory[directoryEntry].p == 1) {
		return E_PAGE_TABLE_PRESENT;
	}

	pageDirectory[directoryEntry] = (page_entry) {
			(uchar) 0x1,
			(uchar) readWrite,
			(uchar) supervisorUser,
			(uchar) 0x0,
			(uchar) 0x0,
			(uchar) 0x0,
			(uchar) 0x0,
			(uchar) 0x0,
			(uchar) 0x0,
			(uchar) 0x0,
			(uint) physicalAddress >> 12
		};

	// Clean the processor's cache
	tlbflush();

	return E_OK;
}

/**
 * Deletes any given page table.
 *
 * @param directoryBase page directory address, 4K aligned
 * @param directoryEntry table index within the page directory
 * @return E_ADDRESS_NOT_ALIGNED, E_OUT_OF_BOUNDS, E_PAGE_TABLE_MISSING, E_OK
 */
int delete_page_table(
	uint directoryBase,
	uint directoryEntry) {

	if (directoryBase != ALIGN(directoryBase)) {
		return E_ADDRESS_NOT_ALIGNED;
	}

	if (directoryEntry >= 1024) {
		return E_OUT_OF_BOUNDS;
	}

	page_entry *pageDirectory = (page_entry *)directoryBase;

	if (pageDirectory[directoryEntry].p == 0) {
		return E_PAGE_TABLE_MISSING;
	}

	uint physicalAddress = pageDirectory[directoryEntry].offset << 12;
	pageDirectory[directoryEntry].p = 0;

	page_entry *pageTable = (page_entry *)physicalAddress;

	int i;

	for (i = 0; i < 1024; ++i) {
		pageTable[i].p = 0;
	}

	// Clean the processor's cache
	tlbflush();

	return E_OK;
}

/**
 * Creates a page at a given page directory and table, with some specified
 * attributes.
 *
 * @param directoryBase page directory address, 4K aligned
 * @param directoryEntry table index within the page directory
 * @param tableEntry page index within the page table
 * @param physicalAddress in-memory location of the page table, 4K aligned
 * @param readWrite whether the page should have write permission
 * @param supervisorUser whether the page protection level should be supervisor
 * @return E_ADDRESS_NOT_ALIGNED, E_OUT_OF_BOUNDS, E_PAGE_TABLE_MISSING,
 * 	       E_PAGE_PRESENT, E_OK
 */
int create_page(
	uint directoryBase,
	uint directoryEntry,
	uint tableEntry,
	uint physicalAddress,
	uchar readWrite,
	uchar supervisorUser) {

	if (directoryBase != ALIGN(directoryBase)) {
		return E_ADDRESS_NOT_ALIGNED;
	}

	if (directoryEntry >= 1024 || tableEntry >= 1024) {
		return E_OUT_OF_BOUNDS;
	}

	page_entry *pageDirectory = (page_entry *)directoryBase;

	if (pageDirectory[directoryEntry].p == 0) {
		return E_PAGE_TABLE_MISSING;
	}

	uint pageTableAddress = pageDirectory[directoryEntry].offset << 12;
	page_entry *pageTable = (page_entry *)pageTableAddress;

	if (pageTable[tableEntry].p == 1) {
		return E_PAGE_PRESENT;
	}

	pageTable[tableEntry] = (page_entry) {
			/**
			Aca guardamos solo los primeros 12 bits del address fisico de la tabla de paginas.
			Los siguientes 12 bits son obtenidos del address lineal que obtengamos en el acceso
			a memoria, y eso nos da el descriptor de pagina, que termina de determinarnos la
			direccion fisica.
			 */
			(uchar) 0x1,
			(uchar) readWrite,
			(uchar) supervisorUser,
			(uchar) 0x0,
			(uchar) 0x0,
			(uchar) 0x0,
			(uchar) 0x0,
			(uchar) 0x0,
			(uchar) 0x0,
			(uchar) 0x0,
			(uint) physicalAddress >> 12
		};

	// Clean the processor's cache
	tlbflush();

	return E_OK;
}


/**
 * Deletes the specified page
 *
 * @param directoryBase page directory address, 4K aligned
 * @param directoryEntry table index within the page directory
 * @param tableEntry page index within the page table
 * @return E_ADDRESS_NOT_ALIGNED, E_OUT_OF_BOUNDS, E_PAGE_TABLE_MISSING,
 *         E_PAGE_MISSING, E_OK
 */
int delete_page(
	uint directoryBase,
	uint directoryEntry,
	uint tableEntry) {

	if (directoryBase != ALIGN(directoryBase)) {
		return E_ADDRESS_NOT_ALIGNED;
	}

	if (directoryEntry >= 1024 || tableEntry >= 1024) {
		return E_OUT_OF_BOUNDS;
	}

	page_entry *pageDirectory = (page_entry *)directoryBase;

	if (pageDirectory[directoryEntry].p == 0) {
		return E_PAGE_TABLE_MISSING;
	}

	uint pageTableAddress = pageDirectory[directoryEntry].offset << 12;
	page_entry *pageTable = (page_entry *)pageTableAddress;

	if (pageTable[tableEntry].p == 0) {
		return E_PAGE_MISSING;
	}

	pageTable[tableEntry].p = 0;

	// Clean the processor's cache
	tlbflush();

	return E_OK;
}

/**
 * Maps a virtual address into a physical address given a base address for the
 * page directory offset. Should the page table for this address not exist, we
 * create it.
 *
 * As of the current implementation, whenever we create a new page table, we
 * just place it in linear order beginning from the address MAPA_BASE_TABLA
 * begins. Whenever we map a new page table, we just put it next to the
 * last defined page table.
 */
uint pageTableLastAddress = DIRECTORY_TABLE_PHYS - PAGE_TABLE_SIZE;

/**
 * Maps a virtual address to a physical address within some table directory.
 *
 * @param virtualAddress address to assign the physical address to
 * @param physicalAddress in-memory location of the page table, 4K aligned
 * @param directoryBase page directory address, 4K aligned
 * @param readWrite whether the page should have write permission
 * @param supervisorUser whether the page protection level should be supervisor
 * @return E_ADDRESS_NOT_ALIGNED, E_INVALID_ADDRESS, E_OK
 */
int mmap(
	uint virtualAddress,
	uint physicalAddress,
	uint directoryBase,
	uchar readWrite,
	uchar supervisorUser) {
	if (directoryBase != ALIGN(directoryBase)) {
		return E_ADDRESS_NOT_ALIGNED;
	}

	// Obtenemos las partes relevantes del address virtual
	uint directoryEntry = virtualAddress >> 22;
	uint tableEntry = (virtualAddress >> 12) & 0x3FF;

	// Alineamos el address fisico al tamano de pagina
	physicalAddress = ALIGN(physicalAddress);

	// Creamos (o no, si ya existe) la page table que vamos a necesitar
	int output = create_page_table(
		directoryBase,
		directoryEntry,
		pageTableLastAddress,
		readWrite,
		supervisorUser);

	if (output == E_OK) {
		pageTableLastAddress -= PAGE_TABLE_SIZE;
	}

	// Creamos la pagina que mapea como queremos
	output = create_page(
		directoryBase,
		directoryEntry,
		tableEntry,
		physicalAddress,
		readWrite,
		supervisorUser);

	if (output != E_OK) {
		return E_INVALID_ADDRESS;
	} else {
		return E_OK;
	}
}

/**
 * Unmaps the page corresponding to a virtual address
 *
 * @param directoryBase page directory address, 4K aligned
 * @param virtualAddress address to remove from pagination
 * @return E_INVALID_ADDRESS, E_OK
 */
int munmap(
	uint directoryBase,
	uint virtualAddress) {

	if (directoryBase != ALIGN(directoryBase)) {
		return E_ADDRESS_NOT_ALIGNED;
	}

	uint directoryEntry = virtualAddress >> 22;
	uint tableEntry = (virtualAddress >> 12) & 0x3FF;

	int output = delete_page(directoryBase, directoryEntry, tableEntry);

	if (output != E_OK) {
		return E_INVALID_ADDRESS;
	} else {
		return E_OK;
	}
}

/**
 * Changes the map from virtual address to a specified physical address within
 * some table directory.
 *
 * @param directoryBase page directory address, 4K aligned
 * @param virtualAddress address to assign the physical address to
 * @param physicalAddress in-memory location of the page table, 4K aligned
 * @return E_ADDRESS_NOT_ALIGNED, E_PAGE_TABLE_MISSING, E_PAGE_MISSING, E_OK
 */
int remap(uint directoryBase, uint virtualAddress, uint physicalAddress) {
	if (directoryBase != ALIGN(directoryBase)) {
		return E_ADDRESS_NOT_ALIGNED;
	}

	uint directoryEntry = virtualAddress >> 22;
	uint tableEntry = (virtualAddress >> 12) & 0x3FF;

	page_entry *pageDirectory = (page_entry *)directoryBase;

	if (pageDirectory[directoryEntry].p == 0) {
		return E_PAGE_TABLE_MISSING;
	}

	uint pageTableAddress = pageDirectory[directoryEntry].offset << 12;
	page_entry *pageTable = (page_entry *)pageTableAddress;

	if (pageTable[tableEntry].p == 0) {
		return E_PAGE_MISSING;
	}

	pageTable[tableEntry].offset = physicalAddress >> 12;

	tlbflush();
	return E_OK;
}

/**
 * Checks if the virtual address is mapped within the specified table directory.
 *
 * @param directoryBase page directory address, 4K aligned
 * @param virtualAddress address to assign the physical address to
 * @return E_ADDRESS_NOT_ALIGNED, true, false
 */
int isMapped(uint directoryBase, uint virtualAddress) {
	if (directoryBase != ALIGN(directoryBase)) {
		return E_ADDRESS_NOT_ALIGNED;
	}

	uint directoryEntry = virtualAddress >> 22;
	uint tableEntry = (virtualAddress >> 12) & 0x3FF;

	page_entry *pageDirectory = (page_entry *)directoryBase;

	if (pageDirectory[directoryEntry].p == 0) {
		return 0;
	}

	uint pageTableAddress = pageDirectory[directoryEntry].offset << 12;
	page_entry *pageTable = (page_entry *)pageTableAddress;

	return pageTable[tableEntry].p;
}

void mmu_inicializar_dir_kernel() {
	create_page_table(KERNEL_DIR_TABLE, 0, KERNEL_PAGE0, 1, 0);

	uint offset = 0;
	long long x;

	for (x = 0; x < 1024; ++x) {
		create_page(KERNEL_DIR_TABLE, 0, x, offset, 1, 0);
		offset += PAGE_SIZE;
	}

	for (x = 0; x < MAPA_ANCHO; ++x) {
		uint y;

		for (y = 0; y < MAPA_ALTO; ++y) {
			uint offset = game_xy2addressPhys(x, y);
			mmap(offset, offset, KERNEL_DIR_TABLE, 1, 0);
		}
	}

	lcr3((uint)KERNEL_DIR_TABLE);
}

/**
 * @param directoryBase page directory address, 4K aligned
 * @param pirateCodeBaseSrc virtual address of the pirate code
 * @param pirateCodeBaseDst physical address to map the address CODIGO_BASE to
 * @return E_ADDRESS_NOT_ALIGNED, E_OK
 */
int mmu_inicializar_dir_pirata(uint directoryBase, uint pirateCodeBaseSrc, uint pirateCodeBaseDst) {
	if (directoryBase != ALIGN(directoryBase)) {
		return E_ADDRESS_NOT_ALIGNED;
	}

	// Armamos el identity mapping
	uint offset = 0;
	long long x;

	for (x = 0; x < 1024; ++x) {
		// La memoria del kernel la ponemos como user y en read
		mmap(offset, offset, directoryBase, 0, 1);
		offset += PAGE_SIZE;
	}

	// Mapeamos el codigo del pirata
	uint code = mmap(CODIGO_BASE, pirateCodeBaseDst, directoryBase, 1, 1);

	if (code != E_OK) {
		remap(directoryBase, CODIGO_BASE, pirateCodeBaseDst);
	}

	// Copiamos la pagina correspondiente
	mmu_move_codepage(directoryBase, pirateCodeBaseSrc, CODIGO_BASE);

	return E_OK;
}

/**
 * Copies 1 page of memory from the source to the destionation
 *
 * @param directoryBase page directory address, 4K aligned
 * @param codeBaseSrc source virtual address
 * @param codeBaseDstVirt destination virtual address
 * @return E_ADDRESS_NOT_ALIGNED, E_INVALID_ADDRESS, E_OK
 */
int mmu_move_codepage(uint directoryBase, uint codeBaseSrc, uint codeBaseDst) {
	if (directoryBase != ALIGN(directoryBase)) {
		return E_ADDRESS_NOT_ALIGNED;
	}

	if (!isMapped(directoryBase, codeBaseSrc)) {
		return E_INVALID_ADDRESS;
	}

	uint oldCr3 = rcr3();
	lcr3(directoryBase);

	int y;
	int *src = (int *)codeBaseSrc;
	int *dst = (int *)codeBaseDst;

	for (y = 0; y < (PAGE_SIZE/4); ++y) {
		*dst = *src;
		++src;
		++dst;
	}

	lcr3(oldCr3);

	return E_OK;
}
