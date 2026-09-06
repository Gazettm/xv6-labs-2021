#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void find(char *path, char *filename)
{
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if ((fd = open(path, 0)) < 0)
    {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }
    if (fstat(fd, &st) < 0)
    {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }
    switch (st.type)
    {
    case T_FILE:
        for (p = path + strlen(path); p > path && *p != '/'; p--)
            ;
        if (*p == '/')
            p++;

        if (strcmp(p, filename) == 0)
        {
            printf("%s\n", path);
        }
        break;

    case T_DIR:
        if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf)
        {
            printf("find: path too long\n");
            break;
        }
        strcpy(buf, path);
        p = buf + strlen(buf);
        *p++ = '/';
        while (read(fd, &de, sizeof(de)) == sizeof(de))
        {
            if (de.inum == 0)
            {
                continue;
            }
            if (de.name[0] == '.' && (de.name[1] == '\0' || (de.name[1] == '.' && de.name[2] == '\0')))
            {
                continue;
            }
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            if (strcmp(p, filename) == 0)
            {
                printf("%s\n", buf);
            }
            if (stat(buf, &st) < 0)
            {
                printf("find: cannot stat %s\n", buf);
                continue;
            }else if(st.type == T_DIR){
                find(buf, filename);
            }
        }
        break;
    }

    close(fd);
}

int main(int argc, char *argv[])
{
    if (argc == 2) {
        find(".", argv[1]);
        exit(0);
    } else if (argc == 3) {
        find(argv[1], argv[2]);
        exit(0);
    } else {
        printf("find: please enter right argv\n");
        exit(0);
    }
}