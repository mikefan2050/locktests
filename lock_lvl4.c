#include "types.h"
#include "user.h"
#include "lock.h"

void child1(int lockid, int pipefd) {
	printf(1, "in child\n");

	int childlock = -1;
	if ((childlock = lock_create(LOCK_SPIN)) < 0)
		printf(1, "error: child cannot create lock\n");
	printf(1, "childlock = %d\n", childlock);

	if (lock_take(lockid) < 0)
		printf(1, "error: child cannot take parent's lock\n");
	if (lock_take(childlock) < 0)
		printf(1, "error: child cannot take the new lock\n");

	printf(1, "write to pipe...\n");
	write(pipefd, &childlock, sizeof(childlock));

	if (lock_release(childlock) < 0)
		printf(1, "error: child1 cannot release the child lock\n");

	if (lock_release(lockid) < 0)
		printf(1, "error: child cannot release parent's lock\n");

	//sleep so that others can try the lock
	sleep(700);

	//Delete childlock
	exit();
}

//Testing processes
void child2(int parentlock, int lockid, int pipefd) {
	int response = 0;
	int divider = 6666666;

	if (lock_take(lockid) < 0) {
		response = -1;
	}

	lock_take(parentlock);

	write(pipefd, &response, sizeof(response));
	write(pipefd, &divider, sizeof(divider));

	lock_release(parentlock);

	lock_release(lockid);

	exit();
}

/*
 * This file test the inheritance of locks. The first child test if
 * child processes can take lock created by parent processes. Then
 * it generates a spinlock and pass back to the parent via pipe. While
 * the first child is sleeping for a period of time, the parent tries
 * to take the child's spinlock. (Should fail) Then the parent fork
 * 20 more children and let them to try to take this lock. (Should fail)
 * The 20 children will also try to take parent's lock and write their
 * test result on the spinlock back to parent process via pipe. The parent's
 * lock will provide mutex over pipe. In the end there should be 20 lines
 * of failure message on first child's spinlock.
 */

int
main(int argc, char *argv[])
{
	int pipes[2];
	int id;

	if (pipe(&pipes[0])) {
		printf(1, "error: pipe error\n");
		exit();
	}

	id = lock_create(LOCK_ADAPTIVE);
	if (id < 0) {
		printf(1, "error: cannot create\n");
		exit();
	}
	printf(1, "Parent lockid = %d\n", id);

	//This is the first child
	int pid;
	pid = fork();
	if (pid < 0) {
		printf(1, "error: fork\n");
		exit();
	} else if (pid == 0) {
		child1(id, pipes[1]);
		exit();
	}
	int childlock = -1;
	read(pipes[0], &childlock, sizeof(childlock));
	printf(1, "received child lockid = %d\n", childlock);

	//Test the lock passed back
	if (lock_take(childlock) < 0)
		printf(1, "error: parent no access to childlock (take)\n");

	if (lock_release(childlock) < 0)
		printf(1, "error: parent no access to childlock (release)\n");

	//The rest of the children
	int pid2;
	int k, feedback;
	for (k = 0; k < 20; k++) {
		if ((pid2 = fork()) == 0) child2(id, childlock, pipes[1]);
	}
	close(pipes[1]);

	//Read all response
	while (read(pipes[0], &feedback, sizeof(feedback)) != 0) {
		//if not a divider
		if (feedback != 6666666)
			printf(1, "Fail to take child spinlock %d\n", childlock);
	}

	for (k = 0; k < 20; k++) {
		wait();
	}

	printf(1, "back to parent\n");


	if (lock_take(id) == -1) {
		printf(1, "error: cannot take %d\n", id);
	}
	if (lock_release(id) == -1) {
		printf(1, "error: cannot release %d\n", id);
	}

	int newid = lock_create(LOCK_BLOCK);
	if (newid < 0) {
		printf(1, "error: cannot create 2nd lock\n");
		exit();
	}
	printf(1, "2nd lockid = %d, child spinlock is deallocated\n", newid);

	//wait for the first child
	wait();
	exit();
}
