// pageReplacement.cpp : Definiert den Einstiegspunkt f�r die Konsolenanwendung.
//

#include "memoryManagement.h"

/* Declaration of local variables */
unsigned emptyFrameCounter;		// number of empty Frames 
frameList_t emptyFrameList = NULL;  // the beginning of EMPTY framelist
frameListEntry_t *emptyFrameListTail = NULL; // end of the EMPTY frame list


// THIS CAN BE REMOVED
unsigned loadedFrameCounter;	// no of USED frames 
usedFrameListEntry_t loadedFrameListTail = NULL; // end of the USED frame List


/* ------------------------------------------------------------------------ */
/*		               Declarations of local helper functions				*/


Boolean isPagePresent(unsigned pid, unsigned page);
/* Predicate returning the present/absent status of the page in memory		*/

Boolean storeEmptyFrame(int frame);
/*	Store the frame number in the data structure of empty frames				
	and update emptyFrameCounter											*/

int getEmptyFrame(void);
/*	Returns the frame number of an empty frame.								
	A return value of -1 indicates that no empty frame exists. In this case	
	a page replacement algorithm must be called to free evict a page and		
	thus clear one frame */


// BOTH STORING AND REMOVING USED FRAME MUST BE CHANGED TO GO THROUGH THE PROCESS TABLE AND ACCESS THE LIST OF EACH PROCESS
//	Ibrahim: 
Boolean storeUsedFrame(unsigned frameNo, unsigned page, unsigned pid);
/*	Einlagern der Frame, und zugeh�rigen information in der List alle der	
	benutzten Rahmen */
// BOTH STORING AND REMOVING USED FRAME MUST BE CHANGED TO GO THROUGH THE PROCESS TABLE AND ACCESS THE LIST OF EACH PROCESS
//	Ibrahim
Boolean removeUsedFrame(int frameNo, unsigned page, unsigned pid);
/* Auslagern der Frame, und zugeh�rigen info aus der Liste alle benutzten Rahmen */

// THIS NEEDS TO TAKE IN PID SO IT CAN REFERENCE THE SAME PROCESS TABLE
Boolean sortUsedFrameList();
/*	Ein Hilsfuntkion zur Sortierung der usedFrameList eines Prozess, der wird		
	bevor eine Seite verdr�ngt werden, aufgerufen, damit die erste Seite (die mit	
	dem kleinsten Aging-wert) in der Liste verdr�ngt wurde							
	Die Sortierung ist modelliert nach Radix sort							*/

Boolean movePageOut(unsigned pid, unsigned page, int frame); 
/*	Creates an empty frame at the given location.							
	Copies the content of the frame occupid by the given page to secondary	
	storage.																	
	The exact location in seocondary storage is found and alocated by		
	this function.															
	The page table entries are updated to reflect that the page is no longer 
	present in RAM, including its location in seondary storage				
	Returns TRUE on success and FALSE on any error							*/

Boolean movePageIn(unsigned pid, unsigned page, unsigned frame);
/* Returns TRUE on success and FALSE on any error							*/

Boolean updatePageEntry(unsigned pid, action_t action);
/*	updates the data relevant for page replacement in the page table entry,	
	e.g. set reference and modify bit.										
	In this simulation this function has to cover also the required actions	
	nornally done by hardware, i.e. it summarises the actions of MMu and OS  
	when accessing physical memory.											
	Returns TRUE on success and FALSE on any error							*/



Boolean pageReplacement(unsigned *pid, unsigned *page, int *frame); 
/*	===== The page replacement algorithm								======	
	In the initial implementation the frame to be cleared is chosen			
	globaly and randomly, i.e. a frame is chosen at random regardless of the	
	process that is currently using it.										
	The values of pid and page number passed to the function may be used by  
	local replacement strategies 
	OUTPUT: 
	The frame number, the process ID and the page currently assigned to the	
	frame that was chosen by this function as the candidate that is to be	
	moved out and replaced by another page is returned via the call-by-		
	reference parameters.													
	Returns TRUE on success and FALSE on any error							*/


/* ------------------------------------------------------------------------ */
/*                Start of public Implementations							*/


