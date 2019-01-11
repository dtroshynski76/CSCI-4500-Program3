/*----------------------------------------------------*/
/* Author: Donovan Troshynski                         */
/* Class: CSCI 4500, Fall 2018                        */
/* Assignment: Program 3, Buddy Algorithm             */
/*----------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef unsigned int uint;	/* ... for convenience */

int verbose;		/* non-zero if the -v option was specified */

/*--------------------------------------------------------------*/
/* msize and asize are the total memory size and the smallest   */
/* allocation size as specified on the first line of the input. */
/* Each of these must be a power of 2.                          */
/*--------------------------------------------------------------*/
uint msize;		/* total memory size */
uint asize;		/* smallest allocation size */

int ns;			/* # of different block sizes possible */
/* This is computed in the main function. */

/*-------------------------------------------------------------------*/
/* Every request for memory in the input will eventually have a node */
/* of type "struct idstat" to record the status of the request. This */
/* node is created when the allocation request for memory is first   */
/* read, and the state then becomes 1 (DEFERRED) or 2 (ALLOCATED),   */
/* depending on whether sufficient memory is available to satisfy    */
/* the request. When a deallocation request is read, the memory is   */
/* released (for this assignment, it's an error to attempt to free   */
/* memory for a currently-deferred allocation), the state is set to  */
/* 3 (DONE/DEALLOCATED), and all currently deferred allocations are  */
/* considered to see if they could now be processed.                 */
/*-------------------------------------------------------------------*/
struct idstat {		/* node for tracking requests */
    struct idstat *next;/* ptr to next idstat node */
    int rid;	        /* request ID */
    int state;	        /* request state: */
    /*    0 = UNKNOWN (only used during node creation) */
    /*    1 = DEFERRED */
    /*    2 = ALLOCATED */
    /*    3 = DEALLOCATED */
    uint size;	        /* region size (power of 2) */
    uint addr;	        /* region address */
} *rlist,		/* list of request status nodes */
  *rle;			/* a single entry on rlist */

/*------------------------------------------------------------------*/
/* Every unused memory region (that is, those that are available to */
/* satisfy allocation requests) appears on one of the linked lists  */
/* pointed to by an entry in the flist array. flist[0] points to a  */
/* list of "struct freg" ("free region") nodes of size msize (of    */
/* course, there'll only ever be one of these), flist[1] points to  */
/* a list of nodes of size msize/2, and so forth. The last flist    */
/* array entry (which will be flist[ns-1]) points to a list with    */
/* nodes for memory regions of size asize. Of course, any or all of */
/* these lists could be empty, depending on current allocations.    */
/* These lists are maintained in ascending order on 'addr', so when */
/* we remove the first entry from a list we're guaranteed to get    */
/* the region with the smallest address. This is needed to cause    */
/* all valid solutions to yield exactly the same results.           */
/*------------------------------------------------------------------*/
struct freg {		/* node for a free/unused region */
    struct freg *next;	/* ptr to next node on the same list */
    uint size;		/* size of the region (is this really needed?) */
    uint addr;		/* starting address of the region */
} *fnode,		/* a single free region node */
  **flist;		/* array of struct freg pointers -- list heads */
/* flist[0] for msize, flist[ns-1] for asize */
/* lists kept in ascending order on addr */

/*-------------------------------------------------------------------*/
/* Every allocation request that could not be immediately granted is */
/* identified on the list pointed to by 'def'. Each node on this     */
/* list is of type "struct areq" and just identifies the node on the */
/* list of all requests (that is, rlist). When a deallocation takes  */
/* place, we go through this list to see if any of the deferred      */
/* requests can now be processed.                                    */
/*-------------------------------------------------------------------*/
struct areq {		/* node for a deferred allocation request */
    struct areq *next;	/* ptr to next request */
    struct idstat *p;	/* ptr to idstat node */
} *anode,		/* a single deferred allocation request node */
  *panode,		/* predecessor of anode */
  *def;			/* head of list of deferred requests */

uint totalAllocatedSize;

/*--------------------------------------------------------------*/
/* Display an error message including the string at s and quit. */
/*--------------------------------------------------------------*/
void fail(char *s) {
    fprintf(stderr,"Error: %s\n", s);
    exit(1);
}

