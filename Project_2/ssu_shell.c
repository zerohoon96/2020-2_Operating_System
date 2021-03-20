#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_INPUT_SIZE 1024
#define MAX_TOKEN_SIZE 64
#define MAX_NUM_TOKENS 64

int tokenNo,commandCnt; //토큰의 개수, 명령어의 개수를 저장하는 전역변수

char **tokenize(char *);
int checkTokens(char **,int *);
void getCommands(char ***,char **,int *);
void execCommands(char ***);
void freeTokens(char **,int *);

int main(int argc, char* argv[]) {
	char line[MAX_INPUT_SIZE];            
	char ***commands,**tokens;
	int *tokenInfo;
	int i,j,index=0;
	FILE* fp;
	long end;

	if(argc==2){ //배치식 모드로 입력되었다면 입력된 파일을 fopen
		fp=fopen(argv[1],"r");
		if(fp<0){
			printf("File doesn't exists.");
			return -1;
		}
		fseek(fp,0,SEEK_END); //파일의 마지막 위치를 저장
		end=ftell(fp);
		fseek(fp,0,SEEK_SET);
	}
	
	while(1){ //종료 전까지 계속 반복
		commandCnt=1;
		index=0;
		bzero(line, sizeof(line));
		if(argc==2){ //배치식 모드. 파일 끝에 도달하는 경우 종료
			if(fgets(line,sizeof(line),fp)==NULL||ftell(fp)>end){
				break;	
			}
			line[strlen(line) - 1] = '\0';
		}
		else { //대화식 모드. 프롬프트는 $이고, 엔터를 입력할 때까지 입력받음
			printf("$ ");
			scanf("%[^\n]", line);
			getchar();
		}

		line[strlen(line)] = '\n';
		tokens=tokenize(line); //사용자가 입력한 명령을 공백을 기준으로 자르고, 그 결과를 리턴받음
		tokenInfo=(int*)malloc(sizeof(int)*tokenNo); //토큰 정보 배열 메모리 할당
		memset(tokenInfo,0,sizeof(int)*tokenNo);

		if(!checkTokens(tokens,tokenInfo)){ //명령어 오류를 검사함과 동시에 명령어 개수까지 구함
			freeTokens(tokens,tokenInfo);		
			continue;
		}

		commands=(char***)malloc(sizeof(char**)*commandCnt); //파이프를 기준으로 명령어를 분리해서 저장하기 위한 명령어 메모리 할당
		for(i=0;i<commandCnt;i++){
			commands[i]=(char**)malloc(sizeof(char*)*MAX_NUM_TOKENS);
		}
		getCommands(commands,tokens,tokenInfo); //명령어를 파이프 기준으로 나누어서 저장

		close(2);
		execCommands(commands); //각 명령어를 실행

		freeTokens(tokens,tokenInfo); //토큰 메모리 해제
	}
	return 0;
}

char **tokenize(char *line){ //문자열을 공백 기준으로 나누고 나눈 토큰 배열을 리턴하는 함수
	char **tokens = (char **)malloc(MAX_NUM_TOKENS * sizeof(char *));
	char *token = (char *)malloc(MAX_TOKEN_SIZE * sizeof(char));
	int i,tokenIndex = 0;
	tokenNo=0;
	for(i =0; i < strlen(line); i++){ //사용자의 명령 길이만큼 반복

		char readChar = line[i];

		if (readChar == ' ' || readChar == '\n' || readChar == '\t'){ //한 토큰이 끝나는 경우
			token[tokenIndex] = '\0';
			if (tokenIndex != 0){ //읽은 토큰이 있는 경우에만 token을 tokens 배열에 삽입
				tokens[tokenNo] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
				strcpy(tokens[tokenNo++], token);
				tokenIndex = 0; 
			}
		}
		else { //한 토큰이 끝날 때까지 token에 저장
			token[tokenIndex++] = readChar;
		}
	}

	free(token);
	tokens[tokenNo] = NULL ;
	return tokens;
}

int checkTokens(char **tokens,int *tokenInfo){ //토큰의 파이프 확인과 동시에 올바른지 여부 체크
	if(tokens[0]==NULL) //토큰이 비어있으면 다음 명령어를 받음
		return 0;
	for(int i=0;tokens[i]!=NULL;i++){ //파이프의 위치에 따라서 오류 검사
		if(strcmp(tokens[i],"|")==0){
			commandCnt++;
			if(i==0||i==tokenNo-1){ //오류인 경우 에러메세지 출력하고 0을 리턴
				printf("SSUShell : Incorrect command\n");
				return 0;
			}
			tokenInfo[i]=1;
		}
	}
	return 1; //성공한 경우 1 리턴
}

void getCommands(char ***commands,char **tokens,int *tokenInfo){ //토큰을 검사해서 파이프를 기준으로 명령어를 분리하는 함수
	int i,index1=0,index2=0;

	for(i=0;i<tokenNo;i++){ //전체 토큰에 대해서 검사
		if(strcmp(tokens[i],"|")==0){ //파이프인 경우 다음 토큰으로 이동
			index1++;
			index2=0;
			continue;
		}
		commands[index1][index2]=(char*)malloc(sizeof(char)*MAX_INPUT_SIZE); //배열 메모리 할당 후 토큰을 저장
		commands[index1][index2++]=tokens[i];
	}
}

void execCommands(char ***commands){ //명령어를 실행하는 함수
	int count=0,tmp=0; 
	int fd[2];
	int status=0;

	while(count<commandCnt){ //모든 명령어를 실행할 때까지 반복
		if(pipe(fd)==-1){ //파이프 생성
			fprintf(stderr,"pipe error\n");
			exit(1);
		}
		switch(fork()){ //명령어 실행을 위한 자식 프로세스 생성
			case -1:
				fprintf(stderr,"fork error\n");
				exit(1);
			case 0: //자식 프로세스인 경우
				dup2(tmp,0); //임시저장한 파이프로 표준입력을 변경
				if(count+1!=commandCnt){ //마지막 명령어에 도달하지 않은 경우 출력파이프로 표준입력을 변경 
					dup2(fd[1],1);
				}
				close(fd[0]); //입력 파이프를 close
				if(execvp(commands[count][0],commands[count])==-1){ //명령어를 실행하고, 실패한 경우 프로세스 종료 
					exit(1);
				}
				break;
			default: //부모 프로세스인 경우
				wait(&status); //자식 프로세스가 종료될 때까지 대기
				if(status!=0){ //정상 실행이 아닌 경우 에러메세지를 띄우고 함수 종료
					printf("SSUShell : Incorrect command\n");
					return;
				}
				close(fd[1]); //출력 파이프를 close
				tmp=fd[0]; //입력 파이프를 임시저장
				count++; //다음 명령어로 위치 이동
		}
	}
}


void freeTokens(char **tokens,int *tokenInfo){ //토큰의 메모리를 해제하는 함수
	int i;
	for(i=0;tokens[i]!=NULL;i++) //tokens 메모리 해제
		free(tokens[i]);
	free(tokens);
	free(tokenInfo); //tokenInfo 메모리 해제
}




