#include "dr_api.h"
#include "dr_tools.h"
#include "drutil.h"
#include "drwrap.h"
#include "drmgr.h"

typedef struct {
  void *ptr;
  size_t size;
  size_t nmemb;
}tmp_data;

void pre_malloc(void *wrapctx, OUT void **user_data)
{
  *user_data = drwrap_get_arg(wrapctx, 0);
}

void post_malloc(void *wrapctx, void *user_data)
{
  dr_printf("malloc(%d) = %p\n", user_data, drwrap_get_retval(wrapctx));
}

void pre_calloc(void *wrapctx, OUT void **user_data)
{
  tmp_data *tmp_data = dr_global_alloc(sizeof(tmp_data));
  if (tmp_data)
    {
      tmp_data->nmemb = (size_t)drwrap_get_arg(wrapctx, 0);
      tmp_data->size = (size_t)drwrap_get_arg(wrapctx, 1);
      *user_data = tmp_data;
    }
}

void post_calloc(void *wrapctx, void *user_data)
{
  tmp_data *tmp = user_data;
  if (tmp != NULL)
    {
      dr_printf("calloc(%d, %d) = %p\n", tmp->nmemb, tmp->size, drwrap_get_retval(wrapctx));
      dr_global_free(tmp, sizeof(*tmp));
    }
}

void pre_realloc(void *wrapctx, OUT void **user_data)
{
  tmp_data *tmp_data = dr_global_alloc(sizeof(tmp_data));
  if (tmp_data)
    {
      tmp_data->ptr = drwrap_get_arg(wrapctx, 0);
      tmp_data->size = (size_t)drwrap_get_arg(wrapctx, 1);
      *user_data = tmp_data;
    }
}

void post_realloc(void *wrapctx, void *user_data)
{
  tmp_data *tmp = user_data;
  if (tmp != NULL)
    {
      dr_printf("realloc(%p, %d) = %p\n", tmp->ptr, tmp->size, drwrap_get_retval(wrapctx));
      dr_global_free(tmp, sizeof(tmp));
    }
}

void pre_free(void *wrapctx, __attribute__((unused))OUT void **user_data)
{
  dr_printf("free(%p) = %p\n", drwrap_get_arg(wrapctx, 0));
}

static void load_event(__attribute__((unused))void *drcontext,
		       const module_data_t *mod,
		       __attribute__((unused))bool loaded)
{
  app_pc		malloc = (app_pc)dr_get_proc_address(mod->handle,
							     "__libc_malloc");
  app_pc		calloc = (app_pc)dr_get_proc_address(mod->handle,
							     "__libc_calloc");
  app_pc		realloc = (app_pc)dr_get_proc_address(mod->handle,
							      "__libc_realloc");
  app_pc		free = (app_pc)dr_get_proc_address(mod->handle,
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

static void exit_event(void)
{
  drwrap_exit();
  drutil_exit();
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
  drutil_init();
  drmgr_init();

  dr_register_exit_event(&exit_event);
  if (!drmgr_register_module_load_event(&load_event))
    DR_ASSERT_MSG(false, "Can't register event handler\n");
}
