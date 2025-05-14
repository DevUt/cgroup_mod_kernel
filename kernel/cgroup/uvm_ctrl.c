#ifndef _LINUX_UVM_CTRL_
#define _LINUX_UVM_CTRL_

#include <linux/uvm_ctrl.h>

static struct uvm_ctrl_css root_cg;

/* On allocation */
static struct cgroup_subsys_state *
uvm_ctrl_cgroup_alloc(struct cgroup_subsys_state *parent_css)
{
	struct uvm_ctrl_css *cg;

	if (!parent_css) {
		cg = &root_cg;
	} else {
		cg = kzalloc(sizeof(*cg), GFP_KERNEL);
		if (!cg)
			return ERR_PTR(-ENOMEM);
	}

	/* Here I may want to add something to default init it idk */
	cg->res[UVM_SOFT_LIMIT] = 200;
	cg->res[UVM_HARD_LIMIT] = 600;
	return &cg->css;
}

static void uvm_ctrl_cgroup_free(struct cgroup_subsys_state *css)
{
	kfree(css_to_uvm_css(css));
}


static struct uvm_ctrl_css *parent_uvm_css(struct uvm_ctrl_css *cgroup)
{
	return cgroup ? css_to_uvm_css(cgroup->css.parent) : NULL;
}

static inline bool valid_type(enum uvm_ctrl_type type) {
	return type >=0 && type <= UVM_CTRL_TYPES;
}

/* We are returning 0 a limit, that is not good. As it can happen 0 is the limit*/
u64 uvm_ctrl_get_limit(enum uvm_ctrl_type type, struct uvm_ctrl_css *cg){
	if(!(valid_type(type) && cg)) {
		return 0;
	}
	
	return cg->res[type];
}
EXPORT_SYMBOL_GPL(uvm_ctrl_get_limit);

int uvm_ctrl_set_limit(enum uvm_ctrl_type type, struct uvm_ctrl_css *cg, u64 new_lim){
	if(!(valid_type(type) && cg)) {
		return -EINVAL;
	}

	struct uvm_ctrl_css *i;
	for(i = cg; i; i = parent_uvm_css(cg)){
		u64 lim =  cg->res[type];
		if (lim < new_lim){
			return -EINVAL;
		}
	}

	WRITE_ONCE(cg->res[type], new_lim);
	return 0;
}
EXPORT_SYMBOL_GPL(uvm_ctrl_set_limit);

static int uvm_ctrl_soft_limit_show(struct seq_file *sf, void *args){
	struct uvm_ctrl_css *css = css_to_uvm_css(seq_css(sf));
	seq_printf(sf, "SOFT LIMIT %llu\n", css->res[UVM_SOFT_LIMIT]);
	return 0;
}

static ssize_t uvm_ctrl_soft_limit_write(struct kernfs_open_file *of, char *buf, size_t nbytes, loff_t off){
	struct uvm_ctrl_css *cg;

	buf = strstrip(buf);
	u64 new_limit;
	int ret;
	if((ret = kstrtoull(buf, 10, &new_limit)) < 0)
		return -EINVAL;

	cg = css_to_uvm_css(of_css(of));
	// if(new_limit > cg->res[UVM_HARD_LIMIT]) {
	// 	return -EINVAL;
	// }
	WRITE_ONCE(cg->res[UVM_SOFT_LIMIT], new_limit);
	return 0;
}


static ssize_t uvm_ctrl_hard_limit_write(struct kernfs_open_file *of, char *buf, size_t nbytes, loff_t off){
	struct uvm_ctrl_css *cg;

	buf = strstrip(buf);
	u64 new_limit;
	int ret;
	if((ret = kstrtoull(buf, 10, &new_limit)) < 0)
		return -EINVAL;

	cg = css_to_uvm_css(of_css(of));
	// if(new_limit < cg->res[UVM_SOFT_LIMIT]) {
	// 	return -EINVAL;
	// }
	WRITE_ONCE(cg->res[UVM_HARD_LIMIT], new_limit);
	return 0;
}

static int uvm_ctrl_hard_limit_show(struct seq_file *sf, void *args){
	struct uvm_ctrl_css *css = css_to_uvm_css(seq_css(sf));
	seq_printf(sf, "HARD LIMIT %llu\n", css->res[UVM_HARD_LIMIT]);
	return 0;
}



static struct cftype uvm_cg_files[] = {
	{
		.name = "soft",
		.seq_show = uvm_ctrl_soft_limit_show,
		.write = uvm_ctrl_soft_limit_write,
	},
	{
		.name = "hard",
		.seq_show = uvm_ctrl_hard_limit_show,
		.write = uvm_ctrl_hard_limit_write,
	},
};


struct cgroup_subsys uvm_ctrl_cgrp_subsys = {
	.css_alloc = uvm_ctrl_cgroup_alloc,
	.css_free = uvm_ctrl_cgroup_free,
	.legacy_cftypes = uvm_cg_files,
	.dfl_cftypes = uvm_cg_files,
};


#endif
