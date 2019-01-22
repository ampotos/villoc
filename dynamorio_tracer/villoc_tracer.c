#include "dr_api.h"
#include "dr_tools.h"
#include "drwrap.h"
#include "drmgr.h"

// realloc is the longest possible string to log
// so the max size the realloc can have will be the of the per thread logging
#define BUF_SIZE (16 + 19 + 20 * 2)

int tls_idx;

void reset_buf()
{
  void *drc = dr_get_current_drcontext();

  char *buf = drmgr_get_tls_field(drc, tls_idx);
  for (size_t i = 0; i < BUF_SIZE; i++)
    buf[i] = 0;
}

void pre_malloc(void *wrapctx,  __attribute__((unused)) OUT void **user_data)
{
  void *drc = dr_get_current_drcontext();

  dr_snprintf(drmgr_get_tls_field(drc, tls_idx), BUF_SIZE, "malloc(%d) = ", drwrap_get_arg(wrapctx, 0));
}

void post_malloc(void *wrapctx,  __attribute__((unused)) void *user_data)
{
  void *drc = dr_get_current_drcontext();

  dr_printf("%s%p\n", drmgr_get_tls_field(drc, tls_idx), drwrap_get_retval(wrapctx));
  reset_buf();
}

void pre_calloc(void *wrapctx,  __attribute__((unused)) OUT void **user_data)
{
  void *drc = dr_get_current_drcontext();

  dr_snprintf(drmgr_get_tls_field(drc, tls_idx), BUF_SIZE, "calloc(%zu, %zu) = ", drwrap_get_arg(wrapctx, 0), drwrap_get_arg(wrapctx, 1));
}

void post_calloc(void *wrapctx,  __attribute__((unused)) void *user_data)
{
  void *drc = dr_get_current_drcontext();

  dr_printf("%s%p\n", drmgr_get_tls_field(drc, tls_idx), drwrap_get_retval(wrapctx));
  reset_buf();
}

void pre_realloc(void *wrapctx,  __attribute__((unused)) OUT void **user_data)
{
  void *drc = dr_get_current_drcontext();

  dr_snprintf(drmgr_get_tls_field(drc, tls_idx), BUF_SIZE, "realloc(%p, %zu) = %p\n", drwrap_get_arg(wrapctx, 0), drwrap_get_arg(wrapctx, 1));
}

void post_realloc(void *wrapctx,  __attribute__((unused)) void *user_data)
{
  void *drc = dr_get_current_drcontext();

  dr_printf("%s%p\n", drmgr_get_tls_field(drc, tls_idx), drwrap_get_retval(wrapctx));
  reset_buf();
}

void pre_free(void *wrapctx, __attribute__((unused))OUT void **user_data)
{
  dr_printf("free(%p) = <void>\n", drwrap_get_arg(wrapctx, 0));
}

void load_event(__attribute__((unused))void *drcontext,
		       const module_data_t *mod,
		       __attribute__((unused))bool loaded)
{
  app_pc malloc = (app_pc)dr_get_proc_address(mod->handle,
							     "__libc_malloc");
  app_pc calloc = (app_pc)dr_get_proc_address(mod->handle,
							     "__libc_calloc");
  app_pc realloc = (app_pc)dr_get_proc_address(mod->handle,
							      "__libc_realloc");
  app_pc free = (app_pc)dr_get_proc_address(mod->handle,
							   "__libc_free");

  if (malloc)
    DR_ASSERT(drwrap_wrap(malloc, pre_malloc, post_malloc));

  if (calloc)
    DR_ASSERT(drwrap_wrap(calloc, pre_calloc, post_calloc));

  if (realloc)
    DR_ASSERT(drwrap_wrap(realloc, pre_realloc, post_realloc));

  if (free)
    DR_ASSERT(drwrap_wrap(free, pre_free, NULL));
}

void thread_init_event(void *drc)
{
  char *buf = dr_global_alloc(BUF_SIZE);

  drmgr_set_tls_field(drc, tls_idx, buf);
  reset_buf();
}

void thread_exit_event(void *drc)
{
  char *buf = drmgr_get_tls_field(drc, tls_idx);

  if (buf[0] != 0)
    dr_printf("%s\n", buf);
  dr_global_free(buf, BUF_SIZE);
}

void exit_event(void)
{
  drwrap_exit();
  drmgr_exit();
}

DR_EXPORT void dr_client_main(client_id_t __attribute__((unused))id,
			      __attribute__((unused))int argc,
			      __attribute__((unused))const char *argv[])
{
  drmgr_priority_t      p = {
    sizeof(p),
    "Print allocation trace for Villoc",
    NULL,
    NULL,
    0};

  dr_set_client_name("Villoc_dbi", "ampotos@gmail.com");

  drwrap_init();
  drmgr_init();

  dr_register_exit_event(&exit_event);
  if (!drmgr_register_module_load_event(&load_event))
    DR_ASSERT_MSG(false, "Can't register module load event handler\n");
  if (!drmgr_register_thread_init_event(&thread_init_event))
    DR_ASSERT_MSG(false, "Can't register thread init event handler\n");
  if (!drmgr_register_thread_exit_event(&thread_exit_event))
    DR_ASSERT_MSG(false, "Can't register thread init exit handler\n");

  // register a slot per thread for logging
  tls_idx = drmgr_register_tls_field();
  DR_ASSERT_MSG(tls_idx != -1, "Can't register tls field\n");
}
