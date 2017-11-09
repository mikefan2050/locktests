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
	int i, j;
	int take = 0;
	int release = 0;
	int numtake = 0;

	for (i = 0 ; i < DATASZ ; i += CRITSECTSZ) {
		/*
		 * If the critical section works, then each child
		 * writes CRITSECTSZ times to the pipe before another
		 * child does.  Thus we can detect race conditions on
		 * the "shared resource" that is the pipe.
		 */
		int t = LOCK_TAKE(lockid);
		take = take + t;
		numtake++;
		for (j = 0 ; j < CRITSECTSZ ; j++) {
			write(pipefd, &tosend, 1);
		}
		int r = LOCK_RELEASE(lockid);
		release = release + r;
	}
	//debug
	//printf(1, "t = %d r = %d num = %d\n", take, release, numtake);
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

	LOCK_CREATE(LOCK_SPIN);
	LOCK_CREATE(LOCK_BLOCK);

	if ((lockid = LOCK_CREATE(LOCK_BLOCK)) < 0) {
		printf(1, "Lock creation error\n");
		exit();
	}

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

	//LOCK_DEL(lockid);

	//debug
	printf(1, "lockid: %d  RACE: %d\n", lockid, race);
	printf(1, "read in: %d\n", size);

	exit();
}
