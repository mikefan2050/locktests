#include "types.h"
#include "user.h"
#include "printf.h"

#define NCHILDREN  8		/* should be > 1 to have contention */
#define CRITSECTSZ 3
//#define DATASZ (1024 * 32 / NCHILDREN)
#define DATASZ (4096 * 32 / NCHILDREN)
//#define DATASZ (32 / NCHILDREN)

#define LOCKS_IMPLEMENTED	/* uncomment this when you have a working lock implementation */
#ifdef LOCKS_IMPLEMENTED
#include "lock.h"
#define LOCK_CREATE(x)  lock_create(x)
#define LOCK_TAKE(x)    lock_take(x)
#define LOCK_RELEASE(x) lock_release(x)
#define LOCK_DEL(x)     lock_delete(x)
#else
int compilehack = 0;
#define LOCK_CREATE(x)  compilehack++
#define LOCK_TAKE(x)    compilehack++
#define LOCK_RELEASE(x) compilehack++
#define LOCK_DEL(x)     compilehack++
#endif


void
child(int lockid, int pipefd, char tosend)
{
	int i, j, childlock;
	int take = 0;
	int numtake = 0;

	/*
	 * Try to create and take as many locks in child process
	 * as possible. In the end of child proc, no release
	 * function will be called. Instead, the exit should
	 * release and deallocate the childlock.
	 */
	childlock = LOCK_CREATE(LOCK_SPIN);
	if (childlock != -1) {
		if (LOCK_TAKE(childlock) < 0)
			printf(1, "error: cannot take %d in child\n", childlock);
	}

	/*
	 * Two locks work together
	 */
	for (i = 0 ; i < DATASZ ; i += CRITSECTSZ) {
		int t = LOCK_TAKE(lockid);
		take = take + t;
		numtake++;
		for (j = 0 ; j < CRITSECTSZ ; j++) {
			write(pipefd, &tosend, 1);
		}
		/*
		 * Call delete instead of release at the last cycle.
		 * The delete should release the lock and only remove
		 * the local lock record (other process not exit yet).
		 * If delete does not work properly, the other children
		 * waiting on the same BLOCK lock will sleep forever.
		 */
		if (i + CRITSECTSZ >= DATASZ) {
			LOCK_DEL(lockid);
		} else {
			LOCK_RELEASE(lockid);
		}
	}
	exit();
}

int
main(void)
{
	int i;
	int pipes[2];
	char data[NCHILDREN], first = 'a';
	int lockid;

	for (i = 0 ; i < NCHILDREN ; i++) {
		data[i] = (char)(first + i);
	}

	if (pipe(&pipes[0])) {
		printf(1, "Pipe error\n");
		exit();
	}

	int lock1 = LOCK_CREATE(LOCK_SPIN);
	int lock2 = LOCK_CREATE(LOCK_BLOCK);

	if (lock1 < 0 || lock2 < 0) {
		printf(1, "error: create lock");
		exit();
	}

	LOCK_DEL(lock1);
	LOCK_DEL(lock2);

	if ((lockid = LOCK_CREATE(LOCK_BLOCK)) < 0) {
		printf(1, "Lock creation error\n");
		exit();
	}

	printf(1, "The 3rd lockid is %d, two old locks deallocated\n");

	for (i = 0 ; i < NCHILDREN ; i++) {
		if (fork() == 0) child(lockid, pipes[1], data[i]);
	}
	close(pipes[1]);

	int race = 0;
	int size = 0;

	while (1) {
		char fst;
		char c;
		int cnt;

		fst = '_';
		for (cnt = 0 ; cnt < CRITSECTSZ ; cnt++) {
			if (read(pipes[0], &c, 1) == 0) goto done;
			++size;

			//printf(1, "%c\n", c);
			if (fst == '_') {
				fst = c;
			} else if (fst != c) {
				printf(1, "%c RACE!!!\n", c);
				++race;
			}

		}
	}
done:

	for (i = 0 ; i < NCHILDREN ; i++) {
		if (wait() < 0) {
			printf(1, "Wait error\n");
			exit();
		}
	}

	/*
	 * Create a new lock after all child exited and check
	 * its lock id. If the lock id is 1, that means all
	 * locks created and held by child processes are released
	 * and deallocated on exit.
	 */

	int newlock;
	if ((newlock = LOCK_CREATE(LOCK_BLOCK)) < 0) {
		printf(1, "error: newlock creation error\n");
		exit();
	}

	printf(1, "After all children, the new lock's id is %d\n", newlock);

	LOCK_DEL(lockid);

	//debug
	printf(1, "lockid: %d  RACE: %d\n", lockid, race);
	printf(1, "read in: %d\n", size);

	exit();
}
