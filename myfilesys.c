#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define InodeHead sizeof(superblock)
#define BlockHead InodeHead+InodeNum*sizeof(Inode)
#define InodeNum 1024
#define MaxBlockPerInode 1024
#define BlockNum  (80*1024)
#define BlockSize (30*sizeof(Dir))
#define MaxDirDepth 10
#define MaxNameLen 30
#define MaxSubNum 100
#define CommandLen 6
#define TargetLen 100

typedef struct 
{
    short inodemap[InodeNum];
    short blockmap[BlockNum];
    short inodeused;
    int blockused;
}superblock;

typedef struct 
{
    int blockpos[MaxBlockPerInode];
    int filesize;
    short type;
}Inode;

typedef struct
{
    char name[MaxNameLen];
    short ind;
}Dir;

FILE* disk;

superblock spblk;

// int curinum;

Inode curInode;  //当前目录的inode

char* curPath[MaxDirDepth+1];   //当前路径名称

Dir subFileList[MaxSubNum];  //目录子项信息

void ReadCurDir(void);

void InitFs(void);

void GetInode(Inode* ind,short num);

void ShowSubDir(void);

void MakeDir(char* name);

void RmDir(char* name);

void GoDir(char* name);

void CreaterFile(char* name);

char* commandlist[]={"quit","ls","cd","mkdir","rm","touch"};

int main()
{
    InitFs();
    while (1)
    {
        //打印终端头部
        printf("hbk@filesys:/");
        for(int i=0;i<MaxDirDepth;i++)
        {
            if(*(curPath+i)==NULL)
                break;
            printf("%s",curPath[i]);
        }
        printf("$");

        char command[CommandLen];
        char target[TargetLen];
        scanf("%s",command);
        short choice=-1;
        for(int i=0;i<sizeof(commandlist)/sizeof(char*);i++)
        {
            if(strcmp(command,commandlist[i])==0)
            {
                choice=i;
                break;
            }
        }
        switch (choice)
        {
        case 1:
            ShowSubDir();
            break;
        case 2:
            scanf("%s",target);
            GoDir(target);
            break;
        case 3:
            scanf("%s",target);
            MakeDir(target);
            break;
        case 4:
            scanf("%s",target);
            RmDir(target);
            break;
        case 5:
            scanf("%s",target);
            CreaterFile(target);
            break;
        default:
            break;
        }
        if(choice==0)
            break;
    }
    return 0;
}

void InitFs(void)
{
    if(BlockSize<sizeof(Dir))
    {
        fprintf(stderr,"BlockSize Too Small!\n");
        exit(1);
    }
    /*初始化变量信息*/
    disk=NULL;
    memset(&spblk,0,sizeof(superblock));
    memset(&curPath,0,MaxDirDepth*sizeof(char*));
    memset(&subFileList,0,MaxSubNum*sizeof(Dir));

    disk=fopen("disk","r+");
    fread(&spblk,sizeof(superblock),1,disk);
    if(spblk.inodeused==0)
    {
        //设置基本信息
        spblk.inodemap[0]=1;
        spblk.inodeused=1;
        spblk.blockmap[0]=1;
        spblk.blockused=1;
        curInode.type=1;
        curInode.filesize=2*sizeof(Dir);
        curInode.blockpos[0]=0;
        fseek(disk,0,SEEK_SET);
        fwrite(&spblk,sizeof(superblock),1,disk);
        fseek(disk,InodeHead,SEEK_SET);
        fwrite(&curInode,sizeof(Inode),1,disk);

        //创建该子目录信息
        subFileList[0].ind=0;
        strcpy(subFileList[0].name,".");
       subFileList[1].ind=0;
        strcpy(subFileList[1].name,"..");
        fseek(disk,BlockHead,SEEK_SET);
        fwrite(subFileList,sizeof(Dir),2,disk);
    }
    else{
        GetInode(&curInode, 0);
        int step=BlockSize/sizeof(Dir);
        for(int i=0;i<curInode.filesize/BlockSize;i++)
        {
            for(int j=0;j<step;j++)
            {
                fseek(disk,BlockHead+i*BlockSize+sizeof(Dir)*j,SEEK_SET);
                fread(subFileList+i*step+j,sizeof(Dir),1,disk);
            }
        }
        int left=curInode.filesize/BlockSize;
        for(int j=0;j<curInode.filesize/sizeof(Dir)%step;j++)
        {
            fseek(disk,BlockHead+left*BlockSize+sizeof(Dir)*j,SEEK_SET);
            fread(subFileList+left*step+j,sizeof(Dir),1,disk);
        }
    }
}

void GetInode(Inode* ind,short num)
{
    fseek(disk,InodeHead+num*sizeof(Inode),SEEK_SET);
    fread(ind,sizeof(Inode),1,disk);
}

void CheckInode()
{
    if(spblk.inodeused>=InodeNum)
    {
        fprintf(stderr,"Inode exhaust\n");
        exit(1);
    }
}

void CheckBlock()
{
    if(spblk.blockused>=BlockNum)
    {
        fprintf(stderr,"Block exhaust\n");
        exit(1);
    }
}

