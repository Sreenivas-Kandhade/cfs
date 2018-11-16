/*
  gcc -Wall cfs.c `pkg-config fuse --cflags --libs` -o cfs
*/
#define FUSE_USE_VERSION 30
#define NAME_MAX_SIZE 512
#define PASS_MAX 20
#define FLUSH_MAX 5
#define FOUND 1
#define NOT_FOUND 0
#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

typedef struct nodedat{
	char name[NAME_MAX_SIZE];
	short int isdir;
	short int rem;
	struct stat st;
}nodedat;

typedef struct inode {
	nodedat header;
	char* filedata;
	struct inode* parent;
	struct inode* firstchild;
	struct inode* next;
}inode;

long int remem;
short int remdef;
inode* root;
const char* file = "DISHERE";
int fd;
int unicount = 0;

int allocate_node(inode** node) {
  	if (remem < sizeof(inode)) {
  		return -ENOSPC;
  	}
  	*node = malloc(sizeof(inode));
  	if (*node == NULL) {
  		return -ENOSPC;
  	}
  	remem-= sizeof(inode);
	return 0;
}

void store(inode* node){
    int len;
    int cc = node->header.st.st_nlink-2;
    inode* tmp = node->firstchild;
    for(int i=0;i<cc;i++){
        write(fd,&(tmp->header),sizeof(nodedat));
        if(tmp->header.isdir){
            store(tmp);
        }else{
            len = tmp->header.st.st_size;
            write(fd,tmp->filedata,len*sizeof(char));
        }
        tmp = tmp->next;
    }
}

void load(inode* parent){
    int len;
    int cc = parent->header.st.st_nlink-2;
    if(cc==0){
        return;
    }
    inode* cur;
    allocate_node(&cur);
    parent->firstchild = cur;
    cur->parent = parent;
    
    for(int i = 1; i < cc; i++){
        inode* new;
        allocate_node(&new);
        cur->next = new;
        new->parent = parent;
        cur = new;
    }
    
    cur = parent->firstchild;
    
    for(int i = 0; i < cc; i++){
        read(fd,&(cur->header),sizeof(nodedat));
        if(cur->header.isdir){
            load(cur);
        }else{
            len = cur->header.st.st_size;
            cur->filedata = calloc(len,sizeof(char));
            remem-=len;
            read(fd,cur->filedata,len*sizeof(char));
        }
        cur=cur->next;
    }
    
}

void flush(){
    fd = open(file, O_RDWR|O_CREAT);
    if(fd){
        write(fd,&(root->header),sizeof(nodedat));
        store(root);
        close(fd);
    }
}

void cfs_destroy(void* private_data) {
    flush();
}

void dad_change_time(inode* parent) {
	time_t cur;
	time(&cur);
	parent->header.st.st_ctime = cur;
	parent->header.st.st_mtime = cur;
}

int check_path(const char* path, inode** n){
	char temp[NAME_MAX_SIZE];
  	strncpy(temp, path, NAME_MAX_SIZE);
  	if(strcmp(path, "/") == 0 || strcmp(path, "") == 0) {
  		*n = root;
  		return 1;		
  	}
  	char* token = strtok(temp, "/");
  	inode* parent = root;
  	inode* child = NULL;
  	while(token != NULL){
  		short int found = 0;
  		child = parent->firstchild;
  		while(child != NULL) {
  			if(strcmp(child->header.name, token) == 0) {
  				found = 1;
  				break;
  			}
  			child = child->next;
  		}
  		if (!found)
  			return NOT_FOUND;
  		token = strtok(NULL, "/");
  		parent = child;
  	}
  	*n = child;
	if(!child->header.isdir){
		if(child->header.rem<=0){
			return NOT_FOUND;
		}
	}
  	return FOUND;
}

static int cfs_getattr(const char *path, struct stat *stbuf){
  	inode* node = NULL;
  	if(!check_path(path, &node)){
  		return -ENOENT;
  	}
  	*stbuf = node->header.st;
  	return 0;
}

static int cfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
						off_t offset, struct fuse_file_info *fi){    
  	time_t T;
  	time(&T);
  	inode* parent = NULL;
  	if(!check_path(path, &parent)){
  		return -ENOENT;
  	}
  	filler(buf, ".", NULL, 0,0);
  	filler(buf, "..", NULL, 0,0);
  	inode* temp = NULL;
  	for(temp = parent->firstchild; temp; temp = temp->next) {
  		filler(buf, temp->header.name, NULL, 0,0);
  	}
  	parent->header.st.st_atime = T;
  	return 0;
}

static int cfs_open(const char *path, struct fuse_file_info *fi){	
  	inode *p= NULL;
  	if(!check_path(path, &p)){
  		return -ENOENT;
  	}
  	return 0;
}

static int cfs_read(const char *path, char *buf, size_t size, off_t offset,
  					struct fuse_file_info *fi){
  	time_t T;
  	inode* node = NULL;
  	int valid = check_path(path, &node);
  	if (!valid) {
  		return -ENOENT;
  	}
	int filesize = node->header.st.st_size;
  	if (node->header.isdir) {
  		return -EISDIR;
  	}
  	time(&T);
  	if (offset < filesize) {
  		if (offset + size > filesize) {
  			size = filesize - offset;
  		}
  		memcpy(buf, node->filedata + offset, size);
  	}else{
  		size = 0;
  	}
  	return size;
}

