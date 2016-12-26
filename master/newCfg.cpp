
#include "log.h"
#include "newCfg.h"

typedef struct Node
{
    int newCfgID;
    struct Node *lchild, *rchild;
}newCfgNode;

static newCfg_table = NULL;

int newCfg_search(newCfgTree stNewCfgTree, int newCfgID)
{

    return 0;
}

int newCfg_init(void)
{
    newCfgTree stNewCfgTree = (newCfgTree)malloc(sizeof(newCfgNode));
    if(stNewCfgTree == NULL)
    {
        log_error("newCfg_tree_init error!");
        return -1;
    }

    return 0;
}