void ReadBlock(void* buff,Inode* ind)
{
    for(int i=0;i<ind->filesize/BlockSize;i++)
    {
        fseek(disk,BlockHead+ind->blockpos[i]*BlockSize,SEEK_SET);
        fread(buff+i*BlockSize,BlockSize,1,disk);
    }
    fseek(disk,BlockHead+ind->blockpos[ind->filesize/BlockSize]*BlockSize,SEEK_SET);
    fread(buff+ind->filesize/BlockSize*BlockSize,ind->filesize%BlockSize,1,disk);
}

void ShowSubDir(void)
{
    for(int i=0;i<curInode.filesize/sizeof(Dir);i++)
    {
        Inode tmp;
        GetInode(&tmp,subFileList[i].ind);
        if(tmp.type)
            printf("\033[32m%s  ",subFileList[i].name);
        else
            printf("\033[0m%s ",subFileList[i].name);
    }
    printf("\033[0m\n");
}

void MakeDir(char* name)
{
    /*异常判别*/
    if(strcmp(name,".")==0||strcmp(name,"..")==0)
    {
        fprintf(stderr,"Wrong name\n");
        return;
    }
    for(int i=2;i<curInode.filesize/sizeof(Dir);i++)
    {
        if(strcmp(name,subFileList[i].name)==0)
        {
            fprintf(stderr,"Duplicate name\n");
            return;
        }
    }
    /*申请Inode*/
    CheckInode();
    Inode subInode;
    subInode.filesize=2*sizeof(Dir);
    subInode.type=1;

    short reqInode=0;
    for(;reqInode<InodeNum;reqInode++)
        if(spblk.inodemap[reqInode]==0)
            break;
    spblk.inodemap[reqInode]=1;
    spblk.inodeused++;
    /*在父目录的block中更新该信息*/
    Dir onesub;
    onesub.ind=reqInode;
    strcpy(onesub.name,name);
    if(curInode.filesize%BlockSize==0)
    {
        CheckBlock();
        for(int i=0;i<BlockNum;i++)
        {
            if(spblk.blockmap[i]==0)
            {
                spblk.blockmap[i]=1;
                spblk.blockused++;
                curInode.blockpos[curInode.filesize/BlockSize]=i;
                fseek(disk,BlockHead+i*BlockSize,SEEK_SET);
                break;
            }
        }
    }else{
        fseek(disk,BlockHead+curInode.blockpos[curInode.filesize/BlockSize]*BlockSize+curInode.filesize%BlockSize,SEEK_SET);
    }
    fwrite(&onesub,sizeof(Dir),1,disk);
    subFileList[curInode.filesize/sizeof(Dir)]=onesub;
    curInode.filesize+=sizeof(Dir);
    fseek(disk,InodeHead+subFileList[0].ind*sizeof(Inode),SEEK_SET);
    fwrite(&curInode,sizeof(Inode),1,disk);
    

    /*申请block添加子目录的子目录*/
    Dir twosub[2];
    twosub[0].ind=reqInode;
    strcpy(twosub[0].name,".");
    twosub[1].ind=subFileList[0].ind;
    strcpy(twosub[1].name,"..");
    CheckBlock();
    for(int i=0;i<BlockNum;i++)
    {
        if(spblk.blockmap[i]==0)
        {
            spblk.blockmap[i]=1;
            spblk.blockused++;
            subInode.blockpos[0]=i;
            fseek(disk,InodeHead+reqInode*sizeof(Inode),SEEK_SET);
            fwrite(&subInode,sizeof(Inode),1,disk);
            fseek(disk,BlockHead+i*BlockSize,SEEK_SET);
            fwrite(twosub,sizeof(Dir),2,disk);
            break;
        }
    }
    fseek(disk,0,SEEK_SET);
    fwrite(&spblk,sizeof(superblock),1,disk);
}

void ClearDir(int rootid)
{
    Inode root;
    GetInode(&root,rootid);
    /*将子目录都清空*/
    Dir* tmpsub=(Dir*)malloc(root.filesize);
    ReadBlock(tmpsub,&root);
    for(int i=2;i<root.filesize/sizeof(Dir);i++)
        ClearDir(tmpsub[i].ind);
    /*把block清空*/
    for(int i=0;i<=root.filesize/BlockSize;i++)
        spblk.blockmap[root.blockpos[i]]=0;
    spblk.blockused-=root.filesize/BlockSize+1;
    spblk.inodemap[rootid]=0;
    spblk.inodeused--;
}