Boolean initMemoryManager(void)
{
	// mark all frames of the physical memory as empty 
	for (int i = 0; i < MEMORYSIZE; i++)
		storeEmptyFrame(i);
	return TRUE;
	// TODO: For each process, load a number of pages into frames,
	// gute Startzahl 4 Rahmen pro Prozess!
}

int accessPage(unsigned pid, action_t action)
/* handles the mapping from logical to physical address, i.e. performs the	*/
/* task of the MMU and parts of the OS in a computer system					*/
/* Returns the number of the frame on success, also in case of a page fault */
/* Returns a negative value on error										*/
{
	int frame = INT_MAX;		// the frame the page resides in on return of the function
	unsigned outPid = pid;
	unsigned outPage= action.page;
	// check if page is present
	if (isPagePresent(pid, action.page))
	{// yes: page is present
		// look up frame in page table and we are done
		frame = processTable[pid].pageTable[action.page].frame;
	}
	else
	{// no: page is not present
		logPid(pid, "Pagefault");
		// check for an empty frame
		frame = getEmptyFrame();
		if (frame < 0)
		{	// no empty frame available: start replacement algorithm to find candidate frame
			logPid(pid, "No empty frame found, running replacement algorithm");
			pageReplacement(&outPid, &outPage, &frame);
			// move candidate frame out to secondary storage
			movePageOut(outPid, outPage, frame);			
			frame = getEmptyFrame();
		} // now we have an empty frame to move the page into
		// move page in to empty frame
		movePageIn(pid, action.page, frame);
	}
	// update page table for replacement algorithm
	updatePageEntry(pid, action);
	return frame;
}

Boolean createPageTable(unsigned pid)
/* Create and initialise the page table	for the given process				*/
/* Information on max. process size must be already stored in the PCB		*/
/* Returns TRUE on success, FALSE otherwise									*/
{
	page_t *pTable = NULL;
	// create and initialise the page table of the process
	pTable = malloc(processTable[pid].size * sizeof(page_t));		// creates an array of pages a process holds
	if (pTable == NULL) return FALSE; 
	// initialise the page table
	for (unsigned i = 0; i < processTable[pid].size; i++)
	{
		pTable[i].present = FALSE;		// indicates that Page is not stored in memory
		pTable[i].frame = -1;
		pTable[i].swapLocation = -1;
	}
	processTable[pid].pageTable = pTable; 
	return TRUE;
}

// TODO remove all page entries/frame entries from the usedFrameList that belong to this process!!
// alle seiten/rahmen eintr�ge der usedFramelist, die zu dem Prozess geh�rt, entfernen!!
Boolean deAllocateProcess(unsigned pid)
/* free the physical memory used by a process, destroy the page table		*/
/* returns TRUE on success, FALSE on error									*/
{
	// iterate the page table and mark all currently used frames as free
	page_t *pTable = processTable[pid].pageTable;
	for (unsigned i = 0; i < processTable[pid].size; i++)
	{
		if (pTable[i].present == TRUE)
		{	// page is in memory, so free the allocated frame
			storeEmptyFrame(pTable[i].frame);	// add to pool of empty frames
			// update the simulation accordingly !! DO NOT REMOVE !!
			sim_UpdateMemoryMapping(pid, (action_t) { deallocate, i }, pTable[i].frame);
		}
	}
	free(processTable[pid].pageTable);	// free the memory of the page table
	return TRUE;
}

/* ---------------------------------------------------------------- */
/*                Implementation of local helper functions          */

Boolean isPagePresent(unsigned pid, unsigned page)
/* Predicate returning the present/absent status of the page in memory		*/
{
	return processTable[pid].pageTable[page].present; 
}

Boolean storeEmptyFrame(int frame)
/* Store the frame number in the data structure of empty frames				*/
/* and update emptyFrameCounter												*/
{
	frameListEntry_t *newEntry = NULL;
	newEntry = malloc(sizeof(frameListEntry_t)); 
	if (newEntry != NULL)
	{
		// create new entry for the frame passed
		newEntry->next = NULL;			
		newEntry->frame = frame;
		if (emptyFrameList == NULL)			// first entry in the list
		{
			emptyFrameList = newEntry;
		}
		else								// appent do the list
			emptyFrameListTail->next = newEntry;
		emptyFrameListTail = newEntry; 
		emptyFrameCounter++;				// one more free frame
	}
	return (newEntry != NULL); 
}

