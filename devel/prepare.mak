CTAGS=$(shell which ctags 2>/dev/null || which exuberant-ctags 2>/dev/null)
AWK=$(shell which gawk 2>/dev/null || which awk 2>/dev/null)

include devel/prepare.cfg

.PHONY: dummy
.PHONY: all
all: xcache/xc_opcode_spec_def.h xc_const_string tags po

.PHONY: clean
clean: clean_xc_const_string clean_po
	rm -f tags xcache/xc_opcode_spec_def.h

.PHONY: clean_xc_const_string
clean_xc_const_string:
	rm -f xcache/xc_const_string_opcodes_php*.h

.PHONY: xc_const_string
xc_const_string: \
	xcache/xc_const_string_opcodes_php4.x.h \
	xcache/xc_const_string_opcodes_php5.0.h \
	xcache/xc_const_string_opcodes_php5.1.h \
	xcache/xc_const_string_opcodes_php5.2.h \
	xcache/xc_const_string_opcodes_php5.3.h \
	xcache/xc_const_string_opcodes_php5.4.h \
	xcache/xc_const_string_opcodes_php5.5.h \
	xcache/xc_const_string_opcodes_php5.6.h \
	xcache/xc_const_string_opcodes_php6.x.h

ifeq (${EA_DIR},)
xcache/xc_opcode_spec_def.h: dummy
	@echo "Skipped $@: EA_DIR not set"
else
xcache/xc_opcode_spec_def.h: ${EA_DIR}/opcodes.c
	$(AWK) -f ./devel/gen_xc_opcode_spec.awk < "$<" > "$@".tmp
	mv "$@".tmp "$@"
endif

ifeq (${PHP4_x_DIR},)
xcache/xc_const_string_opcodes_php4.x.h: dummy
	@echo "Skipped $@: PHP_4_x_DIR not set"
else
xcache/xc_const_string_opcodes_php4.x.h: ${PHP4_x_DIR}/Zend/zend_compile.h
	$(AWK) -f ./devel/gen_const_string_opcodes.awk < "$<" > "$@.tmp"
	mv "$@.tmp" "$@"
endif

ifeq (${PHP5_0_DIR},)
xcache/xc_const_string_opcodes_php5.0.h: dummy
	@echo "Skipped $@: PHP_5_0_DIR not set"
else
xcache/xc_const_string_opcodes_php5.0.h: ${PHP5_0_DIR}/Zend/zend_compile.h
	$(AWK) -f ./devel/gen_const_string_opcodes.awk < "$<" > "$@.tmp"
	mv "$@.tmp" "$@"
endif

ifeq (${PHP5_1_DIR},)
xcache/xc_const_string_opcodes_php5.1.h: dummy
	@echo "Skipped $@: PHP_5_1_DIR not set"
else
xcache/xc_const_string_opcodes_php5.1.h: ${PHP5_1_DIR}/Zend/zend_vm_def.h
	$(AWK) -f ./devel/gen_const_string_opcodes.awk < "$<" > "$@.tmp"
	mv "$@.tmp" "$@"
endif

ifeq (${PHP5_2_DIR},)
xcache/xc_const_string_opcodes_php5.2.h: dummy
	@echo "Skipped $@: PHP_5_2_DIR not set"
else
xcache/xc_const_string_opcodes_php5.2.h: ${PHP5_2_DIR}/Zend/zend_vm_def.h
	$(AWK) -f ./devel/gen_const_string_opcodes.awk < "$<" > "$@.tmp"
	mv "$@.tmp" "$@"
endif

ifeq (${PHP5_3_DIR},)
xcache/xc_const_string_opcodes_php5.3.h: dummy
	@echo "Skipped $@: PHP_5_3_DIR not set"
else
xcache/xc_const_string_opcodes_php5.3.h: ${PHP5_3_DIR}/Zend/zend_vm_def.h
	$(AWK) -f ./devel/gen_const_string_opcodes.awk < "$<" > "$@.tmp"
	mv "$@.tmp" "$@"
