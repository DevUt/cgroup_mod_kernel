#ifndef _LINUX_UVM_CTRL_H
#define _LINUX_UVM_CTRL_H

#include <linux/kernel.h>
#include <linux/cgroup-defs.h>
#include <linux/cgroup.h>
#include <linux/types.h>


enum uvm_ctrl_type {
	UVM_SOFT_LIMIT,
	UVM_HARD_LIMIT,
	UVM_CTRL_TYPES,
};

struct uvm_ctrl_css {
	struct cgroup_subsys_state css;
	u64 res[UVM_CTRL_TYPES];
};

static inline struct uvm_ctrl_css *css_to_uvm_css(struct cgroup_subsys_state *css) {
	return css ? container_of(css, struct uvm_ctrl_css, css) : NULL;
}

static inline int uvm_ctrl_get_subsys_id(void){
	return uvm_ctrl_cgrp_id;
}

u64 uvm_ctrl_get_limit(enum uvm_ctrl_type type, struct uvm_ctrl_css *cg);
int uvm_ctrl_set_limit(enum uvm_ctrl_type type, struct uvm_ctrl_css *cg, u64 new_lim);
#endif
