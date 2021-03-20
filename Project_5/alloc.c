#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "alloc.h"

struct memory_manager{ //메모리 관리자 구조체와 그 배열 선언
	char *start;
	int size;
	int activate;
}mem_arr[PAGESIZE/MINALLOC];

char* addr; //mapping된 주소의 처음을 가리키는 변수
int end; //배열의 마지막 부분을 지정하는 변수

int find_spare(int);
void coalescing(int,int);

//mmap을 실행하고 정보를 초기화하는 함수
int init_alloc(){
	addr=(char*)mmap(0,PAGESIZE,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_ANONYMOUS|MAP_PRIVATE,-1,0);
	if(addr==(void*)-1){
		return 1;
	}
	end=-1;
	return 0;
}

//munmap을 실행하고 메모리 관리자 배열을 초기화하는 함수
int cleanup(){
	int i,cleanup_result;
	//char buf[256];
	//sprintf(buf,"ps -u | grep %d",getpid());
	for(i=0;i<PAGESIZE/MINALLOC;i++)
		memset(mem_arr[i].start,0,mem_arr[i].size);
	/*puts("\n*****munmap 직전*****");
	system(buf);
	printf("\n");*/
	cleanup_result=munmap(addr,PAGESIZE); //munmap 실행
	/*puts("*****munmap 직후*****");
	system(buf);*/
	if(cleanup_result!=0){ //munmap 결과에 따른 리턴
		return 1;
	}
	else
		return 0;
}

//입력받은 크기만큼 메모리를 할당 후 시작 주소를 리턴하는 함수
char *alloc(int num){
	int pos;
	//요청 크기가 8바이트 배수가 아니거나 페이지 크기보다 크커나 1보다 작은 경우 null 리턴
	if(num<=0||num%MINALLOC||num>PAGESIZE){ 
		return NULL;
	}
	//공간 부족으로 인하여 alloc에 실패한 경우 null 리턴
	if((pos=find_spare(num))==-1){ 
		return NULL;
	}
	return mem_arr[pos].start; //할당 시작 주소 리턴
}

//입력받은 주소를 찾아 할당을 해제하는 함수
void dealloc(char *target_addr){
	int i;
	//입력받은 주소의 chunk 위치를 찾아서 memory 초기화
	for(i=0;i<=end;i++){ 
		if(mem_arr[i].start==target_addr){
			memset(target_addr,0,mem_arr[i].size);
			mem_arr[i].activate=0;
			break;
		}
	}
	if(i>end){
		fprintf(stderr,"invalid addr!\n");
		exit(1);
	}
	if(i>0){
		if(mem_arr[i-1].activate==0){ //왼쪽이 free chunk인 경우 coalescing
			coalescing(i-1,i);
		}
	}

	if(i<end){
		if(mem_arr[i+1].activate==0){ //오른쪽이 free chunk인 경우 coalescing
			coalescing(i,i+1);
		}
	}
}

//alloc할 자리를 찾아주는 함수
int find_spare(int size){
	int i,max_diff=-1,max_diff_index=-1;

	//구조체 배열을 탐색
	for(i=0;i<=end;i++){
		//이미 할당된 chunk인 경우, 빈 자리여도 size가 작은 chunk의 경우 continue
		if(mem_arr[i].activate||size>mem_arr[i].size) 
			continue;
		//worst fit 적용
		if(max_diff<mem_arr[i].size-size){
			max_diff=mem_arr[i].size-size;
			max_diff_index=i;
		}
	}
	
	//삽입에 적합한 chunk가 없는 경우
	if(max_diff_index==-1){
		
		//더 이상 chunk 추가가 불가능 하다면 -1 리턴
		if(end>=0&&mem_arr[end].start+mem_arr[end].size+size>addr+PAGESIZE)
			return -1;
		end++;
		//메모리 관리자 배열에서의 삽입 chunk 정보 저장
		if(end==0)
			mem_arr[0].start=addr;
		else
			mem_arr[end].start=mem_arr[end-1].start+mem_arr[end-1].size;
		mem_arr[end].size=size;
		mem_arr[end].activate=1;
		return end; 
	}

	//빈 자리에 삽입하는 경우
	if(max_diff>0){ //단편화가 생기는 경우
		for(i=end+1;i>max_diff_index+1;i--){
			mem_arr[i]=mem_arr[i-1];
		}

		//free chunk 정보 업데이트
		mem_arr[max_diff_index+1].start=mem_arr[max_diff_index].start+size;
		mem_arr[max_diff_index+1].size=max_diff;
		mem_arr[max_diff_index+1].activate=0;

		end++;
	}
		
	//input chunk 정보 업테이트
	mem_arr[max_diff_index].size=size;
	mem_arr[max_diff_index].activate=1;
	
	return max_diff_index;

}

//두 free chunk를 병합하는 함수
void coalescing(int index1,int index2){
	int i;
	mem_arr[index1].size+=mem_arr[index2].size;
	for(i=index2;i<end;i++){
		mem_arr[i]=mem_arr[i+1];
	}
	end--;
}
