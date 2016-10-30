#include <linux/module.h>
#include <linux/init.h>
#include <linux/kmod.h>

MODULE_LICENSE( "GPL" );

static int umh_test( void )
{
  struct subprocess_info *sub_info;
  char *argv[] = { "/system/xbin/su", "-c", "/data/mausb", "bind", "--busid=1-1", NULL };
  static char *envp[] = {
        "HOME=/data",
        "TERM=linux",
		"SHELL=/system/bin/sh",
		"LD_LIBRARY_PATH=/vendor/lib:/system/lib",
		"MKSH=/system/bin/sh",
        "PATH=/sbin:/vendor/bin:/system/sbin:/system/bin:/system/xbin", NULL };

  sub_info = call_usermodehelper_setup( argv[0], argv, envp, GFP_ATOMIC );
  if (sub_info == NULL) return -ENOMEM;

  return call_usermodehelper_exec( sub_info, UMH_WAIT_PROC );
}


static int __init mod_entry_func( void )
{
  return umh_test();
}


static void __exit mod_exit_func( void )
{
  return;
}

module_init( mod_entry_func );
module_exit( mod_exit_func );