/*------------------------------------------------------------*/
/* Display an error message including the string at s (which  */
/* includes a formatting reference to an integer d) and quit. */
/*------------------------------------------------------------*/
void faild(char *s, int d) {
    fprintf(stderr, "Error: ");
    fprintf(stderr, s, d);
    fprintf(stderr, "\n");
    exit(1);
}

/*-------------------------------------------------------------*/
/* Display an error message including the string at s (which   */
/* includes a formatting reference to a character c) and quit. */
/*-------------------------------------------------------------*/
void failc(char *s, char c) {
    fprintf(stderr, "Error: ");
    fprintf(stderr, s, c);
    fprintf(stderr, "\n");
    exit(1);
}

/*---------------------------------------------*/
/* Display the free lists for all size blocks. */
/* This is for debug purposes only.            */
/*---------------------------------------------*/
void show_free_lists(void) {
    uint size;
    int lx;
    struct freg *p;

    printf("Free Lists...\n");
    size = msize;
    lx = 0;
    while (size >= asize) {
        printf("  Size %d:\n", size);
        p = flist[lx];
        if (p == NULL) {
            printf("    (none)\n");
        } else while (p != NULL) {
            printf("    addr = 0x%08x, size = %d\n", p->addr, p->size);
            p = p->next;
        }
        size >>= 1;
        lx++;
    }
    putchar('\n');
}

/*-----------------------------------------------------------------*/
/* Display the status of all requests: deferred, active, or freed. */
/* This is for debug purposes only.                                */
/*-----------------------------------------------------------------*/
void show_allocations(void) {
    struct idstat *p;

    printf("\nStatus of Requests...\n");
    p = rlist;
    while (p != NULL) {
        printf("    ID %d, state =%d ", p->rid, p->state);
        switch(p->state) {
            case 1:
                printf("deferred\n");
                break;
            case 2:
                printf("active, addr 0x%08x, size %d\n",
                        p->addr, p->size);
                break;
            case 3:
                printf("freed, (addr was 0x%08x, size was %d)\n",
                        p->addr, p->size);
                break;
            default:
                printf("Logic error: unknown request state!\n");
                exit(1);
        }
        p = p->next;
    }
    putchar('\n');
}

/*-------------------------------------------*/
/* Display the deferred allocation requests. */
/* This is for debug purposes only.          */
/*-------------------------------------------*/
void show_deferred(void) {
    struct areq *p;		/* ptr to a deferred list node */

    p = def;
    printf("Deferred requests...\n");
    if (def == NULL) {
        printf("  (none)\n");
    }
    else while (p != NULL) {
        printf("    ID %d, size %d\n", p->p->rid, p->p->size);
        p = p->next;
    }
    putchar('\n');
}

/*-------------------------------------------------*/
/* Return 1 if x is a power of 2, and 0 otherwise. */
/*-------------------------------------------------*/
int isp2(uint x) {
    if (x == 0) {
        return 0;	/* special case, treated as not a power of 2 */
    }
    return ((x & (x-1)) == 0);
}

/*--------------------------------------------*/
/* Round up x, if necessary, to a power of 2. */
/*--------------------------------------------*/
uint round2(uint x) {
    uint y = 0x80000000;	/* largest possible 32-bit unsigned integer */

    if (isp2(x)) {		/* if already a power of 2 */
        return x;
    }
    while (y > x) {		/* find y = largest power of 2 less than x */
        y >>= 1;
    }
    return y << 1;		/* y = smallest power of 2 greater than x */
}

/*---------------------------------------------------------*/
/* Return the log base 2 of x, assumed to be a power of 2. */
/*---------------------------------------------------------*/
int logb2(uint x) {
    int y = 0;

    if (x == 0) {
        fprintf(stderr,"Logic error in logb2.\n");
        exit(1);
    }

    while ((x & 1) == 0) {
        y++;
        x >>= 1;
    }
    return y;
}

/*-------------------------------------------------------------------*/
/* Find an rlist entry for the request with ID 'rid' and return a    */
/* pointer to it. Return NULL if no such request exists on the list. */
/*-------------------------------------------------------------------*/
struct idstat *findrle(int rid) {
    struct idstat *p;

    p = rlist;
    while (p != NULL) {
        if (p->rid == rid) {
            return p;
        } else {
            p = p->next;
        }
    }
    return NULL;
}

