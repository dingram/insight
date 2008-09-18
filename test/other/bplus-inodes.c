#include <debug.h>
#include <bplus.h>
#include <stdio.h>

#define printmsg(f, ...) printf("\033[1;32m>>>\033[1;37m " f "\033[m", ##__VA_ARGS__)

#define INODECOUNT 10000

int main(int argc, char **argv) {
  int i;

  printmsg("Opening tree...\n");
  tree_open("my-test-tree");
  printmsg("Done.\n");

  printmsg("Inserting limbo inodes...\n");
  for (i=0; i<INODECOUNT; i++) {
    inode_insert(0, (i+1) | ((i+1)<<16));
  }
  printmsg("Done inserting limbo inodes\n");

  printmsg("Removing limbo inodes...\n");
  for (i=0; i<INODECOUNT; i++) {
    inode_remove(0, (i+1) | ((i+1)<<16));
  }
  printmsg("Done removing limbo inodes\n");

  printmsg("Closing tree...\n");
  tree_close();
  printmsg("Done.\n");
  return 0;
}
