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

	//if (lock_release(childlock) < 0)
		//printf(1, "error: child cannot release the new lock\n");

	//sleep so that the parent can try the lock
	sleep(200);

	//release the parent's lock when exit, and delete childlock
	exit();
}

int
main(int argc, char *argv[])
{
	int pipes[2];
	int i, id, pid;
	i = 0;
	lock_type_t type = LOCK_SPIN;

	if (pipe(&pipes[0])) {
		printf(1, "error: pipe error\n");
		exit();
	}

	while ((id = lock_create(type)) != -1) {
		++i;
		int t = i % 3;
		if (t == 0) {
			type = LOCK_SPIN;
		} else if (t == 1) {
			type = LOCK_BLOCK;
		} else {
			type = LOCK_ADAPTIVE;
		}
	}
	printf(1, "Max number of locks: %d\n", i);

	for (i = i - 1; i >= 0; i--) {
	//for (i = 0; i < MAX_NUM_LOCKS; i++) {
		lock_delete(i);
	}

	id = lock_create(LOCK_ADAPTIVE);
	if (id < 0) {
		printf(1, "error: cannot create\n");
		exit();
	}
	printf(1, "1st lockid = %d\n", id);

	if (lock_take(id + 3) == -1) {
		printf(1, "error: invalid lockid (take)\n");
	}

	if (lock_release(id) == -1) {
		printf(1, "error: release without holding the lock\n");
	}

	if (lock_take(id) == -1) {
		printf(1, "error: cannot take %d\n", id);
	}

	if (lock_take(id) == -1) {
		printf(1, "error: cannot take lock (still holds the lock)\n");
	}

	if (lock_release(id + 3)) {
		printf(1, "error: invalid lockid (release)\n");
	}

	if (lock_release(id)) {
		printf(1, "error: cannot release %d\n", id);
	}

	pid = fork();

	if (pid < 0) {
		printf(1, "error: fork\n");
		exit();
	} else if (pid == 0) {
		child1(id, pipes[1]);
	}
	close(pipes[1]);

	int childlock = -1;

	read(pipes[0], &childlock, sizeof(childlock));

	printf(1, "received child lockid = %d\n", childlock);
	if (lock_take(childlock) < 0)
		printf(1, "error: parent no access to childlock (take)\n");

	if (lock_release(childlock) < 0)
		printf(1, "error: parent no access to childlock (release)\n");

	wait();

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
	printf(1, "2nd lockid = %d, child lock is deallocated\n", newid);

	for (i = newid + 1; i < MAX_NUM_LOCKS; i++) {
		int t = i % 3;
		if (t == 0) {
			type = LOCK_SPIN;
		} else if (t == 1) {
			type = LOCK_BLOCK;
		} else {
			type = LOCK_ADAPTIVE;
		}
		lock_create(type);
	}

	int error_take = 0;
	for (i = 0; i < MAX_NUM_LOCKS; i++) {
		int r = lock_take(i);
		if (r) printf(1, "error: take %d\n", i);
		error_take += r;
	}

	int error_release = 0;
	for (i = 0; i < MAX_NUM_LOCKS; i++) {
		int r = lock_release(i);
		if (r) printf(1, "error: release %d\n", i);
		error_release += r;
	}

	printf(1, "take errors: %d, release errors: %d\n",
	       error_take, error_release);

	for (i = 0; i < MAX_NUM_LOCKS; i++) {
		lock_delete(i);
	}

	int thirdid = lock_create(LOCK_BLOCK);
	if (thirdid < 0) {
		printf(1, "error: cannot create 3rd lock\n");
		exit();
	}
	printf(1, "3rd lockid = %d, all old locks are deallocated\n", thirdid);



	exit();
}

