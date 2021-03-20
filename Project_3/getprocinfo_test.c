#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "processInfo.h"

#define MAX_PID 64

int main(void){
	struct processInfo procInfo;
	int i;
	printf(1,"PID\tPPID\tSIZE\tNumber of Context Switch\n");
	for(i=1;i<=get_max_pid();i++){
		if(get_proc_info(i,&procInfo)<0)
			continue;
		printf(1,"%d\t%d\t%d\t%d\n",procInfo.pid,procInfo.ppid,procInfo.psize,procInfo.numberContextSwitches);
	}
	exit();
}
