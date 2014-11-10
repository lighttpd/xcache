define(`xc_collect_object', `IFCALC(``xc_collect_object'($@)',``xc_collect_object' can be use in calc only')')
define(`xc_var_store_handle', `IFSTORE(``xc_var_store_handle'($@)',``xc_var_store_handle' can be use in store only')')
define(`xc_var_restore_handle', `IFRESTORE(``xc_var_restore_handle'($@)',``xc_var_restore_handle' can be use in restore only')')

define(`xc_collect_class', `IFCALC(``xc_collect_class'($@)',``xc_collect_class' can be use in calc only')')
define(`xc_var_store_ce', `IFSTORE(``xc_var_store_ce'($@)',``xc_var_store_ce' can be use in store only')')
