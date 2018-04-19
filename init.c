#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define max_cmd_num 10
#define max_cmd_length 64
#define max_input_length 256
#define max_path_length 64

#define CONTINUE 1
#define EXIT 0

typedef struct dir{
	int direct[3];    // 0 for < , 1 for > , 2 for >>
    char in_path[64],out_path[64],out_plus_path[64];
}redirect;
redirect redir;

char *args[128];
int cmd_start_at[max_cmd_num]={0};

int exe_cmd(int num);

int main(){
    char cmd[max_input_length];
    int cmd_num=0;
    
    while(1){
    	// refresh
    	int i,j;
    	for(j=0;j<3;j++)
    		redir.direct[j]=0;
    	cmd_num=0;
    	
    	
    	/* start here*/
        printf("# ");
        fflush(stdin);
        fgets(cmd,256,stdin);
        /* 清理结尾的换行符 */
        for (i = 0; cmd[i] != '\n'; i++);
        cmd[i] = '\0';
        /*find < > >> and replace path & <> with blank ' ' */
        char *p;
        char *temp;
        int path_length=0;
        p=cmd;
        while (*p!='\0'){
        	if(*p == '<'){
        		redir.direct[0]=1;
        		path_length = 0;
        		temp = p+1;
        		while(*temp == ' ') temp++;
        		while(*temp != ' ' || *temp != '>' || *temp !='|' || *temp !='<' || *temp !='\0')
        			redir.in_path[path_length++]=*temp;
        		redir.in_path[path_length]='\0';
        		while(p != temp){
        			*p = ' ';
        			p++;
        		}
        		continue;
        	}
        	if(*p == '>'){
        		if(*(p+1) == '>'){
        			redir.direct[2] = 1;
        			path_length = 0;
        			temp = p+2;
        			while(*temp == ' ') temp++;
        			while(*temp != ' ' || *temp != '>' || *temp !='|' || *temp !='<' || *temp !='\0')
        				redir.out_plus_path[path_length++]=*temp;
        			redir.out_plus_path[path_length]='\0';
        			while(p!=temp){
        				*p = ' ';
        				p++;
        			}
        		}
        		else{
        			redir.direct[1] = 1;
        			path_length = 0;
        			temp = p+1;
        			while(*temp == ' ') temp++;
        			while(*temp != ' ' || *temp != '>' || *temp !='|' || *temp !='<' || *temp !='\0')
        				redir.out_path[path_length++]=*temp;
        			redir.out_path[path_length]='\0';
        			while(p!=temp){
        				*p = ' ';
        				p++;
        			}
        		}
        		continue;
        	}
        	/*if(*p == '|'){
        		cmd_num++;
        	}*/
        	p++;
        }   
        /* 拆解命令行 */
        args[0] = cmd;
        for (i = 0; *args[i]; i++)
            for (args[i+1] = args[i] + 1; *args[i+1]; args[i+1]++)
		        if(*args[i+1] == ' '){                
		            while (*args[i+1] == ' ') {
                        *args[i+1] = '\0';
                        args[i+1]++;
                    }
		            break;
		        }
        args[i] = NULL;
        for(i=0;args[i]!=NULL;i++){
        	if(*args[i]=='|'){
        		cmd_start_at[++cmd_num]=i+1;
        		args[i]=NULL;
        	}
        }
        /* 拆解命令行 completed ! */
        
        /*start execute cmd*/
        pid_t pid;
        /* only one cmd    no pipe */
        if(cmd_num == 0){
        	/* no redirect */
        	if(redir.direct[0] == redir.direct[1] == redir.direct[2] == 0){
			int return_value=exe_cmd(0);
        		if(return_value == EXIT) return 0;
        		else if(return_value == 255) return -1;
        		else continue;
        	}
        	/* redirect */
        	else {
        		pid = fork();
        		if(pid == 0){
        			if(redir.direct[0] == 1){
        				int in = open(redir.in_path,O_RDONLY);
        				dup2(in,STDIN_FILENO);
        			}
        			if(redir.direct[1] == 1){
        				int out = open(redir.out_path,O_CREAT | O_WRONLY);
        				dup2(out,STDOUT_FILENO);
        			}
        			else
        				if(redir.direct[2] == 1){
        					int out_plus = open(redir.out_plus_path,O_CREAT | O_WRONLY | O_APPEND);
        					dup2(out_plus,STDOUT_FILENO);
        				}
        			exe_cmd(0);
        			_exit(0);
        		}
        		else{
        			wait(NULL);
        			continue;
        		}
        	}
        }
        /* there are pipes */
        else{
        	int pipefd[2];
        	pipe(pipefd);
        	pid = fork();
        	if(pid == 0){
        		close(pipefd[0]);
        		dup2(pipefd[1],STDOUT_FILENO);
        		close(pipefd[1]);
        		if(redir.direct[0] == 1){
        				int in = open(redir.in_path,O_RDONLY);
        				dup2(in,STDIN_FILENO);
        		}
        		exe_cmd(0);
        		_exit(0);
        	}
        	else{
        		close(pipefd[1]);
        		int prev_pipe_read = pipefd[0];
        		for(i=1;i<=cmd_num;i++){
        			pipe(pipefd);
        			pid = fork();
        			if(pid == 0){
        				close(pipefd[0]);
        				dup2(prev_pipe_read,STDIN_FILENO);
        				if(i != cmd_num){
        					dup2(pipefd[1],STDOUT_FILENO);
        					close(pipefd[1]);
        					if(redir.direct[1] == 1){
        						int out = open(redir.out_path,O_CREAT | O_WRONLY);
        						dup2(out,STDOUT_FILENO);
        					}
        					else
        						if(redir.direct[2] == 1){
        							int out_plus = open(redir.out_plus_path,O_CREAT | O_WRONLY | O_APPEND);
        							dup2(out_plus,STDOUT_FILENO);
        						}
        				}
        				exe_cmd(i);
        				_exit(0);
        			}
        			else{
        				close(pipefd[1]);
        				prev_pipe_read = pipefd[0];
        			}
        		}
        	}
			for(i=0;i<=cmd_num;i++)
        			wait(NULL);
        }
    }
}

