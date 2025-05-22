#ifndef _LINUX_UVM_CTRL_
#define _LINUX_UVM_CTRL_

#include <linux/export.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/uvm_ctrl.h>

static struct uvm_ctrl_css root_cg;
static unsigned long flags;

static void lock_before_call(void)
{
	if (!lock_initialized) {
		spin_lock_init(&callback_spin_lock);
		INIT_LIST_HEAD(&missed_creations_list);
		spin_lock_init(&missed_cl_lock);
	}
	spin_lock_irqsave(&callback_spin_lock, flags);
}

static void unlock_after_call(void)
{
	spin_unlock_irqrestore(&callback_spin_lock, flags);
}

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
	// lock_before_call();
	// if(uvm_ctrl_callback_func != NULL) {
	// 	pr_info("Callback to new css\n");
	// 	(*uvm_ctrl_callback_func)(UVM_NEW_CSS);
	// } else{
	// 	pr_warn("Callaback not found alloc css!\n");
	// }
	// unlock_after_call();
	cg->res[UVM_SOFT_LIMIT] = 200;
	cg->res[UVM_HARD_LIMIT] = 600;
	return &cg->css;
}

void uvm_ctrl_register_callback(void (*func)(enum uvm_ctrl_callback_type))
{
	uvm_ctrl_callback_func = func;
	pr_info("Registered func\n");
}
EXPORT_SYMBOL_GPL(uvm_ctrl_register_callback);

void uvm_ctrl_unregister_callback(void)
{
	uvm_ctrl_callback_func = NULL;
}
EXPORT_SYMBOL_GPL(uvm_ctrl_unregister_callback);

static void uvm_ctrl_cgroup_free(struct cgroup_subsys_state *css)
{
	// lock_before_call();
	// if(uvm_ctrl_callback_func != NULL) {
	// 	pr_info("Callback to new css\n");
	// 	(*uvm_ctrl_callback_func)(UVM_CSS_GONE);
	// } else {
	// 	pr_warn("Callaback not found dead css!\n");
	// }
	// unlock_after_call();
	kfree(css_to_uvm_css(css));
}

static struct uvm_ctrl_css *parent_uvm_css(struct uvm_ctrl_css *cgroup)
{
	return cgroup ? css_to_uvm_css(cgroup->css.parent) : NULL;
}

static inline bool valid_type(enum uvm_ctrl_type type)
{
	return type >= 0 && type <= UVM_CTRL_TYPES;
}

/* We are returning 0 a limit, that is not good. As it can happen 0 is the limit*/
u64 uvm_ctrl_get_limit(enum uvm_ctrl_type type, struct uvm_ctrl_css *cg)
{
	if (!(valid_type(type) && cg)) {
		return 0;
	}

	return cg->res[type];
}
EXPORT_SYMBOL_GPL(uvm_ctrl_get_limit);

int uvm_ctrl_set_limit(enum uvm_ctrl_type type, struct uvm_ctrl_css *cg,
		       u64 new_lim)
{
	if (!(valid_type(type) && cg)) {
		return -EINVAL;
	}

	struct uvm_ctrl_css *i;
	for (i = cg; i; i = parent_uvm_css(cg)) {
		u64 lim = cg->res[type];
		if (lim < new_lim) {
			return -EINVAL;
		}
	}

	WRITE_ONCE(cg->res[type], new_lim);
	return 0;
}
EXPORT_SYMBOL_GPL(uvm_ctrl_set_limit);

static int uvm_ctrl_soft_limit_show(struct seq_file *sf, void *args)
{
	struct uvm_ctrl_css *css = css_to_uvm_css(seq_css(sf));
	seq_printf(sf, "SOFT LIMIT %llu\n", css->res[UVM_SOFT_LIMIT]);
	return 0;
}

static ssize_t uvm_ctrl_soft_limit_write(struct kernfs_open_file *of, char *buf,
					 size_t nbytes, loff_t off)
{
	struct uvm_ctrl_css *cg;

	buf = strstrip(buf);
	u64 new_limit;
	int ret;
	if (nbytes == 0)
		return 0;

	if (nbytes >= 21) // 20 digits should be enough
		return -EINVAL;

	buf[nbytes - 1] = '\0';
	if ((ret = kstrtoull(buf, 10, &new_limit)) < 0)
		return -EINVAL;

	cg = css_to_uvm_css(of_css(of));
	WRITE_ONCE(cg->res[UVM_SOFT_LIMIT], new_limit);
	lock_before_call();
	if (uvm_ctrl_callback_func != NULL) {
		pr_info("Callback to new soft limit\n");
		(*uvm_ctrl_callback_func)(UVM_SOFT_LIMIT_CHANGED);
	} else {
		pr_warn("Callaback not found new soft limit!\n");
	}
	unlock_after_call();
	return nbytes;
}