static int cfs_utime(const char *path, struct utimbuf *ubuf){
	  	return 0;
}
  
void initdir(inode* newchild, char* dname) {
  	
  	newchild->header.isdir = 1;
  	strcpy(newchild->header.name, dname);

	newchild->header.st.st_nlink = 2;   	newchild->header.st.st_uid = getuid();
	newchild->header.st.st_gid = getgid();
	newchild->header.st.st_mode = S_IFDIR |  0755; 
	newchild->header.st.st_size = 4096;
	newchild->header.st.st_blocks = 8;
	time_t T;
	time(&T);

	newchild->header.st.st_atime = T;
	newchild->header.st.st_mtime = T;
	newchild->header.st.st_ctime = T;
}


void initfile(inode* newchild, char* fname) {
	
	newchild->header.isdir = 0;
	strcpy(newchild->header.name, fname);

	newchild->header.st.st_size = 0;
	newchild->header.st.st_nlink = 1;   
	newchild->header.st.st_uid = getuid();
	newchild->header.st.st_gid = getgid();
	newchild->header.st.st_mode = S_IFREG | 0644;
	newchild->header.st.st_blocks = 0;
	time_t T;
	time(&T);
	newchild->header.st.st_atime = T;
	newchild->header.st.st_mtime = T;
	newchild->header.st.st_ctime = T;
	newchild->header.rem=remdef;
}


static int cfs_mkdir(const char *path, mode_t mode) {

	inode *parent = NULL;
	int valid = check_path(path, &parent);

	if(valid) {
		return -EEXIST;
	}

	char* ptr = strrchr(path, '/');
	char tmp[NAME_MAX_SIZE];
	strncpy(tmp, path, ptr - path);
	tmp[ptr - path] = '\0';

	valid = check_path(tmp, &parent);
	if (!valid) {
		return -ENOENT;
	}
	inode* newchild = NULL;
	int ret = allocate_node(&newchild);

	if(ret != 0) {
		return ret;
	}

	ptr++;
	initdir(newchild, ptr);
	
	parent->header.st.st_nlink = parent->header.st.st_nlink + 1;

	newchild->parent = parent;
	newchild->next = parent->firstchild;
	parent->firstchild = newchild;

	dad_change_time(parent);

	return 0;
}

static int cfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {

	time_t T;
	inode* node = NULL;
	int valid = check_path(path, &node);
	int filelen = node->header.st.st_size;
	if (!valid) {
		return -ENOENT;
	}
	if (node->header.isdir)   { return -EISDIR; }
	if (size == 0)          { return 0; }
	if(remem - (sizeof(char)* size) < 0)
		  { return -ENOSPC; }
	node->header.rem = node->header.rem - 1;
	if(filelen == 0) 	{	
		node->filedata = malloc(sizeof(char)* size);		
		node->header.st.st_size = size;
		remem -= size;
		memcpy(node->filedata,buf,size);
	}
	else if((offset + size) > filelen)
	{
		node->filedata = realloc(node->filedata, sizeof(char)* (offset + size));
		memcpy(node->filedata + offset, buf, size);      
		node->header.st.st_size = offset + size;
		remem -= (sizeof(char)* (offset + size - filelen));
	}
		else if((offset + size) < filelen)	
	{
		node->filedata = realloc(node->filedata, sizeof(char)* (offset + size));
		memcpy(node->filedata + offset, buf, size);      
		node->header.st.st_size = offset + size;
		remem -= (sizeof(char)* (offset + size - filelen));
	}
	float store = (node->header.st.st_size)/(512.0);
	if(node->header.st.st_size == 0) {node->header.st.st_blocks = 0; }
	else if(((int)store/8 == 0 || store == 8.0) && node->header.st.st_size != 0) { node->header.st.st_blocks = 8;}
	else if(store > (node->header.st.st_blocks))
		node->header.st.st_blocks += 8;
	else if(node->header.st.st_blocks - store > 8)
		node->header.st.st_blocks -= 8; 
	time(&T);
	node->header.st.st_ctime = T;
	node->header.st.st_mtime = T;
	return size;	  
}


static int cfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
	
	inode *parent = NULL;
	int valid = check_path(path, &parent);

	if(valid) {
		return -EEXIST;
	}

	char* ptr = strrchr(path, '/');
	char tmp[NAME_MAX_SIZE];
	strncpy(tmp, path, ptr - path);
	tmp[ptr - path] = '\0';

	valid = check_path(tmp, &parent);
	if (!valid) {
		return -ENOENT;
	}
	inode* newchild = NULL;
	int ret = allocate_node(&newchild);

	if(ret != 0) {
		return ret;
	}

	ptr++;
	initfile(newchild, ptr);
	
	parent->header.st.st_nlink = parent->header.st.st_nlink + 1;

	newchild->parent = parent;
	newchild->next = parent->firstchild;
	parent->firstchild = newchild;

	dad_change_time(parent);
	if(unicount==0){
	    flush();
	    unicount = FLUSH_MAX;
	}
	return 0;
}