int exe_cmd(int num) {
        /* 没有输入命令 */
        if (!args[cmd_start_at[num]])
            return CONTINUE;

        /* 内建命令 */
        if (strcmp(args[cmd_start_at[num]], "cd") == 0) {
            if (args[cmd_start_at[num]+1])
                chdir(args[cmd_start_at[num]+1]);
            return CONTINUE;
        }
        if (strcmp(args[cmd_start_at[num]], "pwd") == 0) {
            char wd[4096];
            puts(getcwd(wd, 4096));
            return CONTINUE;
        }
        if(strcmp(args[cmd_start_at[num]], "export") == 0){
        	char *tmp=args[cmd_start_at[num]+1];
        	while(*tmp!='=' && *tmp) tmp++;
        	if(*tmp!='=') return CONTINUE;
        	*tmp='\0';
		tmp ++;
        	setenv(args[cmd_start_at[num]+1],tmp,1);
        	return CONTINUE; 
        }
        /*if(strcmp(args[cmd_start_at[num]], "echo") == 0){
        	if(echo(args+cmd_start_at[num]))
        		return CONTINUE;
        }*/
        if (strcmp(args[cmd_start_at[num]], "exit") == 0)
            return EXIT;
		
        /* 外部命令 */
        pid_t pid = fork();
        if (pid == 0) {
            /* 子进程 */
            execvp(args[cmd_start_at[num]], args+cmd_start_at[num]);
            /* execvp失败 */
            return 255;
        }
        /* 父进程 */
        wait(NULL);
        return CONTINUE;
}
