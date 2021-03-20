#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <wait.h>
#include <pthread.h>
#include <unistd.h>

int item_to_produce=0, item_to_consume=0; //마지막으로 생산, 소비한 숫자
int total_items, max_buf_size, num_workers, num_masters; //입력받을 인자
int *buffer; //생산 숫자를 저장할 버퍼

//생산, 소비의  mutex, cond 변수
pthread_mutex_t gen_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t con_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t gen_cond=PTHREAD_COND_INITIALIZER;
pthread_cond_t con_cond=PTHREAD_COND_INITIALIZER;

void print_produced(int num, int master) { //생성한 숫자를 출력하는 함수
	printf("Produced %d by master %d\n", num, master);
}

void print_consumed(int num, int worker) { //소비한 숫자를 출력하는 함수
	printf("Consumed %d by worker %d\n", num, worker);
}

void *generate_requests_loop(void *data){ //숫자를 생성할 함수
	int thread_id = *((int *)data);

	pthread_mutex_lock(&gen_mutex);
	pthread_cond_wait(&gen_cond,&gen_mutex); //쓰레드가 생성되면 생성 mutex를 lock 후 wait

	while(1){ //전체 아이템 수 보다 적게 생산한 경우 진행
		if(item_to_produce>=total_items){ //숫자를 전부 생산한 경우 생산 mutex를 unlock 후 워커, 마스터 쓰레드를 차례로 호출
			pthread_mutex_unlock(&gen_mutex);
			pthread_cond_signal(&con_cond);
			pthread_cond_signal(&gen_cond);
			break;
		}

		if(buffer[item_to_produce%max_buf_size]!=-1){ //버퍼에 들어갈 자리가 비어 있지 않은 경우 워커 쓰레드 호출 후 wait
			pthread_cond_signal(&con_cond);
			pthread_cond_wait(&gen_cond,&gen_mutex);
			continue;
		}
		
		buffer[item_to_produce%max_buf_size] = item_to_produce; //버퍼에 생성한 숫자를 삽입	
		print_produced(item_to_produce++, thread_id); //생산 확인 함수 호출

	}
}

void *consume_requests_loop(void *data){ //생성한 숫자를 소비할 함수
	int thread_id = *((int *)data);

	pthread_mutex_lock(&con_mutex); //쓰레드가 생성되면 소비 mutex를 lock 후 wait
	pthread_cond_wait(&con_cond,&con_mutex);
	
	while(1){ //전체 아이템 수 보다 적게 소비한 경우 진행
		if(item_to_consume>=total_items){ //숫자를 모두 소비한 경우 소비 mutex를 unlock 후에 다른 워커 쓰레드를 호출
			pthread_mutex_unlock(&con_mutex);
			pthread_cond_signal(&con_cond);
			break;
		}
		
		if(item_to_produce==item_to_consume){ //버퍼가 비어 있는 경우 마스터 쓰레드 호출 후 wait
			pthread_cond_signal(&gen_cond);
			pthread_cond_wait(&con_cond,&con_mutex);
			continue;
		}

		print_consumed(buffer[item_to_consume%max_buf_size],thread_id); //소비 확인 함수 호출 
		buffer[item_to_consume++%max_buf_size]=-1; //소비한 버퍼 자리에 -1 저장 (비어 있다는 의미)
	}
}

int main(int argc, char *argv[]){
	int *master_thread_id;	
	int *worker_thread_id;
	int i;
	pthread_t *master_thread;
	pthread_t *worker_thread;

	if(argc < 5) { //input 체크
		printf("./master-worker #total_items #max_buf_size #num_workers #masters e.g. ./exe 10000 1000 4 3\n");
		exit(1);
	}
	else {
		num_masters = atoi(argv[4]); //master thread 개수
		num_workers = atoi(argv[3]); //worker thread 개수
		total_items = atoi(argv[1]); //생성할 숫자 개수
		max_buf_size = atoi(argv[2]); //버퍼의 크기
	}

	//버퍼 생성 및 모든 항목을 -1로 초기화
	buffer = (int *)malloc (sizeof(int) * max_buf_size);
	memset(buffer,-1,sizeof(int)*max_buf_size);

	//숫자를 생성할 마스터 쓰레드 생성
	master_thread_id = (int *)malloc(sizeof(int) * num_masters);
	master_thread = (pthread_t *)malloc(sizeof(pthread_t) * num_masters);

	for(i=0;i<num_masters;i++)
		master_thread_id[i] = i;

	for(i=0;i<num_masters;i++)
		pthread_create(&master_thread[i], NULL, generate_requests_loop, (void *)&master_thread_id[i]);

	usleep(10000);
	
	//생성한 숫자를 소비할 워커 쓰레드 생성
	worker_thread_id = (int *)malloc(sizeof(int) * num_workers );
	worker_thread = (pthread_t *)malloc(sizeof(pthread_t) * num_workers);

	for(i=0;i<num_workers;i++)
		worker_thread_id[i]=i;

	for(i=0;i<num_workers;i++)
		pthread_create(&worker_thread[i], NULL, consume_requests_loop, (void *)&worker_thread_id[i]);
	
	usleep(10000);

	pthread_cond_signal(&gen_cond); //한 개의 마스터 쓰레드에 신호를 보냄

	for(i=0;i<num_workers;i++){ //워커 쓰레드 종료
		pthread_join(worker_thread[i],NULL);
		printf("worker %d joined\n",i);
	}
	for(i = 0; i < num_masters; i++){ //마스터 쓰레드 종료
		pthread_join(master_thread[i], NULL);
		printf("master %d joined\n", i);
	}
	
	//mutex 및 cond 소멸
	pthread_mutex_destroy(&gen_mutex);
	pthread_mutex_destroy(&con_mutex);
	pthread_cond_destroy(&gen_cond);
	pthread_cond_destroy(&con_cond);

	//메모리 해제
	free(buffer);
	free(master_thread_id);
	free(master_thread);
	free(worker_thread_id);
	free(worker_thread);
	return 0;
}