void remove_from_ds (inode* child) {
	inode* parent = child->parent;
	if (parent == NULL) { return;}

	if (parent->firstchild == child) {
		parent->firstchild = child->next;
	} else {
		inode* tmp = parent->firstchild;
		while (tmp != NULL) {
			if (tmp->next == child) {
				tmp->next = child->next;
				break;
			}
			tmp = tmp->next;
		}
	}
	parent->header.st.st_nlink--;
	dad_change_time(parent);
}

static int cfs_rmdir(const char *path) {
	inode* node = NULL;
	int valid = check_path(path, &node);
	if (!valid) {
		return -ENOENT;
	}
	if (!node->header.isdir) { return -ENOTDIR;   }
	if (node->firstchild)  { return -ENOTEMPTY; }
	remove_from_ds(node);
	free(node);
	remem = remem + sizeof(inode);
	return 0;
}

static int cfs_unlink(const char* path) {
	
	inode* node = NULL;
	int valid = check_path(path, &node);
	if (!valid) {
		return -ENOENT;
	}
	if (node->header.isdir) { return -EISDIR;}
	remove_from_ds(node);
	long freed_mem = sizeof(inode);
	if (node->filedata != NULL) {
		freed_mem = freed_mem + node->header.st.st_size;
		free(node->filedata);
		node->filedata = NULL;
	}
	free(node);
	remem = remem + freed_mem;
	return 0;
}

static int cfs_opendir(const char *path, struct fuse_file_info *fi) {
	inode *p= NULL;
	int valid = check_path(path, &p);
	if (!valid) {
		return -ENOENT;
	}
	if (!p->header.isdir) {
		return -ENOTDIR;
	}
	return 0;
}

static int cfs_rename(const char* from, const char* to) {
	inode* nodefrom = NULL;
	inode* nodeto = NULL;
	int valid = check_path(from, &nodefrom);
	if(!valid){
		return -ENOENT;
	}
	valid = check_path(to, &nodeto);
	char* ptr = strrchr(to, '/');
	char tmp[NAME_MAX_SIZE];
	strncpy(tmp, to, ptr - to);
	tmp[ptr - to] = '\0';
	ptr++;
	if(!valid){
		valid = check_path(tmp, &nodeto);
		if (!valid) {
			return -ENOENT;
		}
	}else{
		if(nodeto->header.isdir){		
			if(nodeto->firstchild){
				return -ENOTEMPTY;
			}
			nodeto = nodeto->parent;
			cfs_rmdir(to);
		}else{
			nodeto = nodeto->parent;
			cfs_unlink(to);
		}	
	}
	remove_from_ds(nodefrom);
	nodefrom->parent = nodeto;
	nodefrom->next = nodeto->firstchild;
	nodeto->firstchild = nodefrom;
	nodeto->header.st.st_nlink++;
	strncpy(nodefrom->header.name, ptr, NAME_MAX_SIZE);
	time_t T;
	time(&T);
	nodefrom->header.st.st_ctime = T;
	dad_change_time(nodeto);
	return 0;
}

static struct fuse_operations cfs_oper = {
	.getattr	= cfs_getattr,
	.readdir	= cfs_readdir,
	.open		= cfs_open,
	.read		= cfs_read,
	.rmdir		= cfs_rmdir,
	.mkdir		= cfs_mkdir,
	.create     = cfs_create,
	.utimens	= cfs_utime,
	.write      = cfs_write,
	.unlink 	= cfs_unlink,
	.opendir	= cfs_opendir,   
	.rename     = cfs_rename, 
	.destroy    = cfs_destroy,
	.flush      = flush,
};

int main(int argc, char *argv[])
{
	
	char buf[PASS_MAX];
	fd = open("pass", O_RDWR);
	read(fd, buf,20);
	if(strcmp(buf,argv[argc-3])!=0){
		printf("Invalid Password");
		return -1;
	}
	write(fd, argv[argc-2], 20);
	close(fd);
	fd = open(file, O_RDONLY);
	int inted = 0;
	if(fd){
	    allocate_node(&root);
	    read(fd, &(root->header),sizeof(nodedat));
	    load(root);
	    close(fd);
	    inted = 1;
	}
	if(!inted){
    	remdef = atoi(argv[argc-1]);
    	remem = 50* 1024* 1024;
    	root = (inode *)calloc(1, sizeof(inode));
    	strcpy(root->header.name, "/");
    	root->header.isdir = 1;
    	root->header.st.st_nlink = 2;
    	root->header.st.st_uid = 0;
    	root->header.st.st_gid = 0;
    	root->header.st.st_mode = S_IFDIR|0755;
    	root->header.st.st_size = 4096;
    	root->header.st.st_blocks = 8;
    	time_t T;
	    time(&T);
    	root->header.st.st_atime = T;
    	root->header.st.st_mtime = T;
	    root->header.st.st_ctime = T;
    	remem = remem - sizeof(inode);
	}
	return fuse_main(argc-3, argv, &cfs_oper, NULL);
}
