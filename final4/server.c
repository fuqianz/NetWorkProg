/********************************************************************/
/* Copyright (C) SSE-USTC, 2014-2015                                */
/*                                                                  */
/*  FILE NAME             :  server.c                               */
/*  PRINCIPAL AUTHOR      :  ZhangFuqiang                           */
/*  SUBSYSTEM NAME        :  server                                 */
/*  MODULE NAME           :  server                                 */
/*  LANGUAGE              :  C                                      */
/*  TARGET ENVIRONMENT    :  ANY                                    */
/*  DATE OF FIRST RELEASE :  2015/01/14                             */
/*  DESCRIPTION           :  a server                               */
/********************************************************************/

/*
 * Revision log:
 *
 * Created by ZhangFuqiang,2014/01/14
 *
 */

#include<pthread.h>
#include<netinet/in.h> 
#include<sys/types.h>  
#include<sys/socket.h>  
#include<stdio.h>  
#include<stdlib.h>  
#include<string.h>  

#include"trans_util.h"
#include"file_util.h"

#define DEFAULT_PORT 8899


void *thread_serve(void *t_info)
{
    struct thread_info *tinfo;
    tinfo = (struct thread_info *)t_info;

    int clientfd = tinfo->socketfd;
    int seekpos = tinfo->seekpos;
    char *filename = tinfo->filename;
    int size = tinfo->size;
    char buffer[TRANS_SIZE];
    
    memset(buffer,'\0',sizeof(buffer)); 
    //printf("ss %d  %s\n",size,filename);
    
    FILE *fp;

    if((fp = fopen(filename,"r")) == NULL)
    {
        printf("file is not exist!\n");
        return;
    } 

    printf("size %d seekpos %d\n",size,seekpos);

    int i;
    int n;
    int transferedsize = 0;
    for(i = 0 ; transferedsize != size ; i++) 
    { 
        fseek(fp,seekpos,SEEK_SET);
    
        struct trans_packet *tpack;
        tpack = (struct trans_packet*)malloc(sizeof(struct trans_packet));
      
        if(size > MAXSIZE)
        { 
            tpack->size = MAXSIZE;
        }
        else
        {
            tpack->size = size - i*MAXSIZE;
        }
       
        if(!tpack->size)
        {
            break; 
        } 

        if(fread(tpack->buffer,sizeof(char),tpack->size,fp) > 0)
        {        
            tpack->seekpos = seekpos;
            memcpy(buffer,tpack,sizeof(*tpack));
            n = send(clientfd,buffer,sizeof(buffer),0);        
            printf("size %d seekpos %d send %d bytes\n",tpack->size,tpack->seekpos,n);
        }
        else
        {
            printf("over!\n");
            break;
        }

        seekpos += tpack->size;
        transferedsize += tpack->size;        
    }  

    char resp[10];
    recv(clientfd,resp,10,0);
    if(!strcmp(resp,"ACK"))
    {
        printf("%d send sucess!\n",seekpos);
    }

    closefd(clientfd);
    fclose(fp);
}

void file_send(int clientfd,int serverfd,char *filename)
{
    struct trans_packet tpacket;
    struct sockaddr_in clientaddr;
    int thread_num;
    int filesize;
    int thread_size;

    filesize = getfilesize(filename);
    printf("filesize : %d\n",filesize);

    thread_num = filesize/MAXSIZE + 1;

    if(filesize <= 1024*1024*128)
    {
        thread_num = 1;      
    }    
    else
    {
        thread_num = filesize / 128*1024*1024;
        if(thread_num > 8) 
        {
            thread_num = 8;
        }
    }

    if(filesize <= 1024*1024*1024)
    {
        thread_size = THREAD_LOAD;
    }
    else
    {
        thread_size = (filesize+thread_num)/thread_num; 
    }


    int n;
    n = send(clientfd,(char*)&thread_num,4,0);
    printf("n %d tn %d\n",n,thread_num);

    pthread_t *ptd;
    ptd = (pthread_t *)malloc(thread_num*sizeof(pthread_t));

    int i,size = sizeof(clientaddr); 
   
    listen(serverfd,100);

    for(i = 0 ; i < thread_num ; i ++)
    {
        
        struct thread_info *tinfo;
   
        tinfo = (struct thread_info*)malloc(sizeof(struct thread_info));

        int newclientfd = accept(serverfd, (struct sockaddr*)&clientaddr,&size);
           
        strcpy(tinfo->filename,filename); 
        tinfo->seekpos = i*thread_size; 
        tinfo->socketfd = newclientfd;

        if((i+1)*thread_size > filesize)
        {
            tinfo->size = filesize - i * MAXSIZE;

            if(tinfo->size == 0)
            {
                break;
            } 
        } 
        else
        {
            tinfo->size = thread_size; 
        }

        if(pthread_create((ptd+i),NULL,thread_serve,(void*)tinfo) != 0)
        {
             printf("create thread %d ERROR\n",i);
        }
    }

    for(i = 0; i < thread_num; i++)
    {
        pthread_join(ptd[i], NULL);
    } 
     
}

int main(int argc, char **argv) 
{ 
    char filename[FILE_NAME_MAX_SIZE];
    int serverfd,clientfd;
    int port = DEFAULT_PORT;

    if(argc > 1)  
    {
        port = atoi(argv[1]);
    }
    
    serverfd = start_server(port); 
  
    if(serverfd < 0) 
    { 
        perror("Create Socket ERROR"); 
        exit(1); 
    } 
    
    int rcvbuf_len = TRANS_SIZE/2;
    int len = sizeof(rcvbuf_len);
    if(setsockopt(serverfd, SOL_SOCKET, SO_SNDBUF, (void *)&rcvbuf_len, len ) < 0 )
    {
        perror("getsockopt error");
        return -1;
    } 

    while(1)
    {
        clientfd = filerequestrecv(serverfd,filename);
        printf("fileanme %s\n",filename);

        file_send(clientfd,serverfd,filename);
    }
    
    close(serverfd); 
    return 0; 
} 
