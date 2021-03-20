#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "ealloc.h"

#define ALL_PAGES 4	
struct memory_manager{ //메모리 관리자 배열 선언
	char *start;
	int size;
	int activate;
}mem_arr[ALL_PAGES][PAGESIZE/MINALLOC];

char* addr[ALL_PAGES]; //각 페이지의 mapping된 주소 처음을 가리키는 변수
int end[ALL_PAGES]; //각 페이지의 마지막 chunk를 저장하는 변수
int page_index,chunk_index; //alloc 함수에서 찾은 page,chunk 정보를 저장하는 변수
int max_page_num;

int find_spare(int);
void coalescing(int,int,int);

//최초 정보를 초기화하는 함수
void init_alloc(){
	page_index=-1;
	chunk_index=-1;
	max_page_num=-1;
	//for(int i=0;i<4;i++)end[i]=-1;
	memset(end,-1,sizeof(int)*ALL_PAGES);
}

//모든 페이지를 초기화하는 함수
void cleanup(){
	int i,j;
	for(i=0;i<ALL_PAGES;i++){
		for(j=0;j<PAGESIZE/MINALLOC;j++)
			memset(mem_arr[i][j].start,0,mem_arr[i][j].size);
	}
}

//입력받은 크기반큼 메모리를 할당하는 함수
char *alloc(int num){ 
	//요청 크기가 256바이트 배수가 아니거나 페이지 크기보다 크거나 1보다 작은 경우 null 리턴
	if(num<=0||num%MINALLOC||num>PAGESIZE){
		return NULL;
	}

	//메모리 할당이 불가능한 경우 null 리턴
	if((find_spare(num))<0){
		return NULL;
	}
	return mem_arr[page_index][chunk_index].start;
}

void dealloc(char *target_addr){
	int i,j,finish=0;
	//현재 mapping된 최대 page까지의 모든 chunk를 탐색
	for(i=0;i<=max_page_num;i++){
		for(j=0;j<=end[i];j++){
			//요청 주소를 찾아서 메모리 초기화
			if(mem_arr[i][j].start==target_addr){
				memset(target_addr,0,mem_arr[i][j].size);
				mem_arr[i][j].activate=0;
				finish=1;
				break;
			}
		}
		if(finish)
			break;
	}

	if(!finish){ //해당 주소가 할당되어 있지 않은 경우 종료
		fprintf(stderr,"invalid addr!\n");
		exit(1);
	}

	
	if(j>0){
		if(mem_arr[i][j-1].activate==0){ //왼쪽이 free chunk인 경우 coalescing
			coalescing(i,j-1,j);
		}
	}

	if(j<end[i]){
		if(mem_arr[i][j+1].activate==0){ //오른쪽이 free chunk인 경우 coalescing
			coalescing(i,j,j+1);
		}
	}
}

//alloc할 자리를 찾아주는 함수
int find_spare(int size){ 
	int i,j;
	int max_diff=-1,max_diff_page=-1,max_diff_chunk=-1;

	//구조체 배열을 탐색
	for(i=0;i<=max_page_num;i++){
		for(j=0;j<=end[i];j++){
			//이미 할당된 chunk인 경우, 빈 자리여도 size가 작은 chunk인 경우 continue
			if(mem_arr[i][j].activate||size>mem_arr[i][j].size)
				continue;
			//worst fit 적용
			if(max_diff<mem_arr[i][j].size-size){
				max_diff=mem_arr[i][j].size-size;
				max_diff_page=i;
				max_diff_chunk=j;
			}
		}
	}

	//삽입에 적합한 chunk가 없는 경우
	if(max_diff_page==-1&&max_diff_chunk==-1){
		//mapping 필요 여부 검사(최초 삽입 or 해당 page에 남은 공간이 없음)
		if(max_page_num<0||mem_arr[max_page_num][end[max_page_num]].start+mem_arr[max_page_num][end[max_page_num]].size+size>addr[max_page_num]+PAGESIZE){
			//mapping 가능 여부 검사
			if(max_page_num+1>=ALL_PAGES)
				return -1;
			addr[max_page_num+1]=(char*)mmap(0,PAGESIZE,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_ANONYMOUS|MAP_PRIVATE,-1,0); //page mapping
			if(addr[max_page_num+1]==(void*)-1){
				fprintf(stderr,"mmap fail!\n");
				return -1;
			}

			max_page_num++;
		}

		end[max_page_num]++;

		//메모리 관리자 배열에서의 삽입 chunk 정보 저장
		if(end[max_page_num]==0) 
			mem_arr[max_page_num][0].start=addr[max_page_num];
		else
			mem_arr[max_page_num][end[max_page_num]].start=mem_arr[max_page_num][end[max_page_num]-1].start+mem_arr[max_page_num][end[max_page_num]-1].size;
		mem_arr[max_page_num][end[max_page_num]].size=size;
		mem_arr[max_page_num][end[max_page_num]].activate=1;

		//test program에 리턴할 정보 저장
		page_index=max_page_num;
		chunk_index=end[max_page_num];
		return 1;
	}

	//빈 자리에 삽입하는 경우
	if(max_diff>0){ //단편화가 생기는 경우
		for(i=end[max_diff_page]+1;i>max_diff_chunk+1;i--){
			mem_arr[max_diff_page][i]=mem_arr[max_diff_page][i-1];
		}

		//free chunk 정보 업데이트
		mem_arr[max_diff_page][max_diff_chunk+1].start=mem_arr[max_diff_page][max_diff_chunk].start+size;
		mem_arr[max_diff_page][max_diff_chunk+1].size=max_diff;
		mem_arr[max_diff_page][max_diff_chunk+1].activate=0;

		end[max_diff_page]++;
	}

	//input chunk 정보 업데이트
	mem_arr[max_diff_page][max_diff_chunk].size=size;
	mem_arr[max_diff_page][max_diff_chunk].activate=1;

	//test program에 리턴할 정보 저장
	page_index=max_diff_page;
	chunk_index=max_diff_chunk;
	return 1;

}

//두 free chunk를 병합하는 함수
void coalescing(int page_num,int index1,int index2){ 
	int i;
	mem_arr[page_num][index1].size+=mem_arr[page_num][index2].size;
	for(i=index2;i<end[page_num];i++){
		mem_arr[page_num][i]=mem_arr[page_num][i+1];
	}
	end[page_num]--;
}