/*--------------------------------------------------------------------*/
/* Get the next allocation/deallocation request and return 1, or      */
/* return 0 at end of file. On input error, diagnose and quit.        */
/* *rid == request ID; *rtype = 1 for allocation, 0 for deallocation; */
/* *size = size of region requested (only if *rtype == 1).            */
/*--------------------------------------------------------------------*/
int get_request(int *rid, int *rtype, uint *size) {
    int sfr;				/* scanf result */
    int request_id;
    char request_type;
    int request_size;

    sfr = scanf("%d",&request_id);
    if (sfr == EOF) {
        return 0;
    }
    if (sfr == 0) {
        fail("trouble reading request ID from input.");
    }
    sfr = scanf(" %c",&request_type);
    if (sfr != 1) {
        fail("trouble reading request type from input.");
    }
    if (request_type == '-') {
        *rid = request_id;
        *rtype = 0;
        return 1;
    } else if (request_type == '+') {
        sfr = scanf("%d",&request_size);
        if (sfr != 1) {
            fail("trouble reading request size for input allocation request.");
        }
        if (request_size < 1 || request_size > msize) {
            faild("input allocation request size (%d) is invalid.", request_size);
        }
        *rid = request_id;
        *rtype = 1;
        *size = (uint)request_size;
        return 1;
    }
    failc("unrecognized request type: %c", request_type);
    return 0;		/* not reached */
}


/*---------------------------------------------------*/
/* Try to perform allocation for the request at rle. */
/* rle->size must be a power of 2.                   */
/* Return 1 if successful, 0 otherwise.              */
/* If successful, set rle->addr appropriately.       */
/*---------------------------------------------------*/
int allocate(void) {
    struct freg *nfreg;
    uint newSize = 0;
    int i;
    int foundFreeSpace;

    /* check free list (flist) to see if a free block of size rle->size is available */
    for(i = ns - 1; i >= 0; i--) {
        if(rle->size == (msize / (1<<i))) {
            /* gives the index into flist of the same size as rle */
            fnode = flist[i];
            break;
        }
    }
    foundFreeSpace = 0;

    if(fnode != NULL) {
        /* a free region does exist at flist[i] */
        foundFreeSpace = 1;

        /* use first free block to satisfy the request */
        rle->addr = fnode->addr;
        totalAllocatedSize += rle->size;

        /* remove that free block from flist */
        if(fnode->next == NULL) {
            /* do not have another free block in the list */
            flist[i] = NULL;
        } else {
            /* do have another free block in the list */
            flist[i] = fnode->next;
        }
    } else {
        /* flist is empty at i */

        /* find flist at next largest size (i - 1) */
        /* looping through larger and larger sizes until a free block is found */
        /* or all flists have been examined */
        for(i = i - 1; i >= 0; i--) {
            /* if a larger free block is found it is...*/
            if(flist[i] != NULL) {
                fnode = flist[i];
                nfreg = (struct freg *)malloc(sizeof(struct freg));

                /* 1. removed from its list */
                if(fnode->next == NULL) {
                    /* do not have another free block in the list */
                    flist[i] = NULL;
                } else {
                    /* do have another free block in the list */
                    flist[i] = fnode->next;
                }

                /* 2. split into two smaller blocks (divide size by two) */
                newSize = fnode->size / 2;
                if(newSize < asize) {
                    break;
                }

                fnode->size = newSize;
                fnode->addr = fnode->addr;
                fnode->next = nfreg;

                nfreg->size = newSize;
                nfreg->addr = fnode->addr + newSize;
                nfreg->next = NULL;

                /* 3. two smaller blocks are placed on their own flist (i + 1) */
                flist[++i] = fnode;

                /* 4. block with smaller address is used to satisfy the allocation request */
                if(fnode->size == rle->size) {
                    if(fnode->next == NULL) {
                        flist[i] = NULL;
                    } else {
                        flist[i] = fnode->next;
                        fnode->next = NULL;
                    }
                    foundFreeSpace = 1;
                    rle->size = fnode->size;
                    rle->addr = fnode->addr;
                    totalAllocatedSize += rle->size;
                    break;
                } else {
                    /* 4a. if that smaller block is still too big, repeat from (1) */
                    i += 2;
                    continue;
                }
            }
        }
    }

    return foundFreeSpace;
}

