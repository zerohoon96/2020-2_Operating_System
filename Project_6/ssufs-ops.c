#include "ssufs-ops.h"

extern struct filehandle_t file_handle_array[MAX_OPEN_FILES];

void ssufs_readSuperBlock(struct superblock_t*);

int ssufs_allocFileHandle() {
	for(int i = 0; i < MAX_OPEN_FILES; i++) {
		if (file_handle_array[i].inode_number == -1) {
			return i;
		}
	}
	return -1;
}

//1.입력받은 이름의 파일을 생성하는 함수
int ssufs_create(char *filename){
	int new_inode,i;
	struct inode_t *tmp = (struct inode_t*)malloc(sizeof(struct inode_t));
	
	//파일 이름 중복 검사
	for(i=0;i<MAX_FILES;i++){
		ssufs_readInode(i,tmp);
		if(tmp->status==INODE_FREE)
			continue;
		if(!strcmp(tmp->name,filename)){
			free(tmp);
			return -1;
		}
	}

	//파일 추가 가능 여부 검사
	if((new_inode=ssufs_allocInode())==-1){
		free(tmp);
		return -1;
	}

	//inode 구조체 초기화
	tmp->status = INODE_IN_USE;
	strcpy(tmp->name,filename);
	tmp->file_size = 0;
	memset(tmp->direct_blocks,-1,sizeof(int)*MAX_FILE_SIZE);
	
	//inode 구조체 write
	ssufs_writeInode(new_inode,tmp);

	free(tmp);
	return new_inode;
}

//2.입력받은 이름의 파일을 삭제하는 함수 
void ssufs_delete(char *filename){
	int target_inode,i=0;
	struct inode_t *tmp=(struct inode_t*)malloc(sizeof(struct inode_t));

	//존재하는 파일인지 검사, inode 번호 저장
	for(i=0;i<MAX_FILES;i++){
		ssufs_readInode(i,tmp);
		if(!strcmp(tmp->name,filename)){
			target_inode=i;
			break;
		}
	}
	if(i==MAX_FILES){
		free(tmp);
		return;
	}

	//파일이 열려 있는 경우 해당 인덱스의 파일 핸들 초기화
	for(i=0;i<MAX_OPEN_FILES;i++){
		if(file_handle_array[i].inode_number == target_inode){
			ssufs_close(i);
		}
	}

	//삭제 함수 호출
	ssufs_freeInode(target_inode);

	free(tmp);
}

//3.입력받은 이름의 파일을 여는 함수 
int ssufs_open(char *filename){
	int target_inode,index=-1;

	//새로운 파일 핸들 인덱스 얻음
	if((index=ssufs_allocFileHandle())==-1)
		return -1;

	//존재하는 파일인지 검사, open 후 inode 번호 저장
	if((target_inode=open_namei(filename))==-1)
		return -1;
	

	//파일 핸들 구조체 초기화
	file_handle_array[index].inode_number=target_inode;
	file_handle_array[index].offset=0;

	return index;
}

void ssufs_close(int file_handle){
	file_handle_array[file_handle].inode_number = -1;
	file_handle_array[file_handle].offset = 0;
}

//4.open된 파일의 현재 오프셋에서 요청된 buf로 요청된 nbytes 수를 읽음
int ssufs_read(int file_handle, char *buf, int nbytes){
	int tmp_offset,tmp_inode,curr_block,tmp_size,size,i,diff,count=0;
	char *tmp_buf;
	struct inode_t * inode_info=(struct inode_t*)malloc(sizeof(struct inode_t));
	
	//오류 검사, 현 file handle의 offset과 inode번호 저장
	if((tmp_inode=file_handle_array[file_handle].inode_number)==-1){
		fprintf(stderr,"ssufs_write : inode error\n");
		free(inode_info);
		return -1;
	}

	//정보 저장
	tmp_offset=file_handle_array[file_handle].offset;
	curr_block=tmp_offset/BLOCKSIZE;
	size=nbytes;
	ssufs_readInode(tmp_inode,inode_info);
	tmp_size=inode_info->file_size;

	//읽었을 때에 파일 끝을 넘어가는지 검사
	if(tmp_offset+nbytes>tmp_size){
		free(inode_info);
		return -1;
	}

	//nbytes 만큼 read
	for(i=curr_block;i<curr_block+MAX_FILE_SIZE;i++){
		//현재 data 블록을 읽어서 저장
		tmp_buf=(char*)malloc(sizeof(char)*BLOCKSIZE);
		memset(tmp_buf,0,BLOCKSIZE);
		ssufs_readDataBlock(inode_info->direct_blocks[i],tmp_buf);

		//read할 크기 구하기
		diff=BLOCKSIZE-tmp_offset%BLOCKSIZE;
		if(nbytes/BLOCKSIZE==0&&diff>nbytes%BLOCKSIZE)
			diff=nbytes%BLOCKSIZE;
		strncpy(buf,tmp_buf+tmp_offset%BLOCKSIZE,diff);
		//printf("\n\nbuf : %s\n",buf);

		//정보 업데이트
		buf+=diff;
		nbytes-=diff;
		tmp_offset+=diff;
		free(tmp_buf);
		//printf("읽은 크기 : %d\t남은 크기 : %d\ttmp_offset : %d\n",diff,nbytes,tmp_offset);
		
		//모두 읽은 경우 종료
		if(nbytes==0){
			break;
		}
	}

	ssufs_lseek(file_handle,size);
	free(inode_info);
}