void RmcurDir(int subid, int subrank)
{
    /*遍历子文件夹进行清空*/
    ClearDir(subid);

    /*读取父磁盘上位置并作移动*/
    Dir* rmdir=(Dir*)malloc(sizeof(Dir));
    Dir* moddir=(Dir*)malloc(sizeof(Dir));
    fseek(disk,BlockHead+BlockSize*curInode.blockpos[curInode.filesize/BlockSize]+curInode.filesize%BlockSize*sizeof(Dir),SEEK_SET);
    fread(moddir,sizeof(Dir),1,disk);
    fseek(disk,BlockHead+BlockSize*curInode.blockpos[subrank/(BlockSize/sizeof(Dir))]+subrank%(BlockSize/sizeof(Dir))*sizeof(Dir),SEEK_SET);
    fread(rmdir,sizeof(Dir),1,disk);
    fwrite(moddir,sizeof(Dir),1,disk);
    subFileList[subrank]=subFileList[curInode.filesize/sizeof(Dir)];
    /*修改超级块与该块的信息*/
    if(curInode.filesize%BlockSize==0)
    {
        spblk.blockmap[curInode.blockpos[curInode.filesize/BlockSize]]=0;
        spblk.blockused--;
    }
    curInode.filesize-=sizeof(Dir);
    fseek(disk,InodeHead+subFileList[0].ind*sizeof(Inode),SEEK_SET);
    fwrite(&curInode,sizeof(Inode),1,disk);
    fseek(disk,0,SEEK_SET);
    fwrite(&spblk,sizeof(superblock),1,disk);
}

void RmDir(char* name)
{
    for(int i=2;i<curInode.filesize/sizeof(Dir);i++)
    {
        if(strcmp(subFileList[i].name,name)==0)
        {
            RmcurDir(subFileList[i].ind,i);
            break;
        }
    }
}

void GoDir(char* name)
{
    /*异常判别*/
    for(int i=0;i<curInode.filesize/sizeof(Dir);i++)
    {
        if(strcmp(name,subFileList[i].name)==0)
        {
            if(strcmp(name,".")==0)
                return;
            else if(strcmp(name,"..")==0)
            {
                if(curPath[0]==NULL)
                    return;
                for(int j=1;j<=MaxDirDepth;j++)
                {
                    if(curPath[j]==NULL)
                    {
                        free(curPath[j-1]);
                        curPath[j-1]=NULL;
                        break;
                    }
                }
            }else{
                int j=0;
                for(;j<MaxDirDepth;j++)
                {
                    if(curPath[j]==NULL)
                    {
                        curPath[j]=(char*)malloc((strlen(name)+2)*sizeof(char));
                        strcpy(curPath[j],name);
                        strcat(curPath[j],"/");
                        break;
                    }
                }
                if(j==MaxDirDepth)
                {
                    fprintf(stderr,"Address depth overflow\n");
                    exit(1);
                }
            }
            int presub=curInode.filesize/sizeof(Dir);
            GetInode(&curInode,subFileList[i].ind);
            int cursub=curInode.filesize/sizeof(Dir);
            if(presub-cursub>0)
                memset(subFileList+cursub,0,(presub-cursub)*sizeof(Dir));
            ReadBlock(subFileList,&curInode);
            break;
        }
    }
}

void CreaterFile(char* name)
{
    /*异常判别*/
    if(strcmp(name,".")==0||strcmp(name,"..")==0)
    {
        fprintf(stderr,"Wrong name\n");
        return;
    }
    for(int i=2;i<curInode.filesize/sizeof(Dir);i++)
    {
        if(strcmp(name,subFileList[i].name)==0)
        {
            fprintf(stderr,"Duplicate name\n");
            return;
        }
    }
    /*申请Inode*/
    CheckInode();
    Inode subInode;
    subInode.filesize=0;
    subInode.type=0;

    short reqInode=0;
    for(;reqInode<InodeNum;reqInode++)
        if(spblk.inodemap[reqInode]==0)
            break;
    spblk.inodemap[reqInode]=1;
    spblk.inodeused++;
    /*在父目录的block中更新该信息*/
    Dir onesub;
    onesub.ind=reqInode;
    strcpy(onesub.name,name);
    if(curInode.filesize%BlockSize==0)
    {
        CheckBlock();
        for(int i=0;i<BlockNum;i++)
        {
            if(spblk.blockmap[i]==0)
            {
                spblk.blockmap[i]=1;
                spblk.blockused++;
                curInode.blockpos[curInode.filesize/BlockSize]=i;
                fseek(disk,BlockHead+i*BlockSize,SEEK_SET);
                break;
            }
        }
    }else{
        fseek(disk,BlockHead+curInode.blockpos[curInode.filesize/BlockSize]*BlockSize+curInode.filesize%BlockSize,SEEK_SET);
    }
    //副目录增加该文件
    fwrite(&onesub,sizeof(Dir),1,disk);
    subFileList[curInode.filesize/sizeof(Dir)]=onesub;
    curInode.filesize+=sizeof(Dir);
    //重写父节点
    fseek(disk,InodeHead+subFileList[0].ind*sizeof(Inode),SEEK_SET);
    fwrite(&curInode,sizeof(Inode),1,disk);
    
    /*写入超级块*/
    fseek(disk,InodeHead+reqInode*sizeof(Inode),SEEK_SET);
    fwrite(&subInode,sizeof(Inode),1,disk);
    fseek(disk,0,SEEK_SET);
    fwrite(&spblk,sizeof(superblock),1,disk);
}