int getEmptyFrame(void)
/* Returns the frame number of an empty frame.								*/
/* A return value of -1 indicates that no empty frame exists. In this case	*/
/* a page replacement algorithm must be called to evict a page and thus 	*/
/* clear one frame															*/
{
	frameListEntry_t *toBeDeleted = NULL;
	int emptyFrameNo = -1;
	if (emptyFrameList == NULL) return -1;	// no empty frame exists
	emptyFrameNo = emptyFrameList->frame;	// get number of empty frame
	// remove entry of that frame from the list
	toBeDeleted = emptyFrameList;			
	emptyFrameList = emptyFrameList->next; 
	free(toBeDeleted); 
	emptyFrameCounter--;					// one empty frame less
	return emptyFrameNo; 
}


// MUSS ANGEPASST WERDEN DAMIT ES AUF LISTE EIGENEN PROZESSEN FUNKTIONIERT
/* */
Boolean storeUsedFrame(unsigned frameNo, unsigned page, unsigned pid) {
	usedFrameList_t frameList = processTable[pid].usedFrames; 
	usedFrameListEntry_t* newEntry = NULL;
	newEntry = malloc(sizeof(usedFrameListEntry_t));	// create a new entry
	if (newEntry != NULL) {		
		newEntry->frame = frameNo;
		newEntry->next = NULL;
		/*
		Consider if we need anything else here other than the aging value, if not only keep the aging value
		remove pointe to the page table entry corresponding to this frame, or otherwise store the pid and page no in the usedFrameList
		entry and use that to further simplify other mehtods*/
		newEntry->residentPage = &(processTable[pid].pageTable[page]);	// DIE FRAGE IST: REFERENZIEREN WIR AUF DIE GANZE SEITE
		newEntry->age = processTable[pid].pageTable[page].agingVal;		// ODER SPEICHERN WIR DIE AGING VALUE AB? 
		if (frameList == NULL) {		// Spezialfall: noch keine Eintr�ge in der Liste
			frameList = newEntry;
		}
		else {
			frameList->next = newEntry; // Normalerweise f�gt man neuen Eintrag am Ende der lokalen Liste
		}
	}
	return (newEntry != NULL);
}