endif

ifeq (${PHP5_4_DIR},)
xcache/xc_const_string_opcodes_php5.4.h: dummy
	@echo "Skipped $@: PHP_5_4_DIR not set"
else
xcache/xc_const_string_opcodes_php5.4.h: ${PHP5_4_DIR}/Zend/zend_vm_def.h
	$(AWK) -f ./devel/gen_const_string_opcodes.awk < "$<" > "$@.tmp"
	mv "$@.tmp" "$@"
endif

ifeq (${PHP5_5_DIR},)
xcache/xc_const_string_opcodes_php5.5.h: dummy
	@echo "Skipped $@: PHP_5_5_DIR not set"
else
xcache/xc_const_string_opcodes_php5.5.h: ${PHP5_5_DIR}/Zend/zend_vm_def.h
	$(AWK) -f ./devel/gen_const_string_opcodes.awk < "$<" > "$@.tmp"
	mv "$@.tmp" "$@"
endif

ifeq (${PHP5_6_DIR},)
xcache/xc_const_string_opcodes_php5.6.h: dummy
	@echo "Skipped $@: PHP_5_6_DIR not set"
else
xcache/xc_const_string_opcodes_php5.6.h: ${PHP5_6_DIR}/Zend/zend_vm_def.h
	$(AWK) -f ./devel/gen_const_string_opcodes.awk < "$<" > "$@.tmp"
	mv "$@.tmp" "$@"
endif

ifeq (${PHP6_x_DIR},)
xcache/xc_const_string_opcodes_php6.x.h: dummy
	@echo "Skipped $@: PHP_6_x_DIR not set"
else
xcache/xc_const_string_opcodes_php6.x.h: ${PHP6_x_DIR}/Zend/zend_vm_def.h
	$(AWK) -f ./devel/gen_const_string_opcodes.awk < "$<" > "$@.tmp"
	mv "$@.tmp" "$@"
endif

ifeq (${PHP_DEVEL_DIR},)
tags:
	@echo "* Making tags without php source files"
	"$(CTAGS)" -R .
else
tags:
	@echo "* Making tags with ${PHP_DEVEL_DIR}"
	"$(CTAGS)" -R . "${PHP_DEVEL_DIR}/main" "${PHP_DEVEL_DIR}/Zend" "${PHP_DEVEL_DIR}/TSRM" "${PHP_DEVEL_DIR}/ext/standard"
endif

.PHONY: po
define htdocspo
  po: \
	htdocs/$(1)/lang/en.po \
	htdocs/$(1)/lang/en.po-merged \
	htdocs/$(1)/lang/zh-simplified.po-merged \
	htdocs/$(1)/lang/zh-simplified.po \
	htdocs/$(1)/lang/zh-traditional.po \
	htdocs/$(1)/lang/zh-traditional.po-merged

  htdocs/$(1)/lang/%.po-merged: htdocs/$(1)/lang/%.po htdocs/$(1)/lang/$(1).pot
	msgmerge -o "$$@".tmp $$^
	mv "$$@".tmp "$$@"

  htdocs/$(1)/lang/%.po:
	touch "$$@"

  htdocs/$(1)/lang/$(1).pot:
	xgettext --keyword=_T --keyword=N_ --from-code=UTF-8 -F -D htdocs/$(1)/ $$(subst htdocs/$(1)/,,$$^) -o "$$@".tmp
	mv "$$@".tmp "$$@"

  htdocs/$(1)/lang/$(1).pot: $(shell find htdocs/$(1) -type f | grep php | grep -v lang | grep -v config | grep -vF .swp)

endef

$(eval $(call htdocspo,cacher))
$(eval $(call htdocspo,common))
$(eval $(call htdocspo,coverager))
$(eval $(call htdocspo,diagnosis))

.PHONY: clean_po
clean_po: clean_pot
	rm -f htdocs/*/lang/*.po-merged

.PHONY: clean_pot
clean_pot:
	rm -f htdocs/*/lang/*.pot