//5.open된 파일의 현재 오프셋부터 요청된 buf에서 요청된 nbytes 수를 디스크에 씀
int ssufs_write(int file_handle, char *buf, int nbytes){
	int tmp_offset,tmp_inode,tmp_size,size=nbytes;
	int need_blocks=0, diff=0,count=0,i,curr_block=0,new_start=MAX_FILE_SIZE,access_time;
	char *tmp_buf;
	struct inode_t * inode_info=(struct inode_t*)malloc(sizeof(struct inode_t));
	struct superblock_t *super_info=(struct superblock_t*)malloc(sizeof(struct superblock_t));
	
	//오류 검사, 현 file handle의 offset과 inode번호 저장
	if((tmp_inode=file_handle_array[file_handle].inode_number)==-1){
		fprintf(stderr,"ssufs_write : inode error\n");
		free(inode_info);
		free(super_info);
		return -1;
	}
	tmp_offset=file_handle_array[file_handle].offset;

	//inode 구조체와 superblock 구조체를 읽어서 저장
	ssufs_readInode(tmp_inode,inode_info);
	ssufs_readSuperBlock(super_info);
	tmp_size=inode_info->file_size;

	//할당 필요한 블록의 개수를 저장
	for(i=0;i<MAX_FILE_SIZE;i++){ //현재 파일에서 사용중인 블럭의 수 구함
		if(inode_info->direct_blocks[i]!=-1)
			count++;
	}
	need_blocks=(tmp_offset+nbytes)/BLOCKSIZE-count+1;
	if((tmp_offset+nbytes)%BLOCKSIZE==0)
		need_blocks--;
	if(need_blocks<0)
		need_blocks=0;

	//처음으로 할당이 시작되는 블럭을 저장
	if(need_blocks>0){
		if(tmp_offset%BLOCKSIZE)
			new_start=tmp_offset/BLOCKSIZE+1;
		else
			new_start=tmp_offset/BLOCKSIZE;
	}

	//블록 접근 횟수 저장
	access_time=(tmp_offset+nbytes)/BLOCKSIZE-tmp_offset/BLOCKSIZE+1;
	if((tmp_offset+nbytes)%BLOCKSIZE==0)
		access_time--;
	count=0;
	//printf("need block : %d\taccess time : %d\n",need_blocks,access_time);
	
	//블록을 추가로 할당하는 경우 할당이 가능한지 검사
	if(need_blocks>0){
		
		//파일 당 최대 블록 개수 초과 여부 검사
		for(i=0;i<MAX_FILE_SIZE;i++){
			if(inode_info->direct_blocks[i]!=-1)
				count++;
		}
		//printf("쓰고 있는 개수 : %d\n",count);
		if(count+need_blocks>MAX_FILE_SIZE){
			free(inode_info);
			free(super_info);
			return -1;
		}
		count=0;

		//free 데이터 블록이 필요만큼 있는지 검사
		for(i=0;i<NUM_DATA_BLOCKS;i++){
			if(super_info->datablock_freelist[i]==DATA_BLOCK_USED)
				count++;
		}
		if(count+need_blocks>NUM_BLOCKS){
			free(inode_info);
			free(super_info);
			return -1;
		}
		count=0;

		//할당이 가능한 경우 할당
		for(i=0;i<MAX_FILE_SIZE;i++){
			if(count==need_blocks)
				break;
			if(inode_info->direct_blocks[i]==-1){
				inode_info->direct_blocks[i]=ssufs_allocDataBlock();
				count++;
			}
		}
	}

	//write 실행
	curr_block=tmp_offset/BLOCKSIZE;
	for(i=curr_block;i<curr_block+access_time;i++){
		tmp_buf=(char*)malloc(sizeof(char)*BLOCKSIZE);
		memset(tmp_buf,0,BLOCKSIZE);
		
		//기존에 존재하는 블록의 경우 블록을 읽어서 저장
		if(i<new_start)
			ssufs_readDataBlock(inode_info->direct_blocks[i],tmp_buf); 
		
		//write할 크기를 저장
		diff=BLOCKSIZE-tmp_offset%BLOCKSIZE;
		if(nbytes/BLOCKSIZE==0) //남은 크기가 한 블럭보다 작은 경우
			diff=nbytes%BLOCKSIZE;
		//printf("\n\ndiff : %d\n\n",diff);
		
		//올바른 위치에 write
		strncpy(tmp_buf+tmp_offset%BLOCKSIZE,buf,diff);
		ssufs_writeDataBlock(inode_info->direct_blocks[i],tmp_buf);
		
		//정보 업데이트
		buf+=diff;
		nbytes-=diff;
		tmp_offset+=diff;

		free(tmp_buf);
	}

	//printf("\n\ntmp_offset : %d\n\n",tmp_offset);
	
	//inode 구조체 업데이트
	if(tmp_size<tmp_offset)
		inode_info->file_size=tmp_offset;
	ssufs_writeInode(tmp_inode,inode_info);
	
	//file handle의 오프셋 값 갱신
	ssufs_lseek(file_handle,size);

	free(inode_info);
	free(super_info);
	return 0;
}

int ssufs_lseek(int file_handle, int nseek){
	int offset = file_handle_array[file_handle].offset;

	struct inode_t *tmp = (struct inode_t *) malloc(sizeof(struct inode_t));
	ssufs_readInode(file_handle_array[file_handle].inode_number, tmp);
	
	int fsize = tmp->file_size;
	
	offset += nseek;

	if ((fsize == -1) || (offset < 0) || (offset > fsize)) {
		free(tmp);
		return -1;
	}

	file_handle_array[file_handle].offset = offset;
	free(tmp);

	return 0;
}