// MUSS ANGEPASST WERDEN DAMIT ES AUF LISTE EIGENEN PROZESSEN FUNKTIONIERT
// page fault calls page replacement which evicts a frame with frameNo, then calls this function
Boolean removeUsedFrame(int frameNo, unsigned page, unsigned pid) {
	usedFrameList_t iterator = processTable[pid].usedFrames;
	if (iterator == NULL) return FALSE;
	if (iterator->frame == frameNo) {	// Spezialfall: erste Eintrag der Liste ist die gesuchte usedFrameListEntry
		processTable[pid].usedFrames = iterator->next;
		return TRUE;
	}
	while (iterator->next != NULL) {	// Normalverlauf: iterieren durch die Liste bis gesuchte Eintrag gefunden
		if (iterator->next->frame == frameNo) {
			iterator->next = iterator->next->next; // einbinden der vom Ziel Eintrag, mit der nach dem Ziel Eintrag
			return TRUE;
		}
		iterator = iterator->next;
	}
	return FALSE;	// Wenn der gesuchte Eintrag nicht gefunden ist,  FALSE zur�ckliefern
}
// hier list ist gleich der usedFrameList eines Prozesses, wird von der funktion pageReplacement �bergeben
Boolean sortUsedFrameList(const unsigned char point, usedFrameList_t list)
/*	Diese Sortierung funktionert nach der Prinzip eines Radix sort, d.h:
	es evaluiert jeder Aging Value nicht nach dem gesamten Wert sondern nach
	der Wert jeder Bitstelle. Diese Sort ist rekursive und wird die Liste 
	in Teil-liste zerkleinen. Diese Teil-listen werden dan rekursive sortiert
	ansteigend nach der Agingwert und zusammengef�gt, wenn alle Bitstellen
	evaluiert sind.
	EINSCHR�NKUNGEN:
	wenn es mehr als zwei Rahmen/Frames gibt, die den gleichen Agingwer hat
	wird dieses Algorithmus nicht erkennen welche Rahmen erst in einem Timer-
	Interval referenziert wurde.*/
{
	if (list == NULL || list->next == NULL) return TRUE; // spezialf�lle: nur ein Eintrag in der Liste oder Liste ist leer
	// sublists OR buckets
	usedFrameList_t zeroes = NULL;		//where LSB = 0, the LSB will shift one place right with each recursion e.g �0�000 0000 -> 0�0�00 0000
	usedFrameList_t zeroesLast = NULL;	// 
	usedFrameList_t ones = NULL;		//
	usedFrameList_t onesLast = NULL;	//
	usedFrameList_t iterator = list;
	while (iterator != NULL) {
		if ((iterator->residentPage->agingVal & point)) {
			if (ones = NULL) {		// Erste eintrag der Sublist wo LMB = 1
				ones = iterator;
				onesLast = iterator;
			}
			else {
				onesLast->next = iterator; // normales Appendieren
				onesLast = iterator;
			}
		} else {
			if (zeroes == NULL) {	// Erste eintrag der Sublist wo LMB = 0
				zeroes = iterator;
				zeroesLast = iterator;
			}
			else {
				zeroesLast->next = iterator; // normales Appendieren
				zeroesLast = iterator;
			}
		}
		iterator = iterator->next;
	}
	if (onesLast->next != NULL) { // sichern dass die Sublisten auf Null endet
		onesLast->next = NULL;
	}
	if (zeroesLast->next != NULL) {
		zeroesLast->next = NULL;
	}
	if (point >> 1) {   // Rekrusive aufruf der Funktion, wobei die N�chste Bitstelle �berpr�ft wird
		usedFrameList_t lhs = sortUsedFrameList(point >> 1, zeroes);
		usedFrameList_t rhs = sortUsedFrameList(point >> 1, ones);
		if (lhs != NULL) {
			iterator = lhs;
			if (rhs == NULL) {
				return lhs;
			}
			while (iterator->next != NULL) {
				iterator = iterator->next;
			}
			iterator->next = rhs;
			return lhs;
		}
		else {
			return rhs;
		}
	}
	if (zeroes != NULL) {
		zeroesLast->next = ones;
		return zeroes;
	}
	else {
		return ones;
	}
}

Boolean movePageIn(unsigned pid, unsigned page, unsigned frame)
/* Returns TRUE on success ans FALSE on any error							*/
{
	// copy of the content of the page from secondary memory to RAM not simulated
	// update the page table: mark present, store frame number, clear statistics
	// *** This must be extended for advanced page replacement algorithms ***
	// goes directly to the process and its page, recording info of the frame the page is stored in
	processTable[pid].pageTable[page].frame = frame;		
	processTable[pid].pageTable[page].present = TRUE;
	// page was just moved in, reset all statistics on usage, R- and P-bit at least
	processTable[pid].pageTable[page].modified = FALSE;
	processTable[pid].pageTable[page].referenced = FALSE;
	processTable[pid].pageTable[page].agingVal &= 0x80;	// set left most r bit to 1
	// append the new frame to the list 
	storeUsedFrame(frame, page, pid);	// stores the newly loaded page in the used frame list
	// Statistics for advanced replacement algorithms need to be reset here also 

	// update the simulation accordingly !! DO NOT REMOVE !!
	sim_UpdateMemoryMapping(pid, (action_t) { allocate, page }, frame);
	return TRUE;
}