/*--------------------------------------*/
/* Defer the request pointed to by rle. */
/*--------------------------------------*/
void defer(void) {
    struct areq *a;		/* the deferred request's node */
    struct areq *curr, *prev;	/* to traverse the list */

    /*-----------------------------------------*/
    /* Create a node for the deferred request. */
    /*-----------------------------------------*/
    a = (struct areq *)malloc(sizeof(struct areq));
    if (a == NULL) {
        fail("Out of memory in defer.");
    }
    a->p = rle;
    a->next = NULL;

    /*--------------------------------------------------------------------*/
    /* Add the node to the end of the deferred list.                      */
    /* This would be more efficient if we kept a head and a tail pointer, */
    /* but it works this way, too. We add entries at the end of the list  */
    /* so the longest deferred entries are those considered first.        */
    /*--------------------------------------------------------------------*/
    prev = NULL;
    curr = def;
    while (curr != NULL) {
        prev = curr;
        curr = curr->next;
    }
    if (prev == NULL) {
        def = a;
    } else {
        prev->next = a;
    }
}

/*-----------------------------------------------------------------------*/
/* Deallocate the request at rle and return the memory region that was   */
/* freed to the correct list. Then do the appropriate joining of blocks, */
/* if possible, as appropriate for the buddy algorithm.                  */
/*-----------------------------------------------------------------------*/
void deallocate(void) {
    uint buddy;
    struct freg *p;
    struct freg *temp;
    struct freg *new = (struct freg *)malloc(sizeof(struct freg));
    int i;
    uint freeListSize;
    uint sizeToDeallocate = rle->size;
    int onList;

    if (verbose) {
        printf("Deallocating block at 0x%08x with size = %d\n",
                rle->addr, rle->size);
    }

    /*-----------------------------------------------------*/
    /* Find the appropriate free list with which to start. */
    /*-----------------------------------------------------*/
    for(i = ns - 1; i >= 0; i--) {
        freeListSize = (msize / (1<<i)); /* 1<<i is equivalent to 2^i */
        if(rle->size == freeListSize) {
            p = flist[i];
            break;
        }
    }

    for(;i >= 0;) {
        /*-------------------------------------------------*/
        /* Find the address of the buddy of the free block */
        /*-------------------------------------------------*/
        buddy = rle->addr / rle->size;
        if(!(buddy & 1)) { /* if low-order bit is not 1, then it's 0 and the number must be even */
            buddy = rle->addr + rle->size;
        } else {
            buddy = rle->addr - rle->size;
        }

        /*----------------------------------*/
        /* See if the buddy is on the list. */
        /*----------------------------------*/
        onList = 0;
        if(p != NULL) {
            temp = p;
            while(temp != NULL) {
                if(temp->addr == buddy) {
                    onList = 1;
                    break;
                }
                temp = temp->next;
            }
        }

        /*------------------------------------------------------*/
        /* If the buddy wasn't found on the list, we've reached */
        /* the right list on which to place the free block.     */
        /*------------------------------------------------------*/
        if(!onList) {
            break;		/* This is how this loop eventually terminates */
        }

        /*------------------------------------------------------*/
        /* Otherwise join block being freed and its buddy to   */
        /* form a block (with twice the size), and then repeat  */
        /* the current loop to continue searching for the right */
        /* list on which the new block should be placed.        */
        /*------------------------------------------------------*/
        /* remove temp from flist */
        if(temp->next != NULL) {
            /* have other nodes in the chain */
            flist[i] = flist[i]->next;
        } else {
            /* no other nodes in the chain */
            flist[i] = NULL;
        }

        /* join temp and rle size */
        rle->size = rle->size + temp->size;

        /* join temp and rle addr (smaller address of the two) */
        if(temp->addr < rle->addr) {
            rle->addr = temp->addr;
        }

        /* get next highest flist */
        p = flist[--i];
    }

    /*----------------------------------------------------------------------*/
    /* We've found the right list on which to add the free block of memory. */
    /* So add it to the list. Keep the blocks in ascending address order.   */
    /*----------------------------------------------------------------------*/
    new->size = rle->size;
    new->addr = rle->addr;
    new->next = NULL;
    if(p == NULL) {
        flist[i] = new;
    } else {
        /* add to chain of nodes */
        if(p->addr < new->addr) {
            p->next = new;
        } else {
            new->next = p;
            flist[i] = new;
        }
    }
    totalAllocatedSize -= sizeToDeallocate;
}

