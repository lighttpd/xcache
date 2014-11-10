define(`xc_collect_object', `IFCALC(``xc_collect_object'($@)',``xc_collect_object' can be use in calc only')')
define(`xc_var_store_handle', `IFSTORE(``xc_var_store_handle'($@)',``xc_var_store_handle' can be use in store only')')
define(`xc_var_restore_handle', `IFRESTORE(``xc_var_restore_handle'($@)',``xc_var_restore_handle' can be use in restore only')')