Boolean movePageOut(unsigned pid, unsigned page, int frame)
/* Creates an empty frame at the given location.							*/
/* Copies the content of the frame occupid by the given page to secondary	*/
/* storage.																	*/
/* The exact location in seocondary storage is found and alocated by		*/
/* this function.															*/
/* The page table entries are updated to reflect that the page is no longer */
/* present in RAM, including its location in seondary storage				*/
/* Returns TRUE on success and FALSE on any error							*/
{
	// allocation of secondary memory storage location and copy of page are ommitted for this simulation
	// no distinction between clean and dirty pages made at this point
	// update the page table: mark absent, add frame to pool of empty frames
	// *** This must be extended for advenced page replacement algorithms ***
	processTable[pid].pageTable[page].present = FALSE;
	removeUsedFrame(frame, page, pid); // remove from the list of used frames
	storeEmptyFrame(frame);	// add to pool of empty frames
	// update the simulation accordingly !! DO NOT REMOVE !!
	sim_UpdateMemoryMapping(pid, (action_t) { deallocate, page }, frame);
	return TRUE;
}

Boolean updatePageEntry(unsigned pid, action_t action)
/* updates the data relevant for page replacement in the page table entry,	*/
/* e.g. set reference and modify bit.										*/
/* In this simulation this function has to cover also the required actions	*/
/* normally done by hardware, i.e. it summarises the actions of MMU and OS  */
/* when accessing physical memory.											*/
/* Sets the left most Rbit to 1, this is done on newly stored pages as well */
/* Returns TRUE on success ans FALSE on any error							*/
// *** This must be extended for advences page replacement algorithms ***
{
	processTable[pid].pageTable[action.page].referenced = TRUE;  // Necessary at this stage?
	processTable[pid].pageTable[action.page].agingVal |= 0x80;	// as the page is referenced, set the Left most bit to 1
	if (action.op == write)
		processTable[pid].pageTable[action.page].modified = TRUE;
	return TRUE; 
}



// ALL THIS DOES IS CALL RETURN THE FRAME TO BE REMOVED 
Boolean pageReplacement(unsigned *outPid, unsigned *outPage, int *outFrame)
/* ===== The page replacement algorithm								======	*/
/* In the initial implementation the frame to be cleared is chosen			*/
/* globaly and randomly, i.e. a frame is chosen at random regardless of the	*/
/* process that is currently usigt it.										*/
/* The values of pid and page number passed to the function may be used by  */
/* local replacement strategies */
/* OUTPUT: */
/* The frame number, the process ID and the page currently assigned to the	*/
/* frame that was chosen by this function as the candidate that is to be	*/
/* moved out and replaced by another page is returned via the call-by-		*/
/* reference parameters.													*/
/* Returns TRUE on success and FALSE on any error							*/
{
	Boolean found = FALSE;		// flag to indicate success
	// just for readbility local copies ot the passed values are used:
	unsigned pid = (*outPid); 
	unsigned page = (*outPage);
	int frame = *outFrame; 
	
	// +++++ START OF REPLACEMENT ALGORITHM IMPLEMENTATION: GLOBAL RANDOM ++++
	logGeneric("MEM: Choosing a frame randomly, this must be improved");
	// frame = rand() % MEMORYSIZE; choses a frame by random -> outFrame should be found via rbits
	/* TO CHANGE:
	// As the initial implemetation does not have data structures that allows
	// easy retrieval of the identity of a page residing in a given frame, 
	// now the frame ist searched for in all page tables of all running processes
	// I.e.: Iterate through the process table and for each valid PCB check 
	// the valid entries in its page table until the frame is found 
	*/ 
	// GLOBAL VER:
	// Iterate throught the process table and for each valid PCB check ALL valid
	// page entries till completion, choosing the page with the lowest Rbit amongst
	// all pages and processes (WARNING: very slow possibly O(n�)) A data struct is needed
	// to manage loaded frames and their corresponding pages and processes
	pid = 0; page = 0; 
	do 
	{
		pid++;
		if ((processTable[pid].valid) && (processTable[pid].pageTable != NULL))
			for (page = 0; page < processTable[pid].size; page++)
				if (processTable[pid].pageTable[page].frame == frame) {
					found = TRUE;
					break;
				}
	} while ((!found) && (pid < MAX_PROCESSES));
	// +++++ END OF REPLACEMENT ALFGORITHM found indicates success/failure
	// RESULT is pid, page, frame

	// prepare returning the resolved location for replacement
	if (found)
	{	// assign the current values to the call-by-reference parameters
		(*outPid) = pid;
		(*outPage) = page; 
		(*outFrame) = frame;
	}
	return found; 
}