int main(int argc, char *argv[]) {
    int rid;			/* request ID */
    int rtype;			/* request type: 1 = allocate, 0 = free */
    uint rsize;			/* request size */
    int ok;			/* non-zero if allocation succeeded */

    /*------------------------------------*/
    /* Recognize and record options used. */
    /*------------------------------------*/
    verbose = 0;
    while (argc > 1 && argv[1][0] == '-') {
        if (!strcmp(argv[1],"-v")) {
            verbose = 1;
            argc--;
            argv++;
            continue;
        }
        fprintf(stderr,"Unknown option: %s\n", argv[1]);
        exit(1);
    }

    /*-------------------------------------------------------------*/
    /* Read and verify msize and asize. Each must be a power of 2, */
    /* and msize must be greater than or equal to asize.           */
    /*-------------------------------------------------------------*/
    if (scanf("%u",&msize) != 1) {
        fail("trouble reading msize");
    }
    if (msize < 1 || isp2((uint)msize) != 1) {
        faild("msize (%d) is invalid", msize);
    }
    if (scanf("%u",&asize) != 1) {
        fail("trouble reading asize");
    }
    if (asize < 1 || isp2((uint)asize) != 1 || asize > msize) {
        faild("asize (%d) is invalid", asize);
    }

    /*------------------------------------------------------------*/
    /* Construct an array of lists, one for each power of 2 size  */
    /* between asize and msize. Add a single node to the list for */
    /* size msize (address 0).                                    */
    /*------------------------------------------------------------*/
    ns = 0;				/* # of lists */
    while ((asize << ns) != msize) {
        ns++;
    }
    ns++;
    if (verbose) {			/* verbose output */
        uint temp = msize;
        int i;

        printf("Number of block sizes = %d:\n    ", ns);
        for (i=0;i<ns;i++) {
            printf("%d", temp);
            if (i != ns-1) {
                printf(", ");
                temp /= 2;
            } else {
                putchar('\n');
            }
        }
    }

    /*------------------------------------------------------------*/
    /* Allocate storage for an array of ns pointers to objects of */
    /* type "struct freg" (that is, a free region). Each freg     */
    /* object is a node in a linked list of free memory regions,  */
    /* each of which on the same list has the same size.          */
    /*------------------------------------------------------------*/
    flist = (struct freg **)malloc(ns * sizeof(struct freg *));
    if (flist == NULL) {	/* verify allocation is ok */
        fail("Out of memory for flist.");
    }
    memset((void *)flist, 0, ns*sizeof(struct freg *));  /* all entries NULL */

    /*--------------------------------------------------------------*/
    /* Add a single node to the list for size msize (at address 0). */
    /*--------------------------------------------------------------*/
    fnode = (struct freg *)malloc(sizeof(struct freg));  /* allocate node */
    if (fnode == NULL) {		 /* check allocation */
        fail("Out of memory for first fnode.");
    }
    fnode->next = NULL;			/* no next node on this list */
    fnode->size = msize;		/* memory size = all of memory */
    fnode->addr = 0;			/* base address of memory is 0 */
    flist[0] = fnode;			/* node in on the 0-th free list */

    def = NULL;				/* no deferred requests yet */
    rlist = NULL;			/* no requests yet */

    /*-----------------------------------------------------------------*/
    /* Now we read each request, one at a time, a try to process them. */
    /* rid = request ID						       */
    /* rtype = 0 for deallocation, 1 for allocation                    */
    /* rsize = size of requested allocation (only if rtype == 1)       */
    /* get_request returns 1 on success, and 0 at end of file.         */
    /*-----------------------------------------------------------------*/
    while (get_request(&rid, &rtype, &rsize)) {

        if (verbose) {
            putchar('\n');		/* space 'em out a bit */
        }

        /*----------------------------------------------------*/
        /* Display the request we're going to try to process. */
        /*----------------------------------------------------*/
        printf("Request ID %d: ", rid);
        if (rtype) {
            printf("allocate %d byte%s.\n", rsize, rsize == 1 ? "" : "s");
        } else {
            printf("deallocate.\n");
        }

        /*------------------------------------------------*/
        /* Check the state of requests with this ID value */
        /* by searching through the list of all requests. */
        /* If the ID was previously used, then set rle to */
        /* point to the list entry for the request. If    */
        /* the ID value hasn't been used yet, rle = NULL. */
        /*------------------------------------------------*/
        rle = findrle(rid);

        /*------------------------------------------------*/
        /* If allocation request & ID was already used... */
        /*------------------------------------------------*/
        if (rle != NULL && rtype == 1) {
            fail("allocation request has a previously used ID.");
        }

        /*-----------------------------------------------------*/
        /* If deallocation request with no prior allocation... */
        /*-----------------------------------------------------*/
        if (rle == NULL && rtype == 0) {
            faild("deallocation request has no prior allocation.", rid);
        }

        /*-----------------------------------------------------------*/
        /* If deallocation request & allocation is still deferred... */
        /*-----------------------------------------------------------*/
        if (rle != NULL && rtype == 0 && rle->state == 1) {
            faild("deallocation request for deferred allocation.", rid);
        }

        /*-------------------------------------------------------------*/
        /* If deallocation request & deallocation already completed... */
        /*-------------------------------------------------------------*/
        if (rle != NULL && rtype == 0 && rle->state == 3) {
            faild("duplicate deallocation request!", rid);
        }

        /*----------------------------------------------------------*/
        /* Create a new entry on the rlist for allocation requests. */
        /*----------------------------------------------------------*/
        if (rle == NULL) {

            /*---------------------------------------------------------------*/
            /* Allocate memory for a new request list entry, verify success. */
            /*---------------------------------------------------------------*/
            rle = (struct idstat *)malloc(sizeof(struct idstat));
            if (rle == NULL) {
                fail("Out of memory for new rlist entry.");
            }

            /*-----------------------------------------------------------*/
            /* Initialize the request entry, add to list of all requests */
            /*-----------------------------------------------------------*/
            rle->next = rlist;	    /* put entry at head of list */
            rle->rid = rid;	    /* set its ID */
            rle->state = 0;	    /* unknown right now */
            rle->size = round2(rsize);  /* save rounded-up request size */
            if (rle->size < asize) {  /* increase size, if needed, to minimum */
                rle->size = asize;
            }
            rle->addr = 0;	    /* address of allocation is now unknown */
            rlist = rle;	    /* update request list head pointer */
        }

        /*----------------------*/
        /* Process the request. */
        /*----------------------*/

        /*------------*/
        /* ALLOCATION */
        /*------------*/
        if (rtype == 1) {		/* allocation request */
            ok = allocate();		/* try to perform the allocation */

            if (ok) {			/* if request was successful */
                printf("   Success; addr = 0x%08x, total allocated size = %u\n", rle->addr, totalAllocatedSize);
                rle->state = 2;
            } else {			/* if not successful, then defer it */
                rle->state = 1;
                defer();
                printf("   Request deferred.\n");
            }

            /*--------------*/
            /* DEALLOCATION */
            /*--------------*/
        } else {			/* deallocation request */
            deallocate();		/* do the deallocation */
            rle->state = 3;
            printf("   Success. total allocated size = %u\n", totalAllocatedSize);

            /*----------------------------------------*/
            /* Try to allocate each deferred request. */
            /*----------------------------------------*/
            panode = NULL;
            anode = def;
            while (anode != NULL) {
                rle = anode->p;
                ok = allocate();
                if (ok) {	/* on success, remove from deferred list */
                    printf("   Deferred request %d allocated; addr = 0x%08x, total allocated size = %u\n",
                            rle->rid, rle->addr, totalAllocatedSize);
                    rle->state = 2;
                    if (panode == NULL) {
                        def = anode->next;
                    } else {
                        panode->next = anode->next;
                    }
                    free(anode);
                    if (panode == NULL) {
                        anode = def;
                    } else {
                        anode = panode->next;
                    }
                } else {
                    panode = anode;
                    anode = anode->next;
                }
            }
        }

        /*------------------------------------------------------------*/
        /* If verbose output requested, show the state of everything. */
        /*------------------------------------------------------------*/
        if (verbose) {
            show_allocations();
            show_free_lists();
            show_deferred();
        }
    }

    return 0;		/* And we're done! */
}