static ssize_t uvm_ctrl_hard_limit_write(struct kernfs_open_file *of, char *buf,
					 size_t nbytes, loff_t off)
{
	struct uvm_ctrl_css *cg;

	buf = strstrip(buf);
	u64 new_limit;
	int ret;
	if (nbytes == 0)
		return 0;

	if (nbytes >= 21)
		return -EINVAL;

	buf[nbytes - 1] = '\0';
	if ((ret = kstrtoull(buf, 10, &new_limit)) < 0)
		return -EINVAL;

	cg = css_to_uvm_css(of_css(of));
	WRITE_ONCE(cg->res[UVM_HARD_LIMIT], new_limit);
	lock_before_call();
	if (uvm_ctrl_callback_func != NULL) {
		pr_info("Callback to old task dead\n");
		(*uvm_ctrl_callback_func)(UVM_HARD_LIMIT_CHANGED);
	} else {
		pr_warn("Callaback not found old task dead!\n");
	}
	unlock_after_call();
	return nbytes;
}

static int uvm_ctrl_hard_limit_show(struct seq_file *sf, void *args)
{
	struct uvm_ctrl_css *css = css_to_uvm_css(seq_css(sf));
	seq_printf(sf, "HARD LIMIT %llu\n", css->res[UVM_HARD_LIMIT]);
	return 0;
}

static void got_new_task(struct task_struct *task)
{
	// lock_before_call();
	// if(uvm_ctrl_callback_func != NULL) {
	// 	pr_info("Callback to new task\n");
	// 	(*uvm_ctrl_callback_func)(UVM_NEW_TASK);
	// } else {
	// 	pr_warn("Callaback not found new task!\n");
	// }
	// unlock_after_call();
}

static void old_task_exit(struct task_struct *task)
{
	// lock_before_call();
	// if(uvm_ctrl_callback_func != NULL) {
	// 	pr_info("Callback to old task dead\n");
	// 	(*uvm_ctrl_callback_func)(UVM_EXIT_TASK);
	// } else {
	// 	pr_warn("Callaback not found old task dead!\n");
	// }
	// unlock_after_call();
}

static void css_dead(struct cgroup_subsys_state *css)
{
	lock_before_call();
	if (uvm_ctrl_callback_func != NULL) {
		pr_info("Css died\n");
		(*uvm_ctrl_callback_func)(UVM_CSS_GONE);
	} else {
		pr_warn("callback for dead css not available\n");
		unsigned long deadFlags;
		spin_lock_irqsave(&missed_cl_lock, deadFlags);
		struct missed_creation *entry, *tmp;
		list_for_each_entry_safe(entry, tmp, &missed_creations_list,
					 node) {
			if (entry->css->id == css->id) {
				kfree(entry->css);
				list_del(&entry->node);
				break;
			}
		}
		spin_unlock_irqrestore(&missed_cl_lock, deadFlags);
	}
	unlock_after_call();
}

static int css_ready(struct cgroup_subsys_state *css)
{
	lock_before_call();
	if (uvm_ctrl_callback_func != NULL) {
		pr_info("Css ALIVE\n");
		(*uvm_ctrl_callback_func)(UVM_NEW_CSS);
	} else {
		pr_warn("callback for new css not available\n");
		unsigned long readyFlags;
		spin_lock_irqsave(&missed_cl_lock, readyFlags);
		struct missed_creation *newCreation =
			kzalloc(sizeof(struct missed_creation), GFP_KERNEL);
		newCreation->css = css;
		list_add_tail(&newCreation->node, &missed_creations_list);
		spin_unlock_irqrestore(&missed_cl_lock, readyFlags);
		pr_info("Added to missed\n");
	}
	unlock_after_call();
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

// HOLD LOCK WHEN ACCESSING THIS
struct list_head *uvm_ctrl_get_missed_css_unlocked(void)
{
	return &missed_creations_list;
}
EXPORT_SYMBOL_GPL(uvm_ctrl_get_missed_css_unlocked);

struct cgroup_subsys uvm_ctrl_cgrp_subsys = {
	.css_alloc = uvm_ctrl_cgroup_alloc,
	.css_free = uvm_ctrl_cgroup_free,
	.legacy_cftypes = uvm_cg_files,
	.dfl_cftypes = uvm_cg_files,
	.fork = got_new_task,
	.exit = old_task_exit,
	.css_online = css_ready,
	.css_offline = css_dead,
};

#endif
