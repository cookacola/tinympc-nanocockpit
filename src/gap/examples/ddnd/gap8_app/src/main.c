#include "pmsis.h"
#include "bsp/bsp.h"
#include "bsp/fs.h"
#include "bsp/fs/readfs.h"
#include "bsp/flash.h"
#include "bsp/flash/hyperflash.h"
#include "ddnd_runtime.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "graph.h"
#include "ddnd_board.h"

static int8_t *arena;
static uint8_t *payload;
static int active_node;
static struct pi_device flash, fs, cluster;
static void fail(const char *where) { printf("DDND FAIL %s\n", where); pmsis_exit(1); }
static void read_exact(pi_fs_file_t *file, void *dst, unsigned bytes) {
 unsigned done=0;
 while(done<bytes) {
  int n=pi_fs_read(file,(uint8_t *)dst+done,bytes-done);
  if(n<=0) fail("short read");
  done+=(unsigned)n;
 }
}
static void load(const char *name, void *dst, unsigned bytes) {
 pi_fs_file_t *file=pi_fs_open(&fs,name,0);
 if(!file || file->size!=bytes) fail(name);
 read_exact(file,dst,bytes); pi_fs_close(file);
}
static void worker(void *arg) { ddnd_execute_node(active_node,arena,payload); }
static void cluster_entry(void *arg) { pi_cl_team_fork(8,worker,NULL); }
static void run(void *arg) {
 printf("DDND_START\n");
 ddnd_board_init();
 struct pi_hyperflash_conf flash_conf;
 pi_hyperflash_conf_init(&flash_conf); pi_open_from_conf(&flash,&flash_conf);
 if(pi_flash_open(&flash)) fail("flash open");
 struct pi_readfs_conf fs_conf;
 pi_readfs_conf_init(&fs_conf); fs_conf.fs.flash=&flash;
 pi_open_from_conf(&fs,&fs_conf); if(pi_fs_mount(&fs)) fail("readfs mount");
 arena=pi_l2_malloc(DDND_ARENA_BYTES);
 payload=pi_l2_malloc(DDND_PAYLOAD_BYTES > 4096 ? DDND_PAYLOAD_BYTES : 4096);
 if(!arena || !payload) fail("L2 allocation");
 printf("DDND memory arena=%u payload=%u\n",DDND_ARENA_BYTES,DDND_PAYLOAD_BYTES);
 struct pi_cluster_conf conf; pi_cluster_conf_init(&conf); conf.id=0;
 pi_open_from_conf(&cluster,&conf); if(pi_cluster_open(&cluster)) fail("cluster open");
#ifdef DDND_TILED
 ddnd_workspace=pi_cl_l1_malloc(&cluster,DDND_WORKSPACE_BYTES);
 if(!ddnd_workspace) fail("L1 workspace");
#endif
 pi_fs_file_t *weights=pi_fs_open(&fs,"weights.bin",0); if(!weights) fail("weights open");
 unsigned total_bad=0;
 for(int fixture=0;fixture<DDND_FIXTURE_COUNT;fixture++) {
 load(ddnd_fixture_inputs[fixture],arena+DDND_INPUT_OFFSET,DDND_INPUT_BYTES);
 unsigned start=rt_time_get_us();
 for(active_node=1;active_node<DDND_NODE_COUNT;active_node++) {
  unsigned n=ddnd_payload_bytes[active_node];
  if(n) { pi_fs_seek(weights,ddnd_payload_offsets[active_node]); read_exact(weights,payload,n); }
  struct pi_cluster_task task;
  pi_cluster_task(&task,cluster_entry,NULL);
  task.stack_size=2048; task.slave_stack_size=2048;
  unsigned t=rt_time_get_us();
  if(pi_cluster_send_task_to_cl(&cluster,&task)) fail("cluster task");
#ifdef DDND_NODE_TIMING
  printf("DDND node=%d us=%u\n",active_node,rt_time_get_us()-t);
#endif
 }
 printf("DDND graph_us=%u fixture=%d\n",rt_time_get_us()-start,fixture);
 for(int k=0;k<DDND_OUTPUT_COUNT;k++) {
  pi_fs_file_t *f=pi_fs_open(&fs,ddnd_fixture_goldens[fixture][k],0);
  if(!f || f->size!=ddnd_output_bytes[k]) fail("golden open/size");
  unsigned bad=0; int maxerr=0;
  for(unsigned off=0;off<ddnd_output_bytes[k];off+=4096) {
   unsigned n=ddnd_output_bytes[k]-off; if(n>4096)n=4096;
   read_exact(f,payload,n);
   for(unsigned j=0;j<n;j++) {
    int a=arena[ddnd_output_offsets[k]+off+j], b=((int8_t *)payload)[j];
    int e=a-b; if(e<0)e=-e; if(e){bad++;if(e>maxerr)maxerr=e;}
   }
  }
  pi_fs_close(f);total_bad+=bad;
  printf("DDND output=%s bytes=%u mismatch=%u maxerr=%d\n",ddnd_fixture_goldens[fixture][k],ddnd_output_bytes[k],bad,maxerr);
 }
 }
 pi_fs_close(weights);
 printf("DDND FULL_VECTOR_%s\n",total_bad?"FAIL":"PASS");
 pi_cluster_close(&cluster); pmsis_exit(total_bad?1:0);
}
int main(void) { return pmsis_kickoff((void *)run